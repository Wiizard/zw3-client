#include "STDInclude.hpp"

#include "Controller/Engine/View.hpp"
#include "Controller/Engine/Engine.hpp"

namespace Controller::Engine
{
	using namespace Controller::Aim;

	static constexpr float moveScale = 127.0f;

	static constexpr int locationNeedsAngle = 0x80;

	struct TargetRegion
	{
		float halfWidth;
		float halfHeight;
	};

	static char ClampMove(int value) noexcept
	{
		return static_cast<char>(std::clamp(value, -128, 127));
	}

	static float DiagonalMoveScale(float forward, float side) noexcept
	{
		if (forward == 0.0f && side == 0.0f)
		{
			return moveScale;
		}

		float ratio = 0.0f;

		if (std::fabs(side) <= std::fabs(forward))
		{
			ratio = side / forward;
		}
		else
		{
			ratio = forward / side;
		}

		return std::sqrt(ratio * ratio + 1.0f) * moveScale;
	}

	static std::optional<std::vector<Knot>> TryReadEngineGraph(int index)
	{
		if (index < 0 || static_cast<unsigned int>(index) >= static_cast<unsigned int>(Game::AIM_ASSIST_GRAPH_COUNT))
		{
			return std::nullopt;
		}

		const auto& graph = Game::aaInputGraph[index];
		const auto knotCount = static_cast<std::size_t>(graph.knotCount);

		if (knotCount < 2 || knotCount > maxGraphKnots)
		{
			return std::nullopt;
		}

		std::vector<Knot> knots;
		knots.reserve(knotCount);

		for (std::size_t i = 0; i != knotCount; ++i)
		{
			knots.push_back(Knot{ graph.knots[i][0], graph.knots[i][1] * graph.scale });
		}

		return knots;
	}

	static std::size_t EngineGraphSignature(int index)
	{
		if (index < 0 || static_cast<unsigned int>(index) >= static_cast<unsigned int>(Game::AIM_ASSIST_GRAPH_COUNT))
		{
			return 0;
		}

		const auto& graph = Game::aaInputGraph[index];
		const auto knotCount = static_cast<std::size_t>(graph.knotCount);

		std::size_t hash = std::hash<float>{}(graph.scale) * 31u + knotCount;

		for (std::size_t i = 0; i != knotCount && i != maxGraphKnots; ++i)
		{
			hash = hash * 31u + std::hash<float>{}(graph.knots[i][0]);
			hash = hash * 31u + std::hash<float>{}(graph.knots[i][1]);
		}

		return hash;
	}

	static DeadzoneParams StickDeadzone(const Dvars& dvars)
	{
		const DeadzoneParams params
		{
			Magnitude{ Read(dvars.stickDeadzoneMin, 0.2f) },
			Magnitude{ Read(dvars.stickDeadzoneMax, 0.01f) },
			Magnitude{ Read(dvars.stickAntiDeadzone, 0.0f) },
		};

		std::string why;

		if (IsValid(params, why))
		{
			return params;
		}

		return DeadzoneParams{ Magnitude{ 0.2f }, Magnitude{ 0.01f }, Magnitude{ 0.0f } };
	}

	static AimProfile MakeProfile(const Dvars& dvars, float yawRate, float pitchRate)
	{
		AimProfile profile;
		profile.yawRate = DegreesPerSecond{ yawRate };
		profile.pitchRate = DegreesPerSecond{ pitchRate };
		profile.deadzone = DeadzoneParams
		{
			Magnitude{ Read(dvars.stickDeadzoneMin, 0.2f) },
			Magnitude{ Read(dvars.stickDeadzoneMax, 0.01f) },
			Magnitude{ Read(dvars.stickAntiDeadzone, 0.0f) },
		};
		return profile;
	}

	static std::size_t TuningSignature(const Dvars& dvars)
	{
		const float values[] =
		{
			Read(dvars.turnRateYaw, 260.0f),
			Read(dvars.turnRateYawAds, 90.0f),
			Read(dvars.turnRatePitch, 90.0f),
			Read(dvars.turnRatePitchAds, 55.0f),
			Read(dvars.stickDeadzoneMin, 0.2f),
			Read(dvars.stickDeadzoneMax, 0.01f),
			Read(dvars.stickAntiDeadzone, 0.0f),
			Read(dvars.accelRate, 1200.0f),
			Read(dvars.viewSensitivity, 1.0f),
			static_cast<float>(Read(dvars.accelEnabled, true)),
			static_cast<float>(Read(dvars.graphEnabled, true)),
			static_cast<float>(Read(dvars.graphIndex, 3)),
		};

		std::size_t hash = 0;

		for (const float value : values)
		{
			hash = hash * 31u + std::hash<float>{}(value);
		}

		if (Read(dvars.graphEnabled, true))
		{
			hash = hash * 31u + EngineGraphSignature(Read(dvars.graphIndex, 3));
		}

		return hash;
	}

