#pragma once

#include <filesystem>
#include <fstream>
#include <string_view>

namespace Utils
{
	class RetainedLog
	{
		std::ofstream file;
		std::streamoff bytes = 0;
		std::streamoff limit;

	public:
		explicit RetainedLog(const std::filesystem::path& path, std::streamoff maximumBytes,
			unsigned previousLogs = 8) : limit(maximumBytes)
		{
			std::error_code error;
			// Keep prior runs before opening the new log, including an abruptly ended run.
			for (unsigned i = previousLogs; i > 0; --i)
			{
				const auto destination = std::filesystem::path(path.wstring() + L".previous" + std::to_wstring(i));
				const auto source = i == 1 ? path :
					std::filesystem::path(path.wstring() + L".previous" + std::to_wstring(i - 1));
				if (!std::filesystem::exists(source, error)) continue;
				std::filesystem::remove(destination, error);
				error.clear();
				std::filesystem::rename(source, destination, error);
				// A second instance must not truncate the first instance's open log.
				if (error && i == 1) return;
			}
			file.open(path, std::ios::binary | std::ios::trunc);
		}

		void Write(std::string_view text)
		{
			if (!file || static_cast<std::streamoff>(text.size()) > limit - bytes) return;
			file.write(text.data(), static_cast<std::streamsize>(text.size()));
			file.flush();
			bytes += static_cast<std::streamoff>(text.size());
		}
	};
}
