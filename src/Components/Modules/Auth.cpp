#include "STDInclude.hpp"

#include <proto/auth.pb.h>

#include "Auth.hpp"
#include "Bans.hpp"
#include "Bots.hpp"
#include "ClientSlots.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "FileSystem.hpp"
#include "Friends.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "Scheduler.hpp"
#include "Toast.hpp"
#include "UIScript.hpp"

#include "Steam/Proxy.hpp"

namespace Components
{
	constexpr std::uintptr_t CL_CheckForResend_SendConnectCall = 0x1400F8627;

	constexpr std::uintptr_t CSteamID__IsValid = 0x14024BD40;

	static const std::uint8_t steamIdIsValidEntry[] = { 0x8B, 0x51, 0x04, 0x44, 0x8B, 0xC2 };

	struct InlineSteamIdTest
	{
		std::uintptr_t site;
		std::uintptr_t validTarget;
	};

	static const InlineSteamIdTest lobbyOwnerTests[] =
	{
		{ 0x14024D7C5, 0x14024D83D },
		{ 0x14024DD45, 0x14024DDB0 },
	};

	static const std::uint8_t lobbyOwnerTestEntry[] = { 0x48, 0x8B, 0x00, 0x48, 0x8B, 0xC8, 0x48, 0xC1, 0xE9, 0x20 };

	constexpr std::uintptr_t SV_PacketEventCalls[] = { 0x1401F5FA6, 0x1401F6003 };

	constexpr std::uintptr_t SV_PacketEvent_DirectConnectCall = 0x14023CC87;

	constexpr std::uintptr_t SV_DirectConnect_PasswordCall = 0x140237B54;
	constexpr std::uintptr_t Info_ValueForKey = 0x14028CA30;

	constexpr std::uintptr_t SV_DirectConnect_PrivateClients = 0x140237BD0;
	static const std::uint8_t privateClientsLoad[] = { 0x48, 0x8B, 0x05, 0x41, 0x5D, 0x2D, 0x06, 0x44, 0x8B, 0x70, 0x10 };

	constexpr std::uintptr_t SV_DirectConnect_RejectPrintCall = 0x140237DCA;
	constexpr std::uintptr_t NET_OutOfBandPrint = 0x140206A90;

	extern "C"
	{
		void DirectConnectPrivateClientStub();

		int Auth_PrivateClientCount()
		{
			return Auth::PrivateClientCount();
		}
	}

	Auth::TokenIncrementing Auth::tokenContainer;

	Utils::Cryptography::Token Auth::guidToken;
	Utils::Cryptography::Token Auth::computeToken;
	Utils::Cryptography::ECC::Key Auth::guidKey;

	static bool isCryptographyReady = false;

	Utils::Hook Auth::sendConnectDataHook;
	Utils::Hook Auth::packetEventHooks[2];
	Utils::Hook Auth::directConnectHook;
	Utils::Hook Auth::passwordHook;
	Utils::Hook Auth::privateClientHook;
	Utils::Hook Auth::connectFailedHook;

	bool Auth::hasAccessToReservedSlot = false;

	Game::msg_t* Auth::currentPacket = nullptr;

	std::vector<std::uint64_t> Auth::bannedUids =
	{
		0xf4d2c30b712ac6e3,
		0xf7e33c4081337fa3,
		0x6f5597f103cc50e9,
		0xecd542eee54ffccf,
		0xA46B84C54694FD5B,
		0xECD542EEE54FFCCF,
		0x759096E09CB2BECF,
	};

	std::string Auth::GetGuidFilePath()
	{
		const auto directory = FileSystem::GetAppdataPath();

		if (directory.empty())
		{
			return {};
		}

		Utils::IO::CreateDir(directory.string());

		return (directory / "guid.dat").string();
	}

	std::uint64_t Auth::GetKeyHash(const std::string& key)
	{
		const auto hash = Utils::Cryptography::SHA1::Compute(key);

		if (hash.size() < sizeof(std::uint64_t))
		{
			return 0;
		}

		std::uint64_t id = 0;
		std::memcpy(&id, hash.data(), sizeof(id));

		return id;
	}

	std::uint64_t Auth::GetKeyHash()
	{
		LoadKey();
		return GetKeyHash(guidKey.GetPublicKey());
	}

