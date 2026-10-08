#include "STDInclude.hpp"

#include "Flags.hpp"

namespace Components
{
	std::vector<std::string> Flags::enabledFlags;
	bool Flags::isParsed = false;

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
