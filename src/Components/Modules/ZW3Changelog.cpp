#include "STDInclude.hpp"

#include "ZW3Changelog.hpp"
#include "Changelog.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "UIFeeder.hpp"

namespace Components
{
	std::vector<ZW3Changelog::Entry> ZW3Changelog::entries;
	std::size_t ZW3Changelog::selectedIndex = 0;
	Dvar::Var ZW3Changelog::zw3_changelog_patch_title;
	Dvar::Var ZW3Changelog::zw3_changelog_patch_date;

	constexpr auto changelogUrl = "https://stats.zw3.eu/client/changelog.yaml";
	constexpr float versionFeeder = 63.0f;
	constexpr float detailFeeder = 64.0f;
	constexpr std::size_t maxDetailLineChars = 100;

	static std::string ParseYamlValue(const std::string& line, const std::size_t prefixLength)
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

	static std::string StripLeadingBulletPrefixes(std::string text)
	{
		Utils::String::Trim(text);

		bool didStrip = true;

		while (didStrip)
		{
			didStrip = false;

			for (const auto* prefix : { "- ", "* ", "-- " })
			{
				if (Utils::String::StartsWith(text, prefix))
				{
					text = text.substr(std::strlen(prefix));
					Utils::String::Trim(text);
					didStrip = true;
					break;
				}
			}
		}

		return text;
	}

	static bool IsCategoryLine(const std::string& text)
	{
		const auto lower = Utils::String::ToLower(text);

		return lower == "system"
			|| lower == "new"
			|| lower == "changed"
			|| lower == "changes"
			|| lower == "fixed"
			|| lower == "fixes"
			|| lower == "removed"
			|| lower == "security"
			|| lower == "notice"
			|| lower == "other";
	}

	static std::string FormatNoteLine(std::string rawText)
	{
		rawText = StripLeadingBulletPrefixes(rawText);

		if (rawText.empty() || IsCategoryLine(rawText))
		{
			return rawText;
		}

		return "  - " + rawText;
	}

	static void AddWrappedLine(std::vector<std::string>& lines, std::string line)
	{
		Utils::String::Trim(line);

		if (line.size() <= maxDetailLineChars)
		{
			lines.emplace_back(line);
			return;
		}

		const std::string continuationPrefix = "  ";

		auto remaining = line;
		bool isFirstLine = true;

		while (remaining.size() > maxDetailLineChars)
		{
			auto limit = maxDetailLineChars;

			if (!isFirstLine)
			{
				limit -= continuationPrefix.size();
			}

			auto splitAt = remaining.rfind(' ', limit);

			if (splitAt == std::string::npos || splitAt < 30)
			{
				splitAt = limit;
			}

			auto part = remaining.substr(0, splitAt);
			Utils::String::Trim(part);

			if (!part.empty())
			{
				if (isFirstLine)
				{
					lines.emplace_back(part);
				}
				else
				{
					lines.emplace_back(continuationPrefix + part);
				}
			}

			remaining = remaining.substr(splitAt);
			Utils::String::Trim(remaining);
			isFirstLine = false;
		}

		if (remaining.empty())
		{
			return;
		}

		if (isFirstLine)
		{
			lines.emplace_back(remaining);
		}
		else
		{
			lines.emplace_back(continuationPrefix + remaining);
		}
	}

	static void AddNoteLine(std::vector<std::string>& lines, const std::string& rawText)
	{
		const auto text = FormatNoteLine(rawText);

		if (text.empty())
		{
			lines.emplace_back("");
			return;
		}

		if (IsCategoryLine(StripLeadingBulletPrefixes(text)) && !lines.empty())
		{
			lines.emplace_back("");
		}

		AddWrappedLine(lines, text);
	}

	std::vector<ZW3Changelog::Entry> ZW3Changelog::ParseYamlEntries(const std::string& yaml)
	{
		std::vector<Entry> parsed;

		if (yaml.empty())
		{
			return parsed;
		}

		auto yamlLines = Utils::String::Split(yaml, '\n');

		for (auto& line : yamlLines)
		{
			Utils::String::Replace(line, "\r", "");
		}

		Entry current{};
		bool isInNotes = false;

		const auto flushEntry = [&]
		{
			if (!current.version.empty() || !current.title.empty() || !current.date.empty() || !current.lines.empty())
			{
				while (!current.lines.empty() && current.lines.front().empty())
				{
					current.lines.erase(current.lines.begin());
				}

				while (!current.lines.empty() && current.lines.back().empty())
				{
					current.lines.pop_back();
				}

				if (current.version.empty())
				{
					current.version = "Unknown";
				}

				parsed.emplace_back(current);
			}

			current = {};
			isInNotes = false;
		};

		for (const auto& line : yamlLines)
		{
			if (line.starts_with("version:"))
			{
				flushEntry();
				current.version = ParseYamlValue(line, 8);
				continue;
			}

			if (line.starts_with("title:"))
			{
				current.title = ParseYamlValue(line, 6);
				continue;
			}

			if (line.starts_with("date:"))
			{
				current.date = FormatDate(ParseYamlValue(line, 5));
				continue;
			}

			if (line.starts_with("notes:"))
			{
				isInNotes = true;
				continue;
			}

			if (!isInNotes)
			{
				continue;
			}

			if (!line.empty() && line[0] != ' ' && line[0] != '\t')
			{
				isInNotes = false;
				continue;
			}

			const auto start = line.find_first_not_of(" \t");

			if (start == std::string::npos)
			{
				current.lines.emplace_back("");
				continue;
			}

			auto text = line.substr(start);
			Utils::String::Replace(text, "\\n", "\n");

			for (const auto& subLine : Utils::String::Split(text, '\n'))
			{
				AddNoteLine(current.lines, subLine);
			}
		}

		flushEntry();

		return parsed;
	}

