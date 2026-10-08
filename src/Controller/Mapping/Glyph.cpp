#include "STDInclude.hpp"

#include "Controller/Mapping/Glyph.hpp"

namespace Controller::Mapping
{
	struct GlyphEntry
	{
		EngineKey key;
		const char* xbox;
		const char* playStation;
	};

	static constexpr GlyphEntry glyphs[] =
	{
		{ EngineKey::ButtonA, "^\x01\x32\x32\x08" "button_a", "^\x01\x32\x32\x10" "button_ps3_cross" },
		{ EngineKey::ButtonB, "^\x01\x32\x32\x08" "button_b", "^\x01\x32\x32\x11" "button_ps3_circle" },
		{ EngineKey::ButtonX, "^\x01\x32\x32\x08" "button_x", "^\x01\x32\x32\x11" "button_ps3_square" },
		{ EngineKey::ButtonY, "^\x01\x32\x32\x08" "button_y", "^\x01\x32\x32\x13" "button_ps3_triangle" },
		{ EngineKey::ButtonLShoulder, "^\x01\x32\x32\x0D" "button_lshldr", "^\x01\x32\x32\x0D" "button_ps3_l1" },
		{ EngineKey::ButtonRShoulder, "^\x01\x32\x32\x0D" "button_rshldr", "^\x01\x32\x32\x0D" "button_ps3_r1" },
		{ EngineKey::ButtonStart, "^\x01\x32\x32\x0C" "button_start", "^\x01\x32\x32\x10" "button_ps3_start" },
		{ EngineKey::ButtonBack, "^\x01\x32\x32\x0B" "button_back", "^\x01\x32\x32\x0F" "button_ps3_back" },
		{ EngineKey::ButtonLStick, "^\x01\x48\x32\x0D" "button_lstick", "^\x01\x48\x32\x0D" "button_ps3_l3" },
		{ EngineKey::ButtonRStick, "^\x01\x48\x32\x0D" "button_rstick", "^\x01\x48\x32\x0D" "button_ps3_r3" },
		{ EngineKey::ButtonLTrigger, "^\x01\x32\x32\x0C" "button_ltrig", "^\x01\x32\x32\x0D" "button_ps3_l2" },
		{ EngineKey::ButtonRTrigger, "^\x01\x32\x32\x0C" "button_rtrig", "^\x01\x32\x32\x0D" "button_ps3_r2" },
		{ EngineKey::DpadUp, "^\x01\x32\x32\x07" "dpad_up", "^\x01\x32\x32\x0B" "dpad_ps3_up" },
		{ EngineKey::DpadDown, "^\x01\x32\x32\x09" "dpad_down", "^\x01\x32\x32\x0D" "dpad_ps3_down" },
		{ EngineKey::DpadLeft, "^\x01\x32\x32\x09" "dpad_left", "^\x01\x32\x32\x0D" "dpad_ps3_left" },
		{ EngineKey::DpadRight, "^\x01\x32\x32\x0A" "dpad_right", "^\x01\x32\x32\x0E" "dpad_ps3_right" },
	};

	GlyphFamily GlyphFamilyFor(Controller::Family device, std::optional<GlyphFamily> userOverride) noexcept
	{
		if (userOverride)
		{
			return *userOverride;
		}

		switch (device)
		{
		case Controller::Family::DualShock4:
		case Controller::Family::DualSense:
		case Controller::Family::DualSenseEdge:
			return GlyphFamily::PlayStation;

		case Controller::Family::Xbox:
		case Controller::Family::Unknown:
			return GlyphFamily::Xbox;
		}

		return GlyphFamily::Xbox;
	}

	const char* GlyphFor(EngineKey key, GlyphFamily family) noexcept
	{
		for (const auto& entry : glyphs)
		{
			if (entry.key != key)
			{
				continue;
			}

			if (family == GlyphFamily::PlayStation)
			{
				return entry.playStation;
			}

			return entry.xbox;
		}

		return nullptr;
	}
}
