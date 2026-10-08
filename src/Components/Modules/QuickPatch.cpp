#include "STDInclude.hpp"

#include <span>

#include "QuickPatch.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "Toast.hpp"
#include "UIScript.hpp"
#include "Window.hpp"

namespace Components
{
	constexpr std::uintptr_t Steam_FileRead_RetryTest = 0x14024CDE4;
	constexpr std::uintptr_t Steam_FileRead_RetryExit = 0x14024CDE7;

	static const std::uint8_t retryTest[] = { 0x83, 0xFB, 0x0F, 0x74, 0x42 };

	constexpr std::uintptr_t sv_pure = 0x140425E68;
	constexpr std::uintptr_t sv_pureConstant = 0x1403A5C70;

	static Dvar::Var sv_pureVar;

	constexpr std::uintptr_t IWNet_Frame_DNSLookup = 0x1401ADD74;
	constexpr std::uintptr_t IWNet_Frame_AfterLookup = 0x1401ADEE8;

	static const std::uint8_t dnsLookupHead[] = { 0x80, 0x3D, 0xC2, 0x8A, 0x84, 0x01, 0x00 };

	constexpr std::uintptr_t Live_Frame_LostConnectionCheck = 0x1401AF390;

	static const std::uint8_t lostConnectionCheckHead[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x69, 0x1D, 0x40, 0x9E, 0x84, 0x01, 0xE8, 0x03, 0x00, 0x00 };

	constexpr std::uintptr_t Com_Frame_UpdateSystemDvarsCall = 0x1401F474A;
	constexpr std::uintptr_t GamerProfile_UpdateSystemDvars = 0x1400F1770;

	constexpr std::uintptr_t SetPlayerDataWorker = 0x140268610;

	constexpr std::uintptr_t StructuredDataDef_GetAsset = 0x140281080;
	constexpr std::uintptr_t StructuredDataInitLookup = 0x140281790;
	constexpr std::uintptr_t StructuredDataLookupString = 0x140281990;
	constexpr std::uintptr_t StructuredDataLookupStatus = 0x140281610;
	constexpr unsigned int playerDataSize = 0x1FFC;
	constexpr int lookupStatusValue = 1;

	constexpr std::uintptr_t StringTable_GetAsset = 0x140280D50;
	constexpr std::uintptr_t StringTable_GetColumnValueForRow = 0x140280D70;
	constexpr int stringTableRowCount = 12;

	constexpr std::uintptr_t Sys_AllowVidRestart = 0x1402A4660;

	static const std::uint8_t allowVidRestartHead[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x8D, 0x0D };

	static const Utils::Hook::LeaSite windowNameLea = { 0x140032852, Utils::Hook::leaR8, 0x1403E0980 };

	static const Utils::Hook::LeaSite hostnameDefaultLea = { 0x14023A44A, Utils::Hook::leaRdx, 0x14039E490 };

	static const Utils::Hook::LeaSite consoleLogoLea = { 0x1402A95B5, Utils::Hook::leaRdx, 0x1403A9C18 };
	static const Utils::Hook::LeaSite splashLea = { 0x1402A8787, Utils::Hook::leaRdx, 0x1403A9B50 };

	constexpr std::uintptr_t CG_RegisterScoreboardDvars_PingText = 0x1400E56C8;
	constexpr std::uintptr_t CG_RegisterScoreboardDvars_PingTextCall = 0x1400E56D4;

	static const std::uint8_t pingTextRegister[] = { 0x45, 0x33, 0xC0, 0x48, 0x8D, 0x0D, 0x9E, 0x09, 0x29, 0x00, 0x33, 0xD2, 0xE8, 0x97, 0x05, 0x1A, 0x00 };

	static Utils::Hook pingTextHook;

	constexpr std::uintptr_t Com_Init_CommandLineCount = 0x1401F53CC;
	constexpr std::uintptr_t Com_Init_CommandLineJle = 0x1401F53CE;
	constexpr std::uintptr_t Com_Init_CommandLineList = 0x1401F53DB;

	static const std::uint8_t commandLineCount[] = { 0x85, 0xD2, 0x7E, 0x71 };
	static const std::uint8_t commandLineList[] = { 0x4C, 0x8D, 0x2D, 0x7E, 0xFC, 0x22, 0x00 };

	constexpr std::uintptr_t Sys_Init_MajorVersionTest = 0x1402A5627;
	constexpr std::uintptr_t Sys_Init_MajorVersionDigit = 0x1403A9696;

	static const std::uint8_t majorVersionTest[] = { 0x83, 0x7C, 0x24, 0x24, 0x04, 0x72, 0x21 };

	constexpr std::uintptr_t Com_Init_MigrationDvarErrors = 0x1401F58D6;
	constexpr std::uintptr_t Com_Init_MigrationDvarErrorsDefault = 0x1401F58DE;

	static const std::uint8_t migrationDvarErrors[] = { 0x48, 0x8D, 0x0D, 0x4B, 0x74, 0x19, 0x00, 0xB2, 0x01, 0xE8, 0x8C, 0x03, 0x09, 0x00 };

	constexpr std::uintptr_t ClientConnect_DeveloperTest = 0x14019616B;
	constexpr std::uintptr_t ClientConnect_DeveloperJz = 0x140196176;

	static const std::uint8_t developerTest[] = { 0x48, 0x8B, 0x05, 0xDE, 0x38, 0xA4, 0x01, 0x80, 0x78, 0x10, 0x00, 0x74, 0x09, 0x48, 0x8D, 0x05, 0x21, 0x00, 0x1F, 0x00 };

	constexpr std::uintptr_t FS_Restart_DefaultCfg = 0x1402780B9;
	constexpr std::uintptr_t FS_Restart_FindCall = 0x1402780C5;
	constexpr std::uintptr_t FS_Restart_IsDefaultCall = 0x1402780D6;
	constexpr std::uintptr_t FS_Restart_DefaultJz = 0x1402780DD;
	constexpr std::uintptr_t FS_Restart_DefaultFound = 0x140278163;

	static const std::uint8_t defaultCfgCheck[] =
	{
		0x48, 0x8D, 0x15, 0x00, 0xC8, 0x12, 0x00, 0xB9, 0x24, 0x00, 0x00, 0x00, 0xE8, 0x06, 0x56, 0xEB,
		0xFF, 0x48, 0x8D, 0x15, 0xEF, 0xC7, 0x12, 0x00, 0xB9, 0x24, 0x00, 0x00, 0x00, 0xE8, 0xA5, 0x60,
		0xEB, 0xFF, 0x85, 0xC0, 0x0F, 0x84, 0x80, 0x00, 0x00, 0x00,
	};

	constexpr std::uintptr_t CL_Vid_Restart_f_AllowCall = 0x1400FDB9E;
	constexpr std::uintptr_t CL_Vid_Restart_f_AllowJz = 0x1400FDBA5;

	static const std::uint8_t vidRestartAllowTest[] = { 0xE8, 0xBD, 0x6A, 0x1A, 0x00, 0x84, 0xC0, 0x0F, 0x84, 0x1A, 0x04, 0x00, 0x00 };

	constexpr std::uintptr_t Com_Init_Intro = 0x1401F5A0D;
	constexpr std::uintptr_t Com_Init_IntroJz = 0x1401F5A1D;
	constexpr std::uintptr_t Com_Init_LegalSetString = 0x1401F5A3B;
	constexpr std::uintptr_t Com_Init_IntroSetBool = 0x1401F5A49;

