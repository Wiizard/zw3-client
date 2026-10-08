#include "STDInclude.hpp"

#include <proto/rcon.pb.h>

#include "RCon.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"

namespace Components
{
	std::unordered_map<std::uint32_t, int> RCon::rateLimit;

	std::vector<std::size_t> RCon::rconAddresses;

	std::string RCon::password;

	Dvar::Var RCon::rcon_password;
	Dvar::Var RCon::rcon_log_requests;
	Dvar::Var RCon::rcon_timeout;

	std::string RCon::rconOutputBuffer;

	void RCon::AddCommands()
	{
		Command::Add("rcon", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const auto* operation = params->Get(1);

			if (std::strcmp(operation, "login") == 0)
			{
				if (params->Size() < 3)
				{
					return;
				}

				password = params->Get(2);
				return;
			}

			if (std::strcmp(operation, "logout") == 0)
			{
				password.clear();
				return;
			}

			const auto* addr = Game::clc_serverAddress;

			if (password.empty())
			{
				Logger::Print("You need to be logged in and connected to a server!\n");
			}

			Network::Address target(addr);

			if (!target.IsValid() || target.GetIP() == 0)
			{
				target = Party::Target();
			}

			if (target.IsValid())
			{
				Network::SendCommand(target, "rcon", password + " " + params->Join(1));
				return;
			}

			Logger::Print("You are connected to an invalid server\n");
		});

		Command::Add("rconSafe", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				Logger::Print("Usage: {} <command>\n", params->Get(0));
				return;
			}

			const auto command = params->Join(1);

			const auto* addr = Game::clc_serverAddress;
			Network::Address target(addr);

			if (!target.IsValid() || target.GetIP() == 0)
			{
				target = Party::Target();
			}

			if (!target.IsValid())
			{
				Logger::Print("You are connected to an invalid server\n");
				return;
			}

			const auto& key = CryptoKeyRSA::GetPrivateKey();
			const auto signature = Utils::Cryptography::RSA::SignMessage(key, command);

			Proto::RCon::Command directive;
			directive.set_command(command);
			directive.set_signature(signature);

