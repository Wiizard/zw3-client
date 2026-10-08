#include "STDInclude.hpp"

namespace Game
{
	PM_IsSprinting_t PM_IsSprinting = nullptr;

	void BindPlayerMovement()
	{
		PM_IsSprinting = BindFunction<PM_IsSprinting_t>(0x140091340);
	}
}