	static const std::uint8_t introBlock[] =
	{
		0x48, 0x8B, 0x05, 0x9C, 0x3B, 0x9E, 0x01, 0x4C, 0x8B, 0x7C, 0x24, 0x70, 0x80, 0x78, 0x10, 0x00,
		0x74, 0x2F, 0x48, 0x8D, 0x15, 0x4A, 0x69, 0x19, 0x00, 0x33, 0xC9, 0xE8, 0x93, 0x13, 0xFF, 0xFF,
		0x48, 0x8B, 0x0D, 0x84, 0x40, 0x9E, 0x01, 0x48, 0x8D, 0x15, 0x4D, 0x69, 0x19, 0x00, 0xE8, 0xD0,
		0x1F, 0x09, 0x00, 0x48, 0x8B, 0x0D, 0x69, 0x3B, 0x9E, 0x01, 0x33, 0xD2, 0xE8, 0xF2, 0x14, 0x09,
		0x00,
	};

	static const Utils::Hook::LeaSite introCinematicLea = { 0x1401F5A1F, Utils::Hook::leaRdx, 0x14038C370 };

	static const Utils::Hook::LeaSite binkMainLea = { 0x140035C8C, Utils::Hook::leaR8, 0x1403E0AE8 };
	static const Utils::Hook::LeaSite binkRawLea = { 0x140035CCA, Utils::Hook::leaR8, 0x1403E0B00 };

	constexpr char zw3IntroCommand[] = "cinematic zw3\n";
	constexpr char zw3Video[] = "%s\\zw3\\data\\video\\%s.bik";

	constexpr std::uintptr_t Com_Init_HasInfoChangedCall = 0x1401F5755;

	static const std::uint8_t hasInfoChangedBlock[] =
	{
		0xE8, 0xF6, 0xFD, 0x0A, 0x00,
		0x48, 0x8B, 0xBC, 0x24, 0x90, 0x00, 0x00, 0x00,
		0x48, 0x8B, 0xB4, 0x24, 0xB8, 0x00, 0x00, 0x00,
		0x84, 0xC0, 0x74, 0x09, 0x33, 0xD2, 0x33, 0xC9, 0xE8, 0x29, 0x0A, 0x00, 0x00,
	};

	static const std::uint8_t returnFalse[] = { 0x31, 0xC0, 0x90, 0x90, 0x90 };

	struct DvarDefaultSite
	{
		Utils::Hook::LeaSite name;
		std::uintptr_t load;
		std::array<std::uint8_t, 5> loadBytes;
		std::size_t loadSize;
		std::size_t valueOffset;
		std::size_t valueSize;
	};

	static const DvarDefaultSite zeroDefaults[] =
	{
		{ { 0x14010A7E2, Utils::Hook::leaRcx, 0x14037AA28 }, 0x14010A7ED, { 0x41, 0x8D, 0x50, 0x0A }, 4, 3, 1 },
		{ { 0x14010A812, Utils::Hook::leaRcx, 0x14037AA98 }, 0x14010A81D, { 0x41, 0x8D, 0x50, 0x05 }, 4, 3, 1 },
		{ { 0x14010AB2B, Utils::Hook::leaRcx, 0x14037B060 }, 0x14010AB36, { 0x41, 0x8D, 0x50, 0x3C }, 4, 3, 1 },
		{ { 0x14023A703, Utils::Hook::leaRcx, 0x14039E820 }, 0x14023A6EE, { 0x8D, 0x53, 0x03 }, 3, 2, 1 },
		{ { 0x14010A928, Utils::Hook::leaRcx, 0x14037AC90 }, 0x14010A935, { 0x41, 0x8D, 0x50, 0x04 }, 4, 3, 1 },
		{ { 0x14010AB46, Utils::Hook::leaRcx, 0x14037B088 }, 0x14010AB62, { 0xBA, 0xE8, 0x03, 0x00, 0x00 }, 5, 1, 4 },
		{ { 0x14010AD31, Utils::Hook::leaRcx, 0x14037B550 }, 0x14010AD4D, { 0xBA, 0xD0, 0x07, 0x00, 0x00 }, 5, 1, 4 },
		{ { 0x14023A8AF, Utils::Hook::leaRcx, 0x14039EB00 }, 0x14023A8CB, { 0xBA, 0xC8, 0x00, 0x00, 0x00 }, 5, 1, 4 },
	};

	static const char* const zeroedDvars[] =
	{
		"party_pregameStartTimerLength",
		"party_gameStartTimerLength",
		"party_minLobbyTime",
		"sv_reconnectlimit",
		"party_vetoDelayTime",
		"party_connectTimeout",
		"party_searchPauseTime",
		"sv_hugeSnapshotDelay",
	};

	struct ResendCompare
	{
		std::uintptr_t address;
		std::array<std::uint8_t, 5> bytes;
		std::size_t size;
		std::size_t valueOffset;
		bool isByte;
		int remoteMs;
	};

	static const ResendCompare resendCompares[] =
	{
		{ 0x1400F81F0, { 0x83, 0xF8, 0x64 }, 3, 2, true, 100 },
		{ 0x1400F8203, { 0x3D, 0xB8, 0x0B, 0x00, 0x00 }, 5, 1, false, 3000 },
		{ 0x1400F75CB, { 0x3D, 0xE8, 0x03, 0x00, 0x00 }, 5, 1, false, 1000 },
	};

	constexpr int localResendMs = 4;

	constexpr std::uintptr_t clc_netchanAddressType = 0x140C3B978;

	static bool IsLocalServer()
	{
		return Utils::Hook::Get<int>(clc_netchanAddressType) == Game::NA_LOOPBACK
			|| Game::clc_serverAddress->type == Game::NA_LOOPBACK
			|| Dedicated::IsRunning();
	}

	static void SetResendCompares(bool isLocal)
	{
		for (const auto& compare : resendCompares)
		{
			int value = compare.remoteMs;

			if (isLocal)
			{
				value = localResendMs;
			}

			if (compare.isByte)
			{
				Utils::Hook::Set<std::int8_t>(compare.address + compare.valueOffset, static_cast<std::int8_t>(value));
			}
			else
			{
				Utils::Hook::Set<std::int32_t>(compare.address + compare.valueOffset, value);
			}
		}
	}

	constexpr std::uintptr_t NET_SendPacket = 0x140206BF0;
	constexpr std::uintptr_t NET_GetLoopPacket = 0x140206800;
	constexpr std::uintptr_t NET_GetLoopPacket_Real = 0x140206810;
	constexpr std::uintptr_t Sys_SendPacket = 0x1402A8290;

	static const std::uint8_t sendPacketEntry[] = { 0x48, 0x89, 0x74, 0x24, 0x20, 0x57, 0x48, 0x83, 0xEC, 0x40, 0x41, 0x8B, 0x01 };

	constexpr std::size_t loopSlotCount = 256;
	constexpr std::size_t loopSlotSize = 2048;
	constexpr std::size_t clientLoopQueueCount = 2;

	struct LoopMessage
	{
		char data[loopSlotSize];
		int length;
		int port;
	};

	struct LoopQueue
	{
		std::mutex lock;
		std::uint32_t sent = 0;
		std::uint32_t read = 0;
		std::array<LoopMessage, loopSlotCount> messages{};
	};

	static LoopQueue serverLoopQueue;
	static LoopQueue clientLoopQueues[clientLoopQueueCount];

	static Utils::Hook sendPacketHook;
	static Utils::Hook getLoopPacketHook;

	static LoopQueue* FindLoopQueue(int netsrc)
	{
		if (netsrc == Game::NS_SERVER)
		{
			return &serverLoopQueue;
		}

		if (netsrc < 0 || netsrc >= static_cast<int>(clientLoopQueueCount))
		{
			return nullptr;
		}

		return &clientLoopQueues[netsrc];
	}

