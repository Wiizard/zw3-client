#include "STDInclude.hpp"

#include "HudParallax.hpp"
#include "Events.hpp"
#include "Logger.hpp"

namespace Components
{
	Dvar::Var HudParallax::cg_hudParallax;
	Dvar::Var HudParallax::cg_hudParallaxMenus;
	Dvar::Var HudParallax::cg_hudParallaxHudElems;
	Dvar::Var HudParallax::cg_hudParallaxUnanchored;
	Dvar::Var HudParallax::cg_hudParallaxViewRate;
	Dvar::Var HudParallax::cg_hudParallaxAnimRate;
	Dvar::Var HudParallax::cg_hudParallaxLinearForward;
	Dvar::Var HudParallax::cg_hudParallaxLinearSide;
	Dvar::Var HudParallax::cg_hudParallaxLinearUp;
	Dvar::Var HudParallax::cg_hudParallaxLinearMax;
	Dvar::Var HudParallax::cg_hudParallaxLinearDecay;
	Dvar::Var HudParallax::cg_hudParallaxSmoothing;
	Dvar::Var HudParallax::cg_hudParallaxMaxPitch;
	Dvar::Var HudParallax::cg_hudParallaxMaxYaw;
	Dvar::Var HudParallax::cg_hudParallaxScale;
	Dvar::Var HudParallax::cg_hudParallaxSnapDistance;
	Dvar::Var HudParallax::cg_hudParallaxSnapAngle;
	Dvar::Var HudParallax::cg_hudParallaxBobPitch;
	Dvar::Var HudParallax::cg_hudParallaxBobYaw;

	constexpr std::uintptr_t R_RenderScene_CG_Draw2DCall = 0x140022896;
	constexpr std::uintptr_t CG_Draw2D = 0x1400D0D80;

	constexpr std::uintptr_t CG_Draw2D_Menu_PaintViewportCalls[] = { 0x1400D0ECC, 0x1400D0F91 };
	constexpr std::uintptr_t Menu_PaintViewport = 0x1402664E0;

	constexpr std::uintptr_t ScrPlace_ApplyRect = 0x1400F23B0;
	constexpr std::uintptr_t ScrPlace_ApplyRectWithoutSplitScreenScaling = 0x1400F24E0;
	constexpr std::uintptr_t ScrPlace_ApplyRectClamped_ScrPlace_ApplyRectWithoutSplitScreenScalingCall = 0x1400F2403;
	constexpr std::uintptr_t Rect_ContainsPoint_ScrPlace_ApplyRectCall = 0x140268219;
	constexpr std::uintptr_t Scroll_Slider_SetThumbPos_ScrPlace_ApplyRectCall = 0x1402692FA;

	constexpr std::uintptr_t ScrPlace_ApplyX = 0x1400F27B0;
	constexpr std::uintptr_t ScrPlace_ApplyY = 0x1400F28A0;
	constexpr std::uintptr_t GetHudElemOrg_ScrPlace_ApplyXCall = 0x1400B2318;
	constexpr std::uintptr_t GetHudElemOrg_ScrPlace_ApplyYCall = 0x1400B2356;

	constexpr std::uintptr_t floorfEntry = 0x140335DE0;
	constexpr std::uintptr_t textX_floorfCalls[] = { 0x14026D150, 0x14026D373, 0x14026D4BE };
	constexpr std::uintptr_t textY_floorfCalls[] = { 0x14026D172, 0x14026D395, 0x14026D4E0 };

	constexpr std::uintptr_t hudElemPlace = 0x1400B1A00;
	constexpr std::uintptr_t CG_Draw2dHudElems_hudElemPlaceCall = 0x1400B16A9;
	constexpr std::uintptr_t hudElemPlace_floorfCallX = 0x1400B2110;
	constexpr std::uintptr_t hudElemPlace_floorfCallY = 0x1400B212D;

