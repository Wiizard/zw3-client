#include "STDInclude.hpp"

#include "Voice.hpp"
#include "Chat.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "Network.hpp"

namespace Components
{
	extern "C"
	{
		void UI_MutePlayerStub();
		std::uintptr_t Voice_UIRunMenuScriptExit = 0;

		void Voice_UI_Mute_player(int clientNum, int localClientNum)
		{
			Voice::UI_Mute_player(clientNum, localClientNum);
		}
	}

	Game::VoicePacket_t Voice::voicePackets[Game::MAX_CLIENTS][MAX_SERVER_QUEUED_VOICE_PACKETS];
	int Voice::voicePacketCount[Game::MAX_CLIENTS];

	bool Voice::muteList[Game::MAX_CLIENTS];
	bool Voice::playerMute[Game::MAX_CLIENTS];

	const Game::dvar_t* Voice::sv_voice;
	const Game::dvar_t* Voice::sv_alltalk;

	constexpr std::uintptr_t CL_WriteVoicePacket = 0x140103C80;
	constexpr std::uintptr_t CL_WriteVoicePacketCalls[] = { 0x140103A9B, 0x140103FE9 };

	constexpr std::uintptr_t CL_IsPlayerTalking = 0x140102790;
	constexpr std::uintptr_t CL_IsPlayerMuted = 0x140102730;

	constexpr std::uintptr_t Voice_UnmuteMember = 0x1401173B0;
	constexpr std::uintptr_t Voice_MuteMember = 0x1401172D0;

	constexpr std::uintptr_t SV_SendClientMessages_SendMessageCall = 0x140242142;
	constexpr std::uintptr_t SV_SendMessageToClient = 0x140242440;

	static const Utils::Hook::LeaSite SV_PacketEvent_ICantHearLea = { 0x14023CD58, Utils::Hook::leaRdx, 0x14037A0F0 };
	constexpr std::uintptr_t SV_PacketEvent_ICantHearCall = 0x14023CD82;
	constexpr std::uintptr_t SV_ICantHear = 0x140238830;

	constexpr std::uintptr_t UI_RunMenuScript_MutePlayer = 0x140272403;
	constexpr std::uintptr_t UI_RunMenuScript_Exit = 0x140272035;
	static const std::uint8_t mutePlayerSessionTest[] = { 0x80, 0x3D, 0xB6, 0xF3, 0x29, 0x06, 0x00 };

	constexpr std::uintptr_t sharedUiInfo_playerClientNums = 0x1465D0A40 + 0x534;

	constexpr std::uintptr_t clsState = 0x1406CECF8;
	constexpr std::uintptr_t clc_qport = 0x140BFB7A0;
	constexpr std::uintptr_t clc_netchan = 0x140C3B968;
	constexpr std::uintptr_t clc_demoplaying = 0x140C3B934;
	constexpr std::uintptr_t cl_voiceCommunication = 0x140D09C00;

	static Utils::Hook hooks[8];

	bool Voice::SV_VoiceEnabled()
	{
		return sv_voice->current.enabled;
	}

	void Voice::SV_WriteVoiceDataToClient(const int clientNum, Game::msg_t* msg)
	{
		assert(voicePacketCount[clientNum] >= 0);
		assert(voicePacketCount[clientNum] <= MAX_SERVER_QUEUED_VOICE_PACKETS);

		Game::MSG_WriteByte(msg, voicePacketCount[clientNum]);

		for (auto packet = 0; packet < voicePacketCount[clientNum]; ++packet)
		{
			Game::MSG_WriteByte(msg, voicePackets[clientNum][packet].talker);

			assert(voicePackets[clientNum][packet].dataSize < (2 << 15));

			Game::MSG_WriteByte(msg, voicePackets[clientNum][packet].dataSize);
			Game::MSG_WriteData(msg, voicePackets[clientNum][packet].data, voicePackets[clientNum][packet].dataSize);
		}

		assert(!msg->overflowed);
	}

