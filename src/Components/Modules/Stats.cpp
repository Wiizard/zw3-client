#include "STDInclude.hpp"

#include "Stats.hpp"
#include "Command.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "Scheduler.hpp"
#include "ServerCommands.hpp"
#include "UIScript.hpp"

#include "GSC/Script.hpp"

namespace Components
{
	bool Stats::isInstalled = false;

	constexpr std::uintptr_t Steam_FileRead_Core_GetSteamIDCall = 0x14024CEA2;
	constexpr std::uintptr_t Steam_FileWrite_Core_GetSteamIDCall = 0x14024D317;

	constexpr std::size_t readGetSteamIDLength = 21;
	constexpr std::size_t writeGetSteamIDLength = 24;

	static const std::uint8_t readGetSteamID[] =
	{
		0xFF, 0x15, 0xD0, 0x5B, 0x11, 0x00,
		0x48, 0x8D, 0x54, 0x24, 0x38,
		0x48, 0x8B, 0xC8,
		0x4C, 0x8B, 0x10,
		0x41, 0xFF, 0x52, 0x10,
		0x4C, 0x8B, 0x28,
	};

	static const std::uint8_t writeGetSteamID[] =
	{
		0xFF, 0x15, 0x5B, 0x57, 0x11, 0x00,
		0x48, 0x8D, 0x54, 0x24, 0x30,
		0x48, 0x8B, 0x08,
		0x4C, 0x8B, 0x41, 0x10,
		0x48, 0x8B, 0xC8,
		0x41, 0xFF, 0xD0,
		0x48, 0xC7, 0xC2, 0xFF, 0xFF, 0xFF, 0xFF,
		0x4C, 0x8B, 0x30,
	};

	constexpr std::uintptr_t Steam_FileRead_Core_SourceTest = 0x14024CEDF;
	constexpr std::uintptr_t Steam_FileRead_Core_CloudBranch = 0x14024CEE4;
	constexpr std::uintptr_t Steam_FileRead_Core_NotFound = 0x14024D1FF;

