#include "STDInclude.hpp"

#include <rapidjson/document.h>

#include "Leaderboard.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Localization.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "Toast.hpp"
#include "UIFeeder.hpp"

namespace Components
{
	std::vector<Leaderboard::Entry> Leaderboard::entries;

	Dvar::Var Leaderboard::zw3_leaderboard_map;
	Dvar::Var Leaderboard::zw3_leaderboard_page;
	Dvar::Var Leaderboard::zw3_leaderboard_player_status;
	Dvar::Var Leaderboard::zw3_leaderboard_mapname_display;
	Dvar::Var Leaderboard::zw3_leaderboard_can_prev;
	Dvar::Var Leaderboard::zw3_leaderboard_can_next;

	int Leaderboard::currentOffset = 0;
	int Leaderboard::displayedOffset = 0;
	int Leaderboard::nextOffset = -1;
	int Leaderboard::totalItems = -1;
	bool Leaderboard::hasNextPage = false;
	bool Leaderboard::isLoading = false;
	unsigned int Leaderboard::requestSerial = 0;
	std::string Leaderboard::currentMap;
	int Leaderboard::lastKnownRank = 0;
	bool Leaderboard::isSearching = false;

	constexpr float leaderboardFeeder = 70.0f;
	constexpr int defaultLimit = 10;
	constexpr auto bestRoundsUrl = "https://stats.zw3.eu/leaderboard/best-rounds";

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

	static int GetJsonInt(const rapidjson::Value& object, const char* key, const int fallback = 0)
	{
		if (!object.HasMember(key) || object[key].IsNull())
		{
			return fallback;
		}

		if (object[key].IsInt())
		{
			return object[key].GetInt();
		}

		if (object[key].IsNumber())
		{
			return static_cast<int>(object[key].GetDouble());
		}

		return fallback;
	}

	static float GetJsonFloat(const rapidjson::Value& object, const char* key, const float fallback = 0.0f)
	{
		if (!object.HasMember(key) || object[key].IsNull() || !object[key].IsNumber())
		{
			return fallback;
		}

		return static_cast<float>(object[key].GetDouble());
	}

	static const char* FormatSeconds(const float seconds)
	{
		const auto total = std::max(0, static_cast<int>(seconds));
		const auto hours = total / 3600;
		const auto minutes = (total % 3600) / 60;

		if (hours > 0)
		{
			return Utils::String::VA("%ih %im", hours, minutes);
		}

		return Utils::String::VA("%im", minutes);
	}

	static std::string UrlEncode(const std::string& text)
	{
		constexpr auto hex = "0123456789ABCDEF";

		std::string encoded;
		encoded.reserve(text.size());

		for (const auto character : text)
		{
			const auto byte = static_cast<unsigned char>(character);

			if (std::isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '~')
			{
				encoded.push_back(static_cast<char>(byte));
				continue;
			}

			encoded.push_back('%');
			encoded.push_back(hex[byte >> 4]);
			encoded.push_back(hex[byte & 0x0F]);
		}

		return encoded;
	}

	void Leaderboard::UpdateButtonDvars()
	{
		const bool hasEntries = !entries.empty();

		zw3_leaderboard_can_prev.Set(!isLoading && hasEntries && displayedOffset > 0);
		zw3_leaderboard_can_next.Set(!isLoading && hasEntries && hasNextPage && nextOffset >= 0);
	}

	void Leaderboard::UpdatePageDvar()
	{
		const auto page = (displayedOffset / defaultLimit) + 1;

		if (totalItems >= 0)
		{
			const auto totalPages = std::max(1, (totalItems + defaultLimit - 1) / defaultLimit);
			zw3_leaderboard_page.Set(Utils::String::VA("Page %i of %i", page, totalPages));
		}
		else if (!hasNextPage)
		{
			zw3_leaderboard_page.Set(Utils::String::VA("Page %i of %i", page, page));
		}
		else
		{
			zw3_leaderboard_page.Set(Utils::String::VA("Page %i", page));
		}

		UpdateButtonDvars();
	}

