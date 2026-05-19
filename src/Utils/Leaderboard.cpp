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
	Dvar::Var Leaderboard::UILeaderboardMap;
	Dvar::Var Leaderboard::UILeaderboardPage;
	Dvar::Var Leaderboard::UILeaderboardLoadingIndicator;
	int Leaderboard::CurrentOffset = 0;
	int Leaderboard::NextOffset = -1;
int Leaderboard::TotalItems = -1;
	bool Leaderboard::HasNextPage = false;
	bool Leaderboard::Loading = false;
	unsigned int Leaderboard::RequestSerial = 0;
	int Leaderboard::LoadingFrame = 0;
	std::string Leaderboard::CurrentMap;

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
		if (!object.HasMember(key) || object[key].IsNull()) return fallback;
		if (object[key].IsInt()) return object[key].GetInt();
		if (object[key].IsNumber()) return static_cast<int>(object[key].GetDouble());
		return fallback;
	}

	static float GetJsonFloat(const rapidjson::Value& object, const char* key, float fallback = 0.0f)
	{
		if (!object.HasMember(key) || object[key].IsNull()) return fallback;
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

	void Leaderboard::UpdatePageDvar()
	{
		const auto page = (CurrentOffset / DefaultLimit) + 1;
		if (TotalItems >= 0)
		{
			const auto totalPages = std::max(1, (TotalItems + DefaultLimit - 1) / DefaultLimit);
			UILeaderboardPage.set(Utils::String::VA("Page %i of %i", page, totalPages));
			return;
		}

		if (!HasNextPage)
		{
			UILeaderboardPage.set(Utils::String::VA("Page %i of %i", page, page));
			return;
		}

		UILeaderboardPage.set(Utils::String::VA("Page %i", page));
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

	void Leaderboard::UpdateLoadingStatus()
	{
		if (!Loading)
		{
			return;
		}

		constexpr const char* spinner[] = { "|", "/", "-", "\\" };
		constexpr const char* dots[] = { ".", "..", "..." };
		UILeaderboardLoadingIndicator.set(Utils::String::VA("%s Loading%s",
			spinner[LoadingFrame % std::size(spinner)],
			dots[LoadingFrame % std::size(dots)]));
		LoadingFrame++;
	}

	void Leaderboard::StartRefresh(int offset)
	{
		const auto mapName = GetCurrentMapName();
		UILeaderboardMap.set(mapName.empty() ? "Unknown" : mapName);

		if (mapName.empty())
		{
			++RequestSerial;
			Entries.clear();
			Loading = false;
			HasNextPage = false;
			NextOffset = -1;
			UILeaderboardLoadingIndicator.set("No map");
			UpdatePageDvar();
			Toast::Show("cardicon_redhand", "^1Leaderboard", "Could not detect the current map for leaderboard lookup.", 5000);
			return;
		}

		CurrentMap = mapName;
		CurrentOffset = std::max(0, offset);
		const auto requestId = ++RequestSerial;
		Entries.clear();
		HasNextPage = false;
		NextOffset = -1;
		UpdatePageDvar();

		Loading = true;
		LoadingFrame = 0;
		UpdateLoadingStatus();

		const auto url = CurrentOffset > 0
			? std::format("https://stats.zw3.eu/leaderboard/best-rounds?limit={}&map={}&offset={}", DefaultLimit, UrlEncode(mapName), CurrentOffset)
			: std::format("https://stats.zw3.eu/leaderboard/best-rounds?limit={}&map={}", DefaultLimit, UrlEncode(mapName));

		Scheduler::Once([requestId, url]
			{
				Utils::WebIO::params headers;
				headers["Content-Type"] = "application/json";
				const auto reply = Utils::WebIO("zw3-best-rounds", url).setTimeout(5000)->get(headers);

				Scheduler::Once([requestId, reply]
					{
						if (requestId != RequestSerial)
						{
							return;
						}

						Loading = false;
						UILeaderboardLoadingIndicator.set("");

						if (reply.empty())
						{
							UILeaderboardLoadingIndicator.set("Load failed");
							Toast::Show("cardicon_redhand", "^1Leaderboard", "Could not get a response from the stats API.", 5000);
							return;
						}

						ParseResponse(reply);
					}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);
	}

	void Leaderboard::Refresh([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		StartRefresh(CurrentOffset);
	}

	void Leaderboard::RefreshFirstPage([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		const auto mapName = GetCurrentMapName();
		StartRefresh(mapName == CurrentMap ? CurrentOffset : 0);
	}

	void Leaderboard::PreviousPage([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		StartRefresh(std::max(0, CurrentOffset - DefaultLimit));
	}

	void Leaderboard::NextPage([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
	{
		if (!HasNextPage)
		{
			UpdatePageDvar();
			return;
		}

		StartRefresh(NextOffset);
	}

	void Leaderboard::ParseResponse(const std::string& response)
	{
		Entries.clear();
		HasNextPage = false;
		NextOffset = -1;
		Loading = false;
		UILeaderboardLoadingIndicator.set("");

		rapidjson::Document doc{};
		doc.Parse(response);

		if (doc.HasParseError() || !doc.IsObject())
		{
			UILeaderboardLoadingIndicator.set("Parse error");
			UpdatePageDvar();
			return;
		}

		if (!doc.HasMember("items") || !doc["items"].IsArray())
		{
			UILeaderboardLoadingIndicator.set("No entries");
			TotalItems = 0;
			UpdatePageDvar();
			return;
		}

		// Try to read total items if provided by the API
		TotalItems = GetJsonInt(doc, "total", -1);
		if (TotalItems < 0) TotalItems = GetJsonInt(doc, "total_items", -1);
		if (TotalItems < 0) TotalItems = GetJsonInt(doc, "total_count", -1);

		NextOffset = GetJsonInt(doc, "next_offset", -1);
		HasNextPage = NextOffset >= 0;

		const auto& items = doc["items"];
		Entries.reserve(items.Size());

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

			if (entry.player.empty()) entry.player = "Unknown";
			if (entry.map.empty()) entry.map = "Unknown";

			Entries.push_back(entry);
		}

		if (Entries.empty())
		{
			UILeaderboardLoadingIndicator.set("No entries");
		}

		UpdatePageDvar();
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
				return Loading ? "Loading leaderboard" : "No leaderboard entries";
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
			return Utils::String::VA("#%u", static_cast<unsigned int>(CurrentOffset + index + 1));
		case 1:
			return entry.player.data();
		case 2:
			return Utils::String::VA("%i", entry.round);
		case 3:
			return entry.map.data();
		case 4:
			return Utils::String::VA("%i", entry.players);
		case 5:
			return Utils::String::VA("%i", entry.score);
		case 6:
			return Utils::String::VA("%i", entry.kills);
		case 7:
			return Utils::String::VA("%i", entry.downs);
		case 8:
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
				UILeaderboardMap = Dvar::Register<const char*>("ui_leaderboard_map", "", Game::DVAR_NONE, "Current map used by the best rounds leaderboard.");
				UILeaderboardPage = Dvar::Register<const char*>("ui_leaderboard_page", "Page 1", Game::DVAR_NONE, "Current page used by the best rounds leaderboard.");
				UILeaderboardLoadingIndicator = Dvar::Register<const char*>("ui_leaderboard_loading_indicator", "", Game::DVAR_NONE, "Animated loading indicator for the best rounds leaderboard.");
			});

		UIFeeder::Add(FeederId, Leaderboard::GetEntryCount, Leaderboard::GetEntryText, Leaderboard::SelectEntry);
		UIScript::Add("RefreshLeaderboard", Leaderboard::RefreshFirstPage);
		UIScript::Add("RefreshLeaderboardPage", Leaderboard::Refresh);
		UIScript::Add("PreviousLeaderboardPage", Leaderboard::PreviousPage);
		UIScript::Add("NextLeaderboardPage", Leaderboard::NextPage);

		Scheduler::Loop(Leaderboard::UpdateLoadingStatus, Scheduler::Pipeline::MAIN, 250ms);
	}

	Leaderboard::~Leaderboard()
	{
		Entries.clear();
	}
}
