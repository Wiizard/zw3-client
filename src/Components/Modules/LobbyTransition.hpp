#pragma once

namespace Components::LobbyTransition
{
	inline constexpr unsigned int tauntHoldMs = 2800;
	inline constexpr unsigned int doorStartMs = tauntHoldMs + 1200;
	inline constexpr unsigned int whiteStartMs = tauntHoldMs + 1600;
	inline constexpr unsigned int doorEndMs = tauntHoldMs + 3000;
	inline constexpr unsigned int whiteEndMs = tauntHoldMs + 2800;
	inline constexpr unsigned int cameraEndMs = tauntHoldMs + 3400;
	inline constexpr unsigned int endMs = tauntHoldMs + 3600;

	constexpr float Progress(unsigned int elapsedMs, unsigned int startMs, unsigned int finishMs)
	{
		if (elapsedMs <= startMs)
		{
			return 0.0f;
		}

		if (elapsedMs >= finishMs)
		{
			return 1.0f;
		}

		return static_cast<float>(elapsedMs - startMs) / static_cast<float>(finishMs - startMs);
	}

	constexpr float Smooth(float progress)
	{
		return progress * progress * (3.0f - 2.0f * progress);
	}

	constexpr float CameraProgress(unsigned int elapsedMs)
	{
		return Progress(elapsedMs, tauntHoldMs, cameraEndMs);
	}

	constexpr float WhiteOpacity(unsigned int elapsedMs)
	{
		return Smooth(Progress(elapsedMs, whiteStartMs, whiteEndMs));
	}

	constexpr float BlackOpacity(unsigned int elapsedMs)
	{
		return Smooth(Progress(elapsedMs, whiteEndMs, endMs));
	}

	static_assert(whiteStartMs <= doorEndMs && doorEndMs <= cameraEndMs && cameraEndMs <= endMs);
	static_assert(WhiteOpacity(0) == 0.0f && WhiteOpacity(whiteStartMs) == 0.0f);
	static_assert(WhiteOpacity(whiteEndMs) == 1.0f);
	static_assert(BlackOpacity(whiteEndMs) == 0.0f);
	static_assert(BlackOpacity(endMs) == 1.0f);
	static_assert(CameraProgress(tauntHoldMs) == 0.0f && CameraProgress(cameraEndMs) == 1.0f);
}
