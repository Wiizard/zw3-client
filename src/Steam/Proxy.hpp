#pragma once

#include "Steam/Steam.hpp"

#define GAMEOVERLAY_LIB "gameoverlayrenderer64.dll"
#define STEAMCLIENT_LIB "steamclient64.dll"
#define STEAM_REGISTRY_PATH "Software\\Wow6432Node\\Valve\\Steam"
#define STEAM_REGISTRY_PROCESS_PATH "Software\\Valve\\Steam\\ActiveProcess"

namespace Steam
{
	struct FriendGameInfo
	{
		GameID_t m_gameID;
		unsigned int m_unGameIP;
		unsigned short m_usGamePort;
		unsigned short m_usQueryPort;
		SteamID m_steamIDLobby;
	};

	static_assert(sizeof(FriendGameInfo) == 24);

	class ISteamClient008
	{
	public:
		virtual std::int32_t CreateSteamPipe() = 0;
		virtual bool BReleaseSteamPipe(std::int32_t hSteamPipe) = 0;
		virtual std::int32_t ConnectToGlobalUser(std::int32_t hSteamPipe) = 0;
		virtual std::int32_t CreateLocalUser(std::int32_t* phSteamPipe, int eAccountType) = 0;
		virtual void ReleaseUser(std::int32_t hSteamPipe, std::int32_t hUser) = 0;
		virtual void* GetISteamUser(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamGameServer(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void SetLocalIPBinding(std::uint32_t unIP, std::uint16_t usPort) = 0;
		virtual void* GetISteamFriends(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamUtils(std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamMatchmaking(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamMasterServerUpdater(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamMatchmakingServers(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamGenericInterface(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamUserStats(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
		virtual void* GetISteamApps(std::int32_t hSteamUser, std::int32_t hSteamPipe, const char* pchVersion) = 0;
	};

	class Friends15
	{
	public:
		virtual const char* GetPersonaName() = 0;
		virtual std::uint64_t SetPersonaName(const char* pchPersonaName) = 0;
		virtual int GetPersonaState() = 0;
		virtual int GetFriendCount(int iFriendFlags) = 0;
		virtual SteamID GetFriendByIndex(int iFriend, int iFriendFlags) = 0;
		virtual int GetFriendRelationship(SteamID steamIDFriend) = 0;
		virtual int GetFriendPersonaState(SteamID steamIDFriend) = 0;
		virtual const char* GetFriendPersonaName(SteamID steamIDFriend) = 0;
		virtual bool GetFriendGamePlayed(SteamID steamIDFriend, FriendGameInfo* pFriendGameInfo) = 0;
		virtual const char* GetFriendPersonaNameHistory(SteamID steamIDFriend, int iPersonaName) = 0;
		virtual int GetFriendSteamLevel(SteamID steamIDFriend) = 0;
		virtual const char* GetPlayerNickname(SteamID steamIDPlayer) = 0;
		virtual int GetFriendsGroupCount() = 0;
		virtual std::int16_t GetFriendsGroupIDByIndex(int iFG) = 0;
		virtual const char* GetFriendsGroupName(std::int16_t friendsGroupID) = 0;
		virtual int GetFriendsGroupMembersCount(std::int16_t friendsGroupID) = 0;
		virtual void GetFriendsGroupMembersList(std::int16_t friendsGroupID, SteamID* pOutSteamIDMembers, int nMembersCount) = 0;
		virtual bool HasFriend(SteamID steamIDFriend, int iFriendFlags) = 0;
		virtual int GetClanCount() = 0;
		virtual SteamID GetClanByIndex(int iClan) = 0;
		virtual const char* GetClanName(SteamID steamIDClan) = 0;
		virtual const char* GetClanTag(SteamID steamIDClan) = 0;
		virtual bool GetClanActivityCounts(SteamID steamIDClan, int* pnOnline, int* pnInGame, int* pnChatting) = 0;
		virtual std::uint64_t DownloadClanActivityCounts(SteamID* psteamIDClans, int cClansToRequest) = 0;
		virtual int GetFriendCountFromSource(SteamID steamIDSource) = 0;
		virtual SteamID GetFriendFromSourceByIndex(SteamID steamIDSource, int iFriend) = 0;
		virtual bool IsUserInSource(SteamID steamIDUser, SteamID steamIDSource) = 0;
		virtual void SetInGameVoiceSpeaking(SteamID steamIDUser, bool bSpeaking) = 0;
		virtual void ActivateGameOverlay(const char* pchDialog) = 0;
		virtual void ActivateGameOverlayToUser(const char* pchDialog, SteamID steamID) = 0;
		virtual void ActivateGameOverlayToWebPage(const char* pchURL) = 0;
		virtual void ActivateGameOverlayToStore(std::uint32_t nAppID, int eFlag) = 0;
		virtual void SetPlayedWith(SteamID steamIDUserPlayedWith) = 0;
		virtual void ActivateGameOverlayInviteDialog(SteamID steamIDLobby) = 0;
		virtual int GetSmallFriendAvatar(SteamID steamIDFriend) = 0;
		virtual int GetMediumFriendAvatar(SteamID steamIDFriend) = 0;
		virtual int GetLargeFriendAvatar(SteamID steamIDFriend) = 0;
		virtual bool RequestUserInformation(SteamID steamIDUser, bool bRequireNameOnly) = 0;
		virtual std::uint64_t RequestClanOfficerList(SteamID steamIDClan) = 0;
		virtual SteamID GetClanOwner(SteamID steamIDClan) = 0;
		virtual int GetClanOfficerCount(SteamID steamIDClan) = 0;
		virtual SteamID GetClanOfficerByIndex(SteamID steamIDClan, int iOfficer) = 0;
		virtual std::uint32_t GetUserRestrictions() = 0;
		virtual bool SetRichPresence(const char* pchKey, const char* pchValue) = 0;
		virtual void ClearRichPresence() = 0;
		virtual const char* GetFriendRichPresence(SteamID steamIDFriend, const char* pchKey) = 0;
		virtual int GetFriendRichPresenceKeyCount(SteamID steamIDFriend) = 0;
		virtual const char* GetFriendRichPresenceKeyByIndex(SteamID steamIDFriend, int iKey) = 0;
		virtual void RequestFriendRichPresence(SteamID steamIDFriend) = 0;
	};

	class Apps7
	{
	public:
		virtual bool BIsSubscribed() = 0;
		virtual bool BIsLowViolence() = 0;
		virtual bool BIsCybercafe() = 0;
		virtual bool BIsVACBanned() = 0;
		virtual const char* GetCurrentGameLanguage() = 0;
		virtual const char* GetAvailableGameLanguages() = 0;
		virtual bool BIsSubscribedApp(std::uint32_t appID) = 0;
	};

	class Utils5
	{
	public:
		virtual std::uint32_t GetSecondsSinceAppActive() = 0;
		virtual std::uint32_t GetSecondsSinceComputerActive() = 0;
		virtual int GetConnectedUniverse() = 0;
		virtual std::uint32_t GetServerRealTime() = 0;
		virtual const char* GetIPCountry() = 0;
		virtual bool GetImageSize(int iImage, std::uint32_t* pnWidth, std::uint32_t* pnHeight) = 0;
		virtual bool GetImageRGBA(int iImage, std::uint8_t* pubDest, int nDestBufferSize) = 0;
		virtual bool GetCSERIPPort(std::uint32_t* unIP, std::uint16_t* usPort) = 0;
		virtual std::uint8_t GetCurrentBatteryPower() = 0;
		virtual std::uint32_t GetAppID() = 0;
		virtual void SetOverlayNotificationPosition(int eNotificationPosition) = 0;
		virtual bool IsAPICallCompleted(std::uint64_t hSteamAPICall, bool* pbFailed) = 0;
		virtual int GetAPICallFailureReason(std::uint64_t hSteamAPICall) = 0;
		virtual bool GetAPICallResult(std::uint64_t hSteamAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed) = 0;
		virtual void RunFrame() = 0;
	};

	class User12
	{
	public:
		virtual std::int32_t GetHSteamUser() = 0;
		virtual bool BLoggedOn() = 0;
		virtual SteamID GetSteamID() = 0;
	};

	class Proxy
	{
	public:
		using Callback = void(*)(void* data);

		static bool Initialize();
		static void UnInitialize();

		static void SetGame(std::uint32_t appId);

		static void SetOverlayNotificationPosition(std::uint32_t eNotificationPosition);

		static void RunFrame();

		static void RegisterCall(std::int32_t callId, std::uint32_t size, std::uint64_t call);
		static void RegisterCallback(std::int32_t callId, Callback callback);
		static void UnregisterCallback(std::int32_t callId);

		static Friends15* SteamFriends;
		static Apps7* SteamApps;
		static Utils5* SteamUtils;
		static User12* SteamUser_;

		static std::uint32_t AppId;

	private:
		typedef bool(SteamBGetCallbackFn)(std::int32_t hSteamPipe, void* pCallbackMsg);
		typedef void(SteamFreeLastCallbackFn)(std::int32_t hSteamPipe);
		typedef bool(SteamGetAPICallResultFn)(std::int32_t hSteamPipe, std::uint64_t hSteamAPICall, void* pCallback, int cubCallback, int iCallbackExpected, bool* pbFailed);

		struct CallContainer
		{
			std::uint64_t call;
			bool handled;
			std::int32_t callId;
			std::uint32_t dataSize;
		};

		struct CallbackMsg
		{
			std::int32_t m_hSteamUser;
			int m_iCallback;
			std::uint8_t* m_pubParam;
			int m_cubParam;
		};

		static ::Utils::Library Client;
		static ::Utils::Library Overlay;

		static ISteamClient008* SteamClient;

		static std::int32_t SteamPipe;
		static std::int32_t SteamUser;

		static HANDLE Process;
		static HANDLE CancelHandle;
		static std::jthread WatchGuard;

		static std::recursive_mutex CallMutex;
		static std::vector<CallContainer> Calls;
		static std::unordered_map<std::int32_t, Callback> Callbacks;

		static SteamBGetCallbackFn* SteamBGetCallback;
		static SteamFreeLastCallbackFn* SteamFreeLastCallback;
		static SteamGetAPICallResultFn* SteamGetAPICallResult;

		static void RunCallback(std::int32_t callId, void* data);

		static void UnregisterCalls();
		static void LaunchWatchGuard();

		static std::string GetSteamDirectory();
	};
}
