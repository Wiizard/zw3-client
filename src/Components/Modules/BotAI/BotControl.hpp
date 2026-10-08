#pragma once

#include <cstddef>

namespace Components::BotAI
{
	bool InstallControl();

	bool ThinkBot(Game::client_s* clientState);

	bool IsFillBot(int clientNum);

	const char* PendingBotName();

	bool HasScriptRoomForBot();

	int DesiredBotCount();

	int DesiredFillCount();

	int PlannedBotCount(int peopleCount, int slotCount);

	int ReadDvar(void* dvar);
	unsigned int NextRand();
	void BotLog(const char* format, ...);
	const char* TuningHelp(const char* name);

	bool BotsPath(const char* file, char* out, std::size_t outSize);

	int NearestBotPath(const float* origin, const short** outPath, int* outLength, int* outIndex);
}
