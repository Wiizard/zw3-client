#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Components/Modules/Remix/GpuCompatibility.hpp"

#include <iostream>
#include <stdexcept>

int main(const int argc, char** argv)
{
	try
	{
		using namespace Components::Remix;
		const auto require = [](const bool condition, const char* message)
		{
			if (!condition) throw std::runtime_error(message);
		};
		for (const auto* name : { L"NVIDIA GeForce RTX 2060", L"NVIDIA GeForce RTX 4070 Ti", L"NVIDIA GeForce RTX 5090",
			L"NVIDIA Quadro RTX 8000", L"NVIDIA RTX A4000", L"NVIDIA RTX PRO 6000 Blackwell", L"nvidia geforce rtx 3060 laptop gpu", L"NVIDIA RTX3080" })
		{
			require(IsSupportedRemixGpu(0x10DE, name, false), "Supported RTX GPU was rejected");
		}
		for (const auto* name : { L"NVIDIA GeForce GTX 1080 Ti", L"NVIDIA GeForce GTX 1650", L"NVIDIA Quadro P4000",
			L"NVIDIA TITAN V", L"", L"NVIDIA NOTRTX", L"NVIDIA RTXFake" })
		{
			require(!IsSupportedRemixGpu(0x10DE, name, false), "Non-RTX GPU was accepted");
		}
		require(!IsSupportedRemixGpu(0x1002, L"AMD Radeon RX 7900 XTX", false), "AMD GPU was accepted");
		require(!IsSupportedRemixGpu(0x8086, L"Intel Arc A770", false), "Intel GPU was accepted");
		require(!IsSupportedRemixGpu(0x1002, L"NVIDIA GeForce RTX 4070", false), "Vendor ID was ignored");
		require(!IsSupportedRemixGpu(0x10DE, L"NVIDIA GeForce RTX 4070", true), "Software adapter was accepted");

		RemixGpuCompatibility hybrid;
		hybrid.AddAdapter(0x8086, L"Intel UHD Graphics", false);
		hybrid.AddAdapter(0x10DE, L"NVIDIA GeForce RTX 4060 Laptop GPU", false);
		hybrid.AddAdapter(0x1414, L"Microsoft Basic Render Driver", true);
		require(hybrid.supported, "Integrated GPU hid a compatible discrete GPU");
		require(hybrid.adapters.find(L"Intel UHD") != std::wstring::npos && hybrid.adapters.find(L"RTX 4060") != std::wstring::npos,
			"Detected hardware was omitted");
		require(hybrid.adapters.find(L"Microsoft") == std::wstring::npos, "Software adapter was listed as hardware");

		RemixGpuCompatibility unsupported;
		unsupported.AddAdapter(0x10DE, L"NVIDIA GeForce GTX 1080", false);
		const auto message = unsupported.ErrorMessage();
		require(!unsupported.supported && message.find(L"GTX 1080") != std::wstring::npos, "Unsupported GPU was not explained");
		require(message.find(L"has not been enabled") != std::wstring::npos && message.find(L"will not restart") != std::wstring::npos,
			"Popup did not explain the unchanged setting and blocked restart");
		RemixGpuCompatibility unknown;
		unknown.detectionFailed = true;
		require(!unknown.supported && unknown.ErrorMessage().find(L"could not be detected") != std::wstring::npos,
			"Detection failure was accepted or misreported");
		std::cout << "Remix GPU compatibility: RTX generations, GTX/AMD/Intel, vendor ID, software adapters, hybrid GPUs and errors passed\n";

		if (argc > 1 && std::string_view(argv[1]) == "--probe")
		{
			const auto actual = CheckRemixGpuCompatibility();
			std::wcout << L"DXGI probe: supported=" << actual.supported << L", detectionFailed=" << actual.detectionFailed << L"\n" << actual.adapters << L"\n";
			if (actual.detectionFailed || actual.adapters.empty()) return 2;
		}
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
