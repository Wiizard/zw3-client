#include "STDInclude.hpp"

#include "Utils/JSON.hpp"

#include <rapidjson/prettywriter.h>

#include "Components/Modules/AssetHandler.hpp"
#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#include "IFxEffectDef.hpp"

namespace Assets
{
	constexpr int fxFileVersion = 3;
	constexpr char fxFileMagic[] = "IW4xFx  ";
	constexpr std::size_t fxFileMagicLength = 8;

	static std::unordered_set<std::string> dumpedPaths;

	struct FxLoad
	{
		Utils::Memory::Allocator& allocator;
		Components::ZoneBuilder::Zone* builder;
		const std::string& name;
	};

	static int ElemDefCount(const Game::FxEffectDef* effect)
	{
		return effect->elemDefCountLooping + effect->elemDefCountOneShot + effect->elemDefCountEmission;
	}

	static int TotalSizeWidening(const Game::FxEffectDef* effect)
	{
		auto widening = static_cast<int>(sizeof(Game::FxEffectDef) - sizeof(Game::X86::FxEffectDef));

		if (!effect->elemDefs)
		{
			return widening;
		}

		for (int i = 0; i < ElemDefCount(effect); ++i)
		{
			const auto* const elemDef = &effect->elemDefs[i];
			const auto visualCount = static_cast<unsigned char>(elemDef->visualCount);

			widening += static_cast<int>(sizeof(Game::FxElemDef) - sizeof(Game::X86::FxElemDef));

			if (elemDef->elemType == Game::FX_ELEM_TYPE_DECAL)
			{
				if (elemDef->visuals.markArray)
				{
					widening += visualCount * static_cast<int>(sizeof(Game::FxElemMarkVisuals) - sizeof(Game::X86::FxElemMarkVisuals));
				}
			}
			else if (visualCount > 1 && elemDef->visuals.array)
			{
				widening += visualCount * static_cast<int>(sizeof(Game::FxElemVisuals) - sizeof(Game::X86::FxElemVisuals));
			}

			if (elemDef->elemType == Game::FX_ELEM_TYPE_TRAIL && elemDef->extended.trailDef)
			{
				widening += static_cast<int>(sizeof(Game::FxTrailDef) - sizeof(Game::X86::FxTrailDef));
			}
		}

		return widening;
	}

	static Game::X86::FxElemDef ConvertElemDef(const Game::FxElemDef& elemDef)
	{
		auto record = Game::X86::Convert(elemDef);
		record.visuals.markArray = 0;
		record.effectOnImpact.handle = 0;
		record.effectOnDeath.handle = 0;
		record.effectEmitted.handle = 0;
		record.extended.trailDef = 0;
		return record;
	}

	static Game::FxElemDef ConvertElemDef(const Game::X86::FxElemDef& record)
	{
		auto elemDef = Game::X86::Convert(record);
		elemDef.visuals.markArray = nullptr;
		elemDef.effectOnImpact.handle = nullptr;
		elemDef.effectOnDeath.handle = nullptr;
		elemDef.effectEmitted.handle = nullptr;
		elemDef.extended.trailDef = nullptr;
		return elemDef;
	}

	static const rapidjson::Value* FindMember(const rapidjson::Value& json, const char* key)
	{
		if (!json.IsObject())
		{
			return nullptr;
		}

		const auto member = json.FindMember(key);

		if (member == json.MemberEnd())
		{
			return nullptr;
		}

		return &member->value;
	}

	static bool HasValue(const rapidjson::Value& json, const char* key)
	{
		const auto* const member = FindMember(json, key);
		return member && !member->IsNull();
	}

	static bool TryGetInt(const rapidjson::Value& json, const char* key, int& value)
	{
		const auto* const member = FindMember(json, key);

		if (!member || !member->IsInt())
		{
			return false;
		}

		value = member->GetInt();
		return true;
	}

	template <typename T>
	static bool TryGetSmallInt(const rapidjson::Value& json, const char* key, T& value)
	{
		int wide = 0;

		if (!TryGetInt(json, key, wide))
		{
			return false;
		}

		value = static_cast<T>(wide);
		return true;
	}

	static bool TryGetFloat(const rapidjson::Value& json, const char* key, float& value)
	{
		const auto* const member = FindMember(json, key);

		if (!member || !member->IsNumber())
		{
			return false;
		}

		value = member->GetFloat();
		return true;
	}

	static bool TryGetString(const rapidjson::Value& json, const char* key, const char*& text)
	{
		const auto* const member = FindMember(json, key);

		if (!member || !member->IsString())
		{
			return false;
		}

		text = member->GetString();
		return true;
	}

	template <typename T>
	static bool TryReadNumbers(const rapidjson::Value* json, T* values, std::size_t count)
	{
		if (!json || !json->IsArray() || json->Size() < count)
		{
			return false;
		}

		for (rapidjson::SizeType i = 0; i < count; ++i)
		{
			const auto& entry = (*json)[i];

			if constexpr (std::is_floating_point_v<T>)
			{
				if (!entry.IsNumber())
				{
					return false;
				}

				values[i] = entry.GetFloat();
			}
			else
			{
				if (!entry.IsInt())
				{
					return false;
				}

				values[i] = static_cast<T>(entry.GetInt());
			}
		}

		return true;
	}

	static bool TryReadFloatRange(const rapidjson::Value* json, Game::FxFloatRange& range)
	{
		return json && TryGetFloat(*json, "base", range.base) && TryGetFloat(*json, "amplitude", range.amplitude);
	}

	static bool TryReadFloatRange(const rapidjson::Value& json, const char* key, Game::FxFloatRange& range)
	{
		return TryReadFloatRange(FindMember(json, key), range);
	}

	static bool TryReadFloatRanges(const rapidjson::Value& json, const char* key, Game::FxFloatRange (&ranges)[3])
	{
		const auto* const member = FindMember(json, key);

		if (!member || !member->IsArray() || member->Size() < std::size(ranges))
		{
			return false;
		}

		for (rapidjson::SizeType i = 0; i < std::size(ranges); ++i)
		{
			if (!TryReadFloatRange(&(*member)[i], ranges[i]))
			{
				return false;
			}
		}

		return true;
	}

	static bool TryReadIntRange(const rapidjson::Value& json, const char* key, Game::FxIntRange& range)
	{
		const auto* const member = FindMember(json, key);
		return member && TryGetInt(*member, "base", range.base) && TryGetInt(*member, "amplitude", range.amplitude);
	}

	static bool TryReadVec3Range(const rapidjson::Value* json, Game::FxElemVec3Range& range)
	{
		return json && TryReadNumbers(FindMember(*json, "base"), range.base, 3) && TryReadNumbers(FindMember(*json, "amplitude"), range.amplitude, 3);
	}

	static bool TryReadVelStateInFrame(const rapidjson::Value* json, Game::FxElemVelStateInFrame& frame)
	{
		return json && TryReadVec3Range(FindMember(*json, "velocity"), frame.velocity) && TryReadVec3Range(FindMember(*json, "totalDelta"), frame.totalDelta);
	}

