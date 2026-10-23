#pragma once

namespace Components
{
	class ZombieCollision : public Component
	{
	public:
		ZombieCollision();

		static void PostLink(Game::gentity_s* ent);

		static bool ClipToEntity(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, const Game::Bounds* box, int contents, const float* origin, const float* angles);

		static void BeginPmove(Game::pmove_s* pm);
		static void EndPmove();

	private:
		static void SV_BrushModelTrace_Hk(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, unsigned int model, int contents, const float* origin, const float* angles);
		static void AddScriptMethods();
	};
}
