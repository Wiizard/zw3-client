#pragma once

#include "Controller/Types.hpp"

#include "Controller/Clock.hpp"
#include "Controller/Context.hpp"
#include "Controller/Diagnostic.hpp"
#include "Controller/Calibration/Store.hpp"
#include "Controller/Device/Discovery.hpp"
#include "Controller/Device/Id.hpp"
#include "Controller/Device/Registry.hpp"
#include "Controller/Driver/Set.hpp"
#include "Controller/Engine/Bind.hpp"
#include "Controller/Engine/Dvar.hpp"
#include "Controller/Engine/Feedback.hpp"
#include "Controller/Engine/Key.hpp"
#include "Controller/Engine/View.hpp"
#include "Controller/Sample/Frame.hpp"
#include "Controller/Transport/XInputModule.hpp"

namespace Controller
{
	class Runtime
	{
	public:
		Runtime();
		~Runtime();

		Runtime(const Runtime&) = delete;
		Runtime& operator=(const Runtime&) = delete;
		Runtime(Runtime&&) = delete;
		Runtime& operator=(Runtime&&) = delete;

		const Context& GetContext() const noexcept
		{
			return this->context;
		}

		void EngineReady();
		void Frame();

		DeviceId Active() const noexcept
		{
			return this->active;
		}

		const InputFrame& Latest() const noexcept
		{
			return this->latest;
		}

		Engine::KeyDispatcher& Keys() noexcept
		{
			return this->keys;
		}

		Engine::BindBridge& Binds() noexcept
		{
			return this->binds;
		}

		Engine::ViewDriver& View() noexcept
		{
			return this->view;
		}

		const Engine::Dvars& GetDvars() const noexcept
		{
			return this->dvars;
		}

		bool IsDriving() const noexcept
		{
			return this->active != noDevice && this->keys.IsInUse() && Engine::Read(this->dvars.enabled, true);
		}

		bool SupportsHaptics() const noexcept
		{
			return this->active != noDevice && this->latest.state.caps.Has(Capability::Haptics);
		}

		bool TrySubmit(const Driver::OutputRequest& request);

		void StopHaptic(std::uint32_t tag);

		std::string ActiveDiagnostics() const;

		std::size_t DeviceCount() const noexcept
		{
			return this->drivers.Size();
		}

	private:
		bool TryAdvance(Driver::Driver& source, const DeviceConnection& connection, InputFrame& out);

		const Calibration::Profile& ProfileFor(Controller::Family family);

		void ApplyOutputPolicy();
		void ApplyLightBar();
		void ApplyTriggerFeedback();

		void ForgetDevice();

		LoggingSink sink;
		Context context;

		Transport::XInputModule xinput;
		Registry devices;
		Discovery discovery;
		Driver::DriverSet drivers;
		Calibration::Store calibrationStore;

		Engine::Dvars& dvars;
		Engine::KeyDispatcher keys;
		Engine::BindBridge binds;
		Engine::ViewDriver view;

		bool isEngineReady = false;

		std::array<std::optional<Calibration::Profile>, familyCount> profiles;

		DeviceId active{};
		InputFrame latest{};

		std::uint64_t sequence = 0;
		std::uint64_t lastPublished = 0;

		bool hadDevice = false;

		DeviceId litDevice{};
		std::uint32_t litColour = 0;

		DeviceId feltDevice{};
		Driver::AdaptiveTriggerRequest feltLeft{};
		Driver::AdaptiveTriggerRequest feltRight{};
	};
}