	void Auth::StoreKey()
	{
		if (!guidKey.IsValid())
		{
			return;
		}

		Proto::Auth::Certificate cert;
		cert.set_token(guidToken.ToString());
		cert.set_ctoken(computeToken.ToString());
		cert.set_privatekey(guidKey.Serialize(PK_PRIVATE));

		const auto guidPath = GetGuidFilePath();

		if (guidPath.empty())
		{
			Logger::Warning("could not work out where to keep guid.dat, the key will not survive a restart\n");
			return;
		}

		Utils::IO::WriteFile(guidPath, cert.SerializeAsString());
	}

	void Auth::GenerateKey()
	{
		guidToken.Clear();
		computeToken.Clear();
		guidKey = Utils::Cryptography::ECC::GenerateKey(512, GetMachineEntropy());

		StoreKey();
	}

	void Auth::LoadKey(bool force)
	{
		if (!isCryptographyReady)
		{
			Utils::Cryptography::Initialize();
			isCryptographyReady = true;
		}

		if (!force && guidKey.IsValid())
		{
			return;
		}

		const auto guidPath = GetGuidFilePath();

		Proto::Auth::Certificate cert;

		if (!guidPath.empty() && cert.ParseFromString(Utils::IO::ReadFile(guidPath)))
		{
			guidKey.Deserialize(cert.privatekey());
			guidToken = cert.token();
			computeToken = cert.ctoken();
		}
		else
		{
			guidKey.Free();
		}

		if (!guidKey.IsValid())
		{
			GenerateKey();
		}
	}

	std::uint32_t Auth::GetZeroBits(const Utils::Cryptography::Token& token, const std::string& publicKey)
	{
		const auto hash = Utils::Cryptography::SHA512::Compute(publicKey + token.ToString(), false);

		std::uint32_t bits = 0;

		for (const auto entry : hash)
		{
			const auto value = static_cast<std::uint8_t>(entry);

			if (value == 0)
			{
				bits += 8;
				continue;
			}

			for (auto shift = 7; shift >= 0; --shift)
			{
				if ((value >> shift) & 1)
				{
					return bits;
				}

				++bits;
			}
		}

		return bits;
	}

	std::uint32_t Auth::GetSecurityLevel()
	{
		LoadKey();

		if (guidToken.ToString().empty())
		{
			return 0;
		}

		return GetZeroBits(guidToken, guidKey.GetPublicKey());
	}

	void Auth::IncrementToken(Utils::Cryptography::Token& token, Utils::Cryptography::Token& searchToken,
		const std::string& publicKey, std::uint32_t zeroBits, bool* cancel, std::uint64_t* count)
	{
		if (zeroBits > 512)
		{
			return;
		}

		if (searchToken < token)
		{
			searchToken = token;
		}

		auto lastLevel = token.ToString().empty() ? 0u : GetZeroBits(token, publicKey);

		if (lastLevel >= zeroBits)
		{
			return;
		}

		auto level = lastLevel;

		do
		{
			++searchToken;

			if (count)
			{
				++(*count);
			}

			level = GetZeroBits(searchToken, publicKey);

			if (level >= lastLevel)
			{
				token = searchToken;
				lastLevel = level;
			}

			if (cancel && *cancel)
			{
				return;
			}
		}
		while (level < zeroBits);

		token = searchToken;
	}

	void Auth::IncreaseSecurityLevel(std::uint32_t level, const std::string& command)
	{
		if (GetSecurityLevel() >= level)
		{
			return;
		}

		if (tokenContainer.generating)
		{
			return;
		}

		tokenContainer.cancel = false;
		tokenContainer.targetLevel = level;
		tokenContainer.command = command;

		Command::Execute("openmenu security_increase_popmenu", true);

		tokenContainer.thread = std::jthread([]
		{
			tokenContainer.generating = true;
			tokenContainer.hashes = 0;
			tokenContainer.startTime = Game::Sys_Milliseconds();

			IncrementToken(guidToken, computeToken, guidKey.GetPublicKey(), tokenContainer.targetLevel,
				&tokenContainer.cancel, &tokenContainer.hashes);

			tokenContainer.generating = false;

			if (tokenContainer.cancel)
			{
				Logger::Print("token incrementation thread terminated\n");
			}
		});
	}

