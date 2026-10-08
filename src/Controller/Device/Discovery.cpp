#include "STDInclude.hpp"

#include "Controller/Device/Discovery.hpp"
#include "Controller/Transport/Hid.hpp"

namespace Controller
{
	static constexpr Clock::Duration fallbackInterval{ std::chrono::seconds(5) };

	static constexpr std::uint8_t subtypeWheel = 0x02;
	static constexpr std::uint8_t subtypeArcadeStick = 0x03;
	static constexpr std::uint8_t subtypeFlightStick = 0x04;
	static constexpr std::uint8_t subtypeDancePad = 0x05;
	static constexpr std::uint8_t subtypeGuitar = 0x06;
	static constexpr std::uint8_t subtypeGuitarAlternate = 0x07;
	static constexpr std::uint8_t subtypeDrumKit = 0x08;
	static constexpr std::uint8_t subtypeGuitarBass = 0x0B;

	static bool IsGamepadLike(std::uint8_t subtype) noexcept
	{
		switch (subtype)
		{
		case subtypeWheel:
		case subtypeArcadeStick:
		case subtypeFlightStick:
		case subtypeDancePad:
		case subtypeGuitar:
		case subtypeGuitarAlternate:
		case subtypeGuitarBass:
		case subtypeDrumKit:
			return false;

		default:
			return true;
		}
	}

	static Capabilities CapabilitiesFor(Family family) noexcept
	{
		Capabilities caps;

		switch (family)
		{
		case Family::Xbox:
			caps.Add(Capability::Rumble);
			break;

		case Family::DualShock4:
			caps.Add(Capability::Gyroscope).Add(Capability::Accelerometer).Add(Capability::Touchpad).Add(Capability::Battery);
			caps.Add(Capability::Rumble).Add(Capability::LightBar);
			break;

		case Family::DualSense:
		case Family::DualSenseEdge:
			caps.Add(Capability::Gyroscope).Add(Capability::Accelerometer).Add(Capability::Touchpad).Add(Capability::Battery);
			caps.Add(Capability::MicrophoneButton).Add(Capability::Rumble).Add(Capability::Haptics).Add(Capability::AdaptiveTriggers);
			caps.Add(Capability::LightBar).Add(Capability::PlayerLeds);

			if (family == Family::DualSenseEdge)
			{
				caps.Add(Capability::BackButtons);
			}

			break;

		case Family::Unknown:
			break;
		}

		return caps;
	}

	Discovery::Discovery(const Context& context, Registry& registry, const Transport::XInputModule& xinput)
		: context(context),
		registry(registry),
		xinput(xinput),
		notifier(context),
		thread([this](std::stop_token stop)
		{
			this->Run(stop);
		})
	{
	}

	void Discovery::Scan()
	{
		const bool hasChanged = this->notifier.Consume();
		const Timestamp now = Clock::Now();

		const bool isFallbackDue = this->notifier.HasFailed() && now - this->lastScan >= fallbackInterval;

		if (!hasChanged && this->hasScanned && !isFallbackDue)
		{
			return;
		}

		this->lastScan = now;
		this->hasScanned = true;

		this->isPending.store(true, std::memory_order_release);
		this->isPending.notify_one();
	}

	void Discovery::Run(const std::stop_token& stop)
	{
		const std::stop_callback wake(stop, [this]() noexcept
		{
			this->isPending.store(true, std::memory_order_release);
			this->isPending.notify_one();
		});

		while (!stop.stop_requested())
		{
			this->isPending.wait(false, std::memory_order_acquire);

			if (stop.stop_requested())
			{
				return;
			}

			this->isPending.store(false, std::memory_order_relaxed);

			try
			{
				this->ScanNow();
			}
			catch (const std::exception& exception)
			{
				this->context.Report(Severity::Warning, Facility::Discovery, ErrorCode::TransportFailure, std::format("device scan failed: {}", exception.what()));
			}
		}
	}

	void Discovery::ScanNow()
	{
		std::vector<TransportBinding> seen;
		seen.reserve(UserIndex::count + 4);

		this->ScanXInput(seen);
		this->ScanHid(seen);

		this->RetireUnseen(seen);
	}

	void Discovery::ScanXInput(std::vector<TransportBinding>& seen)
	{
		if (!this->xinput.IsLoaded())
		{
			return;
		}

		for (std::uint8_t i = 0; i < UserIndex::count; ++i)
		{
			XINPUT_STATE state{};

			if (this->xinput.GetState(i, state) != ERROR_SUCCESS)
			{
				continue;
			}

			XINPUT_CAPABILITIES caps{};

			if (this->xinput.GetCapabilities(i, 0, caps) == ERROR_SUCCESS && !IsGamepadLike(caps.SubType))
			{
				continue;
			}

			const UserIndex slot(i);

			DeviceConnection connection;
			connection.identity = DeviceIdentity{ Family::Xbox, std::nullopt, std::nullopt, std::nullopt };
			connection.transport = TransportKind::XInput;
			connection.link = Connection::Unknown;
			connection.caps = CapabilitiesFor(Family::Xbox);
			connection.binding = XInputBinding{ slot };

			this->registry.Add(connection);

			seen.push_back(XInputBinding{ slot });
		}
	}

	void Discovery::ScanHid(std::vector<TransportBinding>& seen)
	{
		std::vector<std::wstring> stillUnbound;

		for (auto& entry : Transport::Enumerate(this->context))
		{
			const Family family = Classify(entry.attributes.vendor, entry.attributes.product);

			assert(family != Family::Unknown);

			if (entry.link == Connection::Unknown)
			{
				const bool isNew = std::find(this->unbound.begin(), this->unbound.end(), entry.path) == this->unbound.end();

				if (isNew)
				{
					this->context.Report(Severity::Warning, Facility::Discovery, ErrorCode::AmbiguousIdentity,
						std::format("HID device {} reports an input report of {} bytes; the drivers decode 64-byte (USB) and 78-byte (Bluetooth) framing, so no driver is bound", ToString(family), entry.inputReportLength));
				}

				stillUnbound.push_back(std::move(entry.path));
				continue;
			}

			DeviceConnection connection;
			connection.identity = DeviceIdentity{ family, entry.attributes.vendor, entry.attributes.product, entry.attributes.version };
			connection.transport = TransportKind::Hid;
			connection.link = entry.link;
			connection.caps = CapabilitiesFor(family);
			connection.binding = HidBinding{ entry.path };

			this->registry.Add(connection);

			seen.push_back(HidBinding{ std::move(entry.path) });
		}

		this->unbound = std::move(stillUnbound);
	}

	void Discovery::RetireUnseen(const std::vector<TransportBinding>& seen)
	{
		std::vector<DeviceId> departed;

		this->registry.ForEach([&seen, &departed](const DeviceConnection& device)
		{
			const bool isPresent = std::any_of(seen.begin(), seen.end(), [&device](const TransportBinding& binding)
			{
				return IsSameBinding(device.binding, binding);
			});

			if (!isPresent)
			{
				departed.push_back(device.id);
			}
		});

		for (const auto id : departed)
		{
			this->registry.Remove(id);
		}
	}
}
