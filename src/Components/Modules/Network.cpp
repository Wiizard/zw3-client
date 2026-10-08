#include "STDInclude.hpp"

#include "Network.hpp"
#include "Logger.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "Weapon.hpp"

namespace Components
{
	std::unordered_map<std::string, Network::Callback> Network::callbacks;
	std::unordered_map<std::string, Network::RawCallback> Network::rawCallbacks;
	Utils::Hook Network::dispatchHooks[2];
	Utils::Hook Network::serverOpHooks[3];
	Utils::Hook Network::serverCommandHook;
	Utils::Hook Network::fileCheckHooks[3];
	Utils::Hook Network::defaultUserCmdHooks[2];
	Utils::Hook Network::snapshotHooks[2];
	bool Network::hasSnapshotHooks = false;
	Utils::Hook Network::scoresRequestHooks[3];
	Utils::Hook Network::entityEventHooks[4];
	std::vector<Network::EntityEventObserver> Network::entityEventObservers;
	Utils::Hook Network::notifyCommandHook;
	Utils::Hook Network::addServerCommandHooks[3];
	Utils::Hook Network::clientCommandTokenizeHook;
	Utils::Hook Network::clientCommandFloodHook;
	Utils::Hook Network::serverDefaultUserCmdHook;
	Utils::Hook Network::serverOpWriteHooks[14];
	Utils::Hook Network::playerStateDeltaHook;
	static bool isNetDebug = false;

	static Network::Address joinedServer;
	static bool isJoinedServerX86 = true;

	constexpr std::uintptr_t serverOpReads[] = { 0x140100D75, 0x1400FFD8F, 0x140100118 };

	static const std::uint8_t serverOpReadBytes[][8] =
	{
		{ 0xE8, 0xC6, 0x12, 0x10, 0x00, 0x83, 0xF8, 0x05 },
		{ 0xE8, 0xAC, 0x22, 0x10, 0x00, 0x83, 0xF8, 0x05 },
		{ 0xE8, 0x23, 0x1F, 0x10, 0x00, 0x83, 0xF8, 0x05 },
	};

	constexpr int svc_nopX86 = 0;

	static constexpr int x64OpForX86Op[] = { 0, 2, 3, 1, 4, 0, 5 };

	constexpr std::uintptr_t CL_CGameNeedsServerCommand_TokenizeCall = 0x1400F3DC2;

	static const std::uint8_t tokenizeCall[] = { 0xE8, 0xB9, 0x40, 0x0F, 0x00 };

	constexpr std::uintptr_t fileCheckCalls[] = { 0x1401001F7, 0x1401002E3, 0x140100366 };

	static const std::uint8_t fileCheckCallBytes[][5] =
	{
		{ 0xE8, 0x14, 0xE4, 0x0F, 0x00 },
		{ 0xE8, 0x08, 0x36, 0x0F, 0x00 },
		{ 0xE8, 0x65, 0x85, 0xFF, 0xFF },
	};

	constexpr std::uintptr_t cls_doVidRestart = 0x140C5CFE8;

	constexpr std::uintptr_t FS_ServerSetReferencedFiles_NameTest = 0x1401FEDB9;
	constexpr std::uintptr_t FS_ServerSetReferencedFiles_CopyName = 0x1401FEDE8;

	static const std::uint8_t referencedNameTest[] = { 0x80, 0x3B, 0x00, 0x8B, 0xD0, 0x74, 0x12, 0x48, 0x83, 0xFA, 0x40, 0x73, 0x0C };
	static const std::uint8_t referencedNameCopy[] = { 0x48, 0x8B, 0xCB, 0xE8, 0xB0, 0x05, 0x08, 0x00 };

	constexpr std::uintptr_t defaultUserCmdSites[] = { 0x1400F5E8C, 0x1400F76F1 };

	static const std::uint8_t defaultUserCmdSiteBytes[][5] =
	{
		{ 0xE9, 0x7F, 0xC8, 0x10, 0x00 },
		{ 0xE8, 0x1A, 0xB0, 0x10, 0x00 },
	};

	constexpr std::ptrdiff_t playerStatePmFlags = 12;
	constexpr std::ptrdiff_t playerStateOtherFlags = 16;
	constexpr int PMF_SPRINTING = 1 << 14;
	constexpr int POF_PLAYER = 1 << 12;
	constexpr int sprintButton = 2;

	constexpr std::uintptr_t CG_ProcessSnapshots_GetSnapshotCalls[] = { 0x1400E95F9, 0x1400E96BB };

	static const std::uint8_t getSnapshotCallBytes[][5] =
	{
		{ 0xE8, 0x42, 0xB2, 0x00, 0x00 },
		{ 0xE8, 0x80, 0xB1, 0x00, 0x00 },
	};

	constexpr std::ptrdiff_t snapshotNumEntities = 0x3128;
	constexpr std::ptrdiff_t snapshotEntities = 0x3130;
	constexpr int maxSnapshotEntities = 768;
	constexpr std::size_t entityStateSize = 256;
	constexpr std::ptrdiff_t entityStateType = 4;
	constexpr std::ptrdiff_t entityStateItemIndex = 144;
	constexpr int ET_ITEM = 3;

	constexpr std::ptrdiff_t playerStateHudElems[] = { 0x848, 0x1CA0 };
	constexpr std::size_t hudElemSize = 0xA8;
	constexpr int hudElemCount = 31;

	constexpr int hudElemFree = 0;
	constexpr int hudElemText = 1;
	constexpr int hudElemMapNameX86 = 4;
	constexpr int hudElemGameTypeX86 = 5;
	constexpr int hudElemMaterialX86 = 6;

	constexpr int hudElemExtraTypesX86 = 2;

	constexpr int weaponModelStrideX86 = 2400;
	constexpr int weaponModelStride = 1400;

	constexpr std::uintptr_t scoresRequestSites[] = { 0x1400D06BF, 0x1400E3FBD, 0x1400E5C02 };

	static const std::uint8_t scoresRequestSiteBytes[][5] =
	{
		{ 0xE8, 0x4C, 0x7A, 0x02, 0x00 },
		{ 0xE8, 0x4E, 0x41, 0x01, 0x00 },
		{ 0xE9, 0x09, 0x25, 0x01, 0x00 },
	};

	constexpr auto scoresRequest = "s";
	constexpr auto scoresRequestX86 = "score";

	constexpr std::uintptr_t CL_CheckNotify_Com_sprintfCall = 0x1400F879E;

	static const std::uint8_t notifyFormatCallBytes[] = { 0xE8, 0x0D, 0x37, 0x19, 0x00 };

	constexpr std::uintptr_t g_bindCommands = 0x140420C90;

	static constexpr int bindCommandCount = 78;

	constexpr std::uintptr_t CG_EntityEventCalls[] = { 0x1400ACEEC, 0x1400ACF7A, 0x1400B6038, 0x1400B60A4 };

	static const std::uint8_t entityEventCallBytes[][5] =
	{
		{ 0xE8, 0x9F, 0x00, 0x00, 0x00 },
		{ 0xE8, 0x11, 0x00, 0x00, 0x00 },
		{ 0xE8, 0x53, 0x6F, 0xFF, 0xFF },
		{ 0xE8, 0xE7, 0x6E, 0xFF, 0xFF },
	};

	constexpr unsigned int eventCount = 175;

	static int X64HudElemType(int x86Type)
	{
		if (x86Type == hudElemMapNameX86 || x86Type == hudElemGameTypeX86)
		{
			return hudElemText;
		}

		if (x86Type >= hudElemMaterialX86)
		{
			return x86Type - hudElemExtraTypesX86;
		}

		return x86Type;
	}

	static int X86HudElemType(int x64Type)
	{
		if (x64Type >= hudElemMaterialX86 - hudElemExtraTypesX86)
		{
			return x64Type + hudElemExtraTypesX86;
		}

		return x64Type;
	}