	void Auth::Frame()
	{
		if (tokenContainer.generating)
		{
			const auto elapsed = Game::Sys_Milliseconds() - tokenContainer.startTime;

			if (elapsed <= 0)
			{
				return;
			}

			const auto hashesPerMs = static_cast<double>(tokenContainer.hashes) / elapsed;
			const auto requiredHashes = std::pow(2, tokenContainer.targetLevel + 1) - static_cast<double>(tokenContainer.hashes);

			auto remaining = (hashesPerMs > 0.0 ? requiredHashes / hashesPerMs : 0.0) + 2 * 60 * 1000;

			if (remaining < 0.0)
			{
				remaining = 0.0;
			}

			Localization::Set("MPUI_SECURITY_INCREASE_MESSAGE",
				Utils::String::VA("Increasing security level from %d to %d (est. %s)", GetSecurityLevel(),
					tokenContainer.targetLevel, Utils::String::FormatTimeSpan(static_cast<int>(remaining)).data()));

			return;
		}

		if (!tokenContainer.thread.joinable())
		{
			return;
		}

		tokenContainer.thread.join();

		StoreKey();
		Logger::Debug("security level is {}", GetSecurityLevel());
		Command::Execute("closemenu security_increase_popmenu", false);

		if (!tokenContainer.cancel)
		{
			if (tokenContainer.command.empty())
			{
				Game::ShowMessageBox(std::format("Your new security level is {}", GetSecurityLevel()), "Success");
			}
			else
			{
				Toast::Show("cardicon_locked", "Success", std::format("Your new security level is {}", GetSecurityLevel()), 5000);
				Command::Execute(tokenContainer.command, false);
			}
		}

		tokenContainer.cancel = false;
	}

	struct ManagedConnectTicket
	{
		std::mutex mutex;
		std::string endpoint;
		std::string ticket;
		std::string matchId;
		std::string sessionId;
		std::chrono::steady_clock::time_point expires{};
	};

	struct ManagedAdmissionConfig
	{
		bool isEnabled = false;
		bool isValid = false;
		std::string token;
		std::string instanceId;
		std::string backendUrl;
	};

	struct RecentManagedAdmission
	{
		std::string ticketHash;
		std::string matchId;
		std::string sessionId;
		std::string playerId;
		std::string instanceId;
		std::string sourceEndpoint;
		std::chrono::steady_clock::time_point expires;
	};

	constexpr auto managedTicketLifetime = 120s;
	constexpr auto recentAdmissionLifetime = 5s;
	constexpr std::size_t recentAdmissionLimit = 64;
	constexpr DWORD admissionTimeoutMs = 1500;

	static ManagedConnectTicket managedConnectTicket;
	static std::mutex recentAdmissionMutex;
	static std::vector<RecentManagedAdmission> recentAdmissions;

	static std::string ReadAdmissionEnvironment(const char* name)
	{
		const DWORD size = GetEnvironmentVariableA(name, nullptr, 0);

		if (size == 0 || size > 1024)
		{
			return {};
		}

		std::string value(size, '\0');
		const DWORD written = GetEnvironmentVariableA(name, value.data(), size);

		if (written == 0 || written >= size)
		{
			return {};
		}

		value.resize(written);
		return value;
	}

	static bool IsOpaqueAdmissionValue(const std::string& value, const std::size_t maximum)
	{
		if (value.empty() || value.size() > maximum)
		{
			return false;
		}

		return std::ranges::all_of(value, [](const unsigned char character)
		{
			return std::isalnum(character) || character == '-' || character == '_';
		});
	}

	static const ManagedAdmissionConfig& GetAdmissionConfig()
	{
		static const ManagedAdmissionConfig config = []
		{
			ManagedAdmissionConfig result;

			result.isEnabled = GetEnvironmentVariableA("ZWNET_ADMISSION_MODE", nullptr, 0) != 0
				|| GetEnvironmentVariableA("ZWNET_ADMISSION_TOKEN", nullptr, 0) != 0
				|| GetEnvironmentVariableA("ZWNET_ADMISSION_INSTANCE_ID", nullptr, 0) != 0;

			if (!result.isEnabled)
			{
				return result;
			}

			const std::string mode = ReadAdmissionEnvironment("ZWNET_ADMISSION_MODE");
			result.token = ReadAdmissionEnvironment("ZWNET_ADMISSION_TOKEN");
			result.instanceId = ReadAdmissionEnvironment("ZWNET_ADMISSION_INSTANCE_ID");
			result.backendUrl = ReadAdmissionEnvironment("ZWNET_ADMISSION_BACKEND_URL");

			constexpr std::string_view loopback = "http://127.0.0.1:";
			const std::string port = result.backendUrl.substr(std::min(result.backendUrl.size(), loopback.size()));

			const bool isLoopbackUrl = result.backendUrl.starts_with(loopback)
				&& !port.empty()
				&& port.size() <= 5
				&& std::ranges::all_of(port, [](const unsigned char character)
				{
					return std::isdigit(character);
				});

			int portNumber = 0;

			if (isLoopbackUrl)
			{
				portNumber = std::stoi(port);
			}

			result.isValid = mode == "REQUIRED"
				&& IsOpaqueAdmissionValue(result.token, 128)
				&& result.token.size() >= 43
				&& IsOpaqueAdmissionValue(result.instanceId, 128)
				&& portNumber > 0
				&& portNumber <= 65535;

			return result;
		}();

		return config;
	}

