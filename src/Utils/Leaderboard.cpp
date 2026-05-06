#include <Utils/WebIO.hpp>

#include <Components/Modules/Dvar.hpp>
#include <Components/Modules/Events.hpp>
#include <Components/Modules/Logger.hpp>
#include <Components/Modules/Toast.hpp>
#include <Components/Modules/UIFeeder.hpp>
#include <Components/Modules/UIScript.hpp>

#include "Leaderboard.hpp"

#include <rapidjson/document.h>

namespace Components
{
	std::vector<Leaderboard::Entry> Leaderboard::Entries;
	Dvar::Var Leaderboard::UILeaderboardStatus;
	Dvar::Var Leaderboard::UILeaderboardFirst;
	Dvar::Var Leaderboard::UILeaderboardMap;
	Dvar::Var Leaderboard::UILeaderboardPage;
	int Leaderboard::CurrentPage = DefaultPage;
	bool Leaderboard::HasNextPage = false;

	const char* Leaderboard::GetApiKey()
	{
		return "zw3_YbEL1IsJUEGPW6cy1wN8q35WsLBLXTYp548WSdUfVJDALM6drgFxM3KTmAeIigfxiylwjlraODV8Fr7AzyVcWSKoQBH3ejJ07A0GCnaHq27ZVy5sKed6VoD55l3eS0N1x762nHcPCYUySB5F9oS92ObaxmYzigGAYlU9TiRiiibs28A3TJtjUeosaUbrTWPcd6EJAu2vqOdRyRzOL5mmzgN5EKZ9NDprTmNq3v98pWvf6HeoRFkk6RF9AxllgEMI";
	}

	static const char* GetJsonString(const rapidjson::Value& object, const char* key, const char* fallback = "")
	{
		if (object.HasMember(key) && object[key].IsString())
		{
			return object[key].GetString();
		}

		return fallback;
	}

	static int GetJsonInt(const rapidjson::Value& object, const char* key, int fallback = 0)
	{
		if (!object.HasMember(key)) return fallback;
		if (object[key].IsInt()) return object[key].GetInt();
		if (object[key].IsNumber()) return static_cast<int>(object[key].GetDouble());
		return fallback;
	}

	static float GetJsonFloat(const rapidjson::Value& object, const char* key, float fallback = 0.0f)
	{
		if (!object.HasMember(key)) return fallback;
		if (object[key].IsNumber()) return static_cast<float>(object[key].GetDouble());
		return fallback;
	}

	static const char* FormatSeconds(float seconds)
	{
		const auto total = std::max(0, static_cast<int>(seconds));
		const auto hours = total / 3600;
		const auto minutes = (total % 3600) / 60;
		const auto secs = total % 60;

		if (hours > 0)
		{
			return Utils::String::VA("%02i:%02i:%02i", hours, minutes, secs);
		}

		return Utils::String::VA("%02i:%02i", minutes, secs);
	}

	static std::string NormalizePlayerName(std::string player)
	{
		Utils::String::Trim(player);
		std::transform(player.begin(), player.end(), player.begin(), [](const unsigned char c)
			{
				return static_cast<char>(std::tolower(c));
			});

		return player;
	}

	bool Leaderboard::IsBetterEntry(const Entry& candidate, const Entry& current)
	{
		if (candidate.round != current.round) return candidate.round > current.round;
		if (candidate.score != current.score) return candidate.score > current.score;
		if (candidate.kills != current.kills) return candidate.kills > current.kills;
		if (candidate.downs != current.downs) return candidate.downs < current.downs;
		if (candidate.revives != current.revives) return candidate.revives > current.revives;
		if (candidate.exfiltrated != current.exfiltrated) return candidate.exfiltrated > current.exfiltrated;
		return candidate.time < current.time;
	}

	void Leaderboard::UpdatePageDvar()
	{
		UILeaderboardPage.set(Utils::String::VA("Page %i%s", CurrentPage, HasNextPage ? "" : " (last)"));
	}

	std::string Leaderboard::GetCurrentMapName()
	{
		const auto* mapname = Game::Dvar_FindVar("mapname");
		if (mapname && mapname->current.string && *mapname->current.string)
		{
			return mapname->current.string;
		}

		if (Game::sv_mapname && *Game::sv_mapname && (*Game::sv_mapname)->current.string && *(*Game::sv_mapname)->current.string)
		{
			return (*Game::sv_mapname)->current.string;
		}

		if (Game::ui_mapname && *Game::ui_mapname && (*Game::ui_mapname)->current.string && *(*Game::ui_mapname)->current.string)
		{
			return (*Game::ui_mapname)->current.string;
		}

		return {};
	}

