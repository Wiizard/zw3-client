#include "Utils/PlaylistImage.hpp"
#include <cassert>
#include <iostream>

int main()
{
	const std::string digest(64, 'a');
	const auto path = "/zwnet/playlist-images/" + digest + ".png";
	assert(Utils::PlaylistImageDigest(path) == digest);
	for (const auto& invalid : { "https://example.com" + path, path + "?x=1", path + "/../x", "/x/" + digest + ".png",
		"/zwnet/playlist-images/" + std::string(64, 'A') + ".png", "/zwnet/playlist-images/" + std::string(63, 'a') + ".png" })
		assert(!Utils::PlaylistImageDigest(invalid));

	std::string bytes(57, '\0');
	bytes.replace(0, 8, "\x89PNG\r\n\x1a\n", 8);
	bytes.replace(12, 4, "IHDR");
	const auto size = [&bytes](const unsigned width, const unsigned height)
	{
		for (int i = 0; i < 4; ++i)
		{
			bytes[16 + i] = static_cast<char>((width >> ((3 - i) * 8)) & 255);
			bytes[20 + i] = static_cast<char>((height >> ((3 - i) * 8)) & 255);
		}
	};
	size(1280, 720); assert(Utils::IsBoundedPlaylistPng(bytes));
	size(2048, 512); assert(Utils::IsBoundedPlaylistPng(bytes));
	size(2048, 2048); assert(!Utils::IsBoundedPlaylistPng(bytes));
	size(2049, 1); assert(!Utils::IsBoundedPlaylistPng(bytes));
	size(0, 1); assert(!Utils::IsBoundedPlaylistPng(bytes));
	size(1, 1); assert(!Utils::IsBoundedPlaylistPng(bytes.substr(0, 56)));
	assert(!Utils::IsBoundedPlaylistPng(bytes + std::string(2 * 1024 * 1024, '\0')));
	bytes[0] = 'x'; assert(!Utils::IsBoundedPlaylistPng(bytes));
	std::cout << "Playlist image path and texture bounds passed\n";
}