	void Voice::SV_SendClientVoiceData(Game::client_s* client)
	{
		Game::msg_t msg{};
		const auto clientNum = static_cast<int>(client - Game::svs_clients);

		const auto msgBufLarge = std::make_unique<unsigned char[]>(0x20000);
		auto* msgBuf = msgBufLarge.get();

		assert(voicePacketCount[clientNum] >= 0);

		if (client->header.state == Game::CS_ACTIVE && voicePacketCount[clientNum])
		{
			Game::MSG_Init(&msg, msgBuf, 0x20000);

			assert(msg.cursize == 0);
			assert(msg.bit == 0);

			Game::MSG_WriteString(&msg, "v");
			SV_WriteVoiceDataToClient(clientNum, &msg);

			if (msg.overflowed)
			{
				Logger::Warning("WARNING: voice msg overflowed for {}\n", client->name);
			}
			else
			{
				Game::NET_OutOfBandVoiceData(Game::NS_SERVER, &client->header.netchan.remoteAddress, msg.data, msg.cursize, true);
				voicePacketCount[clientNum] = 0;
			}
		}
	}

	void Voice::SV_SendMessageToClient_Hk(Game::msg_t* msg, Game::client_s* client)
	{
		SV_SendClientVoiceData(client);

		reinterpret_cast<void(*)(Game::msg_t*, Game::client_s*)>(Utils::Hook::Rebase(SV_SendMessageToClient))(msg, client);
	}

	void Voice::SV_ClearMutedList()
	{
		std::memset(muteList, 0, sizeof(muteList));
	}

	void Voice::SV_MuteClient(const int muteClientIndex)
	{
		AssertIn(muteClientIndex, Game::MAX_CLIENTS);
		muteList[muteClientIndex] = true;
	}

	void Voice::SV_UnmuteClient(const int muteClientIndex)
	{
		AssertIn(muteClientIndex, Game::MAX_CLIENTS);
		muteList[muteClientIndex] = false;
	}

	bool Voice::SV_ServerHasClientMuted(const int talker)
	{
		AssertIn(talker, (*Game::sv_maxclients)->current.integer);
		return muteList[talker];
	}

	bool Voice::OnSameTeam(const Game::gentity_s* ent1, const Game::gentity_s* ent2)
	{
		if (!ent1->client || !ent2->client)
		{
			return false;
		}

		if (ent1->client->sess.cs.team)
		{
			return ent1->client->sess.cs.team == ent2->client->sess.cs.team;
		}

		return false;
	}

	void Voice::SV_QueueVoicePacket(const int talkerNum, const int clientNum, const Game::VoicePacket_t* voicePacket)
	{
		assert(talkerNum >= 0);
		assert(clientNum >= 0);
		assert(talkerNum < (*Game::sv_maxclients)->current.integer);
		assert(clientNum < (*Game::sv_maxclients)->current.integer);

		if (voicePacketCount[clientNum] < MAX_SERVER_QUEUED_VOICE_PACKETS)
		{
			voicePackets[clientNum][voicePacketCount[clientNum]].dataSize = voicePacket->dataSize;
			std::memcpy(voicePackets[clientNum][voicePacketCount[clientNum]].data, voicePacket->data, voicePacket->dataSize);

			assert(talkerNum == static_cast<std::uint8_t>(talkerNum));
			voicePackets[clientNum][voicePacketCount[clientNum]].talker = static_cast<char>(talkerNum);
			++voicePacketCount[clientNum];
		}
	}

	void Voice::G_BroadcastVoice(Game::gentity_s* talker, const Game::VoicePacket_t* voicePacket)
	{
		for (auto otherPlayer = 0; otherPlayer < (*Game::sv_maxclients)->current.integer; ++otherPlayer)
		{
			auto* ent = &Game::g_entities[otherPlayer];
			auto* client = ent->client;

			bool shouldSendVoicePacket = false;

			if (ent->r.isInUse && client)
			{
				bool canCommunicate = false;

				if (client->sess.sessionState == Game::SESS_STATE_INTERMISSION)
				{
					canCommunicate = true;
				}
				else if (OnSameTeam(talker, ent))
				{
					canCommunicate = true;
				}
				else if (talker->client->sess.cs.team == Game::TEAM_FREE)
				{
					canCommunicate = true;
				}
				else if (sv_alltalk->current.enabled)
				{
					canCommunicate = true;
				}

				if (canCommunicate)
				{
					bool sessionStatesCompatible = false;

					if (ent->client->sess.sessionState == talker->client->sess.sessionState)
					{
						sessionStatesCompatible = true;
					}
					else if ((ent->client->sess.sessionState == Game::SESS_STATE_DEAD ||
						talker->client->sess.sessionState == Game::SESS_STATE_DEAD) &&
						(*Game::g_deadChat)->current.enabled)
					{
						sessionStatesCompatible = true;
					}
					else if (sv_alltalk->current.enabled)
					{
						sessionStatesCompatible = true;
					}

					const bool isNotSelf = (talker != ent);
					const bool isNotMuted = !SV_ServerHasClientMuted(talker->s.number);

					shouldSendVoicePacket = sessionStatesCompatible && isNotSelf && isNotMuted;
				}
			}

			if (shouldSendVoicePacket)
			{
				SV_QueueVoicePacket(talker->s.number, otherPlayer, voicePacket);
			}
		}
	}