	constexpr std::uintptr_t AnglesToAxis = 0x14027E1F0;
	constexpr std::uintptr_t AxisToAngles = 0x1402793F0;

	constexpr std::uintptr_t BG_CalculateViewMovement_VerticalBobFactor = 0x14009ED50;
	constexpr std::uintptr_t BG_CalculateViewMovement_HorizontalBobFactor = 0x14009EC60;
	constexpr std::uintptr_t bobSpeed = 0x1404ED124;

	constexpr int firstUnanchoredAlign = 4;
	constexpr int lastUnanchoredAlign = 6;

	constexpr float degreesPerRadian = 57.2957795f;
	constexpr float rateTimeBase = 1.0f / 60.0f;
	constexpr float rootUnitsHigh = 720.0f;
	constexpr float pi = 3.14159265f;
	constexpr float bobCycleSteps = 255.0f;

	using CG_Draw2D_t = void(*)(int localClientNum);
	using Menu_PaintViewport_t = void(*)(void* dc);
	using ScrPlace_ApplyRect_t = void(*)(const float* placement, float* x, float* y, float* w, float* h, int horzAlign, int vertAlign);
	using ScrPlace_ApplyAxis_t = float(*)(const float* placement, float position, int align);
	using HudElemPlace_t = void(*)(int localClientNum, const void* hudElem, float* origin, void* layout);
	using AnglesToAxis_t = void(*)(const float* angles, float* axis);
	using AxisToAngles_t = void(*)(const float* axis, float* angles);
	using BobFactor_t = float(*)(const Game::playerState_s* ps, float phase, float speed);

	struct Motion
	{
		bool isPrimed;
		bool hasVelocity;
		int time;
		int pmType;
		std::chrono::steady_clock::time_point clock;
		float aimAngles[3];
		float cameraAngles[3];
		float cameraOrigin[3];
		float localVelocity[3];
		float velocityKick[3];
		float sway[3];
	};

	static Motion motion{};

	static float offsetX = 0.0f;
	static float offsetY = 0.0f;
	static bool isUnanchoredMoved = false;
	static bool isHudElemMoved = false;

	static thread_local bool isPaintingMenus = false;
	static thread_local float rectAppliedX = 0.0f;
	static thread_local float rectAppliedY = 0.0f;
	static thread_local float hudElemAppliedX = 0.0f;
	static thread_local float hudElemAppliedY = 0.0f;
	static thread_local float hudElemHeldX = 0.0f;
	static thread_local float hudElemHeldY = 0.0f;

	static Utils::Hook draw2DHook;
	static Utils::Hook paintViewportHooks[std::size(CG_Draw2D_Menu_PaintViewportCalls)];
	static Utils::Hook applyRectHook;
	static Utils::Hook applyRectClampedHook;
	static Utils::Hook containsPointHook;
	static Utils::Hook sliderThumbHook;
	static Utils::Hook applyXHook;
	static Utils::Hook applyYHook;
	static Utils::Hook textFloorXHooks[std::size(textX_floorfCalls)];
	static Utils::Hook textFloorYHooks[std::size(textY_floorfCalls)];
	static Utils::Hook hudElemPlaceHook;
	static Utils::Hook hudElemFloorXHook;
	static Utils::Hook hudElemFloorYHook;

	static float AngleNormalize180(float angle)
	{
		const float turns = angle * (1.0f / 360.0f);
		return (turns - std::floor(turns + 0.5f)) * 360.0f;
	}

	static float AngleDelta(float angle, float base)
	{
		return AngleNormalize180(angle - base);
	}

	static float ClampAngle(float angle, float center, float maxOffset)
	{
		const float offset = AngleDelta(angle, center);

		if (offset > maxOffset)
		{
			return center + maxOffset;
		}

		if (offset < -maxOffset)
		{
			return center - maxOffset;
		}

		return angle;
	}

