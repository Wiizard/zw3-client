#include "Utils/PlaylistImage.hpp"
#include <cassert>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

static std::string responseDigest;
static bool httpSuccess = true;
static int requests = 0;
namespace Utils
{
	struct Cryptography { struct SHA256 { static std::string Compute(const std::string&) { return responseDigest; } }; };
	struct WebIO
	{
		std::string path;
		WebIO(const std::string&, const std::string& url) : path(url.substr(std::string("https://backend.zw3.eu").size())) {}
		void SetTimeout(int) {}
		void SetProgressCallback(const std::function<void(std::size_t, std::size_t)>&) {}
		void CancelDownload() {}
		std::string Get(bool* success)
		{
			++requests; *success = httpSuccess;
			responseDigest = *Utils::PlaylistImageDigest(path);
			std::string bytes(57, '\0'); bytes.replace(0, 8, "\x89PNG\r\n\x1a\n", 8); bytes.replace(12, 4, "IHDR"); bytes[19] = 1; bytes[23] = 1;
			return bytes;
		}
	};
}
namespace Components
{
	static bool isActive = true, isInGame = false, isPlaylistSelectorOpen = true;
	static constexpr auto apiBase = "https://backend.zw3.eu";
	static struct { std::mutex mutex; int generation = 1; } playlistCatalog;
	static std::vector<std::function<void()>> worker, mainJobs;
	static void EnqueueAsync(std::function<void()> job) { worker.push_back(std::move(job)); }
	static std::string EncodeLowerHex(const std::string& bytes) { return bytes; }
	struct Scheduler
	{
		enum class Pipeline { MAIN };
		static void Once(std::function<void()> job, Pipeline) { mainJobs.push_back(std::move(job)); }
	};
	struct Materials
	{
		struct Material {};
		static inline std::unordered_map<std::string, Material*> cache;
		static Material* GetRuntimeMaterial(const std::string& name) { const auto entry = cache.find(name); return entry == cache.end() ? nullptr : entry->second; }
		static Material* CreateNewsMaterialFromImageBytes(const std::string& name, const std::string&) { return cache[name] = new Material; }
		static void Delete(Material* material, bool image)
		{
			assert(image); std::erase_if(cache, [material](const auto& entry) { return entry.second == material; }); delete material;
		}
	};
	struct ZWNet
	{
		static std::string ResolvePlaylistImage(const std::string&);
		static void PublishPlaylistCatalog() {}
	};
#include "PlaylistImageLoader.inc"
	static void Drain(std::vector<std::function<void()>>& jobs)
	{
		auto pending = std::move(jobs); jobs.clear(); for (const auto& job : pending) job();
	}
}

int main()
{
	using namespace Components;
	const auto path = [](char value) { return "/zwnet/playlist-images/" + std::string(64, value) + ".png"; };
	assert(ZWNet::ResolvePlaylistImage("https://external.test/art.png").empty()); assert(worker.empty());
	assert(ZWNet::ResolvePlaylistImage(path('a')).empty());
	assert(ZWNet::ResolvePlaylistImage(path('a')).empty()); assert(worker.size() == 1);
	Drain(worker); assert(mainJobs.size() == 1); assert(Materials::cache.empty());
	Drain(mainJobs); assert(!ZWNet::ResolvePlaylistImage(path('a')).empty()); assert(requests == 1);

	ZWNet::ResolvePlaylistImage(path('b')); Drain(worker); isPlaylistSelectorOpen = false; Drain(mainJobs);
	assert(!Materials::GetRuntimeMaterial("zwnet_playlist_" + std::string(64, 'b')));
	isPlaylistSelectorOpen = true; ZWNet::ResolvePlaylistImage(path('b')); assert(worker.size() == 1); Drain(worker); Drain(mainJobs);
	assert(!ZWNet::ResolvePlaylistImage(path('b')).empty());

	ZWNet::ResolvePlaylistImage(path('c')); Drain(worker); ++playlistCatalog.generation; Drain(mainJobs);
	assert(!Materials::GetRuntimeMaterial("zwnet_playlist_" + std::string(64, 'c')));
	ZWNet::ResolvePlaylistImage(path('c')); Drain(worker); Drain(mainJobs); assert(!ZWNet::ResolvePlaylistImage(path('c')).empty());

	httpSuccess = false; ZWNet::ResolvePlaylistImage(path('d')); Drain(worker); Drain(mainJobs);
	assert(ZWNet::ResolvePlaylistImage(path('d')).empty()); assert(worker.empty()); httpSuccess = true;
	for (char value = '0'; value <= '9'; ++value) { ZWNet::ResolvePlaylistImage(path(value)); Drain(worker); Drain(mainJobs); assert(Materials::cache.size() <= 8); }
	while (!Materials::cache.empty()) Materials::Delete(Materials::cache.begin()->second, true);
}
