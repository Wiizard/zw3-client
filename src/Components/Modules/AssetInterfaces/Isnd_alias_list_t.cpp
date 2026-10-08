#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "../AssetHandler.hpp"
#include "../Command.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"
#include "Isnd_alias_list_t.hpp"

namespace Assets
{
	constexpr char loadedSoundType = 1;
	constexpr char streamedSoundType = 2;
	constexpr std::uint32_t maxSpeakers = 6;
	constexpr std::int32_t maxLevels = 2;

	static std::string GetAliasPath(const std::string& name)
	{
		return std::format("sounds/{}.json", name);
	}

	static rapidjson::Value StringOrNull(const char* text, Utils::JSON::Allocator& allocator)
	{
		if (!text)
		{
			return rapidjson::Value(rapidjson::kNullType);
		}

		return rapidjson::Value(text, allocator);
	}

	template <typename T> static bool TryReadNumber(const rapidjson::Value& object, const char* key, T& value)
	{
		if (!object.HasMember(key) || object[key].IsNull())
		{
			value = T{};
			return true;
		}

		if (!object[key].IsNumber())
		{
			return false;
		}

		value = static_cast<T>(object[key].GetDouble());
		return true;
	}

	static const char* DuplicateIfString(const rapidjson::Value& object, const char* key, Utils::Memory::Allocator* allocator)
	{
		if (!object.HasMember(key) || !object[key].IsString())
		{
			return nullptr;
		}

		return allocator->DuplicateString(object[key].GetString());
	}

	static Game::SpeakerMap* TryReadSpeakerMap(const rapidjson::Value& json, Utils::Memory::Allocator* allocator)
	{
		if (!json.IsObject() || !json.HasMember("name") || !json["name"].IsString())
		{
			return nullptr;
		}

		auto* const speakerMap = allocator->Allocate<Game::SpeakerMap>();
		speakerMap->name = allocator->DuplicateString(json["name"].GetString());

		if (json.HasMember("isDefault") && json["isDefault"].IsBool())
		{
			speakerMap->isDefault = json["isDefault"].GetBool();
		}

		if (!json.HasMember("channelMaps") || !json["channelMaps"].IsArray())
		{
			return speakerMap;
		}

		const auto& channelMaps = json["channelMaps"];

		if (channelMaps.Size() != 4)
		{
			return nullptr;
		}

		for (std::size_t channelMapIndex = 0; channelMapIndex < 2; ++channelMapIndex)
		{
			for (std::size_t subChannelIndex = 0; subChannelIndex < 2; ++subChannelIndex)
			{
				const auto& channelMap = channelMaps[static_cast<rapidjson::SizeType>(channelMapIndex * 2 + subChannelIndex)];

				if (!channelMap.IsObject() || !channelMap.HasMember("speakers") || !channelMap["speakers"].IsArray() || channelMap["speakers"].Size() > maxSpeakers)
				{
					return nullptr;
				}

				std::vector<Game::SpeakerMapEntry> entries;

				for (const auto& speakerJson : channelMap["speakers"].GetArray())
				{
					float levels[maxLevels]{};
					std::int32_t numLevels = 0;
					std::int32_t speaker = 0;

					const bool isRead = speakerJson.IsObject()
						&& TryReadNumber(speakerJson, "levels0", levels[0])
						&& TryReadNumber(speakerJson, "levels1", levels[1])
						&& TryReadNumber(speakerJson, "numLevels", numLevels)
						&& TryReadNumber(speakerJson, "speaker", speaker);

					if (!isRead || numLevels < 0 || numLevels > maxLevels || speaker < 0 || speaker > 255)
					{
						return nullptr;
					}

					for (std::int32_t level = 0; level < numLevels; ++level)
					{
						entries.push_back({ static_cast<std::uint8_t>(level), static_cast<std::uint8_t>(speaker), {}, levels[level] });
					}
				}

				auto& channel = speakerMap->channelMaps[channelMapIndex][subChannelIndex];
				channel.entryCount = static_cast<std::uint8_t>(entries.size());

				if (!entries.empty())
				{
					channel.entries = allocator->AllocateArray<Game::SpeakerMapEntry>(entries.size());
					std::memcpy(channel.entries, entries.data(), entries.size() * sizeof(Game::SpeakerMapEntry));
				}
			}
		}

		return speakerMap;
	}