	static bool IsAnchored(int align)
	{
		return align < firstUnanchoredAlign || align > lastUnanchoredAlign;
	}

	static void SeedSamples(const Game::cg_s* cg, const float* cameraAngles, std::chrono::steady_clock::time_point now)
	{
		motion.time = cg->time;
		motion.pmType = cg->predictedPlayerState.pm_type;
		motion.clock = now;

		std::memcpy(motion.aimAngles, cg->predictedPlayerState.viewangles, sizeof(motion.aimAngles));
		std::memcpy(motion.cameraAngles, cameraAngles, sizeof(motion.cameraAngles));
		std::memcpy(motion.cameraOrigin, cg->refdef.view.org, sizeof(motion.cameraOrigin));
	}

	void HudParallax::StepMotion(int localClientNum)
	{
		const auto* const cg = Game::CL_GetLocalClientGlobals(localClientNum);

		if (!cg_hudParallax.Get<bool>())
		{
			motion.isPrimed = false;
			offsetX = 0.0f;
			offsetY = 0.0f;
			return;
		}

		if (motion.isPrimed && cg->time == motion.time)
		{
			return;
		}

		isUnanchoredMoved = cg_hudParallaxUnanchored.Get<bool>();
		isHudElemMoved = cg_hudParallaxHudElems.Get<bool>();

		float cameraAngles[3];
		reinterpret_cast<AxisToAngles_t>(Utils::Hook::Rebase(AxisToAngles))(&cg->refdef.view.axis[0][0], cameraAngles);

		const auto now = std::chrono::steady_clock::now();

		if (!motion.isPrimed || cg->time < motion.time)
		{
			motion = {};
			motion.isPrimed = true;
			SeedSamples(cg, cameraAngles, now);
			offsetX = 0.0f;
			offsetY = 0.0f;
			return;
		}

		const auto& ps = cg->predictedPlayerState;

		if (ps.pm_type != motion.pmType)
		{
			motion.hasVelocity = false;
			SeedSamples(cg, cameraAngles, now);
			return;
		}

		const float gameSeconds = static_cast<float>(cg->time - motion.time) * 0.001f;
		float realSeconds = std::chrono::duration<float>(now - motion.clock).count();

		if (realSeconds <= 0.0f)
		{
			realSeconds = gameSeconds;
		}

		float aimDelta[3];
		float animDelta[3];
		float originDelta[3];
		float animTurn = 0.0f;

		for (int i = 0; i < 3; ++i)
		{
			aimDelta[i] = AngleDelta(ps.viewangles[i], motion.aimAngles[i]);
			animDelta[i] = AngleDelta(AngleDelta(cameraAngles[i], motion.cameraAngles[i]), aimDelta[i]);
			originDelta[i] = cg->refdef.view.org[i] - motion.cameraOrigin[i];
			animTurn = std::max(animTurn, std::fabs(animDelta[i]));
		}

		const float originDistance = std::sqrt(originDelta[0] * originDelta[0] + originDelta[1] * originDelta[1] + originDelta[2] * originDelta[2]);
		const bool isCameraCut = originDistance > cg_hudParallaxSnapDistance.Get<float>() || animTurn > cg_hudParallaxSnapAngle.Get<float>();

		if (isCameraCut)
		{
			motion.hasVelocity = false;
			SeedSamples(cg, cameraAngles, now);
			return;
		}

		const float decay = std::exp(-cg_hudParallaxSmoothing.Get<float>() * realSeconds);
		const float inputGain = (1.0f - decay) / realSeconds * rateTimeBase;
		const float viewRate = cg_hudParallaxViewRate.Get<float>();
		const float animRate = cg_hudParallaxAnimRate.Get<float>();

		float impulse[3];
		impulse[0] = (aimDelta[0] * viewRate + animDelta[0] * animRate) * inputGain;
		impulse[1] = (aimDelta[1] * viewRate + animDelta[1] * animRate) * inputGain;
		impulse[2] = animDelta[2] * animRate * inputGain;

		const float yawRadians = ps.viewangles[1] / degreesPerRadian;
		const float forward[2] = { std::cos(yawRadians), std::sin(yawRadians) };
		const float localVelocity[3] =
		{
			ps.velocity[0] * forward[0] + ps.velocity[1] * forward[1],
			ps.velocity[1] * forward[0] - ps.velocity[0] * forward[1],
			ps.velocity[2],
		};

		if (motion.hasVelocity)
		{
			for (int i = 0; i < 3; ++i)
			{
				motion.velocityKick[i] += localVelocity[i] - motion.localVelocity[i];
			}
		}

		motion.hasVelocity = true;
		std::memcpy(motion.localVelocity, localVelocity, sizeof(motion.localVelocity));

		const float linearMax = cg_hudParallaxLinearMax.Get<float>();
		const float kickLength = std::sqrt(motion.velocityKick[0] * motion.velocityKick[0] + motion.velocityKick[1] * motion.velocityKick[1] + motion.velocityKick[2] * motion.velocityKick[2]);

		float kickDirection[3] = { 0.0f, 0.0f, 0.0f };

		if (kickLength > 0.0f)
		{
			for (int i = 0; i < 3; ++i)
			{
				kickDirection[i] = motion.velocityKick[i] / kickLength;
			}
		}

		const float clampedLength = std::min(kickLength, linearMax);
		const float kickFraction = clampedLength / linearMax;

		float linearTarget[3] = { 0.0f, 0.0f, 0.0f };
		linearTarget[0] += kickDirection[0] * kickFraction * cg_hudParallaxLinearForward.Get<float>();
		linearTarget[0] += kickDirection[2] * kickFraction * cg_hudParallaxLinearUp.Get<float>();
		linearTarget[1] -= kickDirection[1] * kickFraction * cg_hudParallaxLinearSide.Get<float>();

		const float speed = Utils::Hook::Get<float>(bobSpeed);
		const float bobPhase = static_cast<float>(static_cast<std::uint8_t>(ps.bobCycle)) / bobCycleSteps * pi * 2.0f + 2.0f * pi;
		const float verticalBob = reinterpret_cast<BobFactor_t>(Utils::Hook::Rebase(BG_CalculateViewMovement_VerticalBobFactor))(&ps, bobPhase, speed);
		const float horizontalBob = reinterpret_cast<BobFactor_t>(Utils::Hook::Rebase(BG_CalculateViewMovement_HorizontalBobFactor))(&ps, bobPhase, speed);

		linearTarget[0] += verticalBob * cg_hudParallaxBobPitch.Get<float>();
		linearTarget[1] += horizontalBob * cg_hudParallaxBobYaw.Get<float>();

		const float decayedLength = std::max(clampedLength - linearMax / cg_hudParallaxLinearDecay.Get<float>() * realSeconds, 0.0f);

		for (int i = 0; i < 3; ++i)
		{
			motion.velocityKick[i] = kickDirection[i] * decayedLength;
		}

		const float maxOffset[3] = { cg_hudParallaxMaxPitch.Get<float>(), cg_hudParallaxMaxYaw.Get<float>(), cg_hudParallaxMaxYaw.Get<float>() };

		float swayedAngles[3];

		for (int i = 0; i < 3; ++i)
		{
			motion.sway[i] = motion.sway[i] * decay + impulse[i] + linearTarget[i] * (1.0f - decay);
			swayedAngles[i] = ClampAngle(cameraAngles[i] + motion.sway[i], cameraAngles[i], maxOffset[i]);
		}

		float swayedAxis[3][3];
		float cameraAxis[3][3];
		reinterpret_cast<AnglesToAxis_t>(Utils::Hook::Rebase(AnglesToAxis))(swayedAngles, &swayedAxis[0][0]);
		reinterpret_cast<AnglesToAxis_t>(Utils::Hook::Rebase(AnglesToAxis))(cameraAngles, &cameraAxis[0][0]);

		float relativeForward[3];

		for (int i = 0; i < 3; ++i)
		{
			relativeForward[i] = swayedAxis[0][0] * cameraAxis[i][0] + swayedAxis[0][1] * cameraAxis[i][1] + swayedAxis[0][2] * cameraAxis[i][2];
		}

		const float relativeYaw = AngleNormalize180(std::atan2(relativeForward[1], relativeForward[0]) * degreesPerRadian);
		const float relativeHorizontal = std::sqrt(relativeForward[0] * relativeForward[0] + relativeForward[1] * relativeForward[1]);
		const float relativePitch = AngleNormalize180(-std::atan2(relativeForward[2], relativeHorizontal) * degreesPerRadian);

		const float fovY = 2.0f * std::atan(cg->refdef.view.tanHalfFovY) * degreesPerRadian;

		offsetX = 0.0f;
		offsetY = 0.0f;

		if (fovY > 0.0f)
		{
			const float height = static_cast<float>(cg->refdef.height);
			const float pixelsPerDegree = height / fovY * (height / rootUnitsHigh);
			const float scale = cg_hudParallaxScale.Get<float>();

			offsetX = -relativeYaw * scale * pixelsPerDegree;
			offsetY = relativePitch * scale * pixelsPerDegree;
		}

		SeedSamples(cg, cameraAngles, now);
	}

