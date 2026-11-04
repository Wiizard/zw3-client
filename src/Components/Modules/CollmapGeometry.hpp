#pragma once

namespace Components::CollmapGeometry
{
	template <typename Model>
	auto Available(const Model* model) -> decltype(model->physCollmap)
	{
		if (!model)
		{
			return nullptr;
		}

		auto* const geometry = model->physCollmap;
		return geometry && geometry->count && geometry->geoms ? geometry : nullptr;
	}
}