	static char X64ServerCommandLetter(char x86Letter)
	{
		switch (x86Letter)
		{
		case 'A': return 'w';
		case 'B': return 'x';
		case 'C': return 'y';
		case 'D': return 'z';
		case 'E': return 'A';
		case 'F': return 'B';
		case 'G': return 'C';
		case 'H': return 'D';
		case 'I': return 'E';
		case 'J': return 'F';
		case 'L': return 'H';
		case 'N': return 'J';
		case 'O': return 'K';
		case 'Q': return 'M';
		case 'R': return 'N';
		case 'S': return 'O';
		case 'T': return 'P';
		case 'U': return 'Q';
		case 'V': return 'R';
		case 'W': return 'S';
		case 'X': return 'T';
		case 'h': return 'U';
		case 'i': return 'V';
		case 'k': return 'h';
		case 'l': return 'i';
		case 'm': return 'j';
		case 'n': return 'k';
		case 'o': return 'l';
		case 'p': return 'm';
		case 'q': return 'n';
		case 'r': return 'o';
		case 's': return 'p';
		case 't': return 'q';
		case 'u': return 'r';
		case 'v': return 's';
		case 'w': return 't';
		case 'x': return 'u';
		case 'y': return 'v';
		case 'M': return Game::setStatCommand;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
		case 'g':
			return x86Letter;
		default:
			return '\0';
		}
	}

	static void WidenScoresCommand(const char* text, std::string& out)
	{
		std::vector<std::string_view> tokens;
		const std::string_view view(text);
		std::size_t start = 0;

		while (start < view.size())
		{
			std::size_t stop = view.find(' ', start);

			if (stop == std::string_view::npos)
			{
				stop = view.size();
			}

			if (stop > start)
			{
				tokens.push_back(view.substr(start, stop - start));
			}

			start = stop + 1;
		}

		constexpr std::size_t headerCount = 5;
		constexpr std::size_t x86RecordSize = 7;

		out.clear();

		for (std::size_t i = 0; i < tokens.size() && i < headerCount; ++i)
		{
			if (i > 0)
			{
				out.push_back(' ');
			}

			out.append(tokens[i]);
		}

		for (std::size_t record = headerCount; record + x86RecordSize <= tokens.size(); record += x86RecordSize)
		{
			for (std::size_t field = 0; field < x86RecordSize; ++field)
			{
				out.push_back(' ');
				out.append(tokens[record + field]);
			}

			out.append(" 0");
		}
	}

	constexpr std::uintptr_t clc_demoplaying = 0x140C3B934;

	constexpr std::uintptr_t clc_serverMessage = 0x140BFB7CC;
	constexpr int serverMessageSize = 256;

	bool Network::IsX86Server()
	{
		if (*reinterpret_cast<const int*>(Utils::Hook::Rebase(clc_demoplaying)))
		{
			return false;
		}

		const auto* const server = Game::clc_serverAddress;
		const Network::Address current(server);

		if (current.IsLoopback())
		{
			return false;
		}

		if (joinedServer.IsValid() && joinedServer == current)
		{
			return isJoinedServerX86;
		}

		return true;
	}

	constexpr std::uintptr_t CL_DispatchConnectionlessPacketCalls[] = { 0x1400FC775, 0x1400FC7D1 };

	constexpr std::uintptr_t LSP_SendHello = 0x1401B1100;
	constexpr std::uintptr_t LSP_SendLogRequest = 0x1401B1450;
	constexpr std::uintptr_t LSP_ParsePacket = 0x1401B0230;
	constexpr std::uintptr_t LSP_AddKeepAlive = 0x1401B05D0;
	constexpr std::uintptr_t NET_Config_LspSocketCall = 0x1402A76D2;

