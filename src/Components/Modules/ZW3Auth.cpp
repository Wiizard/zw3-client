#include "ZW3Auth.hpp"

#include "Components/Loader.hpp"
#include "Command.hpp"
#include "Menus.hpp"
#include "Scheduler.hpp"
#include "Toast.hpp"
#include "UIScript.hpp"

#include "version.h"

#include <Utils/Utils.hpp>
#include <Utils/WebIO.hpp>

#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

#include <iomanip>

namespace Components
{
	std::mutex ZW3Auth::StateMutex{};
	ZW3Auth::DiscordFlowState ZW3Auth::DiscordFlow{};

	Dvar::Var ZW3Auth::Username;
	Dvar::Var ZW3Auth::Password;
	Dvar::Var ZW3Auth::RegisterUsername;
	Dvar::Var ZW3Auth::RegisterEmail;
	Dvar::Var ZW3Auth::RegisterPassword;
	Dvar::Var ZW3Auth::ActivationCode;

	std::string ZW3Auth::AccessToken;
	std::string ZW3Auth::RefreshToken;
	std::string ZW3Auth::TokenType = "Bearer";
	std::string ZW3Auth::DiscordTicket;
	std::string ZW3Auth::DiscordUrl;
	bool ZW3Auth::GuestMode = true;

	namespace
	{
		constexpr auto kDefaultBaseUrl = "https://auth.zw3.eu";
		constexpr auto kDefaultLoginPath = "/v1/auth/login";
		constexpr auto kDefaultDiscordStartPath = "/v1/auth/discord/start";
		constexpr auto kDefaultDiscordPollPath = "/v1/auth/discord/poll";
		constexpr auto kDefaultStartTicketPath = "/v1/auth/start-ticket";
		constexpr auto kAuthToastImage = "cardicon_locked";
	}

	ZW3Auth::ZW3Auth()
	{
		Scheduler::OnGameInitialized([]()
			{
				Initialize();
			}, Scheduler::Pipeline::MAIN);

		Scheduler::Loop([]()
			{
				PollDiscord();
			}, Scheduler::Pipeline::ASYNC, 1s);

		Menus::Add("ui_mp/zw3_auth.menu");
		Menus::Add("ui_mp/zw3_auth_register.menu");
	}

	void ZW3Auth::Initialize()
	{
		RegisterDvars();
		RegisterCommands();
		RegisterScripts();
	}

	void ZW3Auth::RegisterCommands()
	{
		Command::Add("openmenu", [](const Command::Params* params)
			{
				if (!params || params->size() != 2)
				{
					return;
				}

				const std::string menuName = params->get(1);
				if (!IsAuthenticated() && !IsGuest() && menuName != "zw3_auth" && menuName != "zw3_auth_register")
				{
					Game::Key_SetCatcher(0, Game::KEYCATCH_UI);
					Game::Menus_OpenByName(Game::uiContext, "zw3_auth");
					return;
				}

				if ((*Game::cl_ingame)->current.enabled)
				{
					Game::Key_SetCatcher(0, Game::KEYCATCH_UI);
				}

				Game::Menus_OpenByName(Game::uiContext, menuName.c_str());
			});
	}

	void ZW3Auth::RegisterDvars()
	{
		Username = Dvar::Register<const char*>("zw3_auth_username", "", Game::DVAR_NONE, "ZW3 auth username.");
		Password = Dvar::Register<const char*>("zw3_auth_password", "", Game::DVAR_NONE, "ZW3 auth password.");
		RegisterUsername = Dvar::Register<const char*>("zw3_auth_register_username", "", Game::DVAR_NONE, "ZW3 auth register username.");
		RegisterEmail = Dvar::Register<const char*>("zw3_auth_register_email", "", Game::DVAR_NONE, "ZW3 auth register email.");
		RegisterPassword = Dvar::Register<const char*>("zw3_auth_register_password", "", Game::DVAR_NONE, "ZW3 auth register password.");
		ActivationCode = Dvar::Register<const char*>("zw3_auth_activation_code", "", Game::DVAR_NONE, "ZW3 auth activation code.");
	}

