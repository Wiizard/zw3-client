#include <Utils/WebIO.hpp>
#include <Utils/CSV.hpp>
#include <wincrypt.h>

#include "ZWNet.hpp"
#include "Auth.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Friends.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"
#include "UIScript.hpp"

namespace Components
{
	namespace
	{
		constexpr auto ZWNET_API_BASE = "https://backend.zw3.eu";
		constexpr auto ZWNET_CLIENT_VERSION = "3.0.3";
		constexpr auto ZWNET_MOD_VERSION = "3.0.3";
		constexpr std::size_t ZWNET_MATERIAL_ENUM_CAPACITY = 16384;

		std::array<Game::XAssetHeader, ZWNET_MATERIAL_ENUM_CAPACITY> MaterialEnumerationAssets{};
		std::uint32_t MaterialEnumerationCount{};
		std::string FormatPublicGuid();

		struct SharedLobbyRank
		{
			int level = 1;
			int prestige = 0;
		};

		struct LocalBarracksRank
		{
			int level = 1;
			int prestige = 0;
			int experience = 0;
			int experienceTarget = 50;
		};

		struct LocalChallengeProgress
		{
			int progress = 0;
			int tier = 0;
		};

		struct LocalBarracksChallenges
		{
			std::unordered_map<std::string, LocalChallengeProgress> entries;
			int zombieKills = 0;
			int zombieDeaths = 0;
			int zombieRevives = 0;
		};

		struct ChallengeDefinition
		{
			std::string id;
			std::array<int, 4> targets{};
			std::array<int, 4> rewards{};
		};

		constexpr std::array ChallengeSlotProgressDvars
		{
			"zw3_ch_slot_0_progress", "zw3_ch_slot_1_progress", "zw3_ch_slot_2_progress",
			"zw3_ch_slot_3_progress", "zw3_ch_slot_4_progress"
		};
		constexpr std::array ChallengeSlotTargetDvars
		{
			"zw3_ch_slot_0_target", "zw3_ch_slot_1_target", "zw3_ch_slot_2_target",
			"zw3_ch_slot_3_target", "zw3_ch_slot_4_target"
		};
		constexpr std::array ChallengeSlotTierDvars
		{
			"zw3_ch_slot_0_tier", "zw3_ch_slot_1_tier", "zw3_ch_slot_2_tier",
			"zw3_ch_slot_3_tier", "zw3_ch_slot_4_tier"
		};
		constexpr std::array ChallengeSlotTierCountDvars
		{
			"zw3_ch_slot_0_tier_count", "zw3_ch_slot_1_tier_count", "zw3_ch_slot_2_tier_count",
			"zw3_ch_slot_3_tier_count", "zw3_ch_slot_4_tier_count"
		};
		constexpr std::array ChallengeSlotRewardDvars
		{
			"zw3_ch_slot_0_reward", "zw3_ch_slot_1_reward", "zw3_ch_slot_2_reward",
			"zw3_ch_slot_3_reward", "zw3_ch_slot_4_reward"
		};
		constexpr std::array ChallengeSlotPercentDvars
		{
			"zw3_ch_slot_0_percent", "zw3_ch_slot_1_percent", "zw3_ch_slot_2_percent",
			"zw3_ch_slot_3_percent", "zw3_ch_slot_4_percent"
		};
		constexpr std::array ChallengeSlotCompleteDvars
		{
			"zw3_ch_slot_0_complete", "zw3_ch_slot_1_complete", "zw3_ch_slot_2_complete",
			"zw3_ch_slot_3_complete", "zw3_ch_slot_4_complete"
		};

		using SharedLobbyRankMap =
			std::unordered_map<std::string, SharedLobbyRank>;

		std::mutex SharedLobbyRankMutex;
		SharedLobbyRankMap SharedLobbyRanks;
		std::string SharedLobbyRankPartyId;
		std::chrono::steady_clock::time_point NextRankPublishAttempt{};
		std::string LastRankPublishPartyId;
		int LastRankPublishLevel = -1;
		int LastRankPublishPrestige = -1;

		std::atomic_int& DesiredPartyPrivacy()
		{
			static std::atomic_int value{0};
			return value;
		}

		std::atomic_bool& LocalPartyLeader()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_bool& VisibilitySyncPending()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_bool& EndpointJoinInFlight()
		{
			static std::atomic_bool value{false};
			return value;
		}

		std::atomic_bool& TerminalDisconnectRequested()
		{
			static std::atomic_bool value{false};
			return value;
		}

		const char* PartyVisibilityName(const int privacy)
		{
			switch (privacy)
			{
			case 1: return "INVITE_ONLY";
			case 2: return "CLOSED";
			default: return "OPEN";
			}
		}

		std::string NormalizePartyVisibility(std::string visibility)
		{
			std::ranges::transform(visibility, visibility.begin(), [](const unsigned char character)
			{
				return static_cast<char>(std::toupper(character));
			});
			if (visibility == "INVITE" || visibility == "FRIENDS") return "INVITE_ONLY";
			if (visibility != "OPEN" && visibility != "INVITE_ONLY" && visibility != "CLOSED") return "OPEN";
			return visibility;
		}

		std::atomic_int& CachedPartyMemberCount()
		{
			static std::atomic_int value{0};
			return value;
		}

		std::atomic_int& CachedPartyVisibility()
		{
			static std::atomic_int value{2};
			return value;
		}

		std::atomic_bool& CachedPartyJoinStateSupported()
		{
			static std::atomic_bool value{false};
			return value;
		}

		int PartyVisibilityValue(const std::string& visibility)
		{
			if (visibility == "CLOSED") return 2;
			if (visibility == "INVITE_ONLY") return 1;
			return 0;
		}

		bool IsPartyJoinStateSupported(const std::string& state)
		{
			return state == "IDLE" || state == "IN_PARTY" ||
				state == "SEARCHING" || state == "MATCH_FOUND" ||
				state == "MAP_VOTE" || state == "READY_CHECK" ||
				state == "WAITING_FOR_READY" || state == "RESERVING_SERVER" ||
				state == "STARTING_SERVER" || state == "SERVER_STARTING" ||
				state == "CONNECTING" || state == "IN_MATCH";
		}

		bool IsOpaquePartyId(const std::string& value)
		{
			return value.starts_with("pty_") && value.size() <= 80 &&
				std::ranges::all_of(value, [](const unsigned char character)
				{
					return std::isalnum(character) != 0 || character == '-' || character == '_';
				});
		}

		bool IsOpaqueJoinCapability(const std::string& value)
		{
			return value.size() >= 16 && value.size() <= 256 &&
				std::ranges::all_of(value, [](const unsigned char character)
				{
					return std::isalnum(character) != 0 || character == '-' || character == '_';
				});
		}

		struct MatchLobbySoundDelta
		{
			bool joined{};
			bool left{};
		};

		std::mutex MatchLobbySoundMutex;
		std::string MatchLobbySoundMatchId;
		std::unordered_set<std::string> MatchLobbySoundMembers;
		bool MatchLobbySoundInitialized{};

		void ResetMatchLobbySoundSnapshot()
		{
			std::lock_guard lock(MatchLobbySoundMutex);
			MatchLobbySoundMatchId.clear();
			MatchLobbySoundMembers.clear();
			MatchLobbySoundInitialized = false;
		}

		MatchLobbySoundDelta ObserveMatchLobbyMembers(const std::string& matchId,
			const std::unordered_set<std::string>& members)
		{
			std::lock_guard lock(MatchLobbySoundMutex);
			if (matchId.empty())
			{
				MatchLobbySoundMatchId.clear();
				MatchLobbySoundMembers.clear();
				MatchLobbySoundInitialized = false;
				return {};
			}

			// Entering a lobby or switching matches establishes a silent baseline.
			if (!MatchLobbySoundInitialized || MatchLobbySoundMatchId != matchId)
			{
				MatchLobbySoundMatchId = matchId;
				MatchLobbySoundMembers = members;
				MatchLobbySoundInitialized = true;
				return {};
			}

			MatchLobbySoundDelta delta;
			for (const auto& member : members)
			{
				if (!MatchLobbySoundMembers.contains(member)) delta.joined = true;
			}
			for (const auto& member : MatchLobbySoundMembers)
			{
				if (!members.contains(member)) delta.left = true;
			}
			MatchLobbySoundMembers = members;
			return delta;
		}

		bool TryParseRankValue(const std::string& data,
			const std::string_view field, int& value)
		{
			const auto fieldPosition = data.find(field);
			if (fieldPosition == std::string::npos)
			{
				return false;
			}

			auto valuePosition = fieldPosition + field.size();
			while (valuePosition < data.size() &&
				(data[valuePosition] == ' ' || data[valuePosition] == '\t'))
			{
				++valuePosition;
			}
			if (valuePosition >= data.size() || data[valuePosition] != ':')
			{
				return false;
			}
			++valuePosition;
			while (valuePosition < data.size() &&
				(data[valuePosition] == ' ' || data[valuePosition] == '\t'))
			{
				++valuePosition;
			}

			if (valuePosition >= data.size())
			{
				return false;
			}

			char* end = nullptr;
			const auto parsed = std::strtol(
				data.c_str() + valuePosition, &end, 10);
			if (end == data.c_str() + valuePosition)
			{
				return false;
			}
			while (end < data.c_str() + data.size() &&
				(*end == ' ' || *end == '\t'))
			{
				++end;
			}
			if (end < data.c_str() + data.size() && *end != ';')
			{
				return false;
			}
			if (parsed < std::numeric_limits<int>::min() ||
				parsed > std::numeric_limits<int>::max())
			{
				return false;
			}

			value = static_cast<int>(parsed);
			return true;
		}

		std::optional<SharedLobbyRank> ReadLocalLobbyRank()
		{
			const auto rankPath = std::filesystem::path("zw3") /
				"core" / "scriptdata" /
				("rank_" + FormatPublicGuid());
			std::string data;
			if (!Utils::IO::ReadFile(rankPath.string(), &data))
			{
				return std::nullopt;
			}

			int storedLevel = 0;
			int storedPrestige = 0;
			if (!TryParseRankValue(data, "level", storedLevel) ||
				!TryParseRankValue(data, "prestige", storedPrestige) ||
				storedLevel < 0 || storedLevel > 53 ||
				storedPrestige < 0 || storedPrestige > 20)
			{
				return std::nullopt;
			}

			return SharedLobbyRank{storedLevel + 1, storedPrestige};
		}

		std::optional<LocalBarracksRank> ReadLocalBarracksRank()
		{
			const auto rankPath = std::filesystem::path("zw3") /
				"core" / "scriptdata" /
				("rank_" + FormatPublicGuid());
			std::string data;
			if (!Utils::IO::ReadFile(rankPath.string(), &data))
			{
				return std::nullopt;
			}

			int storedLevel = 0;
			int storedPrestige = 0;
			int storedExperience = 0;
			if (!TryParseRankValue(data, "level", storedLevel) ||
				!TryParseRankValue(data, "prestige", storedPrestige) ||
				!TryParseRankValue(data, "experience", storedExperience) ||
				storedLevel < 0 || storedLevel > 53 ||
				storedPrestige < 0 || storedPrestige > 255 ||
				storedExperience < 0)
			{
				return std::nullopt;
			}

			int experienceTarget = 50;
			switch (storedLevel)
			{
			case 0: experienceTarget = 50; break;
			case 1: experienceTarget = 125; break;
			case 2: experienceTarget = 200; break;
			case 3: experienceTarget = 300; break;
			case 4: experienceTarget = 450; break;
			case 5: experienceTarget = 650; break;
			default: experienceTarget = 650 + ((storedLevel - 5) * 250); break;
			}

			return LocalBarracksRank{storedLevel + 1, storedPrestige, storedExperience, experienceTarget};
		}

		std::string ZombiePrestigeIcon(const int prestige)
		{
			const auto iconLevel = prestige + 1;
			return iconLevel > 8 ? "skullicon" : std::format("prestige_{}", iconLevel);
		}

		bool TryParseChallengeEntry(const std::string& data,
			const std::string_view id, LocalChallengeProgress& entry)
		{
			const auto needle = std::string(id) + ":";
			auto position = data.find(needle);
			while (position != std::string::npos && position > 0 && data[position - 1] != ';')
			{
				position = data.find(needle, position + 1);
			}
			if (position == std::string::npos)
			{
				return false;
			}

			const auto* progressStart = data.c_str() + position + needle.size();
			char* progressEnd = nullptr;
			const auto progress = std::strtol(progressStart, &progressEnd, 10);
			if (progressEnd == progressStart || *progressEnd != ':')
			{
				return false;
			}

			const auto* tierStart = progressEnd + 1;
			char* tierEnd = nullptr;
			const auto tier = std::strtol(tierStart, &tierEnd, 10);
			if (tierEnd == tierStart || *tierEnd != ';' ||
				progress < 0 || progress > std::numeric_limits<int>::max() ||
				tier < 0 || tier > std::numeric_limits<int>::max())
			{
				return false;
			}

			entry.progress = static_cast<int>(progress);
			entry.tier = static_cast<int>(tier);
			return true;
		}

		std::vector<ChallengeDefinition> ReadChallengeDefinitions()
		{
			Utils::CSV table("zw3/core/mp/zw3_challenge_ui.csv", true, false);
			std::vector<ChallengeDefinition> definitions;
			if (!table.isValid())
			{
				return definitions;
			}

			definitions.reserve(table.getRows());
			for (std::size_t row = 0; row < table.getRows(); ++row)
			{
				ChallengeDefinition definition;
				definition.id = table.getElementAt(row, 0);
				if (definition.id.empty())
				{
					continue;
				}

				for (std::size_t tier = 0; tier < definition.targets.size(); ++tier)
				{
					const auto targetText = table.getElementAt(row, 4 + tier * 2);
					const auto rewardText = table.getElementAt(row, 5 + tier * 2);
					if (targetText.empty())
					{
						break;
					}

					char* targetEnd = nullptr;
					char* rewardEnd = nullptr;
					const auto target = std::strtol(targetText.c_str(), &targetEnd, 10);
					const auto reward = std::strtol(rewardText.c_str(), &rewardEnd, 10);
					if (targetEnd == targetText.c_str() || *targetEnd != '\0' || target <= 0 ||
						target > std::numeric_limits<int>::max() ||
						rewardEnd == rewardText.c_str() || *rewardEnd != '\0' || reward < 0 ||
						reward > std::numeric_limits<int>::max())
					{
						break;
					}

					definition.targets[tier] = static_cast<int>(target);
					definition.rewards[tier] = static_cast<int>(reward);
				}

				if (definition.targets[0] > 0)
				{
					definitions.emplace_back(std::move(definition));
				}
			}

			return definitions;
		}