	static bool IsInRegion(const Game::AimScreenTarget& target, const TargetRegion& region) noexcept
	{
		return region.halfWidth >= target.clipMins[0] && target.clipMaxs[0] >= -region.halfWidth
			&& region.halfHeight >= target.clipMins[1] && target.clipMaxs[1] >= -region.halfHeight;
	}

	static const Game::AimScreenTarget* BestTarget(const Game::AimAssistGlobals& aimAssist, float range, const TargetRegion& region) noexcept
	{
		const float rangeSquared = range * range;

		for (int i = 0; i != aimAssist.screenTargetCount; ++i)
		{
			const auto& target = aimAssist.screenTargets[i];

			if (target.distSqr <= rangeSquared && IsInRegion(target, region))
			{
				return &target;
			}
		}

		return nullptr;
	}

	static const Game::AimScreenTarget* PrevOrBestTarget(const Game::AimAssistGlobals& aimAssist, float range, const TargetRegion& region, int previous) noexcept
	{
		if (previous != Game::AIM_TARGET_INVALID)
		{
			for (int i = 0; i != aimAssist.screenTargetCount; ++i)
			{
				const auto& target = aimAssist.screenTargets[i];

				if (target.entIndex == previous && range * range > target.distSqr && IsInRegion(target, region))
				{
					return &target;
				}
			}
		}

		return BestTarget(aimAssist, range, region);
	}

	static float AssistRange(const Game::AimAssistGlobals& aimAssist, float scale) noexcept
	{
		if (aimAssist.ps.weapIndex == 0)
		{
			return 0.0f;
		}

		const auto* weaponDef = Game::BG_GetWeaponDef(static_cast<unsigned int>(aimAssist.ps.weapIndex));

		if (weaponDef == nullptr)
		{
			return 0.0f;
		}

		return std::lerp(weaponDef->aimAssistRange, weaponDef->aimAssistRangeAds, std::clamp(aimAssist.adsLerp, 0.0f, 1.0f)) * scale;
	}

	static bool IsSlowdownActive(const Game::AimAssistPlayerState& playerState) noexcept
	{
		if (playerState.weapIndex == 0)
		{
			return false;
		}

		const auto* weaponDef = Game::BG_GetWeaponDef(static_cast<unsigned int>(playerState.weapIndex));

		if (weaponDef == nullptr || weaponDef->requireLockonToFire)
		{
			return false;
		}

		if ((playerState.linkFlags & Game::PLF_WEAPONVIEW_ONLY) != 0)
		{
			return false;
		}

		if (playerState.weaponState >= Game::WEAPON_STUNNED_START && playerState.weaponState <= Game::WEAPON_STUNNED_END)
		{
			return false;
		}

		if ((playerState.eFlags & (Game::EF_VEHICLE_ACTIVE | Game::EF_TURRET_ACTIVE_DUCK | Game::EF_TURRET_ACTIVE_PRONE)) != 0)
		{
			return false;
		}

		return playerState.hasAmmo;
	}

	static bool IsUsingOffhand(const Game::AimAssistPlayerState& playerState) noexcept
	{
		if ((playerState.weapFlags & Game::PWF_USING_OFFHAND) == 0 || playerState.weapIndex == 0)
		{
			return false;
		}

		const auto* weaponDef = Game::BG_GetWeaponDef(static_cast<unsigned int>(playerState.weapIndex));
		return weaponDef != nullptr && weaponDef->offhandClass != Game::OFFHAND_CLASS_NONE;
	}

	ViewDriver::ViewDriver(const Context& context, const Dvars& dvars)
		: context(context),
		dvars(dvars)
	{
	}

