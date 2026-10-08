#pragma once

namespace Game
{
	typedef bool(*PM_IsSprinting_t)(const playerState_s* ps);
	extern PM_IsSprinting_t PM_IsSprinting;

	void BindPlayerMovement();
}
