#include "STDInclude.hpp"

#include "Security.hpp"
#include "Command.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"

namespace Components
{
	constexpr std::uintptr_t SV_SteamAuthClient = 0x140239420;

	static const std::uint8_t steamAuthClientEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10 };

	constexpr std::uintptr_t Steam_Frame_SteamServerTest = 0x14024D61D;

	static const std::uint8_t steamServerTest[] = { 0x0F, 0x84, 0xC4, 0x00, 0x00, 0x00 };
	static const std::uint8_t steamServerSkip[] = { 0xE9, 0xC5, 0x00, 0x00, 0x00, 0x90 };

	constexpr std::uintptr_t CL_ConnectionlessPacket_RelayTest = 0x1400F9807;
	constexpr std::uintptr_t CL_ConnectionlessPacket_RelayArm = 0x1400F981E;
	constexpr std::uintptr_t CL_ConnectionlessPacket_Handled = 0x1400F9713;

	static const std::uint8_t relayTest[] =
	{
		0x48, 0x8D, 0x15, 0xBA, 0xF4, 0x27, 0x00,
		0x48, 0x8B, 0xCE,
		0xE8, 0xDA, 0x28, 0x19, 0x00,
		0x85, 0xC0,
		0x0F, 0x85, 0xAE, 0x00, 0x00, 0x00,
		0x48, 0x63, 0x05, 0x1B, 0x2E, 0xAC, 0x01,
	};

	static const std::uint8_t handledExit[] = { 0xB0, 0x01 };

	constexpr std::uintptr_t SV_DirectConnect_InvitedTest = 0x140237BA7;
	constexpr std::uintptr_t SV_DirectConnect_InvitedJnz = 0x140237BAA;

	static const std::uint8_t invitedTest[] = { 0x80, 0x38, 0x31, 0x75, 0x24 };

	constexpr std::uintptr_t G_GetClientScore = 0x14019CF00;
	constexpr std::size_t gclientScoreOffset = 0x3134;

	static const std::uint8_t getClientScoreBody[] =
	{
		0x48, 0x63, 0xC1,
		0x48, 0x69, 0xC8, 0x78, 0x36, 0x00, 0x00,
		0x48, 0x8B, 0x05, 0x0F, 0xA1, 0x6C, 0x01,
		0x8B, 0x84, 0x01, 0x34, 0x31, 0x00, 0x00,
		0xC3,
	};

	static Utils::Hook getClientScoreHook;

	constexpr std::uintptr_t PartyHost_HandleJoinPartyRequest_CountCall = 0x14010CE03;
	constexpr std::uintptr_t atoi_wrap = 0x1403373EC;

	static Utils::Hook playerCountHook;

	static int AtolAdjustPlayerLimit(const char* string)
	{
		return std::min(std::atoi(string), 18);
	}

	static int G_GetClientScore_Hook(const int clientNum)
	{
		if (!Game::level->clients)
		{
			return 0;
		}

		const auto* const client = reinterpret_cast<const std::uint8_t*>(&Game::level->clients[clientNum]);
		return *reinterpret_cast<const int*>(client + gclientScoreOffset);
	}

	constexpr std::uintptr_t CL_SelectStringTableEntryInDvar_f = 0x14026BBC0;

	static void SelectStringTableEntryInDvar_Stub()
	{
		const Command::ClientParams params;

		if (params.Size() >= 4)
		{
			const char* name = params.Get(3);

			if (Command::Find(name))
			{
				Logger::Debug("CL_SelectStringTableEntryInDvar_f: parameter is a command");
				return;
			}

			const auto* dvar = Game::Dvar_FindVar(name);
			constexpr unsigned int disallowedFlags = Game::DVAR_CHEAT | Game::DVAR_INIT | Game::DVAR_ROM | Game::DVAR_EXTERNAL | Game::DVAR_LATCH;

			if (dvar && (dvar->flags & disallowedFlags) != 0)
			{
				Logger::Debug("CL_SelectStringTableEntryInDvar_f: parameter is a protected dvar");
				return;
			}
		}

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CL_SelectStringTableEntryInDvar_f))();
	}

	static void GuardSelectStringTableEntryInDvar()
	{
		auto* const command = Command::Find("selectStringTableEntryInDvar");
		const auto engineHandler = reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CL_SelectStringTableEntryInDvar_f));

		if (!command || command->function != engineHandler)
		{
			Logger::Error("security: selectStringTableEntryInDvar is not the engine's, a menu can still write any dvar through it\n");
			return;
		}

		command->function = SelectStringTableEntryInDvar_Stub;
	}

	struct CurlCall
	{
		std::uintptr_t site;
		std::uintptr_t callee;
	};

	static const CurlCall curlCalls[] =
	{
		{ 0x1402A3C06, 0x1401FBB60 },
		{ 0x1402A38C2, 0x1401FB8F0 },
		{ 0x1402A382B, 0x1402A8A40 },
		{ 0x1402A3EDD, 0x1402A8A40 },
	};

	static void DisableCurl()
	{
		for (const auto& call : curlCalls)
		{
			if (!Utils::Hook::BranchesTo(call.site, call.callee, HOOK_CALL))
			{
				Logger::Error("security: 0x{:X} is not the expected curl call, curl is left running\n", call.site);
				return;
			}
		}

		for (const auto& call : curlCalls)
		{
			Utils::Hook::Nop(call.site, 5);
		}
	}

	static void IgnoreRelayPackets()
	{
		if (!Utils::Hook::MatchesBytes(CL_ConnectionlessPacket_RelayTest, relayTest, sizeof(relayTest))
			|| !Utils::Hook::MatchesBytes(CL_ConnectionlessPacket_Handled, handledExit, sizeof(handledExit)))
		{
			Logger::Error("security: CL_ConnectionlessPacket's relay arm does not read as expected, relay packets are still handled\n");
			return;
		}

		const auto distance = static_cast<std::int32_t>(CL_ConnectionlessPacket_Handled - (CL_ConnectionlessPacket_RelayArm + 5));

		Utils::Hook::Set<std::int32_t>(CL_ConnectionlessPacket_RelayArm + 1, distance);
		Utils::Hook::Set<std::uint8_t>(CL_ConnectionlessPacket_RelayArm, 0xE9);
	}

	Security::Security()
	{
		DisableCurl();
		IgnoreRelayPackets();

		Scheduler::Once(GuardSelectStringTableEntryInDvar, Scheduler::Pipeline::MAIN);

		if (!Utils::Hook::MatchesBytes(G_GetClientScore, getClientScoreBody, sizeof(getClientScoreBody))
			|| !getClientScoreHook.Initialize(G_GetClientScore, reinterpret_cast<void*>(G_GetClientScore_Hook), HOOK_JUMP)->Install()->IsInstalled())
		{
			Logger::Error("security: could not replace G_GetClientScore, a score asked for before a map is up still crashes\n");
		}
		else
		{
			getClientScoreHook.Quick();
		}

		if (!Utils::Hook::BranchesTo(PartyHost_HandleJoinPartyRequest_CountCall, atoi_wrap, HOOK_CALL)
			|| !playerCountHook.Initialize(PartyHost_HandleJoinPartyRequest_CountCall, reinterpret_cast<void*>(AtolAdjustPlayerLimit), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("security: could not hook PartyHost_HandleJoinPartyRequest's player count, a join request can still claim any number\n");
		}
		else
		{
			playerCountHook.Quick();
		}

		if (Utils::Hook::MatchesBytes(SV_DirectConnect_InvitedTest, invitedTest, sizeof(invitedTest)))
		{
			Utils::Hook::Set<std::uint8_t>(SV_DirectConnect_InvitedJnz, 0xEB);
		}
		else
		{
			Logger::Error("security: SV_DirectConnect's invited test does not read as expected, a client can still claim an invite\n");
		}

		if (Utils::Hook::MatchesBytes(Steam_Frame_SteamServerTest, steamServerTest, sizeof(steamServerTest)))
		{
			for (std::size_t i = 0; i < sizeof(steamServerSkip); ++i)
			{
				Utils::Hook::Set<std::uint8_t>(Steam_Frame_SteamServerTest + i, steamServerSkip[i]);
			}
		}
		else
		{
			Logger::Error("security: Steam_Frame's game server test does not read as expected, a Steam hosted match kicks its players after 40s\n");
		}

		if (!Utils::Hook::MatchesBytes(SV_SteamAuthClient, steamAuthClientEntry, sizeof(steamAuthClientEntry)))
		{
			Logger::Error("security: SV_SteamAuthClient does not read as expected, left alone, so a steamauth packet can crash a hosted match\n");
			return;
		}

		Utils::Hook::Set<std::uint8_t>(SV_SteamAuthClient, 0xC3);
	}
}