	static bool NET_SendPacket_Hk(int sock, int length, const void* data, const Game::netadr_t* to)
	{
		if (static_cast<unsigned int>(to->type) <= static_cast<unsigned int>(Game::NA_BAD))
		{
			return false;
		}

		Game::netadr_t target = *to;

		if (target.type != Game::NA_LOOPBACK)
		{
			return reinterpret_cast<bool(*)(int, const void*, Game::netadr_t*)>(Utils::Hook::Rebase(Sys_SendPacket))(length, data, &target);
		}

		if (length <= 0 || length > static_cast<int>(loopSlotSize))
		{
			return false;
		}

		LoopQueue* queue = &serverLoopQueue;
		int port = sock;

		if (sock >= Game::NS_SERVER)
		{
			queue = FindLoopQueue(target.port);
			port = target.port;
		}

		if (!queue)
		{
			return false;
		}

		std::lock_guard guard(queue->lock);

		auto& message = queue->messages[queue->sent % loopSlotCount];
		std::memcpy(message.data, data, static_cast<std::size_t>(length));
		message.length = length;
		message.port = port;
		++queue->sent;

		return true;
	}

	static int NET_GetLoopPacket_Hk(int netsrc, Game::netadr_t* from, Game::msg_t* msg)
	{
		auto* const queue = FindLoopQueue(netsrc);

		if (!queue)
		{
			return 0;
		}

		std::lock_guard guard(queue->lock);

		if (queue->sent - queue->read > loopSlotCount)
		{
			queue->read = queue->sent - static_cast<std::uint32_t>(loopSlotCount);
		}

		if (queue->read == queue->sent)
		{
			return 0;
		}

		const auto& message = queue->messages[queue->read % loopSlotCount];
		++queue->read;

		if (msg->maxsize < message.length)
		{
			return 0;
		}

		std::memcpy(msg->data, message.data, static_cast<std::size_t>(message.length));
		msg->cursize = message.length;

		*from = {};
		from->type = Game::NA_LOOPBACK;
		from->port = static_cast<unsigned short>(message.port);

		return 1;
	}

	static void ResetLoopQueues()
	{
		for (auto* queue : { &serverLoopQueue, &clientLoopQueues[0], &clientLoopQueues[1] })
		{
			std::lock_guard guard(queue->lock);
			queue->sent = 0;
			queue->read = 0;
		}
	}

	static const Utils::Hook::LeaSite gameLogLea = { 0x14019D91D, Utils::Hook::leaRdx, 0x140387E88 };

	static const Utils::Hook::LeaSite configNameLeas[] =
	{
		{ 0x1401E75C6, Utils::Hook::leaRdx, 0x14038BC40 },
		{ 0x1401F427F, Utils::Hook::leaRcx, 0x14038BC40 },
		{ 0x1401F42FC, Utils::Hook::leaRdx, 0x14038BC40 },
		{ 0x1401F5557, Utils::Hook::leaRdx, 0x14038BC40 },
		{ 0x140278187, Utils::Hook::leaRdx, 0x14038BC40 },
	};

	constexpr std::uintptr_t MainWndProc_ExecutionState = 0x1402AA9BE;
	constexpr std::uintptr_t MainWndProc_ExecutionStateCall = 0x1402AA9C8;

	static const std::uint8_t executionStateCall[] = { 0xB9, 0x02, 0x00, 0x00, 0x00, 0x49, 0x8B, 0xF0, 0x8B, 0xEA, 0xFF, 0x15, 0x2A, 0x7A, 0x0B, 0x00 };

	static Dvar::Var ui_mousePitch;

	static void SetPlayerData(const std::string& arguments)
	{
		const char* cursor = arguments.data();

		reinterpret_cast<void(*)(unsigned int, Game::UiContext*, void*, const char**)>(
			Utils::Hook::Rebase(SetPlayerDataWorker))(0, Game::uiContext, nullptr, &cursor);
	}

	static bool IsPlayerDataPath(void* def, std::initializer_list<const char*> names)
	{
		std::uint8_t lookup[24]{};

		reinterpret_cast<void(*)(void*, void*)>(Utils::Hook::Rebase(StructuredDataInitLookup))(def, lookup);

		for (const char* const name : names)
		{
			reinterpret_cast<void(*)(void*, const char*)>(Utils::Hook::Rebase(StructuredDataLookupString))(lookup, name);
		}

		return reinterpret_cast<int(*)(void*)>(Utils::Hook::Rebase(StructuredDataLookupStatus))(lookup) == lookupStatusValue;
	}

	static const char* ImageString(std::uintptr_t address)
	{
		return reinterpret_cast<const char*>(Utils::Hook::Rebase(address));
	}

	static Game::dvar_t* Dvar_RegisterScoreboardPingText(const char* name, [[maybe_unused]] bool value,
		[[maybe_unused]] unsigned int flags, const char* description)
	{
		return Game::Dvar_RegisterBool(name, true, Game::DVAR_CHEAT, description);
	}

	void QuickPatch::UnlockStats()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		if (Game::CL_IsCgameInitialized(0))
		{
			Toast::Show("cardicon_locked", "^1Error", "Not allowed while ingame.", 3000);
			return;
		}

		void* const def = reinterpret_cast<void*(*)(const char*, unsigned int)>(
			Utils::Hook::Rebase(StructuredDataDef_GetAsset))("mp/playerdata.def", playerDataSize);

		if (!def || !IsPlayerDataPath(def, { "prestige" }) || !IsPlayerDataPath(def, { "experience" })
			|| !IsPlayerDataPath(def, { "iconUnlocked", "cardicon_prestige10_02" }))
		{
			Logger::Print("unlockstats: mp/playerdata.def does not have the fields to unlock\n");
			return;
		}

		SetPlayerData("\"prestige\" , 10 )");
		SetPlayerData("\"experience\" , 2516000 )");
		SetPlayerData("\"iconUnlocked\" , \"cardicon_prestige10_02\" , 1 )");

		void* table = nullptr;
		reinterpret_cast<void(*)(const char*, void**)>(Utils::Hook::Rebase(StringTable_GetAsset))("mp/allchallengestable.csv", &table);

		if (table)
		{
			const auto cell = reinterpret_cast<const char*(*)(void*, int, int)>(Utils::Hook::Rebase(StringTable_GetColumnValueForRow));
			const int rowCount = *reinterpret_cast<const int*>(static_cast<const std::uint8_t*>(table) + stringTableRowCount);

			for (int row = 0; row < rowCount; ++row)
			{
				const char* const challenge = cell(table, row, 0);

				int maxState = 0;
				int maxProgress = 0;

				for (int tier = 0; tier < 10; ++tier)
				{
					const int progress = std::atoi(cell(table, row, 6 + tier * 2));

					if (!progress)
					{
						break;
					}

					maxState = tier + 2;
					maxProgress = progress;
				}

				if (!*challenge || !IsPlayerDataPath(def, { "challengeState", challenge })
					|| !IsPlayerDataPath(def, { "challengeProgress", challenge }))
				{
					continue;
				}

				SetPlayerData(std::format("\"challengeState\" , \"{}\" , {} )", challenge, maxState));
				SetPlayerData(std::format("\"challengeProgress\" , \"{}\" , {} )", challenge, maxProgress));
			}
		}

