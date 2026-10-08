#include "STDInclude.hpp"

#include "StartupMessages.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "UIScript.hpp"

namespace Components
{
	int StartupMessages::totalMessages = -1;
	std::list<std::tuple<std::string, std::string>> StartupMessages::messageList;

	Dvar::Var StartupMessages::ui_startupMessage;
	Dvar::Var StartupMessages::ui_startupMessageTitle;
	Dvar::Var StartupMessages::ui_startupNextButtonText;

	StartupMessages::StartupMessages()
	{
		Events::OnDvarInit([]
		{
			ui_startupMessage = Dvar::Register("ui_startupMessage", "", Game::DVAR_NONE, "");
			ui_startupMessageTitle = Dvar::Register("ui_startupMessageTitle", "", Game::DVAR_NONE, "");
			ui_startupNextButtonText = Dvar::Register("ui_startupNextButtonText", "", Game::DVAR_NONE, "");
		});

		UIScript::Add("nextStartupMessage", []([[maybe_unused]] const UIScript::Token& token)
		{
			Show();
		});
	}

	void StartupMessages::Show()
	{
		if (messageList.empty())
		{
			return;
		}

		const int messageListSize = static_cast<int>(messageList.size());

		if (totalMessages < 1)
		{
			totalMessages = messageListSize;
		}

		const auto& [title, body] = messageList.front();

		const int messageIndex = totalMessages - messageListSize + 1;
		const std::string formattedTitle = std::format("{} ({}/{})", title, messageIndex, totalMessages);

		std::string nextButtonText = "Next";

		if (messageListSize <= 1)
		{
			nextButtonText = "Close";
		}

		ui_startupMessage.Set(body);
		ui_startupMessageTitle.Set(formattedTitle);
		ui_startupNextButtonText.Set(nextButtonText);

		messageList.pop_front();
		Command::Execute("openmenu startup_messages", false);
	}

	void StartupMessages::AddMessage(const std::string& message)
	{
		AddMessage(message, "Messages");
	}

	void StartupMessages::AddMessage(const std::string& message, const std::string& title)
	{
		messageList.emplace_back(title, message);
	}
}
