#include "STDInclude.hpp"

#include "Controller/Aim/Assist.hpp"

namespace Controller::Aim
{
	static DegreesPerSecond LerpRate(DegreesPerSecond from, DegreesPerSecond to, float t) noexcept
	{
		return { std::lerp(from.value, to.value, t) };
	}

	static float SignOf(float value) noexcept
	{
		if (value >= 0.0f)
		{
			return 1.0f;
		}

		return -1.0f;
	}

	AimProcessor::AimProcessor(Config config) : config(config)
	{
	}

	void AimProcessor::Reset() noexcept
	{
		this->yaw.Reset();
		this->pitch.Reset();
	}

	AimFrameOutput AimProcessor::Process(const AimFrameInput& input) noexcept
	{
		const float adsLerp = std::clamp(input.adsLerp, 0.0f, 1.0f);

		const StickVector look = input.look;

		const float deflection = std::clamp(std::sqrt(look.x * look.x + look.y * look.y), 0.0f, 1.0f);

		float response = 1.0f;

		if (this->config.graph != nullptr)
		{
			response = this->config.graph->Evaluate(deflection);
		}

		StickVector adjusted{ look.x * response, look.y * response };

		if (input.shouldScaleViewAxis)
		{
			adjusted = ScaleDominantAxis(adjusted);
		}

		const float effectiveYaw = adjusted.x;
		const float effectivePitch = adjusted.y;

		const float gainYaw = input.fovScale * input.sensitivity * input.slowdownYaw;
		const float gainPitch = input.fovScale * input.sensitivity * input.slowdownPitch;

		DegreesPerSecond yawRate = LerpRate(this->config.hip.yawRate, this->config.ads.yawRate, adsLerp) * gainYaw;
		DegreesPerSecond pitchRate = LerpRate(this->config.hip.pitchRate, this->config.ads.pitchRate, adsLerp) * gainPitch;

		if (input.yawMax && input.yawMax->value < yawRate.value)
		{
			yawRate = *input.yawMax;
		}

		if (input.pitchMax && input.pitchMax->value < pitchRate.value)
		{
			pitchRate = *input.pitchMax;
		}

		const DegreesPerSecond yawTarget{ std::fabs(effectiveYaw) * yawRate.value };
		const DegreesPerSecond pitchTarget{ std::fabs(effectivePitch) * pitchRate.value };

		const Degrees yawDelta = this->yaw.Advance(yawTarget, this->config.accel, input.deltaTime) * SignOf(effectiveYaw);
		Degrees pitchDelta = this->pitch.Advance(pitchTarget, this->config.accel, input.deltaTime) * SignOf(effectivePitch);

		if (input.isPitchInverted)
		{
			pitchDelta = -pitchDelta;
		}

		return { yawDelta, pitchDelta };
	}

	float SlowdownScale(bool isTargetPresent, float hipScale, float adsScale, float adsLerp) noexcept
	{
		if (!isTargetPresent)
		{
			return 1.0f;
		}

		return std::lerp(hipScale, adsScale, std::clamp(adsLerp, 0.0f, 1.0f));
	}

	StickVector ScaleDominantAxis(StickVector look) noexcept
	{
		const float absoluteX = std::fabs(look.x);
		const float absoluteY = std::fabs(look.y);

		if (absoluteY <= absoluteX)
		{
			look.y *= 1.0f - (absoluteX - absoluteY);
		}
		else
		{
			look.x *= 1.0f - (absoluteY - absoluteX);
		}

		return look;
	}

	AimFrameOutput LockOn(const LockOnTarget& target, const LockOnParams& params, Seconds deltaTime) noexcept
	{
		if (target.distance <= 0.0f)
		{
			return {};
		}

		const float arc = target.distance * pi;

		const float pitchRate = (Dot(target.targetVelocity, target.viewPitchAxis) - Dot(target.playerVelocity, target.viewPitchAxis)) / arc * 180.0f * params.pitchStrength;
		const float yawRate = (Dot(target.targetVelocity, target.viewYawAxis) - Dot(target.playerVelocity, target.viewYawAxis)) / arc * 180.0f * params.yawStrength;

		return { Degrees{ yawRate * deltaTime.count() }, Degrees{ pitchRate * deltaTime.count() } };
	}
}
