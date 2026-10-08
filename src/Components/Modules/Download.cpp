#include "STDInclude.hpp"

#include "Download.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "ModList.hpp"
#include "Party.hpp"
#include "Scheduler.hpp"
#include "UIScript.hpp"
#include "FileSystem.hpp"
#include "Flags.hpp"
#include "MapRotation.hpp"
#include "GSC/Script.hpp"
#include "Maps.hpp"
#include "Node.hpp"
#include "ServerInfo.hpp"

#include <mongoose.h>

namespace Components
{
	Dvar::Var Download::sv_wwwDownload;
	Dvar::Var Download::sv_wwwBaseUrl;

	Dvar::Var Download::ui_dl_timeLeft;
	Dvar::Var Download::ui_dl_progress;
	Dvar::Var Download::ui_dl_transRate;

	Download::ClientDownload Download::clientDownload;
	std::vector<std::unique_ptr<Download::ScriptDownload>> Download::scriptDownloads;
	std::vector<std::unique_ptr<Download::ScriptPost>> Download::scriptPosts;

	void Download::InitiateMapDownload(const std::string& map, bool needPassword)
	{
		InitiateClientDownload(map, needPassword, true);
	}

	void Download::InitiateClientDownload(const std::string& mod, bool needPassword, bool map, bool downloadOnly)
	{
		if (clientDownload.isRunning)
		{
			return;
		}

		if (mod.empty() || Utils::String::Contains(mod, "..") || Utils::String::Contains(mod, ":")
			|| mod.starts_with("/") || mod.starts_with("\\"))
		{
			Party::ConnectError("Invalid mod or map name from the server.");
			return;
		}

		Scheduler::Once([]
		{
			ui_dl_timeLeft.Set(Utils::String::FormatTimeSpan(0));
			ui_dl_progress.Set("(0/0) %");
			ui_dl_transRate.Set("0.0 MB/s");
		}, Scheduler::Pipeline::MAIN);

		Command::Execute("openmenu mod_download_popmenu", false);

		if (needPassword)
		{
			const auto password = Dvar::Var("password").Get<std::string>();

			if (password.empty())
			{
				Party::ConnectError("A password is required to connect to this server!");
				return;
			}

			clientDownload.hashedPassword = Utils::String::DumpHex(Utils::Cryptography::SHA256::Compute(password), "");
		}

		clientDownload.isRunning = true;
		clientDownload.isMap = map;
		clientDownload.mod = mod;
		clientDownload.shouldTerminate = false;
		clientDownload.isDownloadOnly = downloadOnly;
		clientDownload.totalBytes = 0;
		clientDownload.lastTimeStamp = 0;
		clientDownload.downBytes = 0;
		clientDownload.timeStampBytes = 0;
		clientDownload.isPrivate = needPassword;
		clientDownload.target = Party::Target();
		clientDownload.thread = std::jthread(ModDownloader, &clientDownload);
	}

	bool Download::ParseModList(ClientDownload* download, const std::string& list)
	{
		if (!download)
		{
			return false;
		}

		download->files.clear();

		nlohmann::json listData;

		try
		{
			listData = nlohmann::json::parse(list);
		}
		catch (const nlohmann::json::parse_error& ex)
		{
			Logger::Error("JSON Parse Error: {}\n", ex.what());
			return false;
		}

		if (!listData.is_array())
		{
			return false;
		}

		download->totalBytes = 0;
		const nlohmann::json::array_t listDataArray = listData;

		for (const auto& file : listDataArray)
		{
			if (!file.is_object())
			{
				return false;
			}

			try
			{
				const auto hash = file.at("hash").get<std::string>();
				const auto name = file.at("name").get<std::string>();
				const auto size = file.at("size").get<std::size_t>();

				ClientDownload::File fileEntry;
				fileEntry.name = name;
				fileEntry.hash = hash;
				fileEntry.size = size;
				fileEntry.isMap = download->isMap;

				if (!fileEntry.name.empty() && fileEntry.IsAllowed())
				{
					download->files.push_back(fileEntry);
					download->totalBytes += fileEntry.size;
				}
			}
			catch (const nlohmann::json::exception& ex)
			{
				Logger::Error("JSON Error: {}\n", ex.what());
				return false;
			}
		}

		return true;
	}

