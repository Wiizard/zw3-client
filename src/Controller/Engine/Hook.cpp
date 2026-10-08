#include "STDInclude.hpp"

#include "Controller/Engine/Hook.hpp"
#include "Controller/Engine/Bind.hpp"
#include "Controller/Engine/Dvar.hpp"
#include "Controller/Engine/Engine.hpp"
#include "Controller/Engine/NetMove.hpp"
#include "Controller/Mapping/Glyph.hpp"
#include "Controller/Mapping/Key.hpp"
#include "Controller/Runtime.hpp"

#include "Components/Modules/Command.hpp"
#include "Components/Modules/Console.hpp"
#include "Components/Modules/Logger.hpp"
#include "Components/Modules/RawMouse.hpp"

namespace Controller::Engine
{
	static Runtime* installedRuntime = nullptr;

	static bool IsDriving() noexcept
	{
		return installedRuntime != nullptr && installedRuntime->IsDriving();
	}

	static bool ShouldUse(const Game::gentity_s* player, unsigned int heldMs) noexcept
	{
		if ((player->client->buttons & Game::CMD_BUTTON_USE_RELOAD) == 0)
		{
			return true;
		}

		const int holdMs = Read(RegisteredDvars().useHoldTime, 250);

		return holdMs <= 0 || heldMs >= static_cast<unsigned int>(holdMs);
	}

	static Game::keyname_t* combinedLocalizedKeyNames = nullptr;
	static std::optional<Mapping::GlyphFamily> appliedGlyphs;

	static void ApplyGlyphStyle()
	{
		if (combinedLocalizedKeyNames == nullptr || installedRuntime == nullptr)
		{
			return;
		}

		const int style = Read(RegisteredDvars().style, 0);

		std::optional<Mapping::GlyphFamily> chosen;

		if (style == 1)
		{
			chosen = Mapping::GlyphFamily::PlayStation;
		}
		else if (style == 2)
		{
			chosen = Mapping::GlyphFamily::Xbox;
		}

		Family device = Family::Unknown;

		if (installedRuntime->Active() != noDevice)
		{
			device = installedRuntime->Latest().family;
		}

		const Mapping::GlyphFamily family = Mapping::GlyphFamilyFor(device, chosen);

		if (appliedGlyphs == family)
		{
			return;
		}

		std::size_t index = Game::LOCALIZED_KEY_NAME_COUNT;

		for (const auto key : Mapping::Keys())
		{
			const char* glyph = Mapping::GlyphFor(key, family);

			if (glyph == nullptr)
			{
				glyph = Mapping::KeyName(key);
			}

			combinedLocalizedKeyNames[index].name = glyph;
			++index;
		}

		appliedGlyphs = family;
	}

	extern "C"
	{
		void MSG_WriteDeltaUsercmdKeyStub();
		void MSG_ReadDeltaUsercmdKeyStub();
		void MSG_ReadDeltaUsercmdKeyStub2();
		void INFrameMouseMoveStub();

		std::uintptr_t MSG_WriteDeltaUsercmdKey_Resume = 0;
		std::uintptr_t MSG_ReadDeltaUsercmdKey_Resume = 0;
		std::uintptr_t MSG_ReadDeltaUsercmdKey_Resume2 = 0;
		std::uintptr_t Gamepad_INFrameReturn = 0;

		void Gamepad_ApplyMovement(Game::msg_t* msg, int key, Game::usercmd_s* from, Game::usercmd_s* to)
		{
			MoveDelta delta{ from->forwardmove, from->rightmove };

			if (Game::MSG_ReadBit(msg))
			{
				delta = UnpackMove(static_cast<std::uint16_t>(Game::MSG_ReadBits(msg, 16)), key);
			}

			to->forwardmove = delta.forward;
			to->rightmove = delta.right;
		}

		void Gamepad_IN_Frame()
		{
			Components::RawMouse::IN_MouseMove();

			if (installedRuntime == nullptr)
			{
				return;
			}

			ApplyGlyphStyle();

			if (!Game::s_wmv->mouseActive)
			{
				return;
			}

			try
			{
				installedRuntime->Frame();
			}
			catch (const std::exception& exception)
			{
				Components::Logger::Error("controller: input frame failed: {}\n", exception.what());
			}
		}
	}

