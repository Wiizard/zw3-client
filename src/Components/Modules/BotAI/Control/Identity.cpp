#include "STDInclude.hpp"

#include "Components/Modules/BotAI/Control/Identity.hpp"
#include "Components/Modules/BotAI/BotControl.hpp"
#include "Components/Modules/BotAI/Iw4.hpp"

namespace Components::BotAI
{
	static constexpr unsigned int cardIconCount = 297;
	static constexpr unsigned int cardTitleCount = 570;
	static constexpr unsigned int cardNameplateCount = 2;

	static constexpr const char* forcedBotNames[] = { "lil suislide", "GRIIM tB", "Billy" };
	static constexpr int forcedBotNameCount = sizeof(forcedBotNames) / sizeof(forcedBotNames[0]);

	static constexpr std::uint32_t fnvOffsetBasis = 2166136261u;
	static constexpr std::uint32_t fnvPrime = 16777619u;

	static const char* const defaultBotNames[] = { "Dismay", "xKaimma", "IReapZz", "Kazus", "xLXS",
		"Seuz", "Dyl tB", "majesty", "Crossy", "VanillaPWNZ", "elliot", "lukk", "wifibeachh", "lnphect",
		"HARRY", "EX1TE", "CHANFLEZOR", "Michael Oliver" , "TWINZ" , "Xevs" , "Toru", "rwhxo", "momo5502",
		"COKA", "Jebus", "xoxor4d", "ZeRoY", "yolarrydabomb", "Santahunter", "7avern", "digga d", "Tav", "Razza", "Sass", };

	static constexpr std::size_t maxNameLength = 15;

	static std::vector<std::string> botNames;
	static bool hasBotNames = false;

	static std::vector<int> namePermutation;
	static bool hasNamePermutation = false;

	static std::deque<std::string> roundNames;
	static constexpr int maxNameRounds = 64;

	static consteval std::uint32_t ExpectedForcedHash()
	{
		std::uint32_t hash = fnvOffsetBasis;
		for (const char* name : forcedBotNames)
		{
			for (const char* at = name; ; ++at)
			{
				hash = (hash ^ static_cast<unsigned char>(*at)) * fnvPrime;
				if (!*at)
				{
					break;
				}
			}
		}

		return hash;
	}

	bool HasForcedBotNames()
	{
		std::uint32_t hash = fnvOffsetBasis;
		for (const char* name : forcedBotNames)
		{
			for (const volatile char* at = name; ; ++at)
			{
				const char byte = *at;
				hash = (hash ^ static_cast<unsigned char>(byte)) * fnvPrime;
				if (!byte)
				{
					break;
				}
			}
		}

		return hash == ExpectedForcedHash();
	}

	static bool IsNameInList(const char* name, const char* const* names, int count)
	{
		for (int i = 0; i < count; ++i)
		{
			if (names[i] && std::strcmp(names[i], name) == 0)
			{
				return true;
			}
		}

		return false;
	}

	static void WriteDefaultNames(const char* path)
	{
		FILE* file = nullptr;
		if (fopen_s(&file, path, "w") != 0 || !file)
		{
			Print("bots: could not write %s\n", path);
			return;
		}

		std::fprintf(file, "// bot names: one a line, %zu characters at most, read at startup\n", maxNameLength);
		std::fprintf(file, "// the first %d bots always have the same names, these fill the rest\n\n", forcedBotNameCount);
		for (const char* name : defaultBotNames)
		{
			std::fprintf(file, "%s\n", name);
		}

		std::fclose(file);
		Print("bots: wrote %s\n", path);
	}

	static std::string CleanName(const char* line)
	{
		std::string name;
		for (const char* at = line; *at; ++at)
		{
			if (*at == '\\' || *at == '"' || *at == '\r' || *at == '\n')
			{
				continue;
			}

			name.push_back(*at);
		}

		Utils::String::Trim(name);
		if (name.size() > maxNameLength)
		{
			name.resize(maxNameLength);
			Utils::String::Trim(name);
		}

		return name;
	}

	void LoadBotNames()
	{
		if (hasBotNames)
		{
			return;
		}
		hasBotNames = true;

		char path[MAX_PATH * 2];
		if (BotsPath("botnames.txt", path, sizeof(path)))
		{
			FILE* file = nullptr;
			if (fopen_s(&file, path, "r") == 0 && file)
			{
				char line[512];
				bool isFirstLine = true;
				while (std::fgets(line, sizeof(line), file))
				{
					const char* text = line;
					if (isFirstLine && std::strncmp(text, "\xEF\xBB\xBF", 3) == 0)
					{
						text += 3;
					}
					isFirstLine = false;

					const std::string name = CleanName(text);
					if (name.empty() || name.starts_with("//"))
					{
						continue;
					}

					if (IsNameInList(name.c_str(), forcedBotNames, forcedBotNameCount))
					{
						continue;
					}

					botNames.push_back(name);
				}

				std::fclose(file);

				if (botNames.empty())
				{
					Print("bots: %s holds no names, using the built in ones\n", path);
				}
				else
				{
					Print("bots: %zu names loaded from %s\n", botNames.size(), path);
				}
			}
			else
			{
				WriteDefaultNames(path);
			}
		}

		if (botNames.empty())
		{
			botNames.assign(std::begin(defaultBotNames), std::end(defaultBotNames));
		}
	}

