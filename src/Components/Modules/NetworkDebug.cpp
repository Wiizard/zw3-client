#include "STDInclude.hpp"

#include "NetworkDebug.hpp"
#include "Logger.hpp"

namespace Components
{
	constexpr std::uintptr_t I_stricmp = 0x14028C0F0;

	constexpr std::uintptr_t Cmd_ExecuteSingleCommand_NameCompareCalls[] = { 0x1401E77E7, 0x1401E79A7 };

	static Utils::Hook hooks[std::size(Cmd_ExecuteSingleCommand_NameCompareCalls)];

	int NetworkDebug::I_stricmp_Stub(const char* s0, const char* s1)
	{
		assert(s0);
		assert(s1);

		if (!s0)
		{
			return -1;
		}

		if (!s1)
		{
			return 1;
		}

		return reinterpret_cast<int(*)(const char*, const char*)>(Utils::Hook::Rebase(I_stricmp))(s0, s1);
	}

	NetworkDebug::NetworkDebug()
	{
		bool isExpected = true;

		for (const auto call : Cmd_ExecuteSingleCommand_NameCompareCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, I_stricmp, false);
		}

		if (!isExpected)
		{
			Logger::Error("networkdebug: Cmd_ExecuteSingleCommand does not read as expected, command names stay unchecked\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(Cmd_ExecuteSingleCommand_NameCompareCalls); ++i)
		{
			isSeated = hooks[i].Initialize(Cmd_ExecuteSingleCommand_NameCompareCalls[i], reinterpret_cast<void*>(I_stricmp_Stub), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("networkdebug: could not seat every hook, command names stay unchecked\n");
		}
	}
}