	void ZW3Auth::RegisterScripts()
	{
		UIScript::Add("zw3AuthOpen", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				Command::Execute("openmenu zw3_auth", false);
			});

		UIScript::Add("zw3AuthOpenRegister", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				Command::Execute("openmenu zw3_auth_register", false);
			});

		UIScript::Add("zw3AuthAppendAt", [](const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				if (!token.isValid())
				{
					return;
				}

				const auto dvarName = token.get<std::string>();
				if (dvarName.empty())
				{
					return;
				}

				auto dvar = Dvar::Var(dvarName);
				auto value = dvar.get<std::string>();
				value.push_back('@');
				dvar.set(value);
			});

		UIScript::Add("zw3AuthLogin", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				StartLogin();
			});

		UIScript::Add("zw3AuthRegister", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				StartRegister();
			});

		UIScript::Add("zw3AuthActivate", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				StartActivation();
			});

		UIScript::Add("zw3AuthDiscordStart", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				StartDiscord();
			});

		UIScript::Add("zw3AuthLogout", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				Logout();
			});

		UIScript::Add("zw3AuthPlayGuest", []([[maybe_unused]] const UIScript::Token& token, [[maybe_unused]] const Game::uiInfo_s* info)
			{
				PlayAsGuest();
			});

	}

	bool ZW3Auth::IsAuthenticated()
	{
		return !AccessToken.empty();
	}

	bool ZW3Auth::IsGuest()
	{
		return GuestMode;
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
		if (!IsAuthenticated())
		{
			return {};
		}

		return Utils::String::Format("{} {}", GetTokenType(), GetAccessToken());
	}

	void ZW3Auth::StartLogin()
	{
		GuestMode = false;

		const auto username = Username.get<std::string>();
		const auto password = Password.get<std::string>();

		if (username.empty() || password.empty())
		{
			Toast::Show(kAuthToastImage, "Authentication", "Username and password are required.", 3500);
			return;
		}

		Scheduler::Once([username, password]()
			{
				const auto url = BuildUrl(kDefaultLoginPath);
				const auto body = BuildJson({
					{"email", username},
					{"password", password},
					{"device_guid", GetDeviceGuid()},
					});

				Utils::WebIO webio("zw3-auth");
				bool success = false;
				const auto response = webio.setTimeout(10000)->post(url, body, &success);

				Scheduler::Once([success, response]()
					{
						Password.set("");

						if (!success || response.empty())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Login failed. Service unreachable.", 3500);
							return;
						}

						rapidjson::Document document;
						if (document.Parse(response.c_str()).HasParseError() || !document.IsObject())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Login failed. Invalid response.", 3500);
							return;
						}

						if (const auto error = GetString(document, "error"))
						{
							Toast::Show(kAuthToastImage, "Authentication", error.value(), 3500);
							return;
						}

						if (const auto accessToken = GetString(document, "access_token"))
						{
							AccessToken = accessToken.value();
						}

						if (const auto sessionToken = GetString(document, "session_token"))
						{
							AccessToken = sessionToken.value();
							TokenType = "Bearer";
						}

						if (const auto refreshToken = GetString(document, "refresh_token"))
						{
							RefreshToken = refreshToken.value();
						}

						if (const auto tokenType = GetString(document, "token_type"))
						{
							TokenType = tokenType.value();
						}

						Toast::Show(kAuthToastImage, "Authentication", "Login successful.", 3500);
					}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);
	}

	void ZW3Auth::StartRegister()
	{
		GuestMode = false;

		const auto username = RegisterUsername.get<std::string>();
		const auto email = RegisterEmail.get<std::string>();
		const auto password = RegisterPassword.get<std::string>();

		if (username.empty() || email.empty() || password.empty())
		{
			Toast::Show(kAuthToastImage, "Authentication", "Enter username, email, and password to register.", 3500);
			return;
		}

		Scheduler::Once([username, email, password]()
			{
				const auto url = BuildUrl("/v1/auth/register");
				const auto body = BuildJson({
					{"username", username},
					{"email", email},
					{"password", password},
					{"device_guid", GetDeviceGuid()},
					});

				Utils::WebIO webio("zw3-auth");
				bool success = false;
				const auto response = webio.setTimeout(10000)->post(url, body, &success);

				Scheduler::Once([success, response]()
					{
						RegisterPassword.set("");

						if (!success || response.empty())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Registration failed. Please try again.", 3500);
							return;
						}

						rapidjson::Document document;
						if (document.Parse(response.c_str()).HasParseError() || !document.IsObject())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Registration failed. Invalid response.", 3500);
							return;
						}

						if (const auto error = GetString(document, "error"))
						{
							Toast::Show(kAuthToastImage, "Authentication", error.value(), 3500);
							return;
						}

						Toast::Show(kAuthToastImage, "Authentication", "Registration complete. Check your email to activate.", 4000);
					}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);
	}

	void ZW3Auth::StartActivation()
	{
		GuestMode = false;

		const auto code = ActivationCode.get<std::string>();
		if (code.empty())
		{
			Toast::Show(kAuthToastImage, "Authentication", "Enter the activation code from your email.", 3500);
			return;
		}

		Scheduler::Once([code]()
			{
				const auto url = BuildUrl("/v1/auth/activate");
				const auto body = BuildJson({
					{"code", code},
					{"device_guid", GetDeviceGuid()},
					});

				Utils::WebIO webio("zw3-auth");
				bool success = false;
				const auto response = webio.setTimeout(10000)->post(url, body, &success);

				Scheduler::Once([success, response]()
					{
						if (!success || response.empty())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Activation failed. Please try again.", 3500);
							return;
						}

						rapidjson::Document document;
						if (document.Parse(response.c_str()).HasParseError() || !document.IsObject())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Activation failed. Invalid response.", 3500);
							return;
						}

						if (const auto error = GetString(document, "error"))
						{
							Toast::Show(kAuthToastImage, "Authentication", error.value(), 3500);
							return;
						}

						Toast::Show(kAuthToastImage, "Authentication", "Activation successful. You can sign in now.", 3500);
					}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);
	}

	void ZW3Auth::StartDiscord()
	{
		GuestMode = false;

		Scheduler::Once([]()
			{
				const auto startUrl = BuildUrl(kDefaultDiscordStartPath)
					+ BuildQuery({ {"mode", "launcher"}, {"device_guid", GetDeviceGuid()} });
				std::optional<std::string> fallbackState;

				Utils::WebIO webio("zw3-auth");
				bool success = false;
				auto response = webio.setTimeout(10000)->get(startUrl, &success);

				if (!success || response.empty())
				{
					const auto ticketUrl = BuildUrl(kDefaultStartTicketPath);
					const auto body = BuildJson({
						{"device_guid", GetDeviceGuid()},
						});

					success = false;
					response = webio.setTimeout(10000)->post(ticketUrl, body, &success);
					if (success && !response.empty())
					{
						rapidjson::Document ticketDoc;
						if (!ticketDoc.Parse(response.c_str()).HasParseError() && ticketDoc.IsObject())
						{
							if (const auto ticket = GetString(ticketDoc, "start_ticket"))
							{
								const auto startWithTicket = BuildUrl(kDefaultDiscordStartPath)
									+ BuildQuery({ {"mode", "launcher"}, {"device_guid", GetDeviceGuid()}, {"start_ticket", ticket.value()} });
								success = false;
								response = webio.setTimeout(10000)->get(startWithTicket, &success);
							}
							if (const auto state = GetString(ticketDoc, "state"))
							{
								fallbackState = state.value();
							}
						}
					}
				}

				Scheduler::Once([success, response, fallbackState]()
					{
						if (!success || response.empty())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Discord start failed. Service unreachable.", 3500);
							return;
						}

						rapidjson::Document document;
						if (document.Parse(response.c_str()).HasParseError() || !document.IsObject())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Discord start failed. Invalid response.", 3500);
							return;
						}

						if (const auto error = GetString(document, "error"))
						{
							Toast::Show(kAuthToastImage, "Authentication", error.value(), 3500);
							return;
						}

						const auto state = GetString(document, "state");
						const auto urlValue = GetString(document, "url");
						const auto interval = GetInt(document, "interval");
						const auto expiresIn = GetInt(document, "expires_in");

						if ((!state.has_value() && !fallbackState.has_value()) || !urlValue.has_value())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Discord start failed. Missing state.", 3500);
							return;
						}

						DiscordTicket = state.value_or(*fallbackState);
						DiscordUrl = urlValue.value();

						auto now = std::chrono::steady_clock::now();
						{
							std::lock_guard<std::mutex> lock(StateMutex);
							DiscordFlow.active = true;
							DiscordFlow.state = DiscordTicket;
							DiscordFlow.url = urlValue.value();
							DiscordFlow.intervalSeconds = interval.value_or(5);
							DiscordFlow.expiresAt = now + std::chrono::seconds(expiresIn.value_or(600));
							DiscordFlow.nextPoll = now + std::chrono::seconds(DiscordFlow.intervalSeconds);
						}

						Toast::Show(kAuthToastImage, "Authentication", "Discord connection started.", 3500);
						OpenDiscordUrl();
					}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);
	}

	void ZW3Auth::PollDiscord()
	{
		DiscordFlowState flowCopy;
		{
			std::lock_guard<std::mutex> lock(StateMutex);
			if (!DiscordFlow.active)
			{
				return;
			}

			flowCopy = DiscordFlow;
		}

		const auto now = std::chrono::steady_clock::now();
		if (now < flowCopy.nextPoll)
		{
			return;
		}

		if (now >= flowCopy.expiresAt)
		{
			Scheduler::Once([]()
				{
					Toast::Show(kAuthToastImage, "Authentication", "Discord login expired.", 3500);
				}, Scheduler::Pipeline::MAIN);

			std::lock_guard<std::mutex> lock(StateMutex);
			DiscordFlow = {};
			return;
		}

		const auto state = flowCopy.state;
		Scheduler::Once([state]()
			{
				const auto url = BuildUrl(kDefaultDiscordPollPath) + BuildQuery({
					{"state", state},
					});

				Utils::WebIO webio("zw3-auth");
				bool success = false;
				const auto response = webio.setTimeout(10000)->get(url, &success);

				Scheduler::Once([success, response, state]()
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
							if (*error == "authorization_pending")
							{
								return;
							}

							if (*error == "slow_down")
							{
								std::lock_guard<std::mutex> lock(StateMutex);
								DiscordFlow.intervalSeconds += 5;
								DiscordFlow.nextPoll = std::chrono::steady_clock::now() + std::chrono::seconds(DiscordFlow.intervalSeconds);
								return;
							}

							if (*error == "access_denied" || *error == "cancelled" || *error == "expired_token")
							{
								Toast::Show(kAuthToastImage, "Authentication", "Discord authorization cancelled.", 3500);
								std::lock_guard<std::mutex> lock(StateMutex);
								DiscordFlow = {};
								return;
							}

							return;
						}

						if (const auto needsUsername = document.HasMember("needs_username") && document["needs_username"].IsBool()
							? std::optional<bool>(document["needs_username"].GetBool())
							: std::nullopt)
						{
							if (*needsUsername)
							{
								const auto desired = Username.get<std::string>();
								if (desired.empty())
								{
									Toast::Show(kAuthToastImage, "Authentication", "Enter a username to finish Discord login.", 3500);
									return;
								}

								const auto setUrl = BuildUrl("/v1/auth/discord/username");
								const auto body = BuildJson({
									{"state", state},
									{"username", desired},
									});

								Utils::WebIO setWebio("zw3-auth");
								bool setSuccess = false;
								setWebio.setTimeout(10000)->post(setUrl, body, &setSuccess);
								if (!setSuccess)
								{
									Toast::Show(kAuthToastImage, "Authentication", "Could not save Discord username.", 3500);
								}
								return;
							}
						}

						const auto ok = document.HasMember("ok") && document["ok"].IsBool()
							? document["ok"].GetBool()
							: false;

						if (ok)
						{
							if (const auto sessionToken = GetString(document, "session_token"))
							{
								AccessToken = sessionToken.value();
								TokenType = "Bearer";
							}
						}
						else
						{
							return;
						}

						if (const auto refreshToken = GetString(document, "refresh_token"))
						{
							RefreshToken = refreshToken.value();
						}

						if (const auto tokenType = GetString(document, "token_type"))
						{
							TokenType = tokenType.value();
						}

						if (!AccessToken.empty())
						{
							Toast::Show(kAuthToastImage, "Authentication", "Discord connected.", 3500);
						}

						std::lock_guard<std::mutex> lock(StateMutex);
						if (!AccessToken.empty())
						{
							DiscordFlow = {};
						}
					}, Scheduler::Pipeline::MAIN);
			}, Scheduler::Pipeline::ASYNC);

		{
			std::lock_guard<std::mutex> lock(StateMutex);
			DiscordFlow.nextPoll = now + std::chrono::seconds(DiscordFlow.intervalSeconds);
		}
	}

	void ZW3Auth::OpenDiscordUrl()
	{
		const auto url = DiscordUrl;
		if (url.empty())
		{
			Toast::Show(kAuthToastImage, "Authentication", "Discord URL not available.", 3500);
			return;
		}

		Utils::OpenUrl(url);
	}

	void ZW3Auth::Logout()
	{
		AccessToken.clear();
		RefreshToken.clear();
		TokenType = "Bearer";
		DiscordTicket.clear();
		DiscordUrl.clear();
		GuestMode = false;

		std::lock_guard<std::mutex> lock(StateMutex);
		DiscordFlow = {};

		Toast::Show(kAuthToastImage, "Authentication", "Signed out.", 3500);
	}

	void ZW3Auth::PlayAsGuest()
	{
		Logout();
		GuestMode = true;
		Toast::Show(kAuthToastImage, "Authentication", "Continuing as guest.", 3500);
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

	std::string ZW3Auth::BuildJson(const std::vector<std::pair<std::string, std::string>>& values)
	{
		rapidjson::Document document(rapidjson::kObjectType);
		auto& allocator = document.GetAllocator();

		for (const auto& [key, value] : values)
		{
			document.AddMember(rapidjson::Value(key.c_str(), allocator),
				rapidjson::Value(value.c_str(), allocator), allocator);
		}

		rapidjson::StringBuffer buffer;
		rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
		document.Accept(writer);
		return buffer.GetString();
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
		std::transform(result.begin(), result.end(), result.begin(), ::tolower);
		return result;
	}

	std::optional<std::string> ZW3Auth::GetString(const rapidjson::Document& document, const char* member)
	{
		if (document.HasMember(member) && document[member].IsString())
		{
			return document[member].GetString();
		}

		return std::nullopt;
	}

	std::optional<int> ZW3Auth::GetInt(const rapidjson::Document& document, const char* member)
	{
		if (document.HasMember(member) && document[member].IsInt())
		{
			return document[member].GetInt();
		}

		if (document.HasMember(member) && document[member].IsInt64())
		{
			return static_cast<int>(document[member].GetInt64());
		}

		return std::nullopt;
	}
}

namespace
{
	const auto zw3AuthRegistration = []()
		{
			Components::Loader::Register(new Components::ZW3Auth());
			return 0;
		}();
}
