#include "STDInclude.hpp"

#include "Theatre.hpp"
#include "ClientCommand.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Network.hpp"
#include "Scheduler.hpp"
#include "UIFeeder.hpp"

namespace Components
{
	extern "C"
	{
		void BaselineStoreStub();
		void AdjustTimeDeltaStub();
		void UISetActiveMenuStub();

		std::uintptr_t Theatre_clcDemowaiting = 0;
		std::uintptr_t Theatre_clcDemoplaying = 0;
		std::uintptr_t Theatre_clNewSnapshots = 0;
		std::uintptr_t Theatre_ParseSnapshotNext = 0;
		std::uintptr_t Theatre_AdjustTimeDeltaNext = 0;
		std::uintptr_t Theatre_AdjustTimeDeltaSkip = 0;
		std::uintptr_t Theatre_UISetActiveMenuReturn = 0;

		void Theatre_StoreBaseline(Game::msg_t* snapshotMsg)
		{
			Theatre::StoreBaseline(snapshotMsg);
		}

		bool Theatre_AdjustTimeDelta()
		{
			return Theatre::AdjustTimeDelta();
		}
	}

	Theatre::DemoInfo Theatre::currentInfo;
	unsigned int Theatre::currentSelection;
	std::vector<Theatre::DemoInfo> Theatre::demos;

	Dvar::Var Theatre::cl_autoRecord;
	Dvar::Var Theatre::cl_demosKeep;

	char Theatre::baselineSnapshot[131072] = { 0 };
	int Theatre::baselineSnapshotMsgLen;
	int Theatre::baselineSnapshotMsgOff;

	constexpr std::uintptr_t clc_serverMessageSequence = 0x140C1B8DC;
	constexpr std::uintptr_t clc_serverCommandSequence = 0x140C1B8E0;
	constexpr std::uintptr_t clc_lastExecutedServerCommand = 0x140C1B8E4;
	constexpr std::uintptr_t clc_serverCommands = 0x140C1B8E8;
	constexpr std::uintptr_t clc_demoArchiveIndex = 0x140C3B8EC;
	constexpr std::uintptr_t clc_demorecording = 0x140C3B930;
	constexpr std::uintptr_t clc_demoplaying = 0x140C3B934;
	constexpr std::uintptr_t clc_demowaiting = 0x140C3B93C;
	constexpr std::uintptr_t clc_demofile = 0x140C3B948;

	constexpr int maxReliableCommands = 128;
	constexpr int maxStringChars = 1024;

	constexpr std::uintptr_t cgs_serverCommandSequence = 0x140587524;

	constexpr std::uintptr_t cg_landTime = 0x1404E1138;
	constexpr std::uintptr_t cg_weaponSelect = 0x1404EC2BC;
	constexpr std::uintptr_t cg_weaponSelectTime = 0x1404EC2C0;

	constexpr std::uintptr_t cl_snap_serverTime = 0x1406D1F6C;
	constexpr std::uintptr_t cl_serverTime = 0x1406D1F98;
	constexpr std::uintptr_t cl_serverTimeDelta = 0x1406D1FA4;
	constexpr std::uintptr_t cl_newSnapshots = 0x1406D1FB0;
	constexpr std::uintptr_t cls_realtime = 0x140C5CEBC;

	constexpr int svc_snapshot = 0;
	constexpr int svc_EOF = 5;

	constexpr char mapRestartCommand = 'x';

	constexpr auto demoExtension = "dm_17";

	constexpr std::uintptr_t CG_ReadNextSnapshot_GetSnapshotCalls[] = { 0x1400E95F9, 0x1400E96BB };
	constexpr std::uintptr_t CL_GetSnapshot_Engine = 0x1400F4840;

	constexpr std::uintptr_t CL_ReadDemoNetworkPacket_ParseCall = 0x1400FCDB1;
	constexpr std::uintptr_t CL_ParseServerMessage_Engine = 0x140100CA0;

