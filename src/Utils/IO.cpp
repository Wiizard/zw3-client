#include "STDInclude.hpp"

namespace Utils::IO
{
	bool FileExists(const std::string& file)
	{
		return GetFileAttributesA(file.data()) != INVALID_FILE_ATTRIBUTES;
	}

	bool WriteFile(const std::string& file, const std::string& data, bool append)
	{
		const auto separator = file.find_last_of("/\\");

		if (separator != std::string::npos)
		{
			CreateDir(file.substr(0, separator));
		}

		const auto mode = std::ios::binary | std::ofstream::out
			| (append ? std::ofstream::app : std::ofstream::out);

		std::ofstream stream(file, mode);

		if (!stream.is_open())
		{
			return false;
		}

		stream.write(data.data(), static_cast<std::streamsize>(data.size()));
		stream.close();

		return true;
	}

	bool ReadFile(const std::string& file, std::string* data)
	{
		if (!data) return false;
		data->clear();
		const HANDLE raw = CreateFileA(file.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
		if (raw == INVALID_HANDLE_VALUE) return false;
		const std::unique_ptr<void, decltype(&CloseHandle)> handle(raw, &CloseHandle);
		LARGE_INTEGER length{};
		if (!GetFileSizeEx(raw, &length) || length.QuadPart < 0
			|| static_cast<unsigned long long>(length.QuadPart) > data->max_size()) return false;
		data->resize(static_cast<std::size_t>(length.QuadPart));
		std::size_t offset = 0;
		while (offset < data->size())
		{
			const DWORD requested = static_cast<DWORD>(std::min<std::size_t>(data->size() - offset, 16u * 1024u * 1024u));
			DWORD received = 0;
			if (!::ReadFile(raw, data->data() + offset, requested, &received, nullptr) || !received)
			{
				data->clear();
				return false;
			}
			offset += received;
		}
		return true;
	}
	std::string ReadFile(const std::string& file)
	{
		std::string data;
		ReadFile(file, &data);

		return data;
	}

	bool RemoveFile(const std::string& file)
	{
		return DeleteFileA(file.data()) == TRUE;
	}

	std::size_t FileSize(const std::string& file)
	{
		WIN32_FILE_ATTRIBUTE_DATA attributes{};
		if (!GetFileAttributesExA(file.c_str(), GetFileExInfoStandard, &attributes)
			|| (attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
		const std::uint64_t size = (static_cast<std::uint64_t>(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
		return size <= std::numeric_limits<std::size_t>::max() ? static_cast<std::size_t>(size) : 0;
	}
	bool CreateDir(const std::string& directory)
	{
		std::error_code error;

		return std::filesystem::create_directories(directory, error);
	}

	bool DirectoryExists(const std::filesystem::path& directory)
	{
		std::error_code error;

		return std::filesystem::is_directory(directory, error);
	}

	bool DirectoryIsEmpty(const std::filesystem::path& directory)
	{
		std::error_code error;

		return std::filesystem::is_empty(directory, error);
	}

	std::vector<std::filesystem::directory_entry> ListFiles(
		const std::filesystem::path& directory, bool recursive)
	{
		std::vector<std::filesystem::directory_entry> files;
		std::error_code error;

		if (recursive)
		{
			for (const auto& file : std::filesystem::recursive_directory_iterator(directory, error))
			{
				files.push_back(file);
			}

			return files;
		}

		for (const auto& file : std::filesystem::directory_iterator(directory, error))
		{
			files.push_back(file);
		}

		return files;
	}
}
