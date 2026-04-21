#pragma once

#include <cstdint>
#include <string>

namespace DiscordSocialSDK
{
	struct PresenceData
	{
		std::string Details;
		std::string State;
		std::string PartyId;
		std::string JoinSecret;
		int64_t StartTimestamp = 0;
		int PartySize = 0;
		int PartyMax = 0;
	};

	bool Initialize(uint64_t applicationId, void(*joinCallback)(const char* joinSecret));
	void Shutdown();
	void Pump();
	void UpdatePresence(const PresenceData& presence);
}
