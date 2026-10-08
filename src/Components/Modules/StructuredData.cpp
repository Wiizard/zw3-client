#include "STDInclude.hpp"

#include "StructuredData.hpp"
#include "AssetHandler.hpp"
#include "Logger.hpp"

namespace Components
{
	Utils::Hook StructuredData::updateVersionHook;
	Utils::Memory::Allocator StructuredData::memAllocator;

	const char* StructuredData::enumTranslation[COUNT] =
	{
		"features",
		"weapons",
		"attachements",
		"challenges",
		"camos",
		"perks",
		"killstreaks",
		"accolades",
		"cardicons",
		"cardtitles",
		"cardnameplates",
		"teams",
		"gametypes"
	};

	constexpr int basePlayerStatsVersion = 155;

	constexpr unsigned int assetTypeStructuredDataDef = 0x27;

	constexpr std::uintptr_t LiveStorage_FinalizeStatsRead_UpdateVersionCall = 0x1401F7944;

	static const std::uint8_t updateVersionCall[] = { 0xE8, 0x97, 0xB8, 0x08, 0x00 };

	constexpr std::uintptr_t LiveStorage_StatsInit_ClassCount = 0x1401FA6D8;

	static const std::uint8_t classCountTest[] = { 0x83, 0xFE, 0x0A, 0x0F, 0x8C };

	constexpr unsigned int customClassesEnd = 3643;
	constexpr unsigned int customClassesEndAtFifteen = 3963;

	struct PlayerDataPatch
	{
		std::unordered_map<std::string, std::vector<std::string>> enums;
		std::unordered_map<std::string, std::string> other;
		unsigned int formatChecksum = 0;
	};