	bool Download::DownloadFile(ClientDownload* download, unsigned int index)
	{
		if (!download || download->files.size() <= index)
		{
			return false;
		}

		const auto file = download->files[index];

		assert(file.IsAllowed());

		auto path = download->mod + "/" + file.name;

		if (download->isMap)
		{
			path = "usermaps/" + path;
		}

		if (Utils::IO::FileExists(path))
		{
			const auto data = Utils::IO::ReadFile(path);

			if (data.size() == file.size && Utils::String::DumpHex(Utils::Cryptography::SHA256::Compute(data), "") == file.hash)
			{
				download->totalBytes += file.size;
				return true;
			}
		}

		const auto host = "http://" + download->target.GetString();
		auto fastHost = sv_wwwBaseUrl.Get<std::string>();

		if (Utils::String::StartsWith(fastHost, "https://"))
		{
			download->thread.detach();
			download->Clear();

			Scheduler::Once([]
			{
				Command::Execute("closemenu mod_download_popmenu");
				Party::ConnectError("HTTPS not supported for downloading!");
			}, Scheduler::Pipeline::CLIENT);

			return false;
		}

		if (!Utils::String::StartsWith(fastHost, "http://"))
		{
			fastHost = "http://" + fastHost;
		}

		std::string url;

		if (sv_wwwDownload.Get<bool>())
		{
			if (!Utils::String::EndsWith(fastHost, "/"))
			{
				fastHost.append("/");
			}

			url = fastHost + path;
		}
		else
		{
			url = host + "/file/";

			if (download->isMap)
			{
				url += "map/";
			}

			url += file.name;

			if (download->isPrivate)
			{
				url += "?password=" + download->hashedPassword;
			}
		}

		Logger::Print("Downloading from url {}\n", url);

		FileDownload fileDownload;
		fileDownload.file = file;
		fileDownload.index = index;
		fileDownload.download = download;
		fileDownload.isDownloading = true;
		fileDownload.receivedBytes = 0;

		Utils::String::Replace(url, " ", "%20");

		download->isValid = true;

		Utils::WebIO webIO;
		webIO.SetProgressCallback([&fileDownload, &webIO](std::size_t bytes, std::size_t)
		{
			if (!fileDownload.isDownloading || fileDownload.download->shouldTerminate)
			{
				webIO.CancelDownload();
				return;
			}

			DownloadProgress(&fileDownload, bytes - fileDownload.receivedBytes);
		});

		auto result = false;
		fileDownload.buffer = webIO.Get(url, &result);

		if (!result)
		{
			fileDownload.buffer.clear();
		}

		fileDownload.isDownloading = false;

		download->isValid = false;

		if (fileDownload.buffer.size() != file.size || Utils::Cryptography::SHA256::Compute(fileDownload.buffer, true) != file.hash)
		{
			return false;
		}

		if (download->isMap)
		{
			Utils::IO::CreateDir("usermaps/" + download->mod);
		}

		Utils::IO::WriteFile(path, fileDownload.buffer);

		return true;
	}

