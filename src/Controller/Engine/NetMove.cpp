#include "STDInclude.hpp"

#include "Controller/Engine/NetMove.hpp"

namespace Controller::Engine
{
	MoveDelta UnpackMove(std::uint16_t packed, int key) noexcept
	{
		const int bits = (key ^ packed) & 0xFFFF;

		return { static_cast<std::int8_t>(bits & 0xFF), static_cast<std::int8_t>((bits >> 8) & 0xFF) };
	}
}
