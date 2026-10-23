#include "STDInclude.hpp"

#include "Flags.hpp"

#include <cwchar>

namespace Components
{
	std::vector<std::string> Flags::enabledFlags;
	bool Flags::isParsed = false;
	std::wstring Flags::originalCommandLine;

	const std::wstring& Flags::GetOriginalCommandLine()
	{
		ParseFlags();
		return originalCommandLine;
	}

	bool Flags::HasFlag(const std::string& flag)
	{
		ParseFlags();

		const auto wanted = Utils::String::ToLower(flag);

		return std::ranges::any_of(enabledFlags, [&wanted](const std::string& entry)
		{
			return Utils::String::ToLower(entry) == wanted;
		});
	}

	void Flags::ParseFlags()
	{
		if (isParsed)
		{
			return;
		}

		isParsed = true;
		// Keep quoting intact for a later process restart; the legacy flag parser removes quotes below.
		originalCommandLine = GetCommandLineW();

		if (char* const buffer = GetCommandLineA())
		{
			char* read = buffer;
			char* write = buffer;

			while (*read != '\0')
			{
				if (*read != '\"')
				{
					*write = *read;
					++write;
				}

				++read;
			}

			*write = '\0';
		}

		if (wchar_t* const buffer = GetCommandLineW())
		{
			wchar_t* read = buffer;
			wchar_t* write = buffer;

			while (*read != L'\0')
			{
				if (*read != L'\"')
				{
					*write = *read;
					++write;
				}

				++read;
			}

			*write = L'\0';
		}

		int count = 0;
		wchar_t** const arguments = CommandLineToArgvW(GetCommandLineW(), &count);

		if (!arguments)
		{
			return;
		}

		for (int i = 0; i < count; ++i)
		{
			std::wstring argument(arguments[i]);

			if (argument.starts_with(RestartParentFlag))
			{
				wchar_t* end = nullptr;
				const auto parentId = std::wcstoul(argument.c_str() + RestartParentFlag.size(), &end, 10);
				if (!parentId || *end || parentId == GetCurrentProcessId())
				{
					ExitProcess(ERROR_INVALID_PARAMETER);
				}

				// Wait before Singleton and renderer initialization so the previous instance can release its resources.
				if (const HANDLE parent = OpenProcess(SYNCHRONIZE, FALSE, parentId))
				{
					const DWORD result = WaitForSingleObject(parent, 60000);
					CloseHandle(parent);
					if (result != WAIT_OBJECT_0)
					{
						ExitProcess(ERROR_TIMEOUT);
					}
				}
				else if (GetLastError() != ERROR_INVALID_PARAMETER)
				{
					ExitProcess(GetLastError());
				}
				continue;
			}

			if (argument.empty() || argument.front() != L'-')
			{
				continue;
			}

			argument.erase(argument.begin());
			enabledFlags.emplace_back(Utils::String::Convert(argument));
		}

		LocalFree(arguments);
	}
}