	static bool TryAdmitManagedConnect(const Proto::Auth::Connect& packet, const std::uint64_t certificateXuid, const Network::Address& address)
	{
		const auto& config = GetAdmissionConfig();

		if (!config.isEnabled)
		{
			return true;
		}

		const bool hasManagedFields = IsOpaqueAdmissionValue(packet.connect_ticket(), 128)
			&& IsOpaqueAdmissionValue(packet.match_id(), 128)
			&& IsOpaqueAdmissionValue(packet.session_id(), 128);

		if (!config.isValid || !hasManagedFields)
		{
			return false;
		}

		const std::string playerId = std::format("{:016x}", certificateXuid);
		const std::string ticketHash = Utils::Cryptography::SHA256::Compute(packet.connect_ticket());
		const std::string sourceEndpoint = address.GetString();

		{
			std::lock_guard _(recentAdmissionMutex);
			const auto now = std::chrono::steady_clock::now();

			std::erase_if(recentAdmissions, [now](const RecentManagedAdmission& entry)
			{
				return entry.expires <= now;
			});

			for (const auto& entry : recentAdmissions)
			{
				const bool isSameConnect = entry.ticketHash == ticketHash
					&& entry.matchId == packet.match_id()
					&& entry.sessionId == packet.session_id()
					&& entry.playerId == playerId
					&& entry.instanceId == config.instanceId
					&& entry.sourceEndpoint == sourceEndpoint;

				if (isSameConnect)
				{
					return true;
				}
			}
		}

		try
		{
			const nlohmann::json body =
			{
				{ "connect_ticket", packet.connect_ticket() },
				{ "match_id", packet.match_id() },
				{ "session_id", packet.session_id() },
				{ "player_id", playerId },
				{ "instance_id", config.instanceId },
			};

			const Utils::WebIO::Params headers =
			{
				{ "Content-Type", "application/json" },
				{ "Accept", "application/json" },
				{ "X-ZWNET-Admission-Token", config.token },
			};

			bool isSuccessful = false;
			Utils::WebIO request("ZW3-ManagedAdmission/1");
			const std::string response = request.SetTimeout(admissionTimeoutMs)->Post(config.backendUrl + "/internal/connect/admit", body.dump(), headers, &isSuccessful);

			if (!isSuccessful || response.empty())
			{
				return false;
			}

			const auto answer = nlohmann::json::parse(response);

			const bool isAdmitted = answer.is_object()
				&& answer.value("valid", false)
				&& answer.value("match_id", std::string{}) == packet.match_id()
				&& answer.value("player_id", std::string{}) == playerId;

			if (isAdmitted)
			{
				std::lock_guard _(recentAdmissionMutex);

				if (recentAdmissions.size() >= recentAdmissionLimit)
				{
					recentAdmissions.erase(recentAdmissions.begin());
				}

				recentAdmissions.push_back({ ticketHash, packet.match_id(), packet.session_id(), playerId, config.instanceId, sourceEndpoint, std::chrono::steady_clock::now() + recentAdmissionLifetime });
			}

			return isAdmitted;
		}
		catch (const std::exception&)
		{
			return false;
		}
	}

	bool Auth::SetManagedConnectTicket(const Network::Address& target, const std::string& ticket, const std::string& matchId, const std::string& sessionId)
	{
		const bool isTicketValid = IsOpaqueAdmissionValue(ticket, 128)
			&& IsOpaqueAdmissionValue(matchId, 128)
			&& IsOpaqueAdmissionValue(sessionId, 128);

		if (!target.IsValid() || !isTicketValid)
		{
			return false;
		}

		std::lock_guard _(managedConnectTicket.mutex);

		std::ranges::fill(managedConnectTicket.ticket, '\0');
		managedConnectTicket.endpoint = target.GetString();
		managedConnectTicket.ticket = ticket;
		managedConnectTicket.matchId = matchId;
		managedConnectTicket.sessionId = sessionId;
		managedConnectTicket.expires = std::chrono::steady_clock::now() + managedTicketLifetime;

		return true;
	}

