#pragma once

#include "rapidjson/document.h"

namespace Components
{
	class ZW3Auth : public Component
	{
	public:
		ZW3Auth();

		static bool IsAuthenticated();
		static bool IsGuest();
		static std::string GetAccessToken();
		static std::string GetTokenType();
		static std::string GetAuthHeader();

	private:
		struct DiscordFlowState
		{
			bool active = false;
			std::string state;
			std::string url;
			int intervalSeconds = 5;
			std::chrono::steady_clock::time_point nextPoll;
			std::chrono::steady_clock::time_point expiresAt;
		};

		static void Initialize();
		static void RegisterCommands();
		static void RegisterScripts();
		static void RegisterDvars();
		static void StartLogin();
		static void StartRegister();
		static void StartActivation();
		static void StartDiscord();
		static void PollDiscord();
		static void OpenDiscordUrl();
		static void Logout();
		static void PlayAsGuest();

		static std::string BuildUrl(const std::string& path);
		static std::string BuildJson(const std::vector<std::pair<std::string, std::string>>& values);
		static std::string UrlEncode(const std::string& input);
		static std::string BuildQuery(const std::vector<std::pair<std::string, std::string>>& values);
		static std::string GetDeviceGuid();
		static std::optional<std::string> GetString(const rapidjson::Document& document, const char* member);
		static std::optional<int> GetInt(const rapidjson::Document& document, const char* member);

		static std::mutex StateMutex;
		static DiscordFlowState DiscordFlow;

		static Dvar::Var Username;
		static Dvar::Var Password;
		static Dvar::Var RegisterUsername;
		static Dvar::Var RegisterEmail;
		static Dvar::Var RegisterPassword;
		static Dvar::Var ActivationCode;

		static std::string AccessToken;
		static std::string RefreshToken;
		static std::string TokenType;
		static std::string DiscordTicket;
		static std::string DiscordUrl;
		static bool GuestMode;
	};
}
