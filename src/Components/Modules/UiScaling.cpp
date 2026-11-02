#include "STDInclude.hpp"

#include "UiScaling.hpp"
#include "Dedicated.hpp"
#include "Logger.hpp"
#include "ZoneBuilder.hpp"
#include "Utils/UiScaling.hpp"

namespace Components
{
	namespace
	{
		using Utils::UiScaling::Placement;
		constexpr std::uintptr_t setupViewport = 0x1400F2D30;
		constexpr std::uintptr_t pixelAspectRatio = 0x140C5CF40;
		constexpr std::uintptr_t setupCalls[] = { 0x1400F2D19, 0x1400F3007, 0x1400F3080 };
		constexpr std::uintptr_t applyRect = 0x1400F24E0;
		constexpr std::uintptr_t applyX = 0x1400F27B0;
		constexpr std::uintptr_t applyY = 0x1400F28A0;
		constexpr std::uint8_t rectEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x63, 0x44, 0x24, 0x30 };
		constexpr std::uint8_t axisEntry[] = { 0x41, 0xFF, 0xC8, 0x0F, 0x28, 0xD1, 0x41, 0x83, 0xF8, 0x09 };
		Utils::Hook hooks[std::size(setupCalls) + 3];

		void SetupViewport(Placement* placement, float x, float y, float width, float height, int fullWidth, int fullHeight)
		{
			using Setup = void(*)(Placement*, float, float, float, float, int, int);
			reinterpret_cast<Setup>(Utils::Hook::Rebase(setupViewport))(placement, x, y, width, height, fullWidth, fullHeight);
			Utils::UiScaling::Fit(*placement, Utils::Hook::Get<float>(pixelAspectRatio));
		}

		void ApplyRect(const Placement* placement, float* x, float* y, float* width, float* height, int horzAlign, int vertAlign)
		{
			Utils::UiScaling::ApplyRect(*placement, *x, *y, *width, *height, horzAlign, vertAlign);
		}

		float ApplyX(const Placement* placement, float x, int align)
		{
			return Utils::UiScaling::ApplyAxis(*placement, x, align, 0);
		}

		float ApplyY(const Placement* placement, float y, int align)
		{
			return Utils::UiScaling::ApplyAxis(*placement, y, align, 1);
		}
	}

	UiScaling::UiScaling()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		bool expected = Utils::Hook::MatchesBytes(applyRect, rectEntry, sizeof(rectEntry))
			&& Utils::Hook::MatchesBytes(applyX, axisEntry, sizeof(axisEntry))
			&& Utils::Hook::MatchesBytes(applyY, axisEntry, sizeof(axisEntry));
		for (const auto call : setupCalls)
		{
			expected = expected && Utils::Hook::BranchesTo(call, setupViewport, HOOK_CALL);
		}
		if (!expected)
		{
			Logger::Error("ui scaling: screen placement does not match; scaling hooks were not installed\n");
			return;
		}

		bool installed = true;
		for (std::size_t index = 0; index < std::size(setupCalls); ++index)
		{
			installed = hooks[index].Initialize(setupCalls[index], reinterpret_cast<void*>(SetupViewport), HOOK_CALL)->Install()->IsInstalled() && installed;
		}
		const auto first = std::size(setupCalls);
		installed = hooks[first].Initialize(applyRect, reinterpret_cast<void*>(ApplyRect), HOOK_JUMP)->Install()->IsInstalled() && installed;
		installed = hooks[first + 1].Initialize(applyX, reinterpret_cast<void*>(ApplyX), HOOK_JUMP)->Install()->IsInstalled() && installed;
		installed = hooks[first + 2].Initialize(applyY, reinterpret_cast<void*>(ApplyY), HOOK_JUMP)->Install()->IsInstalled() && installed;
		if (!installed)
		{
			for (auto& hook : hooks) hook.Uninstall();
			Logger::Error("ui scaling: could not install every screen-placement hook\n");
			return;
		}
		for (auto& hook : hooks) hook.Quick();
	}
}
