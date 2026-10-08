#pragma once

#include "Network.hpp"

namespace Components
{
	class Playlist : public Component
	{
	public:
		Playlist();

		static void LoadPlaylist();

	private:
		static std::unordered_map<const void*, std::string> mapRelocation;
		static std::string currentPlaylistBuffer;
		static std::string receivedPlaylistBuffer;

		static void PlaylistRequest(Network::Address& address, const std::string& data);
		static void PlaylistResponse(Network::Address& address, const std::string& data);
		static void PlaylistInvalidPassword(Network::Address& address, const std::string& data);

		static Utils::Hook hooks[5];

		static void Live_Init_Hook();
		static char* Com_ParseOnLine_Hook(const char** data);

		static void MapNameCopy(char* dest, const char* src, int destsize);
		static void SetMapName(const char* dvarName, const char* value);
		static int GetMapIndex(const char* mapname);
	};
}
