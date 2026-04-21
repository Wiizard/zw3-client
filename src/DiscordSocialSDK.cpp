#include "STDInclude.hpp"
#include "DiscordSocialSDK.hpp"

#if defined(HAS_DISCORD_SOCIAL_SDK) && HAS_DISCORD_SOCIAL_SDK
#define DISCORDPP_IMPLEMENTATION
#include <discordpp.h>

namespace DiscordSocialSDK
{
	static std::unique_ptr<discordpp::Client> Client;
	static void(*JoinCallback)(const char* joinSecret) = nullptr;

	bool Initialize(const uint64_t applicationId, void(*joinCallback)(const char* joinSecret))
	{
		JoinCallback = joinCallback;
		Client = std::make_unique<discordpp::Client>();
		Client->SetApplicationId(applicationId);
		Client->SetActivityJoinCallback([](const std::string& joinSecret)
		{
			if (JoinCallback)
			{
				JoinCallback(joinSecret.data());
			}
		});

		return true;
	}

	void Shutdown()
	{
		Client.reset();
	}

	void Pump()
	{
		// No explicit callback pump API is required for discordpp::Client.
	}

	void UpdatePresence(const PresenceData& presence)
	{
		if (!Client)
		{
			return;
		}

		discordpp::Activity activity;
		activity.SetType(discordpp::ActivityTypes::Playing);

		if (!presence.Details.empty()) activity.SetDetails(presence.Details);
		if (!presence.State.empty()) activity.SetState(presence.State);

		if (presence.StartTimestamp > 0)
		{
			discordpp::ActivityTimestamps timestamps{};
			timestamps.SetStart(static_cast<uint64_t>(presence.StartTimestamp));
			activity.SetTimestamps(timestamps);
		}

		if (!presence.PartyId.empty() && presence.PartyMax > 0)
		{
			discordpp::ActivityParty party{};
			party.SetId(presence.PartyId);
			party.SetCurrentSize(static_cast<int32_t>(std::max(0, presence.PartySize)));
			party.SetMaxSize(static_cast<int32_t>(std::max(0, presence.PartyMax)));
			activity.SetParty(party);
		}

		if (!presence.JoinSecret.empty())
		{
			discordpp::ActivitySecrets secrets{};
			secrets.SetJoin(presence.JoinSecret);
			activity.SetSecrets(secrets);
		}

		Client->UpdateRichPresence(std::move(activity), [](discordpp::ClientResult result)
		{
			if (!result.Successful())
			{
				Logger::Print(Game::CON_CHANNEL_WARN, "Discord Social SDK: Failed to update presence.\n");
			}
		});
	}
}
#else
namespace DiscordSocialSDK
{
	bool Initialize([[maybe_unused]] const uint64_t applicationId, [[maybe_unused]] void(*joinCallback)(const char* joinSecret))
	{
		return false;
	}

	void Shutdown() {}
	void Pump() {}
	void UpdatePresence([[maybe_unused]] const PresenceData& presence) {}
}
#endif
