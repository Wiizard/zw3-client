#include "STDInclude.hpp"

#include "Controller/Runtime.hpp"
#include "Controller/Calibration/Normalize.hpp"
#include "Controller/Engine/Command.hpp"
#include "Controller/Engine/Engine.hpp"

namespace Controller
{
	static std::filesystem::path CalibrationDirectory()
	{
		std::error_code error;
		const std::filesystem::path here = std::filesystem::current_path(error);

		if (error)
		{
			return std::filesystem::path() / "players" / "controller";
		}

		return here / "players" / "controller";
	}

	static bool IsSameTriggerRequest(const Driver::AdaptiveTriggerRequest& left, const Driver::AdaptiveTriggerRequest& right) noexcept
	{
		return left.effect == right.effect
			&& left.zones == right.zones
			&& left.startPosition == right.startPosition
			&& left.endPosition == right.endPosition
			&& left.strength == right.strength;
	}

	static float EngageFor(const Driver::AdaptiveTriggerRequest& request) noexcept
	{
		if (request.effect != Driver::TriggerEffect::Weapon)
		{
			return 0.0f;
		}

		return static_cast<float>(request.endPosition) / static_cast<float>(Driver::triggerZoneCount);
	}

	Runtime::Runtime()
		: context(sink),
		xinput(context),
		devices(context),
		discovery(context, devices, xinput),
		drivers(context, xinput),
		calibrationStore(context, CalibrationDirectory()),
		dvars(Engine::RegisteredDvars()),
		keys(context, dvars),
		binds(context, dvars),
		view(context, dvars)
	{
		this->context.Report(Severity::Info, Facility::Runtime, ErrorCode::None, "controller runtime initialized");
	}

	Runtime::~Runtime() = default;

	void Runtime::EngineReady()
	{
		if (this->isEngineReady)
		{
			return;
		}

		Engine::RegisterCommands(this->context, *this);

		this->binds.ApplyStartupLayout();

		this->isEngineReady = true;
	}

	const Calibration::Profile& Runtime::ProfileFor(Controller::Family family)
	{
		auto& slot = this->profiles[static_cast<std::size_t>(family)];

		if (slot)
		{
			return *slot;
		}

		auto stored = this->calibrationStore.TryLoad(family, std::nullopt);

		if (stored)
		{
			slot = std::move(*stored);
		}
		else
		{
			slot = Calibration::DefaultProfile(family);
		}

		return *slot;
	}

	bool Runtime::TryAdvance(Driver::Driver& source, const DeviceConnection& connection, InputFrame& out)
	{
		RawSample raw;
		CanonicalSample canonical;

		const Timestamp acquired = Clock::Now();

		if (!source.TryPoll(raw, canonical))
		{
			return false;
		}

		Calibration::ApplyProfile(this->ProfileFor(connection.identity.family), raw, canonical);

		++this->sequence;

		out = InputFrame{ connection.id, connection.identity.family, connection.link, this->sequence, LatencySpan{ acquired, Timestamp{} }, canonical };
		return true;
	}

	bool Runtime::TrySubmit(const Driver::OutputRequest& request)
	{
		if (this->active == noDevice)
		{
			return false;
		}

		this->drivers.Submit(this->active, request);
		return true;
	}

	void Runtime::StopHaptic(std::uint32_t tag)
	{
		if (this->active != noDevice)
		{
			this->drivers.StopHaptic(this->active, tag);
		}
	}

	std::string Runtime::ActiveDiagnostics() const
	{
		if (this->active == noDevice)
		{
			return {};
		}

		return this->drivers.Diagnostics(this->active);
	}

	void Runtime::ApplyOutputPolicy()
	{
		Driver::OutputPolicy policy;

		policy.isRumbleEnabled = Engine::Read(this->dvars.enabled, true) && Engine::Read(this->dvars.rumble, true);
		policy.hapticMode = Driver::HapticMode::Emulated;

		if (Engine::Read(this->dvars.haptics, true))
		{
			policy.hapticMode = Driver::HapticMode::Waveform;
		}

		policy.outputIntervalMs = static_cast<unsigned int>(std::max(0, Engine::Read(this->dvars.outputInterval, 4)));

		this->drivers.Configure(policy);
	}