	static constexpr std::uintptr_t MSG_WriteDeltaUsercmdKey_MoveFlags = 0x140206042;
	static constexpr std::uintptr_t MSG_WriteDeltaUsercmdKey_MoveResume = 0x1402060B1;
	static constexpr std::uintptr_t MSG_WriteDeltaUsercmdKey_PartialBits = 0x1402061AC;
	static constexpr std::uintptr_t MSG_WriteDeltaUsercmdKey_FullBits = 0x140206262;
	static constexpr std::uintptr_t MSG_ReadDeltaUsercmdKey_PartialMove = 0x14020556A;
	static constexpr std::uintptr_t MSG_ReadDeltaUsercmdKey_PartialResume = 0x1402055ED;
	static constexpr std::uintptr_t MSG_ReadDeltaUsercmdKey_FullMove = 0x14020566E;
	static constexpr std::uintptr_t MSG_ReadDeltaUsercmdKey_FullResume = 0x1402056F1;

	static const std::uint8_t writeMoveFlags[] = { 0x0F, 0xBE, 0x4E, 0x1A, 0x45, 0x33, 0xE4, 0x0F, 0xBE, 0x46, 0x1B, 0x4C, 0x89, 0x7C, 0x24, 0x50, 0x41, 0x8D, 0x5C, 0x24, 0x02 };
	static const std::uint8_t writeMoveResume[] = { 0x8B, 0x4D, 0x04 };
	static const std::uint8_t writePartialBits[] = { 0xC7, 0x44, 0x24, 0x20, 0x04, 0x00, 0x00, 0x00 };
	static const std::uint8_t writeFullBits[] = { 0x41, 0xB8, 0x04, 0x00, 0x00, 0x00 };
	static const std::uint8_t readMove[] = { 0x41, 0x0F, 0xBE, 0x56, 0x1A, 0x41, 0x0F, 0xBE, 0x4E, 0x1B, 0x83, 0xFA, 0x0A, 0x7F, 0x0D, 0x33 };
	static const std::uint8_t readPartialResume[] = { 0xBA, 0x03, 0x00, 0x00, 0x00 };
	static const std::uint8_t readFullResume[] = { 0x33, 0x2F, 0x48, 0x8B, 0xCE };

	static constexpr std::uintptr_t CL_InitOnceForAllClients_CG_RegisterDvarsCall = 0x1400FBC3F;
	static constexpr std::uintptr_t CG_RegisterDvars = 0x1400D7540;

	static constexpr std::uintptr_t Player_UpdateActivate_Player_UseEntityCall = 0x14018BE7A;
	static constexpr std::uintptr_t Player_UseEntity = 0x14018C420;
	static constexpr std::uintptr_t level_time = 0x1418673E8;

	static constexpr std::uintptr_t Item_Bind_HandleKey_Key_SetBindingCalls[] = { 0x14025ED06, 0x14025ED14, 0x14025ED31 };
	static constexpr std::uintptr_t Key_SetBinding = 0x1400EF980;

	static constexpr std::uintptr_t UI_RefreshViewport_Dvar_GetBoolCall = 0x140270DE6;
	static constexpr std::uintptr_t Dvar_GetBool = 0x1402851A0;

	static constexpr std::uintptr_t Key_GetCommandAssignment_Jump = 0x1400EF683;
	static constexpr std::uintptr_t Key_GetCommandAssignmentInternal = 0x1400EF690;

	static constexpr std::uintptr_t GetKeyBindingLocalizedString_CL_GetKeyBindingCall = 0x14025EA7E;
	static constexpr std::uintptr_t CL_GetKeyBinding = 0x1400EE690;
	static constexpr std::uintptr_t Key_IsCommandBoundCall = 0x1400DD712;
	static constexpr std::uintptr_t Key_IsCommandBound = 0x1400EF750;
	static constexpr std::uintptr_t Key_KeynumToStringBuf = 0x140101F90;

	static constexpr auto CRITSECT_KEY_BINDINGS = static_cast<Game::CriticalSection>(13);
	static constexpr int keyNameSize = 128;