	static bool TryConvertSpeakerMap(const Game::SpeakerMap& speakerMap, Game::X86::SpeakerMap& converted)
	{
		converted = {};
		converted.isDefault = speakerMap.isDefault;

		for (std::size_t channelMapIndex = 0; channelMapIndex < 2; ++channelMapIndex)
		{
			for (std::size_t subChannelIndex = 0; subChannelIndex < 2; ++subChannelIndex)
			{
				const auto& channel = speakerMap.channelMaps[channelMapIndex][subChannelIndex];
				auto& target = converted.channelMaps[channelMapIndex][subChannelIndex];

				for (std::size_t entryIndex = 0; entryIndex < channel.entryCount; ++entryIndex)
				{
					const auto& entry = channel.entries[entryIndex];

					if (entry.level >= maxLevels)
					{
						return false;
					}

					std::uint32_t slot = 0;

					while (slot < target.speakerCount && target.speakers[slot].speaker != entry.speaker)
					{
						++slot;
					}

					if (slot == target.speakerCount)
					{
						if (slot == maxSpeakers)
						{
							return false;
						}

						target.speakers[slot].speaker = entry.speaker;
						++target.speakerCount;
					}

					auto& speaker = target.speakers[slot];
					speaker.levels[entry.level] = entry.value;
					speaker.numLevels = std::max(speaker.numLevels, static_cast<std::int32_t>(entry.level) + 1);
				}
			}
		}

		return true;
	}

	static bool TryReadAlias(const rapidjson::Value& json, const char* aliasName, Game::snd_alias_t& alias, Components::ZoneBuilder::Zone* builder)
	{
		auto* const allocator = builder->GetAllocator();

		if (!json.IsObject() || !json.HasMember("type") || !json["type"].IsInt())
		{
			Components::Logger::Error("Each alias of {} must have at least a type and a soundFile\n", aliasName);
			return false;
		}

		const char* soundFileKey = "soundFile";

		if (!json.HasMember(soundFileKey))
		{
			soundFileKey = "soundfile";
		}

		if (!json.HasMember(soundFileKey) || !json[soundFileKey].IsString())
		{
			Components::Logger::Error("Each alias of {} must have at least a type and a soundFile\n", aliasName);
			return false;
		}

		const std::string soundFile = json[soundFileKey].GetString();

		alias.aliasName = aliasName;
		alias.subtitle = DuplicateIfString(json, "subtitle", allocator);
		alias.secondaryAliasName = DuplicateIfString(json, "secondaryAliasName", allocator);
		alias.chainAliasName = DuplicateIfString(json, "chainAliasName", allocator);

		const bool areNumbersRead = TryReadNumber(json, "sequence", alias.sequence)
			&& TryReadNumber(json, "volMin", alias.volMin)
			&& TryReadNumber(json, "volMax", alias.volMax)
			&& TryReadNumber(json, "pitchMin", alias.pitchMin)
			&& TryReadNumber(json, "pitchMax", alias.pitchMax)
			&& TryReadNumber(json, "distMin", alias.distMin)
			&& TryReadNumber(json, "distMax", alias.distMax)
			&& TryReadNumber(json, "flags", alias.flags.intValue)
			&& TryReadNumber(json, "slavePercentage", alias.___u15.slavePercentage)
			&& TryReadNumber(json, "probability", alias.probability)
			&& TryReadNumber(json, "lfePercentage", alias.lfePercentage)
			&& TryReadNumber(json, "centerPercentage", alias.centerPercentage)
			&& TryReadNumber(json, "startDelay", alias.startDelay)
			&& TryReadNumber(json, "envelopMin", alias.envelopMin)
			&& TryReadNumber(json, "envelopMax", alias.envelopMax)
			&& TryReadNumber(json, "envelopPercentage", alias.envelopPercentage);

		if (!areNumbersRead)
		{
			Components::Logger::Error("An alias of {} has a number field that is not a number\n", aliasName);
			return false;
		}

		if (json.HasMember("speakerMap") && !json["speakerMap"].IsNull())
		{
			alias.speakerMap = TryReadSpeakerMap(json["speakerMap"], allocator);

			if (!alias.speakerMap)
			{
				Components::Logger::Error("An alias of {} has a malformed speaker map\n", aliasName);
				return false;
			}
		}

		if (json.HasMember("volumeFalloffCurve") && json["volumeFalloffCurve"].IsString())
		{
			std::string curveName = json["volumeFalloffCurve"].GetString();

			if (curveName.empty())
			{
				curveName = "$default";
			}

			alias.volumeFalloffCurve = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_SOUND_CURVE, curveName, builder).sndCurve;

			if (!alias.volumeFalloffCurve)
			{
				Components::Logger::Error("alias.volumeFalloffCurve was unexpectedly null! could not find the sndcurve {} for {}\n", curveName, aliasName);
				return false;
			}
		}

