#pragma once

namespace Components
{
	class SlowMotion : public Component
	{
	public:
		SlowMotion();

	private:
		static void ScrCmd_SetSlowMotion_Stub(int index, const char* string);
	};
}
