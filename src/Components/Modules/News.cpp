#include "STDInclude.hpp"

#include <version.hpp>

#include "News.hpp"
#include "Changelog.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "Localization.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "StartupMessages.hpp"
#include "UIScript.hpp"

namespace Components
{
	constexpr const char* motdDefault = "Welcome to Call of Duty: Zombie Warfare 3!";

	constexpr std::uintptr_t NewsTicker_TextTestJump = 0x14026181D;
	constexpr std::uintptr_t NewsTicker_UI_SafeTranslateStringCall = 0x140261823;
	constexpr std::uintptr_t NewsTicker_UI_SetScissorRectCall = 0x1402618B7;
	constexpr std::uintptr_t UI_SafeTranslateString = 0x140272770;
	constexpr std::uintptr_t UI_SetScissorRect = 0x140272F40;

	static const std::uint8_t textTestJump[] = { 0x75, 0x0C };

	static Utils::Hook translateHook;

	const char* News::GetNewsText()
	{
		return Localization::Get("MPUI_MOTD_TEXT");
	}

	void News::FetchInfo()
	{
		const auto result = Utils::Cache::GetFile("/info");

		if (result.empty())
		{
			return;
		}

		Scheduler::Once([result]
		{
			ApplyInfo(result);
		}, Scheduler::Pipeline::MAIN);
	}

	void News::ApplyInfo(const std::string& info)
	{
		rapidjson::Document jsonDocument{};
		const rapidjson::ParseResult parseResult = jsonDocument.Parse(info);

		if (!parseResult || !jsonDocument.IsObject())
		{
			return;
		}

		if (ProcessPopmenus(jsonDocument))
		{
			StartupMessages::Show();
		}
	}

	bool News::ProcessPopmenus(const rapidjson::Document& document)
	{
		if (!document.HasMember("popmenu") || !document["popmenu"].IsArray())
		{
			return false;
		}

		bool didAdd = false;

		for (const auto& menuItem : document["popmenu"].GetArray())
		{
			const auto item = ExtractPopmenuItem(menuItem);

			if (!item.has_value())
			{
				continue;
			}

			if (ShouldShowForRevision(menuItem["revisions"]))
			{
				StartupMessages::AddMessage(item->second, item->first);
				didAdd = true;
			}
		}

		return didAdd;
	}

	std::optional<std::pair<std::string, std::string>> News::ExtractPopmenuItem(const rapidjson::Value& menuItem)
	{
		if (!menuItem.HasMember("title") || !menuItem.HasMember("message") || !menuItem.HasMember("revisions") || !menuItem.HasMember("show"))
		{
			return std::nullopt;
		}

		if (!menuItem["show"].IsBool() || !menuItem["show"].GetBool())
		{
			return std::nullopt;
		}

		const auto& title = menuItem["title"];
		const auto& message = menuItem["message"];

		if (!title.IsString() || !message.IsString())
		{
			return std::nullopt;
		}

		return std::make_pair(title.GetString(), message.GetString());
	}

	bool News::ShouldShowForRevision(const rapidjson::Value& revisions)
	{
		if (!revisions.IsArray())
		{
			return false;
		}

		for (const auto& revision : revisions.GetArray())
		{
			if (!revision.IsString())
			{
				continue;
			}

			const std::string revisionText = revision.GetString();

			if (revisionText == REVISION_STR || revisionText == "any")
			{
				return true;
			}
		}

		return false;
	}

	News::News()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			Dvar::Register("g_firstLaunch", true, Game::DVAR_ARCHIVE, "");
		});

		UIScript::Add("checkFirstLaunch", []([[maybe_unused]] const UIScript::Token& token)
		{
			if (Dvar::Var("g_firstLaunch").Get<bool>())
			{
				Command::Execute("openmenu menu_first_launch", false);
			}

			StartupMessages::Show();
		});

		UIScript::Add("visitWebsite", []([[maybe_unused]] const UIScript::Token& token)
		{
			Utils::OpenUrl("https://zw3.eu");
		});

		Localization::Set("MPUI_CHANGELOG_TEXT", "Loading...");
		Localization::Set("MPUI_MOTD_TEXT", motdDefault);
		Changelog::SetChangelog("Changelog not available.");

		Scheduler::Once(FetchInfo, Scheduler::Pipeline::ASYNC);

		const bool isTickerExpected = Utils::Hook::MatchesBytes(NewsTicker_TextTestJump, textTestJump, sizeof(textTestJump))
			&& Utils::Hook::BranchesTo(NewsTicker_UI_SafeTranslateStringCall, UI_SafeTranslateString, HOOK_CALL)
			&& Utils::Hook::BranchesTo(NewsTicker_UI_SetScissorRectCall, UI_SetScissorRect, HOOK_CALL);

		if (!isTickerExpected)
		{
			Logger::Error("news: the news ticker does not read as expected, it keeps its own text\n");
			return;
		}

		if (!translateHook.Initialize(NewsTicker_UI_SafeTranslateStringCall, reinterpret_cast<void*>(GetNewsText), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("news: could not seat the news ticker hook, it keeps its own text\n");
			return;
		}

		translateHook.Quick();
		Utils::Hook::Nop(NewsTicker_TextTestJump, sizeof(textTestJump));
		Utils::Hook::Nop(NewsTicker_UI_SetScissorRectCall, 5);
	}
}
