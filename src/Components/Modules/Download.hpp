#pragma once

#include <Game/Functions.hpp>
#include <Utils/WebIO.hpp>


struct mg_connection;
struct mg_http_message;

namespace Components
{
	class Download : public Component
	{
	public:
		Download();
		~Download();

		void preDestroy() override;

		static void InitiateClientDownload(const std::string& mod, bool needPassword, bool map = false);
		static void InitiateMapDownload(const std::string& map, bool needPassword);

		static void ReplyError(mg_connection* connection, int code, std::string messageOverride = {});

		static Dvar::Var SV_wwwDownload;
		static Dvar::Var SV_wwwBaseUrl;

		static Dvar::Var UIDlTimeLeft;
		static Dvar::Var UIDlProgress;
		static Dvar::Var UIDlTransRate;

	private:
		class ClientDownload
		{
		public:
			ClientDownload(bool isMap = false) : running_(false), valid_(false), terminateThread_(false), isMap_(isMap), totalBytes_(0), downBytes_(0), lastTimeStamp_(0), timeStampBytes_(0) {}
			~ClientDownload() { this->clear(); }

			bool running_;
			bool valid_;
			bool terminateThread_;
			bool isMap_;
			bool isPrivate_;
			Network::Address target_;
			std::string hashedPassword_;
			std::string mod_;
			std::thread thread_;

			std::size_t totalBytes_;
			std::size_t downBytes_;

			int lastTimeStamp_;
			std::size_t timeStampBytes_;

			class File
			{
			public:
				std::string name;
				std::string hash;
				std::size_t size;
				bool isMap;

				bool allowed() const;
			};

			std::vector<File> files_;

			void clear()
			{
				this->terminateThread_ = true;

				if (this->thread_.joinable())
				{
					this->thread_.join();
				}

				this->running_ = false;
				this->mod_.clear();
				this->files_.clear();

				if (this->valid_)
				{
					this->valid_ = false;
				}
			}

		};

		class FileDownload
		{
		public:
			ClientDownload* download;
			ClientDownload::File file;

			int timestamp;
			bool downloading;
			unsigned int index;
			std::string buffer;
			std::size_t receivedBytes;
		};

		class ScriptDownload
		{
		public:
			ScriptDownload(const std::string& _url, unsigned int _object) : url(_url), object(_object), webIO(nullptr), done(false), notifyRequired(false), totalSize(0), currentSize(0)
			{
				Game::AddRefToObject(this->getObject());
			}

			ScriptDownload(ScriptDownload&& other) noexcept = delete;
			ScriptDownload& operator=(ScriptDownload&& other) noexcept = delete;

			~ScriptDownload()
			{
				if (this->getObject())
				{
					Game::RemoveRefToObject(this->getObject());
					this->object = 0;
				}

				if (this->workerThread.joinable())
				{
					this->workerThread.join();
				}

				this->destroyWebIO();
			}

			void startWorking()
			{
				if (!this->isWorking())
				{
					this->workerThread = std::thread(std::bind(&ScriptDownload::handler, this));
				}
			}

			bool isWorking()
			{
				return this->workerThread.joinable();
			}

			void notifyProgress()
			{
				if (this->notifyRequired)
				{
					this->notifyRequired = false;

					if (Game::Scr_IsSystemActive())
					{
						Game::Scr_AddInt(static_cast<int>(this->totalSize));
						Game::Scr_AddInt(static_cast<int>(this->currentSize));
						Game::Scr_NotifyId(this->getObject(), Game::SL_GetString("progress", 0), 2);
					}
				}
			}

			void updateProgress(size_t _currentSize, size_t _toalSize)
			{
				this->currentSize = _currentSize;
				this->totalSize = _toalSize;
				this->notifyRequired = true;
			}

			void notifyDone()
			{
				if (!this->isDone()) return;

				if (Game::Scr_IsSystemActive())
				{
					Game::Scr_AddString(this->result.data()); // No binary data supported yet
					Game::Scr_AddInt(this->success);
					Game::Scr_NotifyId(this->getObject(), Game::SL_GetString("done", 0), 2);
				}
			}

