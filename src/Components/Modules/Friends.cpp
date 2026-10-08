#include "STDInclude.hpp"

#include <wincrypt.h>

#pragma warning(push)
#pragma warning(disable: 4100)
#include <proto/friends.pb.h>
#pragma warning(pop)

#include "Friends.hpp"
#include "Auth.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Materials.hpp"
#include "Node.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"
#include "Toast.hpp"
#include "UIFeeder.hpp"
#include "UIScript.hpp"
#include "ZWNet.hpp"

#include "Steam/Proxy.hpp"
#include "Steam/Interfaces/SteamUser.hpp"

namespace Components
{
	bool Friends::isLoggedOn = false;
	bool Friends::shouldSort = false;
	bool Friends::shouldUpdate = false;

	int Friends::initialState = 0;
	unsigned int Friends::currentFriend = 0;
	std::recursive_mutex Friends::mutex;
	std::vector<Friends::Friend> Friends::friendsList;

	Dvar::Var Friends::ui_streamFriendly;
	Dvar::Var Friends::cl_anonymous;
	Dvar::Var Friends::cl_notifyFriendState;

	constexpr std::uintptr_t Steam_ShowFriendsList_Thunk = 0x1402A4180;
	constexpr std::uintptr_t Steam_ShowFriendsList = 0x14024E1F0;

	constexpr std::uintptr_t CL_ClientIsInMyParty = 0x1400E1190;
	static const std::uint8_t clientIsInMyPartyEntry[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20 };

	constexpr std::uintptr_t CG_Init_CG_ParseServerInfoCall = 0x1400D6684;
	constexpr std::uintptr_t CL_RestartCGame_CG_ParseServerInfoCall = 0x1400F4FC0;
	constexpr std::uintptr_t CG_ParseServerInfo = 0x1400E87A0;

	static Utils::Hook showFriendsListHook;
	static Utils::Hook clientInPartyHook;
	static Utils::Hook parseServerInfoHooks[2];

	constexpr int personaStateInvisible = 7;
	constexpr int friendFlagImmediate = 4;

	void Friends::SortIndividualList(std::vector<Friend>* list)
	{
		std::stable_sort(list->begin(), list->end(), [](const Friend& friend1, const Friend& friend2)
		{
			return friend1.cleanName.compare(friend2.cleanName) < 0;
		});
	}

	void Friends::SortList(bool force)
	{
		if (!force)
		{
			shouldSort = true;
			return;
		}

		std::lock_guard _(mutex);

		std::vector<Friend> connectedList;
		std::vector<Friend> playingList;
		std::vector<Friend> onlineList;
		std::vector<Friend> offlineList;

		for (const auto& entry : friendsList)
		{
			if (!entry.online)
			{
				offlineList.push_back(entry);
			}
			else if (!IsOnline(entry.lastTime))
			{
				onlineList.push_back(entry);
			}
			else if (entry.server.GetType() == Game::NA_BAD)
			{
				playingList.push_back(entry);
			}
			else
			{
				connectedList.push_back(entry);
			}
		}

		SortIndividualList(&connectedList);
		SortIndividualList(&playingList);
		SortIndividualList(&onlineList);
		SortIndividualList(&offlineList);

		friendsList.clear();
		friendsList.insert(friendsList.end(), connectedList.begin(), connectedList.end());
		friendsList.insert(friendsList.end(), playingList.begin(), playingList.end());
		friendsList.insert(friendsList.end(), onlineList.begin(), onlineList.end());
		friendsList.insert(friendsList.end(), offlineList.begin(), offlineList.end());
	}

	static std::string BuildSocialRankText(const int level, const int prestige)
	{
		std::string iconName = "skullicon";

		if (prestige < 8)
		{
			iconName = std::format("prestige_{}", prestige + 1);
		}

		std::string text;
		text.push_back('^');
		text.push_back(2);
		text.push_back(0x22);
		text.push_back(0x22);
		text.push_back(static_cast<char>(iconName.size()));
		text.append(iconName);
		text.append(" ");
		text.append(std::to_string(std::clamp(level, 1, 54)));
		return text;
	}

	static bool IsPublicGuid(const std::string& value)
	{
		return value.size() == 16 && std::ranges::all_of(value, [](const unsigned char character)
		{
			return std::isxdigit(character) != 0;
		});
	}

	static bool TryParseZombieRankValue(const std::string& data, const std::string_view field, int& value)
	{
		const auto fieldPosition = data.find(field);

		if (fieldPosition == std::string::npos)
		{
			return false;
		}

		auto valuePosition = fieldPosition + field.size();

		while (valuePosition < data.size() && (data[valuePosition] == ':' || data[valuePosition] == ' ' || data[valuePosition] == '\t'))
		{
			++valuePosition;
		}

		if (valuePosition >= data.size())
		{
			return false;
		}

		char* end = nullptr;
		const auto parsed = std::strtol(data.data() + valuePosition, &end, 10);

		if (end == data.data() + valuePosition)
		{
			return false;
		}

		value = static_cast<int>(parsed);
		return true;
	}

	static std::pair<int, int> ReadLocalZombieRank()
	{
		const auto guid = std::format("{:016x}", Auth::GetKeyHash());
		const auto rankPath = std::filesystem::path("zw3") / "core" / "scriptdata" / ("rank_" + guid);

		std::string data;
		int storedLevel = 0;
		int prestige = 0;

		if (!Utils::IO::ReadFile(rankPath.string(), &data)
			|| !TryParseZombieRankValue(data, "level", storedLevel)
			|| !TryParseZombieRankValue(data, "prestige", prestige))
		{
			return { 1, 0 };
		}

		return { std::clamp(storedLevel, 0, 53) + 1, std::max(prestige, 0) };
	}

	constexpr auto socialApiBase = "https://backend.zw3.eu";
	constexpr auto socialUserAgent = "ZW3-ZWNET-Social/3.0.4";
	constexpr float socialFriendFeeder = 65.0f;
	constexpr float socialRequestFeeder = 66.0f;
	constexpr std::size_t maxSocialFriends = 512;
	constexpr std::size_t maxSocialRequests = 128;

	struct SocialFriend
	{
		std::string id;
		std::string guid;
		std::string discordId;
		std::string displayName;
		std::string status;
		std::string zwnetPartyId;
		bool isJoinable = false;
		bool isRankKnown = false;
		int rankLevel = 0;
		int rankPrestige = 0;
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
		int memberCount = 0;
		int maxMembers = 4;
	};

	static std::atomic_bool isSocialActive = false;
	static std::atomic_bool isSocialBusy = false;
	static std::atomic_bool isSocialRefreshBusy = false;
	static std::atomic_bool isPartyInvitePollBusy = false;

	static std::mutex socialMutex;
	static std::mutex socialErrorMutex;
	static std::string lastSocialError;
	static std::vector<SocialFriend> socialFriends;
	static std::vector<IncomingFriendRequest> incomingRequests;
	static std::unordered_set<std::string> pendingOutgoingFriends;
	static std::optional<IncomingPartyInvite> currentPartyInvite;
	static std::string lastHandledPartyInvite;
	static unsigned int currentSocialFriend = 0;
	static unsigned int currentIncomingRequest = 0;

	static std::string SafeSocialText(const std::string& value, const std::size_t maxLength)
	{
		return TextRenderer::EncodeUtf8ForGame(value, maxLength);
	}

	static std::string ToLowerAscii(std::string value)
	{
		std::ranges::transform(value, value.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});