	void HudParallax::CG_Draw2D_Hk(int localClientNum)
	{
		StepMotion(localClientNum);

		reinterpret_cast<CG_Draw2D_t>(Utils::Hook::Rebase(CG_Draw2D))(localClientNum);
	}

	void HudParallax::Menu_PaintViewport_Hk(void* dc)
	{
		isPaintingMenus = cg_hudParallax.Get<bool>() && cg_hudParallaxMenus.Get<bool>();

		reinterpret_cast<Menu_PaintViewport_t>(Utils::Hook::Rebase(Menu_PaintViewport))(dc);

		isPaintingMenus = false;
	}

	void HudParallax::ScrPlace_ApplyRect_Hk(const float* placement, float* x, float* y, float* w, float* h, int horzAlign, int vertAlign)
	{
		reinterpret_cast<ScrPlace_ApplyRect_t>(Utils::Hook::Rebase(ScrPlace_ApplyRectWithoutSplitScreenScaling))(placement, x, y, w, h, horzAlign, vertAlign);

		rectAppliedX = 0.0f;
		rectAppliedY = 0.0f;

		if (!isPaintingMenus)
		{
			return;
		}

		if (isUnanchoredMoved || IsAnchored(horzAlign))
		{
			rectAppliedX = offsetX;
			*x += offsetX;
		}

		if (isUnanchoredMoved || IsAnchored(vertAlign))
		{
			rectAppliedY = offsetY;
			*y += offsetY;
		}
	}

