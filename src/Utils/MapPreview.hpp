#pragma once

namespace Utils::MapPreview
{
	inline std::string Normalize(std::string_view name)
	{
		std::string result(name);

		for (char& character : result)
		{
			if (character >= 'A' && character <= 'Z')
			{
				character += 'a' - 'A';
			}
		}

		return result;
	}

	inline bool IsMultiplayer(std::string_view name)
	{
		if (name.size() < 3)
		{
			return false;
		}

		const bool isLetterM = name[0] == 'm' || name[0] == 'M';
		const bool isLetterP = name[1] == 'p' || name[1] == 'P';

		return isLetterM && isLetterP && name[2] == '_';
	}

	inline std::vector<std::string> ImageNames(std::string_view name)
	{
		std::string map = Normalize(name);

		if (map.empty() || IsMultiplayer(map))
		{
			return {};
		}

		std::vector<std::string> names = { "preview_" + map, "loadscreen_" + map };

		if (map.find('_') != std::string::npos)
		{
			std::erase(map, '_');
			names.push_back("preview_" + map);
			names.push_back("loadscreen_" + map);
		}

		return names;
	}

	inline bool MatchesMaterial(std::string_view material, std::string_view map)
	{
		if (map.empty() || IsMultiplayer(map))
		{
			return false;
		}

		if (material == "$levelbriefing" || material == "level_loadscreen" || material == "loading_image")
		{
			return true;
		}

		if (material.starts_with("loadscreen_"))
		{
			return material.substr(11) == map;
		}

		if (material.starts_with("preview_"))
		{
			return material.substr(8) == map;
		}

		return false;
	}
}
