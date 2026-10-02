#pragma once

namespace Components::LobbyTransition
{
	inline constexpr unsigned TauntHoldMs = 2800;
	inline constexpr unsigned DoorStartMs = TauntHoldMs + 1200;
	inline constexpr unsigned WhiteStartMs = TauntHoldMs + 1600;
	inline constexpr unsigned DoorEndMs = TauntHoldMs + 3000;
	inline constexpr unsigned WhiteEndMs = TauntHoldMs + 2800;
	inline constexpr unsigned CameraEndMs = TauntHoldMs + 3400;
	inline constexpr unsigned EndMs = TauntHoldMs + 3600;

	constexpr float Progress(unsigned elapsed, unsigned start, unsigned end)
	{
		return elapsed <= start ? 0.0f : elapsed >= end ? 1.0f :
			static_cast<float>(elapsed - start) / static_cast<float>(end - start);
	}
	constexpr float Smooth(float t) { return t * t * (3.0f - 2.0f * t); }
	constexpr float CameraProgress(unsigned elapsed) { return Progress(elapsed, TauntHoldMs, CameraEndMs); }
	constexpr float WhiteOpacity(unsigned elapsed) { return Smooth(Progress(elapsed, WhiteStartMs, WhiteEndMs)); }
	constexpr float BlackOpacity(unsigned elapsed) { return Smooth(Progress(elapsed, WhiteEndMs, EndMs)); }

	static_assert(WhiteStartMs <= DoorEndMs && DoorEndMs <= CameraEndMs && CameraEndMs <= EndMs);
	static_assert(WhiteOpacity(0) == 0.0f && WhiteOpacity(WhiteStartMs) == 0.0f);
	static_assert(WhiteOpacity(WhiteEndMs) == 1.0f);
	static_assert(BlackOpacity(WhiteEndMs) == 0.0f);
	static_assert(BlackOpacity(EndMs) == 1.0f);
	static_assert(CameraProgress(TauntHoldMs) == 0.0f && CameraProgress(CameraEndMs) == 1.0f);
}
