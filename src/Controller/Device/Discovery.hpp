#pragma once

#include "Controller/Types.hpp"

#include "Controller/Clock.hpp"
#include "Controller/Context.hpp"
#include "Controller/Device/Registry.hpp"
#include "Controller/Transport/DeviceNotify.hpp"
#include "Controller/Transport/XInputModule.hpp"

namespace Controller
{
	class Discovery
	{
	public:
		Discovery(const Context& context, Registry& registry, const Transport::XInputModule& xinput);

		Discovery(const Discovery&) = delete;
		Discovery& operator=(const Discovery&) = delete;

		void Scan();

	private:
		void ScanNow();
		void Run(const std::stop_token& stop);

		void ScanXInput(std::vector<TransportBinding>& seen);
		void ScanHid(std::vector<TransportBinding>& seen);
		void RetireUnseen(const std::vector<TransportBinding>& seen);

		const Context& context;
		Registry& registry;
		const Transport::XInputModule& xinput;

		Transport::DeviceNotifier notifier;

		bool hasScanned = false;
		Timestamp lastScan{};

		std::vector<std::wstring> unbound;

		std::atomic<bool> isPending{ false };

		std::jthread thread;
	};
}
