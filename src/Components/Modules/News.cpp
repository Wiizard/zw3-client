#include "Changelog.hpp"
#include "News.hpp"
#include "StartupMessages.hpp"
#include "rapidjson/document.h"
#include "version.h"

#define NEWS_MOTD_DEFAULT "Welcome to Call of Duty: Zombie Warfare 3!"

/*
  MOTD, Changelog and popup messages are fetched as JSON using Cache::GetFile.
  {
     "motd":string,
     "popmenu":[
	    {
		   "title":string,
		   "message":string,
		   "revisions":array of strings,
		   "show":bool
	    }
     ],
     "changelog":string
  }

  Popups are shown on startup using StartupMessages::AddMessage.
  In any case where theres invalid data in the response/the request failed,
  the default values are used or, in case of popups, they are discarded.
  We only accept a valid response, anything that doesn't match the above format is discarded.
*/

namespace Components
{
	const char* News::GetNewsText()
	{
		return Localization::Get("MPUI_MOTD_TEXT");
	}

	std::optional<std::string> News::ExtractStringByMemberName(const rapidjson::Document& document, const std::string& memberName)
	{
		if (document.HasMember(memberName) && document[memberName].IsString())
			return document[memberName].GetString();

		return std::nullopt;
	}

	std::vector<std::pair<std::string, std::string>> News::CollectPopmenus(const rapidjson::Document& document)
	{
		std::vector<std::pair<std::string, std::string>> messages;
		if (!document.HasMember("popmenu") || !document["popmenu"].IsArray())
			return messages;

		for (const auto& menuItem : document["popmenu"].GetArray())
		{
			auto item = ExtractPopmenuItem(menuItem);
			if (!item.has_value())
				continue;

			if (ShouldShowForRevision(menuItem["revisions"]))
				messages.push_back(std::move(*item));
		}
		return messages;
	}

	std::optional<std::pair<std::string, std::string>> News::ExtractPopmenuItem(const rapidjson::Value& menuItem)
	{
		if (!menuItem.HasMember("title") ||
			!menuItem.HasMember("message") ||
			!menuItem.HasMember("revisions") ||
			!menuItem.HasMember("show"))
		{
			return std::nullopt;
		}
			

		if (!menuItem["show"].GetBool())
			return std::nullopt;

		const auto& title = menuItem["title"];
		const auto& message = menuItem["message"];

		if (!title.IsString() || !message.IsString())
			return std::nullopt;

		return std::make_pair(title.GetString(), message.GetString());
	}

	bool News::ShouldShowForRevision(const rapidjson::Value& revisions)
	{
		if (!revisions.IsArray())
			return false;

		for (const auto& revision : revisions.GetArray())
		{
			if (!revision.IsString())
				continue;

			const std::string revStr = revision.GetString();
			if (revStr == REVISION_STR || revStr == "any")
				return true;
		}
		return false;
	}

	News::News()
	{
		if (ZoneBuilder::IsEnabled() || Dedicated::IsEnabled()) return; // Maybe also dedi?

		Dvar::Register<bool>("g_firstLaunch", true, Game::DVAR_ARCHIVE, "");

		// Called by main_text.menu
		UIScript::Add("checkFirstLaunch", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			if (Dvar::Var("g_firstLaunch").get<bool>())
			{
				Command::Execute("openmenu menu_first_launch", false);
			}

			StartupMessages::Show();
		});

		UIScript::Add("visitWebsite", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
		{
			Utils::OpenUrl("https://zw3.eu");
		});

		Localization::Set("MPUI_CHANGELOG_TEXT", "Loading...");
		Localization::Set("MPUI_MOTD_TEXT", NEWS_MOTD_DEFAULT);
		Changelog::SetChangelog("Changelog not available.");

		// make newsfeed (ticker) menu items not cut off based on safe area
		Utils::Hook::Nop(0x63892D, 5);

		// hook for getting the news ticker string
		Utils::Hook::Nop(0x6388BB, 2); // skip the "if (item->text[0] == '@')" localize check
		Utils::Hook(0x6388C1, GetNewsText, HOOK_CALL).install()->quick();

		// A cache miss can block in WinINet for several seconds. Defaults and
		// locally registered UI are ready immediately; only the remote refresh
		// runs on the existing async scheduler.
		Scheduler::Once([]
		{
			const auto result = Utils::Cache::GetFile("/info");
			if (result.empty()) return;

			rapidjson::Document jsonDocument{};
			const rapidjson::ParseResult parseResult = jsonDocument.Parse(result);
			if (!parseResult || !jsonDocument.IsObject()) return;

			auto messages = CollectPopmenus(jsonDocument);
			if (messages.empty()) return;
			Scheduler::Once([messages = std::move(messages)]
			{
				for (const auto& [title, body] : messages)
					StartupMessages::AddMessage(body, title);
				StartupMessages::Show();
			}, Scheduler::Pipeline::MAIN);
		}, Scheduler::Pipeline::ASYNC);
	}

	void News::preDestroy()
	{
	}
}
