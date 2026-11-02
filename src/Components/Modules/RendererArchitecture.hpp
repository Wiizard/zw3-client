#pragma once
#include "T7ArchitectureVisibilityPolicy.hpp"

namespace Components::RendererArchitecture
{
	struct Result { unsigned changed = 0; unsigned clusters = 0; };
	inline Result Raise(Game::GfxWorld* world, bool convertedWorld)
	{
		Result result;
		if (!convertedWorld || !world || !world->dpvs.smodelInsts || !world->dpvs.smodelDrawInsts
			|| world->dpvs.smodelCount > 65535) return result;
		for (unsigned i = 0; i < world->dpvs.smodelCount; ++i)
		{
			auto& draw = world->dpvs.smodelDrawInsts[i];
			const auto* model = draw.model;
			if (!model || !std::isfinite(draw.placement.scale) || draw.placement.scale <= 0) continue;
			const auto& bounds = world->dpvs.smodelInsts[i].bounds;
			const auto distance = t7architecture::distance(draw.cullDist, draw.placement.origin,
				bounds.midPoint, bounds.halfSize);
			if (distance == draw.cullDist) continue;
			draw.cullDist = distance;
			++result.changed;
			result.clusters += model->name && std::string_view(model->name).starts_with("t7cl_");
		}
		return result;
	}
}
