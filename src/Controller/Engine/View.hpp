#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Aim/Assist.hpp"
#include "Controller/Aim/Calibration.hpp"
#include "Controller/Engine/Dvar.hpp"
#include "Controller/Mapping/StickLayout.hpp"
#include "Controller/Sample/Sample.hpp"

namespace Controller::Engine
{
	class ViewDriver
	{
	public:
		ViewDriver(const Context& context, const Dvars& dvars);

		void Observe(const CanonicalSample& sample);
		void Idle() noexcept;

		void ApplyMove(int client, Game::usercmd_s& cmd, float frameTime);
		void ApplyRemoteMove(int client, Game::usercmd_s& cmd);
		void ApplyLocationSelection(int client);

	private:
		bool TryEnsureProcessor();
		bool IsViewActive(int client) const;
		void ApplyLockOn(const Game::AimInput& input, Aim::AimFrameOutput& output);

		const Context& context;
		const Dvars& dvars;

		Mapping::ResolvedAxes axes{};

		std::optional<Aim::AimCalibration> calibration;
		std::optional<Aim::AimProcessor> processor;

		std::size_t tuningSignature = 0;
		bool hasSignature = false;
		bool hasReportedInvalid = false;

		Game::dvar_t* cursorSpeed = nullptr;
	};
}
