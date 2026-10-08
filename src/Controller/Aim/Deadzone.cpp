#include "STDInclude.hpp"

#include "Controller/Aim/Deadzone.hpp"

namespace Controller::Aim
{
	static bool IsInHalfOpenUnit(float value) noexcept
	{
		return value >= 0.0f && value < 1.0f;
	}

	bool IsValid(const DeadzoneParams& params, std::string& why)
	{
		if (!IsInHalfOpenUnit(params.inner.value))
		{
			why = "inner deadzone must be in [0, 1)";
			return false;
		}

		if (!IsInHalfOpenUnit(params.outer.value))
		{
			why = "outer deadzone must be in [0, 1)";
			return false;
		}

		if (!IsInHalfOpenUnit(params.anti.value))
		{
			why = "anti-deadzone must be in [0, 1)";
			return false;
		}

		if (params.inner.value >= 1.0f - params.outer.value)
		{
			why = "inner deadzone must be below (1 - outer deadzone)";
			return false;
		}

		return true;
	}

	StickVector ApplyDeadzone(const DeadzoneParams& params, StickVector stick) noexcept
	{
		const float inner = params.inner.value;
		const float outer = params.outer.value;
		const float anti = params.anti.value;

		assert(inner >= 0.0f && outer >= 0.0f && anti >= 0.0f && inner < 1.0f - outer);

		const float magnitude = std::sqrt(stick.x * stick.x + stick.y * stick.y);

		if (magnitude <= inner)
		{
			return { 0.0f, 0.0f };
		}

		const float range = (1.0f - outer) - inner;
		float travel = std::clamp((magnitude - inner) / range, 0.0f, 1.0f);

		if (anti > 0.0f)
		{
			travel = anti + travel * (1.0f - anti);
		}

		const float scale = travel / magnitude;
		return { stick.x * scale, stick.y * scale };
	}
}