			bool isDone() { return this->done; };

			std::string getUrl() { return this->url; }
			unsigned int getObject() { return this->object; }

			void cancel()
			{
				if (this->webIO)
				{
					this->webIO->cancelDownload();
				}
			}

		private:
			std::string url;
			std::string result;
			unsigned int object;
			std::thread workerThread;
			Utils::WebIO* webIO;

			bool done;
			bool success;
			bool notifyRequired;
			size_t totalSize;
			size_t currentSize;

			void handler()
			{
				this->destroyWebIO();

				this->webIO = new Utils::WebIO("ZW3-client");
				this->webIO->setProgressCallback(std::bind(&ScriptDownload::updateProgress, this, std::placeholders::_1, std::placeholders::_2));

				this->result = this->webIO->get(this->url, &this->success);

				this->destroyWebIO();
				this->done = true;
			}

			void destroyWebIO()
			{
				if (this->webIO)
				{
					delete this->webIO;
					this->webIO = nullptr;
				}
			}
		};

		class ScriptPost
		{
		public:
			ScriptPost(const std::string& _url, const std::string& _postData, unsigned int _object)
				: url(_url), postData(_postData), object(_object), webIO(nullptr), done(false), success(false)
			{
				Game::AddRefToObject(this->object);
			}

			~ScriptPost()
			{
				if (this->object)
				{
					Game::RemoveRefToObject(this->object);
					this->object = 0;
				}

				if (this->workerThread.joinable())
				{
					this->workerThread.join();
				}

				this->destroyWebIO();
			}

			void startWorking()
			{
				if (!this->isWorking())
				{
					this->workerThread = std::thread(&ScriptPost::handler, this);
				}
			}

			bool isWorking()
			{
				return this->workerThread.joinable();
			}

			void notifyProgress() {}

			void notifyDone()
			{
				if (!this->done || !Game::Scr_IsSystemActive())
					return;

				Game::Scr_AddString(this->result.data());
				Game::Scr_AddInt(this->success);
				Game::Scr_NotifyId(this->object, Game::SL_GetString("done", 0), 2);
			}

			bool isDone() { return this->done; }

		private:
			std::string url;
			std::string postData;
			std::string result;
			unsigned int object;
			std::thread workerThread;
			Utils::WebIO* webIO;

			bool done;
			bool success;

			void handler()
			{
				this->destroyWebIO();

				this->webIO = new Utils::WebIO("ZW3-client");

				this->result = this->webIO->post(this->url, this->postData, &this->success);

				this->destroyWebIO();
				this->done = true;
			}

			void destroyWebIO()
			{
				if (this->webIO)
				{
					delete this->webIO;
					this->webIO = nullptr;
				}
			}
		};

		static ClientDownload CLDownload;
		static std::vector<std::shared_ptr<ScriptDownload>> ScriptDownloads;
		static std::vector<std::shared_ptr<ScriptPost>> ScriptPosts;
		static std::thread ServerThread;
		static volatile bool Terminate;
		static bool ServerRunning;

		static std::string MongooseLogBuffer;

		static void DownloadProgress(FileDownload* fDownload, std::size_t bytes);

		static void ModDownloader(ClientDownload* download);
		static bool ParseModList(ClientDownload* download, const std::string& list);
		static bool DownloadFile(ClientDownload* download, unsigned int index);

		static void LogFn(char c, void* param);
		static void Reply(mg_connection* connection, const std::string& contentType, const std::string& data);

		static std::optional<std::string> FileHandler(mg_connection* c, const mg_http_message* hm);
		static void EventHandler(mg_connection* c, const int ev, void* ev_data, void* fn_data);
		static std::optional<std::string> ListHandler(mg_connection* c, const mg_http_message* hm);
		static std::optional<std::string> InfoHandler(mg_connection* c, const mg_http_message* hm);
		static std::optional<std::string> ServerListHandler(mg_connection* c, const mg_http_message* hm);
		static std::optional<std::string> MapHandler(mg_connection* c, const mg_http_message* hm);
	};
}
