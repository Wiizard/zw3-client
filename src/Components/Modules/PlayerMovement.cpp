#include "STDInclude.hpp"

#include "PlayerMovement.hpp"
#include "Events.hpp"
#include "GSC/Script.hpp"
#include "Logger.hpp"

constexpr int MASK_PLAYER_CLIP = 0x10000;
constexpr int MASK_BARRIER_CLIP = 0x400;

extern "C"
{
	void StepSlideMoveStub();
	void ProjectVelocityStub();

	Game::dvar_t* PlayerMovement_bg_bounces = nullptr;
	Game::dvar_t* PlayerMovement_bg_bouncesAllAngles = nullptr;

	std::uintptr_t PlayerMovement_StepSlideMoveSlopeTest = 0;
	std::uintptr_t PlayerMovement_StepSlideMoveProject = 0;
	std::uintptr_t PlayerMovement_StepSlideMoveRestore = 0;
	std::uintptr_t PlayerMovement_ProjectVelocityScale = 0;
	std::uintptr_t PlayerMovement_ProjectVelocityDone = 0;

	void CrashLandScaleStub();
	void JumpCheckStub();

	Game::dvar_t* PlayerMovement_bg_disableLandingSlowdown = nullptr;
	Game::dvar_t* PlayerMovement_bg_bunnyHopAuto = nullptr;

	std::uintptr_t PlayerMovement_CrashLandScaleNext = 0;
	std::uintptr_t PlayerMovement_CrashLandScaleDone = 0;
	std::uintptr_t PlayerMovement_JumpCheckHeldBranch = 0;
	std::uintptr_t PlayerMovement_JumpCheckJump = 0;

	void RocketFireStub();

	std::uintptr_t PlayerMovement_G_FireRocket = 0;

	void PlayerMovement_ApplyRocketJump(Game::gentity_s* ent, const Game::weaponParms* wp)
	{
		Components::PlayerMovement::ApplyRocketJump(ent, wp);
	}

	void LadderClimbRateStub();

	Game::dvar_t* PlayerMovement_bg_ladderFixedInput = nullptr;

	void SprintRepressStub();

	Game::dvar_t* PlayerMovement_bg_sprintIgnoreRepress = nullptr;

	std::uintptr_t PlayerMovement_SprintRepressEnd = 0;
	std::uintptr_t PlayerMovement_SprintRepressDone = 0;

	void DivePerkTestStub();
	void BackDiveScaleStub();

	Game::dvar_t* PlayerMovement_bg_dive = nullptr;
	Game::dvar_t* PlayerMovement_bg_omnimovementDive = nullptr;

	std::uintptr_t PlayerMovement_BackDiveScale = 0;

	void KeyMoveSprintBitStub();
	void SprintIntentStub();
	void WalkMoveSprintStrafeStub();
	void MaxSpeedBackDiagonalStub();
	void MaxSpeedBackPureStub();
	void MovementDirClampStub();
	void StrafeConditionStub();

	Game::dvar_t* PlayerMovement_bg_omnimovement = nullptr;

	std::uintptr_t PlayerMovement_KeyMoveSprintBlock = 0;
	std::uintptr_t PlayerMovement_KeyMoveSprintDone = 0;
	std::uintptr_t PlayerMovement_WalkMoveSprintScale = 0;
	std::uintptr_t PlayerMovement_WalkMoveJumpCheck = 0;
	std::uintptr_t PlayerMovement_player_backSpeedScale = 0;
	std::uintptr_t PlayerMovement_MaxSpeedBackDiagonal = 0;
	std::uintptr_t PlayerMovement_MaxSpeedBackPure = 0;
	std::uintptr_t PlayerMovement_MaxSpeedDone = 0;
}

namespace Components
{
	constexpr std::uintptr_t Dvar_RegisterFloat_Engine = 0x140286050;

	constexpr std::uintptr_t BG_RegisterDvars_SpectateSpeedScaleCall = 0x14008C13A;

	struct ScaleLoad
	{
		std::uintptr_t instruction;
		std::array<std::uint8_t, 8> bytes;
	};

	constexpr ScaleLoad proneScaleLoad = { 0x14008F6C1, { 0xF3, 0x0F, 0x10, 0x05, 0xAF, 0x57, 0x2D, 0x00 } };
	constexpr ScaleLoad duckedScaleLoad = { 0x14008F6CA, { 0xF3, 0x0F, 0x10, 0x05, 0xDE, 0x57, 0x2D, 0x00 } };

	constexpr ScaleLoad noclipScaleLoad = { 0x1400918B5, { 0xF3, 0x0F, 0x59, 0x35, 0x7B, 0x31, 0x35, 0x00 } };
	constexpr ScaleLoad ufoScaleLoad = { 0x1400918C4, { 0xF3, 0x0F, 0x59, 0x35, 0x9C, 0x31, 0x35, 0x00 } };

