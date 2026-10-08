#pragma once

namespace Components::BotAI
{
	struct Loadout;

	bool TryWriteLoadout(int clientNum, const Loadout& loadout);

	void ClearPlayerData(int clientNum);

	bool InstallUnlockHook();
	void UninstallUnlockHook();
}
