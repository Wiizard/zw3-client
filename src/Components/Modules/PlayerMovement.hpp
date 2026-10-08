#pragma once

namespace Components
{
	class PlayerMovement : public Component
	{
	public:
		PlayerMovement();

		static void ApplyRocketJump(Game::gentity_s* ent, const Game::weaponParms* wp);

	private:
		static constexpr auto SURF_LADDER = 0x8;

		static const Game::dvar_t* bg_rocketJump;
		static const Game::dvar_t* bg_rocketJumpScale;
		static const Game::dvar_t* bg_climbAnything;
		static const Game::dvar_t* bg_disableBarrierClips;

		enum BouncesSettings : int { DISABLED, ENABLED, DOUBLE };

		static Game::dvar_t* Dvar_RegisterSpectateSpeedScale(const char* dvarName, float value, float min, float max, unsigned int flags, const char* description);

		static const Game::dvar_t* bg_playerEjection;
		static const Game::dvar_t* bg_playerCollision;

		static void Jump_ClearState_Hk(Game::playerState_s* ps);

		static void PM_PlayerTraceStub(Game::pmove_s* pm, Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, int passEntityNum, int contentMask);

		static void PmoveSingle_Stub(Game::pmove_s* pm);
		static void PM_CheckLadderMove_Stub(Game::pmove_s* pm, Game::pml_t* pml);

		static void PM_LadderMove_RightVector_Hk(const float* source, const float* ladderNormal, float* pmlRight);

		static void GScr_IsSprinting(Game::scr_entref_t entref);

		static int StuckInClient_Hk(Game::gentity_s* self);
		static void CM_TransformedCapsuleTrace_Hk(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, const Game::Bounds* capsule, int contents, const float* origin, const float* angles);
	};
}