	void Auth::ClearManagedConnectTicket()
	{
		std::lock_guard _(managedConnectTicket.mutex);

		std::ranges::fill(managedConnectTicket.ticket, '\0');
		managedConnectTicket.endpoint.clear();
		managedConnectTicket.ticket.clear();
		managedConnectTicket.matchId.clear();
		managedConnectTicket.sessionId.clear();
		managedConnectTicket.expires = {};
	}

	bool Auth::SendConnectDataStub(Game::netsrc_t source, const Game::netadr_t* target, const void* data, int length)
	{
		if (!target || !data || length <= 0)
		{
			return false;
		}

		LoadKey();

		if (!guidKey.IsValid())
		{
			Logger::Error("connecting failed: the guid key is invalid\n");
			return false;
		}

		if (std::ranges::find(bannedUids, GetKeyHash()) != bannedUids.end())
		{
			GenerateKey();
			Logger::Error("your online profile is invalid, a new key has been generated\n");
			return false;
		}

		const std::string_view packet{static_cast<const char*>(data), static_cast<std::size_t>(length)};

		const auto qportStart = packet.find(' ');
		const auto infoStart = qportStart == std::string_view::npos
			? std::string_view::npos
			: packet.find(' ', qportStart + 1);

		if (infoStart == std::string_view::npos)
		{
			Logger::Error("connecting failed: could not read the connect string\n");
			return false;
		}

		auto infoString = packet.substr(infoStart + 1);

		if (infoString.size() < 2 || infoString.front() != '"' || infoString.back() != '"')
		{
			Logger::Error("connecting failed: the connect string is not quoted\n");
			return false;
		}

		infoString = infoString.substr(1, infoString.size() - 2);

		Utils::InfoString info{std::string{infoString}};

		const Network::Address address{target};

		if (!address.IsLoopback())
		{
			info.Set("xuid", Utils::String::VA("%llX", GetKeyHash()));
		}

		if (!Friends::IsInvisible() && !Friends::cl_anonymous.Get<bool>() && ::Steam::Proxy::SteamUser_)
		{
			info.Set("realsteamId", Utils::String::VA("%llX", ::Steam::Proxy::SteamUser_->GetSteamID().bits));
		}

		const auto challenge = info.Get("challenge");

		if (challenge.empty())
		{
			Logger::Error("connecting failed: the server sent no challenge\n");
			return false;
		}

		if (guidToken.ToString().empty() && !address.IsLoopback())
		{
			Logger::Error("connecting failed: the guid token is empty\n");
			return false;
		}

		std::string connectString{packet.substr(0, infoStart + 1)};
		connectString.append("\"");
		connectString.append(info.Build());
		connectString.append("\"");

		Proto::Auth::Connect connectData;
		connectData.set_token(guidToken.ToString());
		connectData.set_publickey(guidKey.GetPublicKey());
		connectData.set_signature(Utils::Cryptography::ECC::SignMessage(guidKey, challenge));
		connectData.set_infostring(connectString);

		{
			std::lock_guard _(managedConnectTicket.mutex);

			const Network::Address ticketTarget{managedConnectTicket.endpoint};
			const bool isTicketForTarget = ticketTarget.IsValid()
				&& ticketTarget == address
				&& std::chrono::steady_clock::now() < managedConnectTicket.expires;

			if (isTicketForTarget)
			{
				connectData.set_connect_ticket(managedConnectTicket.ticket);
				connectData.set_match_id(managedConnectTicket.matchId);
				connectData.set_session_id(managedConnectTicket.sessionId);
			}
		}

		Network::SendCommand(source, address, "connect", connectData.SerializeAsString());

		return true;
	}

	static bool IsSameMachineAddress(const Network::Address& address)
	{
		if (address.IsLoopback())
		{
			return true;
		}

		for (int i = 0; i < *Game::numIP; ++i)
		{
			if (address.GetIP() == Game::localIP[i].full)
			{
				return true;
			}
		}

		return false;
	}