	static constexpr std::uintptr_t Key_Bind_f_Key_GetBindingForCmdCall = 0x1400EF4C7;
	static constexpr std::uintptr_t Key_GetBindingForCmd = 0x1400EF620;

	static constexpr std::uintptr_t IN_Frame_MouseMove = 0x1402A2C28;
	static constexpr std::uintptr_t IN_Frame_Return = 0x1402A2D71;
	static const std::uint8_t mouseMoveStart[] = { 0xFF, 0x15, 0xCA, 0xFA, 0x0B, 0x00 };

	static constexpr std::uintptr_t keynames = 0x140420090;
	static constexpr std::uintptr_t keynames_localized = 0x140420690;

	static const Utils::Hook::LeaSite keyNameLeas[] =
	{
		{ 0x1400EF8AE, Utils::Hook::leaRcx, keynames },
		{ 0x1400EFB85, { 0x48, 0x8D, 0x1D }, keynames },
		{ 0x1400EFD21, { 0x4C, 0x8D, 0x25 }, keynames },
	};

	static const Utils::Hook::LeaSite localizedKeyNameLea = { 0x1400EF8B5, { 0x48, 0x8D, 0x05 }, keynames_localized };

	static constexpr std::uintptr_t CL_CreateCmd_CL_RemoteControlMoveCall = 0x1400F7B95;
	static constexpr std::uintptr_t CL_RemoteControlMove = 0x1400F73A0;
	static constexpr std::uintptr_t CL_CreateCmd_CG_HandleLocationSelectionInputCall = 0x1400F7BB2;
	static constexpr std::uintptr_t CG_HandleLocationSelectionInput = 0x1400F7D00;
	static constexpr std::uintptr_t CL_CreateCmd_CL_MouseMoveCall = 0x1400F7BE4;
	static constexpr std::uintptr_t CL_MouseMove = 0x1400F70F0;

	static Utils::Hook moveHooks[3];
	static Utils::Hook registerDvarsHook;
	static Utils::Hook useEntityHook;

	static Utils::Hook inFrameHook;
	static Utils::Hook cursorHook;
	static Utils::Hook assignmentHook;
	static Utils::Hook keyBindingHook;
	static Utils::Hook commandBoundHook;
	static Utils::Hook setBindingHooks[std::size(Item_Bind_HandleKey_Key_SetBindingCalls)];

	static Utils::Hook bindCommandHook;

	static Utils::Hook remoteControlHook;
	static Utils::Hook locationSelectionHook;
	static Utils::Hook mouseMoveHook;

