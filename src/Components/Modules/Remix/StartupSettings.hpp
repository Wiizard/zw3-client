#pragma once

#include <windows.h>
#include <shellapi.h>

#include <cwchar>
#include <optional>
#include <string_view>

namespace Components::Remix
{
	inline std::optional<bool> ReadStartupRemixOverride(const wchar_t* commandLine)
	{
		if (!commandLine) return std::nullopt;
		int count = 0;
		auto* const arguments = CommandLineToArgvW(commandLine, &count);
		if (!arguments) return std::nullopt;

		std::optional<bool> result;
		for (int i = 1; i + 2 < count; ++i)
		{
			if ((_wcsicmp(arguments[i], L"+set") != 0 && _wcsicmp(arguments[i], L"+seta") != 0)
				|| _wcsicmp(arguments[i + 1], L"r_rtxRemix") != 0)
			{
				continue;
			}

			const std::wstring_view value(arguments[i + 2]);
			if (value == L"0" || value == L"1") result = value == L"1";
			i += 2;
		}

		LocalFree(arguments);
		return result;
	}
}
