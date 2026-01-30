#include "ZW3Auth.hpp"

#include "Components/Loader.hpp"
#include "Command.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "Toast.hpp"

#include "version.h"

#include <Utils/Utils.hpp>
#include <Utils/WebIO.hpp>

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <vector>

#if __has_include(<discord.h>)
#include <discord.h>
#define ZW3_HAS_DISCORD_SDK 1
#else
#define ZW3_HAS_DISCORD_SDK 0
#endif

namespace Components
{
	std::mutex ZW3Auth::StateMutex{};
	bool ZW3Auth::Linked = false;
	std::string ZW3Auth::LinkedAccountLabel;

	std::mutex ZW3Auth::FriendsMutex{};
	std::vector<ZW3Auth::FriendEntry> ZW3Auth::Friends{};

	Dvar::Var ZW3Auth::LinkedDvar;
	Dvar::Var ZW3Auth::LinkedAccountDvar;

	std::string ZW3Auth::AccessToken;
	std::string ZW3Auth::TokenType = "Bearer";
	bool ZW3Auth::SdkReady = false;

	namespace
	{
		constexpr auto kDefaultBaseUrl = "https://auth.zw3.eu";
		constexpr auto kDefaultDiscordStatusPath = "/v1/discord/link/status";
		constexpr auto kAuthToastImage = "cardicon_locked";
		constexpr int64_t kDiscordClientId = 1047291181404528660;

#if ZW3_HAS_DISCORD_SDK
		discord::Core* gDiscordCore = nullptr;

		const char* StatusToString(discord::Status status)
		{
			switch (status)
			{
			case discord::Status::Online:
				return "online";
			case discord::Status::Idle:
				return "idle";
			case discord::Status::DoNotDisturb:
				return "dnd";
			case discord::Status::Offline:
				return "offline";
			default:
				return "unknown";
			}
		}
#endif
	}

	ZW3Auth::ZW3Auth()
	{
		Scheduler::OnGameInitialized([]()
			{
				Initialize();
			}, Scheduler::Pipeline::MAIN);
	}

	void ZW3Auth::preDestroy()
	{
		ShutdownSdk();
	}

	void ZW3Auth::Initialize()
	{
		RegisterDvars();
		InitializeSdk();

		Scheduler::Loop([]()
			{
				PollLinkStatus();
			}, Scheduler::Pipeline::ASYNC, 5s);

		Scheduler::Loop([]()
			{
				RefreshFriends();
			}, Scheduler::Pipeline::ASYNC, 5s);
	}

	void ZW3Auth::RegisterDvars()
	{
		LinkedDvar = Dvar::Register<bool>("zw3_discord_linked", false, Game::DVAR_INIT, "Discord bot link status.");
		LinkedAccountDvar = Dvar::Register<const char*>("zw3_discord_link_account", "", Game::DVAR_INIT, "Linked Discord account label.");
	}

	bool ZW3Auth::IsLinked()
	{
		std::lock_guard<std::mutex> lock(StateMutex);
		return Linked;
	}

	std::string ZW3Auth::GetAccessToken()
	{
		return AccessToken;
	}

	std::string ZW3Auth::GetTokenType()
	{
		return TokenType;
	}

	std::string ZW3Auth::GetAuthHeader()
	{
		if (AccessToken.empty())
		{
			return {};
		}

		return Utils::String::Format("{} {}", GetTokenType(), GetAccessToken());
	}

