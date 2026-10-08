#include "STDInclude.hpp"

#include <bitset>

#include "JSON.hpp"

namespace Utils::JSON
{
	Game::Bounds ReadBounds(const rapidjson::Value& value)
	{
		Game::Bounds bounds{};
		CopyArray(bounds.midPoint, value["midPoint"]);
		CopyArray(bounds.halfSize, value["halfSize"]);

		return bounds;
	}

	rapidjson::Value ToJson(const Game::Bounds& bounds, Allocator& allocator)
	{
		rapidjson::Value json(rapidjson::kObjectType);

		json.AddMember("midPoint", MakeArray(bounds.midPoint, 3, allocator), allocator);
		json.AddMember("halfSize", MakeArray(bounds.halfSize, 3, allocator), allocator);

		return json;
	}

	bool TryReadFlags(const std::string& binaryFlags, std::size_t size, unsigned long& flags)
	{
		const std::size_t bitCount = size * 8;

		if (bitCount > 64 || binaryFlags.size() > bitCount)
		{
			return false;
		}

		std::bitset<64> bits;
		std::size_t position = bitCount;

		for (const char bit : binaryFlags)
		{
			--position;
			bits.set(position, bit == '1');
		}

		flags = bits.to_ulong();
		return true;
	}
}
