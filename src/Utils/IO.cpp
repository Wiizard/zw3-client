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
		if (!data)
		{
			return false;
		}

		data->clear();

		if (!FileExists(file))
		{
			return false;
		}

		std::ifstream stream(file, std::ios::binary);

		if (!stream.is_open())
		{
			return false;
		}

		stream.seekg(0, std::ios::end);
		const std::streamsize size = stream.tellg();
		stream.seekg(0, std::ios::beg);

		if (size < 0)
		{
			return false;
		}

		data->resize(static_cast<std::string::size_type>(size));
		stream.read(data->data(), size);
		stream.close();

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
		if (!FileExists(file))
		{
			return 0;
		}

		std::ifstream stream(file, std::ios::binary);

		if (!stream.good())
		{
			return 0;
		}

		stream.seekg(0, std::ios::end);

		return static_cast<std::size_t>(stream.tellg());
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