	static void CG_RegisterDvars_Hk()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(CG_RegisterDvars))();
		RegisterDvars();
	}

	static void Player_UseEntity_Hk(Game::gentity_s* playerEnt, Game::gentity_s* useEnt)
	{
		const int heldMs = Utils::Hook::Get<int>(level_time) - playerEnt->client->useHoldTime;

		if (!ShouldUse(playerEnt, static_cast<unsigned int>(heldMs)))
		{
			return;
		}

		reinterpret_cast<void(*)(Game::gentity_s*, Game::gentity_s*)>(Utils::Hook::Rebase(Player_UseEntity))(playerEnt, useEnt);
	}

	static void Key_SetBinding_Hk(int localClientNum, int keyNum, int binding)
	{
		int bound = binding;

		if (Mapping::IsControllerKey(keyNum))
		{
			if (installedRuntime != nullptr)
			{
				installedRuntime->Binds().NoteManualRebind();
			}

			if (bound != 0)
			{
				bound = ControllerBindingFor(bound);
			}
		}

		Game::Key_SetBinding(localClientNum, keyNum, bound);
	}

	static std::size_t ShownCommandKeys(int localClientNum, const char* cmd, int (&keys)[2])
	{
		const bool isControllerInUse = installedRuntime != nullptr && installedRuntime->Keys().IsInUse();

		return BindBridge::CommandKeys(localClientNum, isControllerInUse, cmd, keys);
	}

	static int Key_GetCommandAssignmentInternal_Hk(int localClientNum, const char* cmd, int* keys)
	{
		int found[2];
		const std::size_t count = ShownCommandKeys(localClientNum, cmd, found);

		keys[0] = found[0];
		keys[1] = found[1];

		return static_cast<int>(count);
	}

	static int CL_GetKeyBinding_Hk(int localClientNum, const char* cmd, char* keyNames)
	{
		const auto keynumToString = reinterpret_cast<char*(*)(int, char*, int)>(Utils::Hook::Rebase(Key_KeynumToStringBuf));

		Game::Sys_EnterCriticalSection(CRITSECT_KEY_BINDINGS);

		keyNames[keyNameSize] = '\0';

		int keys[2];
		const std::size_t count = ShownCommandKeys(localClientNum, cmd, keys);

		if (count == 0)
		{
			strcpy_s(keyNames, keyNameSize, "KEY_UNBOUND");
		}
		else
		{
			keynumToString(keys[0], keyNames, keyNameSize);

			if (count == 2)
			{
				keynumToString(keys[1], keyNames + keyNameSize, keyNameSize);
			}
		}

		Game::Sys_LeaveCriticalSection(CRITSECT_KEY_BINDINGS);

		return static_cast<int>(count);
	}

	static int Key_IsCommandBound_Hk(int localClientNum, const char* cmd)
	{
		Game::Sys_EnterCriticalSection(CRITSECT_KEY_BINDINGS);

		int keys[2];
		const std::size_t count = ShownCommandKeys(localClientNum, cmd, keys);

		Game::Sys_LeaveCriticalSection(CRITSECT_KEY_BINDINGS);

		if (count == 0)
		{
			return 0;
		}

		return 1;
	}

	static int Key_Bind_f_Key_GetBindingForCmd_Hk(const char* command)
	{
		const Components::Command::ClientParams params;

		return BindingForBindCommand(Game::Key_StringToKeynum(params.Get(1)), command);
	}

	static bool UI_RefreshViewport_Hk(const char* dvarName)
	{
		if (Game::Dvar_GetBool(dvarName))
		{
			return true;
		}

		return installedRuntime != nullptr && installedRuntime->Keys().IsInUse();
	}

	static void CL_MouseMove_Hk(int localClientNum, Game::usercmd_s* cmd, float frameTime)
	{
		if (IsDriving())
		{
			installedRuntime->View().ApplyMove(localClientNum, *cmd, frameTime);
			return;
		}

		reinterpret_cast<void(*)(int, Game::usercmd_s*, float)>(Utils::Hook::Rebase(CL_MouseMove))(localClientNum, cmd, frameTime);
	}

	static void CL_RemoteControlMove_Hk(int localClientNum, Game::usercmd_s* cmd)
	{
		reinterpret_cast<void(*)(int, Game::usercmd_s*)>(Utils::Hook::Rebase(CL_RemoteControlMove))(localClientNum, cmd);

		if (IsDriving())
		{
			installedRuntime->View().ApplyRemoteMove(localClientNum, *cmd);
		}
	}

	static bool CG_HandleLocationSelectionInput_Hk(int localClientNum, Game::usercmd_s* cmd)
	{
		const bool isSelecting = reinterpret_cast<bool(*)(int, Game::usercmd_s*)>(Utils::Hook::Rebase(CG_HandleLocationSelectionInput))(localClientNum, cmd);

		if (isSelecting && IsDriving())
		{
			installedRuntime->View().ApplyLocationSelection(localClientNum);
		}

		return isSelecting;
	}

	static void PatchUsercmdMovement()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(MSG_WriteDeltaUsercmdKey_MoveFlags, writeMoveFlags, sizeof(writeMoveFlags))
			&& Utils::Hook::MatchesBytes(MSG_WriteDeltaUsercmdKey_MoveResume, writeMoveResume, sizeof(writeMoveResume))
			&& Utils::Hook::MatchesBytes(MSG_WriteDeltaUsercmdKey_PartialBits, writePartialBits, sizeof(writePartialBits))
			&& Utils::Hook::MatchesBytes(MSG_WriteDeltaUsercmdKey_FullBits, writeFullBits, sizeof(writeFullBits))
			&& Utils::Hook::MatchesBytes(MSG_ReadDeltaUsercmdKey_PartialMove, readMove, sizeof(readMove))
			&& Utils::Hook::MatchesBytes(MSG_ReadDeltaUsercmdKey_PartialResume, readPartialResume, sizeof(readPartialResume))
			&& Utils::Hook::MatchesBytes(MSG_ReadDeltaUsercmdKey_FullMove, readMove, sizeof(readMove))
			&& Utils::Hook::MatchesBytes(MSG_ReadDeltaUsercmdKey_FullResume, readFullResume, sizeof(readFullResume));

		if (!isExpected)
		{
			Components::Logger::Error("controller: the usercmd move code does not read as expected, left alone, so an IW4x server will misread our movement\n");
			return;
		}

		MSG_WriteDeltaUsercmdKey_Resume = Utils::Hook::Rebase(MSG_WriteDeltaUsercmdKey_MoveResume);
		MSG_ReadDeltaUsercmdKey_Resume = Utils::Hook::Rebase(MSG_ReadDeltaUsercmdKey_FullResume);
		MSG_ReadDeltaUsercmdKey_Resume2 = Utils::Hook::Rebase(MSG_ReadDeltaUsercmdKey_PartialResume);

		const std::uintptr_t sites[] =
		{
			MSG_WriteDeltaUsercmdKey_MoveFlags,
			MSG_ReadDeltaUsercmdKey_FullMove,
			MSG_ReadDeltaUsercmdKey_PartialMove,
		};

		void* const stubs[] =
		{
			reinterpret_cast<void*>(MSG_WriteDeltaUsercmdKeyStub),
			reinterpret_cast<void*>(MSG_ReadDeltaUsercmdKeyStub),
			reinterpret_cast<void*>(MSG_ReadDeltaUsercmdKeyStub2),
		};

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			isSeated = moveHooks[i].Initialize(sites[i], stubs[i], HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : moveHooks)
			{
				hook.Uninstall();
			}

			Components::Logger::Error("controller: could not seat the usercmd move stubs, so an IW4x server will misread our movement\n");
			return;
		}

		for (auto& hook : moveHooks)
		{
			hook.Quick();
		}

		Utils::Hook::Set<std::int32_t>(MSG_WriteDeltaUsercmdKey_PartialBits + 4, 16);
		Utils::Hook::Set<std::int32_t>(MSG_WriteDeltaUsercmdKey_FullBits + 2, 16);
	}

	static void HookRegisterDvars()
	{
		if (!Utils::Hook::BranchesTo(CL_InitOnceForAllClients_CG_RegisterDvarsCall, CG_RegisterDvars, false))
		{
			Components::Logger::Error("controller: CL_InitOnceForAllClients does not call CG_RegisterDvars where expected, no controller dvars\n");
			return;
		}

		if (!registerDvarsHook.Initialize(CL_InitOnceForAllClients_CG_RegisterDvarsCall, reinterpret_cast<void*>(CG_RegisterDvars_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Components::Logger::Error("controller: could not seat the dvar registration hook, no controller dvars\n");
		}
	}

	static void HookUseEntity()
	{
		if (!Utils::Hook::BranchesTo(Player_UpdateActivate_Player_UseEntityCall, Player_UseEntity, false))
		{
			Components::Logger::Error("controller: Player_UpdateActivate does not call Player_UseEntity where expected, no use hold for game pads\n");
			return;
		}

		if (!useEntityHook.Initialize(Player_UpdateActivate_Player_UseEntityCall, reinterpret_cast<void*>(Player_UseEntity_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Components::Logger::Error("controller: could not seat the use hold hook, no use hold for game pads\n");
		}
	}

	static void HookBindCommand()
	{
		if (!Utils::Hook::BranchesTo(Key_Bind_f_Key_GetBindingForCmdCall, Key_GetBindingForCmd, false))
		{
			Components::Logger::Error("controller: Key_Bind_f does not call Key_GetBindingForCmd where expected, old controller binds are not migrated\n");
			return;
		}

		if (!bindCommandHook.Initialize(Key_Bind_f_Key_GetBindingForCmdCall, reinterpret_cast<void*>(Key_Bind_f_Key_GetBindingForCmd_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Components::Logger::Error("controller: could not seat the bind command hook, old controller binds are not migrated\n");
		}
	}

	static bool TryBuildKeyNameTables()
	{
		constexpr std::size_t tableCount = Game::KEY_NAME_COUNT + Mapping::engineKeyCount + 1;
		constexpr std::size_t tableSize = tableCount * sizeof(Game::keyname_t);

		static_assert(Game::KEY_NAME_COUNT == Game::LOCALIZED_KEY_NAME_COUNT, "both key name tables are extended the same way");

		auto* const names = static_cast<Game::keyname_t*>(Utils::Hook::AllocateDataNear(keynames, tableSize));
		auto* const localized = static_cast<Game::keyname_t*>(Utils::Hook::AllocateDataNear(keynames_localized, tableSize));

		if (names == nullptr || localized == nullptr)
		{
			return false;
		}

		std::memcpy(names, reinterpret_cast<const void*>(Utils::Hook::Rebase(keynames)), Game::KEY_NAME_COUNT * sizeof(Game::keyname_t));
		std::memcpy(localized, reinterpret_cast<const void*>(Utils::Hook::Rebase(keynames_localized)), Game::LOCALIZED_KEY_NAME_COUNT * sizeof(Game::keyname_t));

		std::size_t index = Game::KEY_NAME_COUNT;

		for (const auto key : Mapping::Keys())
		{
			const char* glyph = Mapping::GlyphFor(key, Mapping::GlyphFamily::Xbox);

			if (glyph == nullptr)
			{
				glyph = Mapping::KeyName(key);
			}

			names[index] = Game::keyname_t{ Mapping::KeyName(key), static_cast<int>(key) };
			localized[index] = Game::keyname_t{ glyph, static_cast<int>(key) };
			++index;
		}

		names[index] = Game::keyname_t{ nullptr, 0 };
		localized[index] = Game::keyname_t{ nullptr, 0 };

		for (const auto& lea : keyNameLeas)
		{
			if (!Utils::Hook::CanLeaReach(lea, names))
			{
				return false;
			}
		}

		if (!Utils::Hook::CanLeaReach(localizedKeyNameLea, localized))
		{
			return false;
		}

		for (const auto& lea : keyNameLeas)
		{
			Utils::Hook::PointLeaAt(lea, names);
		}

		Utils::Hook::PointLeaAt(localizedKeyNameLea, localized);

		combinedLocalizedKeyNames = localized;
		appliedGlyphs = Mapping::GlyphFamily::Xbox;
		return true;
	}

	static void UninstallInput()
	{
		inFrameHook.Uninstall();
		cursorHook.Uninstall();
		assignmentHook.Uninstall();
		keyBindingHook.Uninstall();
		commandBoundHook.Uninstall();

		for (auto& hook : setBindingHooks)
		{
			hook.Uninstall();
		}
	}

	static bool TryInstallInput()
	{
		bool isExpected = Utils::Hook::BranchesTo(UI_RefreshViewport_Dvar_GetBoolCall, Dvar_GetBool, false)
			&& Utils::Hook::BranchesTo(Key_GetCommandAssignment_Jump, Key_GetCommandAssignmentInternal, true)
			&& Utils::Hook::BranchesTo(GetKeyBindingLocalizedString_CL_GetKeyBindingCall, CL_GetKeyBinding, false)
			&& Utils::Hook::BranchesTo(Key_IsCommandBoundCall, Key_IsCommandBound, false)
			&& Utils::Hook::MatchesBytes(IN_Frame_MouseMove, mouseMoveStart, sizeof(mouseMoveStart))
			&& Utils::Hook::IsLeaIntact(localizedKeyNameLea);

		for (const auto call : Item_Bind_HandleKey_Key_SetBindingCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, Key_SetBinding, false);
		}

		for (const auto& lea : keyNameLeas)
		{
			isExpected = isExpected && Utils::Hook::IsLeaIntact(lea);
		}

		if (!isExpected)
		{
			Components::Logger::Error("controller: the key and input code does not read as expected, no controller support\n");
			return false;
		}

		Gamepad_INFrameReturn = Utils::Hook::Rebase(IN_Frame_Return);

		bool isSeated = inFrameHook.Initialize(IN_Frame_MouseMove, reinterpret_cast<void*>(INFrameMouseMoveStub), HOOK_CALL)->Install()->IsInstalled()
			&& cursorHook.Initialize(UI_RefreshViewport_Dvar_GetBoolCall, reinterpret_cast<void*>(UI_RefreshViewport_Hk), HOOK_CALL)->Install()->IsInstalled()
			&& assignmentHook.Initialize(Key_GetCommandAssignment_Jump, reinterpret_cast<void*>(Key_GetCommandAssignmentInternal_Hk), HOOK_JUMP)->Install()->IsInstalled()
			&& keyBindingHook.Initialize(GetKeyBindingLocalizedString_CL_GetKeyBindingCall, reinterpret_cast<void*>(CL_GetKeyBinding_Hk), HOOK_CALL)->Install()->IsInstalled()
			&& commandBoundHook.Initialize(Key_IsCommandBoundCall, reinterpret_cast<void*>(Key_IsCommandBound_Hk), HOOK_CALL)->Install()->IsInstalled();

		for (std::size_t i = 0; i < std::size(Item_Bind_HandleKey_Key_SetBindingCalls); ++i)
		{
			isSeated = isSeated && setBindingHooks[i].Initialize(Item_Bind_HandleKey_Key_SetBindingCalls[i], reinterpret_cast<void*>(Key_SetBinding_Hk), HOOK_CALL)->Install()->IsInstalled();
		}

		if (!isSeated || !TryBuildKeyNameTables())
		{
			UninstallInput();

			Components::Logger::Error("controller: could not seat every input hook or place the key names, no controller support\n");
			return false;
		}

		Utils::Hook::Nop(IN_Frame_MouseMove + 5, sizeof(mouseMoveStart) - 5);
		return true;
	}

	static void TryInstallMovement()
	{
		const bool isExpected = Utils::Hook::BranchesTo(CL_CreateCmd_CL_RemoteControlMoveCall, CL_RemoteControlMove, false)
			&& Utils::Hook::BranchesTo(CL_CreateCmd_CG_HandleLocationSelectionInputCall, CG_HandleLocationSelectionInput, false)
			&& Utils::Hook::BranchesTo(CL_CreateCmd_CL_MouseMoveCall, CL_MouseMove, false);

		if (!isExpected)
		{
			Components::Logger::Error("controller: the movement code does not read as expected, no stick movement or aim assist\n");
			return;
		}

		const bool isSeated = remoteControlHook.Initialize(CL_CreateCmd_CL_RemoteControlMoveCall, reinterpret_cast<void*>(CL_RemoteControlMove_Hk), HOOK_CALL)->Install()->IsInstalled()
			&& locationSelectionHook.Initialize(CL_CreateCmd_CG_HandleLocationSelectionInputCall, reinterpret_cast<void*>(CG_HandleLocationSelectionInput_Hk), HOOK_CALL)->Install()->IsInstalled()
			&& mouseMoveHook.Initialize(CL_CreateCmd_CL_MouseMoveCall, reinterpret_cast<void*>(CL_MouseMove_Hk), HOOK_CALL)->Install()->IsInstalled();

		if (!isSeated)
		{
			remoteControlHook.Uninstall();
			locationSelectionHook.Uninstall();
			mouseMoveHook.Uninstall();

			Components::Logger::Error("controller: could not seat every movement hook, no stick movement or aim assist\n");
		}
	}

	void InstallProtocol()
	{
		PatchUsercmdMovement();
		HookRegisterDvars();
		HookUseEntity();
	}

	bool TryInstall()
	{
		InstallProtocol();

		if (!TryInstallInput())
		{
			return false;
		}

		TryInstallMovement();
		HookBindCommand();

		Components::Console::OnKey([](int key, int down)
		{
			if (installedRuntime != nullptr && down != 0 && !Mapping::IsControllerKey(key))
			{
				installedRuntime->Keys().NoteOtherInput();
			}

			return false;
		});

		return true;
	}

	void Attach(Runtime* runtime) noexcept
	{
		installedRuntime = runtime;
	}

	void NoteMouseMove(int dx, int dy)
	{
		if (installedRuntime != nullptr && (dx != 0 || dy != 0))
		{
			installedRuntime->Keys().NoteOtherInput();
		}
	}
}