	void Voice::SV_UserVoice(Game::client_s* cl, Game::msg_t* msg)
	{
		Game::VoicePacket_t voicePacket{};

		if (!SV_VoiceEnabled())
		{
			return;
		}

		const auto packetCount = Game::MSG_ReadByte(msg);

		assert(cl->gentity);

		for (auto packet = 0; packet < packetCount; ++packet)
		{
			voicePacket.dataSize = Game::MSG_ReadByte(msg);

			if (voicePacket.dataSize <= 0 || voicePacket.dataSize > MAX_VOICE_PACKET_DATA)
			{
				Logger::PrintFail2Ban("Invalid packet from IP address: {}\n", Network::AdrToString(cl->header.netchan.remoteAddress));
				Logger::Print("Received invalid voice packet of size {} from {}\n", voicePacket.dataSize, cl->name);
				return;
			}

			assert(voicePacket.dataSize <= MAX_VOICE_PACKET_DATA);
			assert(msg->data);

			Game::MSG_ReadData(msg, voicePacket.data, voicePacket.dataSize);
			G_BroadcastVoice(cl->gentity, &voicePacket);
		}
	}

	void Voice::SV_PreGameUserVoice(Game::client_s* cl, Game::msg_t* msg)
	{
		Game::VoicePacket_t voicePacket{};

		if (!SV_VoiceEnabled())
		{
			return;
		}

		const auto talker = static_cast<int>(cl - Game::svs_clients);

		AssertIn(talker, (*Game::sv_maxclients)->current.integer);

		const auto packetCount = Game::MSG_ReadByte(msg);

		for (auto packet = 0; packet < packetCount; ++packet)
		{
			voicePacket.dataSize = Game::MSG_ReadShort(msg);

			if (voicePacket.dataSize <= 0 || voicePacket.dataSize > MAX_VOICE_PACKET_DATA)
			{
				Logger::Print("Received invalid voice packet of size {} from {}\n", voicePacket.dataSize, cl->name);
				return;
			}

			assert(voicePacket.dataSize <= MAX_VOICE_PACKET_DATA);
			assert(msg->data);

			Game::MSG_ReadData(msg, voicePacket.data, voicePacket.dataSize);

			for (auto otherPlayer = 0; otherPlayer < (*Game::sv_maxclients)->current.integer; ++otherPlayer)
			{
				if (otherPlayer != talker && Game::svs_clients[otherPlayer].header.state >= Game::CS_CONNECTED && !SV_ServerHasClientMuted(talker))
				{
					SV_QueueVoicePacket(talker, otherPlayer, &voicePacket);
				}
			}
		}
	}

	void Voice::SV_VoicePacket(Game::netadr_t* from, Game::msg_t* msg)
	{
		const auto qport = Game::MSG_ReadShort(msg);
		auto* cl = Game::SV_FindClientByAddress(from, qport, 0);

		if (!cl || cl->header.state == Game::CS_ZOMBIE)
		{
			return;
		}

		cl->lastPacketTime = *Game::svs_time;

		if (cl->header.state < Game::CS_ACTIVE)
		{
			SV_PreGameUserVoice(cl, msg);
		}
		else
		{
			assert(cl->gentity);
			SV_UserVoice(cl, msg);
		}
	}

