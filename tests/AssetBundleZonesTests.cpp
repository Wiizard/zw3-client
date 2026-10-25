#include "Components/Modules/AssetBundleZones.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
	struct TestDirectory
	{
		std::filesystem::path path = std::filesystem::absolute("build") /
			("asset-bundle-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
		TestDirectory() { std::filesystem::create_directories(path); }
		~TestDirectory()
		{
			std::error_code ignored;
			for (std::filesystem::directory_iterator it(path, ignored);
				!ignored && it != std::filesystem::directory_iterator{}; it.increment(ignored))
			{
				std::filesystem::remove(it->path(), ignored);
			}
			std::filesystem::remove(path, ignored);
		}
		void File(const char* name) const { std::ofstream(path / name) << "test bundle"; }
	};
	void Require(bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		using namespace Components::AssetBundleZones;
		TestDirectory temporary;
		std::error_code error;
		Require(Discover(temporary.path / "missing", error).empty() && !error,
			"missing optional directory must not prevent startup");
		Require(Discover(temporary.path, error).empty() && !error,
			"no asset bundles must be a valid startup state");

		temporary.File("zw3.ff");
		temporary.File("zw3_common.ff");
		temporary.File("zw3_assets_2.ff");
		temporary.File("ZW3_ASSETS_0.FF");
		temporary.File("zw3_assets_1.ff");
		temporary.File("zw3_assets_extra.ff");
		temporary.File("other_zw3_assets.ff");
		temporary.File("zw3_assets_3.ff.bak");
		temporary.File("zw3_assets_4.iwd");
		std::filesystem::create_directory(temporary.path / "zw3_assets_directory.ff");

		const std::vector<std::string> numbered{ "zw3_assets_0", "zw3_assets_1", "zw3_assets_2", "zw3_assets_extra" };
		Require(Discover(temporary.path, error) == numbered && !error,
			"numbered bundles must load without zw3_assets.ff and exclude unrelated files/directories");
		temporary.File("zw3_assets.ff");
		const std::vector<std::string> all{ "zw3_assets", "zw3_assets_0", "zw3_assets_1", "zw3_assets_2", "zw3_assets_extra" };
		Require(Discover(temporary.path, error) == all && !error,
			"the base bundle and all matching extensions must have a deterministic load order");
		std::filesystem::remove(temporary.path / "zw3_assets_1.ff");
		Require(Discover(temporary.path, error).size() == 4 && !error,
			"gaps in numbered bundles must not stop discovery");
		Require(IsZoneName("zw3_assetsAnything") && !IsZoneName("zw3_asset") && !IsZoneName("other_zw3_assets"),
			"the requested prefix must match exactly at the beginning");

		std::cout << "Asset bundles: optional absence, numbered-only sets, prefix/extension filtering and stable ordering passed\n";
		return 0;
	}
	catch (const std::exception& failure)
	{
		std::cerr << failure.what() << '\n';
		return 1;
	}
}