	static bool TryReadVisualState(const rapidjson::Value* json, Game::FxElemVisualState& state)
	{
		return json
			&& TryReadNumbers(FindMember(*json, "color"), state.color, 4)
			&& TryGetFloat(*json, "rotationDelta", state.rotationDelta)
			&& TryGetFloat(*json, "rotationTotal", state.rotationTotal)
			&& TryReadNumbers(FindMember(*json, "size"), state.size, 2)
			&& TryGetFloat(*json, "scale", state.scale);
	}

	static bool TryReadBounds(const rapidjson::Value& json, const char* key, Game::Bounds& bounds)
	{
		const auto* const member = FindMember(json, key);
		return member && TryReadNumbers(FindMember(*member, "midPoint"), bounds.midPoint, 3) && TryReadNumbers(FindMember(*member, "halfSize"), bounds.halfSize, 3);
	}

	static bool TryReadAtlas(const rapidjson::Value& json, Game::FxElemAtlas& atlas)
	{
		const auto* const member = FindMember(json, "atlas");

		return member
			&& TryGetSmallInt(*member, "behavior", atlas.behavior)
			&& TryGetSmallInt(*member, "index", atlas.index)
			&& TryGetSmallInt(*member, "fps", atlas.fps)
			&& TryGetSmallInt(*member, "loopCount", atlas.loopCount)
			&& TryGetSmallInt(*member, "colIndexBits", atlas.colIndexBits)
			&& TryGetSmallInt(*member, "rowIndexBits", atlas.rowIndexBits)
			&& TryGetSmallInt(*member, "entryCount", atlas.entryCount);
	}

	static bool TryReadJsonVisuals(FxLoad& load, const rapidjson::Value& json, char elemType, Game::FxElemVisuals* visuals)
	{
		if (json.IsNull())
		{
			return true;
		}

		if (!json.IsObject())
		{
			return false;
		}

		const char* assetName = nullptr;

		switch (elemType)
		{
		case Game::FX_ELEM_TYPE_MODEL:
		{
			if (TryGetString(json, "model", assetName))
			{
				visuals->model = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_XMODEL, assetName, load.builder).model;
			}

			return true;
		}

		case Game::FX_ELEM_TYPE_OMNI_LIGHT:
		case Game::FX_ELEM_TYPE_SPOT_LIGHT:
			return true;

		case Game::FX_ELEM_TYPE_SOUND:
		{
			if (!TryGetString(json, "sound", assetName))
			{
				return true;
			}

			visuals->soundName = load.allocator.DuplicateString(assetName);

			if (!Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_SOUND, visuals->soundName, load.builder).data)
			{
				Components::Logger::Error("Critical error - could not find sound {}. This will crash the game!\n", visuals->soundName);
				return false;
			}

			return true;
		}

		case Game::FX_ELEM_TYPE_RUNNER:
		{
			if (!TryGetString(json, "runner", assetName))
			{
				return true;
			}

			visuals->effectDef.handle = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_FX, assetName, load.builder).fx;

			if (!visuals->effectDef.handle)
			{
				Components::Logger::Error("Critical error - could not find fx {}. This will crash the game!\n", assetName);
				return false;
			}