	static const std::map<int, PlayerDataPatch> playerDataPatches =
	{
		{
			156,
			{
				{
					{
						"weapons",
						{
							"m40a3", "ak47classic",
						},
					},
					{
						"cardicons",
						{
							"cardicon_rtrolling",
						},
					},
					{
						"cardtitles",
						{
							"cardtitle_evilchicken", "cardtitle_nolaststand",
						},
					},
				},
				{},
			},
		},
		{
			157,
			{
				{
					{
						"weapons",
						{
							"ak74u", "peacekeeper",
						},
					},
				},
				{},
			},
		},
		{
			158,
			{
				{
					{
						"weapons",
						{
							"dragunov", "onemanarmy",
						},
					},
				},
				{},
			},
		},
		{
			159,
			{
				{},
				{
					{ "classes", "15" },
				},
			},
		},
		{
			160,
			{
				{
					{
						"weapons",
						{
							"iw3_skorpion", "iw3_g36c", "iw3_mp44", "iw3_mp5", "iw3_m21", "iw3_m40a3", "iw3_ak47",
							"iw3_ak74u", "iw3_winchester1200", "iw3_remington700", "iw3_m14", "iw3_dragunov",
							"iw3_barrett", "iw3_saw", "iw3_m4", "iw3_m60e4", "iw3_m16", "iw3_g3", "iw3_colt45",
						},
					},
					{
						"cardicons",
						{
							"cardicon_burn", "cardicon_burningrunner", "cardicon_capsule", "cardicon_coffee",
							"cardicon_commando", "cardicon_cooking", "cardicon_devil", "cardicon_flashbang",
							"cardicon_ghostoon", "cardicon_grinch", "cardicon_gunstar", "cardicon_horseshoe",
							"cardicon_hound", "cardicon_icecream", "cardicon_kitty", "cardicon_mushroom",
							"cardicon_nunchucks", "cardicon_rampage", "cardicon_rank_supcomm", "cardicon_reaped",
							"cardicon_smilebomb", "cardicon_spade", "cardicon_toxic", "cardicon_unicorn",
							"cardicon_xrayhand", "cardicon_sniper", "cardicon_chicken_buff_icon", "cardicon_dive",
							"cardicon_eaglegraffiti", "cardicon_ghost2018", "cardicon_ghostdog",
							"cardicon_ghostdoge", "cardicon_graffitibear", "cardicon_graffitygorilla",
							"cardicon_headicon_micro_boss", "cardicon_helicopter", "cardicon_hud_star69icon",
							"cardicon_iw5_bearninja", "cardicon_doomguyface", "cardicon_iw5_cards",
							"cardicon_iw5_cat", "cardicon_iw5_cooking", "cardicon_iw5_death_bell",
							"cardicon_iw5_elite_01", "cardicon_iw5_elite_03", "cardicon_iw5_elite_13",
							"cardicon_iw5_elite_14", "cardicon_iw5_elite_15", "cardicon_iw5_flashbang",
							"cardicon_iw5_frank", "cardicon_iw5_gunstar", "cardicon_iw5_helmet",
							"cardicon_iw5_horseshoe", "cardicon_iw5_medkit", "cardicon_iw5_skullguns",
						},
					},
					{
						"cardtitles",
						{
							"cardtitle_abstract3_a", "cardtitle_abstract3_b", "cardtitle_glass_hispeed_a",
							"cardtitle_glass_hispeed_b", "cardtitle_glass_hispeed_c", "cardtitle_joint_a",
							"cardtitle_joint_b", "cardtitle_flames_1_a", "cardtitle_flames_1_b",
							"cardtitle_jason_nvg_a", "cardtitle_jason_nvg_b", "cardtitle_horsemen_death",
							"cardtitle_horsemen_famine", "cardtitle_horsemen_war", "cardtitle_pinkscar_a",
							"cardtitle_pinkscar_b", "cardtitle_pinkscar_c", "cardtitle_pinkscar_d",
							"cardtitle_wolf_a", "cardtitle_wolf_b", "cardtitle_sunbather_a",
							"cardtitle_sunbather_b", "cardtitle_sunbather_c", "cardtitle_sword_2",
							"cardtitle_sword_3", "cardtitle_graff_a", "cardtitle_graff_b",
							"cardtitle_graffiti_01_a", "cardtitle_graffiti_01_b", "cardtitle_graffiti_01_c",
							"cardtitle_bills_a", "cardtitle_bills_b", "cardtitle_bills_c", "cardtitle_eyes_a",
							"cardtitle_eyes_b", "cardtitle_eyes_c", "cardtitle_machinegunner_a",
							"cardtitle_machinegunner_b", "cardtitle_machinegunner_c", "cardtitle_gungirl_a",
							"cardtitle_gungirl_b", "cardtitle_burgertown_a", "cardtitle_burgertown_b",
							"cardtitle_burgertown_c", "cardtitle_burgertown_d", "cardtitle_naval_a",
							"cardtitle_naval_b", "cardtitle_blackhawk_a", "cardtitle_blackhawk_b",
							"cardtitle_blackhawk_c", "cardtitle_feathers_a", "cardtitle_feathers_b",
							"cardtitle_migs_a", "cardtitle_migs_b", "cardtitle_minigun_a", "cardtitle_minigun_b",
							"cardtitle_minigun_c", "cardtitle_minigun_d", "cardtitle_rainbows_a",
							"cardtitle_rainbows_bb", "cardtitle_redrocket_a", "cardtitle_redrocket_b",
							"cardtitle_redrocket_c", "cardtitle_tbaga", "cardtitle_tbag_b", "cardtitle_uav_a",
							"cardtitle_uav_b", "cardtitle_bandaidnew_a", "cardtitle_bandaidnew_b",
							"cardtitle_bandaidnew_c", "cardtitle_bloodcells_a", "cardtitle_bloodcells_b",
							"cardtitle_diver_a", "cardtitle_diver_b", "cardtitle_girl_a", "cardtitle_girl_b",
							"cardtitle_huntknife_a", "cardtitle_huntknife_b", "cardtitle_huntknife_c",
							"cardtitle_pistols_a", "cardtitle_pistols_b", "cardtitle_smoke_grenade_a",
							"cardtitle_smoke_grenade_b", "cardtitle_smoke_grenade_c", "cardtitle_specops_a",
							"cardtitle_specops_b", "cardtitle_specops_c", "cardtitle_boombox_a",
							"cardtitle_boombox_b", "cardtitle_boombox_c", "cardtitle_bloodlake_a",
							"cardtitle_bloodlake_b", "cardtitle_bloodlake_c", "cardtitle_skullking_a",
							"cardtitle_skullking_b", "cardtitle_skullking_c", "cardtitle_comic_a",
							"cardtitle_comic_b", "cardtitle_comic_c", "cardtitle_assault_silence_a",
							"cardtitle_assault_silence_b", "cardtitle_assault_silence_c", "cardtitle_delta_a",
							"cardtitle_delta_b", "cardtitle_lasers_a", "cardtitle_lasers_b", "cardtitle_makarov",
							"cardtitle_mw_bros", "cardtitle_toonprice_a", "cardtitle_toonprice_b",
							"cardtitle_flag_pride_a", "cardtitle_flag_transgender_a", "cardtitle_flag_asexual_a",
							"cardtitle_flag_nonbinary_a", "cardtitle_flag_bisexual_a", "cardtitle_flag_pride_b",
							"cardtitle_flag_transgender_b", "cardtitle_flag_asexual_b",
							"cardtitle_flag_nonbinary_b", "cardtitle_flag_bisexual_b",
						},
					},
				},
				{},
			},
		},
		{
			161,
			{
				{
					{
						"cardicons",
						{
							"cardicon_red_devil",
						},
					},
				},
				{},
				0x9085448B,
			},
		},
	};