	constexpr std::uintptr_t CL_Record_f_OpenCall = 0x1400FCF16;
	constexpr std::uintptr_t FS_FOpenFileWrite_Engine = 0x140276A10;
	constexpr std::uintptr_t CL_Record_f_GamestateOpCall = 0x1400FCFF4;
	constexpr std::uintptr_t MSG_WriteByte_Engine = 0x140202A40;
	constexpr std::uintptr_t CL_Record_f_SequenceWriteCall = 0x1400FD248;
	constexpr std::uintptr_t FS_WriteToDemo_Engine = 0x140278E50;
	constexpr std::uintptr_t CL_Record_f_ArchiveIndexStore = 0x1400FD2CE;
	static const std::uint8_t archiveIndexStore[] = { 0xC7, 0x05, 0x14, 0xE6, 0xB3, 0x00, 0x00, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t CL_StopRecord_f_CloseCall = 0x1400FDA6F;
	constexpr std::uintptr_t FS_FCloseFile_Engine = 0x140275920;

	constexpr std::uintptr_t CL_ServerTimedOut_HostCall = 0x1400FD529;
	constexpr std::uintptr_t CL_ServerTimedOut_Host = 0x1402A2FA0;

	constexpr std::uintptr_t CL_FirstSnapshot_LoadedFlagsCall = 0x1400F4782;
	constexpr std::uintptr_t DB_GetLoadedFlags_Engine = 0x14012DD20;

	constexpr std::uintptr_t CL_ParseSnapshot_BaselineBranch = 0x140100F9F;
	static const std::uint8_t baselineBranch[] = { 0x48, 0x8B, 0xF3, 0x89, 0x1D, 0x94, 0xA9, 0xB3, 0x00, 0xEB, 0x6C };
	constexpr std::uintptr_t CL_ParseSnapshot_BaselineNext = 0x140101016;
	constexpr std::uintptr_t CL_ParseSnapshot_ArchiveCall = 0x14010132E;
	constexpr std::uintptr_t CL_WriteDemoClientArchive_Engine = 0x1400FE000;

	constexpr std::uintptr_t CG_OwnerDraw_LocationSelectorCall = 0x1400DF2A0;
	constexpr std::uintptr_t CG_CompassDrawPlayerMapLocationSelector = 0x1400A4820;

	constexpr std::uintptr_t UI_SetActiveMenu_MainCase = 0x140272966;
	static const std::uint8_t mainCase[] = { 0xBA, 0x10, 0x00, 0x00, 0x00 };
	constexpr std::uintptr_t UI_SetActiveMenu_Return = 0x140272951;

	constexpr std::uintptr_t CL_SetCGameTime_AdjustTimeDelta = 0x1400F51C1;
	static const std::uint8_t adjustTimeDelta[] = { 0x45, 0x8B, 0xD9, 0x44, 0x89, 0x3D, 0xE5, 0xCD, 0x5D, 0x00 };
	constexpr std::uintptr_t CL_SetCGameTime_AdjustTimeDeltaNext = 0x1400F51CB;
	constexpr std::uintptr_t CL_SetCGameTime_AdjustTimeDeltaEnd = 0x1400F52F6;

	constexpr std::uintptr_t SV_SpawnServer_SyncCall = 0x14023B24A;
	constexpr std::uintptr_t Com_SyncThreads_Engine = 0x1401F6B30;

	constexpr std::uintptr_t SCR_DrawScreenField_HorzAlign = 0x140101ABA;
	constexpr std::uintptr_t SCR_DrawScreenField_VertAlign = 0x140101AAA;
	static const std::uint8_t horzAlign[] = { 0xC7, 0x44, 0x24, 0x28, 0x01, 0x00, 0x00, 0x00 };
	static const std::uint8_t vertAlign[] = { 0xC7, 0x44, 0x24, 0x30, 0x01, 0x00, 0x00, 0x00 };

	static Utils::Hook hooks[16];

	template <typename T>
	static T& EngineGlobal(std::uintptr_t address)
	{
		return *reinterpret_cast<T*>(Utils::Hook::Rebase(address));
	}

	static bool IsDemoPlaying()
	{
		return EngineGlobal<int>(clc_demoplaying) != 0;
	}

	nlohmann::json Theatre::DemoInfo::to_json() const
	{
		return nlohmann::json
		{
			{ "mapname", mapname },
			{ "gametype", gametype },
			{ "author", author },
			{ "length", length },
			{ "timestamp", std::to_string(timeStamp) },
		};
	}

	int Theatre::CL_GetSnapshot_Hk(int localClientNum, int snapshotNumber, Game::snapshot_s* snapshot)
	{
		const auto getSnapshot = reinterpret_cast<int(*)(int, int, Game::snapshot_s*)>(hooks[0].GetOriginal());
		const auto result = getSnapshot(localClientNum, snapshotNumber, snapshot);

		if (IsDemoPlaying())
		{
			auto& weaponSelect = EngineGlobal<int>(cg_weaponSelect);

			if (static_cast<unsigned int>(weaponSelect) != snapshot->ps.weapCommon.weapon)
			{
				weaponSelect = snapshot->ps.weapCommon.weapon;

				EngineGlobal<int>(cg_weaponSelectTime) = snapshot->serverTime;
			}
		}

		return result;
	}

	void Theatre::CL_ParseServerMessage_Hk(int localClientNum, Game::msg_t* msg)
	{
		Game::CL_ParseServerMessage(localClientNum, msg);

		assert(IsDemoPlaying());

		const auto serverCommandSequence = EngineGlobal<int>(clc_serverCommandSequence);
		const auto lastExecutedServerCommand = EngineGlobal<int>(clc_lastExecutedServerCommand);

		if (lastExecutedServerCommand + maxReliableCommands / 2 > serverCommandSequence)
		{
			return;
		}

		if (lastExecutedServerCommand == 0)
		{
			EngineGlobal<int>(clc_lastExecutedServerCommand) = serverCommandSequence;
			EngineGlobal<int>(cgs_serverCommandSequence) = serverCommandSequence;

			EngineGlobal<int>(cg_landTime) = 0;

			return;
		}

		auto* const serverCommands = reinterpret_cast<char*>(Utils::Hook::Rebase(clc_serverCommands));

		for (auto i = lastExecutedServerCommand + 1; i <= serverCommandSequence; ++i)
		{
			auto* const command = &serverCommands[(i & (maxReliableCommands - 1)) * maxStringChars];

			if (command[0] == mapRestartCommand)
			{
				command[0] = '\0';
			}
		}

		Game::CG_ExecuteNewServerCommands(0, serverCommandSequence);
	}

	void Theatre::GamestateWrite_Hk(Game::msg_t* msg, int byte)
	{
		Game::MSG_WriteLong(msg, 0);
		Game::MSG_WriteByte(msg, byte);
	}

	void Theatre::RecordGamestate_Hk([[maybe_unused]] const void* buffer, [[maybe_unused]] int length, Game::fileHandle_t file)
	{
		const auto sequence = EngineGlobal<int>(clc_serverMessageSequence) - 1;
		Game::FS_WriteToDemo(&sequence, 4, file);
	}

	void Theatre::StoreBaseline(Game::msg_t* snapshotMsg)
	{
		if (IsDemoPlaying())
		{
			return;
		}

		baselineSnapshotMsgLen = snapshotMsg->cursize;
		baselineSnapshotMsgOff = snapshotMsg->readcount - 2;

		std::memcpy(baselineSnapshot, snapshotMsg->data, snapshotMsg->cursize);
	}

	void Theatre::WriteBaseline()
	{
		static unsigned char bufData[131072];
		static unsigned char cmpData[131072];

		Game::msg_t buf{};

		Game::MSG_Init(&buf, bufData, sizeof(bufData));
		Game::MSG_WriteByte(&buf, svc_snapshot);
		Game::MSG_WriteLong(&buf, EngineGlobal<int>(cl_snap_serverTime));
		Game::MSG_WriteData(&buf, &baselineSnapshot[baselineSnapshotMsgOff], baselineSnapshotMsgLen - baselineSnapshotMsgOff);
		Game::MSG_WriteByte(&buf, svc_EOF);

		const auto compressedSize = Utils::Huffman::Compress(buf.data, cmpData, buf.cursize, sizeof(cmpData));
		const auto fileCompressedSize = compressedSize + 4;

		const int byte8 = 8;
		const unsigned char byte0 = 0;

		const auto file = EngineGlobal<Game::fileHandle_t>(clc_demofile);

		Game::FS_WriteToDemo(&byte0, sizeof(unsigned char), file);
		Game::FS_WriteToDemo(&EngineGlobal<int>(clc_serverMessageSequence), sizeof(int), file);
		Game::FS_WriteToDemo(&fileCompressedSize, sizeof(int), file);
		Game::FS_WriteToDemo(&byte8, sizeof(int), file);

		for (auto i = 0; i < compressedSize; i += 1024)
		{
			const auto size = std::min(compressedSize - i, 1024);

			if (i + size >= static_cast<int>(sizeof(cmpData)))
			{
				Logger::Error("Writing compressed demo baseline exceeded buffer\n");
				break;
			}

			Game::FS_WriteToDemo(&cmpData[i], size, file);
		}
	}

	void Theatre::BaselineToFile_Hk()
	{
		WriteBaseline();

		EngineGlobal<int>(clc_demoArchiveIndex) = 0;
	}

	bool Theatre::AdjustTimeDelta()
	{
		if (!IsDemoPlaying())
		{
			return false;
		}

		const auto serverTime = EngineGlobal<int>(cl_serverTime);
		const auto snapServerTime = EngineGlobal<int>(cl_snap_serverTime);

		assert(serverTime > 0);
		assert(snapServerTime > 0);

		if (serverTime + 1000 < snapServerTime)
		{
			EngineGlobal<int>(cl_serverTimeDelta) = snapServerTime - EngineGlobal<int>(cls_realtime);
		}

		return true;
	}

	bool Theatre::CL_ServerTimedOut_Hk()
	{
		if (IsDemoPlaying())
		{
			return true;
		}

		return reinterpret_cast<bool(*)()>(Utils::Hook::Rebase(CL_ServerTimedOut_Host))();
	}

	void Theatre::CG_CompassDrawPlayerMapLocationSelector_Hk(int localClientNum, Game::CompassType compassType, const Game::rectDef_s* parentRect, const Game::rectDef_s* rect, Game::Material* material, float* color)
	{
		if (IsDemoPlaying())
		{
			return;
		}

		using Selector = void(*)(int, Game::CompassType, const Game::rectDef_s*, const Game::rectDef_s*, Game::Material*, float*);
		reinterpret_cast<Selector>(Utils::Hook::Rebase(CG_CompassDrawPlayerMapLocationSelector))(localClientNum, compassType, parentRect, rect, material, color);
	}

	void Theatre::CL_WriteDemoClientArchive_Hk(void(*write)(const void* buffer, int len, int localClientNum), const Game::playerState_s* ps, const float* viewangles, [[maybe_unused]] const float* selectedLocation, [[maybe_unused]] float selectedLocationAngle, int localClientNum, int index)
	{
		assert(write);
		assert(ps);

		const unsigned char msgType = 1;
		write(&msgType, sizeof(unsigned char), localClientNum);

		write(&index, sizeof(int), localClientNum);

		write(ps->origin, sizeof(float[3]), localClientNum);
		write(ps->velocity, sizeof(float[3]), localClientNum);
		write(&ps->movementDir, sizeof(int), localClientNum);
		write(&ps->bobCycle, sizeof(int), localClientNum);

		write(&ps->commandTime, sizeof(int), localClientNum);
		write(viewangles, sizeof(float[3]), localClientNum);

		const auto locationSelectionInfo = 0;
		write(&locationSelectionInfo, sizeof(int), localClientNum);
	}

	Game::fileHandle_t Theatre::RecordStub(const char* file)
	{
		currentInfo.name = file;
		currentInfo.mapname = Dvar::Var("mapname").Get<std::string>();
		currentInfo.gametype = Dvar::Var("g_gametype").Get<std::string>();

		currentInfo.author = Dvar::Var("name").Get<std::string>();
		currentInfo.length = Game::Sys_Milliseconds();
		std::time(&currentInfo.timeStamp);

		return Game::FS_FOpenFileWrite(file);
	}

	void Theatre::StopRecordStub(Game::fileHandle_t file)
	{
		Game::FS_FCloseFile(file);

		currentInfo.length = Game::Sys_Milliseconds() - currentInfo.length;

		const FileSystem::FileWriter meta(std::format("{}.json", currentInfo.name));
		meta.Write(nlohmann::json(currentInfo.to_json()).dump());
	}

	void Theatre::LoadDemos([[maybe_unused]] const UIScript::Token& token)
	{
		currentSelection = 0;
		demos.clear();

		const auto files = FileSystem::GetFileList("demos/", demoExtension);

		for (const auto& demo : files)
		{
			FileSystem::File meta(std::format("demos/{}.json", demo));

			if (!meta.Exists())
			{
				continue;
			}

			try
			{
				const auto metaObject = nlohmann::json::parse(meta.GetBuffer());

				DemoInfo demoInfo;
				demoInfo.name = demo.substr(0, demo.find_last_of('.'));
				demoInfo.author = metaObject["author"].get<std::string>();
				demoInfo.gametype = metaObject["gametype"].get<std::string>();
				demoInfo.mapname = metaObject["mapname"].get<std::string>();
				demoInfo.length = metaObject["length"].get<int>();
				const auto timestamp = metaObject["timestamp"].get<std::string>();
				demoInfo.timeStamp = std::strtoll(timestamp.data(), nullptr, 10);

				demos.push_back(demoInfo);
			}
			catch (const nlohmann::json::exception& ex)
			{
				Logger::Error("JSON Parse Error: {}\n", ex.what());
			}
		}

		std::ranges::reverse(demos);
	}

	void Theatre::DeleteDemo(const UIScript::Token& token)
	{
		if (currentSelection >= demos.size())
		{
			return;
		}

		const auto demoInfo = demos.at(currentSelection);

		Logger::Print("Deleting demo {}...\n", demoInfo.name);

		Game::FS_Delete(std::format("demos/{}.{}", demoInfo.name, demoExtension).data());
		Game::FS_Delete(std::format("demos/{}.{}.json", demoInfo.name, demoExtension).data());

		Game::Dvar_SetFromStringByName("ui_demo_mapname", "");
		Game::Dvar_SetFromStringByName("ui_demo_mapname_localized", "");
		Game::Dvar_SetFromStringByName("ui_demo_gametype", "");
		Game::Dvar_SetFromStringByName("ui_demo_length", "");
		Game::Dvar_SetFromStringByName("ui_demo_author", "");
		Game::Dvar_SetFromStringByName("ui_demo_date", "");

		LoadDemos(token);
	}

	void Theatre::PlayDemo([[maybe_unused]] const UIScript::Token& token)
	{
		if (currentSelection >= demos.size())
		{
			return;
		}

		Command::Execute(std::format("demo {}", demos[currentSelection].name), true);
		Command::Execute("demoback", false);
	}

	unsigned int Theatre::GetDemoCount()
	{
		return static_cast<unsigned int>(demos.size());
	}

	const char* Theatre::GetDemoText(unsigned int item, [[maybe_unused]] int column)
	{
		if (item >= demos.size())
		{
			return "";
		}

		const auto& info = demos.at(item);
		return Utils::String::VA("%s on %s", Game::UI_GetGameTypeDisplayName(info.gametype.data()), Localization::LocalizeMapName(info.mapname.data()));
	}

	void Theatre::SelectDemo(unsigned int index)
	{
		if (index >= demos.size())
		{
			return;
		}

		currentSelection = index;
		const auto& info = demos.at(index);

		tm time{};
		char buffer[1000] = { 0 };
		localtime_s(&time, &info.timeStamp);
		asctime_s(buffer, sizeof(buffer), &time);

		Game::Dvar_SetFromStringByName("ui_demo_mapname", info.mapname.data());
		Game::Dvar_SetFromStringByName("ui_demo_mapname_localized", Localization::LocalizeMapName(info.mapname.data()));
		Game::Dvar_SetFromStringByName("ui_demo_gametype", Game::UI_GetGameTypeDisplayName(info.gametype.data()));
		Game::Dvar_SetFromStringByName("ui_demo_length", Utils::String::FormatTimeSpan(info.length).data());
		Game::Dvar_SetFromStringByName("ui_demo_author", info.author.data());
		Game::Dvar_SetFromStringByName("ui_demo_date", buffer);
	}

	int Theatre::CL_FirstSnapshot_Hk()
	{
		if (IsDemoPlaying())
		{
			auto* const sv_cheats = ClientCommand::sv_cheats.Get();

			if (sv_cheats && !sv_cheats->current.enabled)
			{
				sv_cheats->current.enabled = true;
				sv_cheats->modified = true;
			}
		}
		else if (cl_autoRecord.Get<bool>())
		{
			std::vector<std::string> files;
			const auto existing = FileSystem::GetFileList("demos/", demoExtension);

			for (const auto& demo : existing)
			{
				if (Utils::String::StartsWith(demo, "auto_"))
				{
					files.push_back(demo);
				}
			}

			const auto numDel = static_cast<int>(files.size()) - cl_demosKeep.Get<int>();

			for (auto i = 0; i < numDel; ++i)
			{
				Logger::Print("Deleting old demo {}\n", files[i]);
				Game::FS_Delete(std::format("demos/{}", files[i]).data());
				Game::FS_Delete(std::format("demos/{}.json", files[i]).data());
			}

			Scheduler::Schedule([syncAttempts = 50]() mutable
			{
				const auto serverCommandSequence = EngineGlobal<int>(clc_serverCommandSequence);
				const auto lastExecutedServerCommand = EngineGlobal<int>(clc_lastExecutedServerCommand);

				if (lastExecutedServerCommand == serverCommandSequence || --syncAttempts < 0)
				{
					const auto timestamp = static_cast<long long>(std::time(nullptr));
					Command::Execute(Utils::String::VA("record auto_%lld", timestamp), true);
					return true;
				}

				return false;
			}, Scheduler::Pipeline::MAIN);
		}

		return reinterpret_cast<int(*)()>(Utils::Hook::Rebase(DB_GetLoadedFlags_Engine))();
	}

	void Theatre::SV_SpawnServer_Hk()
	{
		StopRecording();
		Game::Com_SyncThreads();
	}

	void Theatre::StopRecording()
	{
		if (EngineGlobal<int>(clc_demorecording))
		{
			Command::Execute("stoprecord", true);
		}
	}

	Theatre::Theatre()
	{
		struct CallSite
		{
			std::uintptr_t address;
			std::uintptr_t target;
			void* replacement;
		};

		const CallSite calls[] =
		{
			{ CG_ReadNextSnapshot_GetSnapshotCalls[0], CL_GetSnapshot_Engine, reinterpret_cast<void*>(CL_GetSnapshot_Hk) },
			{ CG_ReadNextSnapshot_GetSnapshotCalls[1], CL_GetSnapshot_Engine, reinterpret_cast<void*>(CL_GetSnapshot_Hk) },
			{ CL_ReadDemoNetworkPacket_ParseCall, CL_ParseServerMessage_Engine, reinterpret_cast<void*>(CL_ParseServerMessage_Hk) },
			{ CL_Record_f_OpenCall, FS_FOpenFileWrite_Engine, reinterpret_cast<void*>(RecordStub) },
			{ CL_Record_f_GamestateOpCall, MSG_WriteByte_Engine, reinterpret_cast<void*>(GamestateWrite_Hk) },
			{ CL_Record_f_SequenceWriteCall, FS_WriteToDemo_Engine, reinterpret_cast<void*>(RecordGamestate_Hk) },
			{ CL_StopRecord_f_CloseCall, FS_FCloseFile_Engine, reinterpret_cast<void*>(StopRecordStub) },
			{ CL_ServerTimedOut_HostCall, CL_ServerTimedOut_Host, reinterpret_cast<void*>(CL_ServerTimedOut_Hk) },
			{ CL_FirstSnapshot_LoadedFlagsCall, DB_GetLoadedFlags_Engine, reinterpret_cast<void*>(CL_FirstSnapshot_Hk) },
			{ CL_ParseSnapshot_ArchiveCall, CL_WriteDemoClientArchive_Engine, reinterpret_cast<void*>(CL_WriteDemoClientArchive_Hk) },
			{ CG_OwnerDraw_LocationSelectorCall, CG_CompassDrawPlayerMapLocationSelector, reinterpret_cast<void*>(CG_CompassDrawPlayerMapLocationSelector_Hk) },
			{ SV_SpawnServer_SyncCall, Com_SyncThreads_Engine, reinterpret_cast<void*>(SV_SpawnServer_Hk) },
		};

		bool isExpected = Utils::Hook::MatchesBytes(CL_Record_f_ArchiveIndexStore, archiveIndexStore, sizeof(archiveIndexStore))
			&& Utils::Hook::MatchesBytes(CL_ParseSnapshot_BaselineBranch, baselineBranch, sizeof(baselineBranch))
			&& Utils::Hook::MatchesBytes(UI_SetActiveMenu_MainCase, mainCase, sizeof(mainCase))
			&& Utils::Hook::MatchesBytes(CL_SetCGameTime_AdjustTimeDelta, adjustTimeDelta, sizeof(adjustTimeDelta))
			&& Utils::Hook::MatchesBytes(SCR_DrawScreenField_HorzAlign, horzAlign, sizeof(horzAlign))
			&& Utils::Hook::MatchesBytes(SCR_DrawScreenField_VertAlign, vertAlign, sizeof(vertAlign));

		for (const auto& call : calls)
		{
			const bool isCallExpected = Utils::Hook::BranchesTo(call.address, call.target, false) || Network::HasSnapshotHook(call.address);
			isExpected = isExpected && isCallExpected;
		}

		if (!isExpected)
		{
			Logger::Error("theatre: the demo code does not read as expected, no theater\n");
			return;
		}

		Theatre_clcDemowaiting = Utils::Hook::Rebase(clc_demowaiting);
		Theatre_clcDemoplaying = Utils::Hook::Rebase(clc_demoplaying);
		Theatre_clNewSnapshots = Utils::Hook::Rebase(cl_newSnapshots);
		Theatre_ParseSnapshotNext = Utils::Hook::Rebase(CL_ParseSnapshot_BaselineNext);
		Theatre_AdjustTimeDeltaNext = Utils::Hook::Rebase(CL_SetCGameTime_AdjustTimeDeltaNext);
		Theatre_AdjustTimeDeltaSkip = Utils::Hook::Rebase(CL_SetCGameTime_AdjustTimeDeltaEnd);
		Theatre_UISetActiveMenuReturn = Utils::Hook::Rebase(UI_SetActiveMenu_Return);

		bool isSeated = true;
		std::size_t hookCount = 0;

		for (const auto& call : calls)
		{
			isSeated = hooks[hookCount++].Initialize(call.address, call.replacement, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = hooks[hookCount++].Initialize(CL_Record_f_ArchiveIndexStore, reinterpret_cast<void*>(BaselineToFile_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[hookCount++].Initialize(CL_ParseSnapshot_BaselineBranch, BaselineStoreStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[hookCount++].Initialize(CL_SetCGameTime_AdjustTimeDelta, AdjustTimeDeltaStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = hooks[hookCount++].Initialize(UI_SetActiveMenu_MainCase, UISetActiveMenuStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

		assert(hookCount == std::size(hooks));

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("theatre: could not seat every hook, no theater\n");
			return;
		}

		Utils::Hook::Nop(CL_Record_f_ArchiveIndexStore + 5, sizeof(archiveIndexStore) - 5);
		Utils::Hook::Nop(CL_ParseSnapshot_BaselineBranch + 5, sizeof(baselineBranch) - 5);
		Utils::Hook::Nop(CL_SetCGameTime_AdjustTimeDelta + 5, sizeof(adjustTimeDelta) - 5);

		Utils::Hook::Set<std::uint8_t>(SCR_DrawScreenField_HorzAlign + 4, 2);
		Utils::Hook::Set<std::uint8_t>(SCR_DrawScreenField_VertAlign + 4, 2);

		Events::OnDvarInit([]
		{
			cl_autoRecord = Dvar::Register("cl_autoRecord", true, Game::DVAR_ARCHIVE, "Automatically record games");
			cl_demosKeep = Dvar::Register("cl_demosKeep", 100, 1, 999, Game::DVAR_ARCHIVE, "How many demos to keep with autorecord");
		});

		UIScript::Add("loadDemos", LoadDemos);
		UIScript::Add("launchDemo", PlayDemo);
		UIScript::Add("deleteDemo", DeleteDemo);

		UIFeeder::Add(10.0f, GetDemoCount, GetDemoText, SelectDemo);
	}
}
