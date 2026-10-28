#pragma once

#include <dxgi1_2.h>
#include <wrl/client.h>

#include <cstdint>
#include <string>
#include <string_view>

#pragma comment(lib, "dxgi.lib")

namespace Components::Remix
{
	inline bool IsSupportedRemixGpu(const std::uint32_t vendorId, const std::wstring_view name, const bool software)
	{
		if (software || vendorId != 0x10DE) return false;

		const auto upper = [](const wchar_t character)
		{
			return character >= L'a' && character <= L'z' ? character - L'a' + L'A' : character;
		};
		const auto alphanumeric = [&](const wchar_t character)
		{
			const auto value = upper(character);
			return (value >= L'A' && value <= L'Z') || (value >= L'0' && value <= L'9');
		};
		for (std::size_t i = 0; i + 3 <= name.size(); ++i)
		{
			if (upper(name[i]) == L'R' && upper(name[i + 1]) == L'T' && upper(name[i + 2]) == L'X'
				&& (i == 0 || !alphanumeric(name[i - 1]))
				&& (i + 3 == name.size() || !alphanumeric(name[i + 3]) || (name[i + 3] >= L'0' && name[i + 3] <= L'9')))
			{
				return true;
			}
		}
		return false;
	}

	struct RemixGpuCompatibility
	{
		bool supported = false;
		bool detectionFailed = false;
		std::wstring adapters;

		void AddAdapter(const std::uint32_t vendorId, const std::wstring_view name, const bool software)
		{
			if (software) return;
			if (!adapters.empty()) adapters += L"\n";
			adapters += name;
			supported |= IsSupportedRemixGpu(vendorId, name, software);
		}

		std::wstring ErrorMessage() const
		{
			std::wstring message = detectionFailed
				? L"RTX Remix cannot be enabled because the graphics hardware could not be detected.\n\n"
				: L"RTX Remix cannot be enabled because no supported NVIDIA RTX graphics card was detected.\n\n";
			message += L"RTX Remix requires an NVIDIA RTX GPU. GeForce GTX, AMD and Intel graphics are not supported.\n\n";
			if (!adapters.empty()) message += L"Detected graphics cards:\n" + adapters + L"\n\n";
			if (detectionFailed) message += L"Please update your graphics driver and try again.\n\n";
			message += L"RTX Remix has not been enabled. The game will not restart.";
			return message;
		}
	};

	inline RemixGpuCompatibility CheckRemixGpuCompatibility()
	{
		RemixGpuCompatibility result;
		Microsoft::WRL::ComPtr<IDXGIFactory1> factory;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
		{
			result.detectionFailed = true;
			return result;
		}

		// Enumerate every adapter so an integrated display GPU does not hide a laptop's RTX GPU.
		for (UINT index = 0; ; ++index)
		{
			Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
			const HRESULT status = factory->EnumAdapters1(index, adapter.GetAddressOf());
			if (status == DXGI_ERROR_NOT_FOUND) break;
			if (FAILED(status))
			{
				result.detectionFailed = true;
				break;
			}
			DXGI_ADAPTER_DESC1 description{};
			if (FAILED(adapter->GetDesc1(&description)))
			{
				result.detectionFailed = true;
				continue;
			}
			result.AddAdapter(description.VendorId, description.Description, (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0);
		}
		return result;
	}
}