	std::string Leaderboard::UrlEncode(const std::string& value)
	{
		std::string encoded;
		encoded.reserve(value.size());

		constexpr auto* hex = "0123456789ABCDEF";
		for (const auto c : value)
		{
			const auto byte = static_cast<unsigned char>(c);
			if (std::isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~')
			{
				encoded.push_back(static_cast<char>(byte));
			}
			else
			{
				encoded.push_back('%');
				encoded.push_back(hex[byte >> 4]);
				encoded.push_back(hex[byte & 0x0F]);
			}
		}

		return encoded;
	}

	void Leaderboard::Refresh([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		Entries.clear();
		UpdatePageDvar();

		const auto mapName = GetCurrentMapName();
		UILeaderboardMap.set(mapName.empty() ? "Unknown" : mapName);

		if (mapName.empty())
		{
			UILeaderboardStatus.set("^1Could not detect current map.");
			UILeaderboardFirst.set("The mapname, sv_mapname, and ui_mapname dvars are empty.");
			Toast::Show("cardicon_redhand", "^1Stats", "Could not detect the current map for stats lookup.", 5000);
			return;
		}

		UILeaderboardStatus.set(Utils::String::VA("Loading stats for %s, page %i...", mapName.data(), CurrentPage));
		UILeaderboardFirst.set("Waiting for stats API response...");

		const auto url = std::format(
			"https://stats.zw3.eu/matches/data?page={}&per_page={}&period={}&map={}",
			CurrentPage,
			DefaultPerPage,
			DefaultPeriod,
			UrlEncode(mapName));

		Utils::WebIO::params headers;
		headers["Content-Type"] = "application/json";
		const auto reply = Utils::WebIO("zw3-map-stats", url).setTimeout(5000)->get(headers);

		if (reply.empty())
		{
			UILeaderboardStatus.set("^1Could not load stats.");
			UILeaderboardFirst.set(Utils::String::VA("No HTTP response for map %s, page %i.", mapName.data(), CurrentPage));
			Toast::Show("cardicon_redhand", "^1Stats", "Could not get a response from the stats API.", 5000);
			return;
		}

		ParseResponse(reply, mapName);
	}

	void Leaderboard::RefreshFirstPage([[maybe_unused]] const UIScript::Token& token, const Game::uiInfo_s* info)
	{
		CurrentPage = DefaultPage;
		Refresh(token, info);
	}

	void Leaderboard::PreviousPage([[maybe_unused]] const UIScript::Token& token, const Game::uiInfo_s* info)
	{
		if (CurrentPage <= DefaultPage)
		{
			UILeaderboardStatus.set("^3Already on the first stats page.");
			UpdatePageDvar();
			return;
		}

		--CurrentPage;
		Refresh(token, info);
	}

	void Leaderboard::NextPage([[maybe_unused]] const UIScript::Token& token, const Game::uiInfo_s* info)
	{
		if (!HasNextPage)
		{
			UILeaderboardStatus.set("^3No next stats page loaded by the API.");
			UpdatePageDvar();
			return;
		}

		++CurrentPage;
		Refresh(token, info);
	}

	void Leaderboard::ParseResponse(const std::string& response, const std::string& mapName)
	{
		Entries.clear();
		HasNextPage = false;

		rapidjson::Document doc{};
		doc.Parse(response);

		if (doc.HasParseError())
		{
			UILeaderboardStatus.set("^1Stats response parse error.");
			UILeaderboardFirst.set("The stats API returned invalid JSON.");
			UpdatePageDvar();
			return;
		}

		if (!doc.IsObject())
		{
			UILeaderboardStatus.set("^1Invalid stats response format.");
			UILeaderboardFirst.set("Expected a JSON object from /matches/data.");
			UpdatePageDvar();
			return;
		}

		if (!doc.HasMember("items") || !doc["items"].IsArray())
		{
			UILeaderboardStatus.set("^1Stats response has no items.");
			UILeaderboardFirst.set("JSON has no items array.");
			UpdatePageDvar();
			return;
		}

		const auto& items = doc["items"];
		HasNextPage = items.Size() >= static_cast<rapidjson::SizeType>(DefaultPerPage);

		std::unordered_map<std::string, Entry> bestEntries;

		for (const auto& item : items.GetArray())
		{
			if (!item.IsObject())
			{
				continue;
			}

			Entry entry{};
			entry.guid = GetJsonString(item, "guid");
			entry.player = GetJsonString(item, "name", GetJsonString(item, "player", "Unknown"));
			entry.map = GetJsonString(item, "map", mapName.data());
			entry.round = GetJsonInt(item, "round");
			entry.zombiemode = GetJsonString(item, "zombiemode");
			entry.players = GetJsonInt(item, "players");
			entry.playerRank = GetJsonString(item, "rank");
			entry.score = GetJsonInt(item, "score");
			entry.kills = GetJsonInt(item, "kills");
			entry.downs = GetJsonInt(item, "downs");
			entry.revives = GetJsonInt(item, "revives");
			entry.exfiltrated = GetJsonInt(item, "exfiltrated");
			entry.time = GetJsonFloat(item, "time");
			entry.version = GetJsonString(item, "version");
			entry.uploadedAt = GetJsonString(item, "uploaded_at", GetJsonString(item, "uploadedAt"));
			entry.id = GetJsonInt(item, "id");

			if (entry.player.empty()) entry.player = "Unknown";
			if (entry.map.empty()) entry.map = mapName;

			const auto normalizedName = NormalizePlayerName(entry.player);
			auto duplicate = bestEntries.find(normalizedName);
			if (duplicate == bestEntries.end() || IsBetterEntry(entry, duplicate->second))
			{
				bestEntries[normalizedName] = entry;
			}
		}

		Entries.reserve(bestEntries.size());
		for (auto& entry : bestEntries)
		{
			Entries.push_back(std::move(entry.second));
		}

		std::sort(Entries.begin(), Entries.end(), [](const Entry& a, const Entry& b)
			{
				return IsBetterEntry(a, b);
			});

		UpdatePageDvar();

		if (Entries.empty())
		{
			UILeaderboardStatus.set(Utils::String::VA("^3No stats found for %s on page %i.", mapName.data(), CurrentPage));
			UILeaderboardFirst.set("Try refreshing after players have uploaded stats for this map.");
		}
		else
		{
			const auto& first = Entries[0];
			const std::string firstTime = FormatSeconds(first.time);
			const auto removedDuplicates = static_cast<unsigned int>(items.Size() - Entries.size());
			UILeaderboardStatus.set(Utils::String::VA("^2Loaded %u unique players for %s, page %i. Removed %u duplicate rows.",
				static_cast<unsigned int>(Entries.size()),
				mapName.data(),
				CurrentPage,
				removedDuplicates));
			UILeaderboardFirst.set(Utils::String::VA("Best: %s | Round %i | Score %i | Kills %i | Time %s",
				first.player.data(),
				first.round,
				first.score,
				first.kills,
				firstTime.data()));
		}
	}

	unsigned int Leaderboard::GetEntryCount()
	{
		return Entries.empty() ? 1u : static_cast<unsigned int>(Entries.size());
	}

	const char* Leaderboard::GetEntryText(unsigned int index, int column)
	{
		if (Entries.empty())
		{
			switch (column)
			{
			case 0:
				return "--";
			case 1:
				return "No stats loaded";
			case 3:
				return UILeaderboardStatus.get<const char*>();
			default:
				return "";
			}
		}

		if (index >= Entries.size())
		{
			return "";
		}

		const auto& entry = Entries[index];

		switch (column)
		{
		case 0:
			return entry.player.data();
		case 1:
			return Utils::String::VA("%i", entry.round);
		case 2:
			return entry.zombiemode.data();
		case 3:
			return Utils::String::VA("%i", entry.players);
		case 4:
			return Utils::String::VA("%i", entry.score);
		case 5:
			return Utils::String::VA("%i", entry.kills);
		case 6:
			return Utils::String::VA("%i", entry.downs);
		case 7:
			return Utils::String::VA("%i", entry.revives);
		case 8:
			return Utils::String::VA("%i", entry.exfiltrated);
		case 9:
			return FormatSeconds(entry.time);
		default:
			return "";
		}
	}

	void Leaderboard::SelectEntry([[maybe_unused]] unsigned int index)
	{
		// Optional later: open player details for the selected stats row.
	}

	Leaderboard::Leaderboard()
	{
		if (Dedicated::IsEnabled()) return;

		Events::OnDvarInit([]
			{
				UILeaderboardStatus = Dvar::Register<const char*>("ui_leaderboard_status", "", Game::DVAR_NONE, "Current status for the map stats menu.");
				UILeaderboardFirst = Dvar::Register<const char*>("ui_leaderboard_first", "", Game::DVAR_NONE, "Debug line for the first returned map stats row.");
				UILeaderboardMap = Dvar::Register<const char*>("ui_leaderboard_map", "", Game::DVAR_NONE, "Current map used by the map stats menu.");
				UILeaderboardPage = Dvar::Register<const char*>("ui_leaderboard_page", "Page 1", Game::DVAR_NONE, "Current page used by the map stats menu.");
			});

		UIFeeder::Add(FeederId, Leaderboard::GetEntryCount, Leaderboard::GetEntryText, Leaderboard::SelectEntry);
		UIScript::Add("RefreshLeaderboard", Leaderboard::RefreshFirstPage);
		UIScript::Add("RefreshLeaderboardPage", Leaderboard::Refresh);
		UIScript::Add("PreviousLeaderboardPage", Leaderboard::PreviousPage);
		UIScript::Add("NextLeaderboardPage", Leaderboard::NextPage);
	}

	Leaderboard::~Leaderboard()
	{
		Entries.clear();
	}
}