	float HudParallax::ScrPlace_ApplyX_Hk(const float* placement, float x, int horzAlign)
	{
		const float placed = reinterpret_cast<ScrPlace_ApplyAxis_t>(Utils::Hook::Rebase(ScrPlace_ApplyX))(placement, x, horzAlign);

		hudElemAppliedX = 0.0f;

		if (!isHudElemMoved)
		{
			return placed;
		}

		if (!isUnanchoredMoved && !IsAnchored(horzAlign))
		{
			return placed;
		}

		hudElemAppliedX = offsetX;
		return placed + offsetX;
	}

	float HudParallax::ScrPlace_ApplyY_Hk(const float* placement, float y, int vertAlign)
	{
		const float placed = reinterpret_cast<ScrPlace_ApplyAxis_t>(Utils::Hook::Rebase(ScrPlace_ApplyY))(placement, y, vertAlign);

		hudElemAppliedY = 0.0f;

		if (!isHudElemMoved)
		{
			return placed;
		}

		if (!isUnanchoredMoved && !IsAnchored(vertAlign))
		{
			return placed;
		}

		hudElemAppliedY = offsetY;
		return placed + offsetY;
	}

	float HudParallax::TextFloorX_Hk(float value)
	{
		return std::floor(value - rectAppliedX) + rectAppliedX;
	}