	std::string Leaderboard::GetCurrentMapName()
	{
		for (const auto* name : { "mapname", "ui_mapname" })
		{
			const auto* dvar = Game::Dvar_FindVar(name);

			if (dvar && dvar->current.string && *dvar->current.string)
			{
				return dvar->current.string;
			}
		}

		return {};
	}

	void Leaderboard::UpdateMapDisplayDvar(const std::string& rawMap)
	{
		if (!zw3_leaderboard_mapname_display.IsValid())
		{
			return;
		}

		const char* displayName = Game::UI_GetMapDisplayName(rawMap.data());

		if (!displayName || !*displayName || rawMap == displayName)
		{
			displayName = Localization::LocalizeMapName(rawMap.data());
		}

		zw3_leaderboard_mapname_display.Set(displayName);
	}

	void Leaderboard::StartRefresh(const int offset)
	{
		const auto mapName = GetCurrentMapName();

		if (mapName.empty())
		{
			++requestSerial;

			entries.clear();
			isLoading = false;
			hasNextPage = false;
			nextOffset = -1;
			totalItems = 0;
			currentOffset = 0;
			displayedOffset = 0;

			zw3_leaderboard_map.Set("Unknown");
			zw3_leaderboard_player_status.Set("Could not detect current map.");
			UpdatePageDvar();

			Toast::Show("cardicon_redhand", "^1Leaderboard", "Could not detect the current map for leaderboard lookup.", 5000);
			return;
		}

		zw3_leaderboard_map.Set(mapName);
		UpdateMapDisplayDvar(mapName);

		if (mapName != currentMap)
		{
			lastKnownRank = 0;
			currentMap = mapName;
			currentOffset = 0;
			displayedOffset = 0;
			totalItems = -1;
		}

		const int requestedOffset = std::max(0, offset);
		const auto requestId = ++requestSerial;

		currentOffset = requestedOffset;

		entries.clear();
		isLoading = true;
		hasNextPage = false;
		nextOffset = -1;

		if (requestedOffset == 0)
		{
			totalItems = -1;
		}

		zw3_leaderboard_player_status.Set("Loading leaderboard status...");
		UpdatePageDvar();

		auto url = std::format("{}?limit={}&map={}", bestRoundsUrl, defaultLimit, UrlEncode(mapName));

		if (requestedOffset > 0)
		{
			url += std::format("&offset={}", requestedOffset);
		}

		Scheduler::Once([requestId, url]
		{
			const Utils::WebIO::Params headers = { { "Content-Type", "application/json" } };
			const auto reply = Utils::WebIO("zw3-best-rounds", url).SetTimeout(5000)->Get(headers);

			Scheduler::Once([requestId, reply]
			{
				if (requestId != requestSerial)
				{
					return;
				}

				isLoading = false;

				if (reply.empty())
				{
					entries.clear();
					totalItems = 0;
					hasNextPage = false;
					nextOffset = -1;

					zw3_leaderboard_player_status.Set("Could not get a response from the stats API.");
					UpdatePageDvar();

					Toast::Show("cardicon_redhand", "^1Leaderboard", "Could not get a response from the stats API.", 5000);
					return;
				}

				ParseResponse(reply);
			}, Scheduler::Pipeline::MAIN);
		}, Scheduler::Pipeline::ASYNC);
	}

	void Leaderboard::RefreshFirstPage([[maybe_unused]] const UIScript::Token& token)
	{
		if (isLoading)
		{
			return;
		}

		if (displayedOffset != 0 || entries.empty() || GetCurrentMapName() != currentMap)
		{
			StartRefresh(0);
		}
	}

