#include "Utils/UiScaling.hpp"

#include <array>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <fstream>
#include <iterator>
#include <vector>
#endif

namespace
{
	using namespace Utils::UiScaling;

	void Require(bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}

	bool Near(float actual, float expected)
	{
		return std::abs(actual - expected) < 0.002f;
	}

	Placement MakePlacement(float width, float height, float safe = 0.85f)
	{
		Placement result{};
		result.realViewportSize[0] = width;
		result.realViewportSize[1] = height;
		result.scaleVirtualToReal[0] = std::min(width, height * (4.0f / 3.0f)) / virtualWidth;
		result.scaleVirtualToReal[1] = height / virtualHeight;
		result.scaleVirtualToFull[0] = width / virtualWidth;
		result.scaleVirtualToFull[1] = height / virtualHeight;
		for (int axis = 0; axis < 2; ++axis)
		{
			const float size = result.realViewportSize[axis];
			const float inset = std::floor((1.0f - safe) * size * 0.5f + 0.5f);
			result.scaleRealToVirtual[axis] = 1.0f / result.scaleVirtualToReal[axis];
			result.realViewableMin[axis] = inset;
			result.realViewableMax[axis] = size - inset;
			result.realAdjustableMin[axis] = 0.0f;
			result.realAdjustableMax[axis] = size;
			result.virtualViewableMin[axis] = inset * result.scaleRealToVirtual[axis];
			result.virtualViewableMax[axis] = (size - inset) * result.scaleRealToVirtual[axis];
			result.virtualAdjustableMax[axis] = size * result.scaleRealToVirtual[axis];
		}
		result.subScreenLeft = (width - virtualWidth * result.scaleVirtualToReal[0]) * 0.5f;
		return result;
	}