	void Download::ModDownloader(ClientDownload* download)
	{
		if (!download)
		{
			download = &clientDownload;
		}

		const auto host = "http://" + download->target.GetString();

		auto listUrl = host;

		if (download->isMap)
		{
			listUrl += "/map";
		}
		else
		{
			listUrl += "/list";
		}

		if (download->isPrivate)
		{
			listUrl += "?password=" + download->hashedPassword;
		}

		const auto list = Utils::WebIO("zw3", listUrl).SetTimeout(5000)->Get();

		if (list.empty())
		{
			if (download->shouldTerminate)
			{
				return;
			}

			download->thread.detach();
			download->Clear();

			Scheduler::Once([]
			{
				Command::Execute("closemenu mod_download_popmenu");
				Party::ConnectError("Failed to download the modlist!");
			}, Scheduler::Pipeline::CLIENT);

			return;
		}

		if (download->shouldTerminate)
		{
			return;
		}

		if (!ParseModList(download, list))
		{
			if (download->shouldTerminate)
			{
				return;
			}

			download->thread.detach();
			download->Clear();

			Scheduler::Once([]
			{
				Command::Execute("closemenu mod_download_popmenu");
				Party::ConnectError("Failed to parse the modlist!");
			}, Scheduler::Pipeline::CLIENT);

			return;
		}

		if (download->shouldTerminate)
		{
			return;
		}

		static std::string mod;
		mod = download->mod;

		for (std::size_t i = 0; i < download->files.size(); ++i)
		{
			if (download->shouldTerminate)
			{
				return;
			}

			if (!DownloadFile(download, static_cast<unsigned int>(i)))
			{
				if (download->shouldTerminate)
				{
					return;
				}

				mod = std::format("Failed to download file: {}!", download->files[i].name);
				download->thread.detach();
				download->Clear();

				Scheduler::Once([]
				{
					Game::Dvar_SetFromStringByName("partyend_reason", mod.data());
					mod.clear();

					Command::Execute("closemenu mod_download_popmenu");
					Command::Execute("openmenu menu_xboxlive_partyended");
				}, Scheduler::Pipeline::CLIENT);

				return;
			}
		}

		if (download->shouldTerminate)
		{
			return;
		}

		download->thread.detach();
		download->Clear();

		if (download->isMap)
		{
			Scheduler::Once([]
			{
				Command::Execute("reconnect", false);
			}, Scheduler::Pipeline::CLIENT);

			return;
		}

		Scheduler::Once([download]
		{
			Game::Dvar_SetString(*Game::fs_gameDirVar, mod.data());

			const auto statFile = (*Game::fs_basepath)->current.string + "\\players\\"s + mod + "\\iw4x.stat";
			const bool statFileExists = Utils::IO::FileExists(statFile);

			Logger::Print("Mod {} downloaded!\n", mod);
			mod.clear();

			Command::Execute("closemenu mod_download_popmenu");

			if (!statFileExists && !download->isDownloadOnly)
			{
				Logger::Print("Opening stats menu...\n");
				Command::Execute("openmenu stats_mod_warning");
				return;
			}

			if (ModList::cl_modVidRestart.Get<bool>())
			{
				Logger::Print("Restarting video...\n");
				Command::Execute("vid_restart");
			}

			if (!download->isDownloadOnly)
			{
				Logger::Print("Reconnecting to server...\n");
				Command::Execute("reconnect");
			}
		}, Scheduler::Pipeline::MAIN);
	}

	void Download::DownloadProgress(FileDownload* fileDownload, std::size_t bytes)
	{
		fileDownload->receivedBytes += bytes;
		fileDownload->download->downBytes += bytes;
		fileDownload->download->timeStampBytes += bytes;

		static volatile bool isFramePushed = false;

		if (!isFramePushed)
		{
			double progress = 0;

			if (fileDownload->download->totalBytes)
			{
				progress = (100.0 / fileDownload->download->totalBytes) * fileDownload->download->downBytes;
			}

			static std::size_t dlIndex, dlSize;
			static std::uint32_t dlProgress;
			dlIndex = fileDownload->index + 1;
			dlSize = fileDownload->download->files.size();
			dlProgress = static_cast<std::uint32_t>(progress);

			isFramePushed = true;

			Scheduler::Once([]
			{
				isFramePushed = false;
				ui_dl_progress.Set(std::format("({}/{}) {}%", dlIndex, dlSize, dlProgress));
			}, Scheduler::Pipeline::MAIN);
		}

		const auto delta = Game::Sys_Milliseconds() - fileDownload->download->lastTimeStamp;

		if (delta > 300)
		{
			const auto doFormat = fileDownload->download->lastTimeStamp != 0;
			fileDownload->download->lastTimeStamp = Game::Sys_Milliseconds();

			const auto dataLeft = fileDownload->download->totalBytes - fileDownload->download->downBytes;

			int timeLeft = 0;

			if (fileDownload->download->timeStampBytes)
			{
				const double timeLeftD = ((1.0 * dataLeft) / fileDownload->download->timeStampBytes) * delta;
				timeLeft = static_cast<int>(timeLeftD);
			}

			if (doFormat)
			{
				static std::size_t dlTsBytes;
				static int dlDelta, dlTimeLeft;
				dlTimeLeft = timeLeft;
				dlDelta = delta;
				dlTsBytes = fileDownload->download->timeStampBytes;

				Scheduler::Once([]
				{
					ui_dl_timeLeft.Set(Utils::String::FormatTimeSpan(dlTimeLeft));
					ui_dl_transRate.Set(Utils::String::FormatBandwidth(dlTsBytes, dlDelta));
				}, Scheduler::Pipeline::MAIN);
			}

			fileDownload->download->timeStampBytes = 0;
		}
	}

