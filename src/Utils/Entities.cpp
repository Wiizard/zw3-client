#include "STDInclude.hpp"

namespace Utils
{
	std::string Entities::Build() const
	{
		std::string entityString;

		for (const auto& entity : this->entities)
		{
			entityString.append("{\n");

			for (const auto& [key, value] : entity)
			{
				entityString.append("\"");
				entityString.append(key);
				entityString.append("\" \"");
				entityString.append(value);
				entityString.append("\"\n");
			}

			entityString.append("}\n");
		}

		return entityString;
	}

	std::vector<std::string> Entities::GetModels()
	{
		std::vector<std::string> models;

		for (const auto& entity : this->entities)
		{
			const auto itr = entity.find("model");

			if (itr == entity.end())
			{
				continue;
			}

			const auto& model = itr->second;
			const bool isBrushModel = !model.empty() && (model[0] == '*' || model[0] == '?');
			const bool isTeamZone = model == "com_plasticcase_green_big_us_dirt"s;

			if (model.empty() || isBrushModel || isTeamZone)
			{
				continue;
			}

			if (std::find(models.begin(), models.end(), model) == models.end())
			{
				models.push_back(model);
			}
		}

		return models;
	}

	std::vector<std::string> Entities::GetWeapons()
	{
		std::vector<std::string> weapons;

		for (const auto& entity : this->entities)
		{
			const auto itr = entity.find("weaponinfo");

			if (itr == entity.end())
			{
				continue;
			}

			const auto& weapon = itr->second;

			if (!weapon.empty() && std::find(weapons.begin(), weapons.end(), weapon) == weapons.end())
			{
				weapons.push_back(weapon);
			}
		}

		return weapons;
	}

	void Entities::Parse(const std::string& buffer)
	{
		int parseState = PARSE_AWAIT_KEY;
		std::string key;
		std::string value;
		std::unordered_map<std::string, std::string> entity;

		for (const char character : buffer)
		{
			switch (character)
			{
			case '{':
			{
				entity.clear();
				break;
			}

			case '}':
			{
				this->entities.push_back(entity);
				entity.clear();
				break;
			}

			case '"':
			{
				if (parseState == PARSE_AWAIT_KEY)
				{
					key.clear();
					parseState = PARSE_READ_KEY;
				}
				else if (parseState == PARSE_READ_KEY)
				{
					parseState = PARSE_AWAIT_VALUE;
				}
				else if (parseState == PARSE_AWAIT_VALUE)
				{
					value.clear();
					parseState = PARSE_READ_VALUE;
				}
				else if (parseState == PARSE_READ_VALUE)
				{
					entity[String::ToLower(key)] = value;
					parseState = PARSE_AWAIT_KEY;
				}
				else
				{
					throw std::runtime_error("Parsing error!");
				}
				break;
			}

			default:
			{
				if (parseState == PARSE_READ_KEY)
				{
					key.push_back(character);
				}
				else if (parseState == PARSE_READ_VALUE)
				{
					value.push_back(character);
				}

				break;
			}
			}
		}
	}
}
