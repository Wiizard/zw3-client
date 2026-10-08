#pragma once

#include "Network.hpp"

namespace Components
{
	class Auth : public Component
	{
	public:
		Auth();

		static void StoreKey();
		static void LoadKey(bool force = false);
		static void GenerateKey();

		static std::uint64_t GetKeyHash();
		static std::uint64_t GetKeyHash(const std::string& key);

		static std::uint32_t GetSecurityLevel();
		static void IncreaseSecurityLevel(std::uint32_t level, const std::string& command = {});

		static std::uint32_t GetZeroBits(const Utils::Cryptography::Token& token, const std::string& publicKey);
		static void IncrementToken(Utils::Cryptography::Token& token, Utils::Cryptography::Token& searchToken,
			const std::string& publicKey, std::uint32_t zeroBits, bool* cancel = nullptr, std::uint64_t* count = nullptr);

		static std::string GetMachineEntropy();

		static bool SetManagedConnectTicket(const Network::Address& target, const std::string& ticket, const std::string& matchId, const std::string& sessionId);
		static void ClearManagedConnectTicket();

		static int PrivateClientCount();

	private:
		class TokenIncrementing
		{
		public:
			bool cancel = false;
			bool generating = false;
			std::jthread thread;
			std::uint32_t targetLevel = 0;
			int startTime = 0;
			std::string command;
			std::uint64_t hashes = 0;
		};

		static TokenIncrementing tokenContainer;

		static Utils::Cryptography::Token guidToken;
		static Utils::Cryptography::Token computeToken;
		static Utils::Cryptography::ECC::Key guidKey;
		static std::vector<std::uint64_t> bannedUids;

		static Utils::Hook sendConnectDataHook;
		static Utils::Hook packetEventHooks[2];
		static Utils::Hook directConnectHook;
		static Utils::Hook passwordHook;
		static Utils::Hook privateClientHook;
		static Utils::Hook connectFailedHook;

		static bool hasAccessToReservedSlot;

		static Game::msg_t* currentPacket;

		static bool SendConnectDataStub(Game::netsrc_t source, const Game::netadr_t* target,
			const void* data, int length);

		static char SV_PacketEvent_Hook(Game::netadr_t* from, Game::msg_t* message);

		static void DirectConnectStub(const Game::netadr_t* from);

		static void ParseConnectData(Game::msg_t* message, const Game::netadr_t* from);

		static const char* Info_ValueForKeyStub(const char* s, const char* key);

		static bool ClientConnectFailedStub(Game::netsrc_t source, const Game::netadr_t* from, const char* data);

		static std::string GetGuidFilePath();

		static void Frame();
	};
}