	bool StructuredData::UpdateVersionOffsets(Game::StructuredDataDefSet* set, Game::StructuredDataBuffer* buffer, Game::StructuredDataDef* oldDef)
	{
		const int bufferVersion = *reinterpret_cast<int*>(buffer->data);

		for (unsigned int i = 1; i < set->defCount; ++i)
		{
			const Game::StructuredDataDef* const newer = &set->defs[i - 1];
			const Game::StructuredDataDef* const older = &set->defs[i];

			if (older->version == bufferVersion && newer->version == 159 && older->version <= 158)
			{
				std::memmove(&buffer->data[customClassesEndAtFifteen], &buffer->data[customClassesEnd], older->size - customClassesEnd);
				break;
			}
		}

		return reinterpret_cast<bool(*)(Game::StructuredDataDefSet*, Game::StructuredDataBuffer*, Game::StructuredDataDef*)>(
			updateVersionHook.GetOriginal())(set, buffer, oldDef);
	}

	void StructuredData::PatchPlayerDataEnum(Game::StructuredDataDef* data, PlayerDataType type, const std::vector<std::string>& entries)
	{
		auto* const newEnums = memAllocator.AllocateArray<Game::StructuredDataEnum>(data->enumCount);
		std::memcpy(newEnums, data->enums, sizeof(Game::StructuredDataEnum) * data->enumCount);
		data->enums = newEnums;

		Game::StructuredDataEnum* const dataEnum = &data->enums[type];

		std::vector<const char*> dataVector;

		for (int i = 0; i < dataEnum->entryCount; ++i)
		{
			int index = 0;

			for (; index < dataEnum->entryCount; ++index)
			{
				if (dataEnum->entries[index].index == i)
				{
					break;
				}
			}

			dataVector.push_back(dataEnum->entries[index].string);
		}

		for (const std::string& entry : entries)
		{
			const char* value = nullptr;

			for (auto i = dataVector.begin(); i != dataVector.end(); ++i)
			{
				if (*i == entry)
				{
					value = *i;
					dataVector.erase(i);
					break;
				}
			}

			if (!value)
			{
				value = memAllocator.DuplicateString(entry);
			}

			dataVector.push_back(value);
		}

		auto* const indices = memAllocator.AllocateArray<Game::StructuredDataEnumEntry>(dataVector.size());

		for (unsigned short i = 0; i < dataVector.size(); ++i)
		{
			indices[i].index = i;
			indices[i].string = dataVector[i];
		}

		std::sort(indices, indices + dataVector.size(), [](const Game::StructuredDataEnumEntry& first, const Game::StructuredDataEnumEntry& second)
		{
			return std::strcmp(first.string, second.string) < 0;
		});

		dataEnum->entryCount = static_cast<int>(dataVector.size());
		dataEnum->entries = indices;
	}

	void StructuredData::PatchCustomClassLimit(Game::StructuredDataDef* data, int count)
	{
		constexpr int classArray = 5;
		constexpr int customClassSize = 64;

		const int originalClassCount = data->indexedArrays[classArray].arraySize;

		if (count == originalClassCount)
		{
			return;
		}

		const int growth = (count - originalClassCount) * customClassSize;

		auto* const newStructs = memAllocator.AllocateArray<Game::StructuredDataStruct>(data->structCount);
		std::memcpy(newStructs, data->structs, data->structCount * sizeof(Game::StructuredDataStruct));
		data->structs = newStructs;

		Game::StructuredDataStruct* const root = &data->structs[0];

		auto* const newProperties = memAllocator.AllocateArray<Game::StructuredDataStructProperty>(root->propertyCount);
		std::memcpy(newProperties, root->properties, root->propertyCount * sizeof(Game::StructuredDataStructProperty));
		root->properties = newProperties;

		for (int i = 0; i < root->propertyCount; ++i)
		{
			if (root->properties[i].offset >= customClassesEnd)
			{
				root->properties[i].offset += growth;
			}
		}

		data->size += growth;

		auto* const newIndexedArrays = memAllocator.AllocateArray<Game::StructuredDataIndexedArray>(data->indexedArrayCount);
		std::memcpy(newIndexedArrays, data->indexedArrays, data->indexedArrayCount * sizeof(Game::StructuredDataIndexedArray));
		data->indexedArrays = newIndexedArrays;

		data->indexedArrays[classArray].arraySize = count;
	}