	std::jthread Download::serverThread;

	static mg_mgr mgr;
	static std::atomic<bool> isServerTerminating = false;

	void Download::ReplyError(mg_connection* connection, int code, const std::string& messageOverride)
	{
		std::string message;

		switch (code)
		{
		case 400:
			message = "Bad request";
			break;
		case 403:
			message = "Forbidden";
			break;
		case 404:
			message = "Not found";
			break;
		default:
			break;
		}

		if (!messageOverride.empty())
		{
			message = messageOverride;
		}

		mg_http_reply(connection, code, "Content-Type: text/plain\r\n", "%s", message.data());
	}

	void Download::Reply(mg_connection* connection, const std::string& contentType, const std::string& data)
	{
		const auto headers = std::format("Content-Type: {}\r\nAccess-Control-Allow-Origin: *\r\n", contentType);
		mg_http_reply(connection, 200, headers.data(), "%s", data.data());
	}

	static bool VerifyPassword(mg_connection* connection, const mg_http_message* message)
	{
		const auto password = Dvar::Var("g_password").Get<std::string>();

		if (password.empty())
		{
			return true;
		}

		char buffer[128]{};
		const auto length = mg_http_get_var(&message->query, "password", buffer, sizeof(buffer));

		if (length <= 0)
		{
			Download::ReplyError(connection, 403, "Password Required");
			return false;
		}

		if (std::string(buffer, length) != Utils::String::DumpHex(Utils::Cryptography::SHA256::Compute(password), ""))
		{
			Download::ReplyError(connection, 403, "Invalid Password");
			return false;
		}

		return true;
	}

	std::optional<std::string> Download::InfoHandler([[maybe_unused]] mg_connection* connection, [[maybe_unused]] const mg_http_message* message)
	{
		if (!Dvar::Var("sv_running").Get<bool>())
		{
			return std::nullopt;
		}

		std::unordered_map<std::string, nlohmann::json> info;
		info["status"] = ServerInfo::GetInfo().ToJson();
		info["host"] = ServerInfo::GetHostInfo().ToJson();
		info["map_rotation"] = MapRotation::ToJson();
		info["dedicated"] = 0;

		if (Dedicated::com_dedicated)
		{
			info["dedicated"] = Dedicated::com_dedicated->current.integer;
		}

		std::vector<nlohmann::json> players;

		for (std::size_t i = 0; i < Game::MAX_CLIENTS; ++i)
		{
			std::unordered_map<std::string, nlohmann::json> player;
			player["score"] = 0;
			player["ping"] = 0;
			player["name"] = "Unknown Soldier";
			player["test_client"] = 0;

			if (Dedicated::IsRunning())
			{
				const auto& client = Game::svs_clients[i];

				if (client.header.state < Game::CS_ACTIVE || !client.gentity || !client.gentity->client)
				{
					continue;
				}

				player["score"] = Game::G_GetClientScore(static_cast<int>(i));
				player["ping"] = client.ping;
				player["name"] = client.name;
				player["test_client"] = client.bIsTestClient;
			}
			else
			{
				const auto* const name = Game::PartyHost_GetMemberName(Game::g_lobbyData, static_cast<int>(i));

				if (!name || !*name)
				{
					continue;
				}

				player["name"] = name;
			}

			players.emplace_back(player);
		}

		info["players"] = players;

		return nlohmann::json(info).dump();
	}

