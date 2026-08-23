#pragma warning(push)
#pragma warning(disable: 4100)
#include <proto/friends.pb.h>
#pragma warning(pop)

#include <Utils/WebIO.hpp>
#include <wincrypt.h>

#include "Auth.hpp"
#include "Friends.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Materials.hpp"
#include "Node.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"
#include "Toast.hpp"
#include "UIFeeder.hpp"
#include "UIScript.hpp"
#include "ZWNet.hpp"

namespace Components
{
	namespace
	{
		constexpr auto ZWNET_SOCIAL_API_BASE = "https://backend.zw3.eu";
		constexpr auto ZWNET_SOCIAL_USER_AGENT = "ZW3-ZWNET-Social/3.0.3";
		constexpr auto SOCIAL_FRIEND_FEEDER = 65.0f;
		constexpr auto SOCIAL_REQUEST_FEEDER = 66.0f;
		constexpr std::size_t MAX_SOCIAL_FRIENDS = 512;
		constexpr std::size_t MAX_SOCIAL_REQUESTS = 128;

		struct SocialFriend
		{
			std::string id;
			std::string guid;
			std::string discordId;
			std::string displayName;
			std::string status;
			std::string zwnetPartyId;
			bool joinable{};
			int rankLevel{1};
			int rankPrestige{};
		};

		struct IncomingFriendRequest
		{
			nlohmann::json requestId;
			std::string senderId;
			std::string guid;
			std::string displayName;
		};

		struct IncomingPartyInvite
		{
			std::string inviteId;
			std::string partyId;
			std::string createdAt;
			std::string senderName;
			int memberCount{};
			int maxMembers{4};
		};

		std::atomic_bool SocialActive{false};
		std::atomic_bool SocialBusy{false};
		std::atomic_bool SocialRefreshBusy{false};
		std::atomic_bool PartyInvitePollBusy{false};
		std::mutex SocialMutex;
		std::vector<SocialFriend> SocialFriends;
		std::vector<IncomingFriendRequest> IncomingRequests;
		std::optional<IncomingPartyInvite> CurrentPartyInvite;
		std::string LastHandledPartyInvite;
		unsigned int CurrentSocialFriend{};
		unsigned int CurrentIncomingRequest{};

		std::string SafeSocialText(std::string value, const std::size_t maxLength)
		{
			return TextRenderer::EncodeUtf8ForGame(value, maxLength);
		}

		std::string JsonString(const nlohmann::json& object, const char* key)
		{
			if (!object.is_object() || !object.contains(key) || !object.at(key).is_string()) return {};
			return object.at(key).get<std::string>();
		}

		bool JsonBool(const nlohmann::json& object, const char* key, const bool fallback = false)
		{
			if (!object.is_object() || !object.contains(key)) return fallback;
			const auto& value = object.at(key);
			if (value.is_boolean()) return value.get<bool>();
			if (value.is_number_integer()) return value.get<std::int64_t>() != 0;
			if (value.is_number_unsigned()) return value.get<std::uint64_t>() != 0;
			return fallback;
		}

		bool JsonInteger(const nlohmann::json& object, const char* key, int& result)
		{
			if (!object.is_object() || !object.contains(key)) return false;
			const auto& value = object.at(key);
			if (value.is_number_unsigned())
			{
				const auto parsed = value.get<std::uint64_t>();
				if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) return false;
				result = static_cast<int>(parsed);
				return true;
			}
			if (value.is_number_integer())
			{
				const auto parsed = value.get<std::int64_t>();
				if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) return false;
				result = static_cast<int>(parsed);
				return true;
			}
			if (!value.is_string()) return false;

