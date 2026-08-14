#pragma once

#include <atomic>

namespace Components
{
	class ZWNet final : public Component
	{
	public:
		ZWNet();
		void preDestroy() override;

		// Called by the trusted launcher after the identity proof/login exchange.
		// Tokens are protected with Windows DPAPI before they touch disk.
		static bool StoreSession(const std::string& accessToken, const std::string& refreshToken);
		static void ResumeParty(const nlohmann::json& party);
		static bool TryGetSharedLobbyRank(const std::string& guid, int& level, int& prestige);

	private:
		// Function-local storage prevents non-trivial C++ initializers from running
		// in DllMain before the ZW3 runtime and component loader are ready.
		static std::atomic_bool& ActiveState();
		static std::atomic_bool& SearchingState();
		static std::atomic_bool& ClosingOnlineSessionState();
		static std::atomic_bool& ServerJoinTransitionState();
		static std::atomic_bool& OnlineEntryPendingState();
		static std::atomic_bool& InGameState();
		static bool& LoginInFlightState();
		static std::mutex& StateMutex();
		static std::string& AccessTokenState();
		static std::string& RefreshTokenState();
		static std::string& CurrentPartyIdState();
		static std::string& CurrentPlayerIdState();
		static std::string& CurrentProposalIdState();
		static std::string& CurrentMatchIdState();
		static std::mutex& AsyncTaskMutex();
		static std::deque<std::function<void()>>& AsyncTasks();

		static std::string SessionPath();
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
		static void StartQuickPlay();
		static void CancelSearch();
		static void CloseOnlineSession(bool shuttingDown);
		static void UpdatePresence();
		static nlohmann::json PublishLocalRank(nlohmann::json party);
		static void EnterLobby(std::string map);
		static void RefreshLobby();
		static void LeaveParty();
		static void ToggleReady(bool ready);
		static void StartPrivateMatch(std::string map);
		static void VoteMap(const std::string& choice);
		static void UpdateLobbyDvars(const nlohmann::json& party);
		static void UpdateMatchLobbyDvars(const nlohmann::json& status);
		static void UpdateVoteDvars(const nlohmann::json& status);
		static void RefreshActiveParty();
		static void UpdateMatchmaking();
		static void ConnectMatch(const std::string& matchId, bool relay);
		static void InitializeDvars();
		static void EnqueueAsync(std::function<void()> task);
		static void ProcessAsyncTasks();
	};
}
