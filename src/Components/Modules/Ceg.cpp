#include "STDInclude.hpp"

#include "Ceg.hpp"
#include "Logger.hpp"

namespace Components
{
	constexpr std::uintptr_t FS_Startup_Com_ReadCDKeyCall = 0x140278C0D;
	constexpr std::uintptr_t Com_ReadCDKey = 0x1401F5CB0;

	Ceg::Ceg()
	{
		if (!Utils::Hook::BranchesTo(FS_Startup_Com_ReadCDKeyCall, Com_ReadCDKey, false))
		{
			Logger::Error("ceg: FS_Startup does not read as expected, the cd key is still read from the registry\n");
			return;
		}

		Utils::Hook::Nop(FS_Startup_Com_ReadCDKeyCall, 5);
	}
}