	void StructuredData::PatchAdditionalData(Game::StructuredDataDef* data, const std::unordered_map<std::string, std::string>& patches)
	{
		for (const auto& [key, value] : patches)
		{
			if (key == "classes")
			{
				PatchCustomClassLimit(data, std::atoi(value.data()));
			}
		}
	}

	StructuredData::StructuredData()
	{
		if (!AssetHandler::IsInstalled())
		{
			Logger::Error("structureddata: asset loads are not watched, mp/playerdata.def keeps its 10 classes\n");
			return;
		}

		if (!Utils::Hook::MatchesBytes(LiveStorage_FinalizeStatsRead_UpdateVersionCall, updateVersionCall, sizeof(updateVersionCall))
			|| !Utils::Hook::MatchesBytes(LiveStorage_StatsInit_ClassCount, classCountTest, sizeof(classCountTest)))
		{
			Logger::Error("structureddata: the stats code does not read as expected, mp/playerdata.def keeps its 10 classes\n");
			return;
		}

		if (!updateVersionHook.Initialize(LiveStorage_FinalizeStatsRead_UpdateVersionCall, reinterpret_cast<void*>(UpdateVersionOffsets), HOOK_CALL)
			->Install()->IsInstalled())
		{
			Logger::Error("structureddata: could not hook the stats version upgrade, mp/playerdata.def keeps its 10 classes\n");
			return;
		}

		updateVersionHook.Quick();

		AssetHandler::OnLoad([](unsigned int type, void* asset, const std::string& filename, [[maybe_unused]] bool* restrict)
		{
			if (type != assetTypeStructuredDataDef || filename != "mp/playerdata.def")
			{
				return;
			}

			auto* const data = static_cast<Game::StructuredDataDefSet*>(asset);

			if (data->defCount != 1)
			{
				Logger::Error("PlayerDataDefSet contains more than 1 definition!\n");
				return;
			}

			if (data->defs[0].version != basePlayerStatsVersion)
			{
				Logger::Error("Initial PlayerDataDef is not version 155, patching not possible!\n");
				return;
			}

			std::unordered_map<int, std::vector<std::vector<std::string>>> patchDefinitions;
			std::unordered_map<int, std::unordered_map<std::string, std::string>> otherPatchDefinitions;
			std::unordered_map<int, unsigned int> formatChecksums;

			for (int i = 156;; ++i)
			{
				const auto patch = playerDataPatches.find(i);

				if (patch == playerDataPatches.end())
				{
					break;
				}

				std::vector<std::vector<std::string>> enumContainer;

				for (int pType = 0; pType < COUNT; ++pType)
				{
					const auto entries = patch->second.enums.find(enumTranslation[pType]);

					if (entries == patch->second.enums.end())
					{
						enumContainer.emplace_back();
					}
					else
					{
						enumContainer.push_back(entries->second);
					}
				}

				patchDefinitions[i] = enumContainer;
				otherPatchDefinitions[i] = patch->second.other;
				formatChecksums[i] = patch->second.formatChecksum;
			}

			if (patchDefinitions.empty())
			{
				return;
			}

			auto* const newData = memAllocator.AllocateArray<Game::StructuredDataDef>(data->defCount + patchDefinitions.size());
			std::memcpy(&newData[patchDefinitions.size()], data->defs, sizeof(Game::StructuredDataDef) * data->defCount);

			for (unsigned int i = 0; i < patchDefinitions.size(); ++i)
			{
				newData[i].version = static_cast<int>(patchDefinitions.size() - i) + basePlayerStatsVersion;
			}

			data->defs = newData;
			data->defCount += static_cast<unsigned int>(patchDefinitions.size());

			for (int i = static_cast<int>(data->defCount) - 1; i >= 0; --i)
			{
				if (newData[i].version == basePlayerStatsVersion)
				{
					continue;
				}

				const int version = newData[i].version;
				data->defs[i] = data->defs[i + 1];
				newData[i].version = version;

				if (patchDefinitions.contains(version))
				{
					const auto& patchData = patchDefinitions[version];

					for (int pType = 0; pType < COUNT; ++pType)
					{
						if (!patchData[pType].empty())
						{
							PatchPlayerDataEnum(&newData[i], static_cast<PlayerDataType>(pType), patchData[pType]);
						}
					}

					PatchAdditionalData(&newData[i], otherPatchDefinitions[version]);

					if (formatChecksums[version])
					{
						newData[i].formatChecksum = formatChecksums[version];
					}
				}
			}

			Utils::Hook::Set<std::uint8_t>(LiveStorage_StatsInit_ClassCount + 2, NUM_CUSTOM_CLASSES);
		});
	}
}
