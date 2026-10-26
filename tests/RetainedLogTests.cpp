#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include "Utils/RetainedLog.hpp"

#include <chrono>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

static void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

static std::string Read(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

int main()
{
	const auto directory = std::filesystem::temp_directory_path() /
		("zw3-retained-log-test-" + std::to_string(GetCurrentProcessId()) + "-" +
			std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	try
	{
		std::filesystem::create_directory(directory);
		const auto path = directory / "startup.log";
		for (unsigned session = 0; session < 10; ++session)
		{
			Utils::RetainedLog log(path, 64);
			log.Write("session " + std::to_string(session) + "\n");
			Require(Read(path) == "session " + std::to_string(session) + "\n",
				"A startup milestone was not flushed before process termination");
		}
		for (unsigned i = 1; i <= 8; ++i)
			Require(Read(path.wstring() + L".previous" + std::to_wstring(i)) ==
				"session " + std::to_string(9 - i) + "\n", "A previous startup log was overwritten");
		Require(!std::filesystem::exists(path.wstring() + L".previous9"), "Log retention was unbounded");
		{
			Utils::RetainedLog log(directory / "bounded.log", 5);
			log.Write("12345");
			log.Write("6");
		}
		Require(Read(directory / "bounded.log") == "12345", "Diagnostic log exceeded its size limit");
		{
			Utils::RetainedLog unavailable(directory / "missing" / "startup.log", 64);
			unavailable.Write("Logging failure must not prevent startup\n");
		}
		std::filesystem::remove_all(directory);
		std::cout << "Retained logs: eight prior runs, immediate flush, size cap and unavailable path passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::error_code ignored;
		std::filesystem::remove_all(directory, ignored);
		std::cerr << error.what() << '\n';
		return 1;
	}
}