	static const char* RoundName(const std::string& base, int round)
	{
		const auto suffix = std::format(" {}", round);
		std::string name = base;

		if (name.size() + suffix.size() > maxNameLength)
		{
			name.resize(maxNameLength - suffix.size());
			Utils::String::Trim(name);
		}

		name += suffix;

		for (const auto& kept : roundNames)
		{
			if (kept == name)
			{
				return kept.c_str();
			}
		}

		return roundNames.emplace_back(std::move(name)).c_str();
	}

	static void BuildNamePermutation()
	{
		const int botNameCount = static_cast<int>(botNames.size());

		namePermutation.resize(botNames.size());
		for (int i = 0; i < botNameCount; ++i)
		{
			namePermutation[i] = i;
		}

		for (int i = botNameCount - 1; i > 0; --i)
		{
			const int j = static_cast<int>(NextRand() % static_cast<unsigned int>(i + 1));
			const int swap = namePermutation[i];
			namePermutation[i] = namePermutation[j];
			namePermutation[j] = swap;
		}

		hasNamePermutation = true;
	}

	static int RollRank()
	{
		const unsigned int roll = NextRand() % 100u;
		if (roll < 25u)
		{
			return 69;
		}

		if (roll < 60u)
		{
			return 45 + static_cast<int>(NextRand() % 24u);
		}

		if (roll < 85u)
		{
			return 20 + static_cast<int>(NextRand() % 25u);
		}

		return 3 + static_cast<int>(NextRand() % 19u);
	}

	static int RollPrestige(int rank)
	{
		if (rank < 30 && (NextRand() % 100u) < 60u)
		{
			return 0;
		}

		return static_cast<int>(NextRand() % 10u);
	}

	Identity IdentityFor(int ordinal)
	{
		LoadBotNames();
		if (!hasNamePermutation)
		{
			BuildNamePermutation();
		}

		if (ordinal < 0)
		{
			ordinal = 0;
		}

		const int botNameCount = static_cast<int>(botNames.size());
		const unsigned int id = static_cast<unsigned int>(namePermutation[ordinal % botNameCount]);

		Identity identity = {};
		identity.name = botNames[id % botNameCount].c_str();

		if (const int round = ordinal / botNameCount; round > 0)
		{
			identity.name = RoundName(botNames[id % botNameCount], round + 1);
		}

		identity.rank = RollRank();
		identity.prestige = RollPrestige(identity.rank);
		identity.cardIcon = static_cast<int>((id * 37 + 11) % cardIconCount);
		identity.cardTitle = static_cast<int>((id * 97 + 29) % cardTitleCount);
		identity.cardNameplate = static_cast<int>((id + 1) % cardNameplateCount);
		return identity;
	}

	Identity RollIdentity(const char* const* takenNames, int takenCount)
	{
		LoadBotNames();

		const char* chosenName = nullptr;
		for (const char* forcedName : forcedBotNames)
		{
			if (!IsNameInList(forcedName, takenNames, takenCount))
			{
				chosenName = forcedName;
				break;
			}
		}

		if (!chosenName)
		{
			const int botNameCount = static_cast<int>(botNames.size());
			const unsigned int start = NextRand() % static_cast<unsigned int>(botNameCount);

			for (int round = 1; round <= maxNameRounds && !chosenName; ++round)
			{
				for (int step = 0; step < botNameCount; ++step)
				{
					const int candidate = static_cast<int>((start + static_cast<unsigned int>(step)) % static_cast<unsigned int>(botNameCount));
					const char* name = botNames[candidate].c_str();

					if (round > 1)
					{
						name = RoundName(botNames[candidate], round);
					}

					if (!IsNameInList(name, takenNames, takenCount))
					{
						chosenName = name;
						break;
					}
				}
			}

			if (!chosenName)
			{
				chosenName = botNames[start].c_str();
			}
		}

		Identity identity = {};
		identity.name = chosenName;
		identity.rank = RollRank();
		identity.prestige = RollPrestige(identity.rank);
		identity.cardIcon = static_cast<int>(NextRand() % cardIconCount);
		identity.cardTitle = static_cast<int>(NextRand() % cardTitleCount);
		identity.cardNameplate = static_cast<int>(NextRand() % cardNameplateCount);
		return identity;
	}
}
