#include "STDInclude.hpp"

#include "Controller/Engine/Engine.hpp"

namespace Controller::Engine
{
	static constexpr std::uintptr_t cl_snap_ps = 0x1406CEE48;
	static constexpr std::uintptr_t cl_snap_ping = 0x1406D1F78;
	static constexpr std::uintptr_t cl_cgameMaxPitchSpeed = 0x1406D2030;
	static constexpr std::uintptr_t cl_cgameMaxYawSpeed = 0x1406D2034;
	static constexpr std::uintptr_t cl_viewangles = 0x1406D2078;
	static constexpr std::uintptr_t cls_frametime = 0x140C5CEB4;

	static constexpr std::uintptr_t cg_frametime = 0x1404E111C;
	static constexpr std::uintptr_t cg_compassMapWorldSize = 0x1404ED1B0;
	static constexpr std::uintptr_t cg_selectedLocation = 0x1404ED238;

	static constexpr std::size_t sprintButtonUpRequiredOffset = 0x1B8;

	static const Game::playerState_s& SnapPlayerState() noexcept
	{
		return *reinterpret_cast<const Game::playerState_s*>(Utils::Hook::Rebase(cl_snap_ps));
	}

	static float* ViewAngles() noexcept
	{
		return reinterpret_cast<float*>(Utils::Hook::Rebase(cl_viewangles));
	}

	float& ViewPitch([[maybe_unused]] int client) noexcept
	{
		return ViewAngles()[0];
	}

	float& ViewYaw([[maybe_unused]] int client) noexcept
	{
		return ViewAngles()[1];
	}

	float MaxPitchSpeed([[maybe_unused]] int client) noexcept
	{
		return Utils::Hook::Get<float>(cl_cgameMaxPitchSpeed);
	}

	float MaxYawSpeed([[maybe_unused]] int client) noexcept
	{
		return Utils::Hook::Get<float>(cl_cgameMaxYawSpeed);
	}

	int PmFlags([[maybe_unused]] int client) noexcept
	{
		return SnapPlayerState().pm_flags;
	}

	int LastWeaponHand([[maybe_unused]] int client) noexcept
	{
		return SnapPlayerState().weapCommon.lastWeaponHand;
	}

	int SnapPing([[maybe_unused]] int client) noexcept
	{
		return Utils::Hook::Get<int>(cl_snap_ping);
	}

	float ClientFrameTime() noexcept
	{
		return static_cast<float>(Utils::Hook::Get<int>(cls_frametime)) * 0.001f;
	}

	bool IsSprintButtonUpRequired(int client) noexcept
	{
		const auto* cg = Game::CL_GetLocalClientGlobals(client);

		if (cg == nullptr)
		{
			return false;
		}

		const auto* playerState = reinterpret_cast<const std::uint8_t*>(&cg->predictedPlayerState);
		return *reinterpret_cast<const int*>(playerState + sprintButtonUpRequiredOffset) != 0;
	}

	LocationSelection SelectedLocation([[maybe_unused]] int client) noexcept
	{
		auto* const location = reinterpret_cast<float*>(Utils::Hook::Rebase(cg_selectedLocation));

		LocationSelection selection{};
		selection.frameTime = static_cast<float>(Utils::Hook::Get<int>(cg_frametime)) * 0.001f;
		selection.mapWorldSize = reinterpret_cast<const float*>(Utils::Hook::Rebase(cg_compassMapWorldSize));
		selection.location = location;
		selection.angle = &location[2];
		selection.angleLocation = &location[3];
		return selection;
	}

	void AimAssistBegin(const Game::AimInput& input, Game::AimOutput& output)
	{
		Game::AimAssist_ApplyDeltas(&input, &output);
	}
}