	void Leaderboard::PreviousPage([[maybe_unused]] const UIScript::Token& token)
	{
		if (isLoading || entries.empty() || displayedOffset <= 0)
		{
			UpdateButtonDvars();
			return;
		}

		StartRefresh(std::max(0, displayedOffset - defaultLimit));
	}

	void Leaderboard::NextPage([[maybe_unused]] const UIScript::Token& token)
	{
		if (isLoading || entries.empty() || !hasNextPage || nextOffset < 0)
		{
			UpdateButtonDvars();
			return;
		}

		StartRefresh(nextOffset);
	}

	void Leaderboard::ParseResponse(const std::string& response)
	{
		entries.clear();
		hasNextPage = false;
		nextOffset = -1;
		isLoading = false;

		rapidjson::Document document{};
		document.Parse(response);

		if (document.HasParseError() || !document.IsObject() || !document.HasMember("items") || !document["items"].IsArray())
		{
			totalItems = 0;
			UpdatePageDvar();
			return;
		}

		totalItems = GetJsonInt(document, "total", -1);

		if (totalItems < 0)
		{
			totalItems = GetJsonInt(document, "total_items", -1);
		}

		if (totalItems < 0)
		{
			totalItems = GetJsonInt(document, "total_count", -1);
		}

		nextOffset = GetJsonInt(document, "next_offset", -1);
		hasNextPage = nextOffset >= 0;

		const auto& items = document["items"];
		entries.reserve(items.Size());

		for (const auto& item : items.GetArray())
		{
			if (!item.IsObject())
			{
				continue;
			}

			Entry entry{};
			entry.guid = GetJsonString(item, "guid");
			entry.player = GetJsonString(item, "player", GetJsonString(item, "name", "Unknown"));
			entry.map = GetJsonString(item, "map", "Unknown");
			entry.round = GetJsonInt(item, "round", GetJsonInt(item, "metric_value"));
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
			entry.uploadedAt = GetJsonString(item, "uploadedAt", GetJsonString(item, "uploaded_at"));

			if (entry.player.empty())
			{
				entry.player = "Unknown";
			}

			if (entry.map.empty())
			{
				entry.map = "Unknown";
			}

			entries.push_back(entry);
		}

		displayedOffset = currentOffset;

		UpdatePageDvar();
		UpdateLocalPlayerStatus();

		if (lastKnownRank == 0 && displayedOffset == 0 && !isSearching)
		{
			FetchRankBackground(0);
		}
	}

	void Leaderboard::FetchRankBackground(const int offset)
	{
		if (currentMap.empty())
		{
			currentMap = GetCurrentMapName();
		}

		if (currentMap.empty())
		{
			return;
		}

		if (offset == 0)
		{
			isSearching = true;
		}

		const auto url = std::format("{}?limit={}&map={}&offset={}", bestRoundsUrl, defaultLimit, UrlEncode(currentMap), offset);

		Scheduler::Once([url, offset]
		{
			const auto reply = Utils::WebIO("zw3-rank-bg", url).Get();

			Scheduler::Once([reply, offset]
			{
				rapidjson::Document document{};
				document.Parse(reply);

				if (document.HasParseError() || !document.IsObject() || !document.HasMember("items") || !document["items"].IsArray())
				{
					isSearching = false;
					UpdateLocalPlayerStatus();
					return;
				}

				const auto& items = document["items"];
				const std::string localXuidHex = Utils::String::VA("%llX", static_cast<unsigned long long>(Party::GetLocalPlayerXuid()));

				for (rapidjson::SizeType i = 0; i < items.Size(); ++i)
				{
					if (items[i].IsObject() && localXuidHex == GetJsonString(items[i], "guid"))
					{
						lastKnownRank = offset + static_cast<int>(i) + 1;
						isSearching = false;
						UpdateLocalPlayerStatus();
						return;
					}
				}

				const int pageAfter = GetJsonInt(document, "next_offset", -1);

				if (pageAfter >= 0)
				{
					FetchRankBackground(pageAfter);
					return;
				}

				isSearching = false;
				UpdateLocalPlayerStatus();
			}, Scheduler::Pipeline::MAIN);
		}, Scheduler::Pipeline::ASYNC);
	}