	void CheckResolution(float width, float height)
	{
		auto placement = MakePlacement(width, height);
		const auto original = placement;
		Fit(placement);
		const float scaleX = placement.scaleVirtualToReal[0];
		const float scaleY = placement.scaleVirtualToReal[1];
		Require(Near(scaleX, scaleY), "square-pixel displays keep uniform text and icon proportions");
		Require(Near(scaleX * placement.scaleRealToVirtual[0], 1.0f)
			&& Near(scaleY * placement.scaleRealToVirtual[1], 1.0f), "inverse coordinate transforms remain consistent");

		float x = (virtualWidth - contentWidth) * 0.5f, y = 0.0f, w = contentWidth, h = virtualHeight;
		ApplyRect(placement, x, y, w, h, 0, 0);
		Require(x >= -0.002f && y >= -0.002f && x + w <= width + 0.002f && y + h <= height + 0.002f,
			"the complete authored menu and HUD canvas fits on screen");
		Require(Near(x + w * 0.5f, width * 0.5f) && Near(y + h * 0.5f, height * 0.5f), "the fitted canvas is centered");
		Require(Near(ApplyAxis(placement, 0.0f, 2, 0), width * 0.5f)
			&& Near(ApplyAxis(placement, 0.0f, 2, 1), height * 0.5f), "centered crosshairs and prompts stay centered");

		for (int axis = 0; axis < 2; ++axis)
		{
			Require(placement.realViewableMin[axis] >= 0.0f
				&& placement.realViewableMax[axis] <= placement.realViewportSize[axis], "safe-area anchors remain on screen");
			Require(Near(placement.virtualViewableMin[axis] * placement.scaleVirtualToReal[axis], placement.realViewableMin[axis])
				&& Near(placement.virtualAdjustableMax[axis] * placement.scaleVirtualToReal[axis], placement.realAdjustableMax[axis]),
				"safe-area queries agree with rendered coordinates");
		}

		// Fullscreen cursor coordinates are converted to physical pixels before
		// hit testing. The button and text use the same placed rectangle.
		for (int horz = 0; horz <= 10; ++horz)
		{
			for (int vert = 0; vert <= 10; ++vert)
			{
				x = 12.0f; y = 18.0f; w = 80.0f; h = 24.0f;
				ApplyRect(placement, x, y, w, h, horz, vert);
				Require(Near(x, ApplyAxis(placement, 12.0f, horz, 0)) && Near(y, ApplyAxis(placement, 18.0f, vert, 1)),
					"rectangle, text and scripted HUD origins agree for every alignment");
				const float cursorX = (x + w * 0.5f) / placement.scaleVirtualToFull[0];
				const float cursorY = (y + h * 0.5f) / placement.scaleVirtualToFull[1];
				Require(Near(ApplyAxis(placement, cursorX, 4, 0), x + w * 0.5f)
					&& Near(ApplyAxis(placement, cursorY, 4, 1), y + h * 0.5f), "mouse hitboxes match the visible buttons");
			}
		}

		x = y = 0.0f; w = virtualWidth; h = virtualHeight;
		ApplyRect(placement, x, y, w, h, 4, 4);
		Require(Near(x, 0.0f) && Near(y, 0.0f) && Near(w, width) && Near(h, height), "backgrounds and fades still fill the screen");
		Require(std::memcmp(placement.scaleVirtualToFull, original.scaleVirtualToFull, sizeof(placement.scaleVirtualToFull)) == 0,
			"fullscreen cursor mapping is preserved");
		if (width / height >= 16.0f / 9.0f)
		{
			Require(std::memcmp(&placement, &original, sizeof(placement)) == 0, "16:9 and ultrawide placements retain native behavior");
		}
	}

#ifdef _WIN32
	// Optional differential check against the supported executable's actual
	// ScrPlace functions. Map its sections without running the executable.
	void CheckNative(const char* path)
	{
		std::ifstream input(path, std::ios::binary);
		Require(input.good(), "cannot open reference game executable");
		const std::vector<char> bytes((std::istreambuf_iterator<char>(input)), {});
		Require(bytes.size() > sizeof(IMAGE_DOS_HEADER), "reference executable is too short");
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
		Require(dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0
			&& static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS64) <= bytes.size(), "invalid reference DOS header");
		const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data() + dos->e_lfanew);
		Require(pe->Signature == IMAGE_NT_SIGNATURE && pe->FileHeader.Machine == IMAGE_FILE_MACHINE_AMD64
			&& pe->OptionalHeader.SizeOfImage > 0xC5CF44, "reference must be the x64 game");
		auto* mapped = static_cast<char*>(VirtualAlloc(nullptr, pe->OptionalHeader.SizeOfImage, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
		Require(mapped != nullptr, "cannot map reference game");
		struct Release { char* memory; ~Release() { VirtualFree(memory, 0, MEM_RELEASE); } } release{mapped};
		const auto* section = IMAGE_FIRST_SECTION(pe);
		for (unsigned int i = 0; i < pe->FileHeader.NumberOfSections; ++i)
		{
			Require(static_cast<std::size_t>(section[i].PointerToRawData) + section[i].SizeOfRawData <= bytes.size()
				&& static_cast<std::size_t>(section[i].VirtualAddress) + section[i].SizeOfRawData <= pe->OptionalHeader.SizeOfImage,
				"invalid reference section");
			std::memcpy(mapped + section[i].VirtualAddress, bytes.data() + section[i].PointerToRawData, section[i].SizeOfRawData);
		}
		constexpr unsigned char rectEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x63, 0x44, 0x24, 0x30 };
		constexpr unsigned char axisEntry[] = { 0x41, 0xFF, 0xC8, 0x0F, 0x28, 0xD1, 0x41, 0x83, 0xF8, 0x09 };
		Require(std::memcmp(mapped + 0xF24E0, rectEntry, sizeof(rectEntry)) == 0
			&& std::memcmp(mapped + 0xF27B0, axisEntry, sizeof(axisEntry)) == 0
			&& std::memcmp(mapped + 0xF28A0, axisEntry, sizeof(axisEntry)) == 0, "reference ScrPlace functions do not match");
		FlushInstructionCache(GetCurrentProcess(), mapped, pe->OptionalHeader.SizeOfImage);
		using Rect = void(*)(const Placement*, float*, float*, float*, float*, int, int);
		using Axis = float(*)(const Placement*, float, int);
		const auto rect = reinterpret_cast<Rect>(mapped + 0xF24E0);
		const Axis axes[] = { reinterpret_cast<Axis>(mapped + 0xF27B0), reinterpret_cast<Axis>(mapped + 0xF28A0) };
		using Setup = void(*)(Placement*, float, float, float, float, int, int);
		const auto setup = reinterpret_cast<Setup>(mapped + 0xF2D30);
		struct FloatDvar { char header[16]{}; float value; } safe{{}, 0.85f}, adjustable{{}, 1.0f};
		*reinterpret_cast<FloatDvar**>(mapped + 0x6CDD40) = &safe;
		*reinterpret_cast<FloatDvar**>(mapped + 0x6CDD48) = &safe;
		*reinterpret_cast<FloatDvar**>(mapped + 0x6CDD50) = &adjustable;
		*reinterpret_cast<FloatDvar**>(mapped + 0x6CDD58) = &adjustable;
		*reinterpret_cast<float*>(mapped + 0xC5CF40) = 1.0f;
		for (const auto size : { std::array{1280.0f, 800.0f}, std::array{1920.0f, 1080.0f}, std::array{3440.0f, 1440.0f} })
		{
			Placement placement{};
			setup(&placement, 0.0f, 0.0f, size[0], size[1], static_cast<int>(size[0]), static_cast<int>(size[1]));
			const auto expected = MakePlacement(size[0], size[1]);
			const auto* actualFields = reinterpret_cast<const float*>(&placement);
			const auto* expectedFields = reinterpret_cast<const float*>(&expected);
			for (std::size_t field = 0; field < sizeof(Placement) / sizeof(float); ++field)
			{
				Require(Near(actualFields[field], expectedFields[field]), "test placement layout differs from native viewport setup");
			}
			for (bool fitted : {false, true})
			{
				if (fitted) Fit(placement);
				for (int horz = -1; horz <= 12; ++horz)
				{
					for (int vert = -1; vert <= 12; ++vert)
					{
						float x = -106.667f, y = 64.0f, w = 854.0f, h = -48.0f;
						float nx = x, ny = y, nw = w, nh = h;
						rect(&placement, &nx, &ny, &nw, &nh, horz, vert);
						const float top = (vert < 1 || vert > 10) ? std::max(0.0f, (size[1] - virtualHeight * placement.scaleVirtualToReal[1]) * 0.5f) : 0.0f;
						Require(Near(ApplyAxis(placement, x, horz, 0), axes[0](&placement, x, horz))
							&& Near(ApplyAxis(placement, y, vert, 1), axes[1](&placement, y, vert) + top), "axis alignment differs from native ScrPlace");
						ApplyRect(placement, x, y, w, h, horz, vert);
						Require(Near(x, nx) && Near(y, ny + top) && Near(w, nw) && Near(h, nh), "rectangle alignment differs from native ScrPlace");
					}
				}
			}
		}
		for (float aspect : {0.9f, 1.0f, 1.2f})
		{
			*reinterpret_cast<float*>(mapped + 0xC5CF40) = aspect;
			Placement placement{};
			setup(&placement, 0.0f, 0.0f, 1280.0f, 800.0f, 1280, 800);
			Fit(placement, aspect);
			Require(Near(placement.scaleVirtualToReal[0] * aspect, placement.scaleVirtualToReal[1]), "anamorphic displays preserve their pixel aspect");
			float x = (virtualWidth - contentWidth) * 0.5f, y = 0.0f, w = contentWidth, h = virtualHeight;
			ApplyRect(placement, x, y, w, h, 0, 0);
			Require(x >= -0.002f && y >= -0.002f && x + w <= 1280.002f && y + h <= 800.002f, "non-square pixels still fit the authored canvas");
		}
		std::cout << "Native ScrPlace differential checks passed\n";
	}