	std::optional<std::string> Download::ListHandler(mg_connection* connection, const mg_http_message* message)
	{
		static nlohmann::json list;
		static std::filesystem::path previousGame;

		if (!VerifyPassword(connection, message))
		{
			return std::nullopt;
		}

		const std::filesystem::path fsGame = (*Game::fs_gameDirVar)->current.string;

		if (!fsGame.empty() && previousGame != fsGame)
		{
			previousGame = fsGame;

			std::vector<nlohmann::json> files;

			const auto directory = std::filesystem::path((*Game::fs_basepath)->current.string) / fsGame;
			auto names = FileSystem::GetSysFileList(directory.generic_string(), "iwd", false);
			names.emplace_back("mod.ff");

			for (const auto& name : names)
			{
				if (name.find("_svr_") != std::string::npos)
				{
					continue;
				}

				const auto buffer = Utils::IO::ReadFile((directory / name).generic_string());

				if (buffer.empty())
				{
					continue;
				}

				std::unordered_map<std::string, nlohmann::json> file;
				file["name"] = name;
				file["size"] = buffer.size();
				file["hash"] = Utils::Cryptography::SHA256::Compute(buffer, true);

				files.emplace_back(file);
			}

			list = files;
		}

		return list.dump();
	}

	std::optional<std::string> Download::MapHandler(mg_connection* connection, const mg_http_message* message)
	{
		static std::string previousMap;
		static nlohmann::json list;

		if (!VerifyPassword(connection, message))
		{
			return std::nullopt;
		}

		std::string mapName = Maps::GetUserMap()->GetName();

		if (Party::IsInUserMapLobby())
		{
			mapName = Dvar::Var("ui_mapname").Get<std::string>();
		}

		if (!Maps::GetUserMap()->IsValid() && !Party::IsInUserMapLobby())
		{
			previousMap.clear();
			list = {};
		}
		else if (!mapName.empty() && mapName != previousMap)
		{
			std::vector<nlohmann::json> files;

			previousMap = mapName;

			const auto directory = std::filesystem::path((*Game::fs_basepath)->current.string) / "usermaps" / mapName;

			for (const char* const extension : Maps::userMapFiles)
			{
				const auto buffer = Utils::IO::ReadFile(std::format("{}\\{}{}", directory.generic_string(), mapName, extension));

				if (buffer.empty())
				{
					continue;
				}

				std::unordered_map<std::string, nlohmann::json> file;
				file["name"] = mapName + extension;
				file["size"] = buffer.size();
				file["hash"] = Utils::Cryptography::SHA256::Compute(buffer, true);

				files.emplace_back(file);
			}

			list = files;
		}

		return list.dump();
	}

	std::optional<std::string> Download::FileHandler(mg_connection* connection, const mg_http_message* message)
	{
		std::string url(message->uri.ptr, message->uri.len);

		Utils::String::Replace(url, "\\", "/");

		if (url.size() <= 5)
		{
			ReplyError(connection, 400);
			return std::nullopt;
		}

		url = url.substr(6);
		Utils::String::Replace(url, "%20", " ");

		bool isMap = false;

		if (url.starts_with("map/"))
		{
			isMap = true;
			url = url.substr(4);

			std::string mapName = Maps::GetUserMap()->GetName();

			if (Party::IsInUserMapLobby())
			{
				mapName = Dvar::Var("ui_mapname").Get<std::string>();
			}

			bool isValidFile = false;

			for (const char* const extension : Maps::userMapFiles)
			{
				if (url == mapName + extension)
				{
					isValidFile = true;
					break;
				}
			}

			if ((!Maps::GetUserMap()->IsValid() && !Party::IsInUserMapLobby()) || !isValidFile)
			{
				ReplyError(connection, 403);
				return std::nullopt;
			}

			url = std::format("usermaps\\{}\\{}", mapName, url);
		}
		else if ((!url.ends_with(".iwd") && url != "mod.ff") || url.find("_svr_") != std::string::npos)
		{
			ReplyError(connection, 403);
			return std::nullopt;
		}

		const std::string fsGame = (*Game::fs_gameDirVar)->current.string;

		std::string gameFolder;

		if (!isMap)
		{
			gameFolder = fsGame + "\\";
		}

		const auto path = std::format("{}\\{}{}", (*Game::fs_basepath)->current.string, gameFolder, url);

		std::string file;

		if ((!isMap && fsGame.empty()) || !Utils::IO::ReadFile(path, &file))
		{
			ReplyError(connection, 404);
			return std::nullopt;
		}

		mg_printf(connection, "%s", "HTTP/1.1 200 OK\r\n");
		mg_printf(connection, "%s", "Content-Type: application/octet-stream\r\n");
		mg_printf(connection, "Content-Length: %d\r\n", static_cast<int>(file.size()));
		mg_printf(connection, "%s", "Connection: close\r\n");
		mg_printf(connection, "%s", "\r\n");
		mg_send(connection, file.data(), file.size());

		return std::nullopt;
	}