		return value;
	}

	static std::string SocialJsonString(const nlohmann::json& object, const char* key)
	{
		if (!object.is_object() || !object.contains(key) || !object.at(key).is_string())
		{
			return {};
		}

		return object.at(key).get<std::string>();
	}

	static bool SocialJsonBool(const nlohmann::json& object, const char* key, const bool fallback = false)
	{
		if (!object.is_object() || !object.contains(key))
		{
			return fallback;
		}

		const auto& value = object.at(key);

		if (value.is_boolean())
		{
			return value.get<bool>();
		}

		if (value.is_number_integer())
		{
			return value.get<std::int64_t>() != 0;
		}

		if (value.is_number_unsigned())
		{
			return value.get<std::uint64_t>() != 0;
		}

		return fallback;
	}

	static bool TryReadSocialInteger(const nlohmann::json& object, const char* key, int& result)
	{
		if (!object.is_object() || !object.contains(key))
		{
			return false;
		}

		const auto& value = object.at(key);

		if (value.is_number_unsigned())
		{
			const auto parsed = value.get<std::uint64_t>();

			if (parsed > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
			{
				return false;
			}

			result = static_cast<int>(parsed);
			return true;
		}

		if (value.is_number_integer())
		{
			const auto parsed = value.get<std::int64_t>();

			if (parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max())
			{
				return false;
			}

			result = static_cast<int>(parsed);
			return true;
		}

		if (!value.is_string())
		{
			return false;
		}

		const auto& text = value.get_ref<const std::string&>();

		if (text.empty() || text.size() > 12)
		{
			return false;
		}

		char* end = nullptr;
		const auto parsed = std::strtol(text.data(), &end, 10);

		if (end == text.data() || *end != '\0')
		{
			return false;
		}

		result = static_cast<int>(parsed);
		return true;
	}

	static bool TryReadSocialRankObject(const nlohmann::json& object, const bool isNested, int& level, int& prestige)
	{
		const char* levelKey = "rank_level";
		const char* prestigeKey = "rank_prestige";

		if (isNested)
		{
			levelKey = "level";
			prestigeKey = "prestige";
		}

		int parsedLevel = 0;
		int parsedPrestige = 0;
		const bool hasLevel = TryReadSocialInteger(object, levelKey, parsedLevel);
		const bool hasPrestige = TryReadSocialInteger(object, prestigeKey, parsedPrestige);

		if (!hasLevel || !hasPrestige)
		{
			return false;
		}

		if (parsedLevel < 1 || parsedLevel > 54 || parsedPrestige < 0 || parsedPrestige > 20)
		{
			return false;
		}

		level = parsedLevel;
		prestige = parsedPrestige;
		return true;
	}

	static bool TryReadSocialRank(const nlohmann::json& row, int& level, int& prestige)
	{
		if (!row.is_object())
		{
			return false;
		}

		if (TryReadSocialRankObject(row, false, level, prestige))
		{
			return true;
		}

		if (row.contains("rank") && row.at("rank").is_object() && TryReadSocialRankObject(row.at("rank"), true, level, prestige))
		{
			return true;
		}

		if (!row.contains("presence") || !row.at("presence").is_object())
		{
			return false;
		}

		const auto& presence = row.at("presence");

		if (TryReadSocialRankObject(presence, false, level, prestige))
		{
			return true;
		}

		return presence.contains("rank") && presence.at("rank").is_object() && TryReadSocialRankObject(presence.at("rank"), true, level, prestige);
	}

	static std::string SocialStatusText(std::string status)
	{
		std::ranges::transform(status, status.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::toupper(character));
		});

		if (status == "IN_PARTY" || status == "IN_LOBBY" || status == "MATCH_FOUND" || status == "READY_CHECK" || status == "WAITING_FOR_READY")
		{
			return "IN LOBBY";
		}

		if (status == "MAP_VOTE" || status == "VOTING")
		{
			return "VOTING";
		}

		if (status == "RESERVING_SERVER" || status == "STARTING_SERVER" || status == "SERVER_STARTING" || status == "STARTING")
		{
			return "STARTING";
		}

		if (status == "CONNECTING" || status == "JOINING" || status == "JOIN_PREVIEW")
		{
			return "JOINING";
		}

		if (status == "IN_MATCH" || status == "IN_GAME" || status == "IN_PUBLIC_MATCH" || status == "IN_ZOMBIES")
		{
			return "IN GAME";
		}

		if (status == "SEARCHING")
		{
			return "SEARCHING";
		}

		if (status == "MAIN_MENU" || status == "IDLE" || status == "ONLINE")
		{
			return "ONLINE";
		}

		if (status.empty())
		{
			return "OFFLINE";
		}

		return status;
	}

	static void SetLastSocialError(std::string error)
	{
		std::lock_guard _(socialErrorMutex);
		lastSocialError = std::move(error);
	}

	static std::string SocialFailure(const std::string& fallback)
	{
		std::string code;

		{
			std::lock_guard _(socialErrorMutex);
			code = lastSocialError;
		}

		if (code == "PARTY_FULL" || code == "MATCH_FULL" || code == "SERVER_FULL")
		{
			return "That lobby is full.";
		}

		if (code == "PARTY_CLOSED" || code == "INVITE_REQUIRED")
		{
			return "That lobby requires a current invitation.";
		}

		if (code == "PLAYER_BLOCKED")
		{
			return "Joining is unavailable because one player blocked the other.";
		}

		if (code == "FRIEND_PARTY_NOT_JOINABLE" || code == "MATCH_NOT_JOINABLE")
		{
			return "That friend is no longer in a joinable session.";
		}

		if (code == "JOIN_IN_PROGRESS_CLOSED" || code == "SERVER_NOT_JOINABLE")
		{
			return "That match no longer allows joining in progress.";
		}

		if (code == "INVITE_INVALID")
		{
			return "That party invitation expired or was cancelled.";
		}

		if (code == "ALREADY_IN_PARTY")
		{
			return "Leave or finish the current party before joining this one.";
		}

		return fallback;
	}

	static bool HasSocialFailureResolvedInvite()
	{
		std::lock_guard _(socialErrorMutex);

		return !lastSocialError.empty() && lastSocialError != "REQUEST_UNAVAILABLE"
			&& lastSocialError != "LOGIN_REQUIRED" && lastSocialError != "REQUEST_FAILED";
	}

	static bool IsSocialPartyId(const std::string& value)
	{
		return !value.empty() && value.size() <= 80 && std::ranges::all_of(value, [](const unsigned char character)
		{
			return std::isalnum(character) != 0 || character == '-' || character == '_';
		});
	}

	static bool IsZwnetPartyId(const std::string& value)
	{
		return IsSocialPartyId(value) && value.starts_with("pty_");
	}

	static const char* SocialPartyVisibilityName(const int privacy)
	{
		if (privacy == 1)
		{
			return "INVITE_ONLY";
		}

		if (privacy == 2)
		{
			return "CLOSED";
		}

		return "OPEN";
	}

	static void SetSocialUi(const std::string& status, const bool isBusy)
	{
		Scheduler::Once([status, isBusy]
		{
			if (!isSocialActive)
			{
				return;
			}

			Dvar::Var("ui_social_status_message").Set(status);
			Dvar::Var("ui_social_invite_busy").Set(isBusy);
		}, Scheduler::Pipeline::MAIN);
	}

	static std::optional<std::string> LoadSocialAccessToken()
	{
		const auto path = ZWNet::SessionPath();

		if (path.empty())
		{
			return std::nullopt;
		}

		const auto encrypted = Utils::IO::ReadFile(path);

		if (encrypted.empty())
		{
			return std::nullopt;
		}

		DATA_BLOB input{ static_cast<DWORD>(encrypted.size()), reinterpret_cast<BYTE*>(const_cast<char*>(encrypted.data())) };
		DATA_BLOB output{};

		if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output))
		{
			return std::nullopt;
		}

		std::string plain(reinterpret_cast<char*>(output.pbData), output.cbData);
		SecureZeroMemory(output.pbData, output.cbData);
		LocalFree(output.pbData);

		std::optional<std::string> accessToken;

		try
		{
			const auto session = nlohmann::json::parse(plain);

			if (session.contains("access_token") && session.at("access_token").is_string())
			{
				auto token = session.at("access_token").get<std::string>();

				if (!token.empty() && token.size() <= 8192)
				{
					accessToken = std::move(token);
				}
			}
		}
		catch (const nlohmann::json::exception&)
		{
			accessToken.reset();
		}

		SecureZeroMemory(plain.data(), plain.size());
		return accessToken;
	}

	static std::optional<nlohmann::json> SocialApiRequest(const std::string& method, const std::string& path, const nlohmann::json& body = nlohmann::json::object(), const bool isIdempotent = false)
	{
		SetLastSocialError({});

		if (!isSocialActive || (method != "GET" && method != "POST") || path.empty() || path.front() != '/')
		{
			SetLastSocialError("REQUEST_UNAVAILABLE");
			return std::nullopt;
		}

		auto accessToken = LoadSocialAccessToken();

		if (!accessToken)
		{
			SetLastSocialError("LOGIN_REQUIRED");
			return std::nullopt;
		}

		std::optional<nlohmann::json> result;

		try
		{
			Utils::WebIO::Params headers =
			{
				{ "Accept", "application/json" },
				{ "Authorization", "Bearer " + *accessToken },
				{ "Content-Type", "application/json" },
			};

			if (isIdempotent)
			{
				headers["Idempotency-Key"] = std::format("zw3-social-{}-{}", Game::Sys_Milliseconds(), Utils::Cryptography::Rand::GenerateChallenge());
			}

			const auto url = std::string(socialApiBase) + path;
			bool isSuccessful = false;

			Utils::WebIO request(socialUserAgent);
			request.SetTimeout(5000);

			std::string response;

			if (method == "GET")
			{
				request.SetURL(url);
				response = request.Get(headers, &isSuccessful);
			}
			else
			{
				response = request.Post(url, body.dump(), headers, &isSuccessful);
			}

			if (!isSocialActive || !isSuccessful || response.empty())
			{
				SetLastSocialError("REQUEST_UNAVAILABLE");
			}
			else
			{
				auto parsed = nlohmann::json::parse(response);

				const bool isError = parsed.is_object() && parsed.contains("error");
				const bool isNotOk = parsed.is_object() && parsed.contains("ok") && !SocialJsonBool(parsed, "ok");

				if (isError)
				{
					const auto& error = parsed.at("error");

					if (error.is_object())
					{
						SetLastSocialError(SocialJsonString(error, "code"));
					}
					else
					{
						SetLastSocialError("REQUEST_FAILED");
					}
				}
				else if (isNotOk)
				{
					SetLastSocialError("REQUEST_FAILED");
				}
				else
				{
					result = std::move(parsed);
				}
			}
		}
		catch (const std::exception&)
		{
			SetLastSocialError("REQUEST_UNAVAILABLE");
			result.reset();
		}

		SecureZeroMemory(accessToken->data(), accessToken->size());
		return result;
	}

	static const nlohmann::json* FindRows(const nlohmann::json& response, const char* key)
	{
		if (response.is_array())
		{
			return &response;
		}

		if (response.is_object() && response.contains(key) && response.at(key).is_array())
		{
			return &response.at(key);
		}

		return nullptr;
	}

	static bool TryReplaceSocialFriends(const nlohmann::json& response)
	{
		const auto* rows = FindRows(response, "friends");

		if (!rows)
		{
			return false;
		}

		std::vector<SocialFriend> updated;
		updated.reserve(std::min(rows->size(), maxSocialFriends));

		for (const auto& row : *rows)
		{
			if (!row.is_object() || updated.size() >= maxSocialFriends)
			{
				break;
			}

			auto guid = SocialJsonString(row, "guid");
			auto id = guid;

			if (id.empty())
			{
				id = SocialJsonString(row, "id");
			}

			id = ToLowerAscii(id);

			if (!IsPublicGuid(id))
			{
				continue;
			}

			if (guid.empty())
			{
				guid = id;
			}

			auto status = SocialJsonString(row, "status");
			bool isJoinable = SocialJsonBool(row, "joinable");
			auto zwnetPartyId = SocialJsonString(row, "zwnet_party_id");

			if (row.contains("presence") && row.at("presence").is_object())
			{
				const auto& presence = row.at("presence");

				if (status.empty())
				{
					status = SocialJsonString(presence, "status");
				}

				if (status.empty())
				{
					status = SocialJsonString(presence, "state");
				}

				isJoinable = SocialJsonBool(presence, "joinable", isJoinable);

				if (zwnetPartyId.empty())
				{
					zwnetPartyId = SocialJsonString(presence, "zwnet_party_id");
				}
			}

			if (!isJoinable || !IsZwnetPartyId(zwnetPartyId))
			{
				zwnetPartyId.clear();
			}

			if (status.empty() && SocialJsonBool(row, "online"))
			{
				status = "ONLINE";
			}
			else if (status.empty())
			{
				status = "OFFLINE";
			}

			status = SocialStatusText(std::move(status));

			auto displayName = SafeSocialText(SocialJsonString(row, "display_name"), 48);

			if (displayName.empty())
			{
				displayName = SafeSocialText(SocialJsonString(row, "player_name"), 48);
			}

			if (displayName.empty())
			{
				displayName = "ZW3 Player";
			}

			int rankLevel = 0;
			int rankPrestige = 0;
			bool isRankKnown = TryReadSocialRank(row, rankLevel, rankPrestige);

			if (!isRankKnown)
			{
				isRankKnown = ZWNet::TryGetSharedLobbyRank(id, rankLevel, rankPrestige);
			}

			if (!isRankKnown)
			{
				isRankKnown = Friends::TryGetZombieRankByGuid(id, rankLevel, rankPrestige);
			}

			SocialFriend user;
			user.id = SafeSocialText(id, 80);
			user.guid = SafeSocialText(guid, 32);
			user.discordId = SafeSocialText(SocialJsonString(row, "discord_user_id"), 32);
			user.displayName = std::move(displayName);
			user.status = SafeSocialText(status, 32);
			user.zwnetPartyId = SafeSocialText(zwnetPartyId, 80);
			user.isJoinable = isJoinable;
			user.isRankKnown = isRankKnown;

			if (isRankKnown)
			{
				user.rankLevel = rankLevel;
				user.rankPrestige = rankPrestige;
			}

			updated.emplace_back(std::move(user));
		}

		std::lock_guard _(socialMutex);
		socialFriends = std::move(updated);

		for (const auto& user : socialFriends)
		{
			pendingOutgoingFriends.erase(user.id);
			pendingOutgoingFriends.erase(user.guid);
		}

		if (socialFriends.empty())
		{
			currentSocialFriend = 0;
		}
		else
		{
			currentSocialFriend = std::min(currentSocialFriend, static_cast<unsigned int>(socialFriends.size() - 1));
		}

		return true;
	}

	static std::string FirstSocialString(const nlohmann::json& object, const std::initializer_list<const char*> keys)
	{
		for (const auto* key : keys)
		{
			auto value = SocialJsonString(object, key);

			if (!value.empty())
			{
				return value;
			}
		}

		return {};
	}

	static bool TryReplaceIncomingRequests(const nlohmann::json& response)
	{
		const auto* rows = FindRows(response, "requests");

		if (!rows)
		{
			return false;
		}

		std::vector<IncomingFriendRequest> updated;
		updated.reserve(std::min(rows->size(), maxSocialRequests));

		for (const auto& row : *rows)
		{
			if (!row.is_object() || updated.size() >= maxSocialRequests)
			{
				break;
			}

			nlohmann::json requestId;

			if (row.contains("request_id"))
			{
				requestId = row.at("request_id");
			}
			else if (row.contains("id"))
			{
				requestId = row.at("id");
			}

			const bool isIdKind = requestId.is_string() || requestId.is_number_integer() || requestId.is_number_unsigned();

			if (!isIdKind || (requestId.is_string() && requestId.get_ref<const std::string&>().size() > 80))
			{
				continue;
			}

			const nlohmann::json* sender = nullptr;

			if (row.contains("sender") && row.at("sender").is_object())
			{
				sender = &row.at("sender");
			}

			auto displayName = FirstSocialString(row, { "sender_name", "sender_display_name", "from_display_name", "display_name" });

			if (displayName.empty() && sender)
			{
				displayName = FirstSocialString(*sender, { "display_name", "name" });
			}

			displayName = SafeSocialText(displayName, 48);

			if (displayName.empty())
			{
				displayName = "ZW3 Player";
			}

			auto senderId = FirstSocialString(row, { "sender_id", "sender_guid", "from_guid" });
			auto guid = SocialJsonString(row, "guid");

			if (guid.empty() && sender)
			{
				guid = SocialJsonString(*sender, "guid");
			}

			if (senderId.empty() && sender)
			{
				senderId = SocialJsonString(*sender, "guid");
			}

			guid = ToLowerAscii(guid);

			if (!guid.empty() && !IsPublicGuid(guid))
			{
				guid.clear();
			}

			if (senderId.empty())
			{
				senderId = guid;
			}

			IncomingFriendRequest request;
			request.requestId = std::move(requestId);
			request.senderId = SafeSocialText(senderId, 80);
			request.guid = SafeSocialText(guid, 32);
			request.displayName = std::move(displayName);

			updated.emplace_back(std::move(request));
		}

		std::lock_guard _(socialMutex);
		incomingRequests = std::move(updated);

		if (incomingRequests.empty())
		{
			currentIncomingRequest = 0;
		}
		else
		{
			currentIncomingRequest = std::min(currentIncomingRequest, static_cast<unsigned int>(incomingRequests.size() - 1));
		}

		return true;
	}

	static bool TryRefreshSocialFriends()
	{
		const auto response = SocialApiRequest("GET", "/social/friends");
		return response && TryReplaceSocialFriends(*response);
	}

	static bool TryRefreshIncomingRequests()
	{
		const auto response = SocialApiRequest("GET", "/social/friends/requests");
		return response && TryReplaceIncomingRequests(*response);
	}

	static void RunSocialTask(const std::string& pendingStatus, std::function<std::string()> task)
	{
		if (isSocialBusy.exchange(true))
		{
			SetSocialUi("A social request is already running.", true);
			return;
		}

		SetSocialUi(pendingStatus, true);

		Scheduler::Once([task = std::move(task)]
		{
			std::string result;

			try
			{
				result = task();
			}
			catch (const std::exception&)
			{
				result = "The ZW3 social request failed safely.";
			}

			isSocialBusy = false;
			SetSocialUi(result, false);
		}, Scheduler::Pipeline::ASYNC);
	}

	static unsigned int GetSocialFriendCount()
	{
		std::lock_guard _(socialMutex);
		return static_cast<unsigned int>(socialFriends.size());
	}

	static const char* GetSocialFriendText(const unsigned int index, const int column)
	{
		std::string value;

		{
			std::lock_guard _(socialMutex);

			if (index >= socialFriends.size())
			{
				return "";
			}

			const auto& user = socialFriends[index];

			switch (column)
			{
			case 0:
				value = "--";

				if (user.isRankKnown)
				{
					value = BuildSocialRankText(user.rankLevel, user.rankPrestige);
				}

				break;
			case 1:
				value = user.displayName;
				break;
			case 2:
				value = "UNAVAILABLE";

				if (user.isRankKnown)
				{
					value = std::format("PRESTIGE {}", user.rankPrestige);
				}

				break;
			case 3:
				value = user.status;

				if (user.isJoinable)
				{
					value += " / JOINABLE";
				}

				break;
			default:
				return "";
			}
		}

		return Utils::String::VA("%s", value.data());
	}

	static void SelectSocialFriend(const unsigned int index)
	{
		std::lock_guard _(socialMutex);

		if (index < socialFriends.size())
		{
			currentSocialFriend = index;
		}
	}

	static unsigned int GetIncomingRequestCount()
	{
		std::lock_guard _(socialMutex);
		return static_cast<unsigned int>(incomingRequests.size());
	}

	static const char* GetIncomingRequestText(const unsigned int index, const int column)
	{
		std::string value;

		{
			std::lock_guard _(socialMutex);

			if (index >= incomingRequests.size())
			{
				return "";
			}

			const auto& request = incomingRequests[index];

			if (column == 1)
			{
				value = request.displayName;
			}
			else if (column == 2 && request.guid.empty())
			{
				value = request.senderId;
			}
			else if (column == 2)
			{
				value = request.guid;
			}
			else
			{
				return "";
			}
		}

		return Utils::String::VA("%s", value.data());
	}

	static void SelectIncomingRequest(const unsigned int index)
	{
		std::lock_guard _(socialMutex);

		if (index < incomingRequests.size())
		{
			currentIncomingRequest = index;
		}
	}

	static std::optional<SocialFriend> SelectedSocialFriend()
	{
		std::lock_guard _(socialMutex);

		if (currentSocialFriend >= socialFriends.size())
		{
			return std::nullopt;
		}

		return socialFriends[currentSocialFriend];
	}

	static std::optional<IncomingFriendRequest> SelectedIncomingRequest()
	{
		std::lock_guard _(socialMutex);

		if (currentIncomingRequest >= incomingRequests.size())
		{
			return std::nullopt;
		}

		return incomingRequests[currentIncomingRequest];
	}

	static std::string LobbyPlayerRelationship(const std::string& target)
	{
		if (target == std::format("{:016x}", Auth::GetKeyHash()))
		{
			return "SELF";
		}

		std::lock_guard _(socialMutex);

		const bool isFriend = std::ranges::any_of(socialFriends, [&target](const SocialFriend& user)
		{
			return user.id == target || user.guid == target;
		});

		if (isFriend)
		{
			return "FRIEND";
		}

		const bool hasRequested = std::ranges::any_of(incomingRequests, [&target](const IncomingFriendRequest& request)
		{
			return request.senderId == target || request.guid == target;
		});

		if (hasRequested)
		{
			return "INCOMING_REQUEST";
		}

		if (pendingOutgoingFriends.contains(target))
		{
			return "PENDING";
		}

		return "AVAILABLE";
	}

	static void SetLobbyPlayerRelationship(const std::string& target, const std::string& relationship)
	{
		Scheduler::Once([target, relationship]
		{
			const auto selected = ToLowerAscii(Dvar::Var("zwnet_selected_player_guid").Get<std::string>());

			if (isSocialActive && selected == target)
			{
				Dvar::Var("zwnet_selected_player_relationship").Set(relationship);
			}
		}, Scheduler::Pipeline::MAIN);
	}

	static std::string PartyInviteSignature(const IncomingPartyInvite& invite)
	{
		return invite.inviteId + ":" + invite.createdAt;
	}

	static std::optional<IncomingPartyInvite> SelectedPartyInvite()
	{
		std::lock_guard _(socialMutex);
		return currentPartyInvite;
	}

	static void MarkPartyInviteHandled(const IncomingPartyInvite& invite)
	{
		std::lock_guard _(socialMutex);
		lastHandledPartyInvite = PartyInviteSignature(invite);

		if (currentPartyInvite && PartyInviteSignature(*currentPartyInvite) == lastHandledPartyInvite)
		{
			currentPartyInvite.reset();
		}
	}

	static void ClosePartyInvitePopup()
	{
		Scheduler::Once([]
		{
			if (isSocialActive)
			{
				Command::Execute("closemenu popup_social_game_invite", false);
			}
		}, Scheduler::Pipeline::MAIN);
	}

	static void PollPartyInvites()
	{
		if (!isSocialActive || isPartyInvitePollBusy.exchange(true))
		{
			return;
		}

		const auto response = SocialApiRequest("GET", "/zwnet/parties/invites");
		const bool hasInvites = response && response->is_object() && response->contains("invites") && response->at("invites").is_array();

		if (!hasInvites)
		{
			isPartyInvitePollBusy = false;
			return;
		}

		std::unordered_set<std::string> liveInvites;

		for (const auto& row : response->at("invites"))
		{
			if (!row.is_object())
			{
				continue;
			}

			const auto inviteId = SocialJsonString(row, "invite_id");

			if (!inviteId.empty())
			{
				liveInvites.insert(inviteId + ":" + SocialJsonString(row, "created_at"));
			}
		}

		bool shouldCloseExpiredPopup = false;

		{
			std::lock_guard _(socialMutex);

			if (currentPartyInvite && !liveInvites.contains(PartyInviteSignature(*currentPartyInvite)))
			{
				currentPartyInvite.reset();
				shouldCloseExpiredPopup = true;
			}
		}

		if (shouldCloseExpiredPopup)
		{
			ClosePartyInvitePopup();
		}

		for (const auto& row : response->at("invites"))
		{
			if (!row.is_object())
			{
				continue;
			}

			IncomingPartyInvite invite;
			invite.inviteId = SocialJsonString(row, "invite_id");
			invite.partyId = SocialJsonString(row, "party_id");
			invite.createdAt = SocialJsonString(row, "created_at");
			invite.senderName = SafeSocialText(SocialJsonString(row, "sender_name"), 48);
			invite.memberCount = 1;
			invite.maxMembers = 4;

			int count = 0;

			if (row.contains("member_count") && row.at("member_count").is_number_integer() && TryReadSocialInteger(row, "member_count", count))
			{
				invite.memberCount = std::clamp(count, 1, 4);
			}

			if (row.contains("max_members") && row.at("max_members").is_number_integer() && TryReadSocialInteger(row, "max_members", count))
			{
				invite.maxMembers = std::clamp(count, 1, 4);
			}

			if (invite.inviteId.empty() || !IsSocialPartyId(invite.partyId))
			{
				continue;
			}

			if (invite.senderName.empty())
			{
				invite.senderName = "ZW3 Friend";
			}

			const auto signature = PartyInviteSignature(invite);

			{
				std::lock_guard _(socialMutex);

				const bool isKnown = signature == lastHandledPartyInvite || (currentPartyInvite && signature == PartyInviteSignature(*currentPartyInvite));

				if (isKnown)
				{
					continue;
				}

				currentPartyInvite = invite;
			}

			Scheduler::Once([invite]
			{
				if (!isSocialActive)
				{
					return;
				}

				Dvar::Var("ui_social_game_invite_message").Set(std::format("{} invited you to a ZW3 lobby.", invite.senderName));
				Dvar::Var("ui_social_game_invite_details").Set(std::format("{} / {} PLAYERS", invite.memberCount, invite.maxMembers));
				Command::Execute("openmenu popup_social_game_invite", false);
			}, Scheduler::Pipeline::MAIN);

			break;
		}

		isPartyInvitePollBusy = false;
	}

	static std::string JoinFriendParty(const std::string& playerId)
	{
		const auto response = SocialApiRequest("POST", "/zwnet/parties/join-friend", { { "player_id", playerId } }, true);

		if (!response || !response->is_object() || response->contains("error"))
		{
			return SocialFailure("The friend's ZW3 party could not be joined.");
		}

		ZWNet::ResumeParty(*response);
		return "Friend's party joined.";
	}

	static std::optional<std::string> SelectedLobbyPlayerGuid()
	{
		const auto target = ToLowerAscii(Dvar::Var("zwnet_selected_player_guid").Get<std::string>());

		if (!IsPublicGuid(target))
		{
			return std::nullopt;
		}

		return target;
	}

	void Friends::UpdateUserInfo(::Steam::SteamID user)
	{
		std::lock_guard _(mutex);

		const auto entry = std::find_if(friendsList.begin(), friendsList.end(), [user](const Friend& candidate)
		{
			return candidate.userId.bits == user.bits;
		});

		if (entry == friendsList.end() || !::Steam::Proxy::SteamFriends)
		{
			return;
		}

		entry->name = ::Steam::Proxy::SteamFriends->GetFriendPersonaName(user);
		entry->online = ::Steam::Proxy::SteamFriends->GetFriendPersonaState(user) != 0;
		entry->cleanName = Utils::String::ToLower(TextRenderer::StripColors(entry->name));

		const std::string guid = GetPresence(user, "iw4x_guid");
		const std::string name = GetPresence(user, "iw4x_name");
		const std::string experience = GetPresence(user, "iw4x_experience");
		const std::string prestige = GetPresence(user, "iw4x_prestige");
		const std::string zombieRankLevel = GetPresence(user, "zw3_zombie_rank_level");
		const std::string zombieRankPrestige = GetPresence(user, "zw3_zombie_rank_prestige");

		if (!guid.empty())
		{
			entry->guid.bits = std::strtoull(guid.data(), nullptr, 16);
		}

		if (!name.empty())
		{
			entry->playerName = name;
		}

		if (!experience.empty())
		{
			entry->experience = std::atoi(experience.data());
		}

		if (!prestige.empty())
		{
			entry->prestige = std::atoi(prestige.data());
		}

		if (!zombieRankLevel.empty())
		{
			entry->isZombieRankKnown = true;
			entry->zombieRankLevel = std::clamp(std::atoi(zombieRankLevel.data()), 1, 54);
			entry->zombieRankPrestige = std::max(std::atoi(zombieRankPrestige.data()), 0);
		}

		const std::string server = GetPresence(user, "iw4x_server");
		const Network::Address oldAddress = entry->server;

		bool didComeOnline = IsOnline(entry->lastTime);
		entry->lastTime = static_cast<unsigned int>(std::atoi(GetPresence(user, "iw4x_playing").data()));
		didComeOnline = !didComeOnline && IsOnline(entry->lastTime);

		if (server.empty())
		{
			entry->server.SetType(Game::NA_BAD);
			entry->serverName.clear();
		}
		else if (entry->server != Network::Address(server))
		{
			entry->server = Network::Address(server);
			entry->serverName.clear();
		}

		const bool isLocalhost = entry->server.GetType() == Game::NA_LOOPBACK
			|| (entry->server.GetType() == Game::NA_IP && entry->server.GetIP() == 0x0100007F);

		if (isLocalhost)
		{
			entry->server.SetType(Game::NA_BAD);
		}
		else if (entry->server.GetType() != Game::NA_BAD && entry->server != oldAddress)
		{
			Node::Add(entry->server);
			Network::SendCommand(entry->server, "getinfo", Utils::Cryptography::Rand::GenerateChallenge());
		}

		SortList();

		const bool shouldNotify = cl_notifyFriendState.Get<bool>();

		if (didComeOnline && (!shouldNotify || !Game::CL_IsCgameInitialized(0)) && !ui_streamFriendly.Get<bool>())
		{
			Game::Material* material = CreateAvatar(user);

			Toast::Show(material, entry->name, "is playing IW4x", 3000, [material]
			{
				Materials::Delete(material, true);
			});
		}
	}

	void Friends::UpdateState()
	{
		if (cl_anonymous.Get<bool>() || IsInvisible())
		{
			return;
		}

		shouldUpdate = true;
	}

	void Friends::UpdateServer(const Network::Address& server, const std::string& hostname, const std::string& mapname)
	{
		std::lock_guard _(mutex);

		for (auto& entry : friendsList)
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
		SetPresence("iw4x_name", Dvar::Name.Get<std::string>());
		UpdateState();
	}

	void Friends::SetRawPresence(const char* key, const char* value)
	{
		if (::Steam::Proxy::SteamFriends)
		{
			::Steam::Proxy::SteamFriends->SetRichPresence(key, value);
		}
	}

	void Friends::ClearPresence(const std::string& key)
	{
		SetRawPresence(key.data(), nullptr);
	}

	void Friends::SetPresence(const std::string& key, const std::string& value)
	{
		if (!cl_anonymous.Get<bool>() && !IsInvisible())
		{
			SetRawPresence(key.data(), value.data());
		}
	}

	void Friends::RequestPresence(::Steam::SteamID user)
	{
		if (::Steam::Proxy::SteamFriends)
		{
			::Steam::Proxy::SteamFriends->RequestFriendRichPresence(user);
		}
	}

	std::string Friends::GetPresence(::Steam::SteamID user, const std::string& key)
	{
		if (!::Steam::Proxy::SteamFriends)
		{
			return {};
		}

		const char* const value = ::Steam::Proxy::SteamFriends->GetFriendRichPresence(user, key.data());

		if (!value)
		{
			return {};
		}

		return value;
	}

	void Friends::SetServer()
	{
		SetPresence("iw4x_server", Network::Address(*Game::clc_serverAddress).GetString());
		UpdateState();
	}

	void Friends::ClearServer()
	{
		ClearPresence("iw4x_server");
		UpdateState();
	}

	void Friends::CG_ParseServerInfo_Hk(int localClientNum)
	{
		reinterpret_cast<void(*)(int)>(parseServerInfoHooks[0].GetOriginal())(localClientNum);

		SetServer();
	}

	bool Friends::IsClientInParty([[maybe_unused]] int controller, int clientNum)
	{
		if (clientNum < 0 || clientNum >= static_cast<int>(std::size(Dedicated::playerGuids)))
		{
			return false;
		}

		std::lock_guard _(mutex);
		const ::Steam::SteamID guid = Dedicated::playerGuids[clientNum][0];

		for (const auto& entry : friendsList)
		{
			if (entry.guid.bits == guid.bits && IsOnline(entry.lastTime) && entry.online)
			{
				return true;
			}
		}

		return false;
	}

	void Friends::UpdateZombieRankPresence()
	{
		if (!::Steam::Proxy::SteamFriends)
		{
			return;
		}

		static std::optional<std::pair<int, int>> lastRank;
		const auto rank = ReadLocalZombieRank();

		if (lastRank && *lastRank == rank)
		{
			return;
		}

		lastRank = rank;

		SetPresence("zw3_zombie_rank_level", std::to_string(rank.first));
		SetPresence("zw3_zombie_rank_prestige", std::to_string(rank.second));
		UpdateState();
	}

	bool Friends::TryGetZombieRankByGuid(const std::string& guid, int& level, int& prestige)
	{
		if (!IsPublicGuid(guid))
		{
			return false;
		}

		const auto guidValue = std::strtoull(guid.data(), nullptr, 16);

		std::lock_guard _(mutex);

		const auto entry = std::ranges::find_if(friendsList, [guidValue](const Friend& candidate)
		{
			return candidate.guid.bits == guidValue && candidate.isZombieRankKnown;
		});

		if (entry == friendsList.end())
		{
			return false;
		}

		level = entry->zombieRankLevel;
		prestige = entry->zombieRankPrestige;
		return true;
	}

	std::string Friends::GetLobbyPlayerRelationship(const std::string& guid)
	{
		const auto normalized = ToLowerAscii(guid);

		if (!IsPublicGuid(normalized))
		{
			return "UNAVAILABLE";
		}

		return LobbyPlayerRelationship(normalized);
	}

	void Friends::AuthorizeDiscordPartyJoin(const std::string& discordUserId, const std::string& partyId, std::function<void(std::optional<std::string>)> completion)
	{
		auto completeOnMain = [completion = std::move(completion)](std::optional<std::string> joinSecret) mutable
		{
			Scheduler::Once([completion = std::move(completion), joinSecret = std::move(joinSecret)]() mutable
			{
				if (completion)
				{
					completion(std::move(joinSecret));
				}
			}, Scheduler::Pipeline::MAIN);
		};

		const bool isDiscordId = !discordUserId.empty() && discordUserId.size() <= 32 && std::ranges::all_of(discordUserId, [](const unsigned char character)
		{
			return std::isdigit(character) != 0;
		});

		if (!isDiscordId || !IsZwnetPartyId(partyId))
		{
			completeOnMain(std::nullopt);
			return;
		}

		Scheduler::Once([discordUserId, partyId, completeOnMain = std::move(completeOnMain)]() mutable
		{
			std::optional<std::string> joinSecret;

			if (isSocialActive && TryRefreshSocialFriends())
			{
				std::string playerId;

				{
					std::lock_guard _(socialMutex);

					const auto found = std::ranges::find_if(socialFriends, [&discordUserId](const SocialFriend& candidate)
					{
						return candidate.discordId == discordUserId;
					});

					if (found != socialFriends.end())
					{
						playerId = found->id;
					}
				}

				std::optional<nlohmann::json> response;

				if (IsPublicGuid(playerId))
				{
					response = SocialApiRequest("POST", "/zwnet/parties/" + partyId + "/join-capability", { { "player_id", playerId } });
				}

				if (response && response->is_object())
				{
					auto candidate = SocialJsonString(*response, "join_secret");
					constexpr std::string_view prefix = "zwnet-cap:";

					const bool isSecret = candidate.starts_with(prefix) && candidate.size() == prefix.size() + 43
						&& std::ranges::all_of(candidate.substr(prefix.size()), [](const unsigned char character)
						{
							return std::isalnum(character) != 0 || character == '-' || character == '_';
						});

					if (isSecret)
					{
						joinSecret = std::move(candidate);
					}
				}
			}

			completeOnMain(std::move(joinSecret));
		}, Scheduler::Pipeline::ASYNC);
	}

	void Friends::UpdateRank()
	{
		static std::optional<int> levelValue;

		const int experience = Game::Live_GetXp(0);
		const int prestige = Game::Live_GetPrestige(0);
		const int level = (experience & 0xFFFFFF) | ((prestige & 0xFF) << 24);

		if (levelValue.has_value() && levelValue.value() == level)
		{
			return;
		}

		levelValue.emplace(level);

		SetPresence("iw4x_experience", std::to_string(experience));
		SetPresence("iw4x_prestige", std::to_string(prestige));
		UpdateState();
	}

	void Friends::UpdateFriends()
	{
		std::lock_guard _(mutex);

		isLoggedOn = ::Steam::Proxy::SteamUser_ && ::Steam::Proxy::SteamUser_->BLoggedOn();

		if (!::Steam::Proxy::SteamFriends)
		{
			return;
		}

		if (Game::Sys_IsMainThread())
		{
			Game::UI_UpdateArenas();
		}

		const int count = ::Steam::Proxy::SteamFriends->GetFriendCount(friendFlagImmediate);

		Proto::Friends::List list;
		list.ParseFromString(Utils::IO::ReadFile("players/friends.dat"));

		std::vector<Friend> steamFriends;

		for (int i = 0; i < count; ++i)
		{
			const ::Steam::SteamID id = ::Steam::Proxy::SteamFriends->GetFriendByIndex(i, friendFlagImmediate);

			Friend entry{};
			entry.userId = id;
			entry.server.SetType(Game::NA_BAD);

			for (const auto& storedFriend : list.friends())
			{
				if (entry.userId.bits == std::strtoull(storedFriend.steamid().data(), nullptr, 16))
				{
					entry.playerName = storedFriend.name();
					entry.experience = static_cast<int>(storedFriend.experience());
					entry.prestige = static_cast<int>(storedFriend.prestige());
					entry.guid.bits = std::strtoull(storedFriend.guid().data(), nullptr, 16);
					break;
				}
			}

			const auto oldEntry = std::find_if(friendsList.begin(), friendsList.end(), [id](const Friend& candidate)
			{
				return candidate.userId.bits == id.bits;
			});

			if (oldEntry != friendsList.end())
			{
				entry = *oldEntry;
			}
			else
			{
				friendsList.push_back(entry);
			}

			steamFriends.push_back(entry);
		}

		for (auto i = friendsList.begin(); i != friendsList.end();)
		{
			const ::Steam::SteamID id = i->userId;

			const auto steamEntry = std::find_if(steamFriends.begin(), steamFriends.end(), [id](const Friend& candidate)
			{
				return candidate.userId.bits == id.bits;
			});

			if (steamEntry == steamFriends.end())
			{
				i = friendsList.erase(i);
				continue;
			}

			*i = *steamEntry;
			++i;

			UpdateUserInfo(id);
			RequestPresence(id);
		}
	}

	unsigned int Friends::GetFriendCount()
	{
		return static_cast<unsigned int>(friendsList.size());
	}

	const char* Friends::GetFriendText(unsigned int index, int column)
	{
		std::lock_guard _(mutex);

		if (index >= friendsList.size())
		{
			return "";
		}

		const auto& user = friendsList[index];

		switch (column)
		{
		case 0:
			return Utils::String::VA("%i", Game::CL_GetRankForXp(user.experience) + 1);

		case 1:
			if (user.playerName.empty() || user.name == user.playerName)
			{
				return Utils::String::VA("%s", user.name.data());
			}

			return Utils::String::VA("%s ^7(%s^7)", user.name.data(), user.playerName.data());

		case 2:
			if (!user.online)
			{
				return "Offline";
			}

			if (!IsOnline(user.lastTime))
			{
				return "Online";
			}

			if (user.server.GetType() == Game::NA_BAD)
			{
				return "Playing IW4x";
			}

			if (user.serverName.empty())
			{
				return Utils::String::VA("Playing on %s", user.server.GetCString());
			}

			return Utils::String::VA("Playing %s on %s", Localization::LocalizeMapName(user.mapname.data()), user.serverName.data());

		default:
			return "";
		}
	}

	void Friends::SelectFriend(unsigned int index)
	{
		std::lock_guard _(mutex);

		if (index >= friendsList.size())
		{
			return;
		}

		currentFriend = index;
	}

	int Friends::GetGame(::Steam::SteamID user)
	{
		::Steam::FriendGameInfo info{};

		if (!::Steam::Proxy::SteamFriends || !::Steam::Proxy::SteamFriends->GetFriendGamePlayed(user, &info))
		{
			return 0;
		}

		return static_cast<int>(info.m_gameID.appID);
	}

	bool Friends::IsInvisible()
	{
		return initialState == personaStateInvisible;
	}

	void Friends::UpdateTimeStamp()
	{
		if (::Steam::Proxy::SteamUtils)
		{
			SetPresence("iw4x_playing", std::to_string(::Steam::Proxy::SteamUtils->GetServerRealTime()));
		}

		SetPresence("iw4x_guid", Utils::String::VA("%llX", ::Steam::User::LocalId().bits));
	}

	bool Friends::IsOnline(std::uint64_t timeStamp)
	{
		if (!::Steam::Proxy::SteamUtils)
		{
			return false;
		}

		constexpr std::uint64_t duration = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::minutes(5)).count();

		return (::Steam::Proxy::SteamUtils->GetServerRealTime() - timeStamp) < duration;
	}

	void Friends::StoreFriendsList()
	{
		std::lock_guard _(mutex);

		if (!isLoggedOn)
		{
			return;
		}

		Proto::Friends::List list;

		for (const auto& entry : friendsList)
		{
			Proto::Friends::Friend* friendEntry = list.add_friends();

			friendEntry->set_steamid(Utils::String::VA("%llX", entry.userId.bits));
			friendEntry->set_guid(Utils::String::VA("%llX", entry.guid.bits));
			friendEntry->set_name(entry.playerName);
			friendEntry->set_experience(static_cast<std::uint32_t>(entry.experience));
			friendEntry->set_prestige(static_cast<std::uint32_t>(entry.prestige));
		}

		Utils::IO::WriteFile("players/friends.dat", list.SerializeAsString());
	}

	Game::Material* Friends::CreateAvatar(::Steam::SteamID user)
	{
		if (!::Steam::Proxy::SteamUtils || !::Steam::Proxy::SteamFriends)
		{
			return nullptr;
		}

		const int index = ::Steam::Proxy::SteamFriends->GetMediumFriendAvatar(user);

		std::uint32_t width = 0;
		std::uint32_t height = 0;

		if (index <= 0 || !::Steam::Proxy::SteamUtils->GetImageSize(index, &width, &height) || !width || !height)
		{
			return nullptr;
		}

		Game::GfxImage* image = Materials::CreateImage(Utils::String::VA("texture_%llX", user.bits), width, height, 1, 0x1000003, D3DFMT_A8R8G8B8);

		if (!image->texture.map)
		{
			Materials::DeleteImage(image);
			return nullptr;
		}

		D3DLOCKED_RECT lockedRect;
		image->texture.map->LockRect(0, &lockedRect, nullptr, 0);

		auto* const buffer = static_cast<unsigned char*>(lockedRect.pBits);
		::Steam::Proxy::SteamUtils->GetImageRGBA(index, buffer, static_cast<int>(width * height * 4));

		for (std::uint32_t i = 0; i < width * height * 4; i += 4)
		{
			std::swap(buffer[i + 0], buffer[i + 2]);
		}

		buffer[3] = 0;
		buffer[(width - 1) * 4 + 3] = 0;
		buffer[((height - 1) * width * 4) + 3] = 0;
		buffer[((height - 1) * width * 4) + (width - 1) * 4 + 3] = 0;

		image->texture.map->UnlockRect(0);

		return Materials::Create(Utils::String::VA("avatar_%llX", user.bits), image);
	}

	static void ShowFriendsList()
	{
		Command::Execute("openmenu popup_friends", true);
	}

	bool Friends::TryInstallHooks()
	{
		const std::uintptr_t parseServerInfoCalls[] = { CG_Init_CG_ParseServerInfoCall, CL_RestartCGame_CG_ParseServerInfoCall };

		if (!Utils::Hook::BranchesTo(Steam_ShowFriendsList_Thunk, Steam_ShowFriendsList, HOOK_JUMP)
			|| !Utils::Hook::MatchesBytes(CL_ClientIsInMyParty, clientIsInMyPartyEntry, sizeof(clientIsInMyPartyEntry))
			|| !Utils::Hook::BranchesTo(parseServerInfoCalls[0], CG_ParseServerInfo, HOOK_CALL)
			|| !Utils::Hook::BranchesTo(parseServerInfoCalls[1], CG_ParseServerInfo, HOOK_CALL))
		{
			Logger::Error("friends: a site does not read as expected, no friends\n");
			return false;
		}

		bool isSeated = showFriendsListHook.Initialize(Steam_ShowFriendsList_Thunk, reinterpret_cast<void*>(ShowFriendsList), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = clientInPartyHook.Initialize(CL_ClientIsInMyParty, reinterpret_cast<void*>(IsClientInParty), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(parseServerInfoCalls); ++i)
		{
			isSeated = parseServerInfoHooks[i].Initialize(parseServerInfoCalls[i], reinterpret_cast<void*>(CG_ParseServerInfo_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			showFriendsListHook.Uninstall();
			clientInPartyHook.Uninstall();

			for (auto& hook : parseServerInfoHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("friends: could not seat every hook, no friends\n");
			return false;
		}

		showFriendsListHook.Quick();
		clientInPartyHook.Quick();

		for (auto& hook : parseServerInfoHooks)
		{
			hook.Quick();
		}

		return true;
	}

	Friends::Friends()
	{
		isLoggedOn = false;

		if (Dedicated::IsEnabled())
		{
			return;
		}

		if (!TryInstallHooks())
		{
			return;
		}

		::Steam::Proxy::RegisterCallback(336, [](void* data)
		{
			const auto* const update = static_cast<FriendRichPresenceUpdate*>(data);
			UpdateUserInfo(update->m_steamIDFriend);
		});

		::Steam::Proxy::RegisterCallback(304, [](void* data)
		{
			const auto* const state = static_cast<PersonaStateChange*>(data);
			RequestPresence(state->m_ulSteamID);
		});

		Events::OnSteamDisconnect(ClearServer);

		UIScript::Add("LoadFriends", [](const UIScript::Token&)
		{
			UpdateFriends();
		});

		UIScript::Add("RefreshFriends", [](const UIScript::Token&)
		{
			UpdateFriends();

			RunSocialTask("Refreshing ZW3 friends...", []() -> std::string
			{
				const bool areFriendsRefreshed = TryRefreshSocialFriends();
				const bool areRequestsRefreshed = TryRefreshIncomingRequests();

				if (areFriendsRefreshed && areRequestsRefreshed)
				{
					return "Friends and requests updated.";
				}

				if (areFriendsRefreshed)
				{
					return "Friends updated; requests are temporarily unavailable.";
				}

				return "The ZW3 friends service is unavailable.";
			});
		});

		UIScript::Add("LoadFriendRequests", [](const UIScript::Token&)
		{
			RunSocialTask("Refreshing friend requests...", []() -> std::string
			{
				if (TryRefreshIncomingRequests())
				{
					return "Friend requests updated.";
				}

				return "Friend requests are temporarily unavailable.";
			});
		});

		UIScript::Add("AddFriendFromDvar", [](const UIScript::Token&)
		{
			const auto target = ToLowerAscii(Dvar::Var("ui_social_friend_guid").Get<std::string>());

			if (!IsPublicGuid(target))
			{
				SetSocialUi("Enter the 16-character public ZW3 GUID.", false);
				return;
			}

			RunSocialTask("Sending friend request...", [target]() -> std::string
			{
				if (!SocialApiRequest("POST", "/social/friends/request", { { "guid", target } }, true))
				{
					return "The friend request could not be sent.";
				}

				Scheduler::Once([]
				{
					if (isSocialActive)
					{
						Dvar::Var("ui_social_friend_guid").Set("");
					}
				}, Scheduler::Pipeline::MAIN);

				TryRefreshIncomingRequests();
				return "Friend request sent.";
			});
		});

		UIScript::Add("RefreshSelectedLobbyPlayer", [](const UIScript::Token&)
		{
			const auto target = SelectedLobbyPlayerGuid();

			if (!target)
			{
				Dvar::Var("zwnet_selected_player_relationship").Set("UNAVAILABLE");
				return;
			}

			const auto cachedRelationship = LobbyPlayerRelationship(*target);
			Dvar::Var("zwnet_selected_player_relationship").Set(cachedRelationship);

			if (cachedRelationship == "SELF")
			{
				return;
			}

			Scheduler::Once([target = *target, cachedRelationship]
			{
				const bool areFriendsRefreshed = TryRefreshSocialFriends();
				const bool areRequestsRefreshed = TryRefreshIncomingRequests();

				if (!areFriendsRefreshed && !areRequestsRefreshed && cachedRelationship == "AVAILABLE")
				{
					SetLobbyPlayerRelationship(target, "UNAVAILABLE");
					return;
				}

				SetLobbyPlayerRelationship(target, LobbyPlayerRelationship(target));
			}, Scheduler::Pipeline::ASYNC);
		});

		UIScript::Add("AddSelectedLobbyPlayer", [](const UIScript::Token&)
		{
			const auto target = SelectedLobbyPlayerGuid();

			if (!target)
			{
				SetSocialUi("The selected player identity is unavailable.", false);
				return;
			}

			const auto relationship = LobbyPlayerRelationship(*target);
			Dvar::Var("zwnet_selected_player_relationship").Set(relationship);

			static const std::unordered_map<std::string, std::string> refusals =
			{
				{ "SELF", "You cannot add your own profile." },
				{ "FRIEND", "This player is already your friend." },
				{ "INCOMING_REQUEST", "This player already sent you a friend request." },
				{ "PENDING", "Your friend request is already pending." },
			};

			const auto refusal = refusals.find(relationship);

			if (refusal != refusals.end())
			{
				SetSocialUi(refusal->second, false);
				return;
			}

			if (isSocialBusy.load())
			{
				SetSocialUi("A social request is already running.", true);
				return;
			}

			Dvar::Var("zwnet_selected_player_relationship").Set("PENDING");

			RunSocialTask("Sending friend request...", [target = *target]() -> std::string
			{
				if (!SocialApiRequest("POST", "/social/friends/request", { { "guid", target } }, true))
				{
					{
						std::lock_guard _(socialMutex);
						pendingOutgoingFriends.erase(target);
					}

					SetLobbyPlayerRelationship(target, "AVAILABLE");
					return "The friend request could not be sent.";
				}

				{
					std::lock_guard _(socialMutex);
					pendingOutgoingFriends.insert(target);
				}

				SetLobbyPlayerRelationship(target, "PENDING");
				return "Friend request sent.";
			});
		});

		UIScript::Add("AcceptFriendRequest", [](const UIScript::Token&)
		{
			const auto request = SelectedIncomingRequest();

			if (!request)
			{
				SetSocialUi("Select an incoming friend request first.", false);
				return;
			}

			RunSocialTask("Accepting friend request...", [request]() -> std::string
			{
				if (!SocialApiRequest("POST", "/social/friends/accept", { { "request_id", request->requestId } }, true))
				{
					return "The friend request could not be accepted.";
				}

				const bool areFriendsRefreshed = TryRefreshSocialFriends();
				const bool areRequestsRefreshed = TryRefreshIncomingRequests();

				if (areFriendsRefreshed && areRequestsRefreshed)
				{
					return "Friend request accepted.";
				}

				return "Request accepted; the lists will refresh shortly.";
			});
		});

		UIScript::Add("RemoveSelectedFriend", [](const UIScript::Token&)
		{
			const auto user = SelectedSocialFriend();

			if (!user)
			{
				SetSocialUi("Select a ZW3 friend first.", false);
				return;
			}

			RunSocialTask("Removing friend...", [user]() -> std::string
			{
				if (user->discordId.empty())
				{
					return "The Stats friend response has no removable Discord identity.";
				}

				if (!SocialApiRequest("POST", "/social/friends/remove", { { "discord_user_id", user->discordId } }, true))
				{
					return "The friend could not be removed.";
				}

				if (TryRefreshSocialFriends())
				{
					return "Friend removed.";
				}

				return "Friend removed; the list will refresh shortly.";
			});
		});

		UIScript::Add("JoinSelectedFriend", [](const UIScript::Token&)
		{
			const auto user = SelectedSocialFriend();

			if (!user)
			{
				SetSocialUi("Select a ZW3 friend first.", false);
				return;
			}

			if (!user->isJoinable || !IsZwnetPartyId(user->zwnetPartyId))
			{
				SetSocialUi("This friend's ZW3 party is not currently joinable.", false);
				return;
			}

			RunSocialTask("Joining friend's ZW3 party...", [user]() -> std::string
			{
				const auto result = JoinFriendParty(user->id);

				Scheduler::Once([]
				{
					if (isSocialActive)
					{
						Command::Execute("closemenu popup_friends", false);
					}
				}, Scheduler::Pipeline::MAIN);

				return result;
			});
		});

		UIScript::Add("InviteSelectedFriend", [](const UIScript::Token&)
		{
			const auto user = SelectedSocialFriend();

			if (!user)
			{
				SetSocialUi("Select a ZW3 friend first.", false);
				return;
			}

			const std::string visibility = SocialPartyVisibilityName(std::clamp(Dvar::Var("partyPrivacy").Get<int>(), 0, 2));

			RunSocialTask("Preparing party invitation...", [user, visibility]() -> std::string
			{
				auto party = SocialApiRequest("GET", "/zwnet/parties/current");

				if (!party || party->is_null())
				{
					party = SocialApiRequest("POST", "/zwnet/parties/create", { { "visibility", visibility } }, true);
				}

				if (!party || !party->is_object())
				{
					return SocialFailure("A party could not be created or loaded.");
				}

				auto partyId = SocialJsonString(*party, "id");

				if (partyId.empty())
				{
					partyId = SocialJsonString(*party, "partyId");
				}

				if (!IsSocialPartyId(partyId))
				{
					return "The party response was invalid.";
				}

				auto currentVisibility = SocialJsonString(*party, "visibility");

				if (currentVisibility.empty())
				{
					currentVisibility = "OPEN";
				}

				if (currentVisibility != visibility)
				{
					const auto updated = SocialApiRequest("POST", "/zwnet/parties/" + partyId + "/set-visibility", { { "visibility", visibility } }, true);

					if (!updated || !updated->is_object() || updated->contains("error"))
					{
						return SocialFailure("The party privacy setting could not be synchronized.");
					}
				}

				if (SocialApiRequest("POST", "/zwnet/parties/" + partyId + "/invite", { { "player_id", user->id } }, true))
				{
					return "Party invitation sent.";
				}

				return SocialFailure("The party invitation could not be sent.");
			});
		});

		UIScript::Add("ConfirmGameInvite", [](const UIScript::Token&)
		{
			const auto invite = SelectedPartyInvite();

			if (!invite)
			{
				ClosePartyInvitePopup();
				return;
			}

			RunSocialTask("Joining the ZW3 party...", [invite]() -> std::string
			{
				const auto response = SocialApiRequest("POST", "/zwnet/parties/" + invite->partyId + "/accept", nlohmann::json::object(), true);

				if (!response || !response->is_object())
				{
					const auto message = SocialFailure("The party invitation could not be accepted.");

					if (HasSocialFailureResolvedInvite())
					{
						MarkPartyInviteHandled(*invite);
						ClosePartyInvitePopup();
					}

					return message;
				}

				MarkPartyInviteHandled(*invite);
				ZWNet::ResumeParty(*response);

				Scheduler::Once([]
				{
					if (!isSocialActive)
					{
						return;
					}

					Command::Execute("closemenu popup_social_game_invite", false);
					Command::Execute("closemenu popup_friends", false);
				}, Scheduler::Pipeline::MAIN);

				return "Party joined.";
			});
		});

		UIScript::Add("DeclineGameInvite", [](const UIScript::Token&)
		{
			const auto invite = SelectedPartyInvite();

			if (!invite)
			{
				ClosePartyInvitePopup();
				return;
			}

			RunSocialTask("Declining the party invitation...", [invite]() -> std::string
			{
				if (!SocialApiRequest("POST", "/zwnet/parties/" + invite->partyId + "/decline", nlohmann::json::object(), true))
				{
					return SocialFailure("The party invitation could not be declined.");
				}

				MarkPartyInviteHandled(*invite);
				ClosePartyInvitePopup();
				return "Party invitation declined.";
			});
		});

		UIScript::Add("CloseGameInvitePopup", [](const UIScript::Token&)
		{
			const auto invite = SelectedPartyInvite();

			if (!invite)
			{
				return;
			}

			MarkPartyInviteHandled(*invite);

			Scheduler::Once([invite]
			{
				SocialApiRequest("POST", "/zwnet/parties/" + invite->partyId + "/decline", nlohmann::json::object(), true);
			}, Scheduler::Pipeline::ASYNC);
		});

		UIScript::Add("JoinFriend", [](const UIScript::Token&)
		{
			std::string selectedGuid;
			std::optional<Network::Address> server;

			{
				std::lock_guard _(mutex);

				if (currentFriend >= friendsList.size())
				{
					return;
				}

				const auto& selected = friendsList[currentFriend];
				selectedGuid = std::format("{:016x}", selected.guid.bits);

				if (selected.online && selected.server.GetType() != Game::NA_BAD)
				{
					server = selected.server;
				}
			}

			std::optional<SocialFriend> socialUser;

			{
				std::lock_guard _(socialMutex);

				const auto found = std::ranges::find_if(socialFriends, [&selectedGuid](const SocialFriend& candidate)
				{
					return candidate.guid == selectedGuid;
				});

				if (found != socialFriends.end())
				{
					socialUser = *found;
				}
			}

			if (socialUser && IsZwnetPartyId(socialUser->zwnetPartyId))
			{
				RunSocialTask("Joining friend's ZW3 party...", [socialUser]() -> std::string
				{
					return JoinFriendParty(socialUser->id);
				});

				return;
			}

			if (server)
			{
				Scheduler::Once([server = *server]
				{
					if (isSocialActive)
					{
						Party::Connect(server);
					}
				}, Scheduler::Pipeline::MAIN);
			}
			else
			{
				Game::SND_PlayLocalSoundAliasByName(0, "exit_prestige", 0);
			}
		});

		Scheduler::Loop([]
		{
			static Utils::Time::Interval timeInterval;
			static Utils::Time::Interval sortInterval;
			static Utils::Time::Interval stateInterval;
			static Utils::Time::Interval zombieRankInterval;

			if (Game::LiveStorage_DoWeHaveStats(0))
			{
				UpdateRank();
			}

			if (zombieRankInterval.Elapsed(std::chrono::seconds(2)))
			{
				zombieRankInterval.Update();
				UpdateZombieRankPresence();
			}

			if (timeInterval.Elapsed(std::chrono::minutes(2)))
			{
				timeInterval.Update();
				UpdateTimeStamp();
				UpdateState();
			}

			if (stateInterval.Elapsed(std::chrono::seconds(5)))
			{
				stateInterval.Update();

				if (shouldUpdate)
				{
					shouldUpdate = false;
					UpdateState();
				}
			}

			if (sortInterval.Elapsed(std::chrono::seconds(1)))
			{
				sortInterval.Update();

				if (shouldSort)
				{
					shouldSort = false;
					SortList(true);
				}
			}
		}, Scheduler::Pipeline::CLIENT);

		UIFeeder::Add(61.0f, GetFriendCount, GetFriendText, SelectFriend);
		UIFeeder::Add(socialFriendFeeder, GetSocialFriendCount, GetSocialFriendText, SelectSocialFriend);
		UIFeeder::Add(socialRequestFeeder, GetIncomingRequestCount, GetIncomingRequestText, SelectIncomingRequest);

		Scheduler::Loop([]
		{
			if (!isSocialActive || isSocialBusy)
			{
				return;
			}

			auto* const menu = Game::Menus_FindByName(Game::uiContext, "popup_friends");

			if (!menu || !Game::Menu_IsVisible(Game::uiContext, menu) || isSocialRefreshBusy.exchange(true))
			{
				return;
			}

			Scheduler::Once([]
			{
				if (isSocialActive)
				{
					TryRefreshSocialFriends();
					TryRefreshIncomingRequests();
				}

				isSocialRefreshBusy = false;
			}, Scheduler::Pipeline::ASYNC);
		}, Scheduler::Pipeline::MAIN, 15s);

		Scheduler::Loop([]
		{
			try
			{
				PollPartyInvites();
			}
			catch (const std::exception&)
			{
				isPartyInvitePollBusy = false;
			}
		}, Scheduler::Pipeline::ASYNC, 5s);

		Scheduler::OnShutdown([]
		{
			isSocialActive = false;
			isSocialBusy = false;
			StoreFriendsList();
			ClearPresence("iw4x_server");
			ClearPresence("iw4x_playing");
		});

		Scheduler::OnGameInitialized([]
		{
			Dvar::Register("ui_social_status_message", "ZW3 social is ready.", Game::DVAR_NONE, "Stable, non-technical status shown by the ZW3 social menu");
			Dvar::Register("ui_social_invite_busy", false, Game::DVAR_NONE, "Prevents duplicate ZW3 social menu actions");
			Dvar::Register("ui_social_friend_guid", "", Game::DVAR_NONE, "Public ZW3 GUID entered for a friend request");
			Dvar::Register("ui_social_own_guid", "", Game::DVAR_ROM, "Local public ZW3 GUID shown in the social menu");
			Dvar::Register("ui_social_game_invite_message", "", Game::DVAR_NONE, "The ZW3 party invitation being shown");
			Dvar::Register("ui_social_game_invite_details", "", Game::DVAR_NONE, "The size of the ZW3 party in the invitation");

			isSocialActive = true;
			Dvar::Var("ui_social_own_guid").Set(std::format("{:016x}", Auth::GetKeyHash()));

			if (::Steam::Proxy::SteamFriends && ::Steam::Proxy::SteamUser_)
			{
				initialState = ::Steam::Proxy::SteamFriends->GetFriendPersonaState(::Steam::Proxy::SteamUser_->GetSteamID());
			}

			if ((cl_anonymous.Get<bool>() || IsInvisible()) && ::Steam::Proxy::SteamFriends)
			{
				::Steam::Proxy::SteamFriends->ClearRichPresence();
			}

			UpdateTimeStamp();
			UpdateName();
			UpdateZombieRankPresence();
			UpdateState();

			UpdateFriends();
		}, Scheduler::Pipeline::MAIN);
	}

	Friends::~Friends()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		StoreFriendsList();

		::Steam::Proxy::UnregisterCallback(336);
		::Steam::Proxy::UnregisterCallback(304);

		std::lock_guard _(mutex);
		friendsList.clear();
	}
}
