#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "../src/Components/Modules/Remix/Iw4.hpp"
#include "../src/Components/Modules/RenderBudgetPolicy.hpp"

namespace Iw4 = Components::Remix::Iw4;
namespace Policy = Components::RenderBudgetPolicy;
namespace Engine
{
	struct Layout { unsigned smodelListCountShift; };
	static Layout layout{12};
	static const Layout& CurrentDrawSurfLayout() { return layout; }
}

static void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

static std::array<std::uintptr_t, 2> allowedRecords;
template<class T> static T ReadAt(std::uintptr_t address)
{
	// Reject the old decoder's invalid address before dereferencing the fixture.
	bool allowed = false;
	for (const auto record : allowedRecords)
	{
		allowed |= address >= record && address + sizeof(T) <= record + 14;
	}
	Require(allowed, "The reader selected an incorrect model record");
	T result;
	std::memcpy(&result, reinterpret_cast<const void*>(address), sizeof(result));
	return result;
}

static std::uint16_t unbaked = 0;
static bool IsStaticModelBaked(std::uint16_t instance) { return instance != unbaked; }
#include "HasUnbakedInstances.inc"

static void CheckDrawRecord(std::uintptr_t base, std::uint16_t token, std::uintptr_t expectedSurfaces)
{
	const std::uint16_t entries[] = {token};
	const unsigned i = 0;
#include "ReadDrawRecord.inc"
	Require(surfaces == expectedSurfaces, "Surface pointer was decoded from the wrong record");
	Require(surface == expectedSurfaces + 88 * (token & 15), "Surface index was lost");
	Require(instanceCount == 2, "Instance count was decoded from the wrong record");
}

static void WriteRecord(std::uintptr_t address, std::uintptr_t surfaces)
{
	std::memcpy(reinterpret_cast<void*>(address), &surfaces, sizeof(surfaces));
	const std::uint16_t instances[] = {2, 7, 65534};
	std::memcpy(reinterpret_cast<void*>(address + 8), instances, sizeof(instances));
}

int main()
{
	try
	{
		Require(Iw4::smodelRecordStride == 4, "Stock record layout must be the default");
		for (const std::size_t stride : {std::size_t{4}, Policy::modelCommandSlotBytes})
		{
			Iw4::smodelRecordStride = stride;
			std::vector<std::uint8_t> storage(4096 * stride + 14, 0xA5);
			const auto base = reinterpret_cast<std::uintptr_t>(storage.data());
			for (unsigned slot = 0; slot < 4096; ++slot)
			{
				allowedRecords = {base + slot * stride, base + slot * stride};
				const std::uintptr_t expectedSurfaces = 0x123456780000ull + slot * 16 * 88;
				WriteRecord(allowedRecords[0], expectedSurfaces);
				for (unsigned surface = 0; surface < 16; ++surface)
				{
					CheckDrawRecord(base, static_cast<std::uint16_t>((slot << 4) | surface), expectedSurfaces);
				}
			}
			// Both header widths must work with both record formats, independently.
			allowedRecords = {base + 8 * stride, base + 4095 * stride};
			WriteRecord(allowedRecords[0], 0x123456780000ull);
			WriteRecord(allowedRecords[1], 0x876543210000ull);
			for (const unsigned shift : {12u, 14u})
			{
				Engine::layout.smodelListCountShift = shift;
				std::uint16_t tokens[] = {0, static_cast<std::uint16_t>(1u << shift), 0x008F, 0xFFF3};
				void* list[] = {storage.data(), tokens};
				unbaked = 0;
				Require(!HasUnbakedInstances(list), "Fully baked models must remain baked");
				unbaked = 65534;
				Require(HasUnbakedInstances(list), "The unbaked instance was lost");
				Require(list[1] == tokens, "Instance inspection advanced the draw cursor");
			}
		}
		std::cout << "Remix model commands: 131072 draw tokens and independent instance/header layouts passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
