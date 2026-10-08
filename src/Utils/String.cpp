#include "STDInclude.hpp"

namespace Utils::String
{
	const char* VA(const char* format, ...)
	{
		static thread_local VAProvider<8, 256> provider;

		va_list ap;
		va_start(ap, format);

		const char* const result = provider.Get(format, ap);

		va_end(ap);

		return result;
	}

	std::string ToLower(const std::string& text)
	{
		std::string result;

		std::ranges::transform(text, std::back_inserter(result), [](const unsigned char input) -> char
		{
			return static_cast<char>(std::tolower(input));
		});

		return result;
	}

	std::string ToUpper(const std::string& text)
	{
		std::string result;

		std::ranges::transform(text, std::back_inserter(result), [](const unsigned char input) -> char
		{
			return static_cast<char>(std::toupper(input));
		});

		return result;
	}

	bool Compare(const std::string& lhs, const std::string& rhs)
	{
		return ToLower(lhs) == ToLower(rhs);
	}

	std::vector<std::string> Split(const std::string& text, char delimiter)
	{
		std::vector<std::string> result;
		std::stringstream stream(text);
		std::string item;

		while (std::getline(stream, item, delimiter))
		{
			result.push_back(item);
		}

		return result;
	}

	void Replace(std::string& text, const std::string& from, const std::string& to)
	{
		if (from.empty())
		{
			return;
		}

		std::size_t at = text.find(from);

		while (at != std::string::npos)
		{
			text.replace(at, from.size(), to);
			at = text.find(from, at + to.size());
		}
	}

	bool StartsWith(const std::string& haystack, const std::string& needle)
	{
		return haystack.starts_with(needle);
	}

	bool EndsWith(const std::string& haystack, const std::string& needle)
	{
		return haystack.ends_with(needle);
	}

	bool Contains(const std::string& haystack, const std::string& needle)
	{
		return haystack.find(needle) != std::string::npos;
	}

	bool IsNumber(const std::string& text)
	{
		if (text.empty())
		{
			return false;
		}

		return std::ranges::all_of(text, [](const unsigned char input)
		{
			return std::isdigit(input) != 0;
		});
	}

	std::string& LTrim(std::string& text)
	{
		text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](const unsigned char input)
		{
			return std::isspace(input) == 0;
		}));

		return text;
	}

	std::string& RTrim(std::string& text)
	{
		text.erase(std::find_if(text.rbegin(), text.rend(), [](const unsigned char input)
		{
			return std::isspace(input) == 0;
		}).base(), text.end());

		return text;
	}

	void Trim(std::string& text)
	{
		LTrim(RTrim(text));
	}

	std::string Convert(const std::wstring& text)
	{
		std::string result;
		result.reserve(text.size());

		for (const auto character : text)
		{
			result.push_back(static_cast<char>(character));
		}

		return result;
	}

	std::wstring Convert(const std::string& text)
	{
		std::wstring result;
		result.reserve(text.size());

		for (const auto character : text)
		{
			result.push_back(static_cast<wchar_t>(character));
		}

		return result;
	}

	std::string FormatTimeSpan(int milliseconds)
	{
		const int secondsTotal = milliseconds / 1000;
		const int seconds = secondsTotal % 60;
		const int minutesTotal = secondsTotal / 60;
		const int minutes = minutesTotal % 60;
		const int hoursTotal = minutesTotal / 60;

		return VA("%02d:%02d:%02d", hoursTotal, minutes, seconds);
	}

	std::string FormatBandwidth(std::size_t bytes, int milliseconds)
	{
		static const char* const units[] = { "B", "KB", "MB", "GB", "TB" };

		if (!milliseconds)
		{
			return "0.00 B/s";
		}

		double bytesPerSecond = (1000.0 / milliseconds) * static_cast<double>(bytes);

		std::size_t unit = 0;

		while (bytesPerSecond > 1000.0 && unit + 1 < ARRAYSIZE(units))
		{
			bytesPerSecond /= 1000.0;
			++unit;
		}

		return VA("%.2f %s/s", static_cast<float>(bytesPerSecond), units[unit]);
	}

	std::string DumpHex(const std::string& data, const std::string& separator)
	{
		std::string result;

		for (std::size_t i = 0; i < data.size(); ++i)
		{
			if (i > 0)
			{
				result.append(separator);
			}

			result.append(VA("%02X", static_cast<unsigned char>(data[i])));
		}

		return result;
	}

	std::string XOR(std::string text, char value)
	{
		for (auto& character : text)
		{
			character ^= value;
		}

		return text;
	}
}