#endif
}

int main(int argc, char** argv)
{
	try
	{
		for (const auto size : { std::array{640.0f, 480.0f}, std::array{800.0f, 600.0f}, std::array{1024.0f, 768.0f},
			std::array{1280.0f, 720.0f}, std::array{1280.0f, 800.0f}, std::array{1280.0f, 1024.0f}, std::array{1920.0f, 1080.0f},
			std::array{1920.0f, 1200.0f}, std::array{2560.0f, 1440.0f}, std::array{3440.0f, 1440.0f}, std::array{3840.0f, 2160.0f},
			std::array{5120.0f, 1440.0f}, std::array{800.0f, 1280.0f} })
		{
			CheckResolution(size[0], size[1]);
		}
		auto deck = MakePlacement(1280.0f, 800.0f);
		Fit(deck);
		Require(Near(deck.scaleVirtualToReal[0], 1.5f) && Near(ApplyAxis(deck, 0.0f, 0, 1), 40.0f), "Steam Deck fits a centered 1280x720 content canvas");
		Require(!Fit(deck), "fitting an already fitted placement cannot shrink it again");
		for (float safe : {0.0f, 0.5f, 0.85f, 1.0f})
		{
			auto placement = MakePlacement(1280.0f, 800.0f, safe);
			const auto before = placement;
			Fit(placement);
			Require(Near(placement.realViewableMin[0], before.realViewableMin[0])
				&& Near(placement.realViewableMin[1], 40.0f + before.realViewableMin[1] * 0.9f), "user safe-area margins scale inside the content canvas");
		}
		Placement invalid{};
		Require(!Fit(invalid), "uninitialized renderer placement is ignored");
		invalid = MakePlacement(1280.0f, 800.0f);
		invalid.realViewportSize[0] = std::numeric_limits<float>::quiet_NaN();
		Require(!Fit(invalid), "invalid renderer dimensions are ignored");
		// Fresh native setup on every resize must restore, rather than compound,
		// the fit when switching between a handheld and a desktop display.
		for (int iteration = 0; iteration < 3; ++iteration)
		{
			CheckResolution(1280.0f, 800.0f);
			CheckResolution(1920.0f, 1080.0f);
		}
#ifdef _WIN32
		if (argc > 1) CheckNative(argv[1]);
#else
		(void)argc;
		(void)argv;
#endif
		std::cout << "UI scaling tests passed (13 resolutions, all alignments, safe areas and resize transitions)\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
