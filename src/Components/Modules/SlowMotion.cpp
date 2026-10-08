#include "STDInclude.hpp"

#include "SlowMotion.hpp"
#include "Logger.hpp"

extern "C"
{
	void SlowMotionUpdateStub();

	int SlowMotion_Delay = 0;

	std::uintptr_t SlowMotion_Active = 0;
}

namespace Components
{
	constexpr std::uintptr_t ScrCmd_SetSlowMotion_SetConfigstringJump = 0x140178152;
	constexpr std::uintptr_t SV_SetConfigstring = 0x14023ACB0;

	constexpr std::uintptr_t Com_SetSlowMotion = 0x1401F68E0;

	constexpr std::uintptr_t Com_Frame_SlowMotionTest = 0x1401F4603;
	constexpr std::uintptr_t slowMotionActive = 0x141BD9AE0;
	static const std::uint8_t slowMotionTest[] = { 0x80, 0x3D, 0xD6, 0x54, 0x9E, 0x01, 0x00 };

	static Utils::Hook hooks[2];

	void SlowMotion::ScrCmd_SetSlowMotion_Stub(int index, const char* string)
	{
		reinterpret_cast<void(*)(int, const char*)>(Utils::Hook::Rebase(SV_SetConfigstring))(index, string);

		auto duration = 1000;
		const auto start = Game::Scr_GetFloat(0);
		auto end = 1.0f;

		if (Game::Scr_GetNumParam() >= 2)
		{
			end = Game::Scr_GetFloat(1);
		}

		if (Game::Scr_GetNumParam() >= 3)
		{
			duration = static_cast<int>(Game::Scr_GetFloat(2) * 1000.0f);
		}

		auto delay = 0;

		if (start > end)
		{
			if (duration < 150)
			{
				delay = duration;
			}
			else
			{
				delay = 150;
			}
		}

		duration = duration - delay;

		reinterpret_cast<void(*)(float, float, int)>(Utils::Hook::Rebase(Com_SetSlowMotion))(start, end, duration);
		SlowMotion_Delay = delay;

		for (auto i = 0; i < *Game::svs_clientCount; ++i)
		{
			Game::svs_clients[i].nextSnapshotTime = *Game::svs_time - 1;
		}
	}

	SlowMotion::SlowMotion()
	{
		const bool isExpected = Utils::Hook::BranchesTo(ScrCmd_SetSlowMotion_SetConfigstringJump, SV_SetConfigstring, true)
			&& Utils::Hook::MatchesBytes(Com_Frame_SlowMotionTest, slowMotionTest, sizeof(slowMotionTest));

		if (!isExpected)
		{
			Logger::Error("slowmotion: setslowmotion or Com_Frame does not read as expected, no slow motion delay\n");
			return;
		}

		SlowMotion_Delay = 0;
		SlowMotion_Active = Utils::Hook::Rebase(slowMotionActive);

		bool isSeated = hooks[0].Initialize(ScrCmd_SetSlowMotion_SetConfigstringJump, reinterpret_cast<void*>(ScrCmd_SetSlowMotion_Stub), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = hooks[1].Initialize(Com_Frame_SlowMotionTest, SlowMotionUpdateStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("slowmotion: could not seat the slow motion hooks\n");
			return;
		}

		Utils::Hook::Nop(Com_Frame_SlowMotionTest + 5, sizeof(slowMotionTest) - 5);
	}
}
