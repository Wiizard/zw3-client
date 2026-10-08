#include "STDInclude.hpp"

namespace Utils
{
	const char* Cache::urls[] =
	{
		"https://raw.githubusercontent.com/iw4x/iw4x-cache",
		"https://iw4x.dev/v1",
	};

	std::string Cache::validUrl;
	std::mutex Cache::cacheMutex;

	std::string Cache::GetUrl(const std::string& url, const std::string& path)
	{
		return url + path;
	}

	std::string Cache::GetFile(const std::string& path, int timeout, const std::string& useragent)
	{
		std::lock_guard _(cacheMutex);

		if (validUrl.empty())
		{
			for (const auto* url : urls)
			{
				std::string result = WebIO(useragent, GetUrl(url, path)).SetTimeout(timeout)->Get();

				if (!result.empty())
				{
					validUrl = url;
					return result;
				}
			}

			return {};
		}

		return WebIO(useragent, GetUrl(validUrl, path)).SetTimeout(timeout)->Get();
	}
}
