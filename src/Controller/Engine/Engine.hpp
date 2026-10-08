#pragma once

#include "Controller/Types.hpp"

namespace Controller::Engine
{
	inline constexpr int localClient = 0;

	float& ViewPitch(int client) noexcept;
	float& ViewYaw(int client) noexcept;

	float MaxPitchSpeed(int client) noexcept;
	float MaxYawSpeed(int client) noexcept;

	int PmFlags(int client) noexcept;
	int LastWeaponHand(int client) noexcept;
	int SnapPing(int client) noexcept;

	float ClientFrameTime() noexcept;

	bool IsSprintButtonUpRequired(int client) noexcept;

	struct LocationSelection
	{
		float frameTime;
		const float* mapWorldSize;
		float* location;
		float* angle;
		float* angleLocation;
	};

	LocationSelection SelectedLocation(int client) noexcept;

	void AimAssistBegin(const Game::AimInput& input, Game::AimOutput& output);
}