	constexpr std::uintptr_t PM_StepSlideMove_JumpedStepTest = 0x140095F96;
	constexpr std::uintptr_t PM_StepSlideMove_SlopeTest = 0x140095F9B;
	constexpr std::uintptr_t PM_StepSlideMove_Project = 0x140095FDE;
	constexpr std::uintptr_t PM_StepSlideMove_Restore = 0x140095FA9;
	static const std::uint8_t jumpedStepTest[] = { 0x45, 0x85, 0xED, 0x75, 0x0E };

	constexpr std::uintptr_t PM_ProjectVelocity_UpwardTest = 0x140091C43;
	constexpr std::uintptr_t PM_ProjectVelocity_Scale = 0x140091C49;
	constexpr std::uintptr_t PM_ProjectVelocity_Done = 0x140091C64;
	static const std::uint8_t upwardTest[] = { 0x45, 0x0F, 0x2F, 0xD1, 0x76, 0x1B };

	constexpr std::uintptr_t PM_GroundTrace_JumpClearStateCall = 0x1400912A3;
	constexpr std::uintptr_t Jump_ClearState = 0x140087600;

	constexpr std::uintptr_t ClientEndFrame_StuckInClientCall = 0x140192EFA;
	constexpr std::uintptr_t StuckInClient = 0x140195A20;

	constexpr std::uintptr_t CM_TransformedCapsuleTraceCalls[] = { 0x140233D96, 0x1400C9D5E };
	constexpr std::uintptr_t CM_TransformedCapsuleTrace = 0x1401EFF60;

	constexpr std::uintptr_t PM_CrashLand_VelocityScale = 0x14008FF98;
	constexpr std::uintptr_t PM_CrashLand_VelocityScaleNext = 0x14008FF9D;
	constexpr std::uintptr_t PM_CrashLand_VelocityScaleDone = 0x14008FFCA;
	static const std::uint8_t velocityScaleLoad[] = { 0xF3, 0x0F, 0x10, 0x47, 0x28 };

	constexpr std::uintptr_t Jump_Check_HeldTest = 0x140087357;
	constexpr std::uintptr_t Jump_Check_HeldBranch = 0x14008735E;
	constexpr std::uintptr_t Jump_Check_Jump = 0x14008737E;
	static const std::uint8_t heldTest[] = { 0xF7, 0x47, 0x34, 0x00, 0x04, 0x00, 0x00 };

	constexpr std::uintptr_t FireWeapon_FireRocketCall = 0x140187CC9;
	constexpr std::uintptr_t FireWeapon_CalcMuzzlePointsParms = 0x14018789A;
	constexpr std::uintptr_t G_FireRocket = 0x140170FF0;
	static const std::uint8_t calcMuzzlePointsParms[] = { 0x48, 0x8D, 0x55, 0x80, 0x48, 0x8B, 0xCB };

	constexpr std::uintptr_t PM_CheckLadderMove_PlayerTraceCalls[] = { 0x14008F2D2, 0x14008F395 };
	constexpr std::uintptr_t PM_playerTrace = 0x140094020;

	constexpr std::uintptr_t Pmove_PmoveSingleCall = 0x140094335;
	constexpr std::uintptr_t PmoveSingle = 0x1400944B0;
	constexpr std::uintptr_t PmoveSingle_CheckLadderMoveCall = 0x140094F03;
	constexpr std::uintptr_t PM_CheckLadderMove = 0x14008F090;

	constexpr std::uintptr_t PM_LadderMove_ClimbRateClamp = 0x1400913F2;
	static const std::uint8_t climbRateClamp[] = { 0x41, 0x0F, 0x28, 0xF8, 0xF3, 0x0F, 0x5D, 0xF8 };

	constexpr std::uintptr_t PM_LadderMove_RightVectorCall = 0x14009141E;
	constexpr std::uintptr_t ProjectPointOnPlane = 0x14027C490;

	constexpr std::uintptr_t PM_UpdateSprint_RepressEndSetup = 0x14009280A;
	constexpr std::uintptr_t PM_UpdateSprint_RepressEndCall = 0x140092810;
	constexpr std::uintptr_t PM_UpdateSprint_Return = 0x140092A36;
	static const std::uint8_t repressEndSetup[] = { 0x4C, 0x8B, 0xC6, 0x48, 0x8B, 0xD7 };

	constexpr std::uintptr_t Jump_Start_DivePerkTest = 0x140087739;
	constexpr std::uintptr_t Jump_Start_BackDiveScale = 0x1400877A8;
	constexpr std::uintptr_t backDiveScale = 0x140364E7C;
	static const std::uint8_t divePerkTest[] = { 0xF7, 0x83, 0x28, 0x04, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 };
	static const std::uint8_t backDiveScaleLoad[] = { 0xF3, 0x0F, 0x59, 0x15, 0xCC, 0xD6, 0x2D, 0x00 };