	bool ViewDriver::TryEnsureProcessor()
	{
		const std::size_t signature = TuningSignature(this->dvars);

		if (this->hasSignature && signature == this->tuningSignature && this->processor)
		{
			return true;
		}

		this->tuningSignature = signature;
		this->hasSignature = true;

		AimSettings settings;
		settings.hip = MakeProfile(this->dvars, Read(this->dvars.turnRateYaw, 260.0f), Read(this->dvars.turnRatePitch, 90.0f));
		settings.ads = MakeProfile(this->dvars, Read(this->dvars.turnRateYawAds, 90.0f), Read(this->dvars.turnRatePitchAds, 55.0f));

		float accel = 0.0f;

		if (Read(this->dvars.accelEnabled, true))
		{
			accel = Read(this->dvars.accelRate, 1200.0f) * Read(this->dvars.viewSensitivity, 1.0f);
		}

		settings.accel = TurnIntegrator::Limits{ DegreesPerSecondSquared{ accel }, DegreesPerSecondSquared{ 0.0f } };

		if (Read(this->dvars.graphEnabled, true))
		{
			settings.graphKnots = TryReadEngineGraph(Read(this->dvars.graphIndex, 3));
		}

		settings.isGraphMonotonic = false;

		std::string why;
		auto made = AimCalibration::TryMake(settings, why);

		if (!made)
		{
			if (!this->hasReportedInvalid)
			{
				this->hasReportedInvalid = true;
				this->context.Report(Severity::Warning, Facility::Aim, ErrorCode::GraphInvalid,
					std::format("aim configuration rejected ({}); controller view runs unshaped until it is corrected", why));
			}

			this->calibration.reset();
			this->processor.reset();
			return false;
		}

		this->hasReportedInvalid = false;
		this->calibration.emplace(std::move(*made));
		this->processor.emplace(this->calibration->ProcessorConfig());
		return true;
	}

	void ViewDriver::Observe(const CanonicalSample& sample)
	{
		const DeadzoneParams deadzone = StickDeadzone(this->dvars);

		const StickVector left = ApplyDeadzone(deadzone, sample.sticks[static_cast<std::size_t>(Stick::Left)].calibrated);
		const StickVector right = ApplyDeadzone(deadzone, sample.sticks[static_cast<std::size_t>(Stick::Right)].calibrated);

		const Mapping::StickLayout layout = Mapping::StickLayoutFromName(Read(this->dvars.sticksConfig, "thumbstick_default"));

		this->axes = Mapping::Resolve(layout, left, right);
	}

	void ViewDriver::Idle() noexcept
	{
		this->axes = Mapping::ResolvedAxes{};

		if (this->processor)
		{
			this->processor->Reset();
		}
	}

	bool ViewDriver::IsViewActive(int client) const
	{
		if (Game::Key_IsCatcherActive(client, Game::KEYCATCH_MASK_ANY) && Game::UI_GetActiveMenu(client) != Game::UIMENU_SCOREBOARD)
		{
			return false;
		}

		return (PmFlags(client) & Game::PMF_FROZEN) == 0;
	}