			return true;
		}

		default:
		{
			if (!TryGetString(json, "material", assetName))
			{
				return true;
			}

			visuals->material = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, assetName, load.builder).material;

			if (!visuals->material)
			{
				Components::Logger::Error("Critical error - could not find material {}. This will crash the game!\n", assetName);
				return false;
			}

			return true;
		}
		}
	}

	static bool TryReadJsonEffectRef(FxLoad& load, const rapidjson::Value& elem, const char* key, Game::FxEffectDefRef& reference)
	{
		if (!HasValue(elem, key))
		{
			return true;
		}

		const char* effectName = nullptr;

		if (!TryGetString(elem, key, effectName))
		{
			return false;
		}

		reference.handle = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_FX, effectName, load.builder).fx;
		return true;
	}

	static bool TryReadJsonSamples(FxLoad& load, const rapidjson::Value& elem, Game::FxElemDef* elemDef)
	{
		const auto velCount = static_cast<unsigned char>(elemDef->velIntervalCount) + 1u;
		elemDef->velSamples = load.allocator.AllocateArray<Game::FxElemVelStateSample>(velCount);

		if (HasValue(elem, "velSamples"))
		{
			const auto& samples = elem["velSamples"];

			if (!samples.IsArray() || samples.Size() < velCount)
			{
				return false;
			}

			for (rapidjson::SizeType j = 0; j < velCount; ++j)
			{
				auto* const sample = &elemDef->velSamples[j];

				if (!TryReadVelStateInFrame(FindMember(samples[j], "local"), sample->local) || !TryReadVelStateInFrame(FindMember(samples[j], "world"), sample->world))
				{
					return false;
				}
			}
		}

		const auto visCount = static_cast<unsigned char>(elemDef->visStateIntervalCount) + 1u;
		elemDef->visSamples = load.allocator.AllocateArray<Game::FxElemVisStateSample>(visCount);

		if (HasValue(elem, "visSamples"))
		{
			const auto& samples = elem["visSamples"];

			if (!samples.IsArray() || samples.Size() < visCount)
			{
				return false;
			}

			for (rapidjson::SizeType j = 0; j < visCount; ++j)
			{
				auto* const sample = &elemDef->visSamples[j];

				if (!TryReadVisualState(FindMember(samples[j], "base"), sample->base) || !TryReadVisualState(FindMember(samples[j], "amplitude"), sample->amplitude))
				{
					return false;
				}
			}
		}

		return true;
	}

	static bool TryReadJsonElemVisuals(FxLoad& load, const rapidjson::Value& elem, Game::FxElemDef* elemDef)
	{
		const auto visualCount = static_cast<unsigned char>(elemDef->visualCount);

		if (elemDef->elemType == Game::FX_ELEM_TYPE_DECAL)
		{
			if (!HasValue(elem, "markArray"))
			{
				return true;
			}

			const auto& marks = elem["markArray"];

			if (!marks.IsArray() || marks.Size() < visualCount)
			{
				return false;
			}

			elemDef->visuals.markArray = load.allocator.AllocateArray<Game::FxElemMarkVisuals>(visualCount);

			for (rapidjson::SizeType j = 0; j < visualCount; ++j)
			{
				const auto& materials = marks[j];

				if (!materials.IsArray() || materials.Size() < 2)
				{
					return false;
				}

				for (rapidjson::SizeType k = 0; k < 2; ++k)
				{
					if (materials[k].IsNull())
					{
						continue;
					}

					if (!materials[k].IsString())
					{
						return false;
					}

					elemDef->visuals.markArray[j].materials[k] = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, materials[k].GetString(), load.builder).material;
				}
			}

			return true;
		}

		if (visualCount > 1)
		{
			if (!HasValue(elem, "visualsArray"))
			{
				return true;
			}

			const auto& visuals = elem["visualsArray"];

			if (!visuals.IsArray() || visuals.Size() < visualCount)
			{
				return false;
			}

			elemDef->visuals.array = load.allocator.AllocateArray<Game::FxElemVisuals>(visualCount);

			for (rapidjson::SizeType j = 0; j < visualCount; ++j)
			{
				if (!TryReadJsonVisuals(load, visuals[j], elemDef->elemType, &elemDef->visuals.array[j]))
				{
					return false;
				}
			}

			return true;
		}

		if (!visualCount)
		{
			return true;
		}

		const auto* const instance = FindMember(elem, "instance");
		return instance && TryReadJsonVisuals(load, *instance, elemDef->elemType, &elemDef->visuals.instance);
	}

	static bool TryReadJsonExtended(FxLoad& load, const rapidjson::Value& elem, Game::FxElemDef* elemDef, int elemIndex)
	{
		if (elemDef->elemType == Game::FX_ELEM_TYPE_TRAIL)
		{
			if (!HasValue(elem, "trailDef"))
			{
				return true;
			}

			const auto& trailJson = elem["trailDef"];
			auto* const trail = load.allocator.Allocate<Game::FxTrailDef>();
			elemDef->extended.trailDef = trail;

			const auto* const verts = FindMember(trailJson, "verts");
			const auto* const inds = FindMember(trailJson, "inds");

			const bool isComplete = TryGetInt(trailJson, "scrollTimeMsec", trail->scrollTimeMsec)
				&& TryGetInt(trailJson, "repeatDist", trail->repeatDist)
				&& TryGetFloat(trailJson, "invSplitDist", trail->invSplitDist)
				&& TryGetFloat(trailJson, "invSplitArcDist", trail->invSplitArcDist)
				&& TryGetFloat(trailJson, "invSplitTime", trail->invSplitTime)
				&& verts && verts->IsArray()
				&& inds && inds->IsArray();

			if (!isComplete)
			{
				return false;
			}

			trail->vertCount = static_cast<int>(verts->Size());
			trail->verts = load.allocator.AllocateArray<Game::FxTrailVertex>(verts->Size());

			for (rapidjson::SizeType j = 0; j < verts->Size(); ++j)
			{
				const auto& vertJson = (*verts)[j];
				auto* const vert = &trail->verts[j];

				const bool isVertComplete = TryReadNumbers(FindMember(vertJson, "pos"), vert->pos, 2)
					&& TryReadNumbers(FindMember(vertJson, "normal"), vert->normal, 2)
					&& TryGetFloat(vertJson, "texCoord", vert->texCoord);

				if (!isVertComplete)
				{
					return false;
				}
			}

			trail->indCount = static_cast<int>(inds->Size());
			trail->inds = load.allocator.AllocateArray<unsigned short>(inds->Size());

			return TryReadNumbers(inds, trail->inds, inds->Size());
		}

		if (elemDef->elemType != Game::FX_ELEM_TYPE_SPARK_FOUNTAIN)
		{
			return true;
		}

		if (!HasValue(elem, "sparkFountain"))
		{
			Components::Logger::Error("Missing spark fountain definition for fx {} elem {}, this will crash on iw4!\n", load.name, elemIndex);
			return false;
		}

		const auto& sparkJson = elem["sparkFountain"];
		auto* const spark = load.allocator.Allocate<Game::FxSparkFountainDef>();
		elemDef->extended.sparkFountainDef = spark;

		return TryGetFloat(sparkJson, "gravity", spark->gravity)
			&& TryGetFloat(sparkJson, "bounceFrac", spark->bounceFrac)
			&& TryGetFloat(sparkJson, "bounceRand", spark->bounceRand)
			&& TryGetFloat(sparkJson, "sparkSpacing", spark->sparkSpacing)
			&& TryGetFloat(sparkJson, "sparkLength", spark->sparkLength)
			&& TryGetInt(sparkJson, "sparkCount", spark->sparkCount)
			&& TryGetFloat(sparkJson, "loopTime", spark->loopTime)
			&& TryGetFloat(sparkJson, "velMin", spark->velMin)
			&& TryGetFloat(sparkJson, "velMax", spark->velMax)
			&& TryGetFloat(sparkJson, "velConeFrac", spark->velConeFrac)
			&& TryGetFloat(sparkJson, "restSpeed", spark->restSpeed)
			&& TryGetFloat(sparkJson, "boostTime", spark->boostTime)
			&& TryGetFloat(sparkJson, "boostFactor", spark->boostFactor);
	}

	static bool TryReadJsonElemDef(FxLoad& load, const rapidjson::Value& elem, Game::FxElemDef* elemDef, int elemIndex)
	{
		const auto* const spawn = FindMember(elem, "spawn");

		const bool isComplete = TryGetInt(elem, "flags", elemDef->flags)
			&& spawn
			&& TryGetInt(*spawn, "A", elemDef->spawn.looping.count)
			&& TryGetInt(*spawn, "B", elemDef->spawn.looping.intervalMsec)
			&& TryReadFloatRange(elem, "spawnRange", elemDef->spawnRange)
			&& TryReadFloatRange(elem, "fadeInRange", elemDef->fadeInRange)
			&& TryReadFloatRange(elem, "fadeOutRange", elemDef->fadeOutRange)
			&& TryGetFloat(elem, "spawnFrustumCullRadius", elemDef->spawnFrustumCullRadius)
			&& TryReadIntRange(elem, "spawnDelayMsec", elemDef->spawnDelayMsec)
			&& TryReadIntRange(elem, "lifeSpanMsec", elemDef->lifeSpanMsec)
			&& TryReadFloatRanges(elem, "spawnOrigin", elemDef->spawnOrigin)
			&& TryReadFloatRange(elem, "spawnOffsetRadius", elemDef->spawnOffsetRadius)
			&& TryReadFloatRange(elem, "spawnOffsetHeight", elemDef->spawnOffsetHeight)
			&& TryReadFloatRanges(elem, "spawnAngles", elemDef->spawnAngles)
			&& TryReadFloatRanges(elem, "angularVelocity", elemDef->angularVelocity)
			&& TryReadFloatRange(elem, "initialRotation", elemDef->initialRotation)
			&& TryReadFloatRange(elem, "gravity", elemDef->gravity)
			&& TryReadFloatRange(elem, "reflectionFactor", elemDef->reflectionFactor)
			&& TryReadAtlas(elem, elemDef->atlas)
			&& TryGetSmallInt(elem, "elemType", elemDef->elemType)
			&& TryGetSmallInt(elem, "visualCount", elemDef->visualCount)
			&& TryGetSmallInt(elem, "velIntervalCount", elemDef->velIntervalCount)
			&& TryGetSmallInt(elem, "visStateIntervalCount", elemDef->visStateIntervalCount)
			&& TryReadBounds(elem, "collBounds", elemDef->collBounds)
			&& TryReadFloatRange(elem, "emitDist", elemDef->emitDist)
			&& TryReadFloatRange(elem, "emitDistVariance", elemDef->emitDistVariance)
			&& TryGetSmallInt(elem, "sortOrder", elemDef->sortOrder)
			&& TryGetSmallInt(elem, "lightingFrac", elemDef->lightingFrac)
			&& TryGetSmallInt(elem, "useItemClip", elemDef->useItemClip)
			&& TryGetSmallInt(elem, "fadeInfo", elemDef->fadeInfo);

		return isComplete
			&& TryReadJsonSamples(load, elem, elemDef)
			&& TryReadJsonEffectRef(load, elem, "effectOnImpact", elemDef->effectOnImpact)
			&& TryReadJsonEffectRef(load, elem, "effectOnDeath", elemDef->effectOnDeath)
			&& TryReadJsonEffectRef(load, elem, "effectEmitted", elemDef->effectEmitted)
			&& TryReadJsonElemVisuals(load, elem, elemDef)
			&& TryReadJsonExtended(load, elem, elemDef, elemIndex);
	}

	static Game::FxEffectDef* TryReadJsonEffect(FxLoad& load, const std::string& text)
	{
		rapidjson::Document json;
		json.Parse(text.data());

		if (json.HasParseError() || !json.IsObject())
		{
			Components::Logger::Error("Invalid JSON for FX {}!\n", load.name);
			return nullptr;
		}

		int version = 0;

		if (!TryGetInt(json, "version", version) || version > fxFileVersion)
		{
			Components::Logger::Error("Wrong FX version for {}! expected {} and got {}!\n", load.name, fxFileVersion, version);
			return nullptr;
		}

		auto* const effect = load.allocator.Allocate<Game::FxEffectDef>();
		const char* effectName = nullptr;

		const bool isComplete = TryGetString(json, "name", effectName)
			&& TryGetInt(json, "flags", effect->flags)
			&& TryGetInt(json, "totalSize", effect->totalSize)
			&& TryGetInt(json, "msecLoopingLife", effect->msecLoopingLife)
			&& TryGetInt(json, "elemDefCountLooping", effect->elemDefCountLooping)
			&& TryGetInt(json, "elemDefCountOneShot", effect->elemDefCountOneShot)
			&& TryGetInt(json, "elemDefCountEmission", effect->elemDefCountEmission);

		if (!isComplete)
		{
			Components::Logger::Error("Malformed JSON for FX {}!\n", load.name);
			return nullptr;
		}

		effect->name = load.allocator.DuplicateString(effectName);

		if (HasValue(json, "elemDefs"))
		{
			const auto& elems = json["elemDefs"];
			const auto elemCount = ElemDefCount(effect);

			if (!elems.IsArray() || elemCount < 0 || elems.Size() != static_cast<rapidjson::SizeType>(elemCount))
			{
				Components::Logger::Error("Malformed JSON for FX {}! its element list does not hold the {} elements it counts\n", load.name, elemCount);
				return nullptr;
			}

			effect->elemDefs = load.allocator.AllocateArray<Game::FxElemDef>(elems.Size());

			for (rapidjson::SizeType i = 0; i < elems.Size(); ++i)
			{
				if (!TryReadJsonElemDef(load, elems[i], &effect->elemDefs[i], static_cast<int>(i)))
				{
					Components::Logger::Error("Malformed JSON for FX {}! element {} is missing a field or holds the wrong type\n", load.name, i);
					return nullptr;
				}
			}
		}

		effect->totalSize += TotalSizeWidening(effect);
		return effect;
	}

	static void ReadBinaryVisuals(FxLoad& load, Utils::Stream::Reader& reader, const Game::X86::FxElemVisuals& record, char elemType, Game::FxElemVisuals* visuals)
	{
		switch (elemType)
		{
		case Game::FX_ELEM_TYPE_MODEL:
		{
			if (record.model)
			{
				visuals->model = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_XMODEL, reader.ReadString(), load.builder).model;
			}

			break;
		}

		case Game::FX_ELEM_TYPE_OMNI_LIGHT:
		case Game::FX_ELEM_TYPE_SPOT_LIGHT:
			break;

		case Game::FX_ELEM_TYPE_SOUND:
		{
			if (record.soundName)
			{
				visuals->soundName = reader.ReadCString();
				Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_SOUND, visuals->soundName, load.builder);
			}

			break;
		}

		case Game::FX_ELEM_TYPE_RUNNER:
		{
			if (record.effectDef.handle)
			{
				visuals->effectDef.handle = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_FX, reader.ReadString(), load.builder).fx;
			}

			break;
		}

		default:
		{
			if (record.material)
			{
				visuals->material = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, reader.ReadString(), load.builder).material;
			}

			break;
		}
		}
	}

	static Game::FxEffectDef* ReadBinaryEffect(FxLoad& load, Utils::Stream::Reader& reader)
	{
		const auto magic = reader.Read<std::uint64_t>();

		if (std::memcmp(&magic, fxFileMagic, fxFileMagicLength))
		{
			throw std::runtime_error("header is invalid");
		}

		const auto version = reader.Read<std::int32_t>();

		if (version > fxFileVersion)
		{
			throw std::runtime_error(std::format("expected version is {}, but it was {}", fxFileVersion, version));
		}

		const auto record = reader.Read<Game::X86::FxEffectDef>();
		auto* const effect = load.allocator.Allocate<Game::FxEffectDef>();
		*effect = Game::X86::Convert(record);

		if (record.name)
		{
			effect->name = reader.ReadCString();
		}

		if (!record.elemDefs)
		{
			effect->totalSize += TotalSizeWidening(effect);
			return effect;
		}

		const auto elemCount = ElemDefCount(effect);

		if (elemCount < 0)
		{
			throw std::runtime_error("negative element count");
		}

		const auto* const elems = reader.ReadArray<Game::X86::FxElemDef>(static_cast<std::size_t>(elemCount));
		effect->elemDefs = load.allocator.AllocateArray<Game::FxElemDef>(static_cast<std::size_t>(elemCount));

		for (int i = 0; i < elemCount; ++i)
		{
			const auto& elem = elems[i];
			auto* const elemDef = &effect->elemDefs[i];
			*elemDef = ConvertElemDef(elem);

			const auto visualCount = static_cast<unsigned char>(elemDef->visualCount);

			if (elem.velSamples)
			{
				elemDef->velSamples = reader.ReadArray<Game::FxElemVelStateSample>(static_cast<unsigned char>(elemDef->velIntervalCount) + 1u);
			}

			if (elem.visSamples)
			{
				elemDef->visSamples = reader.ReadArray<Game::FxElemVisStateSample>(static_cast<unsigned char>(elemDef->visStateIntervalCount) + 1u);
			}

			if (elemDef->elemType == Game::FX_ELEM_TYPE_DECAL)
			{
				if (elem.visuals.markArray)
				{
					const auto* const marks = reader.ReadArray<Game::X86::FxElemMarkVisuals>(visualCount);
					elemDef->visuals.markArray = load.allocator.AllocateArray<Game::FxElemMarkVisuals>(visualCount);

					for (unsigned char j = 0; j < visualCount; ++j)
					{
						for (int k = 0; k < 2; ++k)
						{
							if (marks[j].materials[k])
							{
								elemDef->visuals.markArray[j].materials[k] = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, reader.ReadString(), load.builder).material;
							}
						}
					}
				}
			}
			else if (visualCount > 1)
			{
				if (elem.visuals.array)
				{
					const auto* const visuals = reader.ReadArray<Game::X86::FxElemVisuals>(visualCount);
					elemDef->visuals.array = load.allocator.AllocateArray<Game::FxElemVisuals>(visualCount);

					for (unsigned char j = 0; j < visualCount; ++j)
					{
						ReadBinaryVisuals(load, reader, visuals[j], elemDef->elemType, &elemDef->visuals.array[j]);
					}
				}
			}
			else if (visualCount == 1)
			{
				ReadBinaryVisuals(load, reader, elem.visuals.instance, elemDef->elemType, &elemDef->visuals.instance);
			}

			const std::pair<std::uint32_t, Game::FxEffectDefRef*> references[] =
			{
				{ elem.effectOnImpact.handle, &elemDef->effectOnImpact },
				{ elem.effectOnDeath.handle, &elemDef->effectOnDeath },
				{ elem.effectEmitted.handle, &elemDef->effectEmitted },
			};

			for (const auto& [stored, reference] : references)
			{
				if (!stored)
				{
					continue;
				}

				const auto effectName = reader.ReadString();
				reference->handle = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_FX, effectName, load.builder).fx;

				if (!reference->handle)
				{
					throw std::runtime_error(std::format("could not find the effect {} it refers to", effectName));
				}
			}

			if (elemDef->elemType == Game::FX_ELEM_TYPE_TRAIL)
			{
				if (elem.extended.trailDef)
				{
					const auto trailRecord = reader.Read<Game::X86::FxTrailDef>();
					auto* const trail = load.allocator.Allocate<Game::FxTrailDef>();
					*trail = Game::X86::Convert(trailRecord);
					elemDef->extended.trailDef = trail;

					if (trailRecord.verts)
					{
						trail->verts = reader.ReadArray<Game::FxTrailVertex>(static_cast<std::size_t>(trail->vertCount));
					}

					if (trailRecord.inds)
					{
						trail->inds = reader.ReadArray<unsigned short>(static_cast<std::size_t>(trail->indCount));
					}
				}
			}
			else if (version >= 2 && elemDef->elemType == Game::FX_ELEM_TYPE_SPARK_FOUNTAIN && elem.extended.sparkFountainDef)
			{
				elemDef->extended.sparkFountainDef = reader.ReadObject<Game::FxSparkFountainDef>();
			}
		}

		effect->totalSize += TotalSizeWidening(effect);
		return effect;
	}

	static rapidjson::Value StringOrNull(const char* text)
	{
		if (!text)
		{
			return rapidjson::Value(rapidjson::kNullType);
		}

		return rapidjson::Value(rapidjson::StringRef(text));
	}

	static rapidjson::Value ToJson(const Game::FxFloatRange& range, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);
		json.AddMember("base", range.base, allocator);
		json.AddMember("amplitude", range.amplitude, allocator);
		return json;
	}

	static rapidjson::Value ToJson(const Game::FxIntRange& range, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);
		json.AddMember("base", range.base, allocator);
		json.AddMember("amplitude", range.amplitude, allocator);
		return json;
	}

	static rapidjson::Value ToJson(const Game::FxFloatRange (&ranges)[3], Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kArrayType);

		for (const auto& range : ranges)
		{
			json.PushBack(ToJson(range, allocator), allocator);
		}

		return json;
	}

	static rapidjson::Value ToJson(const Game::FxElemVec3Range& range, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);
		json.AddMember("base", Utils::JSON::MakeArray(range.base, 3, allocator), allocator);
		json.AddMember("amplitude", Utils::JSON::MakeArray(range.amplitude, 3, allocator), allocator);
		return json;
	}

	static rapidjson::Value ToJson(const Game::FxElemVelStateInFrame& frame, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);
		json.AddMember("velocity", ToJson(frame.velocity, allocator), allocator);
		json.AddMember("totalDelta", ToJson(frame.totalDelta, allocator), allocator);
		return json;
	}

	static rapidjson::Value ToJson(const Game::FxElemVisualState& state, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);
		json.AddMember("color", Utils::JSON::MakeArray(state.color, 4, allocator), allocator);
		json.AddMember("rotationDelta", state.rotationDelta, allocator);
		json.AddMember("rotationTotal", state.rotationTotal, allocator);
		json.AddMember("size", Utils::JSON::MakeArray(state.size, 2, allocator), allocator);
		json.AddMember("scale", state.scale, allocator);
		return json;
	}

	static rapidjson::Value VisualsToJson(const Game::FxElemVisuals* visuals, char elemType, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kNullType);

		switch (elemType)
		{
		case Game::FX_ELEM_TYPE_MODEL:
		{
			if (visuals->model)
			{
				json.SetObject();
				json.AddMember("model", StringOrNull(visuals->model->name), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_XMODEL, visuals->model });
			}

			break;
		}

		case Game::FX_ELEM_TYPE_OMNI_LIGHT:
		case Game::FX_ELEM_TYPE_SPOT_LIGHT:
			break;

		case Game::FX_ELEM_TYPE_SOUND:
		{
			if (visuals->soundName)
			{
				json.SetObject();
				json.AddMember("sound", StringOrNull(visuals->soundName), allocator);

				if (Game::DB_FindXAssetEntry(Game::ASSET_TYPE_SOUND, visuals->soundName))
				{
					Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_SOUND, Game::DB_FindXAssetHeader(Game::ASSET_TYPE_SOUND, visuals->soundName) });
				}
			}

			break;
		}

		case Game::FX_ELEM_TYPE_RUNNER:
		{
			if (visuals->effectDef.handle)
			{
				json.SetObject();
				json.AddMember("runner", StringOrNull(visuals->effectDef.handle->name), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_FX, visuals->effectDef.handle });
			}

			break;
		}

		default:
		{
			if (visuals->material)
			{
				json.SetObject();
				json.AddMember("material", StringOrNull(visuals->material->info.name), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_MATERIAL, visuals->material });
			}

			break;
		}
		}

		return json;
	}

	static rapidjson::Value ElemDefToJson(const Game::FxElemDef* elemDef, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);

		json.AddMember("flags", elemDef->flags, allocator);

		rapidjson::Value spawn(rapidjson::kObjectType);
		spawn.AddMember("A", elemDef->spawn.looping.count, allocator);
		spawn.AddMember("B", elemDef->spawn.looping.intervalMsec, allocator);
		json.AddMember("spawn", spawn, allocator);

		json.AddMember("spawnRange", ToJson(elemDef->spawnRange, allocator), allocator);
		json.AddMember("fadeInRange", ToJson(elemDef->fadeInRange, allocator), allocator);
		json.AddMember("fadeOutRange", ToJson(elemDef->fadeOutRange, allocator), allocator);
		json.AddMember("spawnFrustumCullRadius", elemDef->spawnFrustumCullRadius, allocator);
		json.AddMember("spawnDelayMsec", ToJson(elemDef->spawnDelayMsec, allocator), allocator);
		json.AddMember("lifeSpanMsec", ToJson(elemDef->lifeSpanMsec, allocator), allocator);
		json.AddMember("spawnOrigin", ToJson(elemDef->spawnOrigin, allocator), allocator);
		json.AddMember("spawnOffsetRadius", ToJson(elemDef->spawnOffsetRadius, allocator), allocator);
		json.AddMember("spawnOffsetHeight", ToJson(elemDef->spawnOffsetHeight, allocator), allocator);
		json.AddMember("spawnAngles", ToJson(elemDef->spawnAngles, allocator), allocator);
		json.AddMember("angularVelocity", ToJson(elemDef->angularVelocity, allocator), allocator);
		json.AddMember("initialRotation", ToJson(elemDef->initialRotation, allocator), allocator);
		json.AddMember("gravity", ToJson(elemDef->gravity, allocator), allocator);
		json.AddMember("reflectionFactor", ToJson(elemDef->reflectionFactor, allocator), allocator);

		rapidjson::Value atlas(rapidjson::kObjectType);
		atlas.AddMember("behavior", elemDef->atlas.behavior, allocator);
		atlas.AddMember("index", elemDef->atlas.index, allocator);
		atlas.AddMember("fps", elemDef->atlas.fps, allocator);
		atlas.AddMember("loopCount", elemDef->atlas.loopCount, allocator);
		atlas.AddMember("colIndexBits", elemDef->atlas.colIndexBits, allocator);
		atlas.AddMember("rowIndexBits", elemDef->atlas.rowIndexBits, allocator);
		atlas.AddMember("entryCount", elemDef->atlas.entryCount, allocator);
		json.AddMember("atlas", atlas, allocator);

		json.AddMember("elemType", elemDef->elemType, allocator);
		json.AddMember("visualCount", elemDef->visualCount, allocator);
		json.AddMember("velIntervalCount", elemDef->velIntervalCount, allocator);
		json.AddMember("visStateIntervalCount", elemDef->visStateIntervalCount, allocator);

		if (elemDef->velSamples)
		{
			rapidjson::Value samples(rapidjson::kArrayType);

			for (unsigned int j = 0; j <= static_cast<unsigned char>(elemDef->velIntervalCount); ++j)
			{
				rapidjson::Value sample(rapidjson::kObjectType);
				sample.AddMember("local", ToJson(elemDef->velSamples[j].local, allocator), allocator);
				sample.AddMember("world", ToJson(elemDef->velSamples[j].world, allocator), allocator);
				samples.PushBack(sample, allocator);
			}

			json.AddMember("velSamples", samples, allocator);
		}

		if (elemDef->visSamples)
		{
			rapidjson::Value samples(rapidjson::kArrayType);

			for (unsigned int j = 0; j <= static_cast<unsigned char>(elemDef->visStateIntervalCount); ++j)
			{
				rapidjson::Value sample(rapidjson::kObjectType);
				sample.AddMember("base", ToJson(elemDef->visSamples[j].base, allocator), allocator);
				sample.AddMember("amplitude", ToJson(elemDef->visSamples[j].amplitude, allocator), allocator);
				samples.PushBack(sample, allocator);
			}

			json.AddMember("visSamples", samples, allocator);
		}

		const auto visualCount = static_cast<unsigned char>(elemDef->visualCount);

		if (elemDef->elemType == Game::FX_ELEM_TYPE_DECAL)
		{
			if (elemDef->visuals.markArray)
			{
				rapidjson::Value marks(rapidjson::kArrayType);

				for (unsigned char j = 0; j < visualCount; ++j)
				{
					rapidjson::Value materials(rapidjson::kArrayType);

					for (auto* const material : elemDef->visuals.markArray[j].materials)
					{
						if (!material)
						{
							materials.PushBack(rapidjson::Value(rapidjson::kNullType), allocator);
							continue;
						}

						materials.PushBack(StringOrNull(material->info.name), allocator);
						Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_MATERIAL, material });
					}

					marks.PushBack(materials, allocator);
				}

				json.AddMember("markArray", marks, allocator);
			}
		}
		else if (visualCount > 1)
		{
			if (elemDef->visuals.array)
			{
				rapidjson::Value visuals(rapidjson::kArrayType);

				for (unsigned char j = 0; j < visualCount; ++j)
				{
					visuals.PushBack(VisualsToJson(&elemDef->visuals.array[j], elemDef->elemType, allocator), allocator);
				}

				json.AddMember("visualsArray", visuals, allocator);
			}
		}
		else if (visualCount)
		{
			json.AddMember("instance", VisualsToJson(&elemDef->visuals.instance, elemDef->elemType, allocator), allocator);
		}

		json.AddMember("collBounds", Utils::JSON::ToJson(elemDef->collBounds, allocator), allocator);

		const std::pair<const char*, const Game::FxEffectDefRef*> references[] =
		{
			{ "effectOnImpact", &elemDef->effectOnImpact },
			{ "effectOnDeath", &elemDef->effectOnDeath },
			{ "effectEmitted", &elemDef->effectEmitted },
		};

		for (const auto& [key, reference] : references)
		{
			if (reference->handle)
			{
				json.AddMember(rapidjson::StringRef(key), StringOrNull(reference->handle->name), allocator);
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_FX, reference->handle });
			}
		}

		json.AddMember("emitDist", ToJson(elemDef->emitDist, allocator), allocator);
		json.AddMember("emitDistVariance", ToJson(elemDef->emitDistVariance, allocator), allocator);

		if (elemDef->elemType == Game::FX_ELEM_TYPE_TRAIL && elemDef->extended.trailDef)
		{
			const auto* const trail = elemDef->extended.trailDef;
			rapidjson::Value trailJson(rapidjson::kObjectType);

			trailJson.AddMember("scrollTimeMsec", trail->scrollTimeMsec, allocator);
			trailJson.AddMember("repeatDist", trail->repeatDist, allocator);
			trailJson.AddMember("invSplitDist", trail->invSplitDist, allocator);
			trailJson.AddMember("invSplitArcDist", trail->invSplitArcDist, allocator);
			trailJson.AddMember("invSplitTime", trail->invSplitTime, allocator);

			rapidjson::Value verts(rapidjson::kArrayType);

			for (int j = 0; j < trail->vertCount; ++j)
			{
				rapidjson::Value vert(rapidjson::kObjectType);
				vert.AddMember("pos", Utils::JSON::MakeArray(trail->verts[j].pos, 2, allocator), allocator);
				vert.AddMember("normal", Utils::JSON::MakeArray(trail->verts[j].normal, 2, allocator), allocator);
				vert.AddMember("texCoord", trail->verts[j].texCoord, allocator);
				verts.PushBack(vert, allocator);
			}

			trailJson.AddMember("verts", verts, allocator);
			trailJson.AddMember("inds", Utils::JSON::MakeArray(trail->inds, static_cast<std::size_t>(trail->indCount), allocator), allocator);

			json.AddMember("trailDef", trailJson, allocator);
		}
		else if (elemDef->elemType == Game::FX_ELEM_TYPE_SPARK_FOUNTAIN && elemDef->extended.sparkFountainDef)
		{
			const auto* const spark = elemDef->extended.sparkFountainDef;
			rapidjson::Value sparkJson(rapidjson::kObjectType);

			sparkJson.AddMember("gravity", spark->gravity, allocator);
			sparkJson.AddMember("bounceFrac", spark->bounceFrac, allocator);
			sparkJson.AddMember("bounceRand", spark->bounceRand, allocator);
			sparkJson.AddMember("sparkSpacing", spark->sparkSpacing, allocator);
			sparkJson.AddMember("sparkLength", spark->sparkLength, allocator);
			sparkJson.AddMember("sparkCount", spark->sparkCount, allocator);
			sparkJson.AddMember("loopTime", spark->loopTime, allocator);
			sparkJson.AddMember("velMin", spark->velMin, allocator);
			sparkJson.AddMember("velMax", spark->velMax, allocator);
			sparkJson.AddMember("velConeFrac", spark->velConeFrac, allocator);
			sparkJson.AddMember("restSpeed", spark->restSpeed, allocator);
			sparkJson.AddMember("boostTime", spark->boostTime, allocator);
			sparkJson.AddMember("boostFactor", spark->boostFactor, allocator);

			json.AddMember("sparkFountain", sparkJson, allocator);
		}

		json.AddMember("sortOrder", elemDef->sortOrder, allocator);
		json.AddMember("lightingFrac", elemDef->lightingFrac, allocator);
		json.AddMember("useItemClip", elemDef->useItemClip, allocator);
		json.AddMember("fadeInfo", elemDef->fadeInfo, allocator);

		return json;
	}

	void IFxEffectDef::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		if (!header->data)
		{
			this->LoadFromIW4OF(header, name, builder);
		}

		if (!header->data)
		{
			this->LoadNative(header, name, builder);
		}
	}

	void IFxEffectDef::LoadFromIW4OF(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		FxLoad load{ *builder->GetAllocator(), builder, name };

		Components::FileSystem::File jsonFile(std::format("fx/{}.iw4x.json", name));

		if (jsonFile.Exists())
		{
			header->fx = TryReadJsonEffect(load, jsonFile.GetBuffer());
			return;
		}

		Components::FileSystem::File binaryFile(std::format("fx/{}.iw4xFx", name));

		if (!binaryFile.Exists())
		{
			return;
		}

		Utils::Stream::Reader reader(builder->GetAllocator(), binaryFile.GetBuffer());

		try
		{
			header->fx = ReadBinaryEffect(load, reader);
		}
		catch (const std::runtime_error& error)
		{
			Components::Logger::Error("Reading fx '{}' failed, {}\n", name, error.what());
		}
	}

	void IFxEffectDef::LoadNative(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone*)
	{
		header->fx = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).fx;
	}

	void IFxEffectDef::MarkFxElemVisuals(const Game::FxElemVisuals* visuals, char elemType, Components::ZoneBuilder::Zone* builder)
	{
		switch (elemType)
		{
		case Game::FX_ELEM_TYPE_MODEL:
		{
			if (visuals->model)
			{
				builder->LoadAsset(Game::ASSET_TYPE_XMODEL, visuals->model);
			}

			break;
		}

		case Game::FX_ELEM_TYPE_OMNI_LIGHT:
		case Game::FX_ELEM_TYPE_SPOT_LIGHT:
			break;

		case Game::FX_ELEM_TYPE_SOUND:
		{
			if (visuals->soundName && Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_SOUND, visuals->soundName, builder, false).data)
			{
				builder->LoadAssetByName(Game::ASSET_TYPE_SOUND, visuals->soundName, false);
			}

			break;
		}

		case Game::FX_ELEM_TYPE_RUNNER:
		{
			if (visuals->effectDef.handle)
			{
				builder->LoadAsset(Game::ASSET_TYPE_FX, visuals->effectDef.handle, false);
			}

			break;
		}

		default:
		{
			if (visuals->material)
			{
				builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, visuals->material);
			}

			break;
		}
		}
	}

	void IFxEffectDef::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.fx;

		for (int i = 0; i < ElemDefCount(asset); ++i)
		{
			const auto* const elemDef = &asset->elemDefs[i];
			const auto visualCount = static_cast<unsigned char>(elemDef->visualCount);

			if (elemDef->elemType == Game::FX_ELEM_TYPE_DECAL)
			{
				if (elemDef->visuals.markArray)
				{
					for (unsigned char j = 0; j < visualCount; ++j)
					{
						for (auto* const material : elemDef->visuals.markArray[j].materials)
						{
							if (material)
							{
								builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, material);
							}
						}
					}
				}
			}
			else if (visualCount > 1)
			{
				if (elemDef->visuals.array)
				{
					for (unsigned char j = 0; j < visualCount; ++j)
					{
						this->MarkFxElemVisuals(&elemDef->visuals.array[j], elemDef->elemType, builder);
					}
				}
			}
			else
			{
				this->MarkFxElemVisuals(&elemDef->visuals.instance, elemDef->elemType, builder);
			}

			if (elemDef->effectOnImpact.handle)
			{
				builder->LoadAsset(Game::ASSET_TYPE_FX, elemDef->effectOnImpact.handle, false);
			}

			if (elemDef->effectOnDeath.handle)
			{
				builder->LoadAsset(Game::ASSET_TYPE_FX, elemDef->effectOnDeath.handle, false);
			}

			if (elemDef->effectEmitted.handle)
			{
				builder->LoadAsset(Game::ASSET_TYPE_FX, elemDef->effectEmitted.handle, false);
			}
		}
	}

	void IFxEffectDef::SaveFxElemVisuals(const Game::FxElemVisuals* visuals, Game::X86::FxElemVisuals* destVisuals, char elemType, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();

		switch (elemType)
		{
		case Game::FX_ELEM_TYPE_MODEL:
		{
			if (visuals->model)
			{
				destVisuals->model = builder->SaveSubAsset(Game::ASSET_TYPE_XMODEL, visuals->model);
			}

			break;
		}

		case Game::FX_ELEM_TYPE_OMNI_LIGHT:
		case Game::FX_ELEM_TYPE_SPOT_LIGHT:
			break;

		case Game::FX_ELEM_TYPE_SOUND:
		{
			if (visuals->soundName)
			{
				buffer->SaveString(visuals->soundName);
				Utils::Stream::ClearPointer(&destVisuals->soundName);
			}

			break;
		}

		case Game::FX_ELEM_TYPE_RUNNER:
		{
			if (visuals->effectDef.handle)
			{
				buffer->SaveString(visuals->effectDef.handle->name);
				Utils::Stream::ClearPointer(&destVisuals->effectDef.handle);
			}

			break;
		}

		default:
		{
			if (visuals->material)
			{
				destVisuals->material = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, visuals->material);
			}

			break;
		}
		}
	}

	void IFxEffectDef::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::FxEffectDef, 32);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.fx;
		auto* const dest = buffer->Dest<Game::X86::FxEffectDef>();
		auto record = Game::X86::Convert(*asset);
		record.totalSize = asset->totalSize - TotalSizeWidening(asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->elemDefs)
		{
			AssertSize(Game::X86::FxElemDef, 252);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destElemDefs = buffer->Dest<Game::X86::FxElemDef>();

			for (int i = 0; i < ElemDefCount(asset); ++i)
			{
				const auto elemRecord = ConvertElemDef(asset->elemDefs[i]);
				buffer->Save(&elemRecord);
			}

			for (int i = 0; i < ElemDefCount(asset); ++i)
			{
				auto* const destElemDef = &destElemDefs[i];
				const auto* const elemDef = &asset->elemDefs[i];
				const auto visualCount = static_cast<unsigned char>(elemDef->visualCount);

				if (elemDef->velSamples)
				{
					AssertSize(Game::FxElemVelStateSample, 96);
					AssertSize(Game::X86::FxElemVelStateSample, 96);

					buffer->Align(Utils::Stream::ALIGN_4);

					buffer->SaveArray(elemDef->velSamples, static_cast<unsigned char>(elemDef->velIntervalCount) + 1u);

					Utils::Stream::ClearPointer(&destElemDef->velSamples);
				}

				if (elemDef->visSamples)
				{
					AssertSize(Game::FxElemVisStateSample, 48);
					AssertSize(Game::X86::FxElemVisStateSample, 48);

					buffer->Align(Utils::Stream::ALIGN_4);

					buffer->SaveArray(elemDef->visSamples, static_cast<unsigned char>(elemDef->visStateIntervalCount) + 1u);

					Utils::Stream::ClearPointer(&destElemDef->visSamples);
				}

				if (elemDef->elemType == Game::FX_ELEM_TYPE_DECAL)
				{
					if (elemDef->visuals.markArray)
					{
						AssertSize(Game::X86::FxElemMarkVisuals, 8);

						buffer->Align(Utils::Stream::ALIGN_4);

						auto* const destMarkArray = buffer->Dest<Game::X86::FxElemMarkVisuals>();
						const std::vector<Game::X86::FxElemMarkVisuals> emptyMarks(visualCount);
						buffer->SaveArray(emptyMarks.data(), emptyMarks.size());

						for (unsigned char j = 0; j < visualCount; ++j)
						{
							for (int k = 0; k < 2; ++k)
							{
								if (elemDef->visuals.markArray[j].materials[k])
								{
									destMarkArray[j].materials[k] = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, elemDef->visuals.markArray[j].materials[k]);
								}
							}
						}

						Utils::Stream::ClearPointer(&destElemDef->visuals.markArray);
					}
				}
				else if (visualCount > 1)
				{
					if (elemDef->visuals.array)
					{
						AssertSize(Game::X86::FxElemVisuals, 4);

						buffer->Align(Utils::Stream::ALIGN_4);

						auto* const destVisuals = buffer->Dest<Game::X86::FxElemVisuals>();
						const std::vector<Game::X86::FxElemVisuals> emptyVisuals(visualCount);
						buffer->SaveArray(emptyVisuals.data(), emptyVisuals.size());

						for (unsigned char j = 0; j < visualCount; ++j)
						{
							this->SaveFxElemVisuals(&elemDef->visuals.array[j], &destVisuals[j], elemDef->elemType, builder);
						}

						Utils::Stream::ClearPointer(&destElemDef->visuals.array);
					}
				}
				else
				{
					this->SaveFxElemVisuals(&elemDef->visuals.instance, &destElemDef->visuals.instance, elemDef->elemType, builder);
				}

				if (elemDef->effectOnImpact.handle)
				{
					buffer->SaveString(elemDef->effectOnImpact.handle->name);
					Utils::Stream::ClearPointer(&destElemDef->effectOnImpact.handle);
				}

				if (elemDef->effectOnDeath.handle)
				{
					buffer->SaveString(elemDef->effectOnDeath.handle->name);
					Utils::Stream::ClearPointer(&destElemDef->effectOnDeath.handle);
				}

				if (elemDef->effectEmitted.handle)
				{
					buffer->SaveString(elemDef->effectEmitted.handle->name);
					Utils::Stream::ClearPointer(&destElemDef->effectEmitted.handle);
				}

				AssertSize(Game::X86::FxElemExtendedDefPtr, 4);

				if (elemDef->elemType == Game::FX_ELEM_TYPE_TRAIL)
				{
					if (elemDef->extended.trailDef)
					{
						AssertSize(Game::X86::FxTrailDef, 36);

						buffer->Align(Utils::Stream::ALIGN_4);

						const auto* const trailDef = elemDef->extended.trailDef;
						auto* const destTrailDef = buffer->Dest<Game::X86::FxTrailDef>();
						const auto trailRecord = Game::X86::Convert(*trailDef);
						buffer->Save(&trailRecord);

						if (trailDef->verts)
						{
							AssertSize(Game::FxTrailVertex, 20);
							AssertSize(Game::X86::FxTrailVertex, 20);

							buffer->Align(Utils::Stream::ALIGN_4);

							buffer->SaveArray(trailDef->verts, static_cast<std::size_t>(trailDef->vertCount));
							Utils::Stream::ClearPointer(&destTrailDef->verts);
						}

						if (trailDef->inds)
						{
							buffer->Align(Utils::Stream::ALIGN_2);

							buffer->SaveArray(trailDef->inds, static_cast<std::size_t>(trailDef->indCount));
							Utils::Stream::ClearPointer(&destTrailDef->inds);
						}

						Utils::Stream::ClearPointer(&destElemDef->extended.trailDef);
					}
				}
				else if (elemDef->elemType == Game::FX_ELEM_TYPE_SPARK_FOUNTAIN)
				{
					if (elemDef->extended.sparkFountainDef)
					{
						AssertSize(Game::FxSparkFountainDef, 52);
						AssertSize(Game::X86::FxSparkFountainDef, 52);

						buffer->Align(Utils::Stream::ALIGN_4);

						buffer->Save(elemDef->extended.sparkFountainDef);
						Utils::Stream::ClearPointer(&destElemDef->extended.sparkFountainDef);
					}
				}
				else if (elemDef->extended.unknownDef)
				{
					buffer->Save(elemDef->extended.unknownDef, 1);
					Utils::Stream::ClearPointer(&destElemDef->extended.unknownDef);
				}
			}

			Utils::Stream::ClearPointer(&dest->elemDefs);
		}

		buffer->PopBlock();
	}

	void IFxEffectDef::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.fx;
		const auto path = std::format("{}/fx/{}.iw4x.json", Components::ZoneBuilder::GetDumpingZonePath(), asset->name);

		if (!dumpedPaths.insert(path).second)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", fxFileVersion, allocator);
		output.AddMember("name", StringOrNull(asset->name), allocator);
		output.AddMember("flags", asset->flags, allocator);
		output.AddMember("totalSize", asset->totalSize - TotalSizeWidening(asset), allocator);
		output.AddMember("msecLoopingLife", asset->msecLoopingLife, allocator);
		output.AddMember("elemDefCountLooping", asset->elemDefCountLooping, allocator);
		output.AddMember("elemDefCountOneShot", asset->elemDefCountOneShot, allocator);
		output.AddMember("elemDefCountEmission", asset->elemDefCountEmission, allocator);

		rapidjson::Value elems(rapidjson::kArrayType);

		if (asset->elemDefs)
		{
			for (int i = 0; i < ElemDefCount(asset); ++i)
			{
				elems.PushBack(ElemDefToJson(&asset->elemDefs[i], allocator), allocator);
			}
		}

		output.AddMember("elemDefs", elems, allocator);

		rapidjson::StringBuffer text;
		rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfNullFlag | rapidjson::kWriteNanAndInfFlag> writer(text);
		output.Accept(writer);

		if (!Utils::IO::WriteFile(path, text.GetString()))
		{
			Components::Logger::Error("fx: could not write {}\n", path);
		}
	}
}