	constexpr std::uintptr_t CL_KeyMove_BackActiveTest = 0x1400F6F64;
	constexpr std::uintptr_t CL_KeyMove_SprintBlock = 0x1400F6F6B;
	constexpr std::uintptr_t CL_KeyMove_SprintDone = 0x1400F6F91;
	static const std::uint8_t backActiveTest[] = { 0x41, 0x80, 0x7A, 0x4C, 0x00, 0x75, 0x26 };

	constexpr std::uintptr_t PM_UpdateSprint_ForwardGates[] = { 0x1400927BE, 0x1400928A3 };
	static const std::uint8_t forwardGate[] = { 0x0F, 0xBE, 0x4F, 0x22, 0x3B, 0x48, 0x10 };

	constexpr std::uintptr_t PM_WalkMove_SprintTest = 0x140093D1F;
	constexpr std::uintptr_t PM_WalkMove_SprintStrafeScale = 0x140093D25;
	constexpr std::uintptr_t PM_WalkMove_JumpCheck = 0x140093D43;
	static const std::uint8_t sprintTest[] = { 0x0F, 0xBA, 0xE0, 0x0E, 0x73, 0x1E };

	constexpr std::uintptr_t MaxSpeed_BackDiagonalTest = 0x140090629;
	constexpr std::uintptr_t MaxSpeed_BackDiagonalScale = 0x140090634;
	constexpr std::uintptr_t MaxSpeed_BackPureTest = 0x140090647;
	constexpr std::uintptr_t MaxSpeed_BackPureScale = 0x140090652;
	constexpr std::uintptr_t MaxSpeed_Done = 0x140090685;
	constexpr std::uintptr_t player_backSpeedScale = 0x140440C10;
	static const std::uint8_t backDiagonalTest[] = { 0x84, 0xC9, 0x79, 0x58, 0x48, 0x8B, 0x05, 0xDC, 0x05, 0x3B, 0x00 };
	static const std::uint8_t backPureTest[] = { 0x84, 0xC9, 0x79, 0x3A, 0x48, 0x8B, 0x05, 0xBE, 0x05, 0x3B, 0x00 };

	constexpr std::uintptr_t PM_SetMovementDir_Clamp = 0x140091E36;
	static const std::uint8_t movementDirClamp[] = { 0x83, 0xF8, 0x5A, 0x7E, 0x11, 0x85, 0xC9, 0xB8, 0xA6, 0xFF, 0xFF, 0xFF, 0xBA, 0x5A, 0x00, 0x00, 0x00, 0x0F, 0x4F, 0xC2, 0x8B, 0xC8 };

	constexpr std::uintptr_t StrafeCondition_Setup = 0x14009054D;
	static const std::uint8_t strafeConditionSetup[] = { 0x48, 0x8B, 0x06, 0xBA, 0x07, 0x00, 0x00, 0x00 };

	const Game::dvar_t* PlayerMovement::bg_playerEjection;
	const Game::dvar_t* PlayerMovement::bg_playerCollision;
	const Game::dvar_t* PlayerMovement::bg_rocketJump;
	const Game::dvar_t* PlayerMovement::bg_rocketJumpScale;
	const Game::dvar_t* PlayerMovement::bg_climbAnything;
	const Game::dvar_t* PlayerMovement::bg_disableBarrierClips;

	static Utils::Hook spectateSpeedScaleHook;
	static Utils::Hook rocketFireHook;
	static Utils::Hook ladderHooks[std::size(PM_CheckLadderMove_PlayerTraceCalls) + 2];
	static Utils::Hook ladderInputHooks[2];
	static Utils::Hook sprintRepressHook;
	static Utils::Hook diveHooks[2];
	static Utils::Hook omnimovementHooks[std::size(PM_UpdateSprint_ForwardGates) + 6];
	static Utils::Hook bounceHooks[3];
	static Utils::Hook gateHooks[std::size(CM_TransformedCapsuleTraceCalls) + 3];

	static std::int64_t GetLoadDistance(const ScaleLoad& load, const Game::dvar_t* dvar)
	{
		const auto loadEnd = Utils::Hook::Rebase(load.instruction) + load.bytes.size();
		return reinterpret_cast<std::int64_t>(&dvar->current.value) - static_cast<std::int64_t>(loadEnd);
	}

	static bool CanLoadReach(const ScaleLoad& load, const Game::dvar_t* dvar)
	{
		const auto distance = GetLoadDistance(load, dvar);
		return distance >= std::numeric_limits<std::int32_t>::min() && distance <= std::numeric_limits<std::int32_t>::max();
	}

	static void PointLoadAt(const ScaleLoad& load, const Game::dvar_t* dvar)
	{
		Utils::Hook::Set<std::int32_t>(load.instruction + load.bytes.size() - sizeof(std::int32_t), static_cast<std::int32_t>(GetLoadDistance(load, dvar)));
	}

