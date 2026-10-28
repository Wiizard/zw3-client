#pragma once

#include "UIScript.hpp"
#include "Events.hpp"
#include "Network.hpp"

namespace Components
{
	class ZWNet : public Component
	{
	public:
		ZWNet();

		static bool StoreSession(const std::string& accessToken, const std::string& refreshToken);
		static void ResumeParty(const nlohmann::json& party);
		static void JoinParty(const std::string& partyId);
		static void JoinCapability(const std::string& capability);
		static bool BeginEndpointJoin(const std::string& endpoint);
		static bool BeginManagedReconnect(const std::string& endpoint);
		static bool TryRelayAfterDirectTimeout(const std::string& endpoint);
		static void RefreshManagedConnectTicket(const Network::Address& target);
		static bool TryGetSharedLobbyRank(const std::string& guid, int& level, int& prestige);

		static std::string SessionPath();

	private:
		static bool LoadSession();
		static void ClearSession();
		static std::optional<nlohmann::json> Request(const std::string& method, const std::string& path, const nlohmann::json& body = {});
		static void Refresh();
		static void Login();
		static void CompleteLogin(nlohmann::json requestBody);
		static void BeginOnlineEntry();
		static void CompleteOnlineEntry();
		static void AbandonOnlineSession();
		static void Register();
		static void SetState(const std::string& state, const std::string& error = {});
		static std::optional<nlohmann::json> CurrentOrNewParty();
		static void StartQuickPlay(const std::string& playlistId, std::int64_t playlistRevision, std::uint64_t selectorGeneration, bool isQuickPlay);
		static void RefreshPlaylistCatalog(bool isForced = false);
		static void ClearPlaylistCatalog();
		static void BeginPlaylistSelection();
		static void CancelPlaylistSelection();
		static void HighlightPlaylistSlot(int slot);
		static void ActivatePlaylistSlot(int slot);
		static void ActivateQuickPlay();
		static void ActivatePlaylist(const std::string& playlistId, std::int64_t playlistRevision, bool isQuickPlay);
		static void ChangePlaylistPage(int direction);
		static void AcknowledgePlaylistNotice();
		static void PublishPlaylistCatalog();
		static void SchedulePlaylistCatalogPublish();
		static std::optional<nlohmann::json> PublishPartyContent();
		static std::optional<nlohmann::json> ApplyPartyVisibility(nlohmann::json party);
		static void RefreshPartyVisibility();
		static void CapturePartyPrivacy();
		static void CancelSearch();
		static void CancelMatchmaking();
		static void CloseOnlineSession(bool isShuttingDown, bool isTerminal);
		static void HandleServerDisconnect(bool isTerminal, bool wasMatchmaking, Events::NativeDisconnectError nativeError, bool wasInGame);
		static bool ReturnToMatchmakingLobby(bool requireAssignedCompletion = false);
		static void ScheduleReturnToIdleMenu(Events::NativeDisconnectError nativeError = {});
		static void ReturnToIdleMenu(const Events::NativeDisconnectError& nativeError);
		static void UpdatePresence();
		static nlohmann::json PublishLocalRank(nlohmann::json party);
		static void EnterLobby(std::string map);
		static void RefreshLobby();
		static void LeaveParty();
		static void ToggleReady(bool isReady);
		static void StartPrivateMatch(std::string map);
		static void VoteMap(const std::string& choice);
		static void UpdateLobbyDvars(const nlohmann::json& party);
		static void UpdateMatchLobbyDvars(const nlohmann::json& status);
		static void UpdateVoteDvars(const nlohmann::json& status);
		static void RefreshActiveParty();
		static void UpdateMatchmaking();
		static void RefreshNetworkMetrics();
		static void UpdateSearchTime();
		static void BeginJoinInProgressPreview(const nlohmann::json& status);
		static void CancelJoinInProgressPreview();
		static void CancelPendingJoinPreparation();
		static void ConnectMatch(const std::string& matchId, bool isRelay, bool isReconnect, std::uint64_t expectedGeneration);
		static void InitializeDvars();
		static void EnqueueAsync(std::function<void()> task);
		static void ProcessAsyncTasks();
	};
}