	float HudParallax::TextFloorY_Hk(float value)
	{
		return std::floor(value - rectAppliedY) + rectAppliedY;
	}

	float HudParallax::HudElemFloorX_Hk(float value)
	{
		hudElemHeldX = hudElemAppliedX;
		return std::floor(value - hudElemAppliedX);
	}

	float HudParallax::HudElemFloorY_Hk(float value)
	{
		hudElemHeldY = hudElemAppliedY;
		return std::floor(value - hudElemAppliedY);
	}

	void HudParallax::HudElemPlace_Hk(int localClientNum, const void* hudElem, float* origin, void* layout)
	{
		hudElemHeldX = 0.0f;
		hudElemHeldY = 0.0f;

		reinterpret_cast<HudElemPlace_t>(Utils::Hook::Rebase(hudElemPlace))(localClientNum, hudElem, origin, layout);

		origin[0] += hudElemHeldX;
		origin[1] += hudElemHeldY;
	}

	void HudParallax::RegisterDvars()
	{
		cg_hudParallax = Dvar::Register("cg_hudParallax", false, Game::DVAR_ARCHIVE, "Move the hud with the view, as IW7 does");
		cg_hudParallaxMenus = Dvar::Register("cg_hudParallaxMenus", true, Game::DVAR_ARCHIVE, "Move the hud menus");
		cg_hudParallaxHudElems = Dvar::Register("cg_hudParallaxHudElems", true, Game::DVAR_ARCHIVE, "Move the script hud elements");
		cg_hudParallaxUnanchored = Dvar::Register("cg_hudParallaxUnanchored", false, Game::DVAR_ARCHIVE, "Also move items aligned fullscreen or without scaling");

		cg_hudParallaxViewRate = Dvar::Register("cg_hudParallaxViewRate", -0.12f, -100.0f, 100.0f, Game::DVAR_NONE, "How much turning the view moves the hud, negative lags behind");
		cg_hudParallaxAnimRate = Dvar::Register("cg_hudParallaxAnimRate", -1.5f, -100.0f, 100.0f, Game::DVAR_NONE, "How much camera animation moves the hud, negative lags behind");
		cg_hudParallaxLinearForward = Dvar::Register("cg_hudParallaxLinearForward", 0.0f, 0.0f, 360.0f, Game::DVAR_NONE, "Degrees of pitch from a change in forward speed");
		cg_hudParallaxLinearSide = Dvar::Register("cg_hudParallaxLinearSide", 0.0f, 0.0f, 360.0f, Game::DVAR_NONE, "Degrees of yaw from a change in sideways speed");
		cg_hudParallaxLinearUp = Dvar::Register("cg_hudParallaxLinearUp", 1.0f, 0.0f, 360.0f, Game::DVAR_NONE, "Degrees of pitch from a change in vertical speed");
		cg_hudParallaxLinearMax = Dvar::Register("cg_hudParallaxLinearMax", 40.0f, 0.01f, 100000.0f, Game::DVAR_NONE, "Largest speed change counted, in units per second");
		cg_hudParallaxLinearDecay = Dvar::Register("cg_hudParallaxLinearDecay", 0.3f, 0.01f, 10.0f, Game::DVAR_NONE, "Seconds for a full speed change to fade out");
		cg_hudParallaxSmoothing = Dvar::Register("cg_hudParallaxSmoothing", 7.0f, 0.01f, 1000.0f, Game::DVAR_NONE, "How fast the hud follows its target, per second");
		cg_hudParallaxMaxPitch = Dvar::Register("cg_hudParallaxMaxPitch", 4.5f, 0.0f, 360.0f, Game::DVAR_NONE, "Largest vertical offset, in degrees");
		cg_hudParallaxMaxYaw = Dvar::Register("cg_hudParallaxMaxYaw", 4.5f, 0.0f, 360.0f, Game::DVAR_NONE, "Largest horizontal offset, in degrees");
		cg_hudParallaxScale = Dvar::Register("cg_hudParallaxScale", 0.75f, 0.0f, 100.0f, Game::DVAR_NONE, "Scale on the final offset");
		cg_hudParallaxSnapDistance = Dvar::Register("cg_hudParallaxSnapDistance", 128.0f, 0.0f, 1000000.0f, Game::DVAR_NONE, "Camera move in one frame, in units, treated as a cut and not as motion");
		cg_hudParallaxBobPitch = Dvar::Register("cg_hudParallaxBobPitch", 0.25f, 0.0f, 10.0f, Game::DVAR_NONE, "Degrees of pitch per unit of the view bob's height, so the hud sways with each step");
		cg_hudParallaxBobYaw = Dvar::Register("cg_hudParallaxBobYaw", 0.1f, 0.0f, 10.0f, Game::DVAR_NONE, "Degrees of yaw per unit of the view bob's side motion");
		cg_hudParallaxSnapAngle = Dvar::Register("cg_hudParallaxSnapAngle", 30.0f, 0.0f, 360.0f, Game::DVAR_NONE, "Camera turn in one frame not made by aiming, in degrees, treated as a cut and not as motion");
	}

