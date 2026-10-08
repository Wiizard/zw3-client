#pragma once

namespace Utils
{
	class WebIO
	{
	public:
		using Params = std::map<std::string, std::string>;

		WebIO();
		WebIO(const std::string& userAgent);
		WebIO(const std::string& userAgent, const std::string& url);
		~WebIO();

		WebIO(const WebIO&) = delete;
		WebIO& operator=(const WebIO&) = delete;

		void SetURL(std::string url);

		std::string Get(const std::string& url, bool* success = nullptr);
		std::string Get(bool* success = nullptr);
		std::string Get(const Params& headers, bool* success = nullptr);

		std::string Post(const std::string& url, const std::string& body, const Params& headers, bool* success = nullptr);

		WebIO* SetTimeout(DWORD milliseconds);
		WebIO* SetReadHttpErrorBody(bool shouldRead);

		void SetProgressCallback(const std::function<void(std::size_t, std::size_t)>& callback);
		void CancelDownload();

	private:
		struct WebURL
		{
			std::string protocol;
			std::string server;
			std::string document;
			std::string port;
			std::string raw;
		};

		bool isCancelled;

		WebURL parsedURL;

		HINTERNET session;
		HINTERNET connection;
		HINTERNET request;

		DWORD timeoutMs;
		bool shouldReadHttpErrorBody = false;

		std::function<void(std::size_t, std::size_t)> progressCallback;

		[[nodiscard]] bool IsSecuredConnection() const;
		[[nodiscard]] bool IsZw3Host() const;

		std::string Execute(const char* command, const std::string& body, const Params& headers, bool* success = nullptr);

		void OpenSession(const std::string& userAgent);
		void CloseSession();

		bool OpenConnection();
		void CloseConnection();
	};
}
