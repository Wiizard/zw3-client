#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Utils::UiScaling
{
	// iw4mp 2.0.13 screen placement. Bounds are relative to the viewport;
	// realViewportPosition is added by the renderer, not by ScrPlace_Apply*.
	struct Placement
	{
		float scaleVirtualToReal[2];
		float scaleVirtualToFull[2];
		float scaleRealToVirtual[2];
		float realViewportPosition[2];
		float realViewportSize[2];
		float virtualViewableMin[2];
		float virtualViewableMax[2];
		float realViewableMin[2];
		float realViewableMax[2];
		float virtualAdjustableMin[2];
		float virtualAdjustableMax[2];
		float realAdjustableMin[2];
		float realAdjustableMax[2];
		float subScreenLeft;
	};
	static_assert(sizeof(Placement) == 0x6C);
	static_assert(offsetof(Placement, realViewportSize) == 0x20);
	static_assert(offsetof(Placement, realViewableMin) == 0x38);
	static_assert(offsetof(Placement, subScreenLeft) == 0x68);

	constexpr float virtualHeight = 480.0f;
	constexpr float virtualWidth = 640.0f;
	constexpr float contentWidth = virtualHeight * (16.0f / 9.0f);

	inline bool Fit(Placement& placement, float pixelAspect = 1.0f)
	{
		const float width = placement.realViewportSize[0];
		const float height = placement.realViewportSize[1];
		// The native X scale is clamped to width / 640 below 4:3. Recover
		// the natural scale from Y so fitting does not inherit that distortion.
		const float scaleY = placement.scaleVirtualToReal[1];
		const float scaleX = scaleY / pixelAspect;
		if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(scaleX) || !std::isfinite(scaleY)
			|| !std::isfinite(pixelAspect) || pixelAspect <= 0.0f
			|| width <= 0.0f || height <= 0.0f || scaleX <= 0.0f || scaleY <= 0.0f)
		{
			return false;
		}

		// ZW3 uses the 16:9 extensions of the engine's 640x480 coordinates
		// (-106.667..746.667). Fit the entire authored area, including on 4:3.
		const float fit = std::min({ 1.0f, width / (contentWidth * scaleX), height / (virtualHeight * scaleY) });
		if (fit >= 0.9999f) return false;

		const float contentSize[] = { contentWidth * scaleX * fit, virtualHeight * scaleY * fit };
		placement.scaleVirtualToReal[0] = scaleX * fit;
		placement.scaleVirtualToReal[1] = scaleY * fit;
		for (int axis = 0; axis < 2; ++axis)
		{
			const float size = placement.realViewportSize[axis];
			const float inset = (size - contentSize[axis]) * 0.5f;
			const float fraction = contentSize[axis] / size;
			placement.scaleRealToVirtual[axis] = 1.0f / placement.scaleVirtualToReal[axis];

			// Preserve the user's safe-area fractions inside the fitted canvas.
			placement.realViewableMin[axis] = inset + placement.realViewableMin[axis] * fraction;
			placement.realViewableMax[axis] = inset + placement.realViewableMax[axis] * fraction;
			placement.realAdjustableMin[axis] = inset + placement.realAdjustableMin[axis] * fraction;
			placement.realAdjustableMax[axis] = inset + placement.realAdjustableMax[axis] * fraction;
			const float inverse = placement.scaleRealToVirtual[axis];
			placement.virtualViewableMin[axis] = placement.realViewableMin[axis] * inverse;
			placement.virtualViewableMax[axis] = placement.realViewableMax[axis] * inverse;
			placement.virtualAdjustableMin[axis] = placement.realAdjustableMin[axis] * inverse;
			placement.virtualAdjustableMax[axis] = placement.realAdjustableMax[axis] * inverse;
		}
		placement.subScreenLeft = (width - virtualWidth * placement.scaleVirtualToReal[0]) * 0.5f;
		return true;
	}

	inline float ApplyAxis(const Placement& placement, float position, int align, int axis)
	{
		const float scale = placement.scaleVirtualToReal[axis];
		switch (align)
		{
		case 1: return position * scale + placement.realViewableMin[axis];
		case 2: return position * scale + placement.realViewportSize[axis] * 0.5f;
		case 3: return position * scale + placement.realViewableMax[axis];
		case 4: return position * placement.scaleVirtualToFull[axis];
		case 5: return position;
		case 6: return position * placement.scaleRealToVirtual[axis];
		case 7: return position * scale + (placement.realViewableMin[axis] + placement.realViewableMax[axis]) * 0.5f;
		case 8: return position * scale + placement.realAdjustableMin[axis];
		case 9: return position * scale + (placement.realAdjustableMin[axis] + placement.realAdjustableMax[axis]) * 0.5f;
		case 10: return position * scale + placement.realAdjustableMax[axis];
		default:
			// The stock engine only has a horizontal inset. Fitting a wider
			// canvas also needs a vertical inset for unanchored menu items.
			return position * scale + (axis == 0 ? placement.subScreenLeft
				: std::max(0.0f, (placement.realViewportSize[1] - virtualHeight * scale) * 0.5f));
		}
	}

	inline float ScaleExtent(const Placement& placement, float size, int align, int axis)
	{
		switch (align)
		{
		case 4: return size * placement.scaleVirtualToFull[axis];
		case 5: return size;
		case 6: return size * placement.scaleRealToVirtual[axis];
		default: return size * placement.scaleVirtualToReal[axis];
		}
	}

	inline void ApplyRect(const Placement& placement, float& x, float& y, float& width, float& height, int horzAlign, int vertAlign)
	{
		x = ApplyAxis(placement, x, horzAlign, 0);
		y = ApplyAxis(placement, y, vertAlign, 1);
		width = ScaleExtent(placement, width, horzAlign, 0);
		height = ScaleExtent(placement, height, vertAlign, 1);
	}
}
