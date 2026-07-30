#include <Utils/WebIO.hpp>
#include <wincrypt.h>

#include "ZWNet.hpp"
#include "Auth.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
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
		if (!result || result->contains("error"))
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
		Request("POST", "/social/presence", {{"status", "MAIN_MENU"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
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
		SetState("SEARCH_STARTING");
		auto party = Request("GET", "/zwnet/parties/current");
		if (!party || party->is_null()) party = Request("POST", "/zwnet/parties/create", nlohmann::json::object());
		if (!party || party->is_null() || party->contains("error"))
		{
			SearchingState() = false;
			SetState("ERROR", "ZWNET_PARTY_FAILED");
			return;
		}
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

	void ZWNet::UpdateLobbyDvars(const nlohmann::json& party)
	{
		if (party.is_null() || !party.is_object()) return;
		const auto partyId = JsonString(party, "id");
		const auto leaderId = JsonString(party, "leader_id");
		const auto state = JsonString(party, "state", "IDLE");
		const auto members = party.value("members", nlohmann::json::array());
		std::string owner;
		std::string currentPlayerId;
		{ std::lock_guard lock(StateMutex()); currentPlayerId = CurrentPlayerIdState(); }
		for (const auto& member : members) if (JsonString(member, "player_id") == leaderId) owner = JsonString(member, "display_name");
		{
			std::lock_guard lock(StateMutex());
			CurrentPartyIdState() = partyId;
		}
		const auto stateText = FriendlyStateText(state);
		Scheduler::Once([partyId, leaderId, state, stateText, owner, members, currentPlayerId]
		{
			if (!ActiveState()) return;
			Dvar::Var("zwnet_lobby_active").set(true);
			Dvar::Var("zwnet_lobby_party_id").set(partyId);
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
					Dvar::Var(prefix + "_name").set(JsonString(member, "display_name"));
					Dvar::Var(prefix + "_role").set(JsonString(member, "player_id") == leaderId ? "PARTY LEADER" : "MEMBER");
					Dvar::Var(prefix + "_ready").set(ready);
					allReady = allReady && ready;
					if (JsonString(member, "player_id") == currentPlayerId) selfReady = ready;
				}
				else
				{
					Dvar::Var(prefix + "_name").set("");
					Dvar::Var(prefix + "_role").set("");
					Dvar::Var(prefix + "_ready").set(false);
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
		SearchingState() = false;
		UpdateLobbyDvars(party);
		SetState("IN_PARTY");
	}

	void ZWNet::EnterLobby(std::string map)
	{
		auto party = Request("GET", "/zwnet/parties/current");
		if (!party || party->is_null()) party = Request("POST", "/zwnet/parties/create", nlohmann::json::object());
		if (!party || party->contains("error")) { SetState("ERROR", "ZWNET_PARTY_FAILED"); return; }
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
		const auto party = Request("GET", "/zwnet/parties/current");
		if (party && !party->is_null() && !party->contains("error"))
		{
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

	void ZWNet::LeaveParty()
	{
		const auto result = Request("POST", "/zwnet/parties/leave");
		if (result && !result->contains("error"))
		{
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
		};
		std::array<LobbyMemberSnapshot, 4> members{};
		std::size_t memberCount{};
		for (const auto& member : *membersIt)
		{
			if (memberCount >= members.size()) break;
			if (!member.is_object()) continue;
			auto& snapshot = members[memberCount++];
			snapshot.playerId = JsonString(member, "player_id");
			snapshot.displayName = JsonString(member, "display_name");
			snapshot.role = JsonString(member, "role", "MEMBER");
			const auto readyIt = member.find("ready");
			if (readyIt != member.end())
			{
				if (readyIt->is_boolean()) snapshot.ready = readyIt->get<bool>();
				else if (readyIt->is_number_integer()) snapshot.ready = readyIt->get<std::int64_t>() != 0;
			}
		}
		const auto stateText = FriendlyStateText(JsonString(status, "state", "WAITING_FOR_READY"));
		std::string currentPlayerId;
		{ std::lock_guard lock(StateMutex()); currentPlayerId = CurrentPlayerIdState(); }
		Scheduler::Once([members = std::move(members), memberCount, stateText, currentPlayerId]
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
					Dvar::Var(std::string{prefix} + "_role").set(member.role);
					Dvar::Var(std::string{prefix} + "_ready").set(member.ready);
					allReady = allReady && member.ready;
					if (member.playerId == currentPlayerId) selfReady = member.ready;
				}
				else
				{
					Dvar::Var(std::string{prefix} + "_name").set("");
					Dvar::Var(std::string{prefix} + "_role").set("");
					Dvar::Var(std::string{prefix} + "_ready").set(false);
				}
			}
			Dvar::Var("zwnet_lobby_self_ready").set(selfReady);
			Dvar::Var("zwnet_all_ready").set(allReady);
			Dvar::Var("zwnet_lobby_can_start").set(false);
		}, Scheduler::Pipeline::MAIN);
	}

	void ZWNet::UpdatePresence()
	{
		if (!ActiveState() || ClosingOnlineSessionState()) return;
		std::string matchId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
		}
		const auto status = !matchId.empty() ? "CONNECTING" : SearchingState() ? "SEARCHING" : "MAIN_MENU";
		Request("POST", "/social/presence", {{"status", status}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
	}

	void ZWNet::ToggleReady(const bool ready)
	{
		std::string partyId;
		{ std::lock_guard lock(StateMutex()); partyId = CurrentPartyIdState(); }
		if (partyId.empty()) return;
		const auto result = Request("POST", "/zwnet/parties/" + partyId + (ready ? "/unready" : "/ready"));
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

	void ZWNet::CloseOnlineSession(const bool shuttingDown)
	{
		if (!ActiveState() || ClosingOnlineSessionState().exchange(true)) return;
		const auto closingGuard = gsl::finally([] { ClosingOnlineSessionState() = false; });
		ServerJoinTransitionState() = false;
		std::string matchId;
		{
			std::lock_guard lock(StateMutex());
			matchId = CurrentMatchIdState();
		}
		auto body = nlohmann::json::object();
		if (!matchId.empty()) body["match_id"] = matchId;
		Request("POST", "/zwnet/matchmaking/disconnect", body);
		Request("POST", "/social/presence", {{"status", "OFFLINE"}, {"sequence", NextPresenceSequence()}, {"joinable", false}});
		SearchingState() = false;
		{
			std::lock_guard lock(StateMutex());
			CurrentProposalIdState().clear();
			CurrentMatchIdState().clear();
		}
		if (shuttingDown) return;
		SetState("IDLE");
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
			Dvar::Var("zwnet_server_endpoint").set("");
			Dvar::Var("zwnet_server_status").set("NOT ASSIGNED");
			Dvar::Var("zwnet_join_status").set("WAITING IN LOBBY");
		}, Scheduler::Pipeline::MAIN);
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
		const auto party = Request("GET", "/zwnet/parties/current");
		const auto status = Request("GET", "/zwnet/matchmaking/status");
		if (!status || status->contains("error"))
		{
			if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
			return;
		}
		if (status->contains("lobby") && status->at("lobby").is_object()) UpdateMatchLobbyDvars(*status);
		else if (party && !party->is_null() && !party->contains("error")) UpdateLobbyDvars(*party);
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
		if (status->contains("match_id") && state == "CONNECTING" && joinCountdown <= 0)
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
		Dvar::Register<const char*>("zwnet_lobby_owner", "", Game::DVAR_NONE, "Current party owner");
		Dvar::Register<int>("zwnet_lobby_member_count", 0, 0, 4, Game::DVAR_NONE, "Current party size");
		Dvar::Register<const char*>("zwnet_lobby_status_text", "IDLE", Game::DVAR_NONE, "Current party state");
		Dvar::Register<bool>("zwnet_lobby_can_start", false, Game::DVAR_NONE, "Private match can start");
		Dvar::Register<bool>("zwnet_lobby_self_ready", false, Game::DVAR_NONE, "Local party ready state");
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
		constexpr std::array memberReady
		{
			"zwnet_lobby_member_0_ready", "zwnet_lobby_member_1_ready", "zwnet_lobby_member_2_ready", "zwnet_lobby_member_3_ready"
		};
		for (std::size_t i = 0; i < memberNames.size(); ++i)
		{
			Dvar::Register<const char*>(memberNames[i], "", Game::DVAR_NONE, "Party member name");
			Dvar::Register<const char*>(memberRoles[i], "", Game::DVAR_NONE, "Party member role");
			Dvar::Register<bool>(memberReady[i], false, Game::DVAR_NONE, "Party member ready state");
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
		Dvar::Register<const char*>("zwnet_server_status", "NOT ASSIGNED", Game::DVAR_NONE, "ZW3 server assignment status");
		Dvar::Register<const char*>("zwnet_join_status", "WAITING IN LOBBY", Game::DVAR_NONE, "ZW3 join status");
		Dvar::Register<int>("zwnet_join_countdown", 0, 0, 30, Game::DVAR_NONE, "Synchronized ZW3 join countdown");
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
		Command::Add("zwnet_quickplay", [] { EnqueueAsync([] { StartQuickPlay(); }); });
		Command::Add("zwnet_cancel", [] { EnqueueAsync([] { CancelSearch(); }); });
		Command::Add("zwnet_logout", []
		{
			EnqueueAsync([]
			{
				CloseOnlineSession(false);
				Request("POST", "/social/client/logout");
				if (!ActiveState()) return;
				ClearSession();
				SetState("OFFLINE");
			});
		});
		UIScript::Add("ZWNetQuickPlay", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_quickplay", false); });
		UIScript::Add("ZWNetCancel", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_cancel", false); });
		UIScript::Add("ZWNET_CloseOnlineSession", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { CloseOnlineSession(false); }); });
		UIScript::Add("ZWNetLogin", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_login", false); });
		UIScript::Add("ZWNetRegister", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { Command::Execute("zwnet_register", false); });
		UIScript::Add("ZWNetCopyGuid", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			if (!CopyPublicGuidToClipboard()) SetState("ERROR", "ZWNET_GUID_COPY_FAILED");
		});
		UIScript::Add("ZWNET_EnterPrivateLobby", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			const auto map = Dvar::Var("ui_mapname").get<std::string>();
			EnqueueAsync([map] { EnterLobby(map); });
		});
		UIScript::Add("ZWNET_RefreshLobby", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { RefreshLobby(); }); });
		UIScript::Add("ZWNET_LeaveParty", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*) { EnqueueAsync([] { LeaveParty(); }); });
		UIScript::Add("ZWNET_ToggleReady", []([[maybe_unused]] const UIScript::Token&, [[maybe_unused]] const Game::uiInfo_s*)
		{
			const auto ready = Dvar::Var("zwnet_lobby_self_ready").get<bool>();
			EnqueueAsync([ready] { ToggleReady(ready); });
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
			if (wasConnected || hasMatch) EnqueueAsync([] { CloseOnlineSession(false); });
		});
		Events::OnCGameInit([]
		{
			ServerJoinTransitionState() = false;
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
		Scheduler::Loop(UpdateMatchmaking, Scheduler::Pipeline::ASYNC, 3s);
		Scheduler::Loop([]
		{
			if (!ActiveState()) return;
			if (Dvar::Var("zwnet_vote_active").get<bool>())
			{
				const auto seconds = Dvar::Var("zwnet_vote_seconds").get<int>();
				if (seconds > 0) Dvar::Var("zwnet_vote_seconds").set(seconds - 1);
			}
			const auto startSeconds = Dvar::Var("zwnet_start_seconds").get<int>();
			if (startSeconds > 0) Dvar::Var("zwnet_start_seconds").set(startSeconds - 1);
			const auto joinSeconds = Dvar::Var("zwnet_join_countdown").get<int>();
			if (joinSeconds > 0) Dvar::Var("zwnet_join_countdown").set(joinSeconds - 1);
		}, Scheduler::Pipeline::MAIN, 1s);
		Scheduler::Loop(UpdatePresence, Scheduler::Pipeline::ASYNC, 30s);
	}

	void ZWNet::preDestroy()
	{
		if (ActiveState()) CloseOnlineSession(true);
		ActiveState() = false;
		SearchingState() = false;
		ServerJoinTransitionState() = false;
		{
			std::lock_guard lock(StateMutex());
			LoginInFlightState() = false;
		}
		std::lock_guard lock(AsyncTaskMutex());
		AsyncTasks().clear();
	}
}