	static const std::uint8_t readSourceTest[] = { 0x83, 0xFB, 0x01, 0x75, 0x46, 0x48, 0x8B, 0x0F, 0x49, 0x8B, 0xD7 };
	static const std::uint8_t readNotFound[] = { 0xB8, 0x01, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t Steam_FileWrite_Core_SourceTest = 0x14024D54C;

	static const std::uint8_t writeSourceTest[] = { 0x75, 0x25 };

	constexpr std::uintptr_t Steam_FileWrite_BakTest = 0x14024D249;

	static const std::uint8_t writeBakTest[] = { 0x74, 0x2D };

	constexpr std::uintptr_t statFileFormat = 0x1403A0568;

	static const std::uint8_t statFileFormatText[] = "%s_%I64x.stat";

	constexpr std::uintptr_t LiveStorage_GenerateFilename_Com_sprintfCall = 0x1401F7A66;

	static const std::uint8_t generateFilenameCall[] = { 0xE8, 0x45, 0x44, 0x09, 0x00 };

	constexpr std::uintptr_t LiveStorage_SetStat_ConnectedTest = 0x1401F9183;
	constexpr std::uintptr_t LiveStorage_SetStat = 0x1401F9150;

	static const std::uint8_t setStatConnectedTest[] = { 0x7C, 0x14 };

	constexpr std::uintptr_t LiveStorage_DataSetCmd_ConnectedTest = 0x1401F737F;

	static const std::uint8_t dataSetConnectedTest[] = { 0x7C, 0x18 };

	constexpr std::uintptr_t CG_ServerCommand_StatsIntegrityTest = 0x1400E8D48;
	constexpr std::uintptr_t CG_ServerCommand_LossesTest = 0x1400E8E16;
	constexpr std::uintptr_t CG_ServerCommand_BadStatsTest = 0x1400E8E29;

	static const std::uint8_t statsIntegrityTest[] =
	{
		0x41, 0x83, 0xFE, 0x20,
		0x75, 0x11,
		0x48, 0x8B, 0xD5,
		0x48, 0x8D, 0x0D, 0xE8, 0x8D, 0x5D, 0x00,
		0xE8, 0xA3, 0xAD, 0x19, 0x00,
		0xEB, 0x1C,
		0x45, 0x85, 0xF6,
		0x74, 0x17,
		0x45, 0x8B, 0xCE,
		0x4C, 0x8D, 0x44, 0x24, 0x40,
		0x48, 0x8B, 0xD5,
		0x48, 0x8D, 0x0D, 0xCA, 0x8D, 0x5D, 0x00,
		0xE8, 0xF5, 0xAD, 0x19, 0x00,
	};

	static const std::uint8_t lossesTest[] =
	{
		0x7E, 0x0F,
		0x48, 0x8D, 0x15, 0x39, 0xD4, 0x28, 0x00,
		0x41, 0x8B, 0xCC,
		0xE8, 0x09, 0xFE, 0x10, 0x00,
	};

	static const std::uint8_t badStatsTest[] = { 0x0F, 0x84, 0x93, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t clc_qport = 0x140BFB7A0;

	constexpr std::uintptr_t Steam_FileRead_Core_ChecksumCall = 0x14024D13D;
	constexpr std::uintptr_t Steam_FileWrite_Core_ChecksumCall = 0x14024D380;

	static const std::uint8_t readChecksumCall[] = { 0xE8, 0x0E, 0xFF, 0xE2, 0xFF };
	static const std::uint8_t writeChecksumCall[] = { 0xE8, 0xCB, 0xFC, 0xE2, 0xFF };

	constexpr std::uint32_t statsKeyHash = 0xFC30D07E;

	constexpr std::uintptr_t fileCoreScratch = 0x14651F0A0;

	static Utils::Hook readChecksumHook;
	static Utils::Hook writeChecksumHook;

	using Checksum_t = void(*)(const void* data, unsigned int size, unsigned int key, void* checksum);

	bool Stats::IsInstalled()
	{
		return isInstalled;
	}

	const std::int64_t* Stats::GetStatsID()
	{
		static constexpr std::int64_t id = 0x110000100001337;
		return &id;
	}

	void Stats::SprintfLiveStorageFilename(char* target, std::size_t size)
	{
		constexpr auto statName = "iw4x.stat";

		const Game::dvar_t* const fsGame = Game::Dvar_FindVar("fs_game");

		if (!fsGame || !*fsGame->current.string)
		{
			const std::size_t length = strnlen(statName, size);
			std::memcpy(target, statName, length);

			if (length < size)
			{
				target[length] = '\0';
			}

			return;
		}

		SprintfLiveStorageFilenameWithFsGame(target, size, fsGame->current.string);
	}

	void Stats::SprintfLiveStorageFilenameWithFsGame(char* target, std::size_t size, const char* modName)
	{
		constexpr auto statName = "iw4x.stat";
		const std::size_t statNameLength = strnlen(statName, 16);

		const std::size_t modNameLength = strnlen(modName, size);

		std::memcpy(target, modName, modNameLength);

		if (modNameLength + statNameLength + 1 + 1 > size)
		{
			std::memcpy(&target[size - statNameLength - 1], statName, statNameLength);
			target[size - statNameLength - 2] = '\\';
			target[size - 1] = '\0';

			return;
		}

		target[modNameLength] = '\\';
		std::memcpy(&target[modNameLength + 1], statName, statNameLength);
		target[modNameLength + 1 + statNameLength] = '\0';
	}

	void Stats::Steam_FileWrite_Core_Checksum(const void* data, unsigned int size, [[maybe_unused]] unsigned int key, void* checksum)
	{
		reinterpret_cast<Checksum_t>(writeChecksumHook.GetOriginal())(data, size, statsKeyHash, checksum);
	}

	void Stats::Steam_FileRead_Core_Checksum(const void* data, unsigned int size, unsigned int key, void* checksum)
	{
		const auto computeChecksum = reinterpret_cast<Checksum_t>(readChecksumHook.GetOriginal());

		computeChecksum(data, size, statsKeyHash, checksum);

		const auto* const stored = reinterpret_cast<const void*>(Utils::Hook::Rebase(fileCoreScratch));

		if (std::memcmp(checksum, stored, 16) == 0)
		{
			return;
		}

		computeChecksum(data, size, key, checksum);
	}

	void Stats::MoveOldStatsToNewFolder()
	{
		const std::string basePath = (*Game::fs_basepath)->current.string;

		for (const auto& modName : FileSystem::GetSysFileList(basePath + "\\mods", "", true))
		{
			char statName[32]{};
			SprintfLiveStorageFilenameWithFsGame(statName, sizeof(statName), ("mods\\" + modName).data());

			const auto newPath = basePath + "\\players\\" + statName;
			const auto oldPath = basePath + "\\mods\\" + modName + "\\iw4x.stat";

			if (Utils::IO::FileExists(newPath) || !Utils::IO::FileExists(oldPath))
			{
				continue;
			}

			if (Utils::IO::WriteFile(newPath, Utils::IO::ReadFile(oldPath)))
			{
				Utils::IO::RemoveFile(oldPath);
			}
		}
	}

	void Stats::SendStats()
	{
		if (Game::CL_GetLocalClientConnectionState(0) < Game::CA_LOADING)
		{
			return;
		}

		const auto qport = Utils::Hook::Get<int>(clc_qport);
		const auto* const serverAddress = Game::clc_serverAddress;

		for (unsigned char i = 0; i < 7; ++i)
		{
			Logger::Print("Sending stat packet {} to server.\n", i);

			Game::msg_t msg{};
			unsigned char buffer[2048]{};

			Game::MSG_Init(&msg, buffer, sizeof(buffer));
			Game::MSG_WriteString(&msg, "stats");

			const char* statBuffer = nullptr;

			if (Game::LiveStorage_DoWeHaveStats(0))
			{
				statBuffer = &Game::LiveStorage_GetStatBuffer(0)[1240 * i];
			}

			Game::MSG_WriteShort(&msg, qport);
			Game::MSG_WriteByte(&msg, i);

			if (statBuffer)
			{
				Game::MSG_WriteData(&msg, statBuffer, std::min(8192 - (i * 1240), 1240));
			}

			Network::SendRaw(Game::NS_CLIENT1, serverAddress, std::string(reinterpret_cast<char*>(msg.data), msg.cursize));
		}
	}

	void Stats::AddScriptFunctions()
	{
		GSC::Script::AddMethod("GetStat", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
			const auto index = Game::Scr_GetInt(0);

			if (index < 0 || index > 3499)
			{
				GSC::Script::Scr_ParamError(0, Utils::String::VA("GetStat: invalid index %i", index));
			}

			if (ent->client->sess.connected <= Game::CON_DISCONNECTED)
			{
				GSC::Script::Scr_Error("GetStat: called on a disconnected player");
			}

			Game::Scr_AddInt(Game::SV_GetClientStat(ent->s.number, index));
		});

		GSC::Script::AddMethod("SetStat", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			const auto paramCount = Game::Scr_GetNumParam();

			if (paramCount != 2)
			{
				GSC::Script::Scr_Error(Utils::String::VA("GetStat: takes 2 arguments, got %u.", paramCount));
			}

			const auto index = Game::Scr_GetInt(0);

			if (index < 0 || index > 3499)
			{
				GSC::Script::Scr_ParamError(0, Utils::String::VA("setstat: invalid index %i", index));
			}

			const auto value = Game::Scr_GetInt(1);

			if (index < 2000 && (value < 0 || value > 255))
			{
				GSC::Script::Scr_ParamError(1, Utils::String::VA("setstat: index %i is a byte value, and you're trying to set it to %i", index, value));
			}

			Game::SV_SetClientStat(ent->s.number, index, value);
		});
	}

