#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "IFxWorld.hpp"

#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#define IW4X_FXWORLD_VERSION 1

namespace Assets
{
	static std::string GetFileName(const std::string& name)
	{
		std::string baseName = name;
		Utils::String::Replace(baseName, "maps/mp/", "");
		Utils::String::Replace(baseName, ".d3dbsp", "");

		return std::format("fxworld/{}.iw4x.json", baseName);
	}

	static bool IsArrayMember(const rapidjson::Value& json, const char* name, rapidjson::SizeType minimumSize)
	{
		return json.HasMember(name) && json[name].IsArray() && json[name].Size() >= minimumSize;
	}

	static bool IsStringMember(const rapidjson::Value& json, const char* name)
	{
		return json.HasMember(name) && json[name].IsString();
	}

	static bool TryReadGlassDef(const rapidjson::Value& json, Game::FxGlassDef* def, Components::ZoneBuilder::Zone* builder)
	{
		if (!json.IsObject() || !IsArrayMember(json, "texVecs", 4))
		{
			return false;
		}

		if (!IsStringMember(json, "material") || !IsStringMember(json, "materialShattered") || !IsStringMember(json, "physPreset"))
		{
			return false;
		}

		def->halfThickness = json["halfThickness"].Get<float>();

		def->texVecs[0][0] = json["texVecs"][0].Get<float>();
		def->texVecs[0][1] = json["texVecs"][1].Get<float>();
		def->texVecs[1][0] = json["texVecs"][2].Get<float>();
		def->texVecs[1][1] = json["texVecs"][3].Get<float>();

		def->color.packed = json["color"].Get<std::uint32_t>();

		def->material = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, json["material"].GetString(), builder).material;
		def->materialShattered = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_MATERIAL, json["materialShattered"].GetString(), builder).material;
		def->physPreset = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_PHYSPRESET, json["physPreset"].GetString(), builder).physPreset;

		return def->material && def->materialShattered && def->physPreset;
	}

	static bool TryReadInitPieceState(const rapidjson::Value& json, Game::FxGlassInitPieceState* initial)
	{
		if (!json.IsObject() || !json.HasMember("frame") || !json["frame"].IsObject())
		{
			return false;
		}

		const auto& frame = json["frame"];

		if (!IsArrayMember(frame, "quat", 4) || !IsArrayMember(frame, "origin", 3) || !IsArrayMember(json, "texCoordOrigin", 2))
		{
			return false;
		}

		Utils::JSON::CopyArray(initial->frame.quat, frame["quat"], 4);
		Utils::JSON::CopyArray(initial->frame.origin, frame["origin"], 3);

		initial->radius = json["radius"].Get<float>();
		Utils::JSON::CopyArray(initial->texCoordOrigin, json["texCoordOrigin"], 2);
		initial->supportMask = json["supportMask"].Get<std::uint32_t>();
		initial->areaX2 = json["areaX2"].Get<float>();
		initial->defIndex = json["defIndex"].Get<std::uint8_t>();
		initial->vertCount = json["vertCount"].Get<std::uint8_t>();
		initial->fanDataCount = json["fanDataCount"].Get<std::uint8_t>();

		return true;
	}

	static Game::FxWorld* TryReadFxWorld(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetFileName(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		rapidjson::Document json;
		json.Parse(file.GetBuffer().data());

		if (json.HasParseError() || !json.IsObject())
		{
			Components::Logger::Error("Invalid FXWORLD json for {}\n", name);
			return nullptr;
		}

		std::int32_t version = 0;

		if (json.HasMember("version") && json["version"].IsNumber())
		{
			version = json["version"].Get<std::int32_t>();
		}

		if (version > IW4X_FXWORLD_VERSION)
		{
			Components::Logger::Error("Invalid FXWORLD json version for {}, expected {} and got {}\n", name, IW4X_FXWORLD_VERSION, version);
			return nullptr;
		}

		if (!json.HasMember("glassSys") || !json["glassSys"].IsObject())
		{
			Components::Logger::Error("Malformed FXWORLD json for {}\n", name);
			return nullptr;
		}

		const auto& glassSysJson = json["glassSys"];

		if (!IsArrayMember(glassSysJson, "defs", 0) || !IsArrayMember(glassSysJson, "initPieceStates", 0) || !IsArrayMember(glassSysJson, "initGeoData", 0))
		{
			Components::Logger::Error("Malformed FXWORLD json for {}\n", name);
			return nullptr;
		}

		auto* const allocator = builder->GetAllocator();

		auto* const map = allocator->Allocate<Game::FxWorld>();
		map->name = allocator->DuplicateString(name);

		auto* const glassSys = &map->glassSys;

		glassSys->time = glassSysJson["time"].Get<std::int32_t>();
		glassSys->prevTime = glassSysJson["prevTime"].Get<std::int32_t>();
		glassSys->defCount = glassSysJson["defCount"].Get<std::uint32_t>();
		glassSys->pieceLimit = glassSysJson["pieceLimit"].Get<std::uint32_t>();
		glassSys->pieceWordCount = glassSysJson["pieceWordCount"].Get<std::uint32_t>();
		glassSys->initPieceCount = glassSysJson["initPieceCount"].Get<std::uint32_t>();
		glassSys->cellCount = glassSysJson["cellCount"].Get<std::uint32_t>();
		glassSys->activePieceCount = glassSysJson["activePieceCount"].Get<std::uint32_t>();
		glassSys->firstFreePiece = glassSysJson["firstFreePiece"].Get<std::uint32_t>();
		glassSys->geoDataLimit = glassSysJson["geoDataLimit"].Get<std::uint32_t>();
		glassSys->geoDataCount = glassSysJson["geoDataCount"].Get<std::uint32_t>();
		glassSys->initGeoDataCount = glassSysJson["initGeoDataCount"].Get<std::uint32_t>();

		const auto& defs = glassSysJson["defs"];
		const auto& initPieceStates = glassSysJson["initPieceStates"];
		const auto& initGeoData = glassSysJson["initGeoData"];

		if (defs.Size() > glassSys->defCount || initPieceStates.Size() > glassSys->initPieceCount || initGeoData.Size() > glassSys->initGeoDataCount)
		{
			Components::Logger::Error("Malformed FXWORLD json for {}, an array is longer than its count\n", name);
			return nullptr;
		}

		glassSys->defs = allocator->AllocateArray<Game::FxGlassDef>(glassSys->defCount);

		for (rapidjson::SizeType i = 0; i < defs.Size(); ++i)
		{
			if (!TryReadGlassDef(defs[i], &glassSys->defs[i], builder))
			{
				Components::Logger::Error("Malformed FXWORLD json for {}, glass def {} could not be read\n", name, i);
				return nullptr;
			}
		}

		glassSys->initPieceStates = allocator->AllocateArray<Game::FxGlassInitPieceState>(glassSys->initPieceCount);

		for (rapidjson::SizeType i = 0; i < initPieceStates.Size(); ++i)
		{
			if (!TryReadInitPieceState(initPieceStates[i], &glassSys->initPieceStates[i]))
			{
				Components::Logger::Error("Malformed FXWORLD json for {}, init piece {} could not be read\n", name, i);
				return nullptr;
			}
		}

		glassSys->initGeoData = allocator->AllocateArray<Game::FxGlassGeometryData>(glassSys->initGeoDataCount);

		for (rapidjson::SizeType i = 0; i < initGeoData.Size(); ++i)
		{
			const auto& member = initGeoData[i];

			if (!member.IsArray() || member.Size() < 2)
			{
				Components::Logger::Error("Malformed FXWORLD json for {}, geo data {} could not be read\n", name, i);
				return nullptr;
			}

			glassSys->initGeoData[i].anonymous[0] = member[0].Get<short>();
			glassSys->initGeoData[i].anonymous[1] = member[1].Get<short>();
		}

		glassSys->piecePlaces = allocator->AllocateArray<Game::FxGlassPiecePlace>(glassSys->pieceLimit);
		glassSys->pieceStates = allocator->AllocateArray<Game::FxGlassPieceState>(glassSys->pieceLimit);
		glassSys->pieceDynamics = allocator->AllocateArray<Game::FxGlassPieceDynamics>(glassSys->pieceLimit);
		glassSys->geoData = allocator->AllocateArray<Game::FxGlassGeometryData>(glassSys->geoDataLimit);
		glassSys->isInUse = allocator->AllocateArray<unsigned int>(glassSys->pieceWordCount);
		glassSys->cellBits = allocator->AllocateArray<unsigned int>(glassSys->pieceWordCount * glassSys->cellCount);
		glassSys->visData = allocator->AllocateArray<char>((glassSys->pieceLimit + 15) & 0xFFFFFFF0);
		glassSys->linkOrg = reinterpret_cast<float(*)[3]>(allocator->AllocateArray<float>(glassSys->pieceLimit * 3));
		glassSys->halfThickness = allocator->AllocateArray<float>(glassSys->pieceLimit * 3);
		glassSys->lightingHandles = allocator->AllocateArray<unsigned short>(glassSys->initPieceCount);

		return map;
	}

	void IFxWorld::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		if (!header->fxWorld)
		{
			this->LoadFromDisk(header, name, builder);
		}

		if (!header->fxWorld && !Components::FileSystem::File(GetFileName(name)).Exists())
		{
			this->Generate(header, name, builder);
		}
	}

	void IFxWorld::LoadFromDisk(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->fxWorld = TryReadFxWorld(name, builder);
	}

	void IFxWorld::Generate(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		if (Components::AssetHandler::FindLoadedAsset(Game::ASSET_TYPE_FXWORLD, name.data()).data)
		{
			return;
		}

		auto* const map = builder->GetAllocator()->Allocate<Game::FxWorld>();
		map->name = builder->GetAllocator()->DuplicateString(name);

		std::memset(&map->glassSys, 0, sizeof(map->glassSys));
		map->glassSys.firstFreePiece = 0xFFFF;

		header->fxWorld = map;
	}

	void IFxWorld::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.fxWorld;

		if (!asset->glassSys.defs)
		{
			return;
		}

		for (unsigned int i = 0; i < asset->glassSys.defCount; ++i)
		{
			const auto& def = asset->glassSys.defs[i];

			if (def.physPreset)
			{
				builder->LoadAsset(Game::ASSET_TYPE_PHYSPRESET, def.physPreset);
			}

			if (def.material)
			{
				builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, def.material);
			}

			if (def.materialShattered)
			{
				builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, def.materialShattered);
			}
		}
	}

	void IFxWorld::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::FxWorld, 116);

		auto* const buffer = builder->GetBuffer();
		SaveLogEnter("FxWorld");

		const auto* const asset = header.fxWorld;
		auto* const dest = buffer->Dest<Game::X86::FxWorld>();

		const auto world = Game::X86::Convert(*asset);
		buffer->Save(&world);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		AssertSize(Game::X86::FxGlassSystem, 112);

		const auto& glassSys = asset->glassSys;

		if (glassSys.defs)
		{
			AssertSize(Game::X86::FxGlassDef, 36);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const glassDefTable = buffer->Dest<Game::X86::FxGlassDef>();

			for (unsigned int i = 0; i < glassSys.defCount; ++i)
			{
				const auto glassDef = Game::X86::Convert(glassSys.defs[i]);
				buffer->Save(&glassDef);
			}

			for (unsigned int i = 0; i < glassSys.defCount; ++i)
			{
				const auto& glassDef = glassSys.defs[i];
				auto* const destGlassDef = &glassDefTable[i];

				if (glassDef.physPreset)
				{
					destGlassDef->physPreset = builder->SaveSubAsset(Game::ASSET_TYPE_PHYSPRESET, glassDef.physPreset);
				}

				if (glassDef.material)
				{
					destGlassDef->material = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, glassDef.material);
				}

				if (glassDef.materialShattered)
				{
					destGlassDef->materialShattered = builder->SaveSubAsset(Game::ASSET_TYPE_MATERIAL, glassDef.materialShattered);
				}
			}

			Utils::Stream::ClearPointer(&dest->glassSys.defs);
		}

		buffer->PushBlock(Game::XFILE_BLOCK_RUNTIME);

		if (glassSys.piecePlaces)
		{
			AssertSize(Game::X86::FxGlassPiecePlace, 32);

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(glassSys.piecePlaces, sizeof(Game::X86::FxGlassPiecePlace), glassSys.pieceLimit);
			Utils::Stream::ClearPointer(&dest->glassSys.piecePlaces);
		}

		if (glassSys.pieceStates)
		{
			AssertSize(Game::X86::FxGlassPieceState, 32);

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(glassSys.pieceStates, sizeof(Game::X86::FxGlassPieceState), glassSys.pieceLimit);
			Utils::Stream::ClearPointer(&dest->glassSys.pieceStates);
		}

		if (glassSys.pieceDynamics)
		{
			AssertSize(Game::X86::FxGlassPieceDynamics, 36);

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(glassSys.pieceDynamics, sizeof(Game::X86::FxGlassPieceDynamics), glassSys.pieceLimit);
			Utils::Stream::ClearPointer(&dest->glassSys.pieceDynamics);
		}

		if (glassSys.geoData)
		{
			AssertSize(Game::X86::FxGlassGeometryData, 4);

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->Save(glassSys.geoData, sizeof(Game::X86::FxGlassGeometryData), glassSys.geoDataLimit);
			Utils::Stream::ClearPointer(&dest->glassSys.geoData);
		}

		if (glassSys.isInUse)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(glassSys.isInUse, glassSys.pieceWordCount);
			Utils::Stream::ClearPointer(&dest->glassSys.isInUse);
		}

		if (glassSys.cellBits)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(glassSys.cellBits, glassSys.pieceWordCount * glassSys.cellCount);
			Utils::Stream::ClearPointer(&dest->glassSys.cellBits);
		}

		if (glassSys.visData)
		{
			buffer->Align(Utils::Stream::ALIGN_16);
			buffer->Save(glassSys.visData, 1, (glassSys.pieceLimit + 15) & 0xFFFFFFF0);
			Utils::Stream::ClearPointer(&dest->glassSys.visData);
		}

		if (glassSys.linkOrg)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(glassSys.linkOrg, glassSys.pieceLimit);
			Utils::Stream::ClearPointer(&dest->glassSys.linkOrg);
		}

		if (glassSys.halfThickness)
		{
			buffer->Align(Utils::Stream::ALIGN_16);
			buffer->Save(glassSys.halfThickness, 1, ((4 * glassSys.pieceLimit) + 12) & 0xFFFFFFF0);
			Utils::Stream::ClearPointer(&dest->glassSys.halfThickness);
		}

		buffer->PopBlock();

		if (glassSys.lightingHandles)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(glassSys.lightingHandles, glassSys.initPieceCount);
			Utils::Stream::ClearPointer(&dest->glassSys.lightingHandles);
		}

		if (glassSys.initPieceStates)
		{
			AssertSize(Game::X86::FxGlassInitPieceState, 52);
			static_assert(sizeof(Game::FxGlassInitPieceState) == sizeof(Game::X86::FxGlassInitPieceState));

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(glassSys.initPieceStates, glassSys.initPieceCount);
			Utils::Stream::ClearPointer(&dest->glassSys.initPieceStates);
		}

		if (glassSys.initGeoData)
		{
			static_assert(sizeof(Game::FxGlassGeometryData) == sizeof(Game::X86::FxGlassGeometryData));

			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(glassSys.initGeoData, glassSys.initGeoDataCount);
			Utils::Stream::ClearPointer(&dest->glassSys.initGeoData);
		}

		SaveLogExit();
		buffer->PopBlock();
	}

	void IFxWorld::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.fxWorld;

		if (!asset->name)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", IW4X_FXWORLD_VERSION, allocator);
		output.AddMember("name", rapidjson::StringRef(asset->name), allocator);

		const auto& glassSys = asset->glassSys;
		rapidjson::Value glassSystem(rapidjson::kObjectType);

		glassSystem.AddMember("time", glassSys.time, allocator);
		glassSystem.AddMember("prevTime", glassSys.prevTime, allocator);
		glassSystem.AddMember("defCount", glassSys.defCount, allocator);
		glassSystem.AddMember("pieceLimit", glassSys.pieceLimit, allocator);
		glassSystem.AddMember("pieceWordCount", glassSys.pieceWordCount, allocator);
		glassSystem.AddMember("initPieceCount", glassSys.initPieceCount, allocator);
		glassSystem.AddMember("cellCount", glassSys.cellCount, allocator);
		glassSystem.AddMember("activePieceCount", glassSys.activePieceCount, allocator);
		glassSystem.AddMember("firstFreePiece", glassSys.firstFreePiece, allocator);
		glassSystem.AddMember("geoDataLimit", glassSys.geoDataLimit, allocator);
		glassSystem.AddMember("geoDataCount", glassSys.geoDataCount, allocator);
		glassSystem.AddMember("initGeoDataCount", glassSys.initGeoDataCount, allocator);

		rapidjson::Value defs(rapidjson::kArrayType);

		for (unsigned int i = 0; glassSys.defs && i < glassSys.defCount; ++i)
		{
			const auto& def = glassSys.defs[i];
			rapidjson::Value defJson(rapidjson::kObjectType);

			defJson.AddMember("halfThickness", def.halfThickness, allocator);

			rapidjson::Value texVecs(rapidjson::kArrayType);
			texVecs.PushBack(def.texVecs[0][0], allocator);
			texVecs.PushBack(def.texVecs[0][1], allocator);
			texVecs.PushBack(def.texVecs[1][0], allocator);
			texVecs.PushBack(def.texVecs[1][1], allocator);

			defJson.AddMember("texVecs", texVecs, allocator);
			defJson.AddMember("color", def.color.packed, allocator);

			if (def.material)
			{
				defJson.AddMember("material", rapidjson::StringRef(def.material->info.name), allocator);
			}
			else
			{
				defJson.AddMember("material", rapidjson::Value(rapidjson::kNullType), allocator);
			}

			if (def.materialShattered)
			{
				defJson.AddMember("materialShattered", rapidjson::StringRef(def.materialShattered->info.name), allocator);
			}
			else
			{
				defJson.AddMember("materialShattered", rapidjson::Value(rapidjson::kNullType), allocator);
			}

			if (def.physPreset)
			{
				defJson.AddMember("physPreset", rapidjson::StringRef(def.physPreset->name), allocator);
			}
			else
			{
				defJson.AddMember("physPreset", rapidjson::Value(rapidjson::kNullType), allocator);
			}

			defs.PushBack(defJson, allocator);
		}

		glassSystem.AddMember("defs", defs, allocator);

		rapidjson::Value initPieces(rapidjson::kArrayType);

		for (unsigned int i = 0; glassSys.initPieceStates && i < glassSys.initPieceCount; ++i)
		{
			const auto& initPieceState = glassSys.initPieceStates[i];
			rapidjson::Value initPieceJson(rapidjson::kObjectType);

			rapidjson::Value spatialFrame(rapidjson::kObjectType);
			spatialFrame.AddMember("quat", Utils::JSON::MakeArray(initPieceState.frame.quat, 4, allocator), allocator);
			spatialFrame.AddMember("origin", Utils::JSON::MakeArray(initPieceState.frame.origin, 3, allocator), allocator);

			initPieceJson.AddMember("frame", spatialFrame, allocator);
			initPieceJson.AddMember("radius", initPieceState.radius, allocator);
			initPieceJson.AddMember("texCoordOrigin", Utils::JSON::MakeArray(initPieceState.texCoordOrigin, 2, allocator), allocator);
			initPieceJson.AddMember("supportMask", initPieceState.supportMask, allocator);
			initPieceJson.AddMember("areaX2", initPieceState.areaX2, allocator);
			initPieceJson.AddMember("defIndex", initPieceState.defIndex, allocator);
			initPieceJson.AddMember("vertCount", initPieceState.vertCount, allocator);
			initPieceJson.AddMember("fanDataCount", initPieceState.fanDataCount, allocator);

			initPieces.PushBack(initPieceJson, allocator);
		}

		glassSystem.AddMember("initPieceStates", initPieces, allocator);

		rapidjson::Value initGeoJson(rapidjson::kArrayType);

		for (unsigned int i = 0; glassSys.initGeoData && i < glassSys.initGeoDataCount; ++i)
		{
			initGeoJson.PushBack(Utils::JSON::MakeArray(glassSys.initGeoData[i].anonymous, 2, allocator), allocator);
		}

		glassSystem.AddMember("initGeoData", initGeoJson, allocator);
		output.AddMember("glassSys", glassSystem, allocator);

		rapidjson::StringBuffer stringBuffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(stringBuffer);
		output.Accept(writer);

		const auto path = std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetFileName(asset->name));

		if (!Utils::IO::WriteFile(path, stringBuffer.GetString()))
		{
			Components::Logger::Error("Dumping fxworld '{}' failed, could not write {}\n", asset->name, path);
		}
	}
}
