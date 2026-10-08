#pragma once

#include "Dvar.hpp"
#include "Network.hpp"

namespace Components
{
	class RCon : public Component
	{
	public:
		RCon();

	private:
		class CryptoKeyRSA
		{
		public:
			static bool HasPublicKey();

			static Utils::Cryptography::RSA::Key& GetPublicKey();
			static Utils::Cryptography::RSA::Key& GetPrivateKey();

		private:
			static Utils::Cryptography::RSA::Key GenerateKeyPair();

			static Utils::Cryptography::RSA::Key LoadPublicKey();
			static Utils::Cryptography::RSA::Key GetPublicKeyInternal();

			static bool LoadPrivateKey(Utils::Cryptography::RSA::Key& key);
			static Utils::Cryptography::RSA::Key LoadOrGeneratePrivateKey();
			static Utils::Cryptography::RSA::Key GetPrivateKeyInternal();
		};

		static std::unordered_map<std::uint32_t, int> rateLimit;

		static std::vector<std::size_t> rconAddresses;

		static std::string password;

		static std::string rconOutputBuffer;

		static Dvar::Var rcon_password;
		static Dvar::Var rcon_log_requests;
		static Dvar::Var rcon_timeout;

		static void AddCommands();

		static bool IsRateLimitCheckDisabled();
		static bool RateLimitCheck(const Network::Address& address, int time);
		static void RateLimitCleanup(int time);

		static void RConExecutor(const Network::Address& address, std::string data);
		static void RConSafeExecutor(const Network::Address& address, std::string command);
	};
}
