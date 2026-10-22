#pragma once

namespace Components
{
	class CollmapCollision : public Component
	{
	public:
		CollmapCollision();

		static bool HasLinkHook();

		static bool IsInjected();
		static void Inject();

		static unsigned int FindBox(float halfLength, float halfWidth, float height, bool* isTurned);
		static bool IsBox(unsigned int model);

		static unsigned int ClientCollmapModel(const Game::centity_s* cent);

		static bool ClipToEntity(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, int contents, const float* origin);

	private:
		static void SV_LinkEntity_Hk(Game::gentity_s* ent);
		static void AddScriptFunctions();
	};
}