	void Auth::ParseConnectData(Game::msg_t* message, const Game::netadr_t* from)
	{
		const Network::Address address{from};

		constexpr int payloadOffset = 12;

		Proto::Auth::Connect connectData;

		if (message->cursize <= payloadOffset
			|| !connectData.ParseFromString(std::string(
				reinterpret_cast<const char*>(message->data) + payloadOffset,
				static_cast<std::size_t>(message->cursize) - payloadOffset)))
		{
			Logger::Print("refused a connect from {}, the packet is not a connect protobuf\n",
				address.GetString());
			Network::Send(Game::NS_SERVER, address, "error\nInvalid connect packet!");
			return;
		}

		if (address.IsLoopback() && !GetAdmissionConfig().isEnabled)
		{
			if (connectData.infostring().empty())
			{
				Logger::Print("refused a connect from {}, the packet carries no infostring\n",
					address.GetString());
				Network::Send(Game::NS_SERVER, address, "error\nInvalid infostring data!");
				return;
			}

			Game::SV_Cmd_EndTokenizedString();
			Game::SV_Cmd_TokenizeString(connectData.infostring().data());
			Game::SV_DirectConnect(address.Get());

			Logger::Print("accepted a loopback connect from {}\n", address.GetString());
			return;
		}

		if (connectData.signature().empty() || connectData.publickey().empty()
			|| connectData.token().empty() || connectData.infostring().empty())
		{
			Logger::Print("refused a connect from {}, the packet is missing a field\n",
				address.GetString());
			Network::Send(Game::NS_SERVER, address, "error\nInvalid connect data!");
			return;
		}

		Game::SV_Cmd_EndTokenizedString();
		Game::SV_Cmd_TokenizeString(connectData.infostring().data());

		const Command::ServerParams params;

		if (params.Size() < 3)
		{
			Logger::Print("refused a connect from {}, the infostring is not a connect line\n",
				address.GetString());
			Network::Send(Game::NS_SERVER, address, "error\nInvalid connect string!");
			return;
		}

		const std::string userinfo = params.Get(2);
		const std::string steamId = Game::Info_ValueForKey(userinfo.data(), "xuid");
		const std::string challenge = Game::Info_ValueForKey(userinfo.data(), "challenge");

		if (steamId.empty() || challenge.empty())
		{
			Logger::Print("refused a connect from {}, the infostring has no xuid or no challenge\n",
				address.GetString());
			Network::Send(Game::NS_SERVER, address, "error\nInvalid connect data!");
			return;
		}

		const auto xuid = std::strtoull(steamId.data(), nullptr, 16);

		::Steam::SteamID guid;
		guid.bits = xuid;

		Game::netIP_t ip;
		ip.full = address.GetIP();

		if (Bans::IsBanned({ guid, ip }))
		{
			Logger::Print("refused a connect from {}, xuid {:#x} or its address is banned\n",
				address.GetString(), xuid);
			Network::Send(Game::NS_SERVER, address, "error\nEXE_ERR_BANNED_PERM");
			return;
		}

		if (Game::IsTempBanned(xuid))
		{
			Logger::Print("rejected connection from temporarily banned client {:#x}\n", xuid);
			Network::Send(Game::NS_SERVER, address, "error\nEXE_ERR_BANNED_TEMP");
			return;
		}

		if (std::ranges::find(bannedUids, xuid) != bannedUids.end())
		{
			Logger::Print("refused a connect from {}, xuid {:#x} is on the banned list\n",
				address.GetString(), xuid);
			Network::Send(Game::NS_SERVER, address,
				"error\nYour online profile is invalid. Delete your players folder and restart ^1ZW3^7.");
			return;
		}

		if (xuid != GetKeyHash(connectData.publickey()))
		{
			Logger::Print("refused a connect from {}, xuid {:#x} is not this certificate's\n",
				address.GetString(), xuid);
			Network::Send(Game::NS_SERVER, address, "error\nXUID doesn't match the certificate!");
			return;
		}

		Utils::Cryptography::ECC::Key key;
		key.Set(connectData.publickey());

		if (!key.IsValid()
			|| !Utils::Cryptography::ECC::VerifyMessage(key, challenge, connectData.signature()))
		{
			Logger::Print("refused a connect from {}, the challenge signature does not verify\n",
				address.GetString());
			Network::Send(Game::NS_SERVER, address, "error\nChallenge signature was invalid!");
			return;
		}

		const auto ourLevel = Dvar::Var("sv_securityLevel").Get<unsigned int>();
		const auto userLevel = GetZeroBits(connectData.token(), connectData.publickey());

		if (userLevel < ourLevel && IsSameMachineAddress(address))
		{
			Logger::Print("allowing {} from this machine at security level {} below our {}\n",
				address.GetString(), userLevel, ourLevel);
		}
		else if (userLevel < ourLevel)
		{
			Logger::Print("refused a connect from {}, security level {} is below our {}\n",
				address.GetString(), userLevel, ourLevel);
			Network::Send(Game::NS_SERVER, address, Utils::String::VA(
				"error\nYour security level (%d) is lower than the server's security level (%d)",
				userLevel, ourLevel));
			return;
		}

		if (!TryAdmitManagedConnect(connectData, GetKeyHash(connectData.publickey()), address))
		{
			Logger::PrintFail2Ban("Managed admission rejected connection from {}\n", address.GetString());
			Network::Send(Game::NS_SERVER, address, "error\nThis managed match did not authorize your connection.");
			return;
		}

		Logger::Print("accepted xuid {:#x} at security level {} from {}\n",
			xuid, userLevel, address.GetString());

		Game::SV_DirectConnect(address.Get());
	}