	Game::dvar_t* PlayerMovement::Dvar_RegisterSpectateSpeedScale(const char* dvarName, float value, float min, float max, [[maybe_unused]] unsigned int flags, const char* description)
	{
		return Game::Dvar_RegisterFloat(dvarName, value, min, max, Game::DVAR_CODINFO, description);
	}

	void PlayerMovement::Jump_ClearState_Hk(Game::playerState_s* ps)
	{
		if (PlayerMovement_bg_bounces->current.integer == DOUBLE)
		{
			return;
		}

		reinterpret_cast<decltype(&Jump_ClearState_Hk)>(Utils::Hook::Rebase(Jump_ClearState))(ps);
	}

	int PlayerMovement::StuckInClient_Hk(Game::gentity_s* self)
	{
		if (bg_playerEjection->current.enabled)
		{
			return reinterpret_cast<decltype(&StuckInClient_Hk)>(Utils::Hook::Rebase(StuckInClient))(self);
		}

		return 0;
	}

	void PlayerMovement::CM_TransformedCapsuleTrace_Hk(Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, const Game::Bounds* capsule, int contents, const float* origin, const float* angles)
	{
		if (bg_playerCollision->current.enabled)
		{
			reinterpret_cast<decltype(&CM_TransformedCapsuleTrace_Hk)>(Utils::Hook::Rebase(CM_TransformedCapsuleTrace))(results, start, end, bounds, capsule, contents, origin, angles);
		}
	}

	void PlayerMovement::PM_PlayerTraceStub(Game::pmove_s* pm, Game::trace_t* results, const float* start, const float* end, const Game::Bounds* bounds, int passEntityNum, int contentMask)
	{
		reinterpret_cast<decltype(&PM_PlayerTraceStub)>(Utils::Hook::Rebase(PM_playerTrace))(pm, results, start, end, bounds, passEntityNum, contentMask);

		if (results && bg_climbAnything->current.enabled)
		{
			results[0].surfaceFlags |= SURF_LADDER;
		}
	}

	void PlayerMovement::PmoveSingle_Stub(Game::pmove_s* pm)
	{
		if (bg_disableBarrierClips->current.enabled)
		{
			if (pm != nullptr && (pm->ps->pm_flags & Game::PMF_LADDER) == 0)
			{
				pm->tracemask &= ~MASK_PLAYER_CLIP;
				pm->tracemask |= MASK_BARRIER_CLIP;
			}
		}

		reinterpret_cast<decltype(&PmoveSingle_Stub)>(Utils::Hook::Rebase(PmoveSingle))(pm);
	}

	void PlayerMovement::PM_CheckLadderMove_Stub(Game::pmove_s* pm, Game::pml_t* pml)
	{
		const auto shouldFixLadders = bg_disableBarrierClips->current.enabled && pm != nullptr;

		if (shouldFixLadders)
		{
			pm->tracemask |= MASK_PLAYER_CLIP;
		}

		reinterpret_cast<decltype(&PM_CheckLadderMove_Stub)>(Utils::Hook::Rebase(PM_CheckLadderMove))(pm, pml);

		if (shouldFixLadders && (pm->ps->pm_flags & Game::PMF_LADDER) == 0)
		{
			pm->tracemask &= ~MASK_PLAYER_CLIP;
		}
	}

	void PlayerMovement::PM_LadderMove_RightVector_Hk(const float* source, const float* ladderNormal, float* pmlRight)
	{
		if (PlayerMovement_bg_ladderFixedInput->current.enabled)
		{
			const float lx = ladderNormal[0];
			const float ly = ladderNormal[1];
			const float len2 = lx * lx + ly * ly;

			float invLen = 1.0f;
			if (len2 > 0.0f)
			{
				invLen = 1.0f / std::sqrtf(len2);
			}

			pmlRight[0] = -ly * invLen;
			pmlRight[1] = lx * invLen;
			pmlRight[2] = 0.0f;
			return;
		}

		reinterpret_cast<decltype(&PM_LadderMove_RightVector_Hk)>(Utils::Hook::Rebase(ProjectPointOnPlane))(source, ladderNormal, pmlRight);
	}

	void PlayerMovement::GScr_IsSprinting(const Game::scr_entref_t entref)
	{
		const auto* client = Game::GetEntity(entref)->client;
		if (!client)
		{
			GSC::Script::Scr_Error("IsSprinting can only be called on a player");
			return;
		}

		Game::Scr_AddBool(Game::PM_IsSprinting(&client->ps));
	}

	void PlayerMovement::ApplyRocketJump(Game::gentity_s* ent, const Game::weaponParms* wp)
	{
		if (ent->client && bg_rocketJump->current.enabled && wp->weapDef->inventoryType != Game::WEAPINVENTORY_EXCLUSIVE)
		{
			const auto scale = bg_rocketJumpScale->current.value;
			ent->client->ps.velocity[0] += (0.0f - wp->forward[0]) * scale;
			ent->client->ps.velocity[1] += (0.0f - wp->forward[1]) * scale;
			ent->client->ps.velocity[2] += (0.0f - wp->forward[2]) * scale;
		}
	}

