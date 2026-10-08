#include "STDInclude.hpp"

#include "Elevators.hpp"
#include "Events.hpp"
#include "Logger.hpp"

extern "C"
{
	void StartSolidStub();
	void GroundStartSolidStub();
	void TraceStub();

	Game::dvar_t* Elevators_bg_elevators = nullptr;
}

namespace Components
{
	constexpr std::uintptr_t PM_CorrectAllSolid_StartSolidTest = 0x14008F9AF;
	constexpr std::uintptr_t PM_CorrectAllSolid_GroundStartSolidTest = 0x14008FAB2;
	static const std::uint8_t startSolidTest[] = { 0x0F, 0xB6, 0x47, 0x2D, 0x84, 0xC0 };

	constexpr std::uintptr_t PM_CheckDuck_TraceCalls[] = { 0x14008EA48, 0x14008EB03, 0x14008EBEA };
	static const std::uint8_t traceCall[] = { 0x42, 0xFF, 0x54, 0xD5, 0x00 };

	static Utils::Hook hooks[std::size(PM_CheckDuck_TraceCalls) + 2];

	Elevators::Elevators()
	{
		bool isExpected = Utils::Hook::MatchesBytes(PM_CorrectAllSolid_StartSolidTest, startSolidTest, sizeof(startSolidTest))
			&& Utils::Hook::MatchesBytes(PM_CorrectAllSolid_GroundStartSolidTest, startSolidTest, sizeof(startSolidTest));

		for (const auto call : PM_CheckDuck_TraceCalls)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(call, traceCall, sizeof(traceCall));
		}

		if (!isExpected)
		{
			Logger::Error("elevators: pmove does not read as expected, no bg_elevators\n");
			return;
		}

		Events::OnDvarInit([]
		{
			static const char* values[] =
			{
				"off",
				"normal",
				"easy",
				nullptr
			};

			Elevators_bg_elevators = Game::Dvar_RegisterEnum("bg_elevators", values, ENABLED, Game::DVAR_CODINFO, "Elevators glitch settings");

			bool isSeated = hooks[0].Initialize(PM_CorrectAllSolid_StartSolidTest, StartSolidStub, HOOK_CALL)->Install()->IsInstalled();
			isSeated = hooks[1].Initialize(PM_CorrectAllSolid_GroundStartSolidTest, GroundStartSolidStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

			for (std::size_t i = 0; i < std::size(PM_CheckDuck_TraceCalls); ++i)
			{
				isSeated = hooks[i + 2].Initialize(PM_CheckDuck_TraceCalls[i], TraceStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
			}

			if (!isSeated)
			{
				for (auto& hook : hooks)
				{
					hook.Uninstall();
				}

				Logger::Error("elevators: could not seat every hook, bg_elevators does nothing\n");
				return;
			}

			Utils::Hook::Nop(PM_CorrectAllSolid_StartSolidTest + 5, sizeof(startSolidTest) - 5);
			Utils::Hook::Nop(PM_CorrectAllSolid_GroundStartSolidTest + 5, sizeof(startSolidTest) - 5);
		});
	}
}
