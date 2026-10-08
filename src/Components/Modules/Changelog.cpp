#include "STDInclude.hpp"

#include "Changelog.hpp"
#include "Dedicated.hpp"
#include "UIFeeder.hpp"

namespace Components
{
	std::mutex Changelog::mutex;
	std::vector<std::string> Changelog::lines;

	void Changelog::SetChangelog(const std::string& changelog)
	{
		std::lock_guard _(mutex);
		lines.clear();

		if (changelog.empty())
		{
			lines.emplace_back("^1Unable to get changelog.");
			return;
		}

		auto buffer = Utils::String::Split(changelog, '\n');

		for (auto& line : buffer)
		{
			Utils::String::Replace(line, "\r", "");
		}

		lines = buffer;
	}

	unsigned int Changelog::GetChangelogCount()
	{
		std::lock_guard _(mutex);
		return static_cast<unsigned int>(lines.size());
	}

	const char* Changelog::GetChangelogText(unsigned int item, [[maybe_unused]] int column)
	{
		std::lock_guard _(mutex);

		if (item < lines.size())
		{
			return Utils::String::VA("%s", lines[item].data());
		}

		return "";
	}

	void Changelog::SelectChangelog([[maybe_unused]] unsigned int index)
	{
	}

	Changelog::Changelog()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		UIFeeder::Add(62.0f, GetChangelogCount, GetChangelogText, SelectChangelog);
	}
}