	std::optional<std::string> Download::ServerListHandler([[maybe_unused]] mg_connection* connection, [[maybe_unused]] const mg_http_message* message)
	{
		std::vector<std::string> servers;

		for (const auto& node : Node::GetNodes())
		{
			servers.emplace_back(node.address.GetString());
		}

		return nlohmann::json(servers).dump();
	}

	void Download::EventHandler(mg_connection* connection, int event, void* eventData, [[maybe_unused]] void* userData)
	{
		using Handler = std::function<std::optional<std::string>(mg_connection*, const mg_http_message*)>;

		static const std::vector<std::pair<std::string, Handler>> handlers =
		{
			{ "/file", FileHandler },
			{ "/info", InfoHandler },
			{ "/list", ListHandler },
			{ "/map", MapHandler },
			{ "/serverlist", ServerListHandler },
		};

		if (event != MG_EV_HTTP_MSG)
		{
			return;
		}

		auto* const message = static_cast<mg_http_message*>(eventData);
		const std::string url(message->uri.ptr, message->uri.len);

		bool isHandled = false;

		for (const auto& [prefix, handler] : handlers)
		{
			if (url.starts_with(prefix))
			{
				if (const auto reply = handler(connection, message))
				{
					Reply(connection, "application/json", reply.value());
				}

				isHandled = true;
				break;
			}
		}

		if (!isHandled)
		{
			mg_http_serve_opts options{};
			options.root_dir = "iw4x/html";
			mg_http_serve_dir(connection, message, &options);
		}

		connection->is_resp = 0;
		connection->is_draining = 1;
	}

	constexpr auto scriptRequestLimit = 6;

	Download::ScriptDownload::ScriptDownload(const std::string& url, const unsigned int object) : url(url), object(object), isDone(false), isSuccessful(false), isProgressPending(false), totalSize(0), currentSize(0)
	{
		Game::AddRefToObject(this->object);
	}

	Download::ScriptDownload::~ScriptDownload()
	{
		this->Orphan();

		if (this->workerThread.joinable())
		{
			this->workerThread.join();
		}
	}

	void Download::ScriptDownload::StartWorking()
	{
		if (!this->IsWorking())
		{
			this->workerThread = std::thread(&ScriptDownload::Handler, this);
		}
	}

	bool Download::ScriptDownload::IsWorking() const
	{
		return this->workerThread.joinable();
	}

	bool Download::ScriptDownload::IsDone() const
	{
		return this->isDone;
	}

	void Download::ScriptDownload::Orphan()
	{
		if (this->object)
		{
			Game::RemoveRefToObject(this->object);
			this->object = 0;
		}
	}

	void Download::ScriptDownload::NotifyProgress()
	{
		if (!this->isProgressPending.exchange(false))
		{
			return;
		}

		if (!this->object || !Game::Scr_IsSystemActive())
		{
			return;
		}

		const auto progressString = Game::SL_GetString("progress", 0);

		Game::Scr_AddInt(static_cast<int>(this->totalSize));
		Game::Scr_AddInt(static_cast<int>(this->currentSize));
		Game::Scr_NotifyId(this->object, progressString, 2);

		Game::SL_RemoveRefToString(progressString);
	}