	void Auth::DirectConnectStub(const Game::netadr_t* from)
	{
		if (!currentPacket)
		{
			Game::SV_DirectConnect(from);
			return;
		}

		ParseConnectData(currentPacket, from);
	}

	const char* Auth::Info_ValueForKeyStub(const char* s, const char* key)
	{
		const auto* value = Game::Info_ValueForKey(s, key);

		hasAccessToReservedSlot = std::strcmp((*Game::sv_privatePassword)->current.string, value) == 0;

		Bots::SV_DirectConnect_Full_Check();

		return value;
	}

	bool Auth::ClientConnectFailedStub(const Game::netsrc_t source, const Game::netadr_t* from, const char* data)
	{
		Logger::PrintFail2Ban("Failed connect attempt from IP address: {}\n", Network::AdrToString(from));
		return Game::NET_OutOfBandPrint(source, from, data);
	}

	int Auth::PrivateClientCount()
	{
		const int botFirstSlot = ClientSlots::FirstSlotForNewBot();

		if (botFirstSlot >= 0)
		{
			return botFirstSlot;
		}

		if (hasAccessToReservedSlot)
		{
			return 0;
		}

		return (*Game::sv_privateClients)->current.integer;
	}

	char Auth::SV_PacketEvent_Hook(Game::netadr_t* from, Game::msg_t* message)
	{
		currentPacket = message;

		const auto result = reinterpret_cast<char(*)(Game::netadr_t*, Game::msg_t*)>(
			packetEventHooks[0].GetOriginal())(from, message);

		currentPacket = nullptr;

		return result;
	}

	std::string Auth::GetMachineEntropy()
	{
		std::string entropy;

		DWORD volumeId = 0;

		if (GetVolumeInformationA("C:\\", nullptr, 0, &volumeId, nullptr, nullptr, nullptr, 0))
		{
			entropy += std::to_string(volumeId);
		}

		unsigned long bufferLength = 0;

		if (GetAdaptersInfo(nullptr, &bufferLength) == ERROR_BUFFER_OVERFLOW)
		{
			std::vector<std::uint8_t> buffer(bufferLength);
			auto* adapterInfo = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.data());

			if (GetAdaptersInfo(adapterInfo, &bufferLength) == ERROR_SUCCESS)
			{
				for (auto* adapter = adapterInfo; adapter; adapter = adapter->Next)
				{
					if (adapter->Type != IF_TYPE_IEEE80211 && adapter->Type != MIB_IF_TYPE_ETHERNET)
					{
						continue;
					}

					for (UINT i = 0; i < adapter->AddressLength; ++i)
					{
						entropy += std::to_string(adapter->Address[i]);
					}
				}
			}
		}

		if (entropy.empty())
		{
			return std::to_string(Utils::Cryptography::Rand::GenerateLong());
		}

