#include "Utils/SocialPresentation.hpp"
#include <cassert>

int main()
{
	using Utils::SocialActivityText;
	assert(SocialActivityText("lobby") == "IN LOBBY");
	assert(SocialActivityText("ingame") == "IN GAME");
	assert(SocialActivityText("map_vote") == "VOTING");
	assert(SocialActivityText("ready_check") == "IN LOBBY");
	assert(SocialActivityText("starting_server") == "STARTING");
	assert(SocialActivityText("join_preview") == "JOINING");
	assert(SocialActivityText("searching") == "SEARCHING");
	assert(SocialActivityText("private_match") == "PRIVATE MATCH");
	assert(SocialActivityText("") == "OFFLINE");
	assert(!Utils::SocialActivityOnline(SocialActivityText("offline")));
	assert(!Utils::SocialActivityOnline("UNAVAILABLE"));
	assert(Utils::SocialActivityOnline(SocialActivityText("lobby")));
	assert(Utils::ValidSocialRank(1, 0));
	assert(Utils::ValidSocialRank(54, 20));
	assert(!Utils::ValidSocialRank(0, 0));
	assert(!Utils::ValidSocialRank(55, 1));
	assert(!Utils::ValidSocialRank(20, -1));
	assert(!Utils::ValidSocialRank(20, 21));
	assert((Utils::ParseSocialRank("33", "2") == std::pair{33, 2}));
	assert(!Utils::ParseSocialRank("", "0"));
	assert(!Utils::ParseSocialRank("Unavailable", "0"));
	assert(!Utils::ParseSocialRank("0", "0"));
	assert(!Utils::ParseSocialRank("12garbage", "0"));
	assert(!Utils::ParseSocialRank("12", ""));
	assert(!Utils::ParseSocialRank("12", "21"));
	assert(!Utils::ParseSocialRank("99999999999999999999", "0"));
}
