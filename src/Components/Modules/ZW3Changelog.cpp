#include "STDInclude.hpp"
#include "ZW3Changelog.hpp"
#include "Changelog.hpp"
#include "UIFeeder.hpp"
#include <Utils/WebIO.hpp>

namespace
{
	constexpr std::size_t MaxDetailLineChars = 105;

	std::string ParseYamlValue(const std::string& line, const std::size_t prefixLength)
	{
		auto value = line.substr(prefixLength);
		Utils::String::Trim(value);

		if (value.size() >= 2)
		{
			const auto first = value.front();
			const auto last = value.back();

			if ((first == '"' && last == '"') || (first == '\'' && last == '\''))
			{
				value = value.substr(1, value.size() - 2);
			}
		}

		Utils::String::Replace(value, "\\\"", "\"");
		Utils::String::Replace(value, "\\'", "'");

		return value;
	}

	bool StartsWith(const std::string& value, const std::string& prefix)
	{
		return value.rfind(prefix, 0) == 0;
	}

	std::string StripLeadingBulletPrefixes(std::string text)
	{
		Utils::String::Trim(text);

		bool changed = true;
		while (changed)
		{
			changed = false;

			if (StartsWith(text, "- "))
			{
				text = text.substr(2);
				Utils::String::Trim(text);
				changed = true;
			}
			else if (StartsWith(text, "* "))
			{
				text = text.substr(2);
				Utils::String::Trim(text);
				changed = true;
			}
			else if (StartsWith(text, "-- "))
			{
				text = text.substr(3);
				Utils::String::Trim(text);
				changed = true;
			}
		}

		return text;
	}

	std::string ToLower(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		return value;
	}

	bool IsCategoryLine(const std::string& text)
	{
		const auto lower = ToLower(text);

		return lower == "system"
			|| lower == "new"
			|| lower == "changed"
			|| lower == "changes"
			|| lower == "fixed"
			|| lower == "fixes"
			|| lower == "removed"
			|| lower == "security"
			|| lower == "other";
	}

	std::string FormatNoteLine(std::string rawText)
	{
		Utils::String::Trim(rawText);

		if (rawText.empty())
		{
			return "";
		}

		rawText = StripLeadingBulletPrefixes(rawText);

		if (rawText.empty())
		{
			return "";
		}

		if (IsCategoryLine(rawText))
		{
			return rawText;
		}

		return "- " + rawText;
	}

	void AddWrappedLine(std::vector<std::string>& out, std::string line)
	{
		Utils::String::Trim(line);

		if (line.empty())
		{
			out.emplace_back("");
			return;
		}

		if (line.size() <= MaxDetailLineChars)
		{
			out.emplace_back(line);
			return;
		}

		const std::string continuationPrefix = StartsWith(line, "- ") ? "  " : "  ";

		auto remaining = line;
		bool firstLine = true;

		while (remaining.size() > MaxDetailLineChars)
		{
			const auto limit = firstLine
				? MaxDetailLineChars
				: MaxDetailLineChars - continuationPrefix.size();

			auto splitAt = remaining.rfind(' ', limit);

			if (splitAt == std::string::npos || splitAt < 30)
			{
				splitAt = limit;
			}

			auto part = remaining.substr(0, splitAt);
			Utils::String::Trim(part);

			if (!part.empty())
			{
				if (firstLine)
				{
					out.emplace_back(part);
				}
				else
				{
					out.emplace_back(continuationPrefix + part);
				}
			}

			remaining = remaining.substr(splitAt);
			Utils::String::Trim(remaining);
			firstLine = false;
		}

		if (!remaining.empty())
		{
			if (firstLine)
			{
				out.emplace_back(remaining);
			}
			else
			{
				out.emplace_back(continuationPrefix + remaining);
			}
		}
	}
}

namespace Components
{
	std::mutex ZW3Changelog::Mutex;
	std::vector<ZW3Changelog::Entry> ZW3Changelog::Entries;
	std::size_t ZW3Changelog::SelectedIndex = 0;