	void Download::ScriptDownload::NotifyDone() const
	{
		if (!this->object || !Game::Scr_IsSystemActive())
		{
			return;
		}

		const auto doneString = Game::SL_GetString("done", 0);

		Game::Scr_AddString(this->result.data());
		Game::Scr_AddInt(this->isSuccessful);
		Game::Scr_NotifyId(this->object, doneString, 2);

		Game::SL_RemoveRefToString(doneString);
	}

	void Download::ScriptDownload::Handler()
	{
		Utils::WebIO webIO("zw3");
		webIO.SetProgressCallback([this](const std::size_t received, const std::size_t total)
		{
			this->currentSize = received;
			this->totalSize = total;
			this->isProgressPending = true;
		});

		this->result = webIO.Get(this->url, &this->isSuccessful);
		this->isDone = true;
	}

	Download::ScriptPost::ScriptPost(const std::string& url, const std::string& body, const unsigned int object) : url(url), body(body), object(object), isDone(false), isSuccessful(false)
	{
		Game::AddRefToObject(this->object);
	}

	Download::ScriptPost::~ScriptPost()
	{
		this->Orphan();

		if (this->workerThread.joinable())
		{
			this->workerThread.join();
		}
	}

	void Download::ScriptPost::StartWorking()
	{
		if (!this->IsWorking())
		{
			this->workerThread = std::thread(&ScriptPost::Handler, this);
		}
	}

	bool Download::ScriptPost::IsWorking() const
	{
		return this->workerThread.joinable();
	}

	bool Download::ScriptPost::IsDone() const
	{
		return this->isDone;
	}

	void Download::ScriptPost::Orphan()
	{
		if (this->object)
		{
			Game::RemoveRefToObject(this->object);
			this->object = 0;
		}
	}

	void Download::ScriptPost::NotifyDone() const
	{
		if (!this->object || !Game::Scr_IsSystemActive())
		{
			return;
		}

		const auto doneString = Game::SL_GetString("done", 0);

		Game::Scr_AddString(this->result.data());
		Game::Scr_AddInt(this->isSuccessful);
		Game::Scr_NotifyId(this->object, doneString, 2);

		Game::SL_RemoveRefToString(doneString);
	}

	void Download::ScriptPost::Handler()
	{
		const Utils::WebIO::Params headers;

		Utils::WebIO webIO("zw3");
		this->result = webIO.Post(this->url, this->body, headers, &this->isSuccessful);
		this->isDone = true;
	}