		return entropy;
	}

	Auth::Auth()
	{
		Localization::Set("MPUI_SECURITY_INCREASE_MESSAGE", "");

		if (Utils::Hook::MatchesBytes(CSteamID__IsValid, steamIdIsValidEntry, sizeof(steamIdIsValidEntry)))
		{
			Utils::Hook::Set<std::uint8_t>(CSteamID__IsValid, 0xB0);
			Utils::Hook::Set<std::uint8_t>(CSteamID__IsValid + 1, 0x01);
			Utils::Hook::Set<std::uint8_t>(CSteamID__IsValid + 2, 0xC3);
		}
		else
		{
			Logger::Error("auth: CSteamID::IsValid does not read as expected, a key hash steam id may be refused\n");
		}

		const bool areOwnerTestsIntact = std::ranges::all_of(lobbyOwnerTests, [](const InlineSteamIdTest& test)
		{
			return Utils::Hook::MatchesBytes(test.site, lobbyOwnerTestEntry, sizeof(lobbyOwnerTestEntry));
		});

		if (areOwnerTestsIntact)
		{
			for (const InlineSteamIdTest& test : lobbyOwnerTests)
			{
				Utils::Hook::Set<std::uint8_t>(test.site, 0xEB);
				Utils::Hook::Set<std::int8_t>(test.site + 1, static_cast<std::int8_t>(test.validTarget - (test.site + 2)));
			}
		}
		else
		{
			Logger::Error("auth: the inline lobby owner tests do not read as expected, a private party can drop as disconnected from steam\n");
		}

		if (!sendConnectDataHook.Initialize(CL_CheckForResend_SendConnectCall,
			reinterpret_cast<void*>(SendConnectDataStub), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("auth could not seat its connect hook, joining anything would fail on the far end\n");
			return;
		}

		sendConnectDataHook.Quick();

		int failed = 0;

		for (std::size_t i = 0; i < ARRAYSIZE(SV_PacketEventCalls); ++i)
		{
			failed += !packetEventHooks[i].Initialize(SV_PacketEventCalls[i],
				reinterpret_cast<void*>(SV_PacketEvent_Hook), HOOK_CALL)->Install()->IsInstalled();
		}

		failed += !directConnectHook.Initialize(SV_PacketEvent_DirectConnectCall,
			reinterpret_cast<void*>(DirectConnectStub), HOOK_CALL)->Install()->IsInstalled();

		if (failed)
		{
			Logger::Error("auth could not seat {} of 3 server hooks, nothing will be able to join us\n", failed);
			return;
		}

		for (auto& hook : packetEventHooks)
		{
			hook.Quick();
		}

		directConnectHook.Quick();

		const bool isDirectConnectIntact = Utils::Hook::BranchesTo(SV_DirectConnect_PasswordCall, Info_ValueForKey, HOOK_CALL)
			&& Utils::Hook::MatchesBytes(SV_DirectConnect_PrivateClients, privateClientsLoad, sizeof(privateClientsLoad));

		bool isReservedSlotSeated = false;

		if (isDirectConnectIntact)
		{
			isReservedSlotSeated = passwordHook.Initialize(SV_DirectConnect_PasswordCall, reinterpret_cast<void*>(Info_ValueForKeyStub), HOOK_CALL)->Install()->IsInstalled();
			isReservedSlotSeated = privateClientHook.Initialize(SV_DirectConnect_PrivateClients, DirectConnectPrivateClientStub, HOOK_CALL)->Install()->IsInstalled() && isReservedSlotSeated;
		}

		if (!isReservedSlotSeated)
		{
			passwordHook.Uninstall();
			privateClientHook.Uninstall();

			Logger::Error("auth: SV_DirectConnect does not read as expected, no reserved slots and sv_replaceBots does nothing\n");
		}
		else
		{
			passwordHook.Quick();
			privateClientHook.Quick();
			Utils::Hook::Nop(SV_DirectConnect_PrivateClients + 5, sizeof(privateClientsLoad) - 5);
		}

		if (!Utils::Hook::BranchesTo(SV_DirectConnect_RejectPrintCall, NET_OutOfBandPrint, HOOK_CALL)
			|| !connectFailedHook.Initialize(SV_DirectConnect_RejectPrintCall, reinterpret_cast<void*>(ClientConnectFailedStub), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("auth: SV_DirectConnect's reject print does not read as expected, fail2ban sees no failed connect\n");
		}
		else
		{
			connectFailedHook.Quick();
		}

		Scheduler::Once([]
		{
			LoadKey(true);

			Dvar::Register("sv_securityLevel", 23, 0, 512, Game::DVAR_SERVERINFO,
				"Security level for GUID certificates (POW)");
		}, Scheduler::Pipeline::MAIN);

		Scheduler::Loop(Frame, Scheduler::Pipeline::MAIN, 500ms);

		Command::Add("guid", []
		{
			Logger::Print("your guid: {:#X}\n", GetKeyHash());
		});

		if (!Dedicated::IsEnabled())
		{
			Command::Add("securityLevel", [](const Command::Params* params)
			{
				if (params->Size() < 2)
				{
					Logger::Print("your current security level is {}\n", GetSecurityLevel());
					Logger::Print("your security token is: {}\n", Utils::String::DumpHex(guidToken.ToString(), ""));
					Logger::Print("your computation token is: {}\n", Utils::String::DumpHex(computeToken.ToString(), ""));

					Toast::Show("cardicon_locked", "^5Security Level", std::format("Your security level is {}", GetSecurityLevel()), 3000);
					return;
				}

				IncreaseSecurityLevel(std::strtoul(params->Get(1), nullptr, 10));
			});
		}

		UIScript::Add("security_increase_cancel", []([[maybe_unused]] const UIScript::Token& token)
		{
			tokenContainer.cancel = true;
			Logger::Print("token incrementation process canceled\n");
		});
	}
}