	PlayerMovement::PlayerMovement()
	{
		bool isExpected = Utils::Hook::BranchesTo(BG_RegisterDvars_SpectateSpeedScaleCall, Dvar_RegisterFloat_Engine, false)
			&& Utils::Hook::MatchesBytes(PM_StepSlideMove_JumpedStepTest, jumpedStepTest, sizeof(jumpedStepTest))
			&& Utils::Hook::MatchesBytes(PM_ProjectVelocity_UpwardTest, upwardTest, sizeof(upwardTest))
			&& Utils::Hook::BranchesTo(PM_GroundTrace_JumpClearStateCall, Jump_ClearState, false)
			&& Utils::Hook::BranchesTo(ClientEndFrame_StuckInClientCall, StuckInClient, false)
			&& Utils::Hook::MatchesBytes(PM_CrashLand_VelocityScale, velocityScaleLoad, sizeof(velocityScaleLoad))
			&& Utils::Hook::MatchesBytes(Jump_Check_HeldTest, heldTest, sizeof(heldTest))
			&& Utils::Hook::BranchesTo(FireWeapon_FireRocketCall, G_FireRocket, false)
			&& Utils::Hook::MatchesBytes(FireWeapon_CalcMuzzlePointsParms, calcMuzzlePointsParms, sizeof(calcMuzzlePointsParms))
			&& Utils::Hook::BranchesTo(Pmove_PmoveSingleCall, PmoveSingle, false)
			&& Utils::Hook::BranchesTo(PmoveSingle_CheckLadderMoveCall, PM_CheckLadderMove, false)
			&& Utils::Hook::MatchesBytes(PM_LadderMove_ClimbRateClamp, climbRateClamp, sizeof(climbRateClamp))
			&& Utils::Hook::BranchesTo(PM_LadderMove_RightVectorCall, ProjectPointOnPlane, false)
			&& Utils::Hook::MatchesBytes(PM_UpdateSprint_RepressEndSetup, repressEndSetup, sizeof(repressEndSetup))
			&& Utils::Hook::MatchesBytes(Jump_Start_DivePerkTest, divePerkTest, sizeof(divePerkTest))
			&& Utils::Hook::MatchesBytes(Jump_Start_BackDiveScale, backDiveScaleLoad, sizeof(backDiveScaleLoad))
			&& Utils::Hook::MatchesBytes(CL_KeyMove_BackActiveTest, backActiveTest, sizeof(backActiveTest))
			&& Utils::Hook::MatchesBytes(PM_WalkMove_SprintTest, sprintTest, sizeof(sprintTest))
			&& Utils::Hook::MatchesBytes(MaxSpeed_BackDiagonalTest, backDiagonalTest, sizeof(backDiagonalTest))
			&& Utils::Hook::MatchesBytes(MaxSpeed_BackPureTest, backPureTest, sizeof(backPureTest))
			&& Utils::Hook::MatchesBytes(PM_SetMovementDir_Clamp, movementDirClamp, sizeof(movementDirClamp))
			&& Utils::Hook::MatchesBytes(StrafeCondition_Setup, strafeConditionSetup, sizeof(strafeConditionSetup));

		for (const auto gate : PM_UpdateSprint_ForwardGates)
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(gate, forwardGate, sizeof(forwardGate));
		}