	void Voice::CL_WriteVoicePacket_Hk([[maybe_unused]] const int localClientNum)
	{
		const auto connstate = *reinterpret_cast<const int*>(Utils::Hook::Rebase(clsState));
		const auto isDemoPlaying = *reinterpret_cast<const int*>(Utils::Hook::Rebase(clc_demoplaying)) != 0;
		const auto* vc = reinterpret_cast<const Game::voiceCommunication_t*>(Utils::Hook::Rebase(cl_voiceCommunication));

		if (isDemoPlaying || (connstate < Game::CA_LOADING))
		{
			return;
		}

		unsigned char voicePacketBuf[0x800]{};
		Game::msg_t msg{};

		Game::MSG_Init(&msg, voicePacketBuf, sizeof(voicePacketBuf));
		Game::MSG_WriteString(&msg, "v");
		Game::MSG_WriteShort(&msg, *reinterpret_cast<const int*>(Utils::Hook::Rebase(clc_qport)));
		Game::MSG_WriteByte(&msg, vc->voicePacketCount);

		for (auto voicePacket = 0; voicePacket < vc->voicePacketCount; ++voicePacket)
		{
			assert(vc->voicePackets[voicePacket].dataSize > 0);
			assert(vc->voicePackets[voicePacket].dataSize < (2 << 15));

			Game::MSG_WriteByte(&msg, vc->voicePackets[voicePacket].dataSize);
			Game::MSG_WriteData(&msg, vc->voicePackets[voicePacket].data, vc->voicePackets[voicePacket].dataSize);
		}

		const auto* netchan = reinterpret_cast<const Game::netchan_t*>(Utils::Hook::Rebase(clc_netchan));
		const auto* serverAddress = Game::clc_serverAddress;

		Game::NET_OutOfBandVoiceData(netchan->sock, serverAddress, msg.data, msg.cursize, true);
	}

	void Voice::CL_ClearMutedList()
	{
		std::memset(playerMute, 0, sizeof(playerMute));
	}

	bool Voice::CL_IsPlayerTalking_Hk([[maybe_unused]] Game::SessionData* session, [[maybe_unused]] const int localClientNum, const int talkingClientIndex)
	{
		return Game::Voice_IsClientTalking(talkingClientIndex);
	}

	bool Voice::CL_IsPlayerMuted_Hk([[maybe_unused]] Game::SessionData* session, [[maybe_unused]] const int localClientNum, const int muteClientIndex)
	{
		AssertIn(muteClientIndex, Game::MAX_CLIENTS);
		return playerMute[muteClientIndex];
	}

	void Voice::CL_MutePlayer_Hk([[maybe_unused]] Game::SessionData* session, const int muteClientIndex)
	{
		AssertIn(muteClientIndex, Game::MAX_CLIENTS);
		playerMute[muteClientIndex] = true;
	}

	void Voice::Voice_UnmuteMember_Hk([[maybe_unused]] Game::SessionData* session, const int clientNum)
	{
		AssertIn(clientNum, Game::MAX_CLIENTS);
		playerMute[clientNum] = false;
	}

	void Voice::CL_TogglePlayerMute(const int localClientNum, const int muteClientIndex)
	{
		AssertIn(muteClientIndex, Game::MAX_CLIENTS);

		if (CL_IsPlayerMuted_Hk(nullptr, localClientNum, muteClientIndex))
		{
			Voice_UnmuteMember_Hk(nullptr, muteClientIndex);
		}
		else
		{
			CL_MutePlayer_Hk(nullptr, muteClientIndex);
		}
	}

	void Voice::CL_VoicePacket(Game::netadr_t* address, Game::msg_t* msg)
	{
		const auto* serverAddress = Game::clc_serverAddress;

		if (!Game::NET_CompareBaseAdr(serverAddress, address))
		{
			Logger::Debug("Ignoring stray 'v' network message from '{}'", Network::AdrToString(*address));
			return;
		}

		const auto numPackets = Game::MSG_ReadByte(msg);

		if (numPackets < 0 || numPackets > MAX_SERVER_QUEUED_VOICE_PACKETS)
		{
			return;
		}

		Game::VoicePacket_t voicePacket{};

		for (auto packet = 0; packet < numPackets; ++packet)
		{
			voicePacket.talker = static_cast<char>(Game::MSG_ReadByte(msg));
			voicePacket.dataSize = Game::MSG_ReadByte(msg);

			if (voicePacket.dataSize <= 0 || voicePacket.dataSize > MAX_VOICE_PACKET_DATA)
			{
				Logger::Print("Invalid server voice packet of {} bytes\n", voicePacket.dataSize);
				return;
			}

			Game::MSG_ReadData(msg, voicePacket.data, voicePacket.dataSize);

			if (static_cast<unsigned char>(voicePacket.talker) >= Game::MAX_CLIENTS)
			{
				Logger::Print("Invalid voice packet - talker was {}\n", voicePacket.talker);
				return;
			}

			if (!CL_IsPlayerMuted_Hk(nullptr, 0, voicePacket.talker))
			{
				if ((*Game::cl_voice)->current.enabled)
				{
					Game::Voice_IncomingVoiceData(nullptr, voicePacket.talker, reinterpret_cast<unsigned char*>(voicePacket.data), voicePacket.dataSize);
				}
			}
		}
	}

