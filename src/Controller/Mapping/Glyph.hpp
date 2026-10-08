#pragma once

#include "Controller/Types.hpp"

#include "Controller/Device/Identity.hpp"
#include "Controller/Mapping/Key.hpp"

namespace Controller::Mapping
{
	enum class GlyphFamily : std::uint8_t
	{
		Xbox,
		PlayStation,
	};

	GlyphFamily GlyphFamilyFor(Controller::Family device, std::optional<GlyphFamily> userOverride) noexcept;

	const char* GlyphFor(EngineKey key, GlyphFamily family) noexcept;
}