		LocalBarracksChallenges ReadLocalBarracksChallenges(
			const std::vector<ChallengeDefinition>& definitions)
		{
			LocalBarracksChallenges challenges;
			const auto challengePath = std::filesystem::path("zw3") /
				"core" / "scriptdata" /
				("challenges_" + FormatPublicGuid());
			std::string data;
			if (!Utils::IO::ReadFile(challengePath.string(), &data))
			{
				return challenges;
			}

			for (const auto& definition : definitions)
			{
				LocalChallengeProgress entry;
				if (TryParseChallengeEntry(data, definition.id, entry))
				{
					challenges.entries.emplace(definition.id, entry);
				}
			}

			LocalChallengeProgress value;
			const auto hasKills = TryParseChallengeEntry(data, "stat_zw3_zombie_kills", value);
			if (hasKills) challenges.zombieKills = value.progress;
			const auto hasDeaths = TryParseChallengeEntry(data, "stat_zw3_zombie_deaths", value);
			if (hasDeaths) challenges.zombieDeaths = value.progress;
			const auto hasRevives = TryParseChallengeEntry(data, "stat_zw3_zombie_revives", value);
			if (hasRevives) challenges.zombieRevives = value.progress;

			// Older or briefly out-of-sync challenge files retain the largest known
			// value. Lifetime counters can exceed the final challenge milestone.
			if (const auto it = challenges.entries.find("ch_zw3_zombie_killer"); it != challenges.entries.end())
			{
				challenges.zombieKills = std::max(challenges.zombieKills, it->second.progress);
			}
			if (const auto it = challenges.entries.find("ch_zw3_reviver"); it != challenges.entries.end())
			{
				challenges.zombieRevives = std::max(challenges.zombieRevives, it->second.progress);
			}

			return challenges;
		}

		void RefreshChallengeCategory(const int startIndex)
		{
			const auto definitions = ReadChallengeDefinitions();
			const auto challenges = ReadLocalBarracksChallenges(definitions);
			for (std::size_t slot = 0; slot < ChallengeSlotProgressDvars.size(); ++slot)
			{
				int progress = 0;
				int target = 0;
				int tier = 0;
				int tierCount = 0;
				int reward = 0;
				int percent = 0;
				bool complete = false;

				const auto definitionIndex = startIndex + static_cast<int>(slot);
				if (definitionIndex >= 0 && definitionIndex < static_cast<int>(definitions.size()))
				{
					const auto& definition = definitions[definitionIndex];
					tierCount = static_cast<int>(std::ranges::count_if(definition.targets,
						[](const int value) { return value > 0; }));
					if (const auto it = challenges.entries.find(definition.id); it != challenges.entries.end())
					{
						progress = std::max(it->second.progress, 0);
						tier = std::clamp(it->second.tier, 0, tierCount);
					}

					complete = tierCount > 0 && tier >= tierCount;
					const auto targetTier = complete ? tierCount - 1 : tier;
					if (targetTier >= 0)
					{
						target = definition.targets[targetTier];
						reward = complete ? 0 : definition.rewards[targetTier];
					}
					if (complete)
					{
						percent = 100;
					}
					else if (target > 0)
					{
						percent = std::clamp(static_cast<int>(
							(static_cast<std::int64_t>(progress) * 100) / target), 0, 100);
					}
				}

				Dvar::Var(ChallengeSlotProgressDvars[slot]).set(progress);
				Dvar::Var(ChallengeSlotTargetDvars[slot]).set(target);
				Dvar::Var(ChallengeSlotTierDvars[slot]).set(tier);
				Dvar::Var(ChallengeSlotTierCountDvars[slot]).set(tierCount);
				Dvar::Var(ChallengeSlotRewardDvars[slot]).set(reward);
				Dvar::Var(ChallengeSlotPercentDvars[slot]).set(percent);
				Dvar::Var(ChallengeSlotCompleteDvars[slot]).set(complete);
			}
		}

		void RefreshBarracksProfile()
		{
			const auto rank = ReadLocalBarracksRank();
			Dvar::Var("zw3_barracks_rank_known").set(rank.has_value());
			if (!rank)
			{
				Dvar::Var("zw3_barracks_rank_level").set(1);
				Dvar::Var("zw3_barracks_rank_prestige").set(0);
				Dvar::Var("zw3_barracks_rank_experience").set(0);
				Dvar::Var("zw3_barracks_rank_experience_target").set(50);
				Dvar::Var("zw3_barracks_rank_experience_percent").set(0);
				Dvar::Var("zw3_barracks_rank_icon").set("prestige_1");
			}
			else
			{
				Dvar::Var("zw3_barracks_rank_level").set(rank->level);
				Dvar::Var("zw3_barracks_rank_prestige").set(rank->prestige);
				Dvar::Var("zw3_barracks_rank_experience").set(rank->experience);
				Dvar::Var("zw3_barracks_rank_experience_target").set(rank->experienceTarget);
				Dvar::Var("zw3_barracks_rank_experience_percent").set(std::clamp(static_cast<int>(
					(static_cast<std::int64_t>(rank->experience) * 100) / rank->experienceTarget), 0, 100));
				Dvar::Var("zw3_barracks_rank_icon").set(ZombiePrestigeIcon(rank->prestige));
			}

			const auto definitions = ReadChallengeDefinitions();
			const auto challenges = ReadLocalBarracksChallenges(definitions);
			Dvar::Var("zw3_barracks_zombie_kills").set(challenges.zombieKills);
			Dvar::Var("zw3_barracks_zombie_deaths").set(challenges.zombieDeaths);
			Dvar::Var("zw3_barracks_zombie_revives").set(challenges.zombieRevives);
		}

		SharedLobbyRankMap ParseSharedLobbyRanks(
			const nlohmann::json& party)
		{
			SharedLobbyRankMap ranks;
			const auto settingsIt = party.find("zombie_settings");
			if (settingsIt != party.end() && settingsIt->is_object())
			{
				const auto ranksIt = settingsIt->find("zwnet_player_ranks");
				if (ranksIt != settingsIt->end() && ranksIt->is_object())
				{
					for (const auto& [guid, value] : ranksIt->items())
					{
						if (guid.size() != 16 || !value.is_object()) continue;
						SharedLobbyRank rank;
						rank.level = std::clamp(value.value("level", 1), 1, 54);
						rank.prestige = std::max(value.value("prestige", 0), 0);
						ranks[guid] = rank;
					}
				}
			}

			const auto membersIt = party.find("members");
			if (membersIt != party.end() && membersIt->is_array())
			{
				for (const auto& member : *membersIt)
				{
					if (!member.is_object()) continue;
					const auto playerId = member.find("player_id");
					if (playerId == member.end() || !playerId->is_string()) continue;
					const auto guid = playerId->get<std::string>();
					if (guid.size() != 16 || !std::ranges::all_of(guid,
						[](const unsigned char character) { return std::isxdigit(character) != 0; })) continue;
					SharedLobbyRank rank;
					bool rankKnown = false;
					const auto rankIt = member.find("rank");
					if (rankIt != member.end() && rankIt->is_object() &&
						rankIt->contains("level") && rankIt->at("level").is_number_integer())
					{
						rank.level = std::clamp(rankIt->at("level").get<int>(), 1, 54);
						rank.prestige = rankIt->contains("prestige") &&
							rankIt->at("prestige").is_number_integer()
							? std::max(rankIt->at("prestige").get<int>(), 0)
							: 0;
						rankKnown = true;
					}
					else if (member.contains("level") && member.at("level").is_number_integer())
					{
						rank.level = std::clamp(member.at("level").get<int>(), 1, 54);
						rank.prestige = member.contains("prestige") &&
							member.at("prestige").is_number_integer()
							? std::max(member.at("prestige").get<int>(), 0)
							: 0;
						rankKnown = true;
					}
					if (!rankKnown) continue;
					ranks[guid] = rank;
				}
			}

			return ranks;
		}

		SharedLobbyRankMap CacheSharedLobbyRanks(
			const nlohmann::json& party)
		{
			auto ranks = ParseSharedLobbyRanks(party);
			const auto localGuid = FormatPublicGuid();
			if (const auto localRank = ReadLocalLobbyRank())
			{
				ranks[localGuid] = *localRank;
			}
			else
			{
				ranks.erase(localGuid);
			}
			const auto partyId = party.value("id", std::string{});
			{
				std::lock_guard lock(SharedLobbyRankMutex);
				SharedLobbyRankPartyId = partyId;
				SharedLobbyRanks = ranks;
			}
			return ranks;
		}

		SharedLobbyRankMap GetCachedSharedLobbyRanks()
		{
			std::lock_guard lock(SharedLobbyRankMutex);
			return SharedLobbyRanks;
		}

		void PatchMaterialEnumerationScratch()
		{
			static_assert(sizeof(Game::XAssetHeader) == sizeof(void*));
			constexpr std::array<DWORD, 12> assetArrayOperands
			{
				0x507AB9, 0x50D8BB, 0x50DBEE, 0x50DC41, 0x50DC95, 0x50DCED,
				0x518C22, 0x5239D5, 0x523A0C, 0x523A47, 0x553EE5, 0x5545E6
			};
			constexpr std::array<DWORD, 7> assetCountOperands
			{
				0x518C0C, 0x518C17, 0x5239CA, 0x523A01, 0x523A12, 0x523A2E, 0x523A3C
			};

			for (const auto operand : assetArrayOperands)
			{
				Utils::Hook::Set<Game::XAssetHeader*>(operand, MaterialEnumerationAssets.data());
			}
			for (const auto operand : assetCountOperands)
			{
				Utils::Hook::Set<std::uint32_t*>(operand, &MaterialEnumerationCount);
			}

			// The ZW3 material pool is already expanded to 16384 entries, while the
			// stock renderer scratch list only holds 4096. A successful online join
			// can enumerate more than 4096 map materials and overwrite the adjacent
			// counter before the first frame. Keep the renderer scratch capacity in
			// lockstep with the existing material pool.
			Logger::Print("ZWNET material enumeration scratch: {} entries\n", ZWNET_MATERIAL_ENUM_CAPACITY);
		}

		std::string EncodeLowerHex(const std::string& bytes)
		{
			static constexpr char digits[] = "0123456789abcdef";
			std::string result;
			result.reserve(bytes.size() * 2);
			for (const auto byte : bytes)
			{
				const auto value = static_cast<unsigned char>(byte);
				result.push_back(digits[value >> 4]);
				result.push_back(digits[value & 0x0F]);
			}
			return result;
		}

		const char* PublicGuidText()
		{
			static constexpr char digits[] = "0123456789abcdef";
			static const auto result = []
			{
				std::array<char, 17> buffer{};
				auto value = Auth::GetKeyHash();
				for (std::size_t index = 0; index < 16; ++index)
				{
					buffer[15 - index] = digits[value & 0x0F];
					value >>= 4;
				}
				return buffer;
			}();
			return result.data();
		}

		std::string FormatPublicGuid()
		{
			return PublicGuidText();
		}

		std::string FriendlyStateText(const std::string& state)
		{
			if (state == "SIGNING_IN") return "SIGNING IN";
			if (state == "LOGIN_REQUIRED") return "LOGIN REQUIRED";
			if (state == "SEARCH_STARTING") return "STARTING SEARCH";
			if (state == "IN_PARTY") return "IN PARTY";
			if (state == "MATCH_FOUND") return "MATCH FOUND";
			if (state == "MAP_VOTE") return "MAP VOTE";
			if (state == "READY_CHECK" || state == "WAITING_FOR_READY") return "WAITING FOR READY";
			if (state == "RESERVING_SERVER") return "RESERVING SERVER";
			if (state == "STARTING_SERVER" || state == "SERVER_STARTING") return "SERVER STARTING";
			if (state == "COUNTDOWN") return "JOIN COUNTDOWN";
			if (state == "DIRECT_CONNECTION") return "DIRECT CONNECTION";
			if (state == "RELAY_CONNECTION") return "RELAY CONNECTION";
			if (state == "IN_MATCH") return "IN MATCH";
			return state;
		}

		std::string FriendlyErrorText(const std::string& error)
		{
			if (error.empty()) return {};
			if (error == "ZWNET_LOGIN_REQUIRED") return "Sign in on the ZW3 Stats page.";
			if (error == "ZWNET_SESSION_EXPIRED") return "Your ZW3 session expired. Please sign in again.";
			if (error == "ZWNET_SEARCH_FAILED") return "Quick Play could not enter matchmaking. Please try again.";
			if (error == "ZWNET_VERSION_MISMATCH") return "Your ZW3 client version does not match the online service.";
			if (error == "ZWNET_LEADER_REQUIRED") return "Only the party leader can start Quick Play.";
			if (error == "ZWNET_PARTY_TOO_LARGE") return "This party has too many players for Quick Play.";
			if (error == "ZWNET_CONTENT_MISSING") return "Required ZW3 content is missing.";
			if (error == "ZWNET_ROUTE_UNAVAILABLE") return "No direct or relay route is available.";
			if (error == "ZWNET_SERVER_NOT_READY") return "The assigned ZW3 server is no longer available. Return to the lobby and search again.";
			if (error == "ZWNET_DESCRIPTOR_INVALID") return "The assigned server address is invalid.";
			if (error == "ZWNET_ACCOUNT_LINK_REQUIRED") return "Link this GUID in ZW3 Stats Settings.";
			if (error == "ZWNET_SESSION_STORAGE_FAILED") return "The secure ZW3 session could not be stored.";
			if (error == "ZWNET_REGISTRATION_UNAVAILABLE") return "The ZW3 account page is unavailable.";
			if (error == "ZWNET_PARTY_FAILED") return "The ZW3 party could not be created or loaded.";
			if (error == "ZWNET_PRIVATE_MATCH_FAILED") return "The private ZW3 server could not be reserved.";
			if (error == "ZWNET_MAP_VOTE_FAILED") return "Your map vote could not be submitted.";
			if (error == "ZWNET_MATCH_FAILED") return "The assigned ZW3 server could not be started. Return to the lobby and try again.";
			if (error == "ZWNET_GUID_COPY_FAILED") return "The ZW3 GUID could not be copied.";
			return "The ZW3 online service could not complete this request.";
		}