	void Leaderboard::UpdateLocalPlayerStatus()
	{
		if (isLoading)
		{
			zw3_leaderboard_player_status.Set("Loading leaderboard status...");
		}
		else if (lastKnownRank > 0)
		{
			zw3_leaderboard_player_status.Set(Utils::String::VA("You are currently: ^3#%u", static_cast<unsigned int>(lastKnownRank)));
		}
		else if (!isSearching)
		{
			zw3_leaderboard_player_status.Set("You are not on the leaderboard for this map yet.");
		}
		else
		{
			zw3_leaderboard_player_status.Set("");
		}
	}

	unsigned int Leaderboard::GetEntryCount()
	{
		if (entries.empty())
		{
			return 1;
		}

		return static_cast<unsigned int>(entries.size());
	}

	const char* Leaderboard::GetEntryText(const unsigned int index, const int column)
	{
		if (entries.empty())
		{
			if (column == 0 && !isLoading)
			{
				return "--";
			}

			if (column == 2 && isLoading)
			{
				return "Loading leaderboard...";
			}

			if (column == 2)
			{
				return "No leaderboard entries";
			}

			return "";
		}

		if (index >= entries.size())
		{
			return "";
		}

		const auto& entry = entries[index];

		switch (column)
		{
		case 0:
			return Utils::String::VA("#%u", static_cast<unsigned int>(displayedOffset + index + 1));
		case 1:
			return Utils::String::VA("%i", entry.round);
		case 2:
			return entry.player.data();
		case 3:
			return Utils::String::VA("%i", entry.score);
		case 4:
			return Utils::String::VA("%i", entry.kills);
		case 5:
			return Utils::String::VA("%i", entry.downs);
		case 6:
			return FormatSeconds(entry.time);
		case 7:
			return Utils::String::VA("%i/4", entry.players);
		default:
			return "";
		}
	}

	void Leaderboard::SelectEntry([[maybe_unused]] const unsigned int index)
	{
	}

	Leaderboard::Leaderboard()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			zw3_leaderboard_map = Dvar::Register("zw3_leaderboard_map", "", Game::DVAR_INIT, "Current map used by the best rounds leaderboard.");
			zw3_leaderboard_page = Dvar::Register("zw3_leaderboard_page", "Page 1 of 1", Game::DVAR_INIT, "Current page used by the best rounds leaderboard.");
			zw3_leaderboard_player_status = Dvar::Register("zw3_leaderboard_player_status", "", Game::DVAR_INIT, "Local player leaderboard status.");
			zw3_leaderboard_mapname_display = Dvar::Register("zw3_leaderboard_mapname_display", "", Game::DVAR_INIT, "Display name of the current map.");
			zw3_leaderboard_can_prev = Dvar::Register("zw3_leaderboard_can_prev", false, Game::DVAR_NONE, "Whether leaderboard previous page is available.");
			zw3_leaderboard_can_next = Dvar::Register("zw3_leaderboard_can_next", false, Game::DVAR_NONE, "Whether leaderboard next page is available.");
		});

		Scheduler::Loop([]
		{
			static std::string lastMap;

			const auto mapName = GetCurrentMapName();

			if (mapName.empty() || mapName == lastMap)
			{
				return;
			}

			lastMap = mapName;
			UpdateMapDisplayDvar(mapName);
		}, Scheduler::Pipeline::MAIN, 2s);

		UIFeeder::Add(leaderboardFeeder, GetEntryCount, GetEntryText, SelectEntry);

		UIScript::Add("RefreshLeaderboard", RefreshFirstPage);
		UIScript::Add("PreviousLeaderboardPage", PreviousPage);
		UIScript::Add("NextLeaderboardPage", NextPage);
	}
}