	Stats::Stats()
	{
		UIScript::Add("UpdateClasses", []([[maybe_unused]] const UIScript::Token& token)
		{
			SendStats();
		});

		AddScriptFunctions();

		ServerCommands::OnCommand(Game::setStatCommand, [](const Command::Params* params)
		{
			const auto index = std::atoi(params->Get(1));
			const auto value = std::atoi(params->Get(2));

			reinterpret_cast<void(*)(int, int, int)>(Utils::Hook::Rebase(LiveStorage_SetStat))(0, index, value);
			return true;
		});

		Command::Add("statGet", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				Logger::Print("statget usage: statget <index>\n");
				return;
			}

			const auto index = std::strtol(params->Get(1), nullptr, 0);
			const auto stat = Game::LiveStorage_GetStat(0, index);
			Logger::Print("Stat {}: {}\n", index, stat);
		});

		if (Utils::Hook::MatchesBytes(LiveStorage_SetStat_ConnectedTest, setStatConnectedTest, sizeof(setStatConnectedTest)))
		{
			Utils::Hook::Set<std::uint8_t>(LiveStorage_SetStat_ConnectedTest, 0xEB);
		}
		else
		{
			Logger::Error("stats: LiveStorage_SetStat is not the expected code, setting a stat while connected is still an error\n");
		}

		if (Utils::Hook::MatchesBytes(LiveStorage_DataSetCmd_ConnectedTest, dataSetConnectedTest, sizeof(dataSetConnectedTest)))
		{
			Utils::Hook::Set<std::uint8_t>(LiveStorage_DataSetCmd_ConnectedTest, 0xEB);
		}
		else
		{
			Logger::Error("stats: LiveStorage_DataSetCmd is not the expected code, setting player data while connected is still an error\n");
		}

		const bool isHostStatsExpected = Utils::Hook::MatchesBytes(CG_ServerCommand_StatsIntegrityTest, statsIntegrityTest, sizeof(statsIntegrityTest))
			&& Utils::Hook::MatchesBytes(CG_ServerCommand_LossesTest, lossesTest, sizeof(lossesTest))
			&& Utils::Hook::MatchesBytes(CG_ServerCommand_BadStatsTest, badStatsTest, sizeof(badStatsTest));

		if (isHostStatsExpected)
		{
			Utils::Hook::Set<std::uint8_t>(CG_ServerCommand_StatsIntegrityTest + 1, static_cast<std::uint8_t>(sizeof(statsIntegrityTest) - 2));
			Utils::Hook::Set<std::uint8_t>(CG_ServerCommand_StatsIntegrityTest, 0xEB);

			Utils::Hook::Set<std::uint8_t>(CG_ServerCommand_LossesTest, 0xEB);

			Utils::Hook::Set<std::uint8_t>(CG_ServerCommand_BadStatsTest + 1, 0xE9);
			Utils::Hook::Set<std::uint8_t>(CG_ServerCommand_BadStatsTest, 0x90);
		}
		else
		{
			Logger::Error("stats: the host's stats command is not the expected code, a host's stats change is still checked\n");
		}

		const bool isExpected = Utils::Hook::MatchesBytes(Steam_FileRead_Core_GetSteamIDCall, readGetSteamID, sizeof(readGetSteamID))
			&& Utils::Hook::MatchesBytes(Steam_FileWrite_Core_GetSteamIDCall, writeGetSteamID, sizeof(writeGetSteamID))
			&& Utils::Hook::MatchesBytes(Steam_FileRead_Core_SourceTest, readSourceTest, sizeof(readSourceTest))
			&& Utils::Hook::MatchesBytes(Steam_FileRead_Core_NotFound, readNotFound, sizeof(readNotFound))
			&& Utils::Hook::MatchesBytes(Steam_FileWrite_Core_SourceTest, writeSourceTest, sizeof(writeSourceTest))
			&& Utils::Hook::MatchesBytes(Steam_FileWrite_BakTest, writeBakTest, sizeof(writeBakTest))
			&& Utils::Hook::MatchesBytes(statFileFormat, statFileFormatText, sizeof(statFileFormatText))
			&& Utils::Hook::MatchesBytes(LiveStorage_GenerateFilename_Com_sprintfCall, generateFilenameCall, sizeof(generateFilenameCall));

		if (!isExpected)
		{
			Logger::Error("stats: the steam file layer does not read as expected, stats stay on steam\n");
			return;
		}

		Utils::Hook readIdHook(Steam_FileRead_Core_GetSteamIDCall, reinterpret_cast<void*>(GetStatsID), HOOK_CALL);
		Utils::Hook writeIdHook(Steam_FileWrite_Core_GetSteamIDCall, reinterpret_cast<void*>(GetStatsID), HOOK_CALL);
		Utils::Hook filenameHook(LiveStorage_GenerateFilename_Com_sprintfCall, reinterpret_cast<void*>(SprintfLiveStorageFilename), HOOK_CALL);

		const bool isSeated = readIdHook.Install()->IsInstalled()
			&& writeIdHook.Install()->IsInstalled()
			&& filenameHook.Install()->IsInstalled();

		if (!isSeated)
		{
			Logger::Error("stats: could not seat the file layer hooks, stats stay on steam\n");
			return;
		}

		readIdHook.Quick();
		writeIdHook.Quick();
		filenameHook.Quick();

		Utils::Hook::Nop(Steam_FileRead_Core_GetSteamIDCall + 5, readGetSteamIDLength - 5);
		Utils::Hook::Nop(Steam_FileWrite_Core_GetSteamIDCall + 5, writeGetSteamIDLength - 5);

		const auto notFoundRelative = static_cast<std::int32_t>(Steam_FileRead_Core_NotFound - (Steam_FileRead_Core_CloudBranch + 5));

		Utils::Hook::Set<std::int32_t>(Steam_FileRead_Core_CloudBranch + 1, notFoundRelative);
		Utils::Hook::Set<std::uint8_t>(Steam_FileRead_Core_CloudBranch, 0xE9);
		Utils::Hook::Nop(Steam_FileRead_Core_CloudBranch + 5, 1);

		Utils::Hook::Set<std::uint8_t>(Steam_FileWrite_Core_SourceTest, 0xEB);
		Utils::Hook::Set<std::uint8_t>(Steam_FileWrite_BakTest, 0xEB);

		Utils::Hook::SetString(statFileFormat, "%s");

		isInstalled = true;

		const bool isChecksumExpected = Utils::Hook::MatchesBytes(Steam_FileRead_Core_ChecksumCall, readChecksumCall, sizeof(readChecksumCall))
			&& Utils::Hook::MatchesBytes(Steam_FileWrite_Core_ChecksumCall, writeChecksumCall, sizeof(writeChecksumCall));

		if (!isChecksumExpected)
		{
			Logger::Error("stats: the file cores' checksum calls do not read as expected, stats keep the engine's key\n");
			return;
		}

		const bool isChecksumSeated = readChecksumHook.Initialize(Steam_FileRead_Core_ChecksumCall, reinterpret_cast<void*>(Steam_FileRead_Core_Checksum), HOOK_CALL)->Install()->IsInstalled()
			&& writeChecksumHook.Initialize(Steam_FileWrite_Core_ChecksumCall, reinterpret_cast<void*>(Steam_FileWrite_Core_Checksum), HOOK_CALL)->Install()->IsInstalled();

		if (!isChecksumSeated)
		{
			readChecksumHook.Uninstall();
			writeChecksumHook.Uninstall();

			Logger::Error("stats: could not seat the checksum hooks, stats keep the engine's key\n");
			return;
		}

		readChecksumHook.Quick();
		writeChecksumHook.Quick();

		Scheduler::Once(MoveOldStatsToNewFolder, Scheduler::Pipeline::MAIN);
	}
}