			const auto& text = value.get_ref<const std::string&>();
			if (text.empty() || text.size() > 12) return false;
			char* end = nullptr;
			const auto parsed = std::strtol(text.c_str(), &end, 10);
			if (end == text.c_str() || *end != '\0') return false;
			result = static_cast<int>(parsed);
			return true;
		}

		bool ReadSocialRankObject(const nlohmann::json& object, const bool nested,
			int& level, int& prestige)
		{
			int parsedLevel = level;
			int parsedPrestige = prestige;
			const auto hasLevel = JsonInteger(object, nested ? "level" : "rank_level", parsedLevel);
			const auto hasPrestige = JsonInteger(object, nested ? "prestige" : "rank_prestige", parsedPrestige);
			if (!hasLevel && !hasPrestige) return false;

			if (hasLevel) level = std::clamp(parsedLevel, 1, 54);
			if (hasPrestige) prestige = std::max(parsedPrestige, 0);
			return true;
		}

		bool ReadSocialRank(const nlohmann::json& row, int& level, int& prestige)
		{
			if (!row.is_object()) return false;
			bool found = ReadSocialRankObject(row, false, level, prestige);
			if (row.contains("rank") && row.at("rank").is_object())
			{
				found = ReadSocialRankObject(row.at("rank"), true, level, prestige) || found;
			}
			if (row.contains("presence") && row.at("presence").is_object())
			{
				const auto& presence = row.at("presence");
				found = ReadSocialRankObject(presence, false, level, prestige) || found;
				if (presence.contains("rank") && presence.at("rank").is_object())
				{
					found = ReadSocialRankObject(presence.at("rank"), true, level, prestige) || found;
				}
			}
			return found;
		}

		std::string BuildSocialRankText(const int level, const int prestige)
		{
			const auto iconName = prestige >= 8
				? std::string{"skullicon"}
				: std::format("prestige_{}", prestige + 1);
			auto* material = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_MATERIAL, iconName.c_str()).material;
			if (!material)
			{
				material = Game::DB_FindXAssetDefaultHeaderInternal(Game::ASSET_TYPE_MATERIAL).material;
			}

			const auto* materialName = material && material->info.name ? material->info.name : "default";
			std::string text;
			text.reserve(std::strlen(materialName) + 12);
			text.push_back('^');
			text.push_back(2);
			text.push_back(0x22);
			text.push_back(0x22);
			text.push_back(static_cast<char>(std::strlen(materialName)));
			text.append(materialName);
			text.append(" ");
			text.append(std::to_string(std::clamp(level, 1, 54)));
			return text;
		}

		bool IsPublicGuid(const std::string& value)
		{
			return value.size() == 16 && std::ranges::all_of(value, [](const unsigned char character)
			{
				return std::isxdigit(character) != 0;
			});
		}

		bool TryParseZombieRankValue(const std::string& data,
			const std::string_view field, int& value)
		{
			const auto fieldPosition = data.find(field);
			if (fieldPosition == std::string::npos) return false;

			auto valuePosition = fieldPosition + field.size();
			while (valuePosition < data.size() &&
				(data[valuePosition] == ':' || data[valuePosition] == ' ' ||
					data[valuePosition] == '\t'))
			{
				++valuePosition;
			}

			if (valuePosition >= data.size()) return false;
			char* end = nullptr;
			const auto parsed = std::strtol(data.c_str() + valuePosition, &end, 10);
			if (end == data.c_str() + valuePosition) return false;
			value = static_cast<int>(parsed);
			return true;
		}

		std::pair<int, int> ReadLocalZombieRank()
		{
			const auto guid = std::format("{:016x}", Auth::GetKeyHash());
			const auto rankPath = std::filesystem::path("zw3") / "core" /
				"scriptdata" / ("rank_" + guid);
			std::string data;
			int storedLevel = 0;
			int prestige = 0;
			if (!Utils::IO::ReadFile(rankPath.string(), &data) ||
				!TryParseZombieRankValue(data, "level", storedLevel) ||
				!TryParseZombieRankValue(data, "prestige", prestige))
			{
				return {1, 0};
			}

			return {std::clamp(storedLevel, 0, 53) + 1,
				std::max(prestige, 0)};
		}

		bool IsOpaquePartyId(const std::string& value)
		{
			return !value.empty() && value.size() <= 80 && std::ranges::all_of(value, [](const unsigned char character)
			{
				return std::isalnum(character) != 0 || character == '-' || character == '_';
			});
		}

		const char* PartyVisibilityName(const int privacy)
		{
			if (privacy == 1) return "INVITE_ONLY";
			if (privacy == 2) return "CLOSED";
			return "OPEN";
		}

		void SetSocialUi(const std::string& status, const bool busy)
		{
			Scheduler::Once([status, busy]
			{
				if (!SocialActive) return;
				Dvar::Var("ui_social_status_message").set(status);
				Dvar::Var("ui_social_invite_busy").set(busy);
			}, Scheduler::Pipeline::MAIN);
		}

		std::optional<std::string> LoadSocialAccessToken()
		{
			const auto encrypted = Utils::IO::ReadFile((FileSystem::GetAppdataPath() / "zwnet.session").string());
			if (encrypted.empty()) return std::nullopt;

			DATA_BLOB input
			{
				static_cast<DWORD>(encrypted.size()),
				reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data()))
			};
			DATA_BLOB output{};
			if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
			{
				return std::nullopt;
			}

			std::string plain{reinterpret_cast<char*>(output.pbData), output.cbData};
			SecureZeroMemory(output.pbData, output.cbData);
			LocalFree(output.pbData);
			const auto clearPlain = gsl::finally([&plain]
			{
				if (!plain.empty()) SecureZeroMemory(plain.data(), plain.size());
			});

			try
			{
				const auto session = nlohmann::json::parse(plain);
				if (!session.contains("access_token") || !session.at("access_token").is_string()) return std::nullopt;
				auto accessToken = session.at("access_token").get<std::string>();
				if (accessToken.empty() || accessToken.size() > 8192) return std::nullopt;
				return accessToken;
			}
			catch (const nlohmann::json::exception&)
			{
				return std::nullopt;
			}
		}

		std::optional<nlohmann::json> SocialApiRequest(const std::string& method, const std::string& path,
			const nlohmann::json& body = nlohmann::json::object(), const bool idempotent = false)
		{
			if (!SocialActive || (method != "GET" && method != "POST") || path.empty() || path.front() != '/')
			{
				return std::nullopt;
			}

			auto accessToken = LoadSocialAccessToken();
			if (!accessToken) return std::nullopt;
			const auto clearToken = gsl::finally([&accessToken]
			{
				if (accessToken && !accessToken->empty()) SecureZeroMemory(accessToken->data(), accessToken->size());
			});

			try
			{
				Utils::WebIO::params headers
				{
					{"Accept", "application/json"},
					{"Authorization", "Bearer " + *accessToken},
					{"Content-Type", "application/json"}
				};
				if (idempotent)
				{
					headers["Idempotency-Key"] = std::format("zw3-social-{}-{}", Game::Sys_Milliseconds(),
						Utils::Cryptography::Rand::GenerateChallenge());
				}

				bool success = false;
				Utils::WebIO request(ZWNET_SOCIAL_USER_AGENT, std::string(ZWNET_SOCIAL_API_BASE) + path);
				const auto response = method == "GET"
					? request.setTimeout(5000)->get(headers, &success)
					: request.setTimeout(5000)->post(body.dump(), headers, &success);
				if (!SocialActive || !success || response.empty()) return std::nullopt;
				const auto parsed = nlohmann::json::parse(response);
				if (parsed.is_object() && parsed.contains("error")) return std::nullopt;
				if (parsed.is_object() && parsed.contains("ok") && !JsonBool(parsed, "ok")) return std::nullopt;
				return parsed;
			}
			catch (const std::exception&)
			{
				return std::nullopt;
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		bool ReplaceSocialFriends(const nlohmann::json& response)
		{
			const nlohmann::json* rows = nullptr;
			if (response.is_array()) rows = &response;
			else if (response.is_object() && response.contains("friends") && response.at("friends").is_array())
			{
				rows = &response.at("friends");
			}
			if (!rows) return false;

			std::vector<SocialFriend> updated;
			updated.reserve(std::min(rows->size(), MAX_SOCIAL_FRIENDS));
			for (const auto& row : *rows)
			{
				if (!row.is_object() || updated.size() >= MAX_SOCIAL_FRIENDS) break;
				auto guid = JsonString(row, "guid");
				auto id = guid;
				if (id.empty()) id = JsonString(row, "id");
				std::ranges::transform(id, id.begin(), [](const unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
				if (!IsPublicGuid(id)) continue;
				if (guid.empty()) guid = id;

				auto status = JsonString(row, "status");
				bool joinable = JsonBool(row, "joinable");
				auto zwnetPartyId = JsonString(row, "zwnet_party_id");
				if (row.contains("presence") && row.at("presence").is_object())
				{
					const auto& presence = row.at("presence");
					if (status.empty()) status = JsonString(presence, "state");
					joinable = JsonBool(presence, "joinable", joinable);
					if (zwnetPartyId.empty()) zwnetPartyId = JsonString(presence, "zwnet_party_id");
				}
				if (!IsOpaquePartyId(zwnetPartyId) || !zwnetPartyId.starts_with("pty_")) zwnetPartyId.clear();
				if (status.empty()) status = JsonBool(row, "online") ? "ONLINE" : "OFFLINE";
				std::ranges::transform(status, status.begin(), [](const unsigned char character)
				{
					return static_cast<char>(std::toupper(character));
				});

				auto displayName = SafeSocialText(JsonString(row, "display_name"), 48);
				if (displayName.empty()) displayName = SafeSocialText(JsonString(row, "player_name"), 48);
				if (displayName.empty()) displayName = "ZW3 Player";
				int rankLevel = 1;
				int rankPrestige = 0;
				if (!ReadSocialRank(row, rankLevel, rankPrestige) &&
					!ZWNet::TryGetSharedLobbyRank(id, rankLevel, rankPrestige))
				{
					Friends::TryGetZombieRankByGuid(id, rankLevel, rankPrestige);
				}
				updated.push_back(
				{
					SafeSocialText(id, 80),
					SafeSocialText(guid, 32),
					SafeSocialText(JsonString(row, "discord_user_id"), 32),
					std::move(displayName),
					SafeSocialText(status, 32),
					SafeSocialText(zwnetPartyId, 80),
					joinable,
					std::clamp(rankLevel, 1, 54),
					std::max(rankPrestige, 0)
				});
			}

			std::lock_guard lock(SocialMutex);
			SocialFriends = std::move(updated);
			CurrentSocialFriend = SocialFriends.empty() ? 0 : std::min<unsigned int>(CurrentSocialFriend,
				static_cast<unsigned int>(SocialFriends.size() - 1));
			return true;
		}

		bool ReplaceIncomingRequests(const nlohmann::json& response)
		{
			const nlohmann::json* rows = nullptr;
			if (response.is_array()) rows = &response;
			else if (response.is_object() && response.contains("requests") && response.at("requests").is_array())
			{
				rows = &response.at("requests");
			}
			if (!rows) return false;

			std::vector<IncomingFriendRequest> updated;
			updated.reserve(std::min(rows->size(), MAX_SOCIAL_REQUESTS));
			for (const auto& row : *rows)
			{
				if (!row.is_object() || updated.size() >= MAX_SOCIAL_REQUESTS) break;
				nlohmann::json requestId;
				if (row.contains("request_id")) requestId = row.at("request_id");
				else if (row.contains("id")) requestId = row.at("id");
				if ((!requestId.is_string() && !requestId.is_number_integer() && !requestId.is_number_unsigned())
					|| (requestId.is_string() && requestId.get_ref<const std::string&>().size() > 80))
				{
					continue;
				}
				const nlohmann::json* sender = nullptr;
				if (row.contains("sender") && row.at("sender").is_object()) sender = &row.at("sender");
				auto displayName = JsonString(row, "sender_name");
				if (displayName.empty()) displayName = JsonString(row, "sender_display_name");
				if (displayName.empty()) displayName = JsonString(row, "from_display_name");
				if (displayName.empty()) displayName = JsonString(row, "display_name");
				if (displayName.empty() && sender) displayName = JsonString(*sender, "display_name");
				if (displayName.empty() && sender) displayName = JsonString(*sender, "name");
				displayName = SafeSocialText(std::move(displayName), 48);
				if (displayName.empty()) displayName = "ZW3 Player";
				auto senderId = JsonString(row, "sender_id");
				if (senderId.empty()) senderId = JsonString(row, "sender_guid");
				if (senderId.empty()) senderId = JsonString(row, "from_guid");
				auto guid = JsonString(row, "guid");
				if (guid.empty() && sender) guid = JsonString(*sender, "guid");
				if (senderId.empty() && sender) senderId = JsonString(*sender, "guid");
				std::ranges::transform(guid, guid.begin(), [](const unsigned char character)
				{
					return static_cast<char>(std::tolower(character));
				});
				if (!guid.empty() && !IsPublicGuid(guid)) guid.clear();
				if (senderId.empty()) senderId = guid;
				updated.push_back(
				{
					std::move(requestId),
					SafeSocialText(senderId, 80),
					SafeSocialText(guid, 32),
					std::move(displayName)
				});
			}

			std::lock_guard lock(SocialMutex);
			IncomingRequests = std::move(updated);
			CurrentIncomingRequest = IncomingRequests.empty() ? 0 : std::min<unsigned int>(CurrentIncomingRequest,
				static_cast<unsigned int>(IncomingRequests.size() - 1));
			return true;
		}

		bool RefreshSocialFriends()
		{
			const auto response = SocialApiRequest("GET", "/social/friends");
			return response && ReplaceSocialFriends(*response);
		}

		bool RefreshIncomingRequests()
		{
			const auto response = SocialApiRequest("GET", "/social/friends/requests");
			return response && ReplaceIncomingRequests(*response);
		}

		void RunSocialTask(const std::string& pendingStatus, std::function<std::string()> task)
		{
			if (SocialBusy.exchange(true))
			{
				SetSocialUi("A social request is already running.", true);
				return;
			}
			SetSocialUi(pendingStatus, true);
			Scheduler::Once([task = std::move(task)]
			{
				std::string result = "The ZW3 social service is unavailable.";
				try
				{
					result = task();
				}
				catch (const std::exception&)
				{
					result = "The ZW3 social request failed safely.";
				}
				catch (...)
				{
					result = "The ZW3 social request failed safely.";
				}
				SocialBusy = false;
				SetSocialUi(result, false);
			}, Scheduler::Pipeline::ASYNC);
		}

		unsigned int GetSocialFriendCount()
		{
			std::lock_guard lock(SocialMutex);
			return static_cast<unsigned int>(SocialFriends.size());
		}

		const char* GetSocialFriendText(const unsigned int index, const int column)
		{
			std::string value;
			{
				std::lock_guard lock(SocialMutex);
				if (index >= SocialFriends.size()) return "";
				const auto& user = SocialFriends[index];
				switch (column)
				{
				case 0: value = BuildSocialRankText(user.rankLevel, user.rankPrestige); break;
				case 1: value = user.displayName; break;
				case 2: value = std::format("PRESTIGE {}", user.rankPrestige); break;
				case 3: value = user.status + (user.joinable ? " / JOINABLE" : ""); break;
				default: return "";
				}
			}
			return Utils::String::VA("%s", value.c_str());
		}

		void SelectSocialFriend(const unsigned int index)
		{
			std::lock_guard lock(SocialMutex);
			if (index < SocialFriends.size()) CurrentSocialFriend = index;
		}

		unsigned int GetIncomingRequestCount()
		{
			std::lock_guard lock(SocialMutex);
			return static_cast<unsigned int>(IncomingRequests.size());
		}

		const char* GetIncomingRequestText(const unsigned int index, const int column)
		{
			std::string value;
			{
				std::lock_guard lock(SocialMutex);
				if (index >= IncomingRequests.size()) return "";
				const auto& request = IncomingRequests[index];
				switch (column)
				{
				case 0: value = ""; break;
				case 1: value = request.displayName; break;
				case 2: value = request.guid.empty() ? request.senderId : request.guid; break;
				default: return "";
				}
			}
			return Utils::String::VA("%s", value.c_str());
		}

		void SelectIncomingRequest(const unsigned int index)
		{
			std::lock_guard lock(SocialMutex);
			if (index < IncomingRequests.size()) CurrentIncomingRequest = index;
		}

		std::optional<SocialFriend> SelectedSocialFriend()
		{
			std::lock_guard lock(SocialMutex);
			if (CurrentSocialFriend >= SocialFriends.size()) return std::nullopt;
			return SocialFriends[CurrentSocialFriend];
		}

		std::optional<IncomingFriendRequest> SelectedIncomingRequest()
		{
			std::lock_guard lock(SocialMutex);
			if (CurrentIncomingRequest >= IncomingRequests.size()) return std::nullopt;
			return IncomingRequests[CurrentIncomingRequest];
		}

		std::string PartyInviteSignature(const IncomingPartyInvite& invite)
		{
			return invite.inviteId + ":" + invite.createdAt;
		}

		std::optional<IncomingPartyInvite> SelectedPartyInvite()
		{
			std::lock_guard lock(SocialMutex);
			return CurrentPartyInvite;
		}

		void MarkPartyInviteHandled(const IncomingPartyInvite& invite)
		{
			std::lock_guard lock(SocialMutex);
			LastHandledPartyInvite = PartyInviteSignature(invite);
			if (CurrentPartyInvite && PartyInviteSignature(*CurrentPartyInvite) == LastHandledPartyInvite)
			{
				CurrentPartyInvite.reset();
			}
		}

		void ClosePartyInvitePopup()
		{
			Scheduler::Once([]
			{
				if (!SocialActive) return;
				Command::Execute("closemenu popup_social_game_invite", false);
			}, Scheduler::Pipeline::MAIN);
		}

		void PollPartyInvites()
		{
			if (!SocialActive || PartyInvitePollBusy.exchange(true)) return;
			const auto clearBusy = gsl::finally([] { PartyInvitePollBusy = false; });
			const auto response = SocialApiRequest("GET", "/zwnet/parties/invites");
			if (!response || !response->is_object() || !response->contains("invites")
				|| !response->at("invites").is_array())
			{
				return;
			}

			for (const auto& row : response->at("invites"))
			{
				if (!row.is_object()) continue;
				auto memberCount = 1;
				auto maxMembers = 4;
				if (const auto value = row.find("member_count"); value != row.end() && value->is_number_integer())
				{
					memberCount = std::clamp(value->get<int>(), 1, 4);
				}
				if (const auto value = row.find("max_members"); value != row.end() && value->is_number_integer())
				{
					maxMembers = std::clamp(value->get<int>(), 1, 4);
				}
				IncomingPartyInvite invite
				{
					JsonString(row, "invite_id"),
					JsonString(row, "party_id"),
					JsonString(row, "created_at"),
					SafeSocialText(JsonString(row, "sender_name"), 48),
					memberCount,
					maxMembers
				};
				if (invite.inviteId.empty() || !IsOpaquePartyId(invite.partyId)) continue;
				if (invite.senderName.empty()) invite.senderName = "ZW3 Friend";
				const auto signature = PartyInviteSignature(invite);
				{
					std::lock_guard lock(SocialMutex);
					if (signature == LastHandledPartyInvite
						|| (CurrentPartyInvite && signature == PartyInviteSignature(*CurrentPartyInvite)))
					{
						return;
					}
					CurrentPartyInvite = invite;
				}

				Scheduler::Once([invite]
				{
					if (!SocialActive) return;
					Dvar::Var("ui_social_game_invite_message").set(
						std::format("{} invited you to a ZW3 lobby.", invite.senderName));
					Dvar::Var("ui_social_game_invite_details").set(
						std::format("{} / {} PLAYERS", invite.memberCount, invite.maxMembers));
					Command::Execute("openmenu popup_social_game_invite", false);
				}, Scheduler::Pipeline::MAIN);
				return;
			}
		}
	}

	bool Friends::LoggedOn = false;
	bool Friends::TriggerSort = false;
	bool Friends::TriggerUpdate = false;

	int Friends::InitialState;
	unsigned int Friends::CurrentFriend;
	std::recursive_mutex Friends::Mutex;
	std::vector<Friends::Friend> Friends::FriendsList;

	Dvar::Var Friends::UIStreamFriendly;
	Dvar::Var Friends::CLAnonymous;
	Dvar::Var Friends::CLNotifyFriendState;

	void Friends::AuthorizeDiscordPartyJoin(const std::string& discordUserId,
		const std::string& partyId,
		std::function<void(std::optional<std::string>)> completion)
	{
		auto completeOnMain = [completion = std::move(completion)](
			std::optional<std::string> joinSecret) mutable
		{
			Scheduler::Once([completion = std::move(completion),
				joinSecret = std::move(joinSecret)]() mutable
			{
				if (completion) completion(std::move(joinSecret));
			}, Scheduler::Pipeline::MAIN);
		};

		if (discordUserId.empty() || discordUserId.size() > 32
			|| !std::ranges::all_of(discordUserId, [](const unsigned char character)
			{
				return std::isdigit(character) != 0;
			})
			|| !IsOpaquePartyId(partyId) || !partyId.starts_with("pty_"))
		{
			completeOnMain(std::nullopt);
			return;
		}

		Scheduler::Once([discordUserId, partyId,
			completeOnMain = std::move(completeOnMain)]() mutable
		{
			std::optional<std::string> joinSecret;
			if (SocialActive && RefreshSocialFriends())
			{
				std::string playerId;
				{
					std::lock_guard lock(SocialMutex);
					const auto found = std::ranges::find_if(SocialFriends,
						[&discordUserId](const SocialFriend& candidate)
						{
							return candidate.discordId == discordUserId;
						});
					if (found != SocialFriends.end()) playerId = found->id;
				}

				if (IsPublicGuid(playerId))
				{
					const auto response = SocialApiRequest("POST",
						"/zwnet/parties/" + partyId + "/join-capability",
						{{"player_id", playerId}});
					if (response && response->is_object())
					{
						auto candidate = JsonString(*response, "join_secret");
						constexpr std::string_view prefix{"zwnet-cap:"};
						if (candidate.starts_with(prefix) &&
							candidate.size() == prefix.size() + 43 &&
							std::ranges::all_of(candidate.substr(prefix.size()),
								[](const unsigned char character)
								{
									return std::isalnum(character) != 0 ||
										character == '-' || character == '_';
								}))
						{
							joinSecret = std::move(candidate);
						}
					}
				}
			}

			completeOnMain(std::move(joinSecret));
		}, Scheduler::Pipeline::ASYNC);
	}

	void Friends::SortIndividualList(std::vector<Friends::Friend>* list)
	{
		std::stable_sort(list->begin(), list->end(), [](Friends::Friend const& friend1, Friends::Friend const& friend2)
		{
			return friend1.cleanName.compare(friend2.cleanName) < 0;
		});
	}

	void Friends::SortList(bool force)
	{
		if (!force)
		{
			Friends::TriggerSort = true;
			return;
		}

		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);

		std::vector<Friends::Friend> connectedList;
		std::vector<Friends::Friend> playingList;
		std::vector<Friends::Friend> onlineList;
		std::vector<Friends::Friend> offlineList;

		// Split up the list
		for (auto entry : Friends::FriendsList)
		{
			if (!entry.online) offlineList.push_back(entry);
			else if (!Friends::IsOnline(entry.lastTime)) onlineList.push_back(entry);
			else if (entry.server.getType() == Game::NA_BAD) playingList.push_back(entry);
			else connectedList.push_back(entry);
		}

		Friends::SortIndividualList(&connectedList);
		Friends::SortIndividualList(&playingList);
		Friends::SortIndividualList(&onlineList);
		Friends::SortIndividualList(&offlineList);

		size_t count = Friends::FriendsList.size();
		Friends::FriendsList.clear();
		Friends::FriendsList.reserve(count);

		Utils::Merge(&Friends::FriendsList, connectedList);
		Utils::Merge(&Friends::FriendsList, playingList);
		Utils::Merge(&Friends::FriendsList, onlineList);
		Utils::Merge(&Friends::FriendsList, offlineList);
	}

	void Friends::UpdateUserInfo(SteamID user)
	{
		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);

		auto entry = std::find_if(Friends::FriendsList.begin(), Friends::FriendsList.end(), [user](Friends::Friend entry)
		{
			return (entry.userId.bits == user.bits);
		});

		if (entry == Friends::FriendsList.end() || !Steam::Proxy::SteamFriends) return;

		entry->name = Steam::Proxy::SteamFriends->GetFriendPersonaName(user);
		entry->online = Steam::Proxy::SteamFriends->GetFriendPersonaState(user) != 0;
		entry->cleanName = Utils::String::ToLower(TextRenderer::StripColors(entry->name));

		std::string guid = Friends::GetPresence(user, "iw4x_guid");
		std::string name = Friends::GetPresence(user, "iw4x_name");
		std::string experience = Friends::GetPresence(user, "iw4x_experience");
		std::string prestige = Friends::GetPresence(user, "iw4x_prestige");
		const auto zombieRankLevel = Friends::GetPresence(user, "zw3_zombie_rank_level");
		const auto zombieRankPrestige = Friends::GetPresence(user, "zw3_zombie_rank_prestige");

		if (!guid.empty()) entry->guid.bits = strtoull(guid.data(), nullptr, 16);
		if (!name.empty()) entry->playerName = name;
		if (!experience.empty()) entry->experience = atoi(experience.data());
		if (!prestige.empty()) entry->prestige = atoi(prestige.data());
		if (!zombieRankLevel.empty())
		{
			entry->zombieRankKnown = true;
			entry->zombieRankLevel = std::clamp(atoi(zombieRankLevel.data()), 1, 54);
			entry->zombieRankPrestige = std::max(atoi(zombieRankPrestige.data()), 0);
		}

		std::string server = Friends::GetPresence(user, "iw4x_server");
		Network::Address oldAddress = entry->server;

		bool gotOnline = Friends::IsOnline(entry->lastTime);
		entry->lastTime = static_cast<unsigned int>(atoi(Friends::GetPresence(user, "iw4x_playing").data()));
		gotOnline = !gotOnline && Friends::IsOnline(entry->lastTime);

		if (server.empty())
		{
			entry->server.setType(Game::NA_BAD);
			entry->serverName.clear();
		}
		else if (entry->server != server)
		{
			entry->server = server;
			entry->serverName.clear();
		}

		// Block localhost
		if (entry->server.getType() == Game::NA_LOOPBACK || (entry->server.getType() == Game::NA_IP && entry->server.getIP().full == 0x0100007F)) entry->server.setType(Game::NA_BAD);
		else if (entry->server.getType() != Game::NA_BAD && entry->server != oldAddress)
		{
			Node::Add(entry->server);
			Network::SendCommand(entry->server, "getinfo", Utils::Cryptography::Rand::GenerateChallenge());
		}

		Friends::SortList();

		const auto notify = Friends::CLNotifyFriendState.get<bool>();
		if (gotOnline && (!notify || (notify && !Game::CL_IsCgameInitialized())) && !Friends::UIStreamFriendly.get<bool>())
		{
			Game::Material* material = Friends::CreateAvatar(user);
			Toast::Show(material, entry->name, "is playing IW4x", 3000, [material]()
			{
				Materials::Delete(material, true);
			});
		}
	}

	void Friends::UpdateState()
	{
		if (Friends::CLAnonymous.get<bool>() || Friends::IsInvisible() || !Steam::Enabled())
		{
			return;
		}

		Friends::TriggerUpdate = true;
	}

	void Friends::UpdateServer(Network::Address server, const std::string& hostname, const std::string& mapname)
	{
		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);

		for (auto& entry : Friends::FriendsList)
		{
			if (entry.server == server)
			{
				entry.serverName = hostname;
				entry.mapname = mapname;
			}
		}
	}

	void Friends::UpdateName()
	{
		Friends::SetPresence("iw4x_name", Steam::SteamFriends()->GetPersonaName());
		Friends::UpdateState();
	}

	std::vector<int> Friends::GetAppIdList()
	{
		std::vector<int> ids;

		const auto addId = [&](int id)
		{
			if (std::find(ids.begin(), ids.end(), id) == ids.end())
			{
				ids.push_back(id);
			}
		};

		addId(0);
		addId(10190);
		addId(480);
		addId(Steam::Proxy::AppId);

		if (Steam::Proxy::SteamUtils)
		{
			addId(Steam::Proxy::SteamUtils->GetAppID());
		}

		if (Steam::Proxy::SteamFriends)
		{
			std::lock_guard<std::recursive_mutex> _(Friends::Mutex);

			const auto modId = *reinterpret_cast<const unsigned int*>("IW4x") | 0x80000000;

			// Split up the list
			for (const auto& entry : Friends::FriendsList)
			{
				Steam::FriendGameInfo info;
				if (Steam::Proxy::SteamFriends->GetFriendGamePlayed(entry.userId, &info) && info.m_gameID.modID == modId)
				{
					addId(info.m_gameID.appID);
				}
			}
		}

		return ids;
	}

	void Friends::SetRawPresence(const char* key, const char* value)
	{
		if (Steam::Proxy::ClientFriends)
		{
			// Set the presence for all possible apps that IW4x might have to interact with.
			// GetFriendRichPresence only reads values for the app that we are running,
			// therefore our friends (and we as well) have to set the presence for those apps.
			auto appIds = Friends::GetAppIdList();

			for (auto id : appIds)
			{
				Steam::Proxy::ClientFriends.invoke<void>("SetRichPresence", id, key, value);
			}
		}
	}

	void Friends::ClearPresence(const std::string& key)
	{
		if (Steam::Proxy::ClientFriends && Steam::Proxy::SteamUtils)
		{
			Friends::SetRawPresence(key.data(), nullptr);
		}
	}

	void Friends::SetPresence(const std::string& key, const std::string& value)
	{
		if (Steam::Proxy::ClientFriends && Steam::Proxy::SteamUtils && !Friends::CLAnonymous.get<bool>() && !Friends::IsInvisible() && Steam::Enabled())
		{
			Friends::SetRawPresence(key.data(), value.data());
		}
	}

	void Friends::RequestPresence(SteamID user)
	{
		if (Steam::Proxy::ClientFriends)
		{
			Steam::Proxy::ClientFriends.invoke<void>("RequestFriendRichPresence", Friends::GetGame(user), user);
		}
	}

	std::string Friends::GetPresence(SteamID user, const std::string& key)
	{
		if (!Steam::Proxy::ClientFriends || !Steam::Proxy::SteamUtils) return "";

		std::string result = Steam::Proxy::ClientFriends.invoke<const char*>("GetFriendRichPresence", Friends::GetGame(user), user, key.data());
		return result;
	}

	void Friends::SetServer()
	{
		Friends::SetPresence("iw4x_server", Network::Address(*Game::connectedHost).getString()); // reinterpret_cast<char*>(0x7ED3F8)
		Friends::UpdateState();
	}

	void Friends::ClearServer()
	{
		Friends::ClearPresence("iw4x_server");
		Friends::UpdateState();
	}

	bool Friends::IsClientInParty(int /*controller*/, int clientNum)
	{
		if (clientNum < 0 || clientNum >= ARRAYSIZE(Dedicated::PlayerGuids)) return false;

		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);
		SteamID guid = Dedicated::PlayerGuids[clientNum][0];

		for (auto entry : Friends::FriendsList)
		{
			if (entry.guid.bits == guid.bits && Friends::IsOnline(entry.lastTime) && entry.online)
			{
				return true;
			}
		}

		return false;
	}

	void Friends::UpdateRank()
	{
		static std::optional<int> levelVal;

		int experience = Game::Live_GetXp(0);
		int prestige = Game::Live_GetPrestige(0);
		int level = (experience & 0xFFFFFF) | ((prestige & 0xFF) << 24);

		if (!levelVal.has_value() || levelVal.value() != level)
		{
			levelVal.emplace(level);

			Friends::SetPresence("iw4x_experience", Utils::String::VA("%d", experience));
			Friends::SetPresence("iw4x_prestige", Utils::String::VA("%d", prestige));
			Friends::UpdateState();
		}
	}

	void Friends::UpdateZombieRankPresence()
	{
		if (!Steam::Enabled() || !Steam::Proxy::ClientFriends ||
			!Steam::Proxy::SteamUtils)
		{
			return;
		}

		static std::optional<std::pair<int, int>> lastRank;
		const auto rank = ReadLocalZombieRank();
		if (lastRank && *lastRank == rank) return;
		lastRank = rank;

		Friends::SetPresence("zw3_zombie_rank_level", std::to_string(rank.first));
		Friends::SetPresence("zw3_zombie_rank_prestige", std::to_string(rank.second));
		Friends::UpdateState();
	}

	bool Friends::TryGetZombieRankByGuid(const std::string& guid,
		int& level, int& prestige)
	{
		if (!IsPublicGuid(guid)) return false;
		const auto guidValue = std::strtoull(guid.c_str(), nullptr, 16);
		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);
		const auto entry = std::ranges::find_if(Friends::FriendsList,
			[guidValue](const Friend& candidate)
			{
				return candidate.guid.bits == guidValue && candidate.zombieRankKnown;
			});
		if (entry == Friends::FriendsList.end()) return false;

		level = entry->zombieRankLevel;
		prestige = entry->zombieRankPrestige;
		return true;
	}

	void Friends::UpdateFriends()
	{
		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);

		Friends::LoggedOn = (Steam::Proxy::SteamUser_ && Steam::Proxy::SteamUser_->LoggedOn());
		if (!Steam::Proxy::SteamFriends) return;

		if (Game::Sys_IsMainThread())
		{
			Game::UI_UpdateArenas();
		}

		int count = Steam::Proxy::SteamFriends->GetFriendCount(4);

		Proto::Friends::List list;
		list.ParseFromString(Utils::IO::ReadFile("players/friends.dat"));

		std::vector<Friends::Friend> steamFriends;

		for (int i = 0; i < count; ++i)
		{
			SteamID id = Steam::Proxy::SteamFriends->GetFriendByIndex(i, 4);

			Friends::Friend entry;
			entry.userId = id;
			entry.guid.bits = 0;
			entry.online = false;
			entry.lastTime = 0;
			entry.prestige = 0;
			entry.experience = 0;
			entry.server.setType(Game::NA_BAD);

			for (auto storedFriend : list.friends())
			{
				if (entry.userId.bits == strtoull(storedFriend.steamid().data(), nullptr, 16))
				{
					entry.playerName = storedFriend.name();
					entry.experience = storedFriend.experience();
					entry.prestige = storedFriend.prestige();
					entry.guid.bits = strtoull(storedFriend.guid().data(), nullptr, 16);
					break;
				}
			}

			auto oldEntry = std::find_if(Friends::FriendsList.begin(), Friends::FriendsList.end(), [id](Friends::Friend entry)
			{
				return (entry.userId.bits == id.bits);
			});

			if (oldEntry != Friends::FriendsList.end()) entry = *oldEntry;
			else Friends::FriendsList.push_back(entry);

			steamFriends.push_back(entry);
		}

		for (auto i = Friends::FriendsList.begin(); i != Friends::FriendsList.end();)
		{
			SteamID id = i->userId;

			auto oldEntry = std::find_if(steamFriends.begin(), steamFriends.end(), [id](Friends::Friend entry)
			{
				return (entry.userId.bits == id.bits);
			});

			if (oldEntry == steamFriends.end())
			{
				i = Friends::FriendsList.erase(i);
			}
			else
			{
				*i = *oldEntry;
				++i;

				Friends::UpdateUserInfo(id);
				Friends::RequestPresence(id);
			}
		}
	}

	unsigned int Friends::GetFriendCount()
	{
		return Friends::FriendsList.size();
	}

	const char* Friends::GetFriendText(unsigned int index, int column)
	{
		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);
		if (index >= Friends::FriendsList.size()) return "";

		auto user = Friends::FriendsList[index];

		switch (column)
		{
		case 0:
		{
			static char buffer[0x100];
			ZeroMemory(buffer, sizeof(buffer));

			Game::Material* rankIcon = nullptr;
			int rank = Game::CL_GetRankForXP(user.experience);
			Game::CL_GetRankIcon(rank, user.prestige, &rankIcon);
			if (!rankIcon) rankIcon = Game::DB_FindXAssetDefaultHeaderInternal(Game::XAssetType::ASSET_TYPE_MATERIAL).material;

			buffer[0] = '^';
			buffer[1] = 2;

			// Icon size
			char size = 0x30;
			buffer[2] = size; // Width
			buffer[3] = size; // Height

			// Icon name length
			buffer[4] = static_cast<char>(strlen(rankIcon->info.name));

			strcat_s(buffer, rankIcon->info.name);
			strcat_s(buffer, Utils::String::VA(" %i", (rank + 1)));

			return buffer;
		}
		case 1:
		{
			if (user.playerName.empty())
			{
				return Utils::String::VA("%s", user.name.data());
			}

			if (user.name == user.playerName)
			{
				return Utils::String::VA("%s", user.name.data());
			}

			return Utils::String::VA("%s ^7(%s^7)", user.name.data(), user.playerName.data());
		}
		case 2:
		{
			if (!user.online) return "Offline";
			if (!Friends::IsOnline(user.lastTime)) return "Online";
			if (user.server.getType() == Game::NA_BAD) return "Playing IW4x";
			if (user.serverName.empty()) return Utils::String::VA("Playing on %s", user.server.getCString());
			return Utils::String::VA("Playing %s on %s", Localization::LocalizeMapName(user.mapname.data()), user.serverName.data());
		}

		default:
			break;
		}

		return "";
	}

	void Friends::SelectFriend(unsigned int index)
	{
		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);
		if (index >= Friends::FriendsList.size()) return;

		Friends::CurrentFriend = index;
	}

	int Friends::GetGame(SteamID user)
	{
		int appId = 0;

		Steam::FriendGameInfo info;
		if (Steam::Proxy::SteamFriends && Steam::Proxy::SteamFriends->GetFriendGamePlayed(user, &info))
		{
			appId = info.m_gameID.appID;
		}

		return appId;
	}

	bool Friends::IsInvisible()
	{
		return Friends::InitialState == 7;
	}

	void Friends::UpdateTimeStamp()
	{
		Friends::SetPresence("iw4x_playing", Utils::String::VA("%d", Steam::SteamUtils()->GetServerRealTime()));
		Friends::SetPresence("iw4x_guid", Utils::String::VA("%llX", Steam::SteamUser()->GetSteamID().bits));
	}

	bool Friends::IsOnline(unsigned __int64 timeStamp)
	{
		if (!Steam::Proxy::SteamUtils) return false;
		static const unsigned __int64 duration = std::chrono::duration_cast<std::chrono::seconds>(5min).count();

		return ((Steam::SteamUtils()->GetServerRealTime() - timeStamp) < duration);
	}

	void Friends::StoreFriendsList()
	{
		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);

		// Only store our cache if we are logged in, otherwise it might be invalid
		if (!Friends::LoggedOn) return;

		Proto::Friends::List list;
		for (auto entry : Friends::FriendsList)
		{
			Proto::Friends::Friend* friendEntry = list.add_friends();

			friendEntry->set_steamid(Utils::String::VA("%llX", entry.userId.bits));
			friendEntry->set_guid(Utils::String::VA("%llX", entry.guid.bits));
			friendEntry->set_name(entry.playerName);
			friendEntry->set_experience(entry.experience);
			friendEntry->set_prestige(entry.prestige);
		}

		Utils::IO::WriteFile("players/friends.dat", list.SerializeAsString());
	}

	Game::Material* Friends::CreateAvatar(SteamID user)
	{
		if (!Steam::Proxy::SteamUtils || !Steam::Proxy::SteamFriends) return nullptr;

		int index = Steam::Proxy::SteamFriends->GetMediumFriendAvatar(user);

		unsigned int width, height;
		Steam::Proxy::SteamUtils->GetImageSize(index, &width, &height);

		Game::GfxImage* image = Materials::CreateImage(Utils::String::VA("texture_%llX", user.bits), width, height, 1, 0x1000003, D3DFMT_A8R8G8B8);

		D3DLOCKED_RECT lockedRect;
		image->texture.map->LockRect(0, &lockedRect, nullptr, 0);

		unsigned char* buffer = static_cast<unsigned char*>(lockedRect.pBits);
		Steam::Proxy::SteamUtils->GetImageRGBA(index, buffer, width * height * 4);

		// Swap red and blue channel
		for (unsigned int i = 0; i < width * height * 4; i += 4)
		{
			std::swap(buffer[i + 0], buffer[i + 2]);
		}

		// Steam rounds the corners and somehow fuck up the pixels there
		buffer[3] = 0;                                                // top-left
		buffer[(width - 1) * 4 + 3] = 0;                              // top-right
		buffer[((height - 1) * width * 4) + 3] = 0;                   // bottom-left
		buffer[((height - 1) * width * 4) + (width - 1) * 4 + 3] = 0; // bottom-right

		image->texture.map->UnlockRect(0);

		return Materials::Create(Utils::String::VA("avatar_%llX", user.bits), image);
	}

	Friends::Friends()
	{
		Friends::LoggedOn = false;

		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		//Friends::UIStreamFriendly = Dvar::Register<bool>("ui_streamFriendly", false, Game::DVAR_ARCHIVE, "Stream friendly UI");
		//Friends::CLAnonymous = Dvar::Register<bool>("cl_anonymous", false, Game::DVAR_ARCHIVE, "Enable invisible mode for Steam");
		//Friends::CLNotifyFriendState = Dvar::Register<bool>("cl_notifyFriendState", true, Game::DVAR_ARCHIVE, "Update friends about current game status");

		// Hook Live_ShowFriendsList
		Utils::Hook(0x4D6C70, []()
		{
			Command::Execute("openmenu popup_friends", true);
		}, HOOK_JUMP).install()->quick();

		// Callback to update user information
		Steam::Proxy::RegisterCallback(336, [](void* data)
		{
			Friends::FriendRichPresenceUpdate* update = static_cast<Friends::FriendRichPresenceUpdate*>(data);
			Friends::UpdateUserInfo(update->m_steamIDFriend);
		});

		// Persona state has changed
		Steam::Proxy::RegisterCallback(304, [](void* data)
		{
			Friends::PersonaStateChange* state = static_cast<Friends::PersonaStateChange*>(data);
			Friends::RequestPresence(state->m_ulSteamID);
		});

		// Update state when connecting/disconnecting
		Events::OnSteamDisconnect(Friends::ClearServer);

		Utils::Hook(0x4CD023, Friends::SetServer, HOOK_JUMP).install()->quick();

		// Show blue icons on the minimap
		Utils::Hook(0x493130, Friends::IsClientInParty, HOOK_JUMP).install()->quick();

		UIScript::Add("LoadFriends", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			Friends::UpdateFriends();
		});

		UIScript::Add("RefreshFriends", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			// Preserve the legacy Steam presence cache while refreshing the
			// authoritative ZWNET social lists used by this menu.
			Friends::UpdateFriends();
			RunSocialTask("Refreshing ZW3 friends...", []
			{
				const auto friendsOk = RefreshSocialFriends();
				const auto requestsOk = RefreshIncomingRequests();
				if (friendsOk && requestsOk) return "Friends and requests updated.";
				if (friendsOk) return "Friends updated; requests are temporarily unavailable.";
				return "The ZW3 friends service is unavailable.";
			});
		});

		UIScript::Add("LoadFriendRequests", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			RunSocialTask("Refreshing friend requests...", []
			{
				return RefreshIncomingRequests()
					? "Friend requests updated."
					: "Friend requests are temporarily unavailable.";
			});
		});

		UIScript::Add("AddFriendFromDvar", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			auto target = Dvar::Var("ui_social_friend_guid").get<std::string>();
			std::ranges::transform(target, target.begin(), [](const unsigned char character)
			{
				return static_cast<char>(std::tolower(character));
			});
			if (!IsPublicGuid(target))
			{
				SetSocialUi("Enter the 16-character public ZW3 GUID.", false);
				return;
			}

			RunSocialTask("Sending friend request...", [target]
			{
				const auto response = SocialApiRequest("POST", "/social/friends/request", {{"guid", target}}, true);
				if (!response) return "The friend request could not be sent.";
				Scheduler::Once([]
				{
					if (SocialActive) Dvar::Var("ui_social_friend_guid").set("");
				}, Scheduler::Pipeline::MAIN);
				RefreshIncomingRequests();
				return "Friend request sent.";
			});
		});

		UIScript::Add("AcceptFriendRequest", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			const auto request = SelectedIncomingRequest();
			if (!request)
			{
				SetSocialUi("Select an incoming friend request first.", false);
				return;
			}
			RunSocialTask("Accepting friend request...", [request]
			{
				const auto response = SocialApiRequest("POST", "/social/friends/accept",
					{{"request_id", request->requestId}}, true);
				if (!response) return "The friend request could not be accepted.";
				const auto friendsOk = RefreshSocialFriends();
				const auto requestsOk = RefreshIncomingRequests();
				return friendsOk && requestsOk
					? "Friend request accepted."
					: "Request accepted; the lists will refresh shortly.";
			});
		});

		UIScript::Add("RemoveSelectedFriend", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			const auto user = SelectedSocialFriend();
			if (!user)
			{
				SetSocialUi("Select a ZW3 friend first.", false);
				return;
			}
			RunSocialTask("Removing friend...", [user]
			{
				if (user->discordId.empty()) return "The Stats friend response has no removable Discord identity.";
				const auto response = SocialApiRequest("POST", "/social/friends/remove",
					{{"discord_user_id", user->discordId}}, true);
				if (!response) return "The friend could not be removed.";
				return RefreshSocialFriends()
					? "Friend removed."
					: "Friend removed; the list will refresh shortly.";
			});
		});

		UIScript::Add("JoinSelectedFriend", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			const auto user = SelectedSocialFriend();
			if (!user)
			{
				SetSocialUi("Select a ZW3 friend first.", false);
				return;
			}
			if (!user->joinable || !IsOpaquePartyId(user->zwnetPartyId) ||
				!user->zwnetPartyId.starts_with("pty_"))
			{
				SetSocialUi("This friend's ZW3 party is not currently joinable.", false);
				return;
			}

			RunSocialTask("Joining friend's ZW3 party...", [user]
			{
				const auto response = SocialApiRequest("POST",
					"/zwnet/parties/join-friend",
					{{"player_id", user->id}}, true);
				if (!response || !response->is_object() || response->contains("error"))
				{
					return "The friend's ZW3 party could not be joined.";
				}
				ZWNet::ResumeParty(*response);
				Scheduler::Once([]
				{
					if (SocialActive) Command::Execute("closemenu popup_friends", false);
				}, Scheduler::Pipeline::MAIN);
				return "Friend's party joined.";
			});
		});

		UIScript::Add("InviteSelectedFriend", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			const auto user = SelectedSocialFriend();
			if (!user)
			{
				SetSocialUi("Select a ZW3 friend first.", false);
				return;
			}
			const auto visibility = std::string{PartyVisibilityName(
				std::clamp(Dvar::Var("partyPrivacy").get<int>(), 0, 2))};
			RunSocialTask("Preparing party invitation...", [user, visibility]
			{
				auto party = SocialApiRequest("GET", "/zwnet/parties/current");
				if (!party || party->is_null())
				{
					party = SocialApiRequest("POST", "/zwnet/parties/create",
						{{"visibility", visibility}}, true);
				}
				if (!party || !party->is_object()) return "A party could not be created or loaded.";
				auto partyId = JsonString(*party, "id");
				if (partyId.empty()) partyId = JsonString(*party, "partyId");
				if (!IsOpaquePartyId(partyId)) return "The party response was invalid.";
				auto currentVisibility = JsonString(*party, "visibility");
				if (currentVisibility.empty()) currentVisibility = "OPEN";
				if (currentVisibility != visibility)
				{
					const auto updated = SocialApiRequest("POST",
						"/zwnet/parties/" + partyId + "/set-visibility",
						{{"visibility", visibility}}, true);
					if (!updated || !updated->is_object() || updated->contains("error"))
					{
						return "The party privacy setting could not be synchronized.";
					}
					party = updated;
				}

				const auto response = SocialApiRequest("POST", "/zwnet/parties/" + partyId + "/invite",
					{{"player_id", user->id}}, true);
				return response
					? "Party invitation sent."
					: "The party invitation could not be sent.";
			});
		});

		UIScript::Add("ConfirmGameInvite", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			const auto invite = SelectedPartyInvite();
			if (!invite)
			{
				ClosePartyInvitePopup();
				return;
			}
			RunSocialTask("Joining the ZW3 party...", [invite]
			{
				const auto response = SocialApiRequest("POST", "/zwnet/parties/" + invite->partyId + "/accept",
					nlohmann::json::object(), true);
				if (!response || !response->is_object()) return "The party invitation could not be accepted.";
				MarkPartyInviteHandled(*invite);
				ZWNet::ResumeParty(*response);
				Scheduler::Once([]
				{
					if (!SocialActive) return;
					Command::Execute("closemenu popup_social_game_invite", false);
					Command::Execute("closemenu popup_friends", false);
				}, Scheduler::Pipeline::MAIN);
				return "Party joined.";
			});
		});

		UIScript::Add("DeclineGameInvite", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			const auto invite = SelectedPartyInvite();
			if (!invite)
			{
				ClosePartyInvitePopup();
				return;
			}
			RunSocialTask("Declining the party invitation...", [invite]
			{
				const auto response = SocialApiRequest("POST", "/zwnet/parties/" + invite->partyId + "/decline",
					nlohmann::json::object(), true);
				if (!response) return "The party invitation could not be declined.";
				MarkPartyInviteHandled(*invite);
				ClosePartyInvitePopup();
				return "Party invitation declined.";
			});
		});

		UIScript::Add("CloseGameInvitePopup", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			const auto invite = SelectedPartyInvite();
			if (!invite) return;
			MarkPartyInviteHandled(*invite);
			Scheduler::Once([invite]
			{
				SocialApiRequest("POST", "/zwnet/parties/" + invite->partyId + "/decline",
					nlohmann::json::object(), true);
			}, Scheduler::Pipeline::ASYNC);
		});

		UIScript::Add("JoinFriend", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			std::string selectedGuid;
			std::optional<Network::Address> legacyServer;
			{
				std::lock_guard<std::recursive_mutex> _(Friends::Mutex);
				if (Friends::CurrentFriend >= Friends::FriendsList.size()) return;
				const auto& selected = Friends::FriendsList[Friends::CurrentFriend];
				selectedGuid = Utils::String::VA("%016llx", selected.guid.bits);
				if (selected.online && selected.server.getType() != Game::NA_BAD)
				{
					legacyServer = selected.server;
				}
			}

			std::optional<SocialFriend> socialUser;
			{
				std::lock_guard lock(SocialMutex);
				const auto found = std::ranges::find_if(SocialFriends,
					[&selectedGuid](const SocialFriend& candidate)
					{
						return candidate.guid == selectedGuid;
					});
				if (found != SocialFriends.end()) socialUser = *found;
			}
			if (socialUser && IsOpaquePartyId(socialUser->zwnetPartyId) &&
				socialUser->zwnetPartyId.starts_with("pty_"))
			{
				RunSocialTask("Joining friend's ZW3 party...", [socialUser]
				{
					const auto response = SocialApiRequest("POST",
						"/zwnet/parties/join-friend",
						{{"player_id", socialUser->id}}, true);
					if (!response || !response->is_object() || response->contains("error"))
					{
						return "The friend's ZW3 party could not be joined.";
					}
					ZWNet::ResumeParty(*response);
					return "Friend's party joined.";
				});
				return;
			}

			if (legacyServer)
			{
				const auto server = *legacyServer;
				Scheduler::Once([server]
				{
					if (!SocialActive) return;
					Party::Connect(server);
				}, Scheduler::Pipeline::MAIN);
			}
			else
			{
				Command::Execute("snd_playLocal exit_prestige", false);
			}
		});

		Scheduler::Loop([]
		{
			static Utils::Time::Interval timeInterval;
			static Utils::Time::Interval sortInterval;
			static Utils::Time::Interval stateInterval;
			static Utils::Time::Interval zombieRankInterval;

			if (*reinterpret_cast<bool*>(0x1AD5690)) // LiveStorage_DoWeHaveStats
			{
				Friends::UpdateRank();
			}

			if (zombieRankInterval.elapsed(2s))
			{
				zombieRankInterval.update();
				Friends::UpdateZombieRankPresence();
			}

			if (timeInterval.elapsed(2min))
			{
				timeInterval.update();
				Friends::UpdateTimeStamp();
				Friends::UpdateState();
			}

			if (stateInterval.elapsed(5s))
			{
				stateInterval.update();

				if (Friends::TriggerUpdate)
				{
					Friends::TriggerUpdate = false;
					Friends::UpdateState();
				}
			}

			if (sortInterval.elapsed(1s))
			{
				sortInterval.update();

				if (Friends::TriggerSort)
				{
					Friends::TriggerSort = false;
					Friends::SortList(true);
				}
			}
		}, Scheduler::Pipeline::CLIENT);

		UIFeeder::Add(61.0f, Friends::GetFriendCount, Friends::GetFriendText, Friends::SelectFriend);
		UIFeeder::Add(SOCIAL_FRIEND_FEEDER, GetSocialFriendCount, GetSocialFriendText, SelectSocialFriend);
		UIFeeder::Add(SOCIAL_REQUEST_FEEDER, GetIncomingRequestCount, GetIncomingRequestText, SelectIncomingRequest);
		Scheduler::Loop([]
		{
			if (!SocialActive || SocialBusy) return;
			auto* menu = Game::Menus_FindByName(Game::uiContext, "popup_friends");
			if (!menu || !Game::Menu_IsVisible(Game::uiContext, menu) || SocialRefreshBusy.exchange(true)) return;

			Scheduler::Once([]
			{
				const auto clearBusy = gsl::finally([] { SocialRefreshBusy = false; });
				if (!SocialActive) return;
				RefreshSocialFriends();
				RefreshIncomingRequests();
			}, Scheduler::Pipeline::ASYNC);
		}, Scheduler::Pipeline::MAIN, 15s);
		Scheduler::Loop(PollPartyInvites, Scheduler::Pipeline::ASYNC, 5s);

		Scheduler::OnGameShutdown([]
		{
			Friends::ClearPresence("iw4x_server");
			Friends::ClearPresence("iw4x_playing");

#ifdef DEBUG
			if (Steam::Proxy::SteamFriends)
			{
				Steam::Proxy::SteamFriends->ClearRichPresence();
			}
#endif

			if (Steam::Proxy::ClientFriends)
			{
				Steam::Proxy::ClientFriends.invoke<void>("SetPersonaState", Friends::InitialState);
			}
		});

		Scheduler::OnGameInitialized([]
		{
			// String Dvars use the engine's RD_BUFFER critical section. Register
			// them only after the game database and critical sections are ready.
			Dvar::Register<const char*>("ui_social_status_message", "ZW3 social is ready.", Game::DVAR_NONE,
				"Stable, non-technical status shown by the ZW3 social menu");
			Dvar::Register<bool>("ui_social_invite_busy", false, Game::DVAR_NONE,
				"Prevents duplicate ZW3 social menu actions");
			Dvar::Register<const char*>("ui_social_friend_guid", "", Game::DVAR_NONE,
				"Public ZW3 GUID entered for a friend request");
			Dvar::Register<const char*>("ui_social_own_guid", "", Game::DVAR_ROM,
				"Local public ZW3 GUID shown in the social menu");
			SocialActive = true;
			Dvar::Var("ui_social_own_guid").set(std::format("{:016x}", Auth::GetKeyHash()));

			if (Steam::Proxy::SteamFriends)
			{
				Friends::InitialState = Steam::Proxy::SteamFriends->GetFriendPersonaState(Steam::Proxy::SteamUser_->GetSteamID());
			}

			if (Friends::CLAnonymous.get<bool>() || Friends::IsInvisible() || !Steam::Enabled())
			{
				if (Steam::Proxy::ClientFriends)
				{
					for (const auto id : Friends::GetAppIdList())
					{
						Steam::Proxy::ClientFriends.invoke<void>("ClearRichPresence", id);
					}
				}

				if (Steam::Proxy::SteamFriends)
				{
					Steam::Proxy::SteamFriends->ClearRichPresence();
				}
			}

			Friends::UpdateTimeStamp();
			Friends::UpdateName();
			Friends::UpdateZombieRankPresence();
			Friends::UpdateState();

			Friends::UpdateFriends();
		}, Scheduler::Pipeline::MAIN);
	}

	Friends::~Friends()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		SocialActive = false;
		SocialBusy = false;
		Friends::StoreFriendsList();

		Steam::Proxy::UnregisterCallback(336);
		Steam::Proxy::UnregisterCallback(304);

		{
			std::lock_guard lock(SocialMutex);
			SocialFriends.clear();
			IncomingRequests.clear();
			CurrentPartyInvite.reset();
			LastHandledPartyInvite.clear();
		}

		std::lock_guard<std::recursive_mutex> _(Friends::Mutex);
		Friends::FriendsList.clear();

	}
}