	HudParallax::HudParallax()
	{
		bool isExpected = Utils::Hook::BranchesTo(R_RenderScene_CG_Draw2DCall, CG_Draw2D, HOOK_CALL)
			&& Utils::Hook::BranchesTo(ScrPlace_ApplyRect, ScrPlace_ApplyRectWithoutSplitScreenScaling, HOOK_JUMP)
			&& Utils::Hook::BranchesTo(ScrPlace_ApplyRectClamped_ScrPlace_ApplyRectWithoutSplitScreenScalingCall, ScrPlace_ApplyRectWithoutSplitScreenScaling, HOOK_CALL)
			&& Utils::Hook::BranchesTo(Rect_ContainsPoint_ScrPlace_ApplyRectCall, ScrPlace_ApplyRect, HOOK_CALL)
			&& Utils::Hook::BranchesTo(Scroll_Slider_SetThumbPos_ScrPlace_ApplyRectCall, ScrPlace_ApplyRect, HOOK_CALL)
			&& Utils::Hook::BranchesTo(GetHudElemOrg_ScrPlace_ApplyXCall, ScrPlace_ApplyX, HOOK_CALL)
			&& Utils::Hook::BranchesTo(GetHudElemOrg_ScrPlace_ApplyYCall, ScrPlace_ApplyY, HOOK_CALL)
			&& Utils::Hook::BranchesTo(CG_Draw2dHudElems_hudElemPlaceCall, hudElemPlace, HOOK_CALL)
			&& Utils::Hook::BranchesTo(hudElemPlace_floorfCallX, floorfEntry, HOOK_CALL)
			&& Utils::Hook::BranchesTo(hudElemPlace_floorfCallY, floorfEntry, HOOK_CALL);

		for (const auto call : CG_Draw2D_Menu_PaintViewportCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(call, Menu_PaintViewport, HOOK_CALL);
		}

		for (std::size_t i = 0; i < std::size(textX_floorfCalls); ++i)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(textX_floorfCalls[i], floorfEntry, HOOK_CALL);
			isExpected = isExpected && Utils::Hook::BranchesTo(textY_floorfCalls[i], floorfEntry, HOOK_CALL);
		}

		if (!isExpected)
		{
			Logger::Error("hudparallax: CG_Draw2D or ScrPlace does not read as expected, no hud parallax\n");
			return;
		}