	void ViewDriver::ApplyMove(int client, Game::usercmd_s& cmd, float frameTime)
	{
		const float scale = DiagonalMoveScale(this->axes.forward, this->axes.side);

		cmd.forwardmove = ClampMove(cmd.forwardmove + static_cast<int>(std::floor(this->axes.forward * scale)));
		cmd.rightmove = ClampMove(cmd.rightmove + static_cast<int>(std::floor(this->axes.side * scale)));

		if (LastWeaponHand(client) == Game::WEAPON_HAND_LEFT)
		{
			const int oldButtons = cmd.buttons;
			cmd.buttons &= ~(Game::CMD_BUTTON_ATTACK | Game::CMD_BUTTON_THROW);

			if ((oldButtons & Game::CMD_BUTTON_ATTACK) != 0)
			{
				cmd.buttons |= Game::CMD_BUTTON_THROW;
			}

			if ((oldButtons & Game::CMD_BUTTON_THROW) != 0)
			{
				cmd.buttons |= Game::CMD_BUTTON_ATTACK;
			}
		}

		if (!this->IsViewActive(client) || !this->TryEnsureProcessor())
		{
			return;
		}

		float enginePitchAxis = -this->axes.pitch;

		if (Read(this->dvars.invertPitch, false))
		{
			enginePitchAxis = this->axes.pitch;
		}

		Game::AimInput input{};
		Game::AimOutput output{};
		input.deltaTime = frameTime;
		input.deltaTimeScaled = ClientFrameTime();
		input.pitch = ViewPitch(client);
		input.pitchAxis = enginePitchAxis;
		input.pitchMax = MaxPitchSpeed(client);
		input.yaw = ViewYaw(client);
		input.yawAxis = -this->axes.yaw;
		input.yawMax = MaxYawSpeed(client);
		input.forwardAxis = this->axes.forward;
		input.rightAxis = this->axes.side;
		input.buttons = cmd.buttons;
		input.localClientNum = client;

		AimAssistBegin(input, output);

		const auto& aimAssist = Game::aaGlobArray[client];

		const bool isMeleeSteering = aimAssist.initialized && aimAssist.autoMeleeState == Game::AIM_MELEE_STATE_UPDATING;

		AimFrameInput frame;

		if (!isMeleeSteering)
		{
			frame.look = StickVector{ this->axes.yaw, this->axes.pitch };
		}

		if (aimAssist.initialized)
		{
			frame.adsLerp = aimAssist.adsLerp;
			frame.fovScale = aimAssist.fovTurnRateScale;
		}

		frame.sensitivity = Read(this->dvars.viewSensitivity, 1.0f);
		frame.shouldScaleViewAxis = Read(this->dvars.scaleViewAxis, true);
		frame.isPitchInverted = Read(this->dvars.invertPitch, false);
		frame.deltaTime = Seconds{ frameTime };

		const bool isAssistAllowed = Read(this->dvars.aimAssistEnabled, true);
		const bool isSlowdownWanted = Read(this->dvars.slowdownEnabled, true) && Read(this->dvars.gpadSlowdownEnabled, true);

		if (aimAssist.initialized && isAssistAllowed && isSlowdownWanted && IsSlowdownActive(aimAssist.ps))
		{
			const float range = AssistRange(aimAssist, Read(this->dvars.aimAssistRangeScale, 1.0f));
			const TargetRegion region{ aimAssist.tweakables.slowdownRegionWidth, aimAssist.tweakables.slowdownRegionHeight };

			const bool isTargetPresent = BestTarget(aimAssist, range, region) != nullptr;

			frame.slowdownYaw = SlowdownScale(isTargetPresent, Read(this->dvars.slowdownYawScale, 0.4f), Read(this->dvars.slowdownYawScaleAds, 0.5f), aimAssist.adsLerp);
			frame.slowdownPitch = 1.0f;

			if (!IsUsingOffhand(aimAssist.ps))
			{
				frame.slowdownPitch = SlowdownScale(isTargetPresent, Read(this->dvars.slowdownPitchScale, 0.4f), Read(this->dvars.slowdownPitchScaleAds, 0.5f), aimAssist.adsLerp);
			}
		}

		if (MaxYawSpeed(client) > 0.0f)
		{
			frame.yawMax = DegreesPerSecond{ MaxYawSpeed(client) };
		}

		if (MaxPitchSpeed(client) > 0.0f)
		{
			frame.pitchMax = DegreesPerSecond{ MaxPitchSpeed(client) };
		}

		const AimFrameOutput turned = this->processor->Process(frame);

		output.pitch -= turned.pitchDelta.value;
		output.yaw -= turned.yawDelta.value;

		if (aimAssist.initialized)
		{
			AimFrameOutput lockOn;
			this->ApplyLockOn(input, lockOn);

			output.pitch -= lockOn.pitchDelta.value;
			output.yaw -= lockOn.yawDelta.value;
		}

		ViewPitch(client) = output.pitch;
		ViewYaw(client) = output.yaw;
		cmd.meleeChargeYaw = output.meleeChargeYaw;
		cmd.meleeChargeDist = output.meleeChargeDist;
	}

