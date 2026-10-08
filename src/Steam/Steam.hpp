#pragma once

namespace Steam
{
	typedef union
	{
		struct
		{
			unsigned int accountID : 32;
			unsigned int accountInstance : 20;
			unsigned int accountType : 4;
			int universe : 8;
		};

		unsigned long long bits;
	} SteamID;

#pragma pack(push, 1)
	typedef union
	{
		struct
		{
			unsigned int appID : 24;
			unsigned int type : 8;
			unsigned int modID : 32;
		};

		unsigned long long bits;
	} GameID_t;
#pragma pack(pop)

	class Callbacks
	{
	public:
		class Base
		{
		public:
			Base() : Flags(0), Callback(0) {}

			virtual void Run(void* param) = 0;
			virtual void Run(void* param, bool isIOFailure, std::uint64_t call) = 0;
			virtual int GetCallbackSizeBytes() = 0;

			int GetICallback()
			{
				return this->Callback;
			}

			void SetICallback(int callback)
			{
				this->Callback = callback;
			}

			void SetRegistered(bool isRegistered)
			{
				if (isRegistered)
				{
					this->Flags = static_cast<unsigned char>(this->Flags | registeredFlag);
				}
				else
				{
					this->Flags = static_cast<unsigned char>(this->Flags & ~registeredFlag);
				}
			}

		protected:
			static constexpr unsigned char registeredFlag = 0x01;

			~Base() = default;

			unsigned char Flags;
			int Callback;
		};

		struct Result
		{
			void* data;
			int size;
			int type;
			std::uint64_t call;
		};

		static std::uint64_t RegisterCall();
		static void RegisterCallback(Base* handler, int callback);
		static void RegisterCallResult(std::uint64_t call, Base* result);
		static void UnregisterCallback(Base* handler);
		static void UnregisterCallResult(Base* result, std::uint64_t call);
		static void ReturnCall(void* data, int size, int type, std::uint64_t call);
		static void RunCallbacks();
		static void RunCallback(int callback, void* data);
		static void Uninitialize();

	private:
		static std::uint64_t CallID;
		static std::map<std::uint64_t, bool> Calls;
		static std::map<std::uint64_t, Base*> ResultHandlers;
		static std::vector<Result> Results;
		static std::vector<Base*> CallbackList;
		static std::recursive_mutex Mutex;
	};

	struct LobbyCreated
	{
		static constexpr int CallbackID = 513;

		int m_eResult;
		SteamID m_ulSteamIDLobby;
	};

	struct LobbyEnter
	{
		static constexpr int CallbackID = 504;

		SteamID m_ulSteamIDLobby;
		unsigned int m_rgfChatPermissions;
		bool m_bLocked;
		unsigned int m_EChatRoomEnterResponse;
	};

	static_assert(sizeof(LobbyCreated) == 0x10 && offsetof(LobbyCreated, m_ulSteamIDLobby) == 8);
	static_assert(sizeof(LobbyEnter) == 0x18 && offsetof(LobbyEnter, m_EChatRoomEnterResponse) == 0x10);

	struct Interface
	{
		void* const* vtable;
	};

	std::uint64_t UnusedSlot();

	bool SteamAPI_Init();
	void SteamAPI_RegisterCallResult(Callbacks::Base* result, std::uint64_t call);
	void SteamAPI_RegisterCallback(Callbacks::Base* handler, int callback);
	void SteamAPI_RunCallbacks();
	void SteamAPI_Shutdown();
	void SteamAPI_UnregisterCallResult(Callbacks::Base* result, std::uint64_t call);
	void SteamAPI_UnregisterCallback(Callbacks::Base* handler);

	bool SteamGameServer_Init();
	void SteamGameServer_RunCallbacks();
	void SteamGameServer_Shutdown();

	Interface* SteamApps();
	Interface* SteamFriends();
	Interface* SteamGameServer();
	Interface* SteamMatchmaking();
	Interface* SteamNetworking();
	Interface* SteamRemoteStorage();
	Interface* SteamUser();
	Interface* SteamUtils();
}
