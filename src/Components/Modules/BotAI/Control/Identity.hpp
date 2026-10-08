#pragma once

namespace Components::BotAI
{
	struct Identity
	{
		const char* name;
		int rank;
		int prestige;
		int cardIcon;
		int cardTitle;
		int cardNameplate;
	};

	void LoadBotNames();

	bool HasForcedBotNames();

	Identity IdentityFor(int ordinal);

	Identity RollIdentity(const char* const* takenNames, int takenCount);
}