		alias.soundFile = allocator->Allocate<Game::SoundFile>();
		alias.soundFile->exists = true;

		const auto type = json["type"].GetInt();

		if (type == loadedSoundType)
		{
			alias.soundFile->type = loadedSoundType;
			alias.soundFile->u.loadSnd = Components::AssetHandler::FindAssetForZone(Game::ASSET_TYPE_LOADED_SOUND, soundFile, builder).loadSnd;

			if (!alias.soundFile->u.loadSnd)
			{
				Components::Logger::Error("Could not find the loaded sound {} for {}\n", soundFile, aliasName);
				return false;
			}

			return true;
		}

		if (type == streamedSoundType)
		{
			alias.soundFile->type = streamedSoundType;

			std::string directory;
			std::string fileName = soundFile;
			const auto split = soundFile.find_last_of('/');

			if (split != std::string::npos)
			{
				directory = soundFile.substr(0, split);
				fileName = soundFile.substr(split + 1);
			}

			alias.soundFile->u.streamSnd.filename.info.raw.dir = allocator->DuplicateString(directory);
			alias.soundFile->u.streamSnd.filename.info.raw.name = allocator->DuplicateString(fileName);
			return true;
		}

		Components::Logger::Error("Failed to parse sound {}! Invalid sound type {}\n", aliasName, type);
		return false;
	}

	static Game::snd_alias_list_t* TryReadAliasList(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetAliasPath(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		rapidjson::Document document;
		document.Parse(file.GetBuffer().data(), file.GetBuffer().size());

		if (document.HasParseError() || !document.IsObject() || !document.HasMember("head") || !document["head"].IsArray())
		{
			Components::Logger::Error("Json Parse Error: sounds/{}.json\n", name);
			return nullptr;
		}

		const auto& head = document["head"];
		auto* const allocator = builder->GetAllocator();
		auto* const aliasList = allocator->Allocate<Game::snd_alias_list_t>();

		aliasList->aliasName = allocator->DuplicateString(name);
		aliasList->count = head.Size();
		aliasList->head = allocator->AllocateArray<Game::snd_alias_t>(aliasList->count);

		for (rapidjson::SizeType i = 0; i < head.Size(); ++i)
		{
			if (!TryReadAlias(head[i], aliasList->aliasName, aliasList->head[i], builder))
			{
				Components::Logger::Error("Failed to load sound {}!\n", name);
				return nullptr;
			}
		}

		return aliasList;
	}

	void Isnd_alias_list_t::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->sound = TryReadAliasList(name, builder);

		if (header->sound)
		{
			return;
		}

		header->sound = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).sound;
	}

	void Isnd_alias_list_t::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.sound;

		for (unsigned int i = 0; i < asset->count; ++i)
		{
			const auto* const alias = &asset->head[i];

			if (alias->soundFile && alias->soundFile->type == loadedSoundType)
			{
				builder->LoadAsset(Game::ASSET_TYPE_LOADED_SOUND, alias->soundFile->u.loadSnd);
			}

			if (alias->volumeFalloffCurve)
			{
				if (!builder->LoadAsset(Game::ASSET_TYPE_SOUND_CURVE, alias->volumeFalloffCurve))
				{
					alias->volumeFalloffCurve->filename = "$default";
					builder->LoadAsset(Game::ASSET_TYPE_SOUND_CURVE, alias->volumeFalloffCurve);
				}
			}
		}
	}

	void Isnd_alias_list_t::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.sound;
		auto* const dest = buffer->Dest<Game::X86::snd_alias_list_t>();
		const auto converted = Game::X86::Convert(*asset);
		buffer->Save(&converted);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->aliasName)
		{
			if (builder->HasPointer(asset->aliasName))
			{
				dest->aliasName = builder->GetPointer(asset->aliasName);
			}
			else
			{
				builder->StorePointer(asset->aliasName);
				buffer->SaveString(asset->aliasName);
				Utils::Stream::ClearPointer(&dest->aliasName);
			}
		}

		if (asset->head)
		{
			if (builder->HasPointer(asset->head))
			{
				dest->head = builder->GetPointer(asset->head);
			}
			else
			{
				buffer->Align(Utils::Stream::ALIGN_4);
				builder->StorePointer(asset->head);

				auto* const destHead = buffer->Dest<Game::X86::snd_alias_t>();

				for (unsigned int i = 0; i < asset->count; ++i)
				{
					const auto convertedAlias = Game::X86::Convert(asset->head[i]);
					buffer->Save(&convertedAlias);
				}

				for (unsigned int i = 0; i < asset->count; ++i)
				{
					auto* const destAlias = &destHead[i];
					const auto* const alias = &asset->head[i];

					if (alias->aliasName)
					{
						if (builder->HasPointer(alias->aliasName))
						{
							destAlias->aliasName = builder->GetPointer(alias->aliasName);
						}
						else
						{
							builder->StorePointer(alias->aliasName);
							buffer->SaveString(alias->aliasName);
							Utils::Stream::ClearPointer(&destAlias->aliasName);
						}
					}

					if (alias->subtitle)
					{
						buffer->SaveString(alias->subtitle);
						Utils::Stream::ClearPointer(&destAlias->subtitle);
					}

					if (alias->secondaryAliasName)
					{
						buffer->SaveString(alias->secondaryAliasName);
						Utils::Stream::ClearPointer(&destAlias->secondaryAliasName);
					}

					if (alias->chainAliasName)
					{
						buffer->SaveString(alias->chainAliasName);
						Utils::Stream::ClearPointer(&destAlias->chainAliasName);
					}

					if (alias->mixerGroup)
					{
						buffer->SaveString(alias->mixerGroup);
						Utils::Stream::ClearPointer(&destAlias->mixerGroup);
					}

					if (alias->soundFile)
					{
						if (builder->HasPointer(alias->soundFile))
						{
							destAlias->soundFile = builder->GetPointer(alias->soundFile);
						}
						else
						{
							buffer->Align(Utils::Stream::ALIGN_4);
							builder->StorePointer(alias->soundFile);

							auto* const destSoundFile = buffer->Dest<Game::X86::SoundFile>();
							Game::X86::SoundFile soundFile{};
							soundFile.type = alias->soundFile->type;
							soundFile.exists = alias->soundFile->exists;
							buffer->Save(&soundFile);

							if (alias->soundFile->type == loadedSoundType)
							{
								destSoundFile->u.loadSnd = builder->SaveSubAsset(Game::ASSET_TYPE_LOADED_SOUND, alias->soundFile->u.loadSnd);
							}
							else
							{
								const auto& raw = alias->soundFile->u.streamSnd.filename.info.raw;

								if (raw.dir)
								{
									buffer->SaveString(raw.dir);
									Utils::Stream::ClearPointer(&destSoundFile->u.streamSnd.filename.info.raw.dir);
								}

								if (raw.name)
								{
									buffer->SaveString(raw.name);
									Utils::Stream::ClearPointer(&destSoundFile->u.streamSnd.filename.info.raw.name);
								}
							}

							Utils::Stream::ClearPointer(&destAlias->soundFile);
						}
					}

					if (alias->volumeFalloffCurve)
					{
						destAlias->volumeFalloffCurve = builder->SaveSubAsset(Game::ASSET_TYPE_SOUND_CURVE, alias->volumeFalloffCurve);
					}

					if (alias->speakerMap)
					{
						if (builder->HasPointer(alias->speakerMap))
						{
							destAlias->speakerMap = builder->GetPointer(alias->speakerMap);
						}
						else
						{
							const auto* const speakerMap = alias->speakerMap;
							Game::X86::SpeakerMap convertedMap{};

							if (!TryConvertSpeakerMap(*speakerMap, convertedMap))
							{
								Components::Logger::Fatal("The speaker map of sound '{}' does not fit 1.2.211's {} speakers of {} levels. Export failed!", asset->aliasName, maxSpeakers, maxLevels);
							}

							buffer->Align(Utils::Stream::ALIGN_4);
							builder->StorePointer(alias->speakerMap);

							auto* const destSpeakerMap = buffer->Dest<Game::X86::SpeakerMap>();
							buffer->Save(&convertedMap);

							if (speakerMap->name)
							{
								buffer->SaveString(speakerMap->name);
								Utils::Stream::ClearPointer(&destSpeakerMap->name);
							}

							Utils::Stream::ClearPointer(&destAlias->speakerMap);
						}
					}
				}

				Utils::Stream::ClearPointer(&dest->head);
			}
		}

		buffer->PopBlock();
	}

	void Isnd_alias_list_t::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.sound;

		if (asset->count == 0 || !asset->aliasName)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();
		rapidjson::Value head(rapidjson::kArrayType);

		for (unsigned int i = 0; i < asset->count; ++i)
		{
			const auto& alias = asset->head[i];
			rapidjson::Value speakerMapJson(rapidjson::kNullType);
			const auto* const speakerMap = alias.speakerMap;
			Game::X86::SpeakerMap convertedMap{};

			if (speakerMap && !TryConvertSpeakerMap(*speakerMap, convertedMap))
			{
				Components::Logger::Error("The speaker map of sound {} does not fit {} speakers of {} levels, it is not dumped\n", asset->aliasName, maxSpeakers, maxLevels);
			}
			else if (speakerMap)
			{
				rapidjson::Value channelMaps(rapidjson::kArrayType);

				for (const auto& channelMapRow : convertedMap.channelMaps)
				{
					for (const auto& channelMap : channelMapRow)
					{
						rapidjson::Value speakers(rapidjson::kArrayType);

						for (std::uint32_t speakerIndex = 0; speakerIndex < channelMap.speakerCount; ++speakerIndex)
						{
							const auto& speaker = channelMap.speakers[speakerIndex];
							rapidjson::Value speakerJson(rapidjson::kObjectType);
							speakerJson.AddMember("levels0", speaker.levels[0], allocator);
							speakerJson.AddMember("levels1", speaker.levels[1], allocator);
							speakerJson.AddMember("numLevels", speaker.numLevels, allocator);
							speakerJson.AddMember("speaker", speaker.speaker, allocator);
							speakers.PushBack(speakerJson, allocator);
						}

						rapidjson::Value channelMapJson(rapidjson::kObjectType);
						channelMapJson.AddMember("entryCount", channelMap.speakerCount, allocator);
						channelMapJson.AddMember("speakers", speakers, allocator);
						channelMaps.PushBack(channelMapJson, allocator);
					}
				}

				speakerMapJson.SetObject();
				speakerMapJson.AddMember("channelMaps", channelMaps, allocator);
				speakerMapJson.AddMember("isDefault", speakerMap->isDefault, allocator);
				speakerMapJson.AddMember("name", StringOrNull(speakerMap->name, allocator), allocator);
			}

			std::string fileName;
			int type = 0;

			if (alias.soundFile)
			{
				type = alias.soundFile->type;

				if (alias.soundFile->type == loadedSoundType && alias.soundFile->u.loadSnd)
				{
					fileName = alias.soundFile->u.loadSnd->name;
					Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_LOADED_SOUND, alias.soundFile->u.loadSnd });
				}

				if (alias.soundFile->type == streamedSoundType && alias.soundFile->u.streamSnd.filename.info.raw.name)
				{
					const auto& raw = alias.soundFile->u.streamSnd.filename.info.raw;
					fileName = raw.name;

					if (raw.dir && *raw.dir)
					{
						fileName = std::format("{}/{}", raw.dir, raw.name);
					}

					Components::FileSystem::File streamed(std::format("sound/{}", fileName));

					if (streamed.Exists())
					{
						Utils::IO::WriteFile(std::format("{}/sound/{}", Components::ZoneBuilder::GetDumpingZonePath(), fileName), streamed.GetBuffer());
					}
				}
			}

			const char* curveName = nullptr;

			if (alias.volumeFalloffCurve)
			{
				curveName = alias.volumeFalloffCurve->filename;
				Components::AssetHandler::DumpAsset({ Game::ASSET_TYPE_SOUND_CURVE, alias.volumeFalloffCurve });
			}

			rapidjson::Value aliasJson(rapidjson::kObjectType);
			aliasJson.AddMember("aliasName", StringOrNull(alias.aliasName, allocator), allocator);
			aliasJson.AddMember("centerPercentage", alias.centerPercentage, allocator);
			aliasJson.AddMember("chainAliasName", StringOrNull(alias.chainAliasName, allocator), allocator);
			aliasJson.AddMember("distMax", alias.distMax, allocator);
			aliasJson.AddMember("distMin", alias.distMin, allocator);
			aliasJson.AddMember("envelopMax", alias.envelopMax, allocator);
			aliasJson.AddMember("envelopMin", alias.envelopMin, allocator);
			aliasJson.AddMember("envelopPercentage", alias.envelopPercentage, allocator);
			aliasJson.AddMember("flags", alias.flags.intValue, allocator);
			aliasJson.AddMember("lfePercentage", alias.lfePercentage, allocator);
			aliasJson.AddMember("mixerGroup", rapidjson::Value(rapidjson::kNullType), allocator);
			aliasJson.AddMember("pitchMax", alias.pitchMax, allocator);
			aliasJson.AddMember("pitchMin", alias.pitchMin, allocator);
			aliasJson.AddMember("probability", alias.probability, allocator);
			aliasJson.AddMember("secondaryAliasName", StringOrNull(alias.secondaryAliasName, allocator), allocator);
			aliasJson.AddMember("sequence", alias.sequence, allocator);
			aliasJson.AddMember("slavePercentage", alias.___u15.slavePercentage, allocator);
			aliasJson.AddMember("speakerMap", speakerMapJson, allocator);
			aliasJson.AddMember("soundFile", rapidjson::Value(fileName.data(), allocator), allocator);
			aliasJson.AddMember("startDelay", alias.startDelay, allocator);
			aliasJson.AddMember("subtitle", StringOrNull(alias.subtitle, allocator), allocator);
			aliasJson.AddMember("type", type, allocator);
			aliasJson.AddMember("volMax", alias.volMax, allocator);
			aliasJson.AddMember("volMin", alias.volMin, allocator);
			aliasJson.AddMember("volumeFalloffCurve", StringOrNull(curveName, allocator), allocator);

			head.PushBack(aliasJson, allocator);
		}

		output.AddMember("aliasName", rapidjson::Value(asset->aliasName, allocator), allocator);
		output.AddMember("count", asset->count, allocator);
		output.AddMember("head", head, allocator);

		rapidjson::StringBuffer text;
		rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(text);
		writer.SetFormatOptions(rapidjson::PrettyFormatOptions::kFormatSingleLineArray);
		output.Accept(writer);

		Utils::IO::WriteFile(std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetAliasPath(asset->aliasName)), text.GetString());
	}

	Isnd_alias_list_t::Isnd_alias_list_t()
	{
		Components::Command::Add("dumpSound", [this](const Components::Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const char* name = params->Get(1);

			if (!Game::DB_FindXAssetEntry(this->GetType(), name))
			{
				return;
			}

			const Game::XAssetHeader header{ Game::DB_FindXAssetHeader(this->GetType(), name) };

			if (header.data)
			{
				this->Dump(header);
			}
		});
	}
}
