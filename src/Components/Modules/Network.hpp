#pragma once

namespace Components
{
	class Network : public Component
	{
	public:
		class Address
		{
		public:
			Address();
			Address(const std::string& text);
			Address(const Game::netadr_t& address) : address(address) {}
			Address(const Game::netadr_t* address) : Address(*address) {}
			Address(const sockaddr* address);
			Address(const Address&) = default;
			Address& operator=(const Address&) = default;

			bool operator==(const Address& other) const;
			bool operator!=(const Address& other) const { return !(*this == other); }

			void SetPort(unsigned short port);
			[[nodiscard]] unsigned short GetPort() const;

			void SetIP(unsigned int ip);
			[[nodiscard]] unsigned int GetIP() const;

			void SetType(Game::netadrtype_t type);
			[[nodiscard]] Game::netadrtype_t GetType() const;

			[[nodiscard]] Game::netadr_t* Get() { return &this->address; }
			[[nodiscard]] const Game::netadr_t* Get() const { return &this->address; }

			[[nodiscard]] std::string GetString() const;
			[[nodiscard]] const char* GetCString() const;

			[[nodiscard]] sockaddr GetSockAddr() const;

			[[nodiscard]] bool IsValid() const;

			[[nodiscard]] bool IsLocal() const;

			[[nodiscard]] bool IsSelf() const;

			[[nodiscard]] bool IsLoopback() const;

		private:
			Game::netadr_t address{};
		};

		using Callback = std::function<void(Address&, const std::string&)>;
		using RawCallback = std::function<void(Game::netadr_t*, Game::msg_t*)>;

		Network();

		static const char* AdrToString(const Address& address, bool withPort = true);

		static void Send(const Address& target, const std::string& data);
		static void Send(Game::netsrc_t source, const Address& target, const std::string& data);

		static void SendRaw(const Address& target, const std::string& data);
		static void SendRaw(Game::netsrc_t source, const Address& target, const std::string& data);

		static void SendCommand(const Address& target, const std::string& command,
			const std::string& data = {});
		static void SendCommand(Game::netsrc_t source, const Address& target,
			const std::string& command, const std::string& data = {});

		static void Broadcast(unsigned short port, const std::string& data);
		static void BroadcastRange(unsigned int min, unsigned int max, const std::string& data);

		static std::uint16_t GetPort();

		static void OnPacket(const std::string& command, const Callback& callback);
		static void OnPacketRaw(const std::string& command, const RawCallback& callback);

		using EntityEventObserver = std::function<bool(int localClientNum, Game::centity_s* cent, int event)>;
		static void OnEntityEvent(const EntityEventObserver& observer);

		static void RecordServerBuild(const Address& server, bool isX86);

		static bool IsX86Server();

		static bool HasSnapshotHook(std::uintptr_t site);

		static bool IsX86Client(const Game::client_s* client);

	private:
		static std::unordered_map<std::string, Callback> callbacks;
		static std::unordered_map<std::string, RawCallback> rawCallbacks;
		static Utils::Hook dispatchHooks[2];
		static Utils::Hook serverOpHooks[3];
		static Utils::Hook serverCommandHook;
		static Utils::Hook fileCheckHooks[3];
		static Utils::Hook defaultUserCmdHooks[2];
		static Utils::Hook snapshotHooks[2];
		static bool hasSnapshotHooks;
		static Utils::Hook scoresRequestHooks[3];
		static Utils::Hook entityEventHooks[4];
		static std::vector<EntityEventObserver> entityEventObservers;
		static Utils::Hook notifyCommandHook;

		static bool HandleCommand(Game::netadr_t* from, const char* command, Game::msg_t* message);

		static void RemapServerOps();
		static int ReadServerOp(Game::msg_t* msg);

		static void RemapServerCommands();
		static void TokenizeServerCommand(const char* text);

		static void SkipServerFileChecks();
		static bool FS_PureServerSetLoadedIwds_Hook(const char* iwdSums, const char* iwdNames);
		static void Com_Error_ModifiedFiles_Hook(int code, const char* message);
		static void CL_CompareFilesWithServer_Hook();

		static void KeepSprintInDefaultUserCmd();
		static void MSG_SetDefaultUserCmd_Hook(const std::uint8_t* ps, Game::usercmd_s* cmd);

		static void TranslateSnapshotForX86();
		static int CL_GetSnapshot_Hook(int localClientNum, int snapshotNumber, std::uint8_t* snapshot);

		static void RenameScoresRequest();
		static void CL_AddReliableCommand_Hook(int localClientNum, const char* text);

		static void TranslateNotifyCommand();
		static int CL_CheckNotify_Com_sprintf_Hook(char* command, int size, const char* format, int binding);

		static void DropEventsWeDoNotHave();
		static void CG_EntityEvent_Hook(unsigned int localClientNum, void* entity, unsigned int event, std::uint8_t isPlayer);

		static Utils::Hook addServerCommandHooks[3];
		static Utils::Hook clientCommandTokenizeHook;
		static Utils::Hook clientCommandFloodHook;
		static Utils::Hook serverDefaultUserCmdHook;
		static Utils::Hook serverOpWriteHooks[14];
		static Utils::Hook playerStateDeltaHook;

		static void TrackClientBuilds();
		static void HostX86ClientCommands();
		static void HostX86ClientOps();
		static void HostX86ClientHudElems();
		static bool MSG_WriteDeltaPlayerstate_Hook(std::uint8_t* snapInfo, Game::msg_t* msg, int time,
			const std::uint8_t* from, const std::uint8_t* to);
		static void SV_AddServerCommand_Hook(Game::client_s* client, int type, const char* text);
		static void SV_Cmd_TokenizeString_Hook(const char* text);
		static int SV_ExecuteClientCommand_Q_strncmp_Hook(const char* prefix, const char* text, int count);
		static void SV_UserMove_MSG_SetDefaultUserCmd_Hook(const std::uint8_t* ps, Game::usercmd_s* cmd);

		static bool CL_DispatchConnectionlessPacket_Hook(int localClientNum, Game::netadr_t* from,
			Game::msg_t* msg, int fourth);
	};
}

template <>
struct std::hash<Components::Network::Address>
{
	std::size_t operator()(const Components::Network::Address& x) const noexcept
	{
		return std::hash<std::uint32_t>()(x.GetIP()) ^ std::hash<std::uint16_t>()(x.GetPort());
	}
};
