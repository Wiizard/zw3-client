#include "STDInclude.hpp"

#include "Controller/Driver/Set.hpp"
#include "Controller/Driver/DualSense.hpp"
#include "Controller/Driver/DualSenseEdge.hpp"
#include "Controller/Driver/DualShock4.hpp"
#include "Controller/Driver/PlayStation.hpp"
#include "Controller/Driver/XInput.hpp"

namespace Controller::Driver
{
	DriverSet::DriverSet(const Context& context, const Transport::XInputModule& xinput)
		: context(context),
		xinput(xinput)
	{
	}

	DriverSet::~DriverSet() = default;

	DriverSet::Entry::Entry(DeviceConnection connection, std::unique_ptr<Transport::HidDevice> opened, std::unique_ptr<Driver> bound)
		: device(std::move(connection)),
		hid(std::move(opened)),
		driver(std::move(bound))
	{
	}

	DriverSet::Entry& DriverSet::Entry::operator=(Entry&& other)
	{
		this->driver.reset();

		this->device = std::move(other.device);
		this->hid = std::move(other.hid);
		this->driver = std::move(other.driver);

		return *this;
	}

	std::unique_ptr<Driver> DriverSet::TryBind(const DeviceConnection& device, const std::unique_ptr<Transport::HidDevice>& hid)
	{
		switch (device.identity.family)
		{
		case Family::Xbox:
		{
			const auto* binding = std::get_if<XInputBinding>(&device.binding);

			if (binding == nullptr)
			{
				return nullptr;
			}

			return std::make_unique<XInputDriver>(this->context, this->xinput, device.id, binding->index);
		}

		case Family::DualShock4:
		case Family::DualSense:
		case Family::DualSenseEdge:
		{
			if (hid == nullptr)
			{
				return nullptr;
			}

			assert(hid->Link() == Connection::Usb || hid->Link() == Connection::Bluetooth);

			if (hid->Link() == Connection::Bluetooth)
			{
				TryEnableExtendedReports(this->context, *hid, device.id);
			}

			if (device.identity.family == Family::DualShock4)
			{
				return std::make_unique<DualShock4Driver>(this->context, *hid, device.id);
			}

			if (device.identity.family == Family::DualSense)
			{
				return std::make_unique<DualSenseDriver>(this->context, *hid, device.id);
			}

			return std::make_unique<DualSenseEdgeDriver>(this->context, *hid, device.id);
		}

		case Family::Unknown:
			break;
		}

		return nullptr;
	}

	void DriverSet::Reconcile(const Registry& registry)
	{
		const std::uint64_t registryGeneration = registry.Generation();

		if (this->isReconciled && registryGeneration == this->generation)
		{
			return;
		}

		this->generation = registryGeneration;
		this->isReconciled = true;

		std::vector<DeviceConnection> current;

		registry.ForEach([&current](const DeviceConnection& device)
		{
			current.push_back(device);
		});

		std::erase_if(this->entries, [this, &current](const Entry& entry)
		{
			const bool isGone = std::none_of(current.begin(), current.end(), [&entry](const DeviceConnection& device)
			{
				return device.id == entry.device.id;
			});

			if (isGone)
			{
				this->context.Report(Severity::Info, Facility::Driver, ErrorCode::None, entry.device.id, std::format("driver released: {}", ToString(entry.device.identity.family)));
			}

			return isGone;
		});

		for (auto& device : current)
		{
			const bool isBound = std::any_of(this->entries.begin(), this->entries.end(), [&device](const Entry& entry)
			{
				return entry.device.id == device.id;
			});

			if (isBound)
			{
				continue;
			}

			std::unique_ptr<Transport::HidDevice> hid;

			if (const auto* binding = std::get_if<HidBinding>(&device.binding))
			{
				hid = Transport::TryOpen(this->context, binding->path);

				if (hid == nullptr)
				{
					continue;
				}
			}

			auto bound = this->TryBind(device, hid);

			if (bound == nullptr)
			{
				this->context.Report(Severity::Warning, Facility::Driver, ErrorCode::UnsupportedDevice, device.id,
					std::format("no driver binds {} over {}", ToString(device.identity.family), ToString(device.transport)));
				continue;
			}

			this->context.Report(Severity::Info, Facility::Driver, ErrorCode::None, device.id,
				std::format("driver bound: {} over {}", ToString(device.identity.family), ToString(device.transport)));

			this->entries.push_back(Entry{ std::move(device), std::move(hid), std::move(bound) });
		}
	}

	void DriverSet::ForEach(const std::function<void(Driver&, const DeviceConnection&)>& visit)
	{
		for (auto& entry : this->entries)
		{
			visit(*entry.driver, entry.device);
		}
	}

	void DriverSet::Configure(const OutputPolicy& policy)
	{
		for (auto& entry : this->entries)
		{
			if (entry.driver != nullptr)
			{
				entry.driver->Configure(policy);
			}
		}
	}

	void DriverSet::StopHaptic(DeviceId id, std::uint32_t tag)
	{
		for (auto& entry : this->entries)
		{
			if (entry.device.id == id && entry.driver != nullptr)
			{
				entry.driver->StopHaptic(tag);
			}
		}
	}

	std::string DriverSet::Diagnostics(DeviceId id) const
	{
		for (const auto& entry : this->entries)
		{
			if (entry.device.id == id && entry.driver != nullptr)
			{
				return entry.driver->Diagnostics();
			}
		}

		return {};
	}

	void DriverSet::Submit(DeviceId id, const OutputRequest& request)
	{
		for (auto& entry : this->entries)
		{
			if (entry.device.id == id)
			{
				entry.driver->Submit(request);
				return;
			}
		}
	}
}
