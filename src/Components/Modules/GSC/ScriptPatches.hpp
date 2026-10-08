#pragma once

namespace Components::GSC
{
	class ScriptPatches : public Component
	{
	public:
		ScriptPatches();

	private:
		static int* HECmd_GetHudElemText(Game::scr_entref_t entref);
		static void Scr_TableLookupIStringByRow_Hk();
	};
}