	void Runtime::ApplyLightBar()
	{
		const bool isPlayStation = this->latest.family == Family::DualShock4 || this->latest.family == Family::DualSense || this->latest.family == Family::DualSenseEdge;

		if (!isPlayStation || !this->latest.state.caps.Has(Capability::LightBar))
		{
			return;
		}

		if (!Engine::Read(this->dvars.lightBar, true))
		{
			this->litDevice = noDevice;
			this->litColour = 0;
			return;
		}

		const float brightness = std::clamp(Engine::Read(this->dvars.lightBarBrightness, 1.0f), 0.0f, 1.0f);

		const auto dim = [brightness](int value)
		{
			const auto lit = static_cast<int>(static_cast<float>(value) * brightness + 0.5f);
			return static_cast<std::uint8_t>(std::clamp(lit, 0, 255));
		};

		const std::uint8_t red = dim(Engine::Read(this->dvars.lightBarRed, 196));
		const std::uint8_t green = dim(Engine::Read(this->dvars.lightBarGreen, 151));
		const std::uint8_t blue = dim(Engine::Read(this->dvars.lightBarBlue, 54));

		const std::uint32_t colour = static_cast<std::uint32_t>(red) << 16 | static_cast<std::uint32_t>(green) << 8 | blue;

		if (this->active == this->litDevice && colour == this->litColour)
		{
			return;
		}

		this->drivers.Submit(this->active, Driver::LightBarRequest{ red, green, blue });

		this->litDevice = this->active;
		this->litColour = colour;
	}

	void Runtime::ApplyTriggerFeedback()
	{
		if (!this->latest.state.caps.Has(Capability::AdaptiveTriggers))
		{
			this->keys.SetTriggerEngage(0.0f, 0.0f);
			return;
		}

		Driver::AdaptiveTriggerRequest left{};
		Driver::AdaptiveTriggerRequest right{};

		if (!Engine::TryEvaluateTriggerFeedback(this->dvars, Engine::localClient, left, right))
		{
			this->keys.SetTriggerEngage(0.0f, 0.0f);
			this->feltDevice = noDevice;
			return;
		}

		this->keys.SetTriggerEngage(EngageFor(left), EngageFor(right));

		const bool isKnown = this->active == this->feltDevice;

		if (!isKnown || !IsSameTriggerRequest(left, this->feltLeft))
		{
			this->drivers.Submit(this->active, left);
			this->feltLeft = left;
		}

		if (!isKnown || !IsSameTriggerRequest(right, this->feltRight))
		{
			this->drivers.Submit(this->active, right);
			this->feltRight = right;
		}

		this->feltDevice = this->active;
	}

	void Runtime::ForgetDevice()
	{
		this->keys.ReleaseAll();
		this->view.Idle();
		this->active = noDevice;
		this->hadDevice = false;

		this->litDevice = noDevice;
		this->litColour = 0;
		this->feltDevice = noDevice;
	}

	void Runtime::Frame()
	{
		if (!this->isEngineReady)
		{
			return;
		}

		this->binds.PollConfiguredLayout();

		const bool isEnabled = Engine::Read(this->dvars.enabled, true);

		if (isEnabled)
		{
			this->discovery.Scan();
			this->drivers.Reconcile(this->devices);
		}

		Engine::PublishPresent(this->dvars, this->drivers.Size() != 0);

		this->ApplyOutputPolicy();

		const DeviceConnection* selected = nullptr;

		this->drivers.ForEach([this, &selected](Driver::Driver&, const DeviceConnection& connection)
		{
			if (selected != nullptr && selected->id == this->active)
			{
				return;
			}

			const bool isHidPreferred = connection.transport == TransportKind::Hid && selected != nullptr && selected->transport != TransportKind::Hid;

			if (connection.id == this->active || selected == nullptr || isHidPreferred)
			{
				selected = &connection;
			}
		});

		if (!isEnabled || selected == nullptr || selected->id != this->active)
		{
			if (this->hadDevice)
			{
				this->ForgetDevice();
			}

			if (!isEnabled || selected == nullptr)
			{
				return;
			}
		}

		InputFrame candidate;
		bool hasCandidate = false;

		this->drivers.ForEach([this, selected, &candidate, &hasCandidate](Driver::Driver& source, const DeviceConnection& connection)
		{
			InputFrame polled;

			if (!this->TryAdvance(source, connection, polled))
			{
				return;
			}

			if (connection.id == selected->id)
			{
				candidate = std::move(polled);
				hasCandidate = true;
			}
		});

		if (!hasCandidate)
		{
			if (this->hadDevice)
			{
				this->keys.Tick();
			}

			return;
		}

		this->latest = std::move(candidate);
		this->latest.timing.consumed = Clock::Now();

		assert(this->latest.sequence > this->lastPublished);
		this->lastPublished = this->latest.sequence;

		this->active = this->latest.device;
		this->hadDevice = true;

		this->ApplyTriggerFeedback();

		this->keys.Dispatch(this->latest.state);

		this->view.Observe(this->latest.state);

		this->ApplyLightBar();
	}
}
