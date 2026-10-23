#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Components/Modules/Remix/StartupSettings.hpp"

#include <iostream>
#include <stdexcept>

#pragma comment(lib, "shell32.lib")

int main()
{
	try
	{
		using Components::Remix::ReadStartupRemixOverride;
		const auto require = [](const bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		};
		require(!ReadStartupRemixOverride(nullptr), "Null command line produced an override");
		require(!ReadStartupRemixOverride(L"zw3.exe -console"), "Normal startup overrode saved config");
		require(ReadStartupRemixOverride(L"\"D:\\Games\\ZW3 WIP 2\\zw3.exe\" -console +set r_rtxRemix 1") == true,
			"RTX startup override was lost with a quoted executable path");
		require(ReadStartupRemixOverride(L"zw3.exe +set r_rtxRemix 0") == false,
			"Explicit disable was ignored");
		require(ReadStartupRemixOverride(L"zw3.exe +SeTa R_RTXREMIX \"1\" +set r_gpuSync 0") == true,
			"Archived or case-insensitive override was not recognized");
		require(ReadStartupRemixOverride(L"zw3.exe +set r_rtxRemix 1 +seta r_rtxRemix 0") == false,
			"Last valid startup override did not win");
		require(ReadStartupRemixOverride(L"zw3.exe +set r_rtxRemix 0 +set r_rtxRemix 1") == true,
			"Restart override did not replace an earlier disable");
		require(ReadStartupRemixOverride(L"\"D:\\Games\\ZW3 WIP 2\\zw3.exe\" -console -zw3-restart-parent=1234 +set r_rtxRemix 1") == true,
			"Menu restart handoff lost the enabled renderer choice");
		require(ReadStartupRemixOverride(L"zw3.exe -zw3-restart-parent=1234 +set r_rtxRemix 0") == false,
			"Menu restart handoff lost the disabled renderer choice");
		for (const auto* commandLine : { L"zw3.exe +set r_gpuSync 0", L"zw3.exe +set r_rtxRemix",
			L"zw3.exe +set r_rtxRemix invalid", L"zw3.exe +set r_rtxRemixOther 1",
			L"zw3.exe +settings r_rtxRemix 1", L"zw3.exe +exec \"+set r_rtxRemix 1\"" })
		{
			require(!ReadStartupRemixOverride(commandLine), "Unrelated or malformed argument changed RTX startup");
		}
		std::cout << "Remix startup settings: enable/disable, quoting, case, last override, normal startup and malformed input passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