	std::vector<ZW3Changelog::Entry> ZW3Changelog::ParseYamlEntries(const std::string& yaml)
	{
		std::vector<Entry> entries;

		if (yaml.empty())
		{
			return entries;
		}

		auto lines = Utils::String::Split(yaml, '\n');

		for (auto& line : lines)
		{
			Utils::String::Replace(line, "\r", "");
		}

		Entry current{};
		bool inNotes = false;

		auto flushEntry = [&]()
			{
				if (!current.Version.empty() || !current.Title.empty() || !current.Date.empty() || !current.Lines.empty())
				{
					while (!current.Lines.empty() && current.Lines.front().empty())
					{
						current.Lines.erase(current.Lines.begin());
					}

					while (!current.Lines.empty() && current.Lines.back().empty())
					{
						current.Lines.pop_back();
					}

					if (current.Version.empty())
					{
						current.Version = "Unknown";
					}

					entries.emplace_back(current);
				}

				current = {};
				inNotes = false;
			};

		for (const auto& line : lines)
		{
			if (line.rfind("version:", 0) == 0)
			{
				flushEntry();

				current.Version = ParseYamlValue(line, 8);
				continue;
			}

			if (line.rfind("title:", 0) == 0)
			{
				current.Title = ParseYamlValue(line, 6);
				continue;
			}

			if (line.rfind("date:", 0) == 0)
			{
				current.Date = ParseYamlValue(line, 5);
				continue;
			}

			if (line.rfind("notes:", 0) == 0)
			{
				inNotes = true;
				continue;
			}

			if (!inNotes)
			{
				continue;
			}

			if (!line.empty() && line[0] != ' ' && line[0] != '\t')
			{
				inNotes = false;
				continue;
			}

			const auto start = line.find_first_not_of(" \t");

			if (start == std::string::npos)
			{
				current.Lines.emplace_back("");
				continue;
			}

			auto text = line.substr(start);

			Utils::String::Replace(text, "\\n", "\n");

			if (text.find('\n') != std::string::npos)
			{
				auto subLines = Utils::String::Split(text, '\n');
				for (auto& subLine : subLines)
				{
					subLine = FormatNoteLine(subLine);
					if (!subLine.empty())
					{
						AddWrappedLine(current.Lines, subLine);
					}
					else
					{
						current.Lines.emplace_back("");
					}
				}
			}
			else
			{
				text = FormatNoteLine(text);
				if (!text.empty())
				{
					AddWrappedLine(current.Lines, text);
				}
			}
		}

		flushEntry();

		return entries;
	}

	void ZW3Changelog::SetEntries(std::vector<Entry> entries)
	{
		std::lock_guard _(Mutex);

		Entries = std::move(entries);
		SelectedIndex = 0;
	}

	unsigned int ZW3Changelog::GetVersionCount()
	{
		std::lock_guard _(Mutex);

		return static_cast<unsigned int>(Entries.size());
	}

	const char* ZW3Changelog::GetVersionText(unsigned int item, [[maybe_unused]] int column)
	{
		std::lock_guard _(Mutex);

		if (item >= Entries.size())
		{
			return "";
		}

		const auto& entry = Entries[item];

		return Utils::String::Format("{}", entry.Version);
	}

	void ZW3Changelog::SelectVersion(unsigned int index)
	{
		std::lock_guard _(Mutex);

		if (index < Entries.size())
		{
			SelectedIndex = index;
		}
	}

	unsigned int ZW3Changelog::GetDetailCount()
	{
		std::lock_guard _(Mutex);

		if (Entries.empty() || SelectedIndex >= Entries.size())
		{
			return 0;
		}

		const auto& entry = Entries[SelectedIndex];

		// Keep title behavior exactly as before: title is one normal detail line.
		const auto titleExtra = entry.Title.empty() ? 0u : 1u;
		const auto dateExtra = entry.Date.empty() ? 0u : 1u;
		const auto metaExtra = titleExtra + dateExtra;

		if (entry.Lines.empty())
		{
			return metaExtra + 1u;
		}

		return metaExtra + static_cast<unsigned int>(entry.Lines.size());
	}

	const char* ZW3Changelog::GetDetailText(unsigned int item, [[maybe_unused]] int column)
	{
		std::lock_guard _(Mutex);

		if (Entries.empty() || SelectedIndex >= Entries.size())
		{
			return "";
		}

		const auto& entry = Entries[SelectedIndex];

		// Keep title behavior exactly as before: show the title as the first detail line.
		if (!entry.Title.empty())
		{
			if (item == 0)
			{
				return Utils::String::Format("{}", entry.Title);
			}

			item -= 1;
		}

		// Date is only an extra detail line. Version/title display stays unchanged.
		if (!entry.Date.empty())
		{
			if (item == 0)
			{
				return Utils::String::Format("Date: {}", entry.Date);
			}

			item -= 1;
		}

		if (entry.Lines.empty() && item == 0)
		{
			return "No notes provided for this version.";
		}

		if (item < entry.Lines.size())
		{
			return Utils::String::Format("{}", entry.Lines[item]);
		}

		return "";
	}

	void ZW3Changelog::SelectDetail([[maybe_unused]] unsigned int index)
	{
	}

	void ZW3Changelog::Fetch([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		const auto yaml = Utils::WebIO("Call of Duty: Zombie Warfare 3")
			.setTimeout(5000)
			->get(ChangelogUrl);

		auto parsed = ParseYamlEntries(yaml);

		if (parsed.empty())
		{
			parsed.push_back({ "Unavailable", "", "", {"Changelog not available."} });
		}

		SetEntries(std::move(parsed));
		Changelog::SetChangelog("Loaded remote changelog");
	}

	ZW3Changelog::ZW3Changelog()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		UIScript::Add("loadZW3Changelog", Fetch);

		UIFeeder::Add(63.0f, GetVersionCount, GetVersionText, SelectVersion);
		UIFeeder::Add(64.0f, GetDetailCount, GetDetailText, SelectDetail);
	}
}