	static const std::uint8_t sendHelloEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x48 };
	static const std::uint8_t sendLogRequestEntry[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x83, 0x3D, 0x97, 0x0E, 0x85, 0x01, 0x00, 0x8B, 0xD9, 0x0F };
	static const std::uint8_t parsePacketEntry[] = { 0x40, 0x57, 0x48, 0x81, 0xEC, 0x30, 0x04, 0x00, 0x00, 0x48, 0x8B, 0xF9, 0xE8, 0xFF, 0x1D, 0x05 };
	static const std::uint8_t addKeepAliveEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xDA, 0x8B, 0xF9, 0x48 };
	static const std::uint8_t lspSocketCall[] = { 0xE8, 0x39, 0x04, 0x00, 0x00, 0x48, 0x63, 0xC8, 0x8B, 0xFB, 0x48, 0x89, 0x0D, 0x55, 0x4D, 0x4E };

	constexpr std::uintptr_t g_foundOurExternalIP = 0x146787081;
	constexpr std::uintptr_t Live_SetExternalIP_FoundIPWrite = 0x1402A4084;

	static const std::uint8_t foundIPWrite[] = { 0xC6, 0x05, 0xF6, 0x2F, 0x4E, 0x06, 0x01 };

	constexpr std::uintptr_t g_steamAuthState = 0x146787084;
	constexpr std::uintptr_t Live_SetSteamAuthState = 0x1402A4140;

	static const std::uint8_t setSteamAuthState[] = { 0x89, 0x0D, 0x3E, 0x2F, 0x4E, 0x06, 0x89, 0x15, 0x3C, 0x2F, 0x4E, 0x06, 0xC3 };

	constexpr std::uintptr_t NET_IsNatPunchReady = 0x1401B28B0;

	static const std::uint8_t natPunchReadyEntry[] = { 0x48, 0x83, 0xEC, 0x28, 0x83, 0x3D, 0x11, 0x0E, 0x85, 0x01, 0x04 };

	constexpr std::uintptr_t CL_RunOncePerClientFrame_Epilogue = 0x1400FD453;
	constexpr std::uintptr_t CL_RunOncePerClientFrame_UPNP_FrameTail = 0x1400FD458;

	static const std::uint8_t upnpFrameTail[] = { 0x48, 0x83, 0xC4, 0x30, 0x5B, 0xE9, 0x13, 0x7D, 0x0B, 0x00 };

	Network::Address::Address()
	{
		this->address.type = Game::NA_BAD;
	}

	Network::Address::Address(const std::string& text)
	{
		this->address.type = Game::NA_BAD;

		if (!text.empty())
		{
			Game::NET_StringToAdr(text.data(), &this->address);
		}
	}

	Network::Address::Address(const sockaddr* address)
	{
		Game::SockadrToNetadr(address, &this->address);
	}

	bool Network::Address::operator==(const Address& other) const
	{
		return Game::NET_CompareAdr(&this->address, &other.address);
	}

	void Network::Address::SetPort(unsigned short port)
	{
		this->address.port = htons(port);
	}

	unsigned short Network::Address::GetPort() const
	{
		return ntohs(this->address.port);
	}

	void Network::Address::SetIP(unsigned int ip)
	{
		this->address.ip.full = ip;
	}

	unsigned int Network::Address::GetIP() const
	{
		return this->address.ip.full;
	}

	void Network::Address::SetType(Game::netadrtype_t type)
	{
		this->address.type = type;
	}

	Game::netadrtype_t Network::Address::GetType() const
	{
		return this->address.type;
	}

	std::string Network::Address::GetString() const
	{
		return AdrToString(*this, true);
	}

	const char* Network::Address::GetCString() const
	{
		return AdrToString(*this, true);
	}

	bool Network::Address::IsValid() const
	{
		return this->address.type != Game::NA_BAD && this->address.type >= Game::NA_BOT && this->address.type <= Game::NA_IP;
	}

	sockaddr Network::Address::GetSockAddr() const
	{
		sockaddr sockAddr{};
		Game::NetadrToSockadr(&this->address, &sockAddr);
		return sockAddr;
	}

	bool Network::Address::IsLocal() const
	{
		const auto& ip = this->address.ip;

		if (ip.bytes[0] == 10)
		{
			return true;
		}

		if (ip.bytes[0] == 192 && ip.bytes[1] == 168)
		{
			return true;
		}

		if (ip.bytes[0] == 172 && ip.bytes[1] >= 16 && ip.bytes[1] < 32)
		{
			return true;
		}

		return ip.full == 0x0100007F;
	}

	bool Network::Address::IsSelf() const
	{
		if (Game::NET_IsLocalAddress(&this->address))
		{
			return true;
		}

		if (this->GetPort() != Network::GetPort())
		{
			return false;
		}

		for (auto i = 0; i < *Game::numIP; ++i)
		{
			if (this->address.ip.full == Game::localIP[i].full)
			{
				return true;
			}
		}

		return false;
	}

	bool Network::Address::IsLoopback() const
	{
		if (this->address.ip.full == 0x100007F)
		{
			return true;
		}

		return this->address.type == Game::NA_LOOPBACK;
	}

	const char* Network::AdrToString(const Address& address, bool withPort)
	{
		const auto* const raw = address.Get();

		switch (raw->type)
		{
		case Game::NA_LOOPBACK:
			return "loopback";

		case Game::NA_BOT:
			return "bot";

		case Game::NA_BAD:
			return "bad";

		default:
			break;
		}

		if (withPort)
		{
			return Utils::String::VA("%u.%u.%u.%u:%hu",
				raw->ip.bytes[0], raw->ip.bytes[1], raw->ip.bytes[2], raw->ip.bytes[3], address.GetPort());
		}

		return Utils::String::VA("%u.%u.%u.%u", raw->ip.bytes[0], raw->ip.bytes[1], raw->ip.bytes[2], raw->ip.bytes[3]);
	}

	void Network::SendRaw(Game::netsrc_t source, const Address& target, const std::string& data)
	{
		if (!target.IsValid())
		{
			return;
		}

		Game::NET_SendPacket(source, static_cast<int>(data.size()), data.data(), target.Get());
	}

	void Network::SendRaw(const Address& target, const std::string& data)
	{
		SendRaw(Game::NS_CLIENT1, target, data);
	}

	void Network::Send(Game::netsrc_t source, const Address& target, const std::string& data)
	{
		std::string packet;
		packet.reserve(4 + data.size());
		packet.append("\xFF\xFF\xFF\xFF", 4);
		packet.append(data);

		if (target.GetType() == Game::NA_LOOPBACK && packet.size() > 1400)
		{
			Logger::Error("network: {} bytes is too long for a loopback packet, not sent\n", packet.size());
			return;
		}

		SendRaw(source, target, packet);
	}

	void Network::Send(const Address& target, const std::string& data)
	{
		Send(Game::NS_CLIENT1, target, data);
	}

	void Network::SendCommand(Game::netsrc_t source, const Address& target,
		const std::string& command, const std::string& data)
	{
		std::string packet = command;
		packet.push_back('\n');
		packet.append(data);

		Send(source, target, packet);
	}

	void Network::SendCommand(const Address& target, const std::string& command, const std::string& data)
	{
		SendCommand(Game::NS_CLIENT1, target, command, data);
	}

	void Network::Broadcast(unsigned short port, const std::string& data)
	{
		Address target;
		target.SetType(Game::NA_BROADCAST);
		target.SetPort(port);

		Send(Game::NS_CLIENT1, target, data);
	}

	void Network::BroadcastRange(unsigned int min, unsigned int max, const std::string& data)
	{
		for (unsigned int i = min; i < max; ++i)
		{
			Broadcast(static_cast<unsigned short>(i & 0xFFFF), data);
		}
	}

	std::uint16_t Network::GetPort()
	{
		assert(*Game::port);
		return static_cast<std::uint16_t>((*Game::port)->current.unsignedInt);
	}

	void Network::OnPacket(const std::string& command, const Callback& callback)
	{
		callbacks[Utils::String::ToLower(command)] = callback;
	}

	void Network::OnPacketRaw(const std::string& command, const RawCallback& callback)
	{
		rawCallbacks[Utils::String::ToLower(command)] = callback;
	}

	bool Network::HandleCommand(Game::netadr_t* from, const char* command, Game::msg_t* message)
	{
		if (!from || !command || !message)
		{
			return false;
		}

		const auto name = Utils::String::ToLower(command);

		const auto raw = rawCallbacks.find(name);

		if (raw != rawCallbacks.end())
		{
			raw->second(from, message);
			return true;
		}

		const auto handler = callbacks.find(name);

		if (handler == callbacks.end())
		{
			return false;
		}

		const auto offset = name.size() + 5;

		if (static_cast<std::size_t>(message->cursize) < offset)
		{
			return false;
		}

		std::string data(reinterpret_cast<const char*>(message->data) + offset,
			static_cast<std::size_t>(message->cursize) - offset);

		Address address(from);
		handler->second(address, data);

		return true;
	}

	bool Network::CL_DispatchConnectionlessPacket_Hook(int localClientNum, Game::netadr_t* from,
		Game::msg_t* msg, int fourth)
	{
		const auto& args = *Game::cmd_args;

		if (args.nesting >= 0 && args.nesting < 8 && args.argc[args.nesting] > 0)
		{
			const char* const command = args.argv[args.nesting][0];

			if (isNetDebug && Utils::String::ToLower(command ? command : "") != "inforesponse")
			{
				Address source(from);

				std::string head;

				if (msg && msg->data && msg->cursize > 0)
				{
					const auto shown = std::min(msg->cursize, 24);

					for (int at = 0; at < shown; ++at)
					{
						const auto byte = reinterpret_cast<const unsigned char*>(msg->data)[at];

						head.append(Utils::String::Format("{:02X} ", byte));
					}

					head.append("| ");

					for (int at = 0; at < shown; ++at)
					{
						const auto byte = reinterpret_cast<const unsigned char*>(msg->data)[at];

						head.push_back(byte >= 0x20 && byte < 0x7F ? static_cast<char>(byte) : '.');
					}
				}

				Logger::Print("oob <- {} size {} cmd [{}] raw {}", source.GetString(),
					msg ? msg->cursize : -1, command ? command : "(null)", head);
			}

			if (HandleCommand(from, command, msg))
			{
				return true;
			}
		}

		return reinterpret_cast<bool(*)(int, Game::netadr_t*, Game::msg_t*, int)>(
			dispatchHooks[0].GetOriginal())(localClientNum, from, msg, fourth);
	}

	constexpr std::uintptr_t NET_OutOfBandPrint_LargeLocalSize = 0x140206AA8;
	constexpr std::uintptr_t NET_OutOfBandPrint_LengthCompare = 0x140206AE2;
	constexpr std::uintptr_t NET_Config_PortAttempts = 0x1402A771C;
	constexpr std::uintptr_t CL_InitOnceForAllClients_MaxPacketsDefault = 0x1400FB2EA;
	constexpr std::uintptr_t CL_InitOnceForAllClients_MaxPacketsMax = 0x1400FB2F9;
	constexpr std::uintptr_t CL_InitOnceForAllClients_SnapsDefault = 0x1400FB64E;
	static const std::uint8_t largeLocalSize[] = { 0xBA, 0x00, 0x08, 0x00, 0x00 };
	static const std::uint8_t lengthCompare[] = { 0x81, 0xFE, 0x00, 0x08, 0x00, 0x00 };
	static const std::uint8_t portAttempts[] = { 0x83, 0xFF, 0x0A };
	static const std::uint8_t maxPacketsDefault[] = { 0x8D, 0x53, 0x1E };
	static const std::uint8_t maxPacketsMax[] = { 0x44, 0x8D, 0x4B, 0x64 };
	static const std::uint8_t snapsDefault[] = { 0x8D, 0x53, 0x14 };
	constexpr std::uint32_t outOfBandPrintSize = 0x1FFFC;

	constexpr std::uintptr_t CL_ConnectionlessPacket_SteamAuthJnz = 0x1400F9C46;
	constexpr std::uintptr_t CL_ConnectionlessPacket_VoiceFailJnz = 0x1400F977E;
	static const std::uint8_t steamAuthJnz[] = { 0x0F, 0x85, 0x9D, 0x00, 0x00, 0x00 };
	static const std::uint8_t voiceFailJnz[] = { 0x75, 0x3F };

	constexpr std::uintptr_t getBuildNumberAsInt = 0x1401E6D40;
	static const std::uint8_t buildNumberBody[] = { 0xB8, 0x0D, 0x00, 0x00, 0x00, 0xC3 };
	constexpr std::uint32_t buildNumber = 208;

	static void RaiseLimits()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(NET_OutOfBandPrint_LargeLocalSize, largeLocalSize, sizeof(largeLocalSize))
			&& Utils::Hook::MatchesBytes(NET_OutOfBandPrint_LengthCompare, lengthCompare, sizeof(lengthCompare))
			&& Utils::Hook::MatchesBytes(NET_Config_PortAttempts, portAttempts, sizeof(portAttempts))
			&& Utils::Hook::MatchesBytes(CL_InitOnceForAllClients_MaxPacketsDefault, maxPacketsDefault, sizeof(maxPacketsDefault))
			&& Utils::Hook::MatchesBytes(CL_InitOnceForAllClients_MaxPacketsMax, maxPacketsMax, sizeof(maxPacketsMax))
			&& Utils::Hook::MatchesBytes(CL_InitOnceForAllClients_SnapsDefault, snapsDefault, sizeof(snapsDefault));

		if (!isExpected)
		{
			Logger::Error("network: the packet limits do not read as expected, left at the engine's\n");
			return;
		}

		Utils::Hook::Set<std::uint32_t>(NET_OutOfBandPrint_LargeLocalSize + 1, outOfBandPrintSize);
		Utils::Hook::Set<std::uint32_t>(NET_OutOfBandPrint_LengthCompare + 2, outOfBandPrintSize);

		Utils::Hook::Set<std::uint8_t>(NET_Config_PortAttempts + 2, 100);

		Utils::Hook::Set<std::uint8_t>(CL_InitOnceForAllClients_MaxPacketsMax + 3, 125);

		Utils::Hook::Set<std::uint8_t>(CL_InitOnceForAllClients_SnapsDefault + 2, 30);
		Utils::Hook::Set<std::uint8_t>(CL_InitOnceForAllClients_MaxPacketsDefault + 2, 125);
	}

	static void DisableUnusedPackets()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(CL_ConnectionlessPacket_SteamAuthJnz, steamAuthJnz, sizeof(steamAuthJnz))
			&& Utils::Hook::MatchesBytes(CL_ConnectionlessPacket_VoiceFailJnz, voiceFailJnz, sizeof(voiceFailJnz));

		if (!isExpected)
		{
			Logger::Error("network: CL_ConnectionlessPacket does not read as expected, steamauthReq and voicefail stay handled\n");
			return;
		}

		const std::int32_t steamAuthTarget = 0x9D + 1;
		Utils::Hook::Set<std::uint8_t>(CL_ConnectionlessPacket_SteamAuthJnz, 0xE9);
		Utils::Hook::Set<std::int32_t>(CL_ConnectionlessPacket_SteamAuthJnz + 1, steamAuthTarget);
		Utils::Hook::Nop(CL_ConnectionlessPacket_SteamAuthJnz + 5, 1);

		Utils::Hook::Set<std::uint8_t>(CL_ConnectionlessPacket_VoiceFailJnz, 0xEB);
	}

	static void SetBuildNumber()
	{
		if (!Utils::Hook::MatchesBytes(getBuildNumberAsInt, buildNumberBody, sizeof(buildNumberBody)))
		{
			Logger::Error("network: getBuildNumberAsInt does not read as expected, it stays 13\n");
			return;
		}

		Utils::Hook::Set<std::uint32_t>(getBuildNumberAsInt + 1, buildNumber);
	}

	static void DisableLSP()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(LSP_SendHello, sendHelloEntry, sizeof(sendHelloEntry))
			&& Utils::Hook::MatchesBytes(LSP_SendLogRequest, sendLogRequestEntry, sizeof(sendLogRequestEntry))
			&& Utils::Hook::MatchesBytes(LSP_ParsePacket, parsePacketEntry, sizeof(parsePacketEntry))
			&& Utils::Hook::MatchesBytes(LSP_AddKeepAlive, addKeepAliveEntry, sizeof(addKeepAliveEntry))
			&& Utils::Hook::MatchesBytes(NET_Config_LspSocketCall, lspSocketCall, sizeof(lspSocketCall));

		if (!isExpected)
		{
			Logger::Error("network: the lsp functions do not read as expected, left alone, so lsp stays on\n");
			return;
		}

		Utils::Hook::Set<std::uint8_t>(LSP_SendHello, 0xC3);
		Utils::Hook::Set<std::uint8_t>(LSP_SendLogRequest, 0xC3);
		Utils::Hook::Set<std::uint8_t>(LSP_ParsePacket, 0xC3);
		Utils::Hook::Set<std::uint8_t>(LSP_AddKeepAlive, 0xC3);

		Utils::Hook::Set<std::uint8_t>(NET_Config_LspSocketCall, 0x33);
		Utils::Hook::Set<std::uint8_t>(NET_Config_LspSocketCall + 1, 0xC0);
		Utils::Hook::Nop(NET_Config_LspSocketCall + 2, 3);
	}

	static void DisableIWNet()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(Live_SetExternalIP_FoundIPWrite, foundIPWrite, sizeof(foundIPWrite))
			&& Utils::Hook::MatchesBytes(Live_SetSteamAuthState, setSteamAuthState, sizeof(setSteamAuthState))
			&& Utils::Hook::MatchesBytes(NET_IsNatPunchReady, natPunchReadyEntry, sizeof(natPunchReadyEntry))
			&& Utils::Hook::MatchesBytes(CL_RunOncePerClientFrame_Epilogue, upnpFrameTail, sizeof(upnpFrameTail));

		if (!isExpected)
		{
			Logger::Error("network: the iwnet gates do not read as expected, left alone, so joining waits on iwnet\n");
			return;
		}

		Utils::Hook::Set<std::uint8_t>(CL_RunOncePerClientFrame_UPNP_FrameTail, 0xC3);
		Utils::Hook::Nop(CL_RunOncePerClientFrame_UPNP_FrameTail + 1, 4);

		Utils::Hook::Set<std::uint8_t>(g_foundOurExternalIP, 1);
		Utils::Hook::Set<std::int32_t>(g_steamAuthState, 2);

		Utils::Hook::Set<std::uint8_t>(NET_IsNatPunchReady, 0xB0);
		Utils::Hook::Set<std::uint8_t>(NET_IsNatPunchReady + 1, 0x01);
		Utils::Hook::Set<std::uint8_t>(NET_IsNatPunchReady + 2, 0xC3);
	}

	void Network::RecordServerBuild(const Address& server, bool isX86)
	{
		joinedServer = server;
		isJoinedServerX86 = isX86;
	}

	int Network::ReadServerOp(Game::msg_t* msg)
	{
		const auto readByte = reinterpret_cast<int(*)(Game::msg_t*)>(serverOpHooks[0].GetOriginal());

		int op = readByte(msg);

		if (!IsX86Server())
		{
			return op;
		}

		while (op == svc_nopX86)
		{
			op = readByte(msg);
		}

		if (op < 0 || op >= static_cast<int>(std::size(x64OpForX86Op)))
		{
			return op;
		}

		return x64OpForX86Op[op];
	}

	void Network::RemapServerOps()
	{
		for (std::size_t i = 0; i < std::size(serverOpReads); ++i)
		{
			if (!Utils::Hook::MatchesBytes(serverOpReads[i], serverOpReadBytes[i], sizeof(serverOpReadBytes[i])))
			{
				Logger::Error("network: a server op read does not read as expected, left alone, so a 1.2.211 server cannot be joined\n");
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(serverOpReads); ++i)
		{
			isSeated = serverOpHooks[i].Initialize(serverOpReads[i], reinterpret_cast<void*>(ReadServerOp), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : serverOpHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the server op remap, so a 1.2.211 server cannot be joined\n");
			return;
		}

		for (auto& hook : serverOpHooks)
		{
			hook.Quick();
		}
	}

	void Network::TokenizeServerCommand(const char* text)
	{
		const auto tokenize = reinterpret_cast<void(*)(const char*)>(serverCommandHook.GetOriginal());

		if (!text || !*text || !IsX86Server())
		{
			tokenize(text);
			return;
		}

		const unsigned char first = static_cast<unsigned char>(text[0]);

		if (first >= 0x14 && first <= 0x16)
		{
			tokenize(text);
			return;
		}

		static std::string translated;

		if (first == 'b')
		{
			WidenScoresCommand(text, translated);
			tokenize(translated.data());
			return;
		}

		const char letter = X64ServerCommandLetter(static_cast<char>(first));

		if (!letter)
		{
			tokenize("");
			return;
		}

		translated.assign(text);
		translated[0] = letter;

		tokenize(translated.data());
	}

	void Network::RemapServerCommands()
	{
		if (!Utils::Hook::MatchesBytes(CL_CGameNeedsServerCommand_TokenizeCall, tokenizeCall, sizeof(tokenizeCall)))
		{
			Logger::Error("network: the server command tokenize call does not read as expected, left alone, so a 1.2.211 server's commands will misfire\n");
			return;
		}

		if (!serverCommandHook.Initialize(CL_CGameNeedsServerCommand_TokenizeCall, reinterpret_cast<void*>(TokenizeServerCommand), HOOK_CALL)
			->Install()->IsInstalled())
		{
			Logger::Error("network: could not seat the server command translation, so a 1.2.211 server's commands will misfire\n");
			return;
		}

		serverCommandHook.Quick();
	}

	bool Network::FS_PureServerSetLoadedIwds_Hook(const char* iwdSums, const char* iwdNames)
	{
		const auto setLoadedIwds = reinterpret_cast<bool(*)(const char*, const char*)>(fileCheckHooks[0].GetOriginal());

		if (IsX86Server())
		{
			return setLoadedIwds("", "");
		}

		if (setLoadedIwds(iwdSums, iwdNames))
		{
			*reinterpret_cast<int*>(Utils::Hook::Rebase(cls_doVidRestart)) = 1;
		}

		return false;
	}

	void Network::Com_Error_ModifiedFiles_Hook([[maybe_unused]] int code, [[maybe_unused]] const char* message)
	{
	}

	void Network::CL_CompareFilesWithServer_Hook()
	{
	}

	void Network::MSG_SetDefaultUserCmd_Hook(const std::uint8_t* ps, Game::usercmd_s* cmd)
	{
		reinterpret_cast<void(*)(const std::uint8_t*, Game::usercmd_s*)>(defaultUserCmdHooks[0].GetOriginal())(ps, cmd);

		if (!IsX86Server())
		{
			return;
		}

		const int pm_flags = *reinterpret_cast<const int*>(ps + playerStatePmFlags);
		const int otherFlags = *reinterpret_cast<const int*>(ps + playerStateOtherFlags);

		if ((otherFlags & POF_PLAYER) && (pm_flags & PMF_SPRINTING))
		{
			cmd->buttons |= sprintButton;
		}
	}

	void Network::KeepSprintInDefaultUserCmd()
	{
		for (std::size_t i = 0; i < std::size(defaultUserCmdSites); ++i)
		{
			if (!Utils::Hook::MatchesBytes(defaultUserCmdSites[i], defaultUserCmdSiteBytes[i], sizeof(defaultUserCmdSiteBytes[i])))
			{
				Logger::Error("network: a MSG_SetDefaultUserCmd site does not read as expected, left alone, so sprint sticks on a 1.2.211 server\n");
				return;
			}
		}

		const bool asJump[] = { HOOK_JUMP, HOOK_CALL };
		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(defaultUserCmdSites); ++i)
		{
			isSeated = defaultUserCmdHooks[i].Initialize(defaultUserCmdSites[i], reinterpret_cast<void*>(MSG_SetDefaultUserCmd_Hook), asJump[i])
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : defaultUserCmdHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the default usercmd hooks, so sprint sticks on a 1.2.211 server\n");
			return;
		}

		for (auto& hook : defaultUserCmdHooks)
		{
			hook.Quick();
		}
	}

	int Network::CL_GetSnapshot_Hook(int localClientNum, int snapshotNumber, std::uint8_t* snapshot)
	{
		const auto getSnapshot = reinterpret_cast<int(*)(int, int, std::uint8_t*)>(snapshotHooks[0].GetOriginal());
		const int result = getSnapshot(localClientNum, snapshotNumber, snapshot);

		if (!result || !IsX86Server())
		{
			return result;
		}

		for (const std::ptrdiff_t hudElems : playerStateHudElems)
		{
			for (int i = 0; i < hudElemCount; ++i)
			{
				auto* const type = reinterpret_cast<int*>(snapshot + hudElems + i * hudElemSize);

				if (*type == hudElemFree)
				{
					break;
				}

				*type = X64HudElemType(*type);
			}
		}

		if (Weapon::IsLimitRaised())
		{
			return result;
		}

		int entities = *reinterpret_cast<const int*>(snapshot + snapshotNumEntities);

		if (entities > maxSnapshotEntities)
		{
			entities = maxSnapshotEntities;
		}

		for (int i = 0; i < entities; ++i)
		{
			std::uint8_t* const entity = snapshot + snapshotEntities + i * entityStateSize;

			if (*reinterpret_cast<const int*>(entity + entityStateType) != ET_ITEM)
			{
				continue;
			}

			auto* const item = reinterpret_cast<int*>(entity + entityStateItemIndex);
			const int weapon = *item % weaponModelStrideX86;
			const int model = *item / weaponModelStrideX86;

			*item = weapon + model * weaponModelStride;
		}

		return result;
	}

	void Network::CL_AddReliableCommand_Hook(int localClientNum, const char* text)
	{
		const auto addReliableCommand = reinterpret_cast<void(*)(int, const char*)>(scoresRequestHooks[0].GetOriginal());

		if (IsX86Server() && text && std::strcmp(text, scoresRequest) == 0)
		{
			addReliableCommand(localClientNum, scoresRequestX86);
			return;
		}

		addReliableCommand(localClientNum, text);
	}

	int Network::CL_CheckNotify_Com_sprintf_Hook(char* command, int size, const char* format, int binding)
	{
		const auto comSprintf = reinterpret_cast<int(*)(char*, int, const char*, ...)>(notifyCommandHook.GetOriginal());

		if (IsX86Server() && binding > 0 && binding < bindCommandCount)
		{
			const auto* const bindCommands = reinterpret_cast<const char* const*>(Utils::Hook::Rebase(g_bindCommands));
			const char* const text = bindCommands[binding];

			if (text && *text)
			{
				return comSprintf(command, size, "nt %s", text);
			}
		}

		return comSprintf(command, size, format, binding);
	}

	void Network::TranslateNotifyCommand()
	{
		if (!Utils::Hook::MatchesBytes(CL_CheckNotify_Com_sprintfCall, notifyFormatCallBytes, sizeof(notifyFormatCallBytes)))
		{
			Logger::Error("network: CL_CheckNotify does not read as expected, left alone, so a 1.2.211 server rejects every key press\n");
			return;
		}

		if (!notifyCommandHook.Initialize(CL_CheckNotify_Com_sprintfCall, reinterpret_cast<void*>(CL_CheckNotify_Com_sprintf_Hook), HOOK_CALL)
			->Install()->IsInstalled())
		{
			Logger::Error("network: could not seat the notify command translation, so a 1.2.211 server rejects every key press\n");
			return;
		}

		notifyCommandHook.Quick();
	}

	void Network::OnEntityEvent(const EntityEventObserver& observer)
	{
		entityEventObservers.push_back(observer);
	}

	void Network::CG_EntityEvent_Hook(unsigned int localClientNum, void* entity, unsigned int event, std::uint8_t isPlayer)
	{
		for (const auto& observer : entityEventObservers)
		{
			if (observer(static_cast<int>(localClientNum), static_cast<Game::centity_s*>(entity), static_cast<int>(event)))
			{
				return;
			}
		}

		if (IsX86Server() && event >= eventCount)
		{
			return;
		}

		reinterpret_cast<void(*)(unsigned int, void*, unsigned int, std::uint8_t)>(entityEventHooks[0].GetOriginal())(localClientNum, entity, event, isPlayer);
	}

	void Network::DropEventsWeDoNotHave()
	{
		for (std::size_t i = 0; i < std::size(CG_EntityEventCalls); ++i)
		{
			if (!Utils::Hook::MatchesBytes(CG_EntityEventCalls[i], entityEventCallBytes[i], sizeof(entityEventCallBytes[i])))
			{
				Logger::Error("network: an entity event call does not read as expected, left alone, so an IW4x rumble event drops us\n");
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(CG_EntityEventCalls); ++i)
		{
			isSeated = entityEventHooks[i].Initialize(CG_EntityEventCalls[i], reinterpret_cast<void*>(CG_EntityEvent_Hook), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : entityEventHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the entity event filter, so an IW4x rumble event drops us\n");
			return;
		}

		for (auto& hook : entityEventHooks)
		{
			hook.Quick();
		}
	}

	void Network::RenameScoresRequest()
	{
		for (std::size_t i = 0; i < std::size(scoresRequestSites); ++i)
		{
			if (!Utils::Hook::MatchesBytes(scoresRequestSites[i], scoresRequestSiteBytes[i], sizeof(scoresRequestSiteBytes[i])))
			{
				Logger::Error("network: a scores request does not read as expected, left alone, so the scoreboard stays empty on a 1.2.211 server\n");
				return;
			}
		}

		const bool asJump[] = { HOOK_CALL, HOOK_CALL, HOOK_JUMP };
		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(scoresRequestSites); ++i)
		{
			isSeated = scoresRequestHooks[i].Initialize(scoresRequestSites[i], reinterpret_cast<void*>(CL_AddReliableCommand_Hook), asJump[i])
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : scoresRequestHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the scores request rename, so the scoreboard stays empty on a 1.2.211 server\n");
			return;
		}

		for (auto& hook : scoresRequestHooks)
		{
			hook.Quick();
		}
	}

	bool Network::HasSnapshotHook(std::uintptr_t site)
	{
		for (std::size_t i = 0; i < std::size(CG_ProcessSnapshots_GetSnapshotCalls); ++i)
		{
			if (CG_ProcessSnapshots_GetSnapshotCalls[i] == site)
			{
				return hasSnapshotHooks;
			}
		}

		return false;
	}

	void Network::TranslateSnapshotForX86()
	{
		for (std::size_t i = 0; i < std::size(CG_ProcessSnapshots_GetSnapshotCalls); ++i)
		{
			if (!Utils::Hook::MatchesBytes(CG_ProcessSnapshots_GetSnapshotCalls[i], getSnapshotCallBytes[i], sizeof(getSnapshotCallBytes[i])))
			{
				Logger::Error("network: a snapshot hand-off does not read as expected, left alone, so a 1.2.211 server's hud and dropped weapons come out wrong\n");
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(CG_ProcessSnapshots_GetSnapshotCalls); ++i)
		{
			isSeated = snapshotHooks[i].Initialize(CG_ProcessSnapshots_GetSnapshotCalls[i], reinterpret_cast<void*>(CL_GetSnapshot_Hook), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : snapshotHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the snapshot translation, so a 1.2.211 server's hud and dropped weapons come out wrong\n");
			return;
		}

		for (auto& hook : snapshotHooks)
		{
			hook.Quick();
		}

		hasSnapshotHooks = true;
	}

	void Network::SkipServerFileChecks()
	{
		if (Utils::Hook::MatchesBytes(FS_ServerSetReferencedFiles_NameTest, referencedNameTest, sizeof(referencedNameTest))
			&& Utils::Hook::MatchesBytes(FS_ServerSetReferencedFiles_CopyName, referencedNameCopy, sizeof(referencedNameCopy)))
		{
			const auto relative = static_cast<std::int8_t>(FS_ServerSetReferencedFiles_CopyName - (FS_ServerSetReferencedFiles_NameTest + 2));

			Utils::Hook::Set<std::int8_t>(FS_ServerSetReferencedFiles_NameTest + 1, relative);
			Utils::Hook::Set<std::uint8_t>(FS_ServerSetReferencedFiles_NameTest, 0xEB);
		}
		else
		{
			Logger::Error("network: FS_ServerSetReferencedFiles does not read as expected, a server's odd package name still drops us\n");
		}

		for (std::size_t i = 0; i < std::size(fileCheckCalls); ++i)
		{
			if (!Utils::Hook::MatchesBytes(fileCheckCalls[i], fileCheckCallBytes[i], sizeof(fileCheckCallBytes[i])))
			{
				Logger::Error("network: a file check call does not read as expected, left alone, so a 1.2.211 server drops us for its files\n");
				return;
			}
		}

		void* const replacements[] =
		{
			reinterpret_cast<void*>(FS_PureServerSetLoadedIwds_Hook),
			reinterpret_cast<void*>(Com_Error_ModifiedFiles_Hook),
			reinterpret_cast<void*>(CL_CompareFilesWithServer_Hook),
		};

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(fileCheckCalls); ++i)
		{
			isSeated = fileCheckHooks[i].Initialize(fileCheckCalls[i], replacements[i], HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : fileCheckHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the file check skips, so a 1.2.211 server drops us for its files\n");
			return;
		}

		for (auto& hook : fileCheckHooks)
		{
			hook.Quick();
		}
	}

	enum class ClientBuild : std::int8_t
	{
		Unknown,
		X86,
		X64,
	};

	static Dvar::Var x64Marker;
	static ClientBuild clientBuilds[Game::MAX_CLIENTS]{};

	constexpr std::uintptr_t SV_AddServerCommandCalls[] = { 0x140236557, 0x14023D216, 0x14023D23D };
	constexpr std::uintptr_t SV_AddServerCommand = 0x14023BAB0;

	constexpr std::uintptr_t SV_ExecuteClientCommand_TokenizeCall = 0x1402370A5;
	constexpr std::uintptr_t SV_Cmd_TokenizeString = 0x1401E8370;
	constexpr std::uintptr_t SV_ExecuteClientCommand_FloodExemptCall = 0x140237042;
	constexpr std::uintptr_t Q_strncmp = 0x14028C340;
	constexpr auto notifyX86Prefix = "nt ";

	constexpr std::uintptr_t SV_UserMove_DefaultUserCmdCall = 0x14023967B;
	constexpr std::uintptr_t MSG_SetDefaultUserCmd = 0x140202710;
	constexpr std::uintptr_t SV_GetPlayerstateForClientNum = 0x140233470;

	constexpr std::uintptr_t MSG_WriteByte = 0x140202A40;

	struct ServerOpWrite
	{
		std::uintptr_t address;
		bool isJump;
		void(*stub)();
	};

	static constexpr int x86OpForX64Op[] = { 5, 3, 1, 2, 4, 6 };

	extern "C"
	{
		void SV_WriteGameState_MSG_WriteByte();
		void SV_WriteGameState_MSG_WriteByteTail();
		void MSG_WriteByte_ClientInRbx();
		void MSG_WriteByte_ClientInRsi();
		void MSG_WriteByte_ClientInRdi();
		void MSG_WriteByte_ClientInR13();

		void Network_MSG_WriteByte(Game::msg_t* msg, int op, const Game::client_s* client)
		{
			if (op < 0 || op >= static_cast<int>(std::size(x86OpForX64Op)) || !Network::IsX86Client(client))
			{
				Game::MSG_WriteByte(msg, op);
				return;
			}

			Game::MSG_WriteByte(msg, x86OpForX64Op[op]);
		}
	}

	static const ServerOpWrite serverOpWrites[] =
	{
		{ 0x140238DEC, false, SV_WriteGameState_MSG_WriteByte },
		{ 0x140238E3C, false, SV_WriteGameState_MSG_WriteByte },
		{ 0x14023904B, false, SV_WriteGameState_MSG_WriteByte },
		{ 0x1402390AC, true, SV_WriteGameState_MSG_WriteByteTail },
		{ 0x140241578, false, MSG_WriteByte_ClientInRdi },
		{ 0x140241976, false, MSG_WriteByte_ClientInRsi },
		{ 0x140241A19, false, MSG_WriteByte_ClientInRsi },
		{ 0x140241A5E, false, MSG_WriteByte_ClientInRsi },
		{ 0x140241F7A, false, MSG_WriteByte_ClientInRbx },
		{ 0x140241FF4, false, MSG_WriteByte_ClientInRbx },
		{ 0x140242098, false, MSG_WriteByte_ClientInRbx },
		{ 0x1402420E6, false, MSG_WriteByte_ClientInRbx },
		{ 0x140242968, false, MSG_WriteByte_ClientInRdi },
		{ 0x140242C37, false, MSG_WriteByte_ClientInR13 },
	};

	constexpr std::uintptr_t SV_WriteGameState_ClientSpill = 0x140238DCA;
	constexpr std::uintptr_t SV_WriteGameState_ClientReload = 0x140239050;

	static const std::uint8_t clientSpill[] = { 0x48, 0x89, 0x4C, 0x24, 0x08 };
	static const std::uint8_t clientReload[] = { 0x48, 0x8B, 0x4C, 0x24, 0x60 };

	constexpr std::uintptr_t SV_WriteSnapshotToClient_DeltaCall = 0x140242CC6;
	constexpr std::uintptr_t MSG_WriteDeltaPlayerstate = 0x140208920;
	constexpr std::ptrdiff_t snapshotInfoClient = 0x10;
	constexpr std::size_t playerStateSize = 0x311C;

	static void ToX86HudElemTypes(std::uint8_t* playerState)
	{
		for (const std::ptrdiff_t hudElems : playerStateHudElems)
		{
			for (int i = 0; i < hudElemCount; ++i)
			{
				auto* const type = reinterpret_cast<int*>(playerState + hudElems + i * hudElemSize);

				if (*type == hudElemFree)
				{
					continue;
				}

				*type = X86HudElemType(*type);
			}
		}
	}

	static char X86ServerCommandLetter(char x64Letter)
	{
		static const auto inverse = []
		{
			std::array<char, 128> table{};

			for (int x86Letter = 1; x86Letter < 128; ++x86Letter)
			{
				const char mapped = X64ServerCommandLetter(static_cast<char>(x86Letter));

				if (mapped > 0)
				{
					table[static_cast<unsigned char>(mapped)] = static_cast<char>(x86Letter);
				}
			}

			return table;
		}();

		const auto index = static_cast<unsigned char>(x64Letter);

		if (index >= inverse.size())
		{
			return '\0';
		}

		return inverse[index];
	}

	static void NarrowScoresCommand(const char* text, std::string& out)
	{
		std::vector<std::string_view> tokens;
		const std::string_view view(text);
		std::size_t start = 0;

		while (start < view.size())
		{
			std::size_t stop = view.find(' ', start);

			if (stop == std::string_view::npos)
			{
				stop = view.size();
			}

			if (stop > start)
			{
				tokens.push_back(view.substr(start, stop - start));
			}

			start = stop + 1;
		}

		constexpr std::size_t headerCount = 5;
		constexpr std::size_t x64RecordSize = 8;
		constexpr std::size_t x86RecordSize = 7;

		out.clear();

		for (std::size_t i = 0; i < tokens.size() && i < headerCount; ++i)
		{
			if (i > 0)
			{
				out.push_back(' ');
			}

			out.append(tokens[i]);
		}

		for (std::size_t record = headerCount; record + x64RecordSize <= tokens.size(); record += x64RecordSize)
		{
			for (std::size_t field = 0; field < x86RecordSize; ++field)
			{
				out.push_back(' ');
				out.append(tokens[record + field]);
			}
		}
	}

	bool Network::IsX86Client(const Game::client_s* client)
	{
		if (!client || client->bIsTestClient)
		{
			return false;
		}

		const std::ptrdiff_t index = client - Game::svs_clients;

		if (index < 0 || index >= static_cast<std::ptrdiff_t>(Game::MAX_CLIENTS))
		{
			return false;
		}

		auto& build = clientBuilds[index];

		if (build == ClientBuild::Unknown)
		{
			const Utils::InfoString userinfo(client->userinfo);

			if (userinfo.Get("x64").empty())
			{
				build = ClientBuild::X86;
			}
			else
			{
				build = ClientBuild::X64;
			}
		}

		return build == ClientBuild::X86;
	}

	void Network::SV_AddServerCommand_Hook(Game::client_s* client, int type, const char* text)
	{
		const auto addServerCommand = reinterpret_cast<void(*)(Game::client_s*, int, const char*)>(addServerCommandHooks[0].GetOriginal());

		if (!text || !*text || !IsX86Client(client))
		{
			addServerCommand(client, type, text);
			return;
		}

		const unsigned char first = static_cast<unsigned char>(text[0]);

		if (first >= 0x14 && first <= 0x16)
		{
			addServerCommand(client, type, text);
			return;
		}

		std::string translated;

		if (first == 'b')
		{
			NarrowScoresCommand(text, translated);
			addServerCommand(client, type, translated.data());
			return;
		}

		const char letter = X86ServerCommandLetter(static_cast<char>(first));

		if (!letter)
		{
			return;
		}

		translated.assign(text);
		translated[0] = letter;

		addServerCommand(client, type, translated.data());
	}

	void Network::SV_Cmd_TokenizeString_Hook(const char* text)
	{
		const auto tokenize = reinterpret_cast<void(*)(const char*)>(clientCommandTokenizeHook.GetOriginal());

		if (text && std::strcmp(text, scoresRequestX86) == 0)
		{
			tokenize(scoresRequest);
			return;
		}

		if (text && std::strncmp(text, notifyX86Prefix, std::strlen(notifyX86Prefix)) == 0)
		{
			const char* const command = text + std::strlen(notifyX86Prefix);
			const auto* const bindCommands = reinterpret_cast<const char* const*>(Utils::Hook::Rebase(g_bindCommands));

			for (int binding = 1; binding < bindCommandCount; ++binding)
			{
				if (bindCommands[binding] && std::strcmp(bindCommands[binding], command) == 0)
				{
					tokenize(Utils::String::VA("n %i", binding));
					return;
				}
			}

			tokenize("");
			return;
		}

		tokenize(text);
	}

	int Network::SV_ExecuteClientCommand_Q_strncmp_Hook(const char* prefix, const char* text, int count)
	{
		const int result = reinterpret_cast<int(*)(const char*, const char*, int)>(clientCommandFloodHook.GetOriginal())(prefix, text, count);

		if (result != 0 && text && std::strncmp(text, notifyX86Prefix, std::strlen(notifyX86Prefix)) == 0)
		{
			return 0;
		}

		return result;
	}

	void Network::SV_UserMove_MSG_SetDefaultUserCmd_Hook(const std::uint8_t* ps, Game::usercmd_s* cmd)
	{
		reinterpret_cast<void(*)(const std::uint8_t*, Game::usercmd_s*)>(serverDefaultUserCmdHook.GetOriginal())(ps, cmd);

		const auto getPlayerState = reinterpret_cast<const std::uint8_t*(*)(int)>(Utils::Hook::Rebase(SV_GetPlayerstateForClientNum));
		const Game::client_s* client = nullptr;

		for (int i = 0; i < *Game::svs_clientCount; ++i)
		{
			if (getPlayerState(i) == ps)
			{
				client = &Game::svs_clients[i];
				break;
			}
		}

		if (!IsX86Client(client))
		{
			return;
		}

		const int pm_flags = *reinterpret_cast<const int*>(ps + playerStatePmFlags);
		const int otherFlags = *reinterpret_cast<const int*>(ps + playerStateOtherFlags);

		if ((otherFlags & POF_PLAYER) && (pm_flags & PMF_SPRINTING))
		{
			cmd->buttons |= sprintButton;
		}
	}

	void Network::HostX86ClientCommands()
	{
		bool isExpected = Utils::Hook::BranchesTo(SV_ExecuteClientCommand_TokenizeCall, SV_Cmd_TokenizeString, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_ExecuteClientCommand_FloodExemptCall, Q_strncmp, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_UserMove_DefaultUserCmdCall, MSG_SetDefaultUserCmd, HOOK_CALL);

		for (const std::uintptr_t call : SV_AddServerCommandCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, SV_AddServerCommand, HOOK_CALL);
		}

		if (!isExpected)
		{
			Logger::Error("network: a server command site does not read as expected, left alone, so a 1.2.211 client cannot play here\n");
			return;
		}

		bool isSeated = clientCommandTokenizeHook.Initialize(SV_ExecuteClientCommand_TokenizeCall, reinterpret_cast<void*>(SV_Cmd_TokenizeString_Hook), HOOK_CALL)->Install()->IsInstalled();
		isSeated = clientCommandFloodHook.Initialize(SV_ExecuteClientCommand_FloodExemptCall, reinterpret_cast<void*>(SV_ExecuteClientCommand_Q_strncmp_Hook), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = serverDefaultUserCmdHook.Initialize(SV_UserMove_DefaultUserCmdCall, reinterpret_cast<void*>(SV_UserMove_MSG_SetDefaultUserCmd_Hook), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(SV_AddServerCommandCalls); ++i)
		{
			isSeated = addServerCommandHooks[i].Initialize(SV_AddServerCommandCalls[i], reinterpret_cast<void*>(SV_AddServerCommand_Hook), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			clientCommandTokenizeHook.Uninstall();
			clientCommandFloodHook.Uninstall();
			serverDefaultUserCmdHook.Uninstall();

			for (auto& hook : addServerCommandHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the server command translation, so a 1.2.211 client cannot play here\n");
			return;
		}

		clientCommandTokenizeHook.Quick();
		clientCommandFloodHook.Quick();
		serverDefaultUserCmdHook.Quick();

		for (auto& hook : addServerCommandHooks)
		{
			hook.Quick();
		}
	}

	void Network::TrackClientBuilds()
	{
		Events::OnClientConnect([](Game::client_s* client)
		{
			const std::ptrdiff_t index = client - Game::svs_clients;

			if (index >= 0 && index < static_cast<std::ptrdiff_t>(Game::MAX_CLIENTS))
			{
				clientBuilds[index] = ClientBuild::Unknown;
			}
		});

		Events::OnClientDisconnect([](int clientNum)
		{
			if (clientNum >= 0 && clientNum < static_cast<int>(Game::MAX_CLIENTS))
			{
				clientBuilds[clientNum] = ClientBuild::Unknown;
			}
		});

		if (!Dedicated::IsEnabled())
		{
			Events::OnDvarInit([]
			{
				x64Marker = Dvar::Register("x64", true, Game::DVAR_USERINFO | Game::DVAR_ROM, "This client is IW4x's 64 bit port, for a server's cross-play translation");
			});
		}
	}

	void Network::HostX86ClientOps()
	{
		static_assert(std::size(serverOpWrites) == std::size(serverOpWriteHooks));

		bool isExpected = Utils::Hook::MatchesBytes(SV_WriteGameState_ClientSpill, clientSpill, sizeof(clientSpill))
			&& Utils::Hook::MatchesBytes(SV_WriteGameState_ClientReload, clientReload, sizeof(clientReload));

		for (const ServerOpWrite& site : serverOpWrites)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(site.address, MSG_WriteByte, site.isJump);
		}

		if (!isExpected)
		{
			Logger::Error("network: a server op write does not read as expected, left alone, so a 1.2.211 client cannot play here\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(serverOpWrites); ++i)
		{
			const ServerOpWrite& site = serverOpWrites[i];
			isSeated = serverOpWriteHooks[i].Initialize(site.address, reinterpret_cast<void*>(site.stub), site.isJump)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : serverOpWriteHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("network: could not seat the server op remap, so a 1.2.211 client cannot play here\n");
			return;
		}

		for (auto& hook : serverOpWriteHooks)
		{
			hook.Quick();
		}
	}

	bool Network::MSG_WriteDeltaPlayerstate_Hook(std::uint8_t* snapInfo, Game::msg_t* msg, int time, const std::uint8_t* from, const std::uint8_t* to)
	{
		const auto writeDelta = reinterpret_cast<bool(*)(std::uint8_t*, Game::msg_t*, int, const std::uint8_t*, const std::uint8_t*)>(playerStateDeltaHook.GetOriginal());
		const auto* const client = *reinterpret_cast<const Game::client_s* const*>(snapInfo + snapshotInfoClient);

		if (!IsX86Client(client))
		{
			return writeDelta(snapInfo, msg, time, from, to);
		}

		static std::array<std::uint8_t, playerStateSize> x86From;
		static std::array<std::uint8_t, playerStateSize> x86To;

		std::memcpy(x86To.data(), to, playerStateSize);
		ToX86HudElemTypes(x86To.data());

		if (!from)
		{
			return writeDelta(snapInfo, msg, time, nullptr, x86To.data());
		}

		std::memcpy(x86From.data(), from, playerStateSize);
		ToX86HudElemTypes(x86From.data());

		return writeDelta(snapInfo, msg, time, x86From.data(), x86To.data());
	}

	void Network::HostX86ClientHudElems()
	{
		if (!Utils::Hook::BranchesTo(SV_WriteSnapshotToClient_DeltaCall, MSG_WriteDeltaPlayerstate, HOOK_CALL))
		{
			Logger::Error("network: the snapshot's player state call does not read as expected, left alone, so a 1.2.211 client here draws the wrong hud\n");
			return;
		}

		if (!playerStateDeltaHook.Initialize(SV_WriteSnapshotToClient_DeltaCall, reinterpret_cast<void*>(MSG_WriteDeltaPlayerstate_Hook), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("network: could not seat the hudelem translation, so a 1.2.211 client here draws the wrong hud\n");
			return;
		}

		playerStateDeltaHook.Quick();
	}

	Network::Network()
	{
		RaiseLimits();
		DisableUnusedPackets();
		SetBuildNumber();
		DisableLSP();
		DisableIWNet();
		RemapServerOps();
		RemapServerCommands();
		SkipServerFileChecks();
		KeepSprintInDefaultUserCmd();
		TranslateSnapshotForX86();
		RenameScoresRequest();
		TranslateNotifyCommand();
		DropEventsWeDoNotHave();
		TrackClientBuilds();
		HostX86ClientCommands();
		HostX86ClientOps();
		HostX86ClientHudElems();

		int failed = 0;

		for (std::size_t i = 0; i < ARRAYSIZE(CL_DispatchConnectionlessPacketCalls); ++i)
		{
			failed += !dispatchHooks[i]
				.Initialize(CL_DispatchConnectionlessPacketCalls[i],
					CL_DispatchConnectionlessPacket_Hook, HOOK_CALL)
				->Install()->IsInstalled();
		}

		if (failed)
		{
			Logger::Error("network failed to seat {} of 2 dispatch hooks", failed);
			return;
		}

		for (auto& hook : dispatchHooks)
		{
			hook.Quick();
		}

		OnPacket("resolveaddress", [](Address& address, [[maybe_unused]] const std::string& data)
		{
			SendRaw(address, address.GetString());
		});

		Command::Add("netdebug", []
		{
			isNetDebug = !isNetDebug;
			Logger::Print("out of band logging {}", isNetDebug ? "on" : "off");
		});

		OnPacketRaw("print", [](Game::netadr_t* address, Game::msg_t* msg)
		{
			const auto* const server = Game::clc_serverAddress;

			if (!Game::NET_CompareBaseAdr(server, address))
			{
				return;
			}

			const auto separatorAt = 4 + std::string_view{"print"}.size();
			const bool isSpaceSeparated = static_cast<std::size_t>(msg->cursize) > separatorAt
				&& msg->data[separatorAt] == ' ';

			std::string text;

			if (isSpaceSeparated)
			{
				text.assign(reinterpret_cast<const char*>(msg->data) + separatorAt + 1,
					static_cast<std::size_t>(msg->cursize) - separatorAt - 1);
			}
			else
			{
				text = Game::MSG_ReadBigString(msg);
			}

			Game::I_strncpyz(reinterpret_cast<char*>(Utils::Hook::Rebase(clc_serverMessage)), text.c_str(), serverMessageSize);
			Logger::Print("{}", text);
		});
	}
}
