#include "STDInclude.hpp"

#include "WebIO.hpp"

#include "Components/Modules/Leaderboard.hpp"

namespace Utils
{
	WebIO::WebIO() : WebIO("WebIO")
	{
	}

	WebIO::WebIO(const std::string& userAgent, const std::string& url) : WebIO(userAgent)
	{
		this->SetURL(url);
	}

	WebIO::WebIO(const std::string& userAgent) : isCancelled(false), session(nullptr), connection(nullptr), request(nullptr), timeoutMs(5000)
	{
		this->OpenSession(userAgent);
	}

	WebIO::~WebIO()
	{
		this->CloseConnection();
		this->CloseSession();
	}

	void WebIO::OpenSession(const std::string& userAgent)
	{
		this->CloseSession();
		this->session = InternetOpenA(userAgent.data(), INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
	}

	void WebIO::CloseSession()
	{
		if (this->session)
		{
			InternetCloseHandle(this->session);
			this->session = nullptr;
		}
	}

	void WebIO::SetURL(std::string url)
	{
		this->parsedURL.server.clear();
		this->parsedURL.protocol.clear();
		this->parsedURL.document.clear();

		if (url.find("://") == std::string::npos)
		{
			url = "http://" + url;
		}

		PARSEDURLA parsed{};
		parsed.cbSize = sizeof(parsed);
		ParseURLA(url.data(), &parsed);

		if (parsed.cchProtocol && parsed.pszProtocol)
		{
			for (UINT i = 0; i < parsed.cchProtocol; ++i)
			{
				this->parsedURL.protocol.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(parsed.pszProtocol[i]))));
			}
		}
		else
		{
			this->parsedURL.protocol.append("http");
		}

		std::string server;

		if (!parsed.cchSuffix || !parsed.pszSuffix)
		{
			return;
		}

		server.append(parsed.pszSuffix, parsed.cchSuffix);

		if (server.starts_with("//"))
		{
			server = server.substr(2);
		}

		auto pos = server.find('/');

		if (pos == std::string::npos)
		{
			this->parsedURL.server = server;
			this->parsedURL.document = "/";
		}
		else
		{
			this->parsedURL.server = server.substr(0, pos);
			this->parsedURL.document = server.substr(pos);
		}

		this->parsedURL.port.clear();

		pos = this->parsedURL.server.find(':');

		if (pos != std::string::npos)
		{
			this->parsedURL.port = this->parsedURL.server.substr(pos + 1);
			this->parsedURL.server = this->parsedURL.server.substr(0, pos);
		}

		this->parsedURL.raw.clear();
		this->parsedURL.raw.append(this->parsedURL.protocol);
		this->parsedURL.raw.append("://");
		this->parsedURL.raw.append(this->parsedURL.server);

		if (!this->parsedURL.port.empty())
		{
			this->parsedURL.raw.append(":");
			this->parsedURL.raw.append(this->parsedURL.port);
		}

		this->parsedURL.raw.append(this->parsedURL.document);
	}

	std::string WebIO::Get(const std::string& url, bool* success)
	{
		this->SetURL(url);
		return this->Get(success);
	}

	std::string WebIO::Get(bool* success)
	{
		const Params params;
		return this->Execute("GET", "", params, success);
	}

	std::string WebIO::Get(const Params& headers, bool* success)
	{
		return this->Execute("GET", "", headers, success);
	}

	std::string WebIO::Post(const std::string& url, const std::string& body, const Params& headers, bool* success)
	{
		this->SetURL(url);
		return this->Execute("POST", body, headers, success);
	}

	bool WebIO::OpenConnection()
	{
		WORD port = INTERNET_DEFAULT_HTTP_PORT;

		if (this->IsSecuredConnection())
		{
			port = INTERNET_DEFAULT_HTTPS_PORT;
		}

		if (!this->parsedURL.port.empty())
		{
			port = static_cast<WORD>(std::atoi(this->parsedURL.port.data()));
		}

		this->connection = InternetConnectA(this->session, this->parsedURL.server.data(), port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);

		return this->connection && this->connection != INVALID_HANDLE_VALUE;
	}

	void WebIO::CloseConnection()
	{
		if (this->request && this->request != INVALID_HANDLE_VALUE)
		{
			InternetCloseHandle(this->request);
		}

		if (this->connection && this->connection != INVALID_HANDLE_VALUE)
		{
			InternetCloseHandle(this->connection);
		}

		this->request = nullptr;
		this->connection = nullptr;
	}

	WebIO* WebIO::SetTimeout(DWORD milliseconds)
	{
		this->timeoutMs = milliseconds;
		return this;
	}

	WebIO* WebIO::SetReadHttpErrorBody(const bool shouldRead)
	{
		this->shouldReadHttpErrorBody = shouldRead;
		return this;
	}

	std::string WebIO::Execute(const char* command, const std::string& body, const Params& headers, bool* success)
	{
		if (success)
		{
			*success = false;
		}

		if (!this->session || !this->OpenConnection())
		{
			return {};
		}

		static const char* acceptTypes[] = { "application/x-www-form-urlencoded", "application/json", nullptr };

		DWORD flags = INTERNET_FLAG_RELOAD;

		if (this->IsSecuredConnection())
		{
			flags |= INTERNET_FLAG_SECURE;
		}

		InternetSetOptionA(this->connection, INTERNET_OPTION_CONNECT_TIMEOUT, &this->timeoutMs, sizeof(this->timeoutMs));
		InternetSetOptionA(this->connection, INTERNET_OPTION_RECEIVE_TIMEOUT, &this->timeoutMs, sizeof(this->timeoutMs));
		InternetSetOptionA(this->connection, INTERNET_OPTION_SEND_TIMEOUT, &this->timeoutMs, sizeof(this->timeoutMs));

		this->request = HttpOpenRequestA(this->connection, command, this->parsedURL.document.data(), nullptr, nullptr, acceptTypes, flags, 0);

		if (!this->request || this->request == INVALID_HANDLE_VALUE)
		{
			this->CloseConnection();
			return {};
		}

		auto params = headers;

		if (!params.contains("Content-Type"))
		{
			params["Content-Type"] = "application/json";

			if (this->IsZw3Host())
			{
				params["Authorization"] = std::format("Bearer {}", Components::Leaderboard::GetApiKey());
			}
		}

		std::string finalHeaders;

		for (const auto& [name, value] : params)
		{
			finalHeaders.append(name);
			finalHeaders.append(": ");
			finalHeaders.append(value);
			finalHeaders.append("\r\n");
		}

		if (!HttpSendRequestA(this->request, finalHeaders.data(), static_cast<DWORD>(finalHeaders.size()), const_cast<char*>(body.data()), static_cast<DWORD>(body.size())))
		{
			this->CloseConnection();
			return {};
		}

		DWORD statusCode = 404;
		DWORD length = sizeof(statusCode);

		if (!HttpQueryInfoA(this->request, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_STATUS_CODE, &statusCode, &length, nullptr))
		{
			this->CloseConnection();
			return {};
		}

		constexpr std::size_t maxErrorBodySize = 65536;

		const bool isAcceptedStatus = statusCode == 200 || statusCode == 201 || statusCode == 304;
		const bool isReadableError = this->shouldReadHttpErrorBody && statusCode >= 400 && statusCode <= 599;

		if (!isAcceptedStatus && !isReadableError)
		{
			this->CloseConnection();
			return {};
		}

		DWORD contentLength = 0;
		length = sizeof(contentLength);

		if (!HttpQueryInfoA(this->request, HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_CONTENT_LENGTH, &contentLength, &length, nullptr))
		{
			contentLength = 0;
		}

		std::string returnBuffer;

		if (isAcceptedStatus)
		{
			returnBuffer.reserve(contentLength);
		}
		else
		{
			returnBuffer.reserve(std::min<std::size_t>(contentLength, maxErrorBodySize));
		}

		DWORD size = 0;
		char buffer[0x2001]{};

		while (InternetReadFile(this->request, buffer, sizeof(buffer) - 1, &size))
		{
			if (this->isCancelled)
			{
				this->CloseConnection();
				return {};
			}

			returnBuffer.append(buffer, size);

			if (!isAcceptedStatus && returnBuffer.size() > maxErrorBodySize)
			{
				this->CloseConnection();
				return {};
			}

			if (this->progressCallback)
			{
				this->progressCallback(returnBuffer.size(), contentLength);
			}

			if (!size)
			{
				break;
			}
		}

		this->CloseConnection();

		if (success)
		{
			*success = isAcceptedStatus;
		}

		return returnBuffer;
	}

	bool WebIO::IsSecuredConnection() const
	{
		return this->parsedURL.protocol == "https";
	}

	bool WebIO::IsZw3Host() const
	{
		const auto server = String::ToLower(this->parsedURL.server);

		return server == "zw3.eu" || server.ends_with(".zw3.eu");
	}

	void WebIO::SetProgressCallback(const std::function<void(std::size_t, std::size_t)>& callback)
	{
		this->progressCallback = callback;
	}

	void WebIO::CancelDownload()
	{
		this->isCancelled = true;
	}
}