	void Voice::UI_Mute_player(const int clientNum, const int localClientNum)
	{
		const auto* playerClientNums = reinterpret_cast<const int*>(Utils::Hook::Rebase(sharedUiInfo_playerClientNums));
		CL_TogglePlayerMute(localClientNum, playerClientNums[clientNum]);
	}

	Voice::Voice()
	{
		std::memset(voicePackets, 0, sizeof(voicePackets));
		std::memset(voicePacketCount, 0, sizeof(voicePacketCount));

		SV_ClearMutedList();
		CL_ClearMutedList();

		Events::OnSteamDisconnect(CL_ClearMutedList);
		Events::OnClientDisconnect(SV_UnmuteClient);
		Events::OnClientConnect([](Game::client_s* cl) -> void
		{
			if (Chat::IsMuted(cl))
			{
				SV_MuteClient(static_cast<int>(cl - Game::svs_clients));
			}
		});

		Events::OnDvarInit([]
		{
			sv_voice = Game::Dvar_RegisterBool("sv_voice", false, Game::DVAR_NONE, "Use server side voice communications");
			sv_alltalk = Game::Dvar_RegisterBool("sv_alltalk", false, Game::DVAR_NONE, "Allow talking across teams");
		});

		Events::OnSVInit([]
		{
			if (!Dedicated::IsEnabled())
			{
				Command::Execute("sv_voice 1", false);
			}
		});

		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t target;
			void* stub;
			bool isJump;
		};

		const HookSite sites[] =
		{
			{ CL_WriteVoicePacketCalls[0], CL_WriteVoicePacket, reinterpret_cast<void*>(CL_WriteVoicePacket_Hk), HOOK_CALL },
			{ CL_WriteVoicePacketCalls[1], CL_WriteVoicePacket, reinterpret_cast<void*>(CL_WriteVoicePacket_Hk), HOOK_CALL },
			{ SV_SendClientMessages_SendMessageCall, SV_SendMessageToClient, reinterpret_cast<void*>(SV_SendMessageToClient_Hk), HOOK_CALL },
			{ SV_PacketEvent_ICantHearCall, SV_ICantHear, reinterpret_cast<void*>(SV_VoicePacket), HOOK_CALL },
		};

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.target, hookSite.isJump))
			{
				Logger::Error("voice: 0x{:X} no longer reaches 0x{:X}, voice is the engine's\n", hookSite.site, hookSite.target);
				return;
			}
		}

		if (!Utils::Hook::IsLeaIntact(SV_PacketEvent_ICantHearLea)
			|| !Utils::Hook::MatchesBytes(UI_RunMenuScript_MutePlayer, mutePlayerSessionTest, sizeof(mutePlayerSessionTest)))
		{
			Logger::Error("voice: SV_PacketEvent or UI_RunMenuScript does not read as expected, voice is the engine's\n");
			return;
		}

		const auto* voiceCommand = Utils::Hook::PlaceNearImage("v");

		if (!voiceCommand || !Utils::Hook::CanLeaReach(SV_PacketEvent_ICantHearLea, voiceCommand))
		{
			Logger::Error("voice: \"v\" cannot be placed where SV_PacketEvent reaches it, voice is the engine's\n");
			return;
		}

		Voice_UIRunMenuScriptExit = Utils::Hook::Rebase(UI_RunMenuScript_Exit);

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].stub, sites[i].isJump)->Install()->IsInstalled() && isSeated;
		}

		isSeated = hooks[4].Initialize(CL_IsPlayerTalking, reinterpret_cast<void*>(CL_IsPlayerTalking_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[5].Initialize(CL_IsPlayerMuted, reinterpret_cast<void*>(CL_IsPlayerMuted_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[6].Initialize(Voice_UnmuteMember, reinterpret_cast<void*>(Voice_UnmuteMember_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[7].Initialize(Voice_MuteMember, reinterpret_cast<void*>(CL_MutePlayer_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		static Utils::Hook mutePlayerHook;
		isSeated = mutePlayerHook.Initialize(UI_RunMenuScript_MutePlayer, UI_MutePlayerStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			mutePlayerHook.Uninstall();

			Logger::Error("voice: could not seat every hook, voice is the engine's\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		mutePlayerHook.Quick();

		Utils::Hook::PointLeaAt(SV_PacketEvent_ICantHearLea, voiceCommand);

		Network::OnPacketRaw("v", CL_VoicePacket);
	}
}