		for (const auto call : PM_CheckLadderMove_PlayerTraceCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, PM_playerTrace, false);
		}

		for (const auto call : CM_TransformedCapsuleTraceCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, CM_TransformedCapsuleTrace, false);
		}

		for (const auto& load : { proneScaleLoad, duckedScaleLoad, noclipScaleLoad, ufoScaleLoad })
		{
			isExpected = isExpected && Utils::Hook::MatchesBytes(load.instruction, load.bytes.data(), load.bytes.size());
		}

		if (!isExpected)
		{
			Logger::Error("playermovement: pmove does not read as expected, no movement dvars\n");
			return;
		}

		if (!spectateSpeedScaleHook.Initialize(BG_RegisterDvars_SpectateSpeedScaleCall, reinterpret_cast<void*>(Dvar_RegisterSpectateSpeedScale), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("playermovement: could not seat the spectate speed hook, no movement dvars\n");
			return;
		}

		PlayerMovement_StepSlideMoveSlopeTest = Utils::Hook::Rebase(PM_StepSlideMove_SlopeTest);
		PlayerMovement_StepSlideMoveProject = Utils::Hook::Rebase(PM_StepSlideMove_Project);
		PlayerMovement_StepSlideMoveRestore = Utils::Hook::Rebase(PM_StepSlideMove_Restore);
		PlayerMovement_ProjectVelocityScale = Utils::Hook::Rebase(PM_ProjectVelocity_Scale);
		PlayerMovement_ProjectVelocityDone = Utils::Hook::Rebase(PM_ProjectVelocity_Done);
		PlayerMovement_CrashLandScaleNext = Utils::Hook::Rebase(PM_CrashLand_VelocityScaleNext);
		PlayerMovement_CrashLandScaleDone = Utils::Hook::Rebase(PM_CrashLand_VelocityScaleDone);
		PlayerMovement_JumpCheckHeldBranch = Utils::Hook::Rebase(Jump_Check_HeldBranch);
		PlayerMovement_JumpCheckJump = Utils::Hook::Rebase(Jump_Check_Jump);
		PlayerMovement_G_FireRocket = Utils::Hook::Rebase(G_FireRocket);
		PlayerMovement_SprintRepressEnd = Utils::Hook::Rebase(PM_UpdateSprint_RepressEndCall);
		PlayerMovement_SprintRepressDone = Utils::Hook::Rebase(PM_UpdateSprint_Return);
		PlayerMovement_BackDiveScale = Utils::Hook::Rebase(backDiveScale);
		PlayerMovement_KeyMoveSprintBlock = Utils::Hook::Rebase(CL_KeyMove_SprintBlock);
		PlayerMovement_KeyMoveSprintDone = Utils::Hook::Rebase(CL_KeyMove_SprintDone);
		PlayerMovement_WalkMoveSprintScale = Utils::Hook::Rebase(PM_WalkMove_SprintStrafeScale);
		PlayerMovement_WalkMoveJumpCheck = Utils::Hook::Rebase(PM_WalkMove_JumpCheck);
		PlayerMovement_player_backSpeedScale = Utils::Hook::Rebase(player_backSpeedScale);
		PlayerMovement_MaxSpeedBackDiagonal = Utils::Hook::Rebase(MaxSpeed_BackDiagonalScale);
		PlayerMovement_MaxSpeedBackPure = Utils::Hook::Rebase(MaxSpeed_BackPureScale);
		PlayerMovement_MaxSpeedDone = Utils::Hook::Rebase(MaxSpeed_Done);

		GSC::Script::AddMethod("IsSprinting", GScr_IsSprinting);

		Events::OnDvarInit([]
		{
			static const char* bg_bouncesValues[] =
			{
				"disabled",
				"enabled",
				"double",
				nullptr,
			};

			static const char* bg_bouncesAllAnglesValues[] =
			{
				"disabled",
				"simple",
				"all surfaces",
				nullptr,
			};

			PlayerMovement_bg_bounces = Game::Dvar_RegisterEnum("bg_bounces", bg_bouncesValues, DISABLED, Game::DVAR_CODINFO, "Bounce glitch settings");
			PlayerMovement_bg_bouncesAllAngles = Game::Dvar_RegisterEnum("bg_bouncesAllAngles", bg_bouncesAllAnglesValues, DISABLED, Game::DVAR_CODINFO, "Force bounce from all angles");

			bool isSeated = bounceHooks[0].Initialize(PM_StepSlideMove_JumpedStepTest, StepSlideMoveStub, HOOK_JUMP)->Install()->IsInstalled();
			isSeated = bounceHooks[1].Initialize(PM_ProjectVelocity_UpwardTest, ProjectVelocityStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;
			isSeated = bounceHooks[2].Initialize(PM_GroundTrace_JumpClearStateCall, reinterpret_cast<void*>(Jump_ClearState_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

			if (isSeated)
			{
				Utils::Hook::Nop(PM_ProjectVelocity_UpwardTest + 5, sizeof(upwardTest) - 5);
			}
			else
			{
				for (auto& hook : bounceHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("playermovement: could not seat every bounce hook, bg_bounces does nothing\n");
			}

			PlayerMovement_bg_disableLandingSlowdown = Game::Dvar_RegisterBool("bg_disableLandingSlowdown", false, Game::DVAR_CODINFO, "Toggle landing slowdown");
			PlayerMovement_bg_bunnyHopAuto = Game::Dvar_RegisterBool("bg_bunnyHopAuto", false, Game::DVAR_CODINFO, "Constantly jump when holding space");
			bg_playerEjection = Game::Dvar_RegisterBool("bg_playerEjection", false, Game::DVAR_CODINFO, "Push intersecting players away from each other");
			bg_playerCollision = Game::Dvar_RegisterBool("bg_playerCollision", false, Game::DVAR_CODINFO, "Push intersecting players away from each other");

			std::size_t gateCount = 0;
			isSeated = gateHooks[gateCount++].Initialize(ClientEndFrame_StuckInClientCall, reinterpret_cast<void*>(StuckInClient_Hk), HOOK_CALL)->Install()->IsInstalled();

			for (const auto call : CM_TransformedCapsuleTraceCalls)
			{
				isSeated = gateHooks[gateCount++].Initialize(call, reinterpret_cast<void*>(CM_TransformedCapsuleTrace_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
			}

			isSeated = gateHooks[gateCount++].Initialize(PM_CrashLand_VelocityScale, CrashLandScaleStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;
			isSeated = gateHooks[gateCount++].Initialize(Jump_Check_HeldTest, JumpCheckStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;

			assert(gateCount == std::size(gateHooks));

			if (isSeated)
			{
				Utils::Hook::Nop(Jump_Check_HeldTest + 5, sizeof(heldTest) - 5);
			}
			else
			{
				for (auto& hook : gateHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("playermovement: could not seat every collision, landing and jump hook, their dvars do nothing\n");
			}

			bg_rocketJump = Game::Dvar_RegisterBool("bg_rocketJump", false, Game::DVAR_CODINFO, "Enable CoD4 rocket jumps");
			bg_rocketJumpScale = Game::Dvar_RegisterFloat("bg_rocketJumpScale", 64.0f, 1.0f, std::numeric_limits<float>::max(), Game::DVAR_CODINFO, "The scale applied to the pushback force of a rocket");

			if (!rocketFireHook.Initialize(FireWeapon_FireRocketCall, RocketFireStub, HOOK_CALL)->Install()->IsInstalled())
			{
				Logger::Error("playermovement: could not seat the rocket hook, bg_rocketJump does nothing\n");
			}

			bg_climbAnything = Game::Dvar_RegisterBool("bg_climbAnything", false, Game::DVAR_CODINFO, "Treat any surface as a ladder");
			bg_disableBarrierClips = Game::Dvar_RegisterBool("bg_disableBarrierClips", false, Game::DVAR_CODINFO, "Disable player collision with out of bound barriers");

			std::size_t ladderCount = 0;
			isSeated = true;

			for (const auto call : PM_CheckLadderMove_PlayerTraceCalls)
			{
				isSeated = ladderHooks[ladderCount++].Initialize(call, reinterpret_cast<void*>(PM_PlayerTraceStub), HOOK_CALL)->Install()->IsInstalled() && isSeated;
			}

			isSeated = ladderHooks[ladderCount++].Initialize(Pmove_PmoveSingleCall, reinterpret_cast<void*>(PmoveSingle_Stub), HOOK_CALL)->Install()->IsInstalled() && isSeated;
			isSeated = ladderHooks[ladderCount++].Initialize(PmoveSingle_CheckLadderMoveCall, reinterpret_cast<void*>(PM_CheckLadderMove_Stub), HOOK_CALL)->Install()->IsInstalled() && isSeated;

			assert(ladderCount == std::size(ladderHooks));

			if (!isSeated)
			{
				for (auto& hook : ladderHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("playermovement: could not seat every ladder hook, bg_climbAnything and bg_disableBarrierClips do nothing\n");
			}

			PlayerMovement_bg_ladderFixedInput = Game::Dvar_RegisterBool("bg_ladderFixedInput", false, Game::DVAR_SYSTEMINFO, "Make ladder climb and strafe independent of view angle");

			isSeated = ladderInputHooks[0].Initialize(PM_LadderMove_ClimbRateClamp, LadderClimbRateStub, HOOK_CALL)->Install()->IsInstalled();
			isSeated = ladderInputHooks[1].Initialize(PM_LadderMove_RightVectorCall, reinterpret_cast<void*>(PM_LadderMove_RightVector_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

			if (isSeated)
			{
				Utils::Hook::Nop(PM_LadderMove_ClimbRateClamp + 5, sizeof(climbRateClamp) - 5);
			}
			else
			{
				for (auto& hook : ladderInputHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("playermovement: could not seat the ladder input hooks, bg_ladderFixedInput does nothing\n");
			}

			PlayerMovement_bg_sprintIgnoreRepress = Game::Dvar_RegisterBool("bg_sprintIgnoreRepress", false, Game::DVAR_SYSTEMINFO, "Ignore sprint-key re-presses while already sprinting (matches console behaviour)");

			if (sprintRepressHook.Initialize(PM_UpdateSprint_RepressEndSetup, SprintRepressStub, HOOK_JUMP)->Install()->IsInstalled())
			{
				Utils::Hook::Nop(PM_UpdateSprint_RepressEndSetup + 5, sizeof(repressEndSetup) - 5);
			}
			else
			{
				Logger::Error("playermovement: could not seat the sprint re-press hook, bg_sprintIgnoreRepress does nothing\n");
			}

			PlayerMovement_bg_dive = Game::Dvar_RegisterBool("bg_dive", false, Game::DVAR_CODINFO, "Toggle dive-to-prone (bypasses specialty_jumpdive perk requirement)");
			PlayerMovement_bg_omnimovementDive = Game::Dvar_RegisterBool("bg_omnimovementDive", false, Game::DVAR_CODINFO, "Disable the backward-dive attenuation (requires bg_dive or specialty_jumpdive perk)");

			isSeated = diveHooks[0].Initialize(Jump_Start_DivePerkTest, DivePerkTestStub, HOOK_CALL)->Install()->IsInstalled();
			isSeated = diveHooks[1].Initialize(Jump_Start_BackDiveScale, BackDiveScaleStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

			if (isSeated)
			{
				Utils::Hook::Nop(Jump_Start_DivePerkTest + 5, sizeof(divePerkTest) - 5);
				Utils::Hook::Nop(Jump_Start_BackDiveScale + 5, sizeof(backDiveScaleLoad) - 5);
			}
			else
			{
				for (auto& hook : diveHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("playermovement: could not seat the dive hooks, bg_dive and bg_omnimovementDive do nothing\n");
			}

			PlayerMovement_bg_omnimovement = Game::Dvar_RegisterBool("bg_omnimovement", true, Game::DVAR_CODINFO, "Toggle omnidirectional sprint (sprint in any direction)");

			std::size_t omnimovementCount = 0;

			isSeated = omnimovementHooks[omnimovementCount++].Initialize(CL_KeyMove_BackActiveTest, KeyMoveSprintBitStub, HOOK_JUMP)->Install()->IsInstalled();

			for (const auto gate : PM_UpdateSprint_ForwardGates)
			{
				isSeated = omnimovementHooks[omnimovementCount++].Initialize(gate, SprintIntentStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
			}

			isSeated = omnimovementHooks[omnimovementCount++].Initialize(PM_WalkMove_SprintTest, WalkMoveSprintStrafeStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;

			isSeated = omnimovementHooks[omnimovementCount++].Initialize(MaxSpeed_BackDiagonalTest, MaxSpeedBackDiagonalStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;
			isSeated = omnimovementHooks[omnimovementCount++].Initialize(MaxSpeed_BackPureTest, MaxSpeedBackPureStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;

			isSeated = omnimovementHooks[omnimovementCount++].Initialize(PM_SetMovementDir_Clamp, MovementDirClampStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

			isSeated = omnimovementHooks[omnimovementCount++].Initialize(StrafeCondition_Setup, StrafeConditionStub, HOOK_CALL)->Install()->IsInstalled() && isSeated;

			assert(omnimovementCount == std::size(omnimovementHooks));

			if (isSeated)
			{
				Utils::Hook::Nop(CL_KeyMove_BackActiveTest + 5, sizeof(backActiveTest) - 5);

				for (const auto gate : PM_UpdateSprint_ForwardGates)
				{
					Utils::Hook::Nop(gate + 5, sizeof(forwardGate) - 5);
				}

				Utils::Hook::Nop(PM_WalkMove_SprintTest + 5, sizeof(sprintTest) - 5);
				Utils::Hook::Nop(MaxSpeed_BackDiagonalTest + 5, sizeof(backDiagonalTest) - 5);
				Utils::Hook::Nop(MaxSpeed_BackPureTest + 5, sizeof(backPureTest) - 5);
				Utils::Hook::Nop(PM_SetMovementDir_Clamp + 5, sizeof(movementDirClamp) - 5);
				Utils::Hook::Nop(StrafeCondition_Setup + 5, sizeof(strafeConditionSetup) - 5);
			}
			else
			{
				for (auto& hook : omnimovementHooks)
				{
					hook.Uninstall();
				}

				Logger::Error("playermovement: could not seat every omnimovement hook, bg_omnimovement does nothing\n");
			}

			const auto* player_duckedSpeedScale = Game::Dvar_RegisterFloat("player_duckedSpeedScale", 0.65f, 0.0f, 5.0f, Game::DVAR_CHEAT, "The scale applied to the player speed when ducking");
			const auto* player_proneSpeedScale = Game::Dvar_RegisterFloat("player_proneSpeedScale", 0.15f, 0.0f, 5.0f, Game::DVAR_CHEAT, "The scale applied to the player speed when crawling");

			const auto* cg_ufo_scaler = Game::Dvar_RegisterFloat("cg_ufo_scaler", 6.0f, 0.001f, 1000.0f, Game::DVAR_CHEAT, "The speed at which ufo camera moves");
			const auto* cg_noclip_scaler = Game::Dvar_RegisterFloat("cg_noclip_scaler", 3.0f, 0.001f, 1000.0f, Game::DVAR_CHEAT, "The speed at which noclip camera moves");

			const std::pair<ScaleLoad, const Game::dvar_t*> scales[] =
			{
				{ proneScaleLoad, player_proneSpeedScale },
				{ duckedScaleLoad, player_duckedSpeedScale },
				{ noclipScaleLoad, cg_noclip_scaler },
				{ ufoScaleLoad, cg_ufo_scaler },
			};

			for (const auto& [load, dvar] : scales)
			{
				if (!CanLoadReach(load, dvar))
				{
					Logger::Error("playermovement: a speed scale dvar is out of pmove's reach, the scales stay stock\n");
					return;
				}
			}

			for (const auto& [load, dvar] : scales)
			{
				PointLoadAt(load, dvar);
			}
		});
	}
}
