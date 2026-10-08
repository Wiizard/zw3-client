#pragma once

#include "Dvar.hpp"

namespace Components
{
	class StartupMessages : public Component
	{
	public:
		StartupMessages();

		static void AddMessage(const std::string& message);
		static void AddMessage(const std::string& message, const std::string& title);
		static void Show();

	private:
		static int totalMessages;
		static std::list<std::tuple<std::string, std::string>> messageList;

		static Dvar::Var ui_startupMessage;
		static Dvar::Var ui_startupMessageTitle;
		static Dvar::Var ui_startupNextButtonText;
	};
}