	std::string ZW3Changelog::FormatDate(const std::string& date)
	{
		std::tm time{};
		std::istringstream stream(date);
		stream >> std::get_time(&time, "%Y-%m-%d");

		if (stream.fail())
		{
			return date;
		}

		char buffer[64]{};
		std::strftime(buffer, sizeof(buffer), "%d %B %Y", &time);

		return buffer;
	}

	void ZW3Changelog::ShowSelectedTitle()
	{
		zw3_changelog_patch_title.Set(entries[selectedIndex].title);
		zw3_changelog_patch_date.Set(entries[selectedIndex].date);
	}

	void ZW3Changelog::SetEntries(std::vector<Entry> newEntries)
	{
		entries = std::move(newEntries);
		selectedIndex = 0;

		if (entries.empty())
		{
			zw3_changelog_patch_title.Set("");
			zw3_changelog_patch_date.Set("");
		}
		else
		{
			ShowSelectedTitle();
		}

		UIFeeder::Select(versionFeeder, 0, true);
		UIFeeder::Select(detailFeeder, 0, true);
	}

	unsigned int ZW3Changelog::GetVersionCount()
	{
		if (entries.empty())
		{
			return 0;
		}

		return static_cast<unsigned int>(entries.size() + 1);
	}

	const char* ZW3Changelog::GetVersionText(const unsigned int item, [[maybe_unused]] const int column)
	{
		if (entries.empty())
		{
			return "";
		}

		if (item == 0)
		{
			return Utils::String::Format("{} (Latest)", entries[0].version);
		}

		if (item == 1)
		{
			return "--- Older Patches ---";
		}

		const auto entryIndex = item - 1;

		if (entryIndex >= entries.size())
		{
			return "";
		}

		return Utils::String::Format("{}", entries[entryIndex].version);
	}

	void ZW3Changelog::SelectVersion(const unsigned int index)
	{
		if (entries.empty())
		{
			return;
		}

		if (index == 0)
		{
			selectedIndex = 0;
			UIFeeder::Select(detailFeeder, 0, true);
			ShowSelectedTitle();
			return;
		}

		if (index == 1)
		{
			if (selectedIndex == 0 && entries.size() > 1)
			{
				selectedIndex = 1;
				UIFeeder::Select(versionFeeder, 2, true);
			}
			else
			{
				selectedIndex = 0;
				UIFeeder::Select(versionFeeder, 0, true);
			}

			UIFeeder::Select(detailFeeder, 0, true);
			ShowSelectedTitle();
			return;
		}

		const auto entryIndex = index - 1;

		if (entryIndex < entries.size())
		{
			selectedIndex = entryIndex;
			UIFeeder::Select(detailFeeder, 0, true);
			ShowSelectedTitle();
		}
	}

	unsigned int ZW3Changelog::GetDetailCount()
	{
		if (selectedIndex >= entries.size())
		{
			return 0;
		}

		const auto& entry = entries[selectedIndex];

		if (entry.lines.empty())
		{
			return 1;
		}

		return static_cast<unsigned int>(entry.lines.size());
	}

	const char* ZW3Changelog::GetDetailText(const unsigned int item, const int column)
	{
		if (selectedIndex >= entries.size())
		{
			return "";
		}

		const auto& entry = entries[selectedIndex];

		if (entry.lines.empty() && item == 0)
		{
			if (column != 0)
			{
				return "";
			}

			return "No notes provided for this version.";
		}

		if (item >= entry.lines.size() || entry.lines[item].empty())
		{
			return "";
		}

		const auto& line = entry.lines[item];

		auto cleanLine = line;
		Utils::String::Trim(cleanLine);

		if (IsCategoryLine(StripLeadingBulletPrefixes(cleanLine)))
		{
			return Utils::String::Format("^1{}", Utils::String::ToUpper(cleanLine));
		}

		return Utils::String::Format("             ^7{}", line);
	}

	void ZW3Changelog::SelectDetail([[maybe_unused]] const unsigned int index)
	{
	}

	void ZW3Changelog::Fetch([[maybe_unused]] const UIScript::Token& token)
	{
		const auto yaml = Utils::WebIO("Call of Duty: Zombie Warfare 3", changelogUrl).SetTimeout(5000)->Get();

		auto parsed = ParseYamlEntries(yaml);

		if (parsed.empty())
		{
			parsed.push_back({ "Unavailable", "", "", { "Changelog not available." } });
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

		Events::OnDvarInit([]
		{
			zw3_changelog_patch_title = Dvar::Register("zw3_changelog_patch_title", "", Game::DVAR_INIT, "Title of the selected patch");
			zw3_changelog_patch_date = Dvar::Register("zw3_changelog_patch_date", "", Game::DVAR_INIT, "Date of the selected patch");
		});

		UIScript::Add("loadZW3Changelog", Fetch);

		UIFeeder::Add(versionFeeder, GetVersionCount, GetVersionText, SelectVersion);
		UIFeeder::Add(detailFeeder, GetDetailCount, GetDetailText, SelectDetail);
	}
}
