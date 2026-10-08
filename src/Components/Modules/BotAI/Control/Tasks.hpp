#pragma once

#include "Components/Modules/BotAI/Control/State.hpp"

namespace Components::BotAI
{
	const char* TaskName(int kind);

	void PlanLife(int clientNum, const PlayerView& self);

	void TickTasks(int clientNum, const PlayerView& self);

	bool RequestTask(int clientNum, const Task& task);
	bool RequestPush(int clientNum, const float* point, int priority, const char* source);
	bool RequestCoverAt(int clientNum, const float* point, float lookYaw);

	bool RequestHunt(int clientNum, int client, int untilTime, int priority, const char* source);

	void OnGoalReached(int clientNum);
	void OnGoalFailed(int clientNum, const char* reason);
	void OnKillConfirmed(int clientNum);

	bool IsHolding(int clientNum);
	bool HoldView(int clientNum, const PlayerView& self, BotInput& input, bool allowTurn);

	int TaskHuntClient(int clientNum);
	int HuntersOf(int clientNum, int enemy);

	bool IsHoldSpotTaken(int clientNum, const float* origin);

	const char* TaskLabel(int clientNum);
}
