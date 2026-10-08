#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Device/Registry.hpp"
#include "Controller/Driver/Driver.hpp"
#include "Controller/Transport/Hid.hpp"
#include "Controller/Transport/XInputModule.hpp"

namespace Controller::Driver
{
	class DriverSet
	{
	public:
		DriverSet(const Context& context, const Transport::XInputModule& xinput);
		~DriverSet();

		DriverSet(const DriverSet&) = delete;
		DriverSet& operator=(const DriverSet&) = delete;

		void Reconcile(const Registry& registry);

		void ForEach(const std::function<void(Driver&, const DeviceConnection&)>& visit);

		void Submit(DeviceId id, const OutputRequest& request);

		std::size_t Size() const noexcept
		{
			return this->entries.size();
		}

		void Configure(const OutputPolicy& policy);

		void StopHaptic(DeviceId id, std::uint32_t tag);

		std::string Diagnostics(DeviceId id) const;

	private:
		struct Entry
		{
			Entry(DeviceConnection connection, std::unique_ptr<Transport::HidDevice> opened, std::unique_ptr<Driver> bound);

			Entry(Entry&& other) = default;
			Entry& operator=(Entry&& other);

			DeviceConnection device;
			std::unique_ptr<Transport::HidDevice> hid;
			std::unique_ptr<Driver> driver;
		};

		std::unique_ptr<Driver> TryBind(const DeviceConnection& device, const std::unique_ptr<Transport::HidDevice>& hid);

		const Context& context;
		const Transport::XInputModule& xinput;

		std::uint64_t generation = 0;
		bool isReconciled = false;
		std::vector<Entry> entries;
	};
}