	Download::Download()
	{
		if (Dedicated::IsEnabled())
		{
			if (!Flags::HasFlag("disable-mongoose"))
			{
				mg_log_set(MG_LL_ERROR);
				mg_mgr_init(&mgr);

				Events::OnNetworkInit([]
				{
					const auto* const listener = mg_http_listen(&mgr, Utils::String::VA(":%hu", Network::GetPort()), EventHandler, &mgr);

					if (!listener)
					{
						Logger::Error("Failed to bind TCP socket, mod download won't work!\n");
						isServerTerminating = true;
					}
				});

				serverThread = Utils::Thread::CreateNamedThread("Mongoose", [](const std::stop_token& stopToken)
				{
					Game::Com_InitThreadData();

					while (!stopToken.stop_requested() && !isServerTerminating)
					{
						mg_mgr_poll(&mgr, 1000);
					}
				});
			}
		}
		else
		{
			Events::OnDvarInit([]
			{
				ui_dl_timeLeft = Dvar::Register("ui_dl_timeLeft", "", Game::DVAR_NONE, "");
				ui_dl_progress = Dvar::Register("ui_dl_progress", "", Game::DVAR_NONE, "");
				ui_dl_transRate = Dvar::Register("ui_dl_transRate", "", Game::DVAR_NONE, "");
			});

			UIScript::Add("mod_download_cancel", []([[maybe_unused]] const UIScript::Token& token)
			{
				clientDownload.Clear();
			});
		}

		Events::OnDvarInit([]
		{
			sv_wwwDownload = Dvar::Register("sv_wwwDownload", false, Game::DVAR_NONE, "Set to true to enable downloading maps/mods from an external server.");
			sv_wwwBaseUrl = Dvar::Register("sv_wwwBaseUrl", "", Game::DVAR_NONE, "Set to the base url for the external map download.");
		});

		Scheduler::Loop([]
		{
			auto workingCount = 0;

			for (std::size_t i = 0; i < scriptDownloads.size();)
			{
				if (scriptDownloads[i]->IsDone())
				{
					const auto download = std::move(scriptDownloads[i]);
					scriptDownloads.erase(scriptDownloads.begin() + static_cast<std::ptrdiff_t>(i));
					download->NotifyDone();
					continue;
				}

				if (scriptDownloads[i]->IsWorking())
				{
					scriptDownloads[i]->NotifyProgress();
					++workingCount;
				}

				++i;
			}

			for (const auto& download : scriptDownloads)
			{
				if (workingCount >= scriptRequestLimit)
				{
					break;
				}

				if (!download->IsWorking())
				{
					download->StartWorking();
					++workingCount;
				}
			}
		}, Scheduler::Pipeline::CLIENT);

		Scheduler::Loop([]
		{
			auto workingCount = 0;

			for (std::size_t i = 0; i < scriptPosts.size();)
			{
				if (scriptPosts[i]->IsDone())
				{
					const auto post = std::move(scriptPosts[i]);
					scriptPosts.erase(scriptPosts.begin() + static_cast<std::ptrdiff_t>(i));
					post->NotifyDone();
					continue;
				}

				if (scriptPosts[i]->IsWorking())
				{
					++workingCount;
				}

				++i;
			}

			for (const auto& post : scriptPosts)
			{
				if (workingCount >= scriptRequestLimit)
				{
					break;
				}

				if (!post->IsWorking())
				{
					post->StartWorking();
					++workingCount;
				}
			}
		}, Scheduler::Pipeline::CLIENT);

		Events::OnVMShutdown([]
		{
			for (const auto& download : scriptDownloads)
			{
				download->Orphan();
			}

			for (const auto& post : scriptPosts)
			{
				post->Orphan();
			}

			std::erase_if(scriptDownloads, [](const std::unique_ptr<ScriptDownload>& download)
			{
				return !download->IsWorking();
			});

			std::erase_if(scriptPosts, [](const std::unique_ptr<ScriptPost>& post)
			{
				return !post->IsWorking();
			});
		});

		GSC::Script::AddFunction("HttpGet", []
		{
			const auto* url = Game::Scr_GetString(0);

			if (!url)
			{
				GSC::Script::Scr_ParamError(0, "^1HttpGet: Illegal parameter!\n");
				return;
			}

			const auto object = Game::AllocObject();
			Game::Scr_AddObject(object);
			scriptDownloads.emplace_back(std::make_unique<ScriptDownload>(url, object));
			Game::RemoveRefToObject(object);
		});

		GSC::Script::AddFunction("HttpPost", []
		{
			const auto* url = Game::Scr_GetString(0);
			const auto* body = Game::Scr_GetString(1);

			if (!url || !body)
			{
				GSC::Script::Scr_ParamError(0, "^1HttpPost: Invalid parameters!\n");
				return;
			}

			const auto object = Game::AllocObject();
			Game::Scr_AddObject(object);
			scriptPosts.emplace_back(std::make_unique<ScriptPost>(url, body, object));
			Game::RemoveRefToObject(object);
		});
	}

	bool Download::ClientDownload::File::IsAllowed() const
	{
		if (Utils::String::Contains(this->name, "..") || Utils::String::Contains(this->name, ":"))
		{
			return false;
		}

		if (Utils::String::Contains(this->name, "\\") || Utils::String::Contains(this->name, "/"))
		{
			return false;
		}

		if (this->isMap)
		{
			if (this->name.ends_with(".arena"))
			{
				return true;
			}

			if (this->name.ends_with(".iwd"))
			{
				return true;
			}

			if (this->name.ends_with(".ff"))
			{
				return true;
			}
		}
		else
		{
			if (this->name == "mod.ff")
			{
				return true;
			}

			if (this->name.ends_with(".iwd"))
			{
				return true;
			}
		}

		return false;
	}
}