	void ZW3Auth::PollLinkStatus()
	{
		Scheduler::Once([]()
			{
				const auto url = BuildUrl(kDefaultDiscordStatusPath)
					+ BuildQuery({ {"device_guid", GetDeviceGuid()} });

				Utils::WebIO webio("zw3-discord");
				bool success = false;
				const auto response = webio.setTimeout(10000)->get(url, &success);

				Scheduler::Once([success, response]()
					{
						if (!success || response.empty())
						{
							return;
						}

						rapidjson::Document document;
						if (document.Parse(response.c_str()).HasParseError() || !document.IsObject())
						{
							return;
						}

						if (const auto error = GetString(document, "error"))
						{
							Logger::Print("DiscordSocial: link status error: {}\n", error.value());
							return;
						}

						const auto linked = document.HasMember("linked") && document["linked"].IsBool()
							? document["linked"].GetBool()
							: false;

						const auto sessionToken = GetString(document, "session_token");
						const auto tokenType = GetString(document, "token_type");
						const auto accountLabel = GetString(document, "account_label");

						bool notify = false;
						bool wasLinked = false;
						std::string previousAccount;
						{
							std::lock_guard<std::mutex> lock(StateMutex);
							wasLinked = Linked;
							previousAccount = LinkedAccountLabel;

							if (linked)
							{
								Linked = true;
								if (sessionToken.has_value() && !sessionToken->empty())
								{
									AccessToken = sessionToken.value();
									TokenType = "Bearer";
								}

								if (tokenType.has_value() && !tokenType->empty())
								{
									TokenType = tokenType.value();
								}

								if (accountLabel.has_value())
								{
									LinkedAccountLabel = accountLabel.value();
								}
							}
							else
							{
								Linked = false;
								AccessToken.clear();
								TokenType = "Bearer";
								LinkedAccountLabel.clear();
							}

							notify = (linked != wasLinked);
						}

						const auto nextAccountLabel = accountLabel.value_or(previousAccount);

						if (linked && accountLabel.has_value() && !accountLabel->empty())
						{
							Command::Execute(Utils::String::VA("name \"%s\"", accountLabel->c_str()), false);
						}

						Scheduler::Once([linked, notify, nextAccountLabel]()
							{
								LinkedDvar.set(linked);
								LinkedAccountDvar.set(nextAccountLabel);

								if (notify && linked)
								{
									Toast::Show(kAuthToastImage, "Authentication", "Discord linked via bot.", 3500);
								}
								else if (notify && !linked)
								{
									Toast::Show(kAuthToastImage, "Authentication", "Discord link removed.", 3500);
								}
							}, Scheduler::Pipeline::MAIN);
					}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);
	}

	void ZW3Auth::RefreshFriends()
	{
#if ZW3_HAS_DISCORD_SDK
		if (!SdkReady || !gDiscordCore)
		{
			return;
		}

		gDiscordCore->RunCallbacks();

		auto& relationships = gDiscordCore->RelationshipManager();
		relationships.Filter([](discord::Relationship const& relationship)
			{
				return relationship.GetType() == discord::RelationshipType::Friend;
			});

		int32_t count = 0;
		relationships.Count(&count);

		std::vector<FriendEntry> next;
		next.reserve(static_cast<size_t>(count));

		for (int32_t i = 0; i < count; ++i)
		{
			discord::Relationship relationship{};
			if (relationships.GetAt(i, &relationship) != discord::Result::Ok)
			{
				continue;
			}

			FriendEntry entry{};
			entry.userId = std::to_string(static_cast<int64_t>(relationship.GetUser().GetId()));
			entry.username = relationship.GetUser().GetUsername();
			entry.status = StatusToString(relationship.GetPresence().GetStatus());
			entry.activity = relationship.GetPresence().GetActivity().GetName();
			next.push_back(std::move(entry));
		}

		{
			std::lock_guard<std::mutex> lock(FriendsMutex);
			Friends = std::move(next);
		}
#else
		if (!SdkReady)
		{
			SdkReady = true;
			Logger::Print("DiscordSocial: Discord Game SDK not available; friends list disabled.\n");
		}
#endif
	}

	void ZW3Auth::DumpFriends()
	{
		std::lock_guard<std::mutex> lock(FriendsMutex);
		Logger::Print("DiscordSocial: {} friend(s).\n", Friends.size());
		for (const auto& friendEntry : Friends)
		{
			Logger::Print("DiscordSocial: {} ({}) - {} [{}]\n",
				friendEntry.username,
				friendEntry.userId,
				friendEntry.status,
				friendEntry.activity);
		}
	}

	void ZW3Auth::InitializeSdk()
	{
#if ZW3_HAS_DISCORD_SDK
		if (SdkReady)
		{
			return;
		}

		const auto result = discord::Core::Create(kDiscordClientId, DiscordCreateFlags_NoRequireDiscord, &gDiscordCore);
		if (result != discord::Result::Ok)
		{
			Logger::Print("DiscordSocial: failed to initialize Discord Game SDK ({}).\n", static_cast<int>(result));
			return;
		}

		SdkReady = true;
		Logger::Print("DiscordSocial: Discord Game SDK initialized.\n");
#else
		SdkReady = true;
		Logger::Print("DiscordSocial: Discord Game SDK not available; friends list disabled.\n");
#endif
	}

	void ZW3Auth::ShutdownSdk()
	{
#if ZW3_HAS_DISCORD_SDK
		if (gDiscordCore)
		{
			delete gDiscordCore;
			gDiscordCore = nullptr;
		}
#endif
		SdkReady = false;
	}

	std::string ZW3Auth::BuildUrl(const std::string& path)
	{
		std::string base = kDefaultBaseUrl;
		if (base.ends_with('/') && path.starts_with('/'))
		{
			return base.substr(0, base.size() - 1) + path;
		}

		if (!base.ends_with('/') && !path.starts_with('/'))
		{
			return base + "/" + path;
		}

		return base + path;
	}

	std::string ZW3Auth::UrlEncode(const std::string& input)
	{
		std::ostringstream escaped;
		escaped.fill('0');
		escaped << std::hex << std::uppercase;

		for (const auto ch : input)
		{
			if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '-' || ch == '_' || ch == '.' || ch == '~')
			{
				escaped << ch;
			}
			else
			{
				escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(ch));
			}
		}

		return escaped.str();
	}

	std::string ZW3Auth::BuildQuery(const std::vector<std::pair<std::string, std::string>>& values)
	{
		if (values.empty())
		{
			return {};
		}

		std::string query = "?";
		for (size_t i = 0; i < values.size(); ++i)
		{
			const auto& [key, value] = values[i];
			query += UrlEncode(key);
			query += "=";
			query += UrlEncode(value);
			if (i + 1 < values.size())
			{
				query += "&";
			}
		}

		return query;
	}

	std::string ZW3Auth::GetDeviceGuid()
	{
		const auto guid = Utils::String::VA("%016llX", Steam::SteamUser()->GetSteamID().bits);
		std::string result = guid;
		std::transform(result.begin(), result.end(), result.begin(), [](const auto& c)
			{
				return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			});

		return result;
	}

	std::optional<std::string> ZW3Auth::GetString(const rapidjson::Document& document, const char* member)
	{
		if (!document.HasMember(member) || !document[member].IsString())
		{
			return std::nullopt;
		}

		return document[member].GetString();
	}
}

namespace
{
	const auto discordSocialRegistration = []()
		{
			//Components::Loader::Register(new Components::ZW3Auth());
			return 0;
		}();
}
