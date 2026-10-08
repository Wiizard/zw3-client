#include "STDInclude.hpp"

#include "Controller/Device/Registry.hpp"

namespace Controller
{
	Registry::Registry(const Context& context) : context(context)
	{
	}

	DeviceId Registry::Add(DeviceConnection connection)
	{
		DeviceId id;

		{
			std::scoped_lock lock(this->mutex);

			for (auto& device : this->devices)
			{
				if (!IsSameBinding(device.binding, connection.binding))
				{
					continue;
				}

				device.identity = connection.identity;
				device.transport = connection.transport;
				device.link = connection.link;
				device.caps = connection.caps;
				return device.id;
			}

			id = DeviceId(this->nextId++);
			connection.id = id;
			this->devices.push_back(connection);

			this->generation.fetch_add(1);
		}

		this->context.Report(Severity::Info, Facility::Discovery, ErrorCode::None, id,
			std::format("device connected: {} over {}/{}", ToString(connection.identity.family), ToString(connection.transport), ToString(connection.link)));

		return id;
	}

	bool Registry::Remove(DeviceId id)
	{
		Family family = Family::Unknown;

		{
			std::scoped_lock lock(this->mutex);

			const auto device = std::find_if(this->devices.begin(), this->devices.end(), [id](const DeviceConnection& connection)
			{
				return connection.id == id;
			});

			if (device == this->devices.end())
			{
				return false;
			}

			family = device->identity.family;
			this->devices.erase(device);

			this->generation.fetch_add(1);
		}

		this->context.Report(Severity::Info, Facility::Discovery, ErrorCode::None, id, std::format("device disconnected: {}", ToString(family)));
		return true;
	}

	void Registry::ForEach(const std::function<void(const DeviceConnection&)>& visit) const
	{
		std::scoped_lock lock(this->mutex);

		for (const auto& device : this->devices)
		{
			visit(device);
		}
	}
}