		Game::Cbuf_AddText(0, "uploadStats\n");
	}

	constexpr std::uintptr_t ClientEvents_FireWeaponJump = 0x140194EDF;
	constexpr std::uintptr_t ClientEvents_FireWeaponMeleeJump = 0x140194EFA;
	constexpr std::uintptr_t FireWeapon = 0x1401877F0;
	constexpr std::uintptr_t FireWeaponMelee = 0x140187DE0;
	constexpr std::uintptr_t level_time = 0x1418673E8;

	static Dvar::Var g_antilag;
	static Utils::Hook fireWeaponHook;
	static Utils::Hook fireWeaponMeleeHook;

	static int AntilagTime(const int gameTime)
	{
		if (g_antilag.IsValid() && !g_antilag.Get<bool>())
		{
			return Utils::Hook::Get<int>(level_time);
		}

		return gameTime;
	}

	static void FireWeapon_Hk(Game::gentity_s* ent, const int gameTime, const int hand)
	{
		reinterpret_cast<void(*)(Game::gentity_s*, int, int)>(Utils::Hook::Rebase(FireWeapon))(ent, AntilagTime(gameTime), hand);
	}

	static void FireWeaponMelee_Hk(Game::gentity_s* ent, const int gameTime)
	{
		reinterpret_cast<void(*)(Game::gentity_s*, int)>(Utils::Hook::Rebase(FireWeaponMelee))(ent, AntilagTime(gameTime));
	}

	extern "C"
	{
		void VehicleCl_ResetEntity_PlayerIndexStub();
		void VehicleCl_ProcessEntity_PlayerIndexStub();
		void VehicleFx_PlayerIndexCheckStub();
		void SV_UserinfoChanged_LanRateStub();

		std::uintptr_t QuickPatch_Sys_IsLANAddress = 0;
	}

	constexpr std::uintptr_t VehicleCl_ResetEntity_PlayerIndexStore = 0x14029828C;
	static const std::uint8_t resetEntityStore[] = { 0x8B, 0x85, 0x58, 0x01, 0x00, 0x00, 0x89, 0x46, 0x38 };

	constexpr std::uintptr_t VehicleCl_ProcessEntity_PlayerIndexStore = 0x1402988C8;
	static const std::uint8_t processEntityStore[] = { 0x8B, 0x82, 0x58, 0x01, 0x00, 0x00, 0x42, 0x89, 0x84, 0x23, 0x88, 0x44, 0x74, 0x06 };

	constexpr std::uintptr_t VehicleFx_PlayerIndexCompare = 0x14029BDED;
	static const std::uint8_t playerIndexCompare[] = { 0x48, 0x69, 0xD0, 0x48, 0x05, 0x00, 0x00, 0x46, 0x3B, 0x8C, 0x2A, 0x4C, 0x9B, 0x10, 0x00, 0xEB, 0x03 };
	constexpr std::size_t playerIndexCompareLength = 15;

	static Utils::Hook vehicleHooks[3];

	static void PatchVehiclePlayerIndex()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(VehicleCl_ResetEntity_PlayerIndexStore, resetEntityStore, sizeof(resetEntityStore))
			&& Utils::Hook::MatchesBytes(VehicleCl_ProcessEntity_PlayerIndexStore, processEntityStore, sizeof(processEntityStore))
			&& Utils::Hook::MatchesBytes(VehicleFx_PlayerIndexCompare, playerIndexCompare, sizeof(playerIndexCompare));

		if (!isExpected)
		{
			Logger::Error("quickpatch: the vehicle player index code does not read as expected, a bad vehicle can still crash the client\n");
			return;
		}

		bool isSeated = vehicleHooks[0].Initialize(VehicleCl_ResetEntity_PlayerIndexStore, VehicleCl_ResetEntity_PlayerIndexStub, HOOK_CALL)->Install()->IsInstalled();
		isSeated = vehicleHooks[1].Initialize(VehicleCl_ProcessEntity_PlayerIndexStore, VehicleCl_ProcessEntity_PlayerIndexStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = vehicleHooks[2].Initialize(VehicleFx_PlayerIndexCompare, VehicleFx_PlayerIndexCheckStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : vehicleHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("quickpatch: could not seat the vehicle player index hooks, a bad vehicle can still crash the client\n");
			return;
		}

		for (auto& hook : vehicleHooks)
		{
			hook.Quick();
		}

		Utils::Hook::Nop(VehicleCl_ResetEntity_PlayerIndexStore + 5, sizeof(resetEntityStore) - 5);
		Utils::Hook::Nop(VehicleCl_ProcessEntity_PlayerIndexStore + 5, sizeof(processEntityStore) - 5);
		Utils::Hook::Nop(VehicleFx_PlayerIndexCompare + 5, playerIndexCompareLength - 5);
	}

	constexpr std::uintptr_t SV_UserinfoChanged_LoopbackTest = 0x1402398E7;
	constexpr std::uintptr_t SV_UserinfoChanged_LoopbackJnz = 0x1402398F2;
	constexpr std::size_t loopbackTestLength = 11;

	static const std::uint8_t loopbackRate[] =
	{
		0x83, 0x7D, 0x28, 0x02, 0x48, 0xC7, 0xC3, 0xFF, 0xFF, 0xFF, 0xFF, 0x75, 0x0C, 0xC7, 0x85, 0xE8,
		0x12, 0x02, 0x00, 0x9F, 0x86, 0x01, 0x00, 0xEB, 0x69,
	};

	constexpr std::uintptr_t Sys_IsLANAddress = 0x1402A81D0;

	static const std::uint8_t isLanAddressHead[] =
	{
		0x48, 0x83, 0xEC, 0x28, 0x0F, 0x10, 0x01, 0x4C, 0x8B, 0xC1, 0x66, 0x0F, 0x7E, 0xC0, 0x0F, 0x29,
		0x04, 0x24, 0x83, 0xF8, 0x05, 0x77, 0x0A, 0xB9, 0x25, 0x00, 0x00, 0x00, 0x0F, 0xA3, 0xC1, 0x72,
		0x27,
	};

	static Utils::Hook lanRateHook;

	static void PatchLanRate()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(SV_UserinfoChanged_LoopbackTest, loopbackRate, sizeof(loopbackRate))
			&& Utils::Hook::MatchesBytes(Sys_IsLANAddress, isLanAddressHead, sizeof(isLanAddressHead));

		if (!isExpected)
		{
			Logger::Error("quickpatch: SV_UserinfoChanged or Sys_IsLANAddress does not read as expected, only loopback gets full rate\n");
			return;
		}

		QuickPatch_Sys_IsLANAddress = Utils::Hook::Rebase(Sys_IsLANAddress);

		if (!lanRateHook.Initialize(SV_UserinfoChanged_LoopbackTest, SV_UserinfoChanged_LanRateStub, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("quickpatch: could not seat SV_UserinfoChanged's LAN rate hook, only loopback gets full rate\n");
			return;
		}

		lanRateHook.Quick();

		Utils::Hook::Nop(SV_UserinfoChanged_LoopbackTest + 5, loopbackTestLength - 5);
		Utils::Hook::Set<std::uint8_t>(SV_UserinfoChanged_LoopbackJnz, 0x74);
	}

	constexpr std::uintptr_t SND_GetAliasOffset = 0x140280B70;
	static const std::uint8_t getAliasOffsetEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57 };

	constexpr std::size_t aliasListHead = 0x8;
	constexpr std::size_t aliasListCount = 0x10;
	constexpr std::size_t soundAliasSize = 0x88;

	static Utils::Hook getAliasOffsetHook;

	constexpr std::uintptr_t SV_IsClientUsingOnlineStatsOffline = 0x1402389D0;
	static const std::uint8_t usingOnlineStatsOfflineBody[] = { 0x48, 0x63, 0xC1, 0x48, 0x69, 0xC8, 0xB0, 0x67, 0x0A, 0x00 };

	static int SND_GetAliasOffset_Hk(const void* alias)
	{
		const auto* const name = *static_cast<const char* const*>(alias);

		auto* list = static_cast<const std::uint8_t*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_SOUND, name));

		if (Game::DB_IsXAssetDefault(Game::ASSET_TYPE_SOUND, name))
		{
			list = nullptr;
		}

		if (!list)
		{
			Game::Com_Error(Game::ERR_DROP, "SND_GetAliasOffset: Could not find sound alias '%s'", name);
			return 0;
		}

		const auto count = *reinterpret_cast<const int*>(list + aliasListCount);
		const auto* const head = *reinterpret_cast<const std::uint8_t* const*>(list + aliasListHead);

		for (int i = 0; i < count; ++i)
		{
			if (head + i * soundAliasSize == alias)
			{
				return i;
			}
		}

		return 0;
	}

	constexpr std::uintptr_t r_aspectRatioRegisterCall = 0x14002FF95;
	constexpr std::uintptr_t R_CreateDevice_WideScreenCall = 0x140031F69;
	constexpr std::uintptr_t Dvar_RegisterEnum = 0x140285F50;
	constexpr std::uintptr_t Dvar_SetBool = 0x140286F40;
	constexpr std::uintptr_t vidConfig_aspectRatioWindow = 0x148CCC918;

	constexpr int customAspectRatioIndex = 4;

	static Dvar::Var r_customAspectRatio;
	static const Game::dvar_t* r_aspectRatio;
	static Utils::Hook aspectRatioRegisterHook;
	static Utils::Hook wideScreenHook;

	static Game::dvar_t* Dvar_RegisterAspectRatioDvar(const char* dvarName, [[maybe_unused]] const char** valueList, const int defaultIndex, const unsigned int flags, const char* description)
	{
		static const char* r_aspectRatioEnum[] =
		{
			"auto",
			"standard",
			"wide 16:10",
			"wide 16:9",
			"custom",
			nullptr
		};

		r_customAspectRatio = Dvar::Register("r_customAspectRatio", 16.0f / 9.0f, 4.0f / 3.0f, 63.0f / 9.0f, flags,
			"Screen aspect ratio. Divide the width by the height in order to get the aspect ratio value. For example: 16 / 9 = 1,77");

		auto* const dvar = Game::Dvar_RegisterEnum(dvarName, r_aspectRatioEnum, defaultIndex, flags, description);
		r_aspectRatio = dvar;

		return dvar;
	}

	static void Dvar_SetWideScreen_Hk(const Game::dvar_t* dvar, bool isWideScreen)
	{
		if (r_aspectRatio->current.integer == customAspectRatioIndex)
		{
			Utils::Hook::Set<float>(vidConfig_aspectRatioWindow, r_customAspectRatio.Get<float>());
			isWideScreen = true;
		}

		Game::Dvar_SetBool(dvar, isWideScreen);
	}

	constexpr std::uintptr_t CL_InitRef_ConfigureCall = 0x1400FBD3D;
	constexpr std::uintptr_t CL_Vid_Restart_f_ConfigureCall = 0x1400FDE64;
	constexpr std::uintptr_t R_ConfigureRenderer = 0x1400334D0;
	constexpr std::size_t gfxConfigDisplayMode = 0x58;
	constexpr int displayModeWindowed = 2;

	constexpr std::uintptr_t R_InitGraphicsApi_EnumModesCall = 0x140032792;
	constexpr std::uintptr_t R_EnumDisplayModes = 0x1400316F0;
	constexpr std::uintptr_t dx_d3d9 = 0x148CC6BE8;

	static Utils::Hook configureRendererHook;
	static Utils::Hook restartConfigureRendererHook;
	static Utils::Hook enumDisplayModesHook;

	static void CL_InitRef_Hk(std::uint8_t* config)
	{
		*reinterpret_cast<int*>(config + gfxConfigDisplayMode) = displayModeWindowed;

		reinterpret_cast<void(*)(std::uint8_t*)>(Utils::Hook::Rebase(R_ConfigureRenderer))(config);
	}

	static void R_EnumDisplayModes_Hk(const unsigned int adapterIndex)
	{
		reinterpret_cast<void(*)(unsigned int)>(Utils::Hook::Rebase(R_EnumDisplayModes))(adapterIndex);

		Window::ApplyDisplayModeDvars();

		if (!Dvar::Var("g_firstLaunch").Get<bool>())
		{
			return;
		}

		auto* const d3d9 = Utils::Hook::Get<IDirect3D9*>(dx_d3d9);
		if (!d3d9)
		{
			return;
		}

		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(MONITORINFO);

		if (!GetMonitorInfoA(d3d9->GetAdapterMonitor(adapterIndex), &monitorInfo))
		{
			return;
		}

		const int monitorWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
		const int monitorHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;

		auto* const r_mode = Game::Dvar_FindVar("r_mode");
		if (!r_mode || !r_mode->domain.enumeration.strings || r_mode->domain.enumeration.stringCount <= 0)
		{
			return;
		}

		int modeIndex = r_mode->current.integer;

		for (int i = 0; i < r_mode->domain.enumeration.stringCount; ++i)
		{
			int modeWidth = 0;
			int modeHeight = 0;

			if (std::sscanf(r_mode->domain.enumeration.strings[i], "%ix%i", &modeWidth, &modeHeight) != 2)
			{
				continue;
			}

			if (modeWidth == monitorWidth && modeHeight == monitorHeight)
			{
				modeIndex = i;
				break;
			}
		}

		Game::Dvar_SetInt(r_mode, modeIndex);

		if (!Flags::HasFlag("dev"))
		{
			Dvar::Var("g_firstLaunch").Set(false);
		}
	}

	constexpr std::uintptr_t j_CL_Snd_Restart_f = 0x1400FD8D0;
	constexpr std::uintptr_t CL_Snd_Restart_f = 0x1402C1F70;
	constexpr std::uintptr_t CL_Vid_Restart_f = 0x1400FDB90;

	static Utils::Hook sndRestartHook;

	static void CL_Snd_Restart_f_Hk()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CL_Snd_Restart_f))();
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CL_Vid_Restart_f))();
	}

	static const Utils::Hook::LeaSite addImageToListLeas[] =
	{
		{ 0x14003831D, Utils::Hook::leaRdx, 0x140037660 },
		{ 0x14003838C, Utils::Hook::leaRdx, 0x140037660 },
	};

	static void R_AddImageToList_Hk(Game::GfxImage* image, void* data)
	{
		auto* const imageList = static_cast<Game::ImageList*>(data);

		assert(imageList->count < ARRAYSIZE(imageList->image));

		if (image->texture.basemap)
		{
			imageList->image[imageList->count++] = image;
		}
	}

	static void PatchImageList()
	{
		for (const auto& lea : addImageToListLeas)
		{
			if (!Utils::Hook::IsLeaIntact(lea))
			{
				Logger::Error("quickpatch: R_ImageList_f does not read as expected, imagelist still crashes on an image with no texture\n");
				return;
			}
		}

		const auto addImageToList = Utils::Hook::Trampoline(Utils::Hook::Rebase(addImageToListLeas[0].address), reinterpret_cast<std::uintptr_t>(R_AddImageToList_Hk));

		for (const auto& lea : addImageToListLeas)
		{
			if (!addImageToList || !Utils::Hook::CanLeaReach(lea, reinterpret_cast<void*>(addImageToList)))
			{
				Logger::Error("quickpatch: no room for R_AddImageToList beside the image, imagelist still crashes on an image with no texture\n");
				return;
			}
		}

		for (const auto& lea : addImageToListLeas)
		{
			Utils::Hook::PointLeaAt(lea, reinterpret_cast<void*>(addImageToList));
		}
	}

	constexpr std::uintptr_t CG_DrawChatMessages_LineHeightLoad = 0x1400D1342;
	constexpr std::uintptr_t CG_DrawChatMessages_IconWidthLoad = 0x1400D136B;
	static const std::uint8_t lineHeightLoad[] = { 0xF3, 0x44, 0x0F, 0x10, 0x25, 0x49, 0x37, 0x31, 0x00 };
	static const std::uint8_t iconWidthLoad[] = { 0xF3, 0x44, 0x0F, 0x10, 0x35, 0x1C, 0x37, 0x31, 0x00 };
	constexpr std::size_t movssDisplacement = 5;

	static std::int64_t LoadDistance(const std::uintptr_t load, const std::size_t length, const float* target)
	{
		return static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(target)) - static_cast<std::int64_t>(Utils::Hook::Rebase(load) + length);
	}

	static void PatchChatFontSizes()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(CG_DrawChatMessages_LineHeightLoad, lineHeightLoad, sizeof(lineHeightLoad))
			&& Utils::Hook::MatchesBytes(CG_DrawChatMessages_IconWidthLoad, iconWidthLoad, sizeof(iconWidthLoad));

		if (!isExpected)
		{
			Logger::Error("quickpatch: CG_DrawChatMessages does not read as expected, chat keeps its font sizes\n");
			return;
		}

		auto* const sizes = static_cast<float*>(Utils::Hook::AllocateDataNear(CG_DrawChatMessages_LineHeightLoad, 2 * sizeof(float)));
		if (!sizes)
		{
			Logger::Error("quickpatch: no room for the chat font sizes beside the image\n");
			return;
		}

		auto* const lineHeight = &sizes[0];
		auto* const iconWidth = &sizes[1];
		*lineHeight = 13.0f;
		*iconWidth = 10.0f;

		const auto lineHeightDistance = LoadDistance(CG_DrawChatMessages_LineHeightLoad, sizeof(lineHeightLoad), lineHeight);
		const auto iconWidthDistance = LoadDistance(CG_DrawChatMessages_IconWidthLoad, sizeof(iconWidthLoad), iconWidth);

		const bool canReach = lineHeightDistance >= std::numeric_limits<std::int32_t>::min() && lineHeightDistance <= std::numeric_limits<std::int32_t>::max()
			&& iconWidthDistance >= std::numeric_limits<std::int32_t>::min() && iconWidthDistance <= std::numeric_limits<std::int32_t>::max();

		if (!canReach)
		{
			Logger::Error("quickpatch: the chat font sizes landed out of reach, chat keeps its font sizes\n");
			return;
		}

		Utils::Hook::Set<std::int32_t>(CG_DrawChatMessages_LineHeightLoad + movssDisplacement, static_cast<std::int32_t>(lineHeightDistance));
		Utils::Hook::Set<std::int32_t>(CG_DrawChatMessages_IconWidthLoad + movssDisplacement, static_cast<std::int32_t>(iconWidthDistance));
	}

	QuickPatch::QuickPatch()
	{
		Command::Add("unlockstats", UnlockStats);

		if (Utils::Hook::Get<std::uintptr_t>(sv_pure) != Utils::Hook::Rebase(sv_pureConstant))
		{
			Logger::Error("quickpatch: sv_pure does not point at its constant, the server stays pure\n");
		}
		else
		{
			Events::OnDvarInit([]
			{
				sv_pureVar = Dvar::Register("sv_pure", false, Game::DVAR_CODINFO | Game::DVAR_SERVERINFO, "Cannot use modified IWD files");
				Utils::Hook::Set<const void*>(sv_pure, &sv_pureVar.Get()->current.enabled);
			});
		}

		PatchVehiclePlayerIndex();
		PatchImageList();
		PatchChatFontSizes();
		PatchLanRate();

		if (!Utils::Hook::MatchesBytes(SND_GetAliasOffset, getAliasOffsetEntry, sizeof(getAliasOffsetEntry))
			|| !getAliasOffsetHook.Initialize(SND_GetAliasOffset, reinterpret_cast<void*>(SND_GetAliasOffset_Hk), HOOK_JUMP)->Install()->IsInstalled())
		{
			Logger::Error("quickpatch: SND_GetAliasOffset does not read as expected, a missing alias still crashes\n");
		}
		else
		{
			getAliasOffsetHook.Quick();
		}

		if (Utils::Hook::MatchesBytes(SV_IsClientUsingOnlineStatsOffline, usingOnlineStatsOfflineBody, sizeof(usingOnlineStatsOfflineBody)))
		{
			Utils::Hook::Set<std::array<std::uint8_t, 3>>(SV_IsClientUsingOnlineStatsOffline, { 0x33, 0xC0, 0xC3 });
		}
		else
		{
			Logger::Error("quickpatch: SV_IsClientUsingOnlineStatsOffline does not read as expected, left alone\n");
		}

		if (!Utils::Hook::BranchesTo(r_aspectRatioRegisterCall, Dvar_RegisterEnum, HOOK_CALL)
			|| !Utils::Hook::BranchesTo(R_CreateDevice_WideScreenCall, Dvar_SetBool, HOOK_CALL)
			|| !aspectRatioRegisterHook.Initialize(r_aspectRatioRegisterCall, reinterpret_cast<void*>(Dvar_RegisterAspectRatioDvar), HOOK_CALL)->Install()->IsInstalled()
			|| !wideScreenHook.Initialize(R_CreateDevice_WideScreenCall, reinterpret_cast<void*>(Dvar_SetWideScreen_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			aspectRatioRegisterHook.Uninstall();
			wideScreenHook.Uninstall();

			Logger::Error("quickpatch: R_RegisterDvars or R_CreateDevice does not read as expected, no custom aspect ratio\n");
		}
		else
		{
			aspectRatioRegisterHook.Quick();
			wideScreenHook.Quick();
		}

		if (!Utils::Hook::BranchesTo(CL_InitRef_ConfigureCall, R_ConfigureRenderer, HOOK_CALL)
			|| !Utils::Hook::BranchesTo(CL_Vid_Restart_f_ConfigureCall, R_ConfigureRenderer, HOOK_CALL)
			|| !Utils::Hook::BranchesTo(R_InitGraphicsApi_EnumModesCall, R_EnumDisplayModes, HOOK_CALL)
			|| !configureRendererHook.Initialize(CL_InitRef_ConfigureCall, reinterpret_cast<void*>(CL_InitRef_Hk), HOOK_CALL)->Install()->IsInstalled()
			|| !restartConfigureRendererHook.Initialize(CL_Vid_Restart_f_ConfigureCall, reinterpret_cast<void*>(CL_InitRef_Hk), HOOK_CALL)->Install()->IsInstalled()
			|| !enumDisplayModesHook.Initialize(R_InitGraphicsApi_EnumModesCall, reinterpret_cast<void*>(R_EnumDisplayModes_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			configureRendererHook.Uninstall();
			restartConfigureRendererHook.Uninstall();
			enumDisplayModesHook.Uninstall();

			Logger::Error("quickpatch: CL_InitRef or R_InitGraphicsApi does not read as expected, first launch stays fullscreen\n");
		}
		else
		{
			configureRendererHook.Quick();
			restartConfigureRendererHook.Quick();
			enumDisplayModesHook.Quick();
		}

		if (!Utils::Hook::BranchesTo(j_CL_Snd_Restart_f, CL_Snd_Restart_f, HOOK_JUMP)
			|| !sndRestartHook.Initialize(j_CL_Snd_Restart_f, reinterpret_cast<void*>(CL_Snd_Restart_f_Hk), HOOK_JUMP)->Install()->IsInstalled())
		{
			sndRestartHook.Uninstall();

			Logger::Error("quickpatch: snd_restart does not read as expected, an options page's Apply restarts sound only\n");
		}
		else
		{
			sndRestartHook.Quick();
		}

		if (!Utils::Hook::BranchesTo(ClientEvents_FireWeaponJump, FireWeapon, HOOK_JUMP)
			|| !Utils::Hook::BranchesTo(ClientEvents_FireWeaponMeleeJump, FireWeaponMelee, HOOK_JUMP)
			|| !fireWeaponHook.Initialize(ClientEvents_FireWeaponJump, reinterpret_cast<void*>(FireWeapon_Hk), HOOK_JUMP)->Install()->IsInstalled()
			|| !fireWeaponMeleeHook.Initialize(ClientEvents_FireWeaponMeleeJump, reinterpret_cast<void*>(FireWeaponMelee_Hk), HOOK_JUMP)->Install()->IsInstalled())
		{
			fireWeaponHook.Uninstall();
			fireWeaponMeleeHook.Uninstall();

			Logger::Error("quickpatch: ClientEvents does not read as expected, g_antilag does nothing\n");
		}
		else
		{
			fireWeaponHook.Quick();
			fireWeaponMeleeHook.Quick();
		}

		if (Utils::Hook::MatchesBytes(Steam_FileRead_RetryTest, retryTest, sizeof(retryTest)))
		{
			Utils::Hook::Set<std::uint8_t>(Steam_FileRead_RetryExit, 0xEB);
		}
		else
		{
			Logger::Error("quickpatch: Steam_FileRead does not read as expected, a failed read still sleeps\n");
		}

		if (Utils::Hook::MatchesBytes(IWNet_Frame_DNSLookup, dnsLookupHead, sizeof(dnsLookupHead)))
		{
			const auto relative = static_cast<std::int32_t>(IWNet_Frame_AfterLookup - (IWNet_Frame_DNSLookup + 5));

			Utils::Hook::Set<std::int32_t>(IWNet_Frame_DNSLookup + 1, relative);
			Utils::Hook::Set<std::uint8_t>(IWNet_Frame_DNSLookup, 0xE9);
			Utils::Hook::Nop(IWNet_Frame_DNSLookup + 5, 2);
		}
		else
		{
			Logger::Error("quickpatch: IWNet_Frame does not read as expected, the iwnet lookup still runs\n");
		}

		if (Utils::Hook::MatchesBytes(Live_Frame_LostConnectionCheck, lostConnectionCheckHead, sizeof(lostConnectionCheckHead)))
		{
			Utils::Hook::Set<std::uint8_t>(Live_Frame_LostConnectionCheck, 0xC3);
		}
		else
		{
			Logger::Error("quickpatch: Live_Frame's lost connection check does not read as expected, losing internet still drops the game\n");
		}

		if (Utils::Hook::BranchesTo(Com_Frame_UpdateSystemDvarsCall, GamerProfile_UpdateSystemDvars, HOOK_CALL))
		{
			Utils::Hook::Nop(Com_Frame_UpdateSystemDvarsCall, 5);
		}
		else
		{
			Logger::Error("quickpatch: Com_Frame does not read as expected, the profile still holds volume, gamma and safe area\n");
		}

		Scheduler::Once([]
		{
			Dvar::Register("scr_intermissionTime", 10.0f, 0.0f, 120.0f, Game::DVAR_NONE, "Time in seconds before match server loads the next map");
			ui_mousePitch = Dvar::Register("ui_mousePitch", false, Game::DVAR_ARCHIVE, "");
			g_antilag = Dvar::Register("g_antilag", true, Game::DVAR_CODINFO, "Perform antilag");
		}, Scheduler::Pipeline::MAIN);

		if (Utils::Hook::MatchesBytes(Sys_AllowVidRestart, allowVidRestartHead, sizeof(allowVidRestartHead)))
		{
			Utils::Hook::Set<std::array<std::uint8_t, 3>>(Sys_AllowVidRestart, { 0xB0, 0x01, 0xC3 });
		}
		else
		{
			Logger::Error("quickpatch: Sys_AllowVidRestart does not read as expected, alt-enter still refuses in game\n");
		}

		if (!Utils::Hook::TryPointLeaAt(windowNameLea, Utils::Hook::PlaceNearImage("Call of Duty: Zombie Warfare 3")))
		{
			Logger::Error("quickpatch: the window title does not read as expected, left alone\n");
		}

		if (!Utils::Hook::TryPointLeaAt(hostnameDefaultLea, Utils::Hook::PlaceNearImage("ZW3Host")))
		{
			Logger::Error("quickpatch: sv_hostname's registration does not read as expected, its default stays CoD4Host\n");
		}

		if (!Utils::Hook::TryPointLeaAt(consoleLogoLea, Utils::Hook::PlaceNearImage("zw3/data/images/logo.bmp")))
		{
			Logger::Error("quickpatch: the console logo does not read as expected, left alone\n");
		}

		if (!Utils::Hook::TryPointLeaAt(splashLea, Utils::Hook::PlaceNearImage("zw3/data/images/splash.bmp")))
		{
			Logger::Error("quickpatch: the splash does not read as expected, left alone\n");
		}

		if (!Utils::Hook::MatchesBytes(CG_RegisterScoreboardDvars_PingText, pingTextRegister, sizeof(pingTextRegister))
			|| !pingTextHook.Initialize(CG_RegisterScoreboardDvars_PingTextCall,
				reinterpret_cast<void*>(Dvar_RegisterScoreboardPingText), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("quickpatch: could not take cg_scoreboardPingText's registration, ping stays graphical\n");
		}
		else
		{
			pingTextHook.Quick();
		}

		if (Utils::Hook::MatchesBytes(Com_Init_CommandLineCount, commandLineCount, sizeof(commandLineCount))
			&& Utils::Hook::MatchesBytes(Com_Init_CommandLineList, commandLineList, sizeof(commandLineList)))
		{
			Utils::Hook::Set<std::uint8_t>(Com_Init_CommandLineJle, 0xEB);
		}
		else
		{
			Logger::Error("quickpatch: Com_Init's command line filter does not read as expected, only whitelisted commands pass\n");
		}

		if (Utils::Hook::MatchesBytes(Sys_Init_MajorVersionTest, majorVersionTest, sizeof(majorVersionTest))
			&& Utils::Hook::Get<char>(Sys_Init_MajorVersionDigit) == '4')
		{
			Utils::Hook::Set<std::uint8_t>(Sys_Init_MajorVersionTest + 4, 6);
			Utils::Hook::Set<char>(Sys_Init_MajorVersionDigit, '6');
		}
		else
		{
			Logger::Error("quickpatch: Sys_Init's windows version test does not read as expected, left alone\n");
		}

		if (Utils::Hook::MatchesBytes(Com_Init_MigrationDvarErrors, migrationDvarErrors, sizeof(migrationDvarErrors)))
		{
			Utils::Hook::Set<std::uint8_t>(Com_Init_MigrationDvarErrorsDefault, 0);
		}
		else
		{
			Logger::Error("quickpatch: migration_dvarErrors' registration does not read as expected, it stays on\n");
		}

		if (Utils::Hook::MatchesBytes(ClientConnect_DeveloperTest, developerTest, sizeof(developerTest)))
		{
			Utils::Hook::Set<std::uint8_t>(ClientConnect_DeveloperJz, 0xEB);
		}
		else
		{
			Logger::Error("quickpatch: ClientConnect's developer test does not read as expected, developer servers still refuse joins\n");
		}

		if (Utils::Hook::MatchesBytes(FS_Restart_DefaultCfg, defaultCfgCheck, sizeof(defaultCfgCheck)))
		{
			const auto relative = static_cast<std::int32_t>(FS_Restart_DefaultFound - (FS_Restart_DefaultJz + 5));

			Utils::Hook::Nop(FS_Restart_FindCall, 5);
			Utils::Hook::Nop(FS_Restart_IsDefaultCall, 5);
			Utils::Hook::Set<std::int32_t>(FS_Restart_DefaultJz + 1, relative);
			Utils::Hook::Set<std::uint8_t>(FS_Restart_DefaultJz, 0xE9);
			Utils::Hook::Nop(FS_Restart_DefaultJz + 5, 1);
		}
		else
		{
			Logger::Error("quickpatch: FS_Restart's default_mp.cfg check does not read as expected, left alone\n");
		}

		if (Utils::Hook::MatchesBytes(CL_Vid_Restart_f_AllowCall, vidRestartAllowTest, sizeof(vidRestartAllowTest)))
		{
			Utils::Hook::Nop(CL_Vid_Restart_f_AllowJz, 6);
		}
		else
		{
			Logger::Error("quickpatch: CL_Vid_Restart_f does not read as expected, vid_restart still refuses in game\n");
		}

		if (Utils::Hook::MatchesBytes(Com_Init_Intro, introBlock, sizeof(introBlock))
			&& Utils::Hook::TryPointLeaAt(introCinematicLea, Utils::Hook::PlaceNearImage(zw3IntroCommand)))
		{
			Utils::Hook::Nop(Com_Init_LegalSetString, 5);
			Utils::Hook::Nop(Com_Init_IntroSetBool, 5);

			if (Flags::HasFlag("nointro"))
			{
				Utils::Hook::Set<std::uint8_t>(Com_Init_IntroJz, 0xEB);
			}
		}
		else
		{
			Logger::Error("quickpatch: Com_Init's intro does not read as expected, left alone\n");
		}

		const char* const binkPath = Utils::Hook::PlaceNearImage(zw3Video);

		if (binkPath && Utils::Hook::IsLeaIntact(binkMainLea) && Utils::Hook::IsLeaIntact(binkRawLea) && Utils::Hook::CanLeaReach(binkMainLea, binkPath))
		{
			Utils::Hook::PointLeaAt(binkMainLea, binkPath);
			Utils::Hook::PointLeaAt(binkRawLea, ImageString(binkMainLea.target));
		}
		else
		{
			Logger::Error("quickpatch: R_Cinematic_BinkOpen does not read as expected, videos only come from main\n");
		}

		if (Utils::Hook::MatchesBytes(Com_Init_HasInfoChangedCall, hasInfoChangedBlock, sizeof(hasInfoChangedBlock)))
		{
			for (std::size_t i = 0; i < sizeof(returnFalse); ++i)
			{
				Utils::Hook::Set<std::uint8_t>(Com_Init_HasInfoChangedCall + i, returnFalse[i]);
			}
		}
		else
		{
			Logger::Error("quickpatch: Com_Init's hardware compare does not read as expected, a changed machine still gets the recommended settings\n");
		}

		const bool areDefaultsIntact = std::ranges::all_of(zeroDefaults, [](const DvarDefaultSite& site)
		{
			return Utils::Hook::IsLeaIntact(site.name) && Utils::Hook::MatchesBytes(site.load, site.loadBytes.data(), site.loadSize);
		});

		if (areDefaultsIntact)
		{
			for (const auto& site : zeroDefaults)
			{
				for (std::size_t i = 0; i < site.valueSize; ++i)
				{
					Utils::Hook::Set<std::uint8_t>(site.load + site.valueOffset + i, 0);
				}
			}
		}
		else
		{
			Logger::Error("quickpatch: a party or server timer registration does not read as expected, the stock defaults stay\n");
		}

		const bool areResendComparesIntact = std::ranges::all_of(resendCompares, [](const ResendCompare& compare)
		{
			return Utils::Hook::MatchesBytes(compare.address, compare.bytes.data(), compare.size);
		});

		if (!Dedicated::IsEnabled() && areResendComparesIntact)
		{
			Scheduler::Loop([]
			{
				static bool wasLocal = false;
				const bool isLocal = IsLocalServer();

				if (isLocal != wasLocal)
				{
					SetResendCompares(isLocal);
					wasLocal = isLocal;
				}
			}, Scheduler::Pipeline::MAIN);
		}
		else if (!Dedicated::IsEnabled())
		{
			Logger::Error("quickpatch: CL_CheckForResend or CL_SendCmd does not read as expected, a local connect keeps the remote waits\n");
		}

		const bool areLoopFunctionsIntact = Utils::Hook::MatchesBytes(NET_SendPacket, sendPacketEntry, sizeof(sendPacketEntry))
			&& Utils::Hook::BranchesTo(NET_GetLoopPacket, NET_GetLoopPacket_Real, HOOK_JUMP);

		if (areLoopFunctionsIntact)
		{
			const bool isSeated = sendPacketHook.Initialize(NET_SendPacket, reinterpret_cast<void*>(NET_SendPacket_Hk), HOOK_JUMP)->Install()->IsInstalled()
				&& getLoopPacketHook.Initialize(NET_GetLoopPacket, reinterpret_cast<void*>(NET_GetLoopPacket_Hk), HOOK_JUMP)->Install()->IsInstalled();

			if (!isSeated)
			{
				sendPacketHook.Uninstall();
				getLoopPacketHook.Uninstall();
				Logger::Error("quickpatch: could not seat the loopback queues, the stock 12 and 64 slots stay\n");
			}
			else
			{
				sendPacketHook.Quick();
				getLoopPacketHook.Quick();

				Events::OnCLDisconnected([](bool)
				{
					ResetLoopQueues();
				});
			}
		}
		else
		{
			Logger::Error("quickpatch: NET_SendPacket or NET_GetLoopPacket does not read as expected, the stock loopback queues stay\n");
		}

		Scheduler::Once([]
		{
			for (const char* name : zeroedDvars)
			{
				if (auto* const dvar = Game::Dvar_FindVar(name))
				{
					Game::Dvar_SetInt(dvar, 0);
				}
			}
		}, Scheduler::Pipeline::MAIN);

		if (!Utils::Hook::TryPointLeaAt(gameLogLea, Utils::Hook::PlaceNearImage("logs/games_mp.log")))
		{
			Logger::Error("quickpatch: g_log's registration does not read as expected, its default stays games_mp.log\n");
		}

		if (!Utils::Hook::TryPointLeasAt(configNameLeas, Utils::Hook::PlaceNearImage(CLIENT_CONFIG)))
		{
			Logger::Error("quickpatch: the config_mp.cfg references do not read as expected, the config keeps its stock name\n");
		}

		if (Utils::Hook::MatchesBytes(MainWndProc_ExecutionState, executionStateCall, sizeof(executionStateCall)))
		{
			Utils::Hook::Nop(MainWndProc_ExecutionStateCall, 6);

			Scheduler::Loop([]
			{
				SetThreadExecutionState(ES_DISPLAY_REQUIRED);
			}, Scheduler::Pipeline::RENDERER);
		}
		else
		{
			Logger::Error("quickpatch: MainWndProc does not read as expected, every window message still sets the execution state\n");
		}

		UIScript::Add("updateui_mousePitch", [](const UIScript::Token&)
		{
			if (ui_mousePitch.Get<bool>())
			{
				Dvar::Var("m_pitch").Set(-0.022f);
			}
			else
			{
				Dvar::Var("m_pitch").Set(0.022f);
			}
		});
	}
}
