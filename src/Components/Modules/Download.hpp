#pragma once

#include "Dvar.hpp"
#include "Network.hpp"

struct mg_connection;
struct mg_http_message;

namespace Components
{
	class Download : public Component
	{
	public:
		Download();

		static void InitiateClientDownload(const std::string& mod, bool needPassword, bool map = false, bool downloadOnly = false);
		static void InitiateMapDownload(const std::string& map, bool needPassword);

		static void ReplyError(mg_connection* connection, int code, const std::string& messageOverride = {});

		static Dvar::Var sv_wwwDownload;
		static Dvar::Var sv_wwwBaseUrl;

		static Dvar::Var ui_dl_timeLeft;
		static Dvar::Var ui_dl_progress;
		static Dvar::Var ui_dl_transRate;

	private:
		class ClientDownload
		{
		public:
			ClientDownload(bool isMap = false, bool isDownloadOnly = false) : isRunning(false), isValid(false), shouldTerminate(false), isDownloadOnly(isDownloadOnly), isMap(isMap), isPrivate(false), totalBytes(0), downBytes(0), lastTimeStamp(0), timeStampBytes(0) {}
			~ClientDownload() { this->Clear(); }

			bool isRunning;
			bool isValid;
			bool shouldTerminate;
			bool isDownloadOnly;
			bool isMap;
			bool isPrivate;
			Network::Address target;
			std::string hashedPassword;
			std::string mod;
			std::jthread thread;

			std::size_t totalBytes;
			std::size_t downBytes;

			int lastTimeStamp;
			std::size_t timeStampBytes;

			class File
			{
			public:
				std::string name;
				std::string hash;
				std::size_t size;
				bool isMap;

				[[nodiscard]] bool IsAllowed() const;
			};

			std::vector<File> files;

			void Clear()
			{
				this->shouldTerminate = true;

				if (this->thread.joinable())
				{
					this->thread.join();
				}

				this->isRunning = false;
				this->mod.clear();
				this->files.clear();
				this->isValid = false;
			}
		};

		class FileDownload
		{
		public:
			ClientDownload* download;
			ClientDownload::File file;

			int timestamp;
			bool isDownloading;
			unsigned int index;
			std::string buffer;
			std::size_t receivedBytes;
		};

		class ScriptDownload
		{
		public:
			ScriptDownload(const std::string& url, unsigned int object);
			~ScriptDownload();

			ScriptDownload(const ScriptDownload&) = delete;
			ScriptDownload& operator=(const ScriptDownload&) = delete;

			void StartWorking();
			[[nodiscard]] bool IsWorking() const;
			[[nodiscard]] bool IsDone() const;
			void NotifyProgress();
			void NotifyDone() const;
			void Orphan();

		private:
			std::string url;
			std::string result;
			unsigned int object;
			std::thread workerThread;

			std::atomic<bool> isDone;
			bool isSuccessful;
			std::atomic<bool> isProgressPending;
			std::atomic<std::size_t> totalSize;
			std::atomic<std::size_t> currentSize;

			void Handler();
		};

		class ScriptPost
		{
		public:
			ScriptPost(const std::string& url, const std::string& body, unsigned int object);
			~ScriptPost();

			ScriptPost(const ScriptPost&) = delete;
			ScriptPost& operator=(const ScriptPost&) = delete;

			void StartWorking();
			[[nodiscard]] bool IsWorking() const;
			[[nodiscard]] bool IsDone() const;
			void NotifyDone() const;
			void Orphan();

		private:
			std::string url;
			std::string body;
			std::string result;
			unsigned int object;
			std::thread workerThread;

			std::atomic<bool> isDone;
			bool isSuccessful;

			void Handler();
		};

		static ClientDownload clientDownload;
		static std::vector<std::unique_ptr<ScriptDownload>> scriptDownloads;
		static std::vector<std::unique_ptr<ScriptPost>> scriptPosts;

		static void DownloadProgress(FileDownload* fileDownload, std::size_t bytes);

		static void ModDownloader(ClientDownload* download);
		static bool ParseModList(ClientDownload* download, const std::string& list);
		static bool DownloadFile(ClientDownload* download, unsigned int index);

		static std::jthread serverThread;

		static void Reply(mg_connection* connection, const std::string& contentType, const std::string& data);

		static std::optional<std::string> FileHandler(mg_connection* connection, const mg_http_message* message);
		static std::optional<std::string> InfoHandler(mg_connection* connection, const mg_http_message* message);
		static std::optional<std::string> ListHandler(mg_connection* connection, const mg_http_message* message);
		static std::optional<std::string> MapHandler(mg_connection* connection, const mg_http_message* message);
		static std::optional<std::string> ServerListHandler(mg_connection* connection, const mg_http_message* message);
		static void EventHandler(mg_connection* connection, int event, void* eventData, void* userData);
	};
}