		std::string JsonString(const nlohmann::json& object, const char* key, const char* fallback = "")
		{
			if (!object.is_object()) return fallback;
			const auto it = object.find(key);
			return it != object.end() && it->is_string() ? it->get<std::string>() : std::string{fallback};
		}

		std::string SafeDisplayName(std::string value)
		{
			constexpr std::size_t maxLength = 48;
			value = TextRenderer::EncodeUtf8ForGame(value, maxLength);
			Utils::String::Trim(value);
			return value.empty() ? "ZW3 Player" : value;
		}

		std::string ResponseErrorCode(const nlohmann::json& response)
		{
			if (!response.contains("error") || !response.at("error").is_object()) return {};
			return JsonString(response.at("error"), "code");
		}

		bool IsActiveMatchmakingState(const std::string& state)
		{
			return state == "SEARCHING"
				|| state == "MATCH_FOUND"
				|| state == "MAP_VOTE"
				|| state == "READY_CHECK"
				|| state == "WAITING_FOR_READY"
				|| state == "RESERVING_SERVER"
				|| state == "STARTING_SERVER"
				|| state == "SERVER_STARTING"
				|| state == "CONNECTING"
				|| state == "IN_MATCH";
		}

		std::uint64_t NextPresenceSequence()
		{
			static std::atomic_uint64_t sequence{1};
			return sequence.fetch_add(1, std::memory_order_relaxed);
		}

		bool CopyPublicGuidToClipboard()
		{
			if (!OpenClipboard(GetDesktopWindow())) return false;
			const auto closeClipboard = gsl::finally([] { CloseClipboard(); });
			if (!EmptyClipboard()) return false;

			const auto guid = FormatPublicGuid();
			auto memory = GlobalAlloc(GMEM_MOVEABLE, guid.size() + 1);
			if (!memory) return false;
			bool clipboardOwnsMemory = false;
			const auto releaseMemory = gsl::finally([&]
			{
				if (!clipboardOwnsMemory) GlobalFree(memory);
			});

			auto* destination = static_cast<char*>(GlobalLock(memory));
			if (!destination) return false;
			std::memcpy(destination, guid.c_str(), guid.size() + 1);
			GlobalUnlock(memory);
			if (!SetClipboardData(CF_TEXT, memory)) return false;
			clipboardOwnsMemory = true;
			return true;
		}

	}

	std::atomic_bool& ZWNet::ActiveState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::SearchingState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::ClosingOnlineSessionState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::ServerJoinTransitionState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::OnlineEntryPendingState() { static std::atomic_bool value = false; return value; }
	std::atomic_bool& ZWNet::InGameState() { static std::atomic_bool value = false; return value; }
	bool& ZWNet::LoginInFlightState() { static bool value = false; return value; }
	std::mutex& ZWNet::StateMutex() { static std::mutex value; return value; }
	std::string& ZWNet::AccessTokenState() { static std::string value; return value; }
	std::string& ZWNet::RefreshTokenState() { static std::string value; return value; }
	std::string& ZWNet::CurrentPartyIdState() { static std::string value; return value; }
	std::string& ZWNet::CurrentPlayerIdState() { static std::string value; return value; }
	std::string& ZWNet::CurrentProposalIdState() { static std::string value; return value; }
	std::string& ZWNet::CurrentMatchIdState() { static std::string value; return value; }
	std::mutex& ZWNet::AsyncTaskMutex() { static std::mutex value; return value; }
	std::deque<std::function<void()>>& ZWNet::AsyncTasks() { static std::deque<std::function<void()>> value; return value; }

	bool ZWNet::TryGetSharedLobbyRank(const std::string& guid, int& level, int& prestige)
	{
		auto normalizedGuid = guid;
		std::ranges::transform(normalizedGuid, normalizedGuid.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});