			Network::SendCommand(target, "rconSafe", directive.SerializeAsString());
		});

		Command::AddSV("RconWhitelistAdd", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				Logger::Print("Usage: {} <ip-address>\n", params->Get(0));
				return;
			}

			const Network::Address address(params->Get(1));
			const auto hash = std::hash<std::uint32_t>()(address.GetIP());

			if (address.IsValid() && std::ranges::find(rconAddresses, hash) == rconAddresses.end())
			{
				rconAddresses.push_back(hash);
			}
		});
	}

	bool RCon::IsRateLimitCheckDisabled()
	{
		static std::optional<bool> flag;

		if (!flag.has_value())
		{
			flag.emplace(Flags::HasFlag("disable-rate-limit-check"));
		}

		return flag.value();
	}

	bool RCon::RateLimitCheck(const Network::Address& address, const int time)
	{
		const auto ip = address.GetIP();
		const auto lastTime = rateLimit[ip];

		if (lastTime && (time - lastTime) < rcon_timeout.Get<int>())
		{
			return false;
		}

		rateLimit[ip] = time;
		return true;
	}

	void RCon::RateLimitCleanup(const int time)
	{
		for (auto i = rateLimit.begin(); i != rateLimit.end();)
		{
			if ((time - i->second) > rcon_timeout.Get<int>())
			{
				i = rateLimit.erase(i);
			}
			else
			{
				++i;
			}
		}
	}

	void RCon::RConExecutor(const Network::Address& address, std::string data)
	{
		Utils::String::Trim(data);

		const auto pos = data.find_first_of(' ');

		if (pos == std::string::npos)
		{
			Logger::PrintFail2Ban("Invalid packet from IP address: {}\n", Network::AdrToString(address));
			Logger::Print("Invalid RCon request from {}\n", Network::AdrToString(address));
			return;
		}

		auto requestPassword = data.substr(0, pos);
		const auto command = data.substr(pos + 1);

		if (!requestPassword.empty() && requestPassword[0] == '"' && requestPassword.back() == '"')
		{
			requestPassword.pop_back();
			requestPassword.erase(requestPassword.begin());
		}

		const auto svPassword = rcon_password.Get<std::string>();

		if (svPassword.empty())
		{
			Logger::Print("RCon request from {} dropped. No password set!\n", address.GetString());
			return;
		}

		if (svPassword != requestPassword)
		{
			Logger::PrintFail2Ban("Invalid packet from IP address: {}\n", Network::AdrToString(address));
			Logger::Print("Invalid RCon password sent from {}\n", Network::AdrToString(address));
			return;
		}

		rconOutputBuffer.clear();

#ifndef _DEBUG
		if (rcon_log_requests.Get<bool>())
#endif
		{
			Logger::Print("Executing RCon request from {}: {}\n", Network::AdrToString(address), command);
		}

		Logger::PipeOutput([](const std::string& output)
		{
			rconOutputBuffer.append(output);
		});

		Command::Execute(command, true);

		Logger::PipeOutput(nullptr);

		Network::SendCommand(address, "print", rconOutputBuffer);
		rconOutputBuffer.clear();
	}

	void RCon::RConSafeExecutor(const Network::Address& address, std::string command)
	{
		rconOutputBuffer.clear();

#ifndef _DEBUG
		if (rcon_log_requests.Get<bool>())
#endif
		{
			Logger::Print("Executing Safe RCon request from {}: {}\n", address.GetString(), command);
		}

		Logger::PipeOutput([](const std::string& output)
		{
			rconOutputBuffer.append(output);
		});

		Command::Execute(command, true);

		Logger::PipeOutput(nullptr);

		Network::SendCommand(address, "print", rconOutputBuffer);
		rconOutputBuffer.clear();
	}

	RCon::RCon()
	{
		Events::OnSVInit(AddCommands);

		if (!Dedicated::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			rcon_password = Dvar::Register("rcon_password", "", Game::DVAR_NONE, "The password for rcon");
			rcon_log_requests = Dvar::Register("rcon_log_requests", false, Game::DVAR_NONE, "Print remote commands in log");
			rcon_timeout = Dvar::Register("rcon_timeout", 500, 100, 10000, Game::DVAR_NONE, "");
		});

		Network::OnPacket("rcon", [](Network::Address& address, [[maybe_unused]] const std::string& data)
		{
			const auto hash = std::hash<std::uint32_t>()(address.GetIP());

			if (!rconAddresses.empty() && std::ranges::find(rconAddresses, hash) == rconAddresses.end())
			{
				return;
			}

			const auto time = Game::Sys_Milliseconds();

			if (!IsRateLimitCheckDisabled() && !RateLimitCheck(address, time))
			{
				Logger::PrintFail2Ban("Invalid packet from IP address: {}\n", Network::AdrToString(address));
				return;
			}

			RateLimitCleanup(time);

			auto rconData = data;

			Scheduler::Once([address, s = std::move(rconData)]
			{
				RConExecutor(address, s);
			}, Scheduler::Pipeline::MAIN);
		});

		Network::OnPacket("rconSafe", [](Network::Address& address, [[maybe_unused]] const std::string& data)
		{
			const auto hash = std::hash<std::uint32_t>()(address.GetIP());

			if (!rconAddresses.empty() && std::ranges::find(rconAddresses, hash) == rconAddresses.end())
			{
				return;
			}

			const auto time = Game::Sys_Milliseconds();

			if (!IsRateLimitCheckDisabled() && !RateLimitCheck(address, time))
			{
				Logger::PrintFail2Ban("Invalid packet from IP address: {}\n", Network::AdrToString(address));
				return;
			}

			RateLimitCleanup(time);

			if (!CryptoKeyRSA::HasPublicKey())
			{
				return;
			}

			auto& key = CryptoKeyRSA::GetPublicKey();

			if (!key.IsValid())
			{
				Logger::Error("RSA public key is invalid\n");
				return;
			}

			Proto::RCon::Command directive;

			if (!directive.ParseFromString(data))
			{
				Logger::PrintFail2Ban("Invalid packet from IP address: {}\n", Network::AdrToString(address));
				Logger::Error("Unable to parse secure command from {}\n", Network::AdrToString(address));
				return;
			}

			if (!Utils::Cryptography::RSA::VerifyMessage(key, directive.command(), directive.signature()))
			{
				Logger::PrintFail2Ban("Invalid packet from IP address: {}\n", Network::AdrToString(address));
				Logger::Error("RSA signature verification failed for message from {}\n", Network::AdrToString(address));
				return;
			}

			std::string rconData = directive.command();

			Scheduler::Once([address, s = std::move(rconData)]
			{
				RConSafeExecutor(address, s);
			}, Scheduler::Pipeline::MAIN);
		});
	}

	Utils::Cryptography::RSA::Key RCon::CryptoKeyRSA::LoadPublicKey()
	{
		Utils::Cryptography::RSA::Key key;
		std::string data;

		if (!Utils::IO::ReadFile("./rsa-public.key", &data))
		{
			return key;
		}

		key.Set(data);
		return key;
	}

	Utils::Cryptography::RSA::Key RCon::CryptoKeyRSA::GetPublicKeyInternal()
	{
		auto key = LoadPublicKey();
		return key;
	}

	Utils::Cryptography::RSA::Key& RCon::CryptoKeyRSA::GetPublicKey()
	{
		static auto key = GetPublicKeyInternal();
		return key;
	}

	bool RCon::CryptoKeyRSA::LoadPrivateKey(Utils::Cryptography::RSA::Key& key)
	{
		std::string data;

		if (!Utils::IO::ReadFile("./rsa-private.key", &data))
		{
			return false;
		}

		key.Set(data);
		return key.IsValid();
	}

	Utils::Cryptography::RSA::Key RCon::CryptoKeyRSA::GenerateKeyPair()
	{
		auto key = Utils::Cryptography::RSA::GenerateKey(4096);

		if (!key.IsValid())
		{
			throw std::runtime_error("Failed to generate RSA key!");
		}

		if (!Utils::IO::WriteFile("./rsa-private.key", key.Serialize(PK_PRIVATE)))
		{
			throw std::runtime_error("Failed to write RSA private key!");
		}

		if (!Utils::IO::WriteFile("./rsa-public.key", key.Serialize(PK_PUBLIC)))
		{
			throw std::runtime_error("Failed to write RSA public key!");
		}

		return key;
	}

	Utils::Cryptography::RSA::Key RCon::CryptoKeyRSA::LoadOrGeneratePrivateKey()
	{
		Utils::Cryptography::RSA::Key key;

		if (LoadPrivateKey(key))
		{
			return key;
		}

		return GenerateKeyPair();
	}

	Utils::Cryptography::RSA::Key RCon::CryptoKeyRSA::GetPrivateKeyInternal()
	{
		auto key = LoadOrGeneratePrivateKey();
		return key;
	}

	Utils::Cryptography::RSA::Key& RCon::CryptoKeyRSA::GetPrivateKey()
	{
		static auto key = GetPrivateKeyInternal();
		return key;
	}

	bool RCon::CryptoKeyRSA::HasPublicKey()
	{
		return Utils::IO::FileExists("./rsa-public.key");
	}
}