		void* const applyRectInner = reinterpret_cast<void*>(Utils::Hook::Rebase(ScrPlace_ApplyRectWithoutSplitScreenScaling));

		struct HookSite
		{
			Utils::Hook* hook;
			std::uintptr_t site;
			void* replacement;
			bool asJump;
		};

		const HookSite sites[] =
		{
			{ &containsPointHook, Rect_ContainsPoint_ScrPlace_ApplyRectCall, applyRectInner, HOOK_CALL },
			{ &sliderThumbHook, Scroll_Slider_SetThumbPos_ScrPlace_ApplyRectCall, applyRectInner, HOOK_CALL },
			{ &textFloorXHooks[0], textX_floorfCalls[0], reinterpret_cast<void*>(TextFloorX_Hk), HOOK_CALL },
			{ &textFloorXHooks[1], textX_floorfCalls[1], reinterpret_cast<void*>(TextFloorX_Hk), HOOK_CALL },
			{ &textFloorXHooks[2], textX_floorfCalls[2], reinterpret_cast<void*>(TextFloorX_Hk), HOOK_CALL },
			{ &textFloorYHooks[0], textY_floorfCalls[0], reinterpret_cast<void*>(TextFloorY_Hk), HOOK_CALL },
			{ &textFloorYHooks[1], textY_floorfCalls[1], reinterpret_cast<void*>(TextFloorY_Hk), HOOK_CALL },
			{ &textFloorYHooks[2], textY_floorfCalls[2], reinterpret_cast<void*>(TextFloorY_Hk), HOOK_CALL },
			{ &hudElemFloorXHook, hudElemPlace_floorfCallX, reinterpret_cast<void*>(HudElemFloorX_Hk), HOOK_CALL },
			{ &hudElemFloorYHook, hudElemPlace_floorfCallY, reinterpret_cast<void*>(HudElemFloorY_Hk), HOOK_CALL },
			{ &hudElemPlaceHook, CG_Draw2dHudElems_hudElemPlaceCall, reinterpret_cast<void*>(HudElemPlace_Hk), HOOK_CALL },
			{ &applyRectHook, ScrPlace_ApplyRect, reinterpret_cast<void*>(ScrPlace_ApplyRect_Hk), HOOK_JUMP },
			{ &applyRectClampedHook, ScrPlace_ApplyRectClamped_ScrPlace_ApplyRectWithoutSplitScreenScalingCall, reinterpret_cast<void*>(ScrPlace_ApplyRect_Hk), HOOK_CALL },
			{ &applyXHook, GetHudElemOrg_ScrPlace_ApplyXCall, reinterpret_cast<void*>(ScrPlace_ApplyX_Hk), HOOK_CALL },
			{ &applyYHook, GetHudElemOrg_ScrPlace_ApplyYCall, reinterpret_cast<void*>(ScrPlace_ApplyY_Hk), HOOK_CALL },
			{ &paintViewportHooks[0], CG_Draw2D_Menu_PaintViewportCalls[0], reinterpret_cast<void*>(Menu_PaintViewport_Hk), HOOK_CALL },
			{ &paintViewportHooks[1], CG_Draw2D_Menu_PaintViewportCalls[1], reinterpret_cast<void*>(Menu_PaintViewport_Hk), HOOK_CALL },
			{ &draw2DHook, R_RenderScene_CG_Draw2DCall, reinterpret_cast<void*>(CG_Draw2D_Hk), HOOK_CALL },
		};

		bool isSeated = true;

		for (const auto& site : sites)
		{
			isSeated = site.hook->Initialize(site.site, site.replacement, site.asJump)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (const auto& site : sites)
			{
				site.hook->Uninstall();
			}

			Logger::Error("hudparallax: could not seat every hook, no hud parallax\n");
			return;
		}

		Events::OnDvarInit([]
		{
			RegisterDvars();
		});
	}
}
