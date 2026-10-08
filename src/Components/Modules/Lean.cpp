#include "STDInclude.hpp"

#include "Lean.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "Logger.hpp"

namespace Components
{
	Dvar::Var Lean::bg_lean;

	constexpr std::uintptr_t PM_UpdateLean = 0x140092180;
	constexpr std::uintptr_t PM_UpdateViewAngles_UpdateLeanCalls[] = { 0x140092AE5, 0x140092C8C };

	static Utils::Hook hooks[std::size(PM_UpdateViewAngles_UpdateLeanCalls)];

	static const char* const leanBindCommands[] = { "+leanleft", "-leanleft", "+leanright", "-leanright" };

	constexpr std::uintptr_t playerKeys = 0x1406C70A0;
	constexpr std::size_t playerKeysKeysOffset = 0x124;
	constexpr int keyCount = 256;

	struct KeyState
	{
		int down;
		int repeats;
		int binding;
	};

	static_assert(sizeof(KeyState) == 12);

	bool Lean::isLeaningLeft = false;
	bool Lean::isLeaningRight = false;

	bool Lean::IsBindingHeld(int binding)
	{
		const auto* const keys = reinterpret_cast<const KeyState*>(Utils::Hook::Rebase(playerKeys) + playerKeysKeysOffset);

		for (int key = 0; key < keyCount; ++key)
		{
			if (keys[key].down && keys[key].binding == binding)
			{
				return true;
			}
		}

		return false;
	}

	void Lean::ApplyLeanFlags(Game::usercmd_s* cmd)
	{
		if (!bg_lean.Get<bool>())
		{
			return;
		}

		if (isLeaningLeft || IsBindingHeld(Command::GetBinding("+leanleft")))
		{
			cmd->buttons |= Game::CMD_BUTTON_LEAN_LEFT;
		}

		if (isLeaningRight || IsBindingHeld(Command::GetBinding("+leanright")))
		{
			cmd->buttons |= Game::CMD_BUTTON_LEAN_RIGHT;
		}
	}

	void Lean::PM_UpdateLean_Stub(Game::playerState_s* ps, float msec, Game::usercmd_s* cmd, void(*capsuleTrace)(Game::trace_t*, const float*, const float*, const Game::Bounds*, int, int))
	{
		if (bg_lean.Get<bool>())
		{
			reinterpret_cast<decltype(&PM_UpdateLean_Stub)>(Utils::Hook::Rebase(PM_UpdateLean))(ps, msec, cmd, capsuleTrace);
		}
	}

	Lean::Lean()
	{
		bool isExpected = true;

		for (const auto call : PM_UpdateViewAngles_UpdateLeanCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, PM_UpdateLean, false);
		}

		if (!isExpected)
		{
			Logger::Error("lean: PM_UpdateViewAngles does not read as expected, no bg_lean\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(PM_UpdateViewAngles_UpdateLeanCalls); ++i)
		{
			isSeated = hooks[i].Initialize(PM_UpdateViewAngles_UpdateLeanCalls[i], reinterpret_cast<void*>(PM_UpdateLean_Stub), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("lean: could not seat every hook, no bg_lean\n");
			return;
		}

		Events::OnDvarInit([]
		{
			bg_lean = Dvar::Register("bg_lean", true, Game::DVAR_CODINFO, "Enable CoD4 leaning");
		});

		Command::Add("+leanleft", []
		{
			isLeaningLeft = true;
		});

		Command::Add("-leanleft", []
		{
			isLeaningLeft = false;
		});

		Command::Add("+leanright", []
		{
			isLeaningRight = true;
		});

		Command::Add("-leanright", []
		{
			isLeaningRight = false;
		});

		Events::OnClientCmdButtons(ApplyLeanFlags);

		for (const auto* const name : leanBindCommands)
		{
			if (!Command::AddBindable(name))
			{
				Logger::Error("lean: {} cannot be bound to a key\n", name);
			}
		}
	}
}
