#pragma once

#include "rapidjson/document.h"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace Components
{
	class ZW3Auth : public Component
	{
	public:
		ZW3Auth();

		void preDestroy() override;

		static bool IsLinked();
		static std::string GetAccessToken();
		static std::string GetTokenType();
		static std::string GetAuthHeader();

	private:
		struct FriendEntry
		{
			std::string userId;
			std::string username;
			std::string status;
			std::string activity;
		};

		static void Initialize();
		static void RegisterDvars();
		static void PollLinkStatus();

		static void RefreshFriends();
		static void DumpFriends();

		static void InitializeSdk();
		static void ShutdownSdk();

		static std::string BuildUrl(const std::string& path);
		static std::string UrlEncode(const std::string& input);
		static std::string BuildQuery(const std::vector<std::pair<std::string, std::string>>& values);
		static std::string GetDeviceGuid();
		static std::optional<std::string> GetString(const rapidjson::Document& document, const char* member);

		static std::mutex StateMutex;
		static bool Linked;
		static std::string LinkedAccountLabel;

		static std::mutex FriendsMutex;
		static std::vector<FriendEntry> Friends;

		static Dvar::Var LinkedDvar;
		static Dvar::Var LinkedAccountDvar;

		static std::string AccessToken;
		static std::string TokenType;
		static bool SdkReady;
	};
}
