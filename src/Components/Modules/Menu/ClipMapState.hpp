#pragma once

namespace Components::Menu::BackdropData
{
	template <typename ClipMap>
	const char* loaded_clip_map_name(ClipMap* map) noexcept
	{
		if (map == nullptr) return nullptr;

		// CM_Unload clears the clip map but retains its name, which belongs to the freed level zone.
		// Discard it before any string conversion or database lookup can dereference it.
		if (map->mapEnts == nullptr)
		{
			map->name = nullptr;
			return nullptr;
		}

		return map->name;
	}
}