	void ViewDriver::ApplyLockOn(const Game::AimInput& input, AimFrameOutput& output)
	{
		auto& aimAssist = Game::aaGlobArray[input.localClientNum];

		const int previous = aimAssist.lockOnTargetEnt;
		aimAssist.lockOnTargetEnt = Game::AIM_TARGET_INVALID;

		const bool isLockOnWanted = Read(this->dvars.aimAssistEnabled, true) && Read(this->dvars.lockOnEnabled, true) && Read(this->dvars.gpadLockOnEnabled, true);

		if (!isLockOnWanted)
		{
			return;
		}

		if (IsUsingOffhand(aimAssist.ps) || aimAssist.autoMeleeState == Game::AIM_MELEE_STATE_UPDATING)
		{
			return;
		}

		if (aimAssist.ps.weapIndex == 0)
		{
			return;
		}

		const auto* weaponDef = Game::BG_GetWeaponDef(static_cast<unsigned int>(aimAssist.ps.weapIndex));

		if (weaponDef == nullptr || weaponDef->requireLockonToFire)
		{
			return;
		}

		const float threshold = Read(this->dvars.lockOnDeflection, 0.05f);

		if (threshold > std::fabs(input.pitchAxis) && threshold > std::fabs(input.yawAxis) && threshold > std::fabs(input.rightAxis))
		{
			return;
		}

		const float range = AssistRange(aimAssist, Read(this->dvars.aimAssistRangeScale, 1.0f));
		const TargetRegion region{ aimAssist.tweakables.lockOnRegionWidth, aimAssist.tweakables.lockOnRegionHeight };

		const auto* target = PrevOrBestTarget(aimAssist, range, region, previous);

		if (target == nullptr || !(target->distSqr > 0.0f))
		{
			return;
		}

		aimAssist.lockOnTargetEnt = target->entIndex;

		const auto& viewAxis = aimAssist.viewAxis;

		LockOnTarget lockOnTarget;
		lockOnTarget.targetVelocity = { target->velocity[0], target->velocity[1], target->velocity[2] };
		lockOnTarget.playerVelocity = { aimAssist.ps.velocity[0], aimAssist.ps.velocity[1], aimAssist.ps.velocity[2] };
		lockOnTarget.viewYawAxis = { -viewAxis[1][0], -viewAxis[1][1], -viewAxis[1][2] };
		lockOnTarget.viewPitchAxis = { viewAxis[2][0], viewAxis[2][1], viewAxis[2][2] };
		lockOnTarget.distance = std::sqrt(target->distSqr);

		LockOnParams params;
		params.yawStrength = Read(this->dvars.lockOnStrength, 0.6f);
		params.pitchStrength = Read(this->dvars.lockOnPitchStrength, 0.6f);

		const AimFrameOutput lockOn = LockOn(lockOnTarget, params, Seconds{ input.deltaTime });

		output.yawDelta = output.yawDelta + lockOn.yawDelta;
		output.pitchDelta = output.pitchDelta + lockOn.pitchDelta;
	}

	void ViewDriver::ApplyRemoteMove([[maybe_unused]] int client, Game::usercmd_s& cmd)
	{
		const float sensitivity = Read(this->dvars.viewSensitivity, 1.0f);

		const auto pitchMove = static_cast<int>(std::lround(-(this->axes.forward + this->axes.pitch) * moveScale * sensitivity));
		const auto yawMove = static_cast<int>(std::lround(-(this->axes.side + this->axes.yaw) * moveScale * sensitivity));

		cmd.remoteControlAngles[0] = ClampMove(cmd.remoteControlAngles[0] + pitchMove);
		cmd.remoteControlAngles[1] = ClampMove(cmd.remoteControlAngles[1] + yawMove);
	}

	void ViewDriver::ApplyLocationSelection(int client)
	{
		const auto* cg = Game::CL_GetLocalClientGlobals(client);

		if (cg == nullptr)
		{
			return;
		}

		const LocationSelection selection = SelectedLocation(client);

		float aspect = 1.0f;

		if (selection.mapWorldSize[1] != 0.0f)
		{
			aspect = selection.mapWorldSize[0] / selection.mapWorldSize[1];
		}

		float up = this->axes.forward;
		float right = this->axes.side;

		const float magnitude = up * up + right * right;

		if (magnitude > 1.0f)
		{
			const float length = std::sqrt(magnitude);
			up /= length;
			right /= length;
		}

		if (this->cursorSpeed == nullptr)
		{
			this->cursorSpeed = Game::Dvar_FindVar("cg_mapLocationSelectionCursorSpeed");
		}

		const float speed = Read(this->cursorSpeed, 100.0f);

		selection.location[0] += right * speed * selection.frameTime;
		selection.location[1] -= up * aspect * speed * selection.frameTime;

		const bool isAngleNeeded = (cg->predictedPlayerState.locationSelectionInfo & locationNeedsAngle) != 0;
		const bool isAngleSteered = this->axes.pitch != 0.0f || this->axes.yaw != 0.0f;

		if (isAngleNeeded && isAngleSteered)
		{
			const float direction[2] = { this->axes.pitch, -this->axes.yaw };

			*selection.angle = Game::AngleNormalize360(Game::vectoyaw(direction));
		}

		selection.angleLocation[0] = selection.location[0];
		selection.angleLocation[1] = selection.location[1];
	}
}
