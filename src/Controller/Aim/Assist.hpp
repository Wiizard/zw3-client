#pragma once

#include "Controller/Types.hpp"

#include "Controller/Clock.hpp"
#include "Controller/Aim/Deadzone.hpp"
#include "Controller/Aim/Graph.hpp"
#include "Controller/Aim/Integrator.hpp"
#include "Controller/Aim/Types.hpp"
#include "Controller/Sample/Axis.hpp"

namespace Controller::Aim
{
	struct AimProfile
	{
		DegreesPerSecond yawRate{ 0.0f };
		DegreesPerSecond pitchRate{ 0.0f };
		DeadzoneParams deadzone;
	};

	struct AimFrameInput
	{
		StickVector look;
		float adsLerp = 0.0f;
		float fovScale = 1.0f;
		float sensitivity = 1.0f;
		float slowdownYaw = 1.0f;
		float slowdownPitch = 1.0f;
		bool shouldScaleViewAxis = true;
		bool isPitchInverted = false;
		std::optional<DegreesPerSecond> yawMax;
		std::optional<DegreesPerSecond> pitchMax;
		Seconds deltaTime{ 0.0f };
	};

	struct AimFrameOutput
	{
		Degrees yawDelta{ 0.0f };
		Degrees pitchDelta{ 0.0f };
	};

	class AimProcessor
	{
	public:
		struct Config
		{
			AimProfile hip;
			AimProfile ads;
			TurnIntegrator::Limits accel;
			const AimGraph* graph = nullptr;
		};

		explicit AimProcessor(Config config);

		AimFrameOutput Process(const AimFrameInput& input) noexcept;

		void Reset() noexcept;

	private:
		Config config;
		TurnIntegrator yaw;
		TurnIntegrator pitch;
	};

	float SlowdownScale(bool isTargetPresent, float hipScale, float adsScale, float adsLerp) noexcept;

	StickVector ScaleDominantAxis(StickVector look) noexcept;

	struct LockOnTarget
	{
		WorldVector targetVelocity;
		WorldVector playerVelocity;
		WorldVector viewPitchAxis;
		WorldVector viewYawAxis;
		float distance = 0.0f;
	};

	struct LockOnParams
	{
		float yawStrength = 0.0f;
		float pitchStrength = 0.0f;
	};

	AimFrameOutput LockOn(const LockOnTarget& target, const LockOnParams& params, Seconds deltaTime) noexcept;
}