		std::lock_guard lock(SharedLobbyRankMutex);
		const auto rank = SharedLobbyRanks.find(normalizedGuid);
		if (rank == SharedLobbyRanks.end()) return false;
		level = rank->second.level;
		prestige = rank->second.prestige;
		return true;
	}

	void ZWNet::EnqueueAsync(std::function<void()> task)
	{
		if (!ActiveState()) return;
		std::lock_guard lock(AsyncTaskMutex());
		if (!ActiveState()) return;
		AsyncTasks().emplace_back(std::move(task));
	}

	void ZWNet::ProcessAsyncTasks()
	{
		if (!ActiveState()) return;
		std::function<void()> task;
		{
			std::lock_guard lock(AsyncTaskMutex());
			if (!ActiveState() || AsyncTasks().empty()) return;
			task = std::move(AsyncTasks().front());
			AsyncTasks().pop_front();
		}
		if (!ActiveState()) return;
		try
		{
			task();
		}
		catch (const std::exception&)
		{
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
		}
		catch (...)
		{
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
		}
	}

	std::string ZWNet::SessionPath()
	{
		return (FileSystem::GetAppdataPath() / "zwnet.session").string();
	}

	bool ZWNet::StoreSession(const std::string& accessToken, const std::string& refreshToken)
	{
		if (accessToken.empty() || refreshToken.empty()) return false;
		const auto plain = nlohmann::json{{"access_token", accessToken}, {"refresh_token", refreshToken}}.dump();
		DATA_BLOB input{static_cast<DWORD>(plain.size()), reinterpret_cast<BYTE*>(const_cast<char*>(plain.data()))};
		DATA_BLOB output{};
		if (!CryptProtectData(&input, L"ZW3 ZWNET session", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
		const std::string encrypted{reinterpret_cast<char*>(output.pbData), output.cbData};
		LocalFree(output.pbData);
		if (!Utils::IO::WriteFile(SessionPath(), encrypted)) return false;
		std::lock_guard lock(StateMutex());
		AccessTokenState() = accessToken;
		RefreshTokenState() = refreshToken;
		return true;
	}

	bool ZWNet::LoadSession()
	{
		const auto encrypted = Utils::IO::ReadFile(SessionPath());
		if (encrypted.empty()) return false;
		DATA_BLOB input{static_cast<DWORD>(encrypted.size()), reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()))};
		DATA_BLOB output{};
		if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) return false;
		const std::string plain{reinterpret_cast<char*>(output.pbData), output.cbData};
		SecureZeroMemory(output.pbData, output.cbData);
		LocalFree(output.pbData);
		try
		{
			const auto data = nlohmann::json::parse(plain);
			std::lock_guard lock(StateMutex());
			AccessTokenState() = data.at("access_token").get<std::string>();
			RefreshTokenState() = data.at("refresh_token").get<std::string>();
			return !AccessTokenState().empty() && !RefreshTokenState().empty();
		}
		catch (const nlohmann::json::exception&)
		{
			return false;
		}
	}

	void ZWNet::ClearSession()
	{
		std::lock_guard lock(StateMutex());
		std::ranges::fill(AccessTokenState(), '\0');
		std::ranges::fill(RefreshTokenState(), '\0');
		AccessTokenState().clear(); RefreshTokenState().clear();
		Utils::IO::RemoveFile(SessionPath());
	}

	std::optional<nlohmann::json> ZWNet::Request(const std::string& method, const std::string& path, const nlohmann::json& body)
	{
		if (!ActiveState()) return std::nullopt;
		try
		{
			std::string token;
			{
				std::lock_guard lock(StateMutex());
				token = AccessTokenState();
			}
			Utils::WebIO::params headers{{"Accept", "application/json"}, {"Content-Type", "application/json"}};
			if (!token.empty()) headers["Authorization"] = "Bearer " + token;
			bool success = false;
			// Dvars belong to the main game thread, while all HTTP requests run on the
			// asynchronous scheduler. Keep the trusted production endpoint immutable.
			Utils::WebIO request("ZW3-ZWNET/3.0.3", std::string(ZWNET_API_BASE) + path);
			const auto response = method == "GET" ? request.setTimeout(5000)->get(headers, &success) : request.setTimeout(5000)->post(body.dump(), headers, &success);
			if (!ActiveState() || response.empty()) return std::nullopt;
			auto parsed = nlohmann::json::parse(response);
			if (!success && !parsed.contains("error")) return std::nullopt;
			return parsed;
		}
		catch (const std::exception&) { return std::nullopt; }
		catch (...) { return std::nullopt; }
	}

	void ZWNet::SetState(const std::string& state, const std::string& error)
	{
		const auto stateText = FriendlyStateText(state);
		const auto errorText = FriendlyErrorText(error);
		Scheduler::Once([state, stateText, error, errorText]
		{
			if (!ActiveState()) return;
			Dvar::Var("ui_zwnet_state").set(state);
			Dvar::Var("ui_zwnet_state_text").set(stateText);
			Dvar::Var("ui_zwnet_error").set(error);
			Dvar::Var("ui_zwnet_error_text").set(errorText);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::Refresh()
	{
		if (!ActiveState()) return;
		std::string refresh;
		{ std::lock_guard lock(StateMutex()); refresh = RefreshTokenState(); }
		if (refresh.empty()) { Login(); return; }
		const auto result = Request("POST", "/social/client/refresh", {{"refresh_token", refresh}});
		if (!result || result->contains("error"))
		{
			if (!ActiveState()) return;
			ClearSession();
			Login();
			return;
		}
		if (!ActiveState()) return;
		const auto access = result->at("access_token").get<std::string>();
		StoreSession(access, result->value("refresh_token", access));
		if (const auto me = Request("GET", "/social/me"); me && me->contains("id"))
		{
			std::lock_guard lock(StateMutex());
			CurrentPlayerIdState() = me->at("id").get<std::string>();
		}
		SetState("ONLINE");
		CompleteOnlineEntry();
		Request("POST", "/social/presence", {{"status", "MAIN_MENU"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
	}

	void ZWNet::Login()
	{
		if (!ActiveState()) return;
		{
			std::lock_guard lock(StateMutex());
			if (LoginInFlightState()) return;
			LoginInFlightState() = true;
		}
		SetState("SIGNING_IN");
		const auto guid = FormatPublicGuid();
		const auto entropy = Auth::GetMachineEntropy();
		const auto deviceId = EncodeLowerHex(Utils::Cryptography::SHA256::Compute(entropy));
		if (deviceId.size() != 64)
		{
			std::lock_guard lock(StateMutex());
			LoginInFlightState() = false;
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
			return;
		}
		EnqueueAsync([requestBody = nlohmann::json{{"guid", guid}, {"device_id", deviceId}, {"client_version", ZWNET_CLIENT_VERSION}, {"mod_version", ZWNET_MOD_VERSION}}]() mutable
		{
			CompleteLogin(std::move(requestBody));
		});
	}

	void ZWNet::CompleteLogin(nlohmann::json requestBody)
	{
		if (!ActiveState()) return;
		const auto loginGuard = gsl::finally([]
		{
			std::lock_guard lock(StateMutex());
			LoginInFlightState() = false;
		});
		const auto result = Request("POST", "/social/client/login", requestBody);
		if (!ActiveState()) return;
		if (!result)
		{
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
			return;
		}
		if (result->contains("error"))
		{
			SetState("LOGIN_REQUIRED", "ZWNET_ACCOUNT_LINK_REQUIRED");
			return;
		}
		const auto access = result->value("access_token", result->value("token", ""));
		const auto refresh = result->value("refresh_token", access);
		if (!StoreSession(access, refresh))
		{
			SetState("ERROR", "ZWNET_SESSION_STORAGE_FAILED");
			return;
		}
		if (result->contains("profile") && result->at("profile").contains("id"))
		{
			std::lock_guard lock(StateMutex());
			CurrentPlayerIdState() = result->at("profile").at("id").get<std::string>();
		}
		SetState("ONLINE");
		CompleteOnlineEntry();
		Request("POST", "/social/presence", {{"status", "MAIN_MENU"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
	}

	void ZWNet::BeginOnlineEntry()
	{
		if (!ActiveState()) return;
		OnlineEntryPendingState() = true;
		SetState("SIGNING_IN");

		bool hasRefreshToken = false;
		{
			std::lock_guard lock(StateMutex());
			hasRefreshToken = !RefreshTokenState().empty();
		}
		if (hasRefreshToken) EnqueueAsync([] { Refresh(); });
		else Login();
	}

	void ZWNet::CompleteOnlineEntry()
	{
		EnqueueAsync([]
		{
			if (!ActiveState() || !OnlineEntryPendingState()) return;

			auto party = Request("GET", "/zwnet/parties/current");
			if (!party || party->is_null())
			{
				party = Request("POST", "/zwnet/parties/create",
					{{"visibility", PartyVisibilityName(DesiredPartyPrivacy().load())}});
			}

			if (!party || party->is_null() || party->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
			}
			else
			{
				*party = PublishLocalRank(std::move(*party));
				UpdateLobbyDvars(*party);
				const auto partyState = party->value("state", "IDLE");
				const auto activeMatchmaking = IsActiveMatchmakingState(partyState);
				SearchingState() = activeMatchmaking;
				SetState(activeMatchmaking ? partyState : "IN_PARTY");
				if (activeMatchmaking) UpdateMatchmaking();
			}

			Scheduler::Once([]
			{
				if (!ActiveState() || !OnlineEntryPendingState().exchange(false)) return;
				if (auto* connecting = Game::Menus_FindByName(Game::uiContext, "popup_zwnet_connecting"))
				{
					Game::Menus_CloseRequest(Game::uiContext, connecting);
				}
				Game::Menus_OpenByName(Game::uiContext, "zwnet_matchmaking");
			}, Scheduler::Pipeline::MAIN);
		});
	}

	void ZWNet::AbandonOnlineSession()
	{
		ResetMatchLobbySoundSnapshot();
		SearchingState() = false;
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		OnlineEntryPendingState() = false;
		LocalPartyLeader() = false;
		CachedPartyMemberCount() = 0;
		CachedPartyVisibility() = 2;
		CachedPartyJoinStateSupported() = false;
		{
			std::lock_guard lock(StateMutex());
			CurrentPartyIdState().clear();
			CurrentProposalIdState().clear();
			CurrentMatchIdState().clear();
		}
		Dvar::Var("zwnet_lobby_active").set(false);
		Dvar::Var("zwnet_lobby_party_id").set("");
		Dvar::Var("zwnet_lobby_visibility").set("OPEN");
		Dvar::Var("zwnet_lobby_owner").set("");
		Dvar::Var("zwnet_lobby_member_count").set(0);
		Dvar::Var("zwnet_lobby_status_text").set("IDLE");
		Dvar::Var("zwnet_lobby_can_start").set(false);
		Dvar::Var("zwnet_lobby_self_ready").set(false);
		for (std::size_t i = 0; i < 4; ++i)
		{
			const auto prefix = std::format("zwnet_lobby_member_{}", i);
			Dvar::Var(prefix + "_name").set("");
			Dvar::Var(prefix + "_guid").set("");
			Dvar::Var(prefix + "_role").set("");
			Dvar::Var(prefix + "_ready").set(false);
			Dvar::Var(prefix + "_self").set(false);
			Dvar::Var(prefix + "_shared_rank_known").set(false);
			Dvar::Var(prefix + "_shared_rank_level").set(1);
			Dvar::Var(prefix + "_shared_rank_prestige").set(0);
			Dvar::Var(prefix + "_rank_icon").set("");
			Dvar::Var(prefix + "_rank_level").set("");
		}
		Dvar::Var("zwnet_vote_active").set(false);
		Dvar::Var("zwnet_vote_selection").set("");
		Dvar::Var("zwnet_all_ready").set(false);
		Dvar::Var("zwnet_start_phase").set("");
		Dvar::Var("zwnet_start_seconds").set(0);
		Dvar::Var("zwnet_vote_winner_id").set("");
		Dvar::Var("zwnet_vote_winner_name").set("");
		Dvar::Var("zwnet_vote_winner_image").set("");
		Dvar::Var("zwnet_match_id").set("");
		Dvar::Var("zwnet_server_endpoint").set("");
		Dvar::Var("zwnet_server_hostname").set("");
		Dvar::Var("zwnet_server_status").set("NOT ASSIGNED");
		Dvar::Var("zwnet_join_status").set("WAITING IN LOBBY");
		Dvar::Var("zwnet_join_countdown").set(0);
	}

	void ZWNet::Register()
	{
		const auto result = Request("GET", "/social/client/register");
		if (!result || !result->contains("guid_link_url"))
		{
			SetState("ERROR", "ZWNET_REGISTRATION_UNAVAILABLE");
			return;
		}
		const auto url = JsonString(*result, "guid_link_url");
		if (url != "https://stats.zw3.eu/settings")
		{
			SetState("ERROR", "ZWNET_REGISTRATION_UNAVAILABLE");
			return;
		}
		Scheduler::Once([url]
		{
			if (!ActiveState()) return;
			Command::Execute("openLink " + url, false);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::StartQuickPlay()
	{
		ResetMatchLobbySoundSnapshot();
		SetState("SEARCH_STARTING");
		auto party = Request("GET", "/zwnet/parties/current");
		if (!party || party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create",
				{{"visibility", PartyVisibilityName(DesiredPartyPrivacy().load())}});
		}
		if (!party || party->is_null() || party->contains("error"))
		{
			SearchingState() = false;
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		party = ApplyPartyVisibility(std::move(*party));
		if (!party)
		{
			SearchingState() = false;
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		*party = PublishLocalRank(std::move(*party));
		UpdateLobbyDvars(*party);
		const auto partyState = party->value("state", "IDLE");
		if (IsActiveMatchmakingState(partyState))
		{
			SearchingState() = true;
			SetState(partyState);
			UpdateMatchmaking();
			return;
		}
		const auto result = Request("POST", "/zwnet/matchmaking/search", {{"playlist_id", "zombies-quickplay"}, {"region_id", "eu-central"}, {"client_version", ZWNET_CLIENT_VERSION}, {"mod_version", ZWNET_MOD_VERSION}, {"content", nlohmann::json::array()}, {"ping_ms", 50}});
		if (!result)
		{
			SearchingState() = false;
			SetState("ERROR", "ZWNET_SEARCH_FAILED");
			return;
		}
		if (result->contains("error"))
		{
			const auto code = ResponseErrorCode(*result);
			if (code == "ALREADY_QUEUED" || code == "ALREADY_MATCHED")
			{
				SearchingState() = true;
				SetState("SEARCHING");
				UpdateMatchmaking();
				return;
			}
			SearchingState() = false;
			if (code == "VERSION_MISMATCH") SetState("ERROR", "ZWNET_VERSION_MISMATCH");
			else if (code == "LEADER_REQUIRED") SetState("ERROR", "ZWNET_LEADER_REQUIRED");
			else if (code == "PARTY_TOO_LARGE") SetState("ERROR", "ZWNET_PARTY_TOO_LARGE");
			else if (code == "CONTENT_MISSING") SetState("ERROR", "ZWNET_CONTENT_MISSING");
			else SetState("ERROR", "ZWNET_SEARCH_FAILED");
			return;
		}
		SearchingState() = true; SetState("SEARCHING");
	}

	std::optional<nlohmann::json> ZWNet::ApplyPartyVisibility(nlohmann::json party)
	{
		if (!party.is_object() || party.contains("error")) return std::nullopt;
		const auto partyId = JsonString(party, "id");
		if (!IsOpaquePartyId(partyId)) return std::nullopt;

		std::string currentPlayerId;
		{
			std::lock_guard lock(StateMutex());
			currentPlayerId = CurrentPlayerIdState();
		}
		const auto isLeader = !currentPlayerId.empty() &&
			JsonString(party, "leader_id") == currentPlayerId;
		LocalPartyLeader() = isLeader;
		if (!isLeader) return party;

		const auto desiredVisibility = std::string{
			PartyVisibilityName(DesiredPartyPrivacy().load())};
		const auto currentVisibility = NormalizePartyVisibility(
			JsonString(party, "visibility", "OPEN"));
		if (currentVisibility == desiredVisibility) return party;

		const auto updated = Request("POST",
			"/zwnet/parties/" + partyId + "/set-visibility",
			{{"visibility", desiredVisibility}});
		if (!updated || !updated->is_object() || updated->contains("error"))
		{
			return std::nullopt;
		}
		return *updated;
	}

	void ZWNet::RefreshPartyVisibility()
	{
		const auto resetPending = gsl::finally([]
		{
			VisibilitySyncPending() = false;
		});
		if (!ActiveState() || !LocalPartyLeader()) return;
		const auto party = Request("GET", "/zwnet/parties/current");
		if (!party || party->is_null() || party->contains("error")) return;
		const auto updated = ApplyPartyVisibility(*party);
		if (updated) UpdateLobbyDvars(*updated);
	}

	void ZWNet::CapturePartyPrivacy()
	{
		if (!ActiveState()) return;
		const auto privacy = std::clamp(
			Dvar::Var("partyPrivacy").get<int>(), 0, 2);
		DesiredPartyPrivacy() = privacy;
		if (LocalPartyLeader() && CachedPartyVisibility().load() != privacy &&
			!VisibilitySyncPending().exchange(true))
		{
			EnqueueAsync([] { RefreshPartyVisibility(); });
		}
	}

	nlohmann::json ZWNet::PublishLocalRank(nlohmann::json party)
	{
		if (!party.is_object() || party.contains("error"))
		{
			return party;
		}

		const auto partyId = JsonString(party, "id");
		if (partyId.empty())
		{
			return party;
		}

		const auto guid = FormatPublicGuid();
		const auto rank = ReadLocalLobbyRank();
		if (!rank)
		{
			CacheSharedLobbyRanks(party);
			return party;
		}
		const auto publishedRanks = ParseSharedLobbyRanks(party);
		const auto publishedRank = publishedRanks.find(guid);
		if (publishedRank != publishedRanks.end() &&
			publishedRank->second.level == rank->level &&
			publishedRank->second.prestige == rank->prestige)
		{
			CacheSharedLobbyRanks(party);
			return party;
		}

		const auto now = std::chrono::steady_clock::now();
		{
			std::lock_guard lock(SharedLobbyRankMutex);
			const auto rankChanged =
				partyId != LastRankPublishPartyId ||
				rank->level != LastRankPublishLevel ||
				rank->prestige != LastRankPublishPrestige;
			if (!rankChanged && now < NextRankPublishAttempt)
			{
				SharedLobbyRanks[guid] = *rank;
				return party;
			}

			LastRankPublishPartyId = partyId;
			LastRankPublishLevel = rank->level;
			LastRankPublishPrestige = rank->prestige;
			NextRankPublishAttempt = now + std::chrono::seconds(30);
		}

		const auto result = Request(
			"POST",
			"/zwnet/parties/" + partyId + "/player-rank",
			{{"level", rank->level}, {"prestige", rank->prestige}});
		if (result && result->is_object() && !result->contains("error"))
		{
			party = *result;
		}

		CacheSharedLobbyRanks(party);
		return party;
	}

	void ZWNet::UpdateLobbyDvars(const nlohmann::json& party)
	{
		if (party.is_null() || !party.is_object()) return;
		const auto sharedRanks = CacheSharedLobbyRanks(party);
		const auto partyId = JsonString(party, "id");
		const auto leaderId = JsonString(party, "leader_id");
		const auto state = JsonString(party, "state", "IDLE");
		const auto members = party.value("members", nlohmann::json::array());
		std::string owner;
		std::string currentPlayerId;
		{ std::lock_guard lock(StateMutex()); currentPlayerId = CurrentPlayerIdState(); }
		LocalPartyLeader() = !currentPlayerId.empty() && currentPlayerId == leaderId;
		const auto visibility = NormalizePartyVisibility(
			JsonString(party, "visibility", "OPEN"));
		CachedPartyMemberCount() = static_cast<int>(members.size());
		CachedPartyVisibility() = PartyVisibilityValue(visibility);
		CachedPartyJoinStateSupported() = IsPartyJoinStateSupported(state);
		for (const auto& member : members)
		{
			if (JsonString(member, "player_id") == leaderId)
			{
				owner = SafeDisplayName(JsonString(member, "display_name"));
				break;
			}
		}
		{
			std::lock_guard lock(StateMutex());
			CurrentPartyIdState() = partyId;
		}
		const auto stateText = FriendlyStateText(state);
		Scheduler::Once([partyId, leaderId, state, stateText, owner, members, currentPlayerId, sharedRanks, visibility]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_lobby_active").set(true);
			Dvar::Var("zwnet_lobby_party_id").set(partyId);
			Dvar::Var("zwnet_lobby_visibility").set(visibility);
			Dvar::Var("zwnet_lobby_owner").set(owner);
			Dvar::Var("zwnet_lobby_member_count").set(static_cast<int>(members.size()));
			Dvar::Var("zwnet_lobby_status_text").set(stateText);
			bool allReady = !members.empty();
			bool selfReady = false;
			for (std::size_t i = 0; i < 4; ++i)
			{
				const auto prefix = std::format("zwnet_lobby_member_{}", i);
				if (i < members.size())
				{
					const auto& member = members[i];
					const auto ready = member.value("ready", 0) == 1;
					const auto isSelf = JsonString(member, "player_id") == currentPlayerId;
					const auto displayName = SafeDisplayName(JsonString(member, "display_name"));
					Dvar::Var(prefix + "_name").set(displayName);
					Dvar::Var(prefix + "_guid").set(JsonString(member, "player_id"));
					Dvar::Var(prefix + "_role").set(JsonString(member, "player_id") == leaderId ? "PARTY LEADER" : "MEMBER");
					Dvar::Var(prefix + "_ready").set(ready);
					Dvar::Var(prefix + "_self").set(isSelf);
					const auto rankIt = sharedRanks.find(
						JsonString(member, "player_id"));
					const auto rankKnown = rankIt != sharedRanks.end();
					Dvar::Var(prefix + "_shared_rank_known").set(rankKnown);
					Dvar::Var(prefix + "_shared_rank_level").set(
						rankKnown ? rankIt->second.level : 1);
					Dvar::Var(prefix + "_shared_rank_prestige").set(
						rankKnown ? rankIt->second.prestige : 0);
					allReady = allReady && ready;
					if (isSelf) selfReady = ready;
				}
				else
				{
					Dvar::Var(prefix + "_name").set("");
					Dvar::Var(prefix + "_guid").set("");
					Dvar::Var(prefix + "_role").set("");
					Dvar::Var(prefix + "_ready").set(false);
					Dvar::Var(prefix + "_self").set(false);
					Dvar::Var(prefix + "_shared_rank_known").set(false);
					Dvar::Var(prefix + "_shared_rank_level").set(1);
					Dvar::Var(prefix + "_shared_rank_prestige").set(0);
				}
			}
			Dvar::Var("zwnet_lobby_self_ready").set(selfReady);
			Dvar::Var("zwnet_all_ready").set(allReady);
			Dvar::Var("zwnet_lobby_can_start").set(currentPlayerId == leaderId && allReady && state != "SEARCHING" && state != "IN_MATCH");
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::ResumeParty(const nlohmann::json& party)
	{
		if (!ActiveState() || !party.is_object() || JsonString(party, "id").empty()) return;
		auto resumedParty = PublishLocalRank(party);
		const auto partyState = JsonString(resumedParty, "state", "IDLE");
		const auto activeMatchmaking = IsActiveMatchmakingState(partyState);
		SearchingState() = activeMatchmaking;
		UpdateLobbyDvars(resumedParty);
		SetState(activeMatchmaking ? partyState : "IN_PARTY");
		Scheduler::Once([activeMatchmaking]
		{
			if (!ActiveState()) return;
			Command::Execute(activeMatchmaking
				? "openmenu zwnet_matchmaking"
				: "openmenu zwnet_party_lobby", false);
		}, Scheduler::Pipeline::MAIN);
		if (activeMatchmaking) UpdateMatchmaking();
	}

	void ZWNet::JoinParty(const std::string& partyId)
	{
		if (!IsOpaquePartyId(partyId))
		{
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		EnqueueAsync([partyId]
		{
			const auto response = Request("POST",
				"/zwnet/parties/" + partyId + "/join",
				nlohmann::json::object());
			if (!response || !response->is_object() || response->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}
			ResumeParty(*response);
		});
	}

	void ZWNet::JoinCapability(const std::string& capability)
	{
		if (!IsOpaqueJoinCapability(capability))
		{
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
		EnqueueAsync([capability]
		{
			const auto response = Request("POST",
				"/zwnet/parties/join-capability",
				{{"capability", capability}});
			if (!response || !response->is_object() || response->contains("error"))
			{
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}
			ResumeParty(*response);
		});
	}

	bool ZWNet::BeginEndpointJoin(const std::string& endpoint)
	{
		if (!ActiveState() || endpoint.empty() || endpoint.size() > 255) return false;
		const Network::Address requestedTarget(endpoint);
		if (!requestedTarget.isValid()) return false;
		bool knownAssignedEndpoint = false;
		bool hasAccessToken = false;
		{
			std::lock_guard lock(StateMutex());
			hasAccessToken = !AccessTokenState().empty();
			const Network::Address assignedTarget(
				Dvar::Var("zwnet_server_endpoint").get<std::string>());
			knownAssignedEndpoint = !CurrentMatchIdState().empty() &&
				assignedTarget.isValid() && assignedTarget == requestedTarget;
		}
		EndpointJoinInFlight() = true;
		if (!hasAccessToken)
		{
			// Preserve native dedicated/private joins while requiring the endpoint's
			// existing getinfo response to prove that it is not a managed session.
			Scheduler::Once([endpoint]
			{
				EndpointJoinInFlight() = false;
				const Network::Address target(endpoint);
				if (target.isValid()) Party::Connect(target, false, true);
			}, Scheduler::Pipeline::MAIN);
			return true;
		}

		EnqueueAsync([endpoint, knownAssignedEndpoint]
		{
			const auto response = Request("POST",
				"/zwnet/matchmaking/join-by-endpoint",
				{{"endpoint", endpoint}});
			if (!response || !response->is_object())
			{
				if (knownAssignedEndpoint)
				{
					EndpointJoinInFlight() = false;
					SetState("ERROR", "ZWNET_REQUEST_FAILED");
					return;
				}
				// An unknown endpoint must prove through the existing getinfo flow
				// that it is unmanaged before the native connection may continue.
				Scheduler::Once([endpoint]
				{
					EndpointJoinInFlight() = false;
					const Network::Address target(endpoint);
					if (target.isValid()) Party::Connect(target, false, true);
				}, Scheduler::Pipeline::MAIN);
				return;
			}
			if (response->contains("error"))
			{
				EndpointJoinInFlight() = false;
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}

			if (!response->value("managed", false))
			{
				Scheduler::Once([endpoint]
				{
					EndpointJoinInFlight() = false;
					const Network::Address target(endpoint);
					if (target.isValid()) Party::Connect(target);
				}, Scheduler::Pipeline::MAIN);
				return;
			}

			const auto party = response->find("party");
			if (!response->value("connect", false) || party == response->end() ||
				!party->is_object() || party->contains("error"))
			{
				EndpointJoinInFlight() = false;
				SetState("ERROR", "ZWNET_PARTY_FAILED");
				return;
			}

			ServerJoinTransitionState() = true;
			ResumeParty(*party);
			Scheduler::Once([endpoint]
			{
				if (!ActiveState()) return;
				const Network::Address target(endpoint);
				if (!target.isValid())
				{
					EndpointJoinInFlight() = false;
					ServerJoinTransitionState() = false;
					SetState("ERROR", "ZWNET_DESCRIPTOR_INVALID");
					return;
				}
				Dvar::Var("zwnet_server_endpoint").set(endpoint);
				Dvar::Var("zwnet_join_status").set("JOINING SERVER");
				Scheduler::Once([]
				{
					EndpointJoinInFlight() = false;
					ServerJoinTransitionState() = false;
				}, Scheduler::Pipeline::MAIN, 12s);
				Party::Connect(target);
			}, Scheduler::Pipeline::MAIN);
		});
		return true;
	}

	void ZWNet::EnterLobby(std::string map)
	{
		auto party = Request("GET", "/zwnet/parties/current");
		if (!party || party->is_null())
		{
			party = Request("POST", "/zwnet/parties/create",
				{{"visibility", PartyVisibilityName(DesiredPartyPrivacy().load())}});
		}
		if (!party || party->contains("error")) { SetState("ERROR", "ZWNET_PARTY_FAILED"); return; }
		party = ApplyPartyVisibility(std::move(*party));
		if (!party) { SetState("ERROR", "ZWNET_PARTY_FAILED"); return; }
		*party = PublishLocalRank(std::move(*party));
		UpdateLobbyDvars(*party);
		const auto partyId = JsonString(*party, "id");
		if (!partyId.empty())
		{
			Request("POST", "/zwnet/parties/" + partyId + "/set-map", {{"map", map}});
			Request("POST", "/zwnet/parties/" + partyId + "/set-mode", {{"mode", "zw3"}});
		}
		SetState("IN_PARTY");
	}

	void ZWNet::RefreshLobby()
	{
		auto party = Request("GET", "/zwnet/parties/current");
		if (party && !party->is_null() && !party->contains("error"))
		{
			*party = PublishLocalRank(std::move(*party));
			UpdateLobbyDvars(*party);
			const auto partyState = party->value("state", "IDLE");
			if (IsActiveMatchmakingState(partyState))
			{
				SearchingState() = true;
				SetState(partyState);
				UpdateMatchmaking();
			}
		}
	}

	void ZWNet::RefreshActiveParty()
	{
		if (!ActiveState() || SearchingState() || ClosingOnlineSessionState()) return;

		std::string partyId;
		{
			std::lock_guard lock(StateMutex());
			partyId = CurrentPartyIdState();
		}
		if (partyId.empty()) return;

		auto party = Request("GET", "/zwnet/parties/current");
		if (party && party->is_object() && !party->contains("error"))
		{
			*party = PublishLocalRank(std::move(*party));
			UpdateLobbyDvars(*party);
		}
	}

	void ZWNet::LeaveParty()
	{
		const auto result = Request("POST", "/zwnet/parties/leave");
		if (result && !result->contains("error"))
		{
			ResetMatchLobbySoundSnapshot();
			std::lock_guard lock(StateMutex());
			CurrentPartyIdState().clear(); CurrentProposalIdState().clear(); CurrentMatchIdState().clear();
		}
		SearchingState() = false;
		Scheduler::Once([]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_lobby_active").set(false);
			Dvar::Var("zwnet_vote_active").set(false);
			Dvar::Var("zwnet_all_ready").set(false);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdateMatchLobbyDvars(const nlohmann::json& status)
	{
		if (!status.contains("lobby") || !status.at("lobby").is_object()) return;
		const auto& lobby = status.at("lobby");
		const auto membersIt = lobby.find("members");
		if (membersIt == lobby.end() || !membersIt->is_array()) return;
		struct LobbyMemberSnapshot
		{
			std::string playerId;
			std::string displayName;
			std::string role;
			bool ready{};
			bool rankKnown{};
			int rankLevel{1};
			int rankPrestige{};
		};
		std::array<LobbyMemberSnapshot, 4> members{};
		std::unordered_set<std::string> memberIds;
		std::size_t memberCount{};
		for (const auto& member : *membersIt)
		{
			if (memberCount >= members.size()) break;
			if (!member.is_object()) continue;
			auto& snapshot = members[memberCount++];
			snapshot.playerId = JsonString(member, "player_id");
			if (!snapshot.playerId.empty()) memberIds.emplace(snapshot.playerId);
			snapshot.displayName = SafeDisplayName(JsonString(member, "display_name"));
			snapshot.role = JsonString(member, "role", "MEMBER");
			const auto rankIt = member.find("rank");
			if (rankIt != member.end() && rankIt->is_object())
			{
				snapshot.rankLevel = std::clamp(rankIt->value("level", 1), 1, 54);
				snapshot.rankPrestige = std::max(rankIt->value("prestige", 0), 0);
				snapshot.rankKnown = rankIt->contains("level");
			}
			else if (member.contains("level") && member.at("level").is_number_integer())
			{
				snapshot.rankLevel = std::clamp(member.at("level").get<int>(), 1, 54);
				snapshot.rankPrestige = member.contains("prestige") &&
					member.at("prestige").is_number_integer()
					? std::max(member.at("prestige").get<int>(), 0)
					: 0;
				snapshot.rankKnown = true;
			}
			else if (member.contains("rank_level"))
			{
				snapshot.rankLevel = std::clamp(member.value("rank_level", 1), 1, 54);
				snapshot.rankPrestige = std::max(member.value("rank_prestige", 0), 0);
				snapshot.rankKnown = true;
			}
			if (!snapshot.rankKnown)
			{
				snapshot.rankKnown = Friends::TryGetZombieRankByGuid(
					snapshot.playerId, snapshot.rankLevel, snapshot.rankPrestige);
			}
			const auto readyIt = member.find("ready");
			if (readyIt != member.end())
			{
				if (readyIt->is_boolean()) snapshot.ready = readyIt->get<bool>();
				else if (readyIt->is_number_integer()) snapshot.ready = readyIt->get<std::int64_t>() != 0;
			}
		}
		const auto soundDelta = ObserveMatchLobbyMembers(
			JsonString(status, "match_id"), memberIds);
		const auto lobbyState = JsonString(status, "state", "WAITING_FOR_READY");
		CachedPartyMemberCount() = static_cast<int>(memberCount);
		CachedPartyJoinStateSupported() = IsPartyJoinStateSupported(lobbyState);
		const auto stateText = FriendlyStateText(lobbyState);
		const auto sharedRanks = GetCachedSharedLobbyRanks();
		std::string currentPlayerId;
		{ std::lock_guard lock(StateMutex()); currentPlayerId = CurrentPlayerIdState(); }
		Scheduler::Once([members = std::move(members), memberCount, stateText, currentPlayerId, sharedRanks, soundDelta]
		{
			if (!ActiveState()) return;
			static constexpr std::array memberPrefixes
			{
				"zwnet_lobby_member_0",
				"zwnet_lobby_member_1",
				"zwnet_lobby_member_2",
				"zwnet_lobby_member_3",
			};
			Dvar::Var("zwnet_lobby_active").set(true);
			Dvar::Var("zwnet_lobby_member_count").set(static_cast<int>(memberCount));
			Dvar::Var("zwnet_lobby_status_text").set(stateText);
			bool allReady = memberCount > 0;
			bool selfReady = false;
			for (std::size_t i = 0; i < members.size(); ++i)
			{
				const auto prefix = memberPrefixes[i];
				if (i < memberCount)
				{
					const auto& member = members[i];
					Dvar::Var(std::string{prefix} + "_name").set(member.displayName);
					Dvar::Var(std::string{prefix} + "_guid").set(member.playerId);
					Dvar::Var(std::string{prefix} + "_role").set(member.role);
					Dvar::Var(std::string{prefix} + "_ready").set(member.ready);
					Dvar::Var(std::string{prefix} + "_self").set(member.playerId == currentPlayerId);
					const auto rankIt = sharedRanks.find(member.playerId);
					const auto sharedRankKnown = rankIt != sharedRanks.end();
					const auto rankKnown = member.rankKnown || sharedRankKnown;
					Dvar::Var(std::string{prefix} + "_shared_rank_known").set(rankKnown);
					Dvar::Var(std::string{prefix} + "_shared_rank_level").set(
						member.rankKnown ? member.rankLevel :
						sharedRankKnown ? rankIt->second.level : 1);
					Dvar::Var(std::string{prefix} + "_shared_rank_prestige").set(
						member.rankKnown ? member.rankPrestige :
						sharedRankKnown ? rankIt->second.prestige : 0);
					allReady = allReady && member.ready;
					if (member.playerId == currentPlayerId) selfReady = member.ready;
				}
				else
				{
					Dvar::Var(std::string{prefix} + "_name").set("");
					Dvar::Var(std::string{prefix} + "_guid").set("");
					Dvar::Var(std::string{prefix} + "_role").set("");
					Dvar::Var(std::string{prefix} + "_ready").set(false);
					Dvar::Var(std::string{prefix} + "_self").set(false);
					Dvar::Var(std::string{prefix} + "_shared_rank_known").set(false);
					Dvar::Var(std::string{prefix} + "_shared_rank_level").set(1);
					Dvar::Var(std::string{prefix} + "_shared_rank_prestige").set(0);
				}
			}
			Dvar::Var("zwnet_lobby_self_ready").set(selfReady);
			Dvar::Var("zwnet_all_ready").set(allReady);
			Dvar::Var("zwnet_lobby_can_start").set(false);
			// Matchmaking rosters are HTTP-backed, so reproduce the native private
			// lobby membership sounds from stable player-ID deltas.
			if (soundDelta.left) Command::Execute("snd_playLocal mp_player_leave", false);
			if (soundDelta.joined) Command::Execute("snd_playLocal mp_player_join", false);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdatePresence()
	{
		if (!ActiveState() || ClosingOnlineSessionState() || OnlineEntryPendingState()) return;
		std::string matchId;
		std::string partyId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
			partyId = CurrentPartyIdState();
		}
		const auto status = !matchId.empty() ? (InGameState() ? "IN_MATCH" : "CONNECTING") : SearchingState() ? "SEARCHING" : "MAIN_MENU";
		const auto joinable = IsOpaquePartyId(partyId) &&
			CachedPartyMemberCount().load() > 0 &&
			CachedPartyMemberCount().load() < 4 &&
			CachedPartyVisibility().load() != 2 &&
			CachedPartyJoinStateSupported().load();
		const auto result = Request("POST", "/social/presence",
			{{"status", status}, {"sequence", NextPresenceSequence()}, {"joinable", joinable}});
		if (!result || result->contains("error"))
		{
			Logger::Print("ZWNET presence update failed; matchmaking state preserved\n");
		}
	}

	void ZWNet::ToggleReady(const bool ready)
	{
		std::string partyId;
		{ std::lock_guard lock(StateMutex()); partyId = CurrentPartyIdState(); }
		if (partyId.empty())
		{
			Scheduler::Once([]
			{
				if (ActiveState()) Dvar::Var("zwnet_ready_pending").set(false);
			}, Scheduler::Pipeline::MAIN);
			return;
		}
		const auto result = Request("POST", "/zwnet/parties/" + partyId + (ready ? "/unready" : "/ready"));
		Scheduler::Once([]
		{
			if (ActiveState()) Dvar::Var("zwnet_ready_pending").set(false);
		}, Scheduler::Pipeline::MAIN);
		if (result && !result->contains("error"))
		{
			Scheduler::Once([ready]
			{
				if (!ActiveState()) return;
				Dvar::Var("zwnet_lobby_self_ready").set(!ready);
			}, Scheduler::Pipeline::MAIN);
			UpdateLobbyDvars(*result);
			if (SearchingState()) UpdateMatchmaking();
		}
	}

	void ZWNet::StartPrivateMatch(std::string map)
	{
		std::string partyId;
		{ std::lock_guard lock(StateMutex()); partyId = CurrentPartyIdState(); }
		if (partyId.empty()) return;
		Request("POST", "/zwnet/parties/" + partyId + "/set-map", {{"map", map}});
		Request("POST", "/zwnet/parties/" + partyId + "/set-mode", {{"mode", "zw3"}});
		const auto result = Request("POST", "/zwnet/parties/" + partyId + "/start-private-match");
		if (!result || result->contains("error")) { SetState("ERROR", "ZWNET_PRIVATE_MATCH_FAILED"); return; }
		SearchingState() = true;
		SetState(result->value("state", "RESERVING_SERVER"));
		UpdateMatchmaking();
	}

	void ZWNet::VoteMap(const std::string& choice)
	{
		std::string proposalId;
		{ std::lock_guard lock(StateMutex()); proposalId = CurrentProposalIdState(); }
		if (proposalId.empty()) return;
		const auto result = Request("POST", "/zwnet/matchmaking/map-vote", {{"proposal_id", proposalId}, {"choice", choice}});
		if (!result) { SetState("ERROR", "ZWNET_MAP_VOTE_FAILED"); return; }
		if (result->contains("error"))
		{
			if (ResponseErrorCode(*result) == "MAP_VOTE_CLOSED")
			{
				UpdateMatchmaking();
				return;
			}
			SetState("ERROR", "ZWNET_MAP_VOTE_FAILED");
			return;
		}
		if (result->value("closed", false))
		{
			UpdateMatchmaking();
			return;
		}
		Scheduler::Once([choice]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_vote_selection").set(choice);
		}, Scheduler::Pipeline::MAIN);
		const auto* vote = result->contains("map_vote") && result->at("map_vote").is_object()
			? &result->at("map_vote")
			: result->contains("choices") ? &*result : nullptr;
		if (vote)
		{
			std::string matchId;
			{ std::lock_guard lock(StateMutex()); matchId = CurrentMatchIdState(); }
			UpdateVoteDvars({{"match_id", matchId}, {"map_vote", *vote}});
		}
	}

	void ZWNet::CancelSearch()
	{
		Request("POST", "/zwnet/matchmaking/cancel");
		SearchingState() = false; SetState("IDLE");
	}

	void ZWNet::CloseOnlineSession(const bool shuttingDown, const bool terminal)
	{
		if (!ActiveState() || ClosingOnlineSessionState().exchange(true)) return;
		const auto closingGuard = gsl::finally([] { ClosingOnlineSessionState() = false; });
		ResetMatchLobbySoundSnapshot();
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		InGameState() = false;
		std::string matchId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
		}
		auto body = nlohmann::json::object();
		if (!matchId.empty()) body["match_id"] = matchId;
		if (terminal) body["terminal"] = true;
		Request("POST", "/zwnet/matchmaking/disconnect", body);
		Request("POST", "/social/presence", {{"status", "OFFLINE"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
		SearchingState() = false;
		{
			std::lock_guard lock(StateMutex());
			if (terminal) CurrentPartyIdState().clear();
			CurrentProposalIdState().clear();
			CurrentMatchIdState().clear();
		}
		if (shuttingDown) return;
		SetState("IDLE");
		Scheduler::Once([terminal]
		{
			if (!ActiveState()) return;
			if (terminal)
			{
				AbandonOnlineSession();
				return;
			}
			Dvar::Var("zwnet_vote_active").set(false);
			Dvar::Var("zwnet_vote_selection").set("");
			Dvar::Var("zwnet_all_ready").set(false);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
			Dvar::Var("zwnet_vote_winner_id").set("");
			Dvar::Var("zwnet_vote_winner_name").set("");
			Dvar::Var("zwnet_vote_winner_image").set("");
			Dvar::Var("zwnet_server_endpoint").set("");
			Dvar::Var("zwnet_server_hostname").set("");
			Dvar::Var("zwnet_server_status").set("NOT ASSIGNED");
			Dvar::Var("zwnet_join_status").set("WAITING IN LOBBY");
		}, Scheduler::Pipeline::MAIN);
	}

	bool ZWNet::ReturnToMatchmakingLobby()
	{
		auto completed = false;
		for (auto attempt = 0; attempt < 7; ++attempt)
		{
			const auto status = Request("GET", "/zwnet/matchmaking/status");
			if (!status || !status->is_object() || status->contains("error")) return false;
			const auto state = JsonString(*status, "state");
			completed = state == "RESETTING" || state == "POST_MATCH" ||
				state == "FINISHED" || state == "IDLE";
			if (completed) break;
			if (attempt < 6) std::this_thread::sleep_for(1s);
		}
		if (!completed) return false;

		auto party = Request("GET", "/zwnet/parties/current");
		if (!party || !party->is_object() || party->is_null() || party->contains("error") ||
			JsonString(*party, "id").empty()) return false;
		*party = PublishLocalRank(std::move(*party));

		SearchingState() = false;
		InGameState() = false;
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		ResetMatchLobbySoundSnapshot();
		{
			std::lock_guard lock(StateMutex());
			CurrentProposalIdState().clear();
			CurrentMatchIdState().clear();
		}
		UpdateLobbyDvars(*party);
		SetState("IN_PARTY");
		UpdatePresence();
		Scheduler::Once([]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_vote_active").set(false);
			Dvar::Var("zwnet_vote_selection").set("");
			Dvar::Var("zwnet_all_ready").set(false);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
			Dvar::Var("zwnet_vote_winner_id").set("");
			Dvar::Var("zwnet_vote_winner_name").set("");
			Dvar::Var("zwnet_vote_winner_image").set("");
			Dvar::Var("zwnet_match_id").set("");
			Dvar::Var("zwnet_server_endpoint").set("");
			Dvar::Var("zwnet_server_hostname").set("");
			Dvar::Var("zwnet_server_status").set("NOT ASSIGNED");
			Dvar::Var("zwnet_join_status").set("WAITING IN LOBBY");
			Command::Execute("openmenu zwnet_matchmaking", false);
		}, Scheduler::Pipeline::MAIN, 500ms);
		return true;
	}

	void ZWNet::HandleServerDisconnect(const bool terminal)
	{
		if (!terminal && ReturnToMatchmakingLobby())
		{
			Logger::Print("ZWNET completed match: returned to the preserved party lobby\n");
			return;
		}
		CloseOnlineSession(false, terminal);
	}

	void ZWNet::ConnectMatch(const std::string& matchId, const bool relay)
	{
		SetState(relay ? "RELAY_CONNECTION" : "DIRECT_CONNECTION");
		const auto descriptor = Request("GET", "/zwnet/connect/" + matchId + (relay ? "?relay=1" : ""));
		if (!descriptor)
		{
			SearchingState() = false;
			SetState("ERROR", "ZWNET_REQUEST_FAILED");
			return;
		}
		if (descriptor->contains("error"))
		{
			const auto code = ResponseErrorCode(*descriptor);
			SearchingState() = false;
			Scheduler::Once([]
			{
				if (!ActiveState()) return;
				Dvar::Var("zwnet_server_status").set("SERVER UNAVAILABLE");
				Dvar::Var("zwnet_join_status").set("RETURN TO LOBBY");
			}, Scheduler::Pipeline::MAIN);
			SetState("ERROR", code == "SERVER_NOT_READY" ? "ZWNET_SERVER_NOT_READY" : "ZWNET_ROUTE_UNAVAILABLE");
			return;
		}
		const auto key = relay ? "relay_endpoint" : "direct_endpoint";
		const auto endpoint = JsonString(*descriptor, key);
		if (endpoint.empty()) { SearchingState() = false; SetState("ERROR", "ZWNET_DESCRIPTOR_INVALID"); return; }
		// Never place the returned short-lived connect ticket in a command line or URL.
		SearchingState() = false;
		Scheduler::Once([endpoint]
		{
			if (!ActiveState()) return;
			const Network::Address target(endpoint);
			if (!target.isValid())
			{
				Dvar::Var("ui_zwnet_state").set("ERROR");
				Dvar::Var("ui_zwnet_state_text").set("ERROR");
				Dvar::Var("ui_zwnet_error").set("ZWNET_DESCRIPTOR_INVALID");
				Dvar::Var("ui_zwnet_error_text").set(FriendlyErrorText("ZWNET_DESCRIPTOR_INVALID"));
				Dvar::Var("zwnet_join_status").set("SERVER ADDRESS INVALID");
				return;
			}
			Dvar::Var("ui_zwnet_state").set("CONNECTING");
			Dvar::Var("ui_zwnet_error").set("");
			Dvar::Var("zwnet_server_endpoint").set(endpoint);
			Dvar::Var("zwnet_server_status").set("SERVER ASSIGNED");
			Dvar::Var("zwnet_join_status").set("JOINING SERVER");
			ServerJoinTransitionState() = true;
			Scheduler::Once([]
			{
				ServerJoinTransitionState() = false;
			}, Scheduler::Pipeline::MAIN, 12s);
			Party::Connect(target);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdateVoteDvars(const nlohmann::json& status)
	{
		if (!status.contains("map_vote")) return;
		const auto vote = status.at("map_vote");
		const auto choices = vote.value("choices", nlohmann::json::array());
		if (choices.size() != 3) return;
		const auto allReady = status.value("all_ready", false);
		{
			std::lock_guard lock(StateMutex());
			CurrentProposalIdState() = JsonString(vote, "proposal_id");
			CurrentMatchIdState() = JsonString(status, "match_id");
		}
		Scheduler::Once([vote, choices, allReady]
		{
			if (!ActiveState()) return;
			const auto selected = vote.contains("selected") && vote.at("selected").is_string() ? vote.at("selected").get<std::string>() : "";
			Dvar::Var("zwnet_vote_active").set(true);
			Dvar::Var("zwnet_all_ready").set(allReady);
			Dvar::Var("zwnet_start_phase").set("");
			Dvar::Var("zwnet_start_seconds").set(0);
			Dvar::Var("zwnet_vote_proposal_id").set(JsonString(vote, "proposal_id"));
			Dvar::Var("zwnet_vote_seconds").set(vote.value("seconds_remaining", 0));
			Dvar::Var("zwnet_vote_selection").set(selected);
			Dvar::Var("zwnet_vote_winner_id").set("");
			Dvar::Var("zwnet_vote_winner_name").set("");
			Dvar::Var("zwnet_vote_winner_image").set("");
			Dvar::Var("zwnet_server_status").set("WAITING FOR MAP VOTE");
			Dvar::Var("zwnet_join_status").set("VOTE IN PROGRESS");
			for (std::size_t i = 0; i < 2; ++i)
			{
				const auto prefix = std::format("zwnet_vote_map_{}", i == 0 ? "a" : "b");
				Dvar::Var(prefix + "_id").set(JsonString(choices[i], "id"));
				Dvar::Var(prefix + "_name").set(JsonString(choices[i], "name"));
				Dvar::Var(prefix + "_image").set(JsonString(choices[i], "image"));
				Dvar::Var(prefix + "_votes").set(choices[i].value("votes", 0));
			}
			Dvar::Var("zwnet_vote_random_votes").set(choices[2].value("votes", 0));
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdateMatchmaking()
	{
		if (!ActiveState() || !SearchingState()) return;
		auto party = Request("GET", "/zwnet/parties/current");
		if (party && party->is_object() && !party->contains("error"))
		{
			*party = PublishLocalRank(std::move(*party));
		}
		const auto status = Request("GET", "/zwnet/matchmaking/status");
		if (!status)
		{
			if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
			Logger::Print("ZWNET matchmaking status poll failed; current session preserved\n");
			return;
		}
		if (status->contains("error"))
		{
			if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
			return;
		}
		if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
		if (status->contains("lobby") && status->at("lobby").is_object()) UpdateMatchLobbyDvars(*status);
		const auto state = JsonString(*status, "state", "SEARCHING");
		const auto joinCountdown = status->value("join_countdown_seconds", 0);
		const auto allReady = status->value("all_ready", false);
		const auto startPhase = JsonString(*status, "start_phase");
		const auto startSeconds = status->value("start_seconds", joinCountdown);
		if (state == "ERROR" || state == "FAILED")
		{
			SearchingState() = false;
			SetState("ERROR", "ZWNET_MATCH_FAILED");
			return;
		}
		if (state == "IDLE" || state == "FINISHED" || state == "FAILED") SearchingState() = false;
		SetState(state == "CONNECTING" && joinCountdown > 0 ? "COUNTDOWN" : state);
		if (state == "MAP_VOTE") UpdateVoteDvars(*status);
		else
		{
			const auto map = JsonString(*status, "map");
			const auto matchId = JsonString(*status, "match_id");
			if (!matchId.empty())
			{
				std::lock_guard lock(StateMutex());
				CurrentMatchIdState() = matchId;
			}
			auto mapName = std::string{};
			auto mapImage = std::string{};
			if (status->contains("selected_map") && status->at("selected_map").is_object())
			{
				mapName = JsonString(status->at("selected_map"), "name");
				mapImage = JsonString(status->at("selected_map"), "image");
			}
			auto serverStatus = std::string{"NOT ASSIGNED"};
			auto joinStatus = std::string{"WAITING IN LOBBY"};
			if (state == "WAITING_FOR_READY") { serverStatus = "START LOCKED"; joinStatus = "WAITING FOR ALL PLAYERS"; }
			else if (state == "RESERVING_SERVER") { serverStatus = "ALLOCATING SERVER"; joinStatus = "MAP LOCKED"; }
			else if (state == "SERVER_STARTING") { serverStatus = "SERVER STARTING"; joinStatus = "WAITING FOR SERVER"; }
			else if (state == "RESETTING" || state == "POST_MATCH") { serverStatus = "MATCH COMPLETE"; joinStatus = "RETURNING TO LOBBY"; }
			else if (state == "CONNECTING")
			{
				serverStatus = "SERVER READY";
				joinStatus = joinCountdown > 0 ? std::format("JOINING IN {}", joinCountdown) : "JOIN AUTHORIZED";
			}
			Scheduler::Once([map, mapName, mapImage, matchId, serverStatus, joinStatus, joinCountdown, allReady, startPhase, startSeconds]
			{
				if (!ActiveState()) return;
				Dvar::Var("zwnet_vote_active").set(false);
				Dvar::Var("zwnet_all_ready").set(allReady);
				Dvar::Var("zwnet_start_phase").set(startPhase);
				Dvar::Var("zwnet_start_seconds").set(startSeconds);
				Dvar::Var("zwnet_match_id").set(matchId);
				Dvar::Var("zwnet_join_countdown").set(joinCountdown);
				Dvar::Var("zwnet_server_status").set(serverStatus);
				Dvar::Var("zwnet_join_status").set(joinStatus);
				if (!map.empty())
				{
					Dvar::Var("ui_mapname").set(map);
					Dvar::Var("zwnet_vote_winner_id").set(map);
					Dvar::Var("zwnet_vote_winner_name").set(mapName);
					Dvar::Var("zwnet_vote_winner_image").set(mapImage);
				}
			}, Scheduler::Pipeline::MAIN);
		}
		if (status->contains("match_id") && state == "CONNECTING" && joinCountdown <= 0 &&
			!InGameState() && !ServerJoinTransitionState() && !EndpointJoinInFlight())
		{
			ConnectMatch(status->at("match_id").get<std::string>(), false);
		}
	}

	void ZWNet::InitializeDvars()
	{
		Dvar::Register<const char*>("ui_zwnet_state", "OFFLINE", Game::DVAR_NONE, "Localized ZWNET state key");
		Dvar::Register<const char*>("ui_zwnet_state_text", "OFFLINE", Game::DVAR_NONE, "Readable ZWNET state text");
		Dvar::Register<const char*>("ui_zwnet_error", "", Game::DVAR_NONE, "Stable ZWNET error key");
		Dvar::Register<const char*>("ui_zwnet_error_text", "", Game::DVAR_NONE, "Readable ZWNET error text");
		Dvar::Register<const char*>("ui_zwnet_guid", PublicGuidText(), Game::DVAR_ROM, "Public ZW3 GUID used for Stats account linking");
		Dvar::Register<bool>("zwnet_lobby_active", false, Game::DVAR_NONE, "ZWNET party lobby is active");
		Dvar::Register<const char*>("zwnet_lobby_party_id", "", Game::DVAR_NONE, "Current ZWNET party");
		Dvar::Register<const char*>("zwnet_lobby_visibility", "OPEN", Game::DVAR_NONE, "Current ZWNET party visibility");
		Dvar::Register<const char*>("zwnet_lobby_owner", "", Game::DVAR_NONE, "Current party owner");
		Dvar::Register<int>("zwnet_lobby_member_count", 0, 0, 4, Game::DVAR_NONE, "Current party size");
		Dvar::Register<const char*>("zwnet_lobby_status_text", "IDLE", Game::DVAR_NONE, "Current party state");
		Dvar::Register<bool>("zwnet_lobby_can_start", false, Game::DVAR_NONE, "Private match can start");
		Dvar::Register<bool>("zwnet_lobby_self_ready", false, Game::DVAR_NONE, "Local party ready state");
		Dvar::Register<bool>("zwnet_ready_pending", false, Game::DVAR_NONE, "A ready-state update is in flight");
		Dvar::Register<bool>("zwnet_all_ready", false, Game::DVAR_NONE, "All active match players are ready");
		Dvar::Register<const char*>("zwnet_start_phase", "", Game::DVAR_NONE, "Server start phase");
		Dvar::Register<int>("zwnet_start_seconds", 0, 0, 300, Game::DVAR_NONE, "Server start phase time remaining");
		constexpr std::array memberNames
		{
			"zwnet_lobby_member_0_name", "zwnet_lobby_member_1_name", "zwnet_lobby_member_2_name", "zwnet_lobby_member_3_name"
		};
		constexpr std::array memberRoles
		{
			"zwnet_lobby_member_0_role", "zwnet_lobby_member_1_role", "zwnet_lobby_member_2_role", "zwnet_lobby_member_3_role"
		};
		constexpr std::array memberGuids
		{
			"zwnet_lobby_member_0_guid", "zwnet_lobby_member_1_guid", "zwnet_lobby_member_2_guid", "zwnet_lobby_member_3_guid"
		};
		constexpr std::array memberReady
		{
			"zwnet_lobby_member_0_ready", "zwnet_lobby_member_1_ready", "zwnet_lobby_member_2_ready", "zwnet_lobby_member_3_ready"
		};
		constexpr std::array memberSelf
		{
			"zwnet_lobby_member_0_self", "zwnet_lobby_member_1_self", "zwnet_lobby_member_2_self", "zwnet_lobby_member_3_self"
		};
		constexpr std::array memberSharedRankKnown
		{
			"zwnet_lobby_member_0_shared_rank_known", "zwnet_lobby_member_1_shared_rank_known", "zwnet_lobby_member_2_shared_rank_known", "zwnet_lobby_member_3_shared_rank_known"
		};
		constexpr std::array memberSharedRankLevels
		{
			"zwnet_lobby_member_0_shared_rank_level", "zwnet_lobby_member_1_shared_rank_level", "zwnet_lobby_member_2_shared_rank_level", "zwnet_lobby_member_3_shared_rank_level"
		};
		constexpr std::array memberSharedRankPrestiges
		{
			"zwnet_lobby_member_0_shared_rank_prestige", "zwnet_lobby_member_1_shared_rank_prestige", "zwnet_lobby_member_2_shared_rank_prestige", "zwnet_lobby_member_3_shared_rank_prestige"
		};
		constexpr std::array memberRankIcons
		{
			"zwnet_lobby_member_0_rank_icon", "zwnet_lobby_member_1_rank_icon", "zwnet_lobby_member_2_rank_icon", "zwnet_lobby_member_3_rank_icon"
		};
		constexpr std::array memberRankLevels
		{
			"zwnet_lobby_member_0_rank_level", "zwnet_lobby_member_1_rank_level", "zwnet_lobby_member_2_rank_level", "zwnet_lobby_member_3_rank_level"
		};
		for (std::size_t i = 0; i < memberNames.size(); ++i)
		{
			Dvar::Register<const char*>(memberNames[i], "", Game::DVAR_NONE, "Party member name");
			Dvar::Register<const char*>(memberGuids[i], "", Game::DVAR_NONE, "Public-lobby member GUID");
			Dvar::Register<const char*>(memberRoles[i], "", Game::DVAR_NONE, "Party member role");
			Dvar::Register<bool>(memberReady[i], false, Game::DVAR_NONE, "Party member ready state");
			Dvar::Register<bool>(memberSelf[i], false, Game::DVAR_NONE, "Local public-lobby member slot");
			Dvar::Register<bool>(memberSharedRankKnown[i], false, Game::DVAR_NONE, "Public-lobby member shared rank is available");
			Dvar::Register<int>(memberSharedRankLevels[i], 1, 1, 54, Game::DVAR_NONE, "Public-lobby member shared rank level");
			Dvar::Register<int>(memberSharedRankPrestiges[i], 0, 0, 255, Game::DVAR_NONE, "Public-lobby member shared rank prestige");
			Dvar::Register<const char*>(memberRankIcons[i], "", Game::DVAR_NONE, "Public lobby member rank icon");
			Dvar::Register<const char*>(memberRankLevels[i], "", Game::DVAR_NONE, "Public lobby member rank level");
		}
		Dvar::Register<bool>("zwnet_vote_active", false, Game::DVAR_NONE, "Map vote is active");
		Dvar::Register<const char*>("zwnet_vote_proposal_id", "", Game::DVAR_NONE, "Current map vote");
		Dvar::Register<int>("zwnet_vote_seconds", 0, 0, 60, Game::DVAR_NONE, "Map vote time remaining");
		Dvar::Register<const char*>("zwnet_vote_selection", "", Game::DVAR_NONE, "Local map vote selection");
		constexpr std::array voteIds{"zwnet_vote_map_a_id", "zwnet_vote_map_b_id"};
		constexpr std::array voteNames{"zwnet_vote_map_a_name", "zwnet_vote_map_b_name"};
		constexpr std::array voteImages{"zwnet_vote_map_a_image", "zwnet_vote_map_b_image"};
		constexpr std::array voteCounts{"zwnet_vote_map_a_votes", "zwnet_vote_map_b_votes"};
		for (std::size_t i = 0; i < voteIds.size(); ++i)
		{
			Dvar::Register<const char*>(voteIds[i], "", Game::DVAR_NONE, "Map vote internal id");
			Dvar::Register<const char*>(voteNames[i], "", Game::DVAR_NONE, "Map vote display name");
			Dvar::Register<const char*>(voteImages[i], "", Game::DVAR_NONE, "Map vote preview material");
			Dvar::Register<int>(voteCounts[i], 0, 0, 4, Game::DVAR_NONE, "Map vote count");
		}
		Dvar::Register<int>("zwnet_vote_random_votes", 0, 0, 4, Game::DVAR_NONE, "Random map vote count");
		Dvar::Register<const char*>("zwnet_vote_winner_id", "", Game::DVAR_NONE, "Winning ZW3 map id");
		Dvar::Register<const char*>("zwnet_vote_winner_name", "", Game::DVAR_NONE, "Winning ZW3 map name");
		Dvar::Register<const char*>("zwnet_vote_winner_image", "", Game::DVAR_NONE, "Winning ZW3 map preview");
		Dvar::Register<const char*>("zwnet_match_id", "", Game::DVAR_NONE, "Current ZW3 match id");
		Dvar::Register<const char*>("zwnet_server_endpoint", "", Game::DVAR_NONE, "Assigned public ZW3 server endpoint");
		Dvar::Register<const char*>("zwnet_server_hostname", "", Game::DVAR_NONE, "Connected ZW3 server hostname");
		Dvar::Register<const char*>("zwnet_server_status", "NOT ASSIGNED", Game::DVAR_NONE, "ZW3 server assignment status");
		Dvar::Register<const char*>("zwnet_join_status", "WAITING IN LOBBY", Game::DVAR_NONE, "ZW3 join status");
		Dvar::Register<int>("zwnet_join_countdown", 0, 0, 30, Game::DVAR_NONE, "Synchronized ZW3 join countdown");
		Dvar::Register<const char*>("zwnet_selected_player_guid", "", Game::DVAR_NONE, "Selected public-lobby player GUID");
		Dvar::Register<const char*>("zwnet_selected_player_name", "", Game::DVAR_NONE, "Selected public-lobby player name");
		Dvar::Register<const char*>("zwnet_selected_player_role", "", Game::DVAR_NONE, "Selected public-lobby player role");
		Dvar::Register<int>("zwnet_selected_player_rank", 1, 1, 54, Game::DVAR_NONE, "Selected public-lobby player rank");
		Dvar::Register<int>("zwnet_selected_player_prestige", 0, 0, 255, Game::DVAR_NONE, "Selected public-lobby player prestige");
		Dvar::Register<const char*>("zwnet_selected_player_rank_icon", "prestige_1", Game::DVAR_NONE, "Selected public-lobby player prestige icon");
		Dvar::Register<bool>("zwnet_selected_player_self", false, Game::DVAR_NONE, "Selected public-lobby player is local");
		Dvar::Register<const char*>("zwnet_selected_player_relationship", "UNAVAILABLE", Game::DVAR_NONE, "Selected public-lobby player friend state");
		Dvar::Register<bool>("zwnet_barracks_compare_active", false, Game::DVAR_NONE, "Barracks was opened for a lobby comparison");
		Dvar::Register<bool>("zw3_barracks_rank_known", false, Game::DVAR_NONE, "Local ZW3 Barracks rank data is available");
		Dvar::Register<int>("zw3_barracks_rank_level", 1, 1, 54, Game::DVAR_NONE, "Local ZW3 Barracks rank level");
		Dvar::Register<int>("zw3_barracks_rank_prestige", 0, 0, 255, Game::DVAR_NONE, "Local ZW3 Barracks prestige");
		Dvar::Register<int>("zw3_barracks_rank_experience", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Local ZW3 Barracks experience");
		Dvar::Register<int>("zw3_barracks_rank_experience_target", 50, 1, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 Barracks level XP target");
		Dvar::Register<int>("zw3_barracks_rank_experience_percent", 0, 0, 100, Game::DVAR_NONE, "Current ZW3 Barracks level XP completion percent");
		Dvar::Register<const char*>("zw3_barracks_rank_icon", "prestige_1", Game::DVAR_NONE, "Local ZW3 Barracks prestige icon");
		Dvar::Register<int>("zw3_barracks_zombie_kills", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 zombie kills");
		Dvar::Register<int>("zw3_barracks_zombie_deaths", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 zombie deaths");
		Dvar::Register<int>("zw3_barracks_zombie_revives", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Lifetime ZW3 teammate revives");
		for (std::size_t slot = 0; slot < ChallengeSlotProgressDvars.size(); ++slot)
		{
			Dvar::Register<int>(ChallengeSlotProgressDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 challenge progress");
			Dvar::Register<int>(ChallengeSlotTargetDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Current ZW3 challenge target");
			Dvar::Register<int>(ChallengeSlotTierDvars[slot], 0, 0, 4, Game::DVAR_NONE, "Completed ZW3 challenge tiers");
			Dvar::Register<int>(ChallengeSlotTierCountDvars[slot], 0, 0, 4, Game::DVAR_NONE, "Available ZW3 challenge tiers");
			Dvar::Register<int>(ChallengeSlotRewardDvars[slot], 0, 0, std::numeric_limits<int>::max(), Game::DVAR_NONE, "Next ZW3 challenge XP reward");
			Dvar::Register<int>(ChallengeSlotPercentDvars[slot], 0, 0, 100, Game::DVAR_NONE, "Current ZW3 challenge completion percent");
			Dvar::Register<bool>(ChallengeSlotCompleteDvars[slot], false, Game::DVAR_NONE, "ZW3 challenge is complete");
		}
		constexpr std::array questChallengeDvars
		{
			"zw3_quest_mp_factory_sh", "zw3_quest_mp_asylum_sh", "zw3_quest_mp_prototype_sh",
			"zw3_quest_mp_sumpf_sh", "zw3_quest_mp_za_island", "zw3_quest_mp_deathmarch_chap4_a",
			"zw3_quest_mp_deathmarch_chap4_b", "zw3_quest_mp_surv_town", "zw3_quest_mp_burg",
			"zw3_quest_mp_lambeth"
		};
		for (const auto* challengeDvar : questChallengeDvars)
		{
			Dvar::Register<int>(challengeDvar, 0, 0, 10, Game::DVAR_ARCHIVE,
				"Completed tiers for a repeatable ZW3 questline");
		}
		{
			std::lock_guard lock(StateMutex());
			// Reading an engine-owned string Dvar here also establishes that the
			// Dvar critical sections are usable before the async login is released.
			const auto localPlayerName = Dvar::Var("name").get<std::string>();
			(void)localPlayerName;
			// Construct all function-local state on the main game thread before the
			// asynchronous worker can access the CRT-backed string objects.
			(void)AccessTokenState();
			(void)RefreshTokenState();
			(void)CurrentPartyIdState();
			(void)CurrentPlayerIdState();
			(void)CurrentProposalIdState();
			(void)CurrentMatchIdState();
		}
		ActiveState() = true;
		RefreshBarracksProfile();
	}

	ZWNet::ZWNet()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;
		PatchMaterialEnumerationScratch();
		Localization::Set("ZWNET_LOGIN_REQUIRED", "Sign in on the ZW3 Stats page.");
		Localization::Set("ZWNET_SESSION_EXPIRED", "Your ZW3 session expired. Please sign in again.");
		Localization::Set("ZWNET_SEARCH_FAILED", "Matchmaking could not be started.");
		Localization::Set("ZWNET_ROUTE_UNAVAILABLE", "No direct or relay route is available.");
		Localization::Set("ZWNET_SERVER_NOT_READY", "The assigned ZW3 server is no longer available.");
		Localization::Set("ZWNET_DESCRIPTOR_INVALID", "The connection response was invalid.");
		Localization::Set("ZWNET_ACCOUNT_LINK_REQUIRED", "Link this GUID in ZW3 Stats Settings.");
		Localization::Set("ZWNET_SESSION_STORAGE_FAILED", "The secure ZW3 session could not be stored.");
		Localization::Set("ZWNET_REGISTRATION_UNAVAILABLE", "The ZW3 registration page is unavailable.");
		Localization::Set("ZWNET_PARTY_FAILED", "The party could not be created or loaded.");
		Localization::Set("ZWNET_PRIVATE_MATCH_FAILED", "The private match server could not be reserved.");
		Localization::Set("ZWNET_MAP_VOTE_FAILED", "Your map vote could not be submitted.");
		Localization::Set("ZWNET_REQUEST_FAILED", "The ZW3 online service did not respond safely.");
		Localization::Set("ZWNET_GUID_COPY_FAILED", "The ZW3 GUID could not be copied to the clipboard.");
		Command::Add("zwnet_login", [] { Login(); });
		Command::Add("zwnet_register", [] { EnqueueAsync([] { Register(); }); });
		Command::Add("zwnet_quickplay", []
		{
			CapturePartyPrivacy();
			EnqueueAsync([] { StartQuickPlay(); });
		});
		Command::Add("zwnet_cancel", [] { EnqueueAsync([] { CancelSearch(); }); });
		Command::Add("zwnet_terminal_disconnect", []
		{
			TerminalDisconnectRequested() = true;
			Scheduler::Once([] { TerminalDisconnectRequested() = false; },
				Scheduler::Pipeline::MAIN, 5s);
		});
		Command::Add("zwnet_logout", []
		{
			EnqueueAsync([]
			{
				CloseOnlineSession(false, true);
				Request("POST", "/social/client/logout");
				if (!ActiveState()) return;
				ClearSession();
				SetState("OFFLINE");
			});
		});
		UIScript::Add("ZWNetQuickPlay", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_quickplay", false); });
		UIScript::Add("ZWNetCancel", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_cancel", false); });
		UIScript::Add("ZWNET_CloseOnlineSession", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { CloseOnlineSession(false, true); }); });
		UIScript::Add("ZWNetLogin", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_login", false); });
		UIScript::Add("ZWNET_ConnectOnline", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { BeginOnlineEntry(); });
		UIScript::Add("ZWNET_CancelOnlineEntry", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { OnlineEntryPendingState() = false; });
		UIScript::Add("ZWNET_AbandonOnlineSession", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { AbandonOnlineSession(); });
		UIScript::Add("ZWNetRegister", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_register", false); });
		UIScript::Add("ZWNetCopyGuid", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			if (!CopyPublicGuidToClipboard()) SetState("ERROR", "ZWNET_GUID_COPY_FAILED");
		});
		UIScript::Add("ZWNET_EnterPrivateLobby", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			CapturePartyPrivacy();
			const auto map = Dvar::Var("ui_mapname").get<std::string>();
			EnqueueAsync([map] { EnterLobby(map); });
		});
		UIScript::Add("ZWNET_RefreshLobby", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { RefreshLobby(); }); });
		UIScript::Add("ZWNET_LeaveParty", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { LeaveParty(); }); });
		UIScript::Add("ZWNET_ToggleReady", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			if (Dvar::Var("zwnet_ready_pending").get<bool>()) return;
			const auto ready = Dvar::Var("zwnet_lobby_self_ready").get<bool>();
			Dvar::Var("zwnet_ready_pending").set(true);
			EnqueueAsync([ready] { ToggleReady(ready); });
		});
		UIScript::Add("ZWNET_SelectLobbyPlayer", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			const auto index = token.get<int>();
			if (index < 0 || index >= 4) return;
			const auto prefix = std::format("zwnet_lobby_member_{}", index);
			auto guid = Dvar::Var(prefix + "_guid").get<std::string>();
			const auto name = Dvar::Var(prefix + "_name").get<std::string>();
			if (guid.empty() || name.empty()) return;
			std::ranges::transform(guid, guid.begin(), [](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			Dvar::Var("zwnet_selected_player_guid").set(guid);
			Dvar::Var("zwnet_selected_player_name").set(name);
			Dvar::Var("zwnet_selected_player_role").set(Dvar::Var(prefix + "_role").get<std::string>());
			Dvar::Var("zwnet_selected_player_rank").set(Dvar::Var(prefix + "_shared_rank_level").get<int>());
			Dvar::Var("zwnet_selected_player_prestige").set(Dvar::Var(prefix + "_shared_rank_prestige").get<int>());
			Dvar::Var("zwnet_selected_player_rank_icon").set(Dvar::Var(prefix + "_rank_icon").get<std::string>());
			Dvar::Var("zwnet_selected_player_self").set(Dvar::Var(prefix + "_self").get<bool>());
			Dvar::Var("zwnet_selected_player_relationship").set(Friends::GetLobbyPlayerRelationship(guid));
		});
		UIScript::Add("ZWNET_RefreshBarracksProfile", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			RefreshBarracksProfile();
		});
		UIScript::Add("ZWNET_RefreshChallengeCategory", [](const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s*)
		{
			RefreshChallengeCategory(token.get<int>());
		});
		UIScript::Add("ZWNET_StartPrivateMatch", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			const auto map = Dvar::Var("ui_mapname").get<std::string>();
			EnqueueAsync([map] { StartPrivateMatch(map); });
		});
		UIScript::Add("ZWNET_VoteMapA", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { VoteMap("A"); }); });
		UIScript::Add("ZWNET_VoteMapB", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { VoteMap("B"); }); });
		UIScript::Add("ZWNET_VoteRandom", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { VoteMap("RANDOM"); }); });
		Events::OnCLDisconnected([](const bool wasConnected)
		{
			InGameState() = false;
			// CL_ConnectFromParty performs an internal CL_Disconnect while moving
			// from the ZWNET lobby into the assigned dedicated server. That planned
			// transition must not revoke the match and reset the server underneath
			// the in-flight connection.
			if (ServerJoinTransitionState().exchange(false))
			{
				Logger::Print("ZWNET server join transition: preserving online session\n");
				return;
			}
			bool hasMatch = false;
			{
				std::lock_guard lock(StateMutex());
				hasMatch = !CurrentMatchIdState().empty();
			}
			const auto terminal = TerminalDisconnectRequested().exchange(false);
			if (wasConnected || hasMatch) EnqueueAsync([terminal] { HandleServerDisconnect(terminal); });
		});
		Events::OnCGameInit([]
		{
			EndpointJoinInFlight() = false;
			ServerJoinTransitionState() = false;
			bool hasMatch = false;
			{
				std::lock_guard lock(StateMutex());
				hasMatch = !CurrentMatchIdState().empty();
			}
			InGameState() = hasMatch;
			if (hasMatch)
			{
				Dvar::Var("zwnet_server_hostname").set(Party::GetHostName());
				SetState("IN_MATCH");
				EnqueueAsync([] { UpdatePresence(); });
			}
		});
		Scheduler::OnGameInitialized([]
		{
			Logger::Print("ZWNET initialization: registering dvars\n");
			InitializeDvars();
		}, Scheduler::Pipeline::MAIN);
		Scheduler::OnGameInitialized([]
		{
			if (!ActiveState()) return;
			Logger::Print("ZWNET initialization: checking saved session\n");
			const auto hasSession = LoadSession();
			Logger::Print("ZWNET initialization: starting {}\n", hasSession ? "refresh" : "login");
			if (hasSession) EnqueueAsync([] { Refresh(); });
			else Login();
		}, Scheduler::Pipeline::MAIN, 2s);
		Scheduler::Loop(ProcessAsyncTasks, Scheduler::Pipeline::ASYNC, 50ms);
		// The backend owns all server-start and join deadlines. Poll at the same
		// one-second resolution displayed by the lobby instead of free-running a
		// second client countdown that can reach zero before connection begins.
		Scheduler::Loop(UpdateMatchmaking, Scheduler::Pipeline::ASYNC, 1s);
		Scheduler::Loop(RefreshActiveParty, Scheduler::Pipeline::ASYNC, 3s);
		Scheduler::Loop([]
		{
			if (!ActiveState()) return;
			if (Dvar::Var("zwnet_vote_active").get<bool>())
			{
				const auto seconds = Dvar::Var("zwnet_vote_seconds").get<int>();
				if (seconds > 0) Dvar::Var("zwnet_vote_seconds").set(seconds - 1);
			}
		}, Scheduler::Pipeline::MAIN, 1s);
		Scheduler::Loop(UpdatePresence, Scheduler::Pipeline::ASYNC, 30s);
		Scheduler::Loop(CapturePartyPrivacy, Scheduler::Pipeline::MAIN, 1s);
	}

	void ZWNet::preDestroy()
	{
		if (ActiveState()) CloseOnlineSession(true, true);
		ActiveState() = false;
		SearchingState() = false;
		ResetMatchLobbySoundSnapshot();
		EndpointJoinInFlight() = false;
		ServerJoinTransitionState() = false;
		OnlineEntryPendingState() = false;
		InGameState() = false;
		{
			std::lock_guard lock(StateMutex());
			LoginInFlightState() = false;
		}
		std::lock_guard lock(AsyncTaskMutex());
		AsyncTasks().clear();
	}
}
