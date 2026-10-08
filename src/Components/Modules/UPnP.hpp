#pragma once

#include "Dvar.hpp"

namespace Components
{
	class UPnP : public Component
	{
	public:
		UPnP();

	private:
		static Dvar::Var net_upnp;
		static Dvar::Var net_upnp_prompted;

		static bool IsPrivateLobbyOpen();
		static void CheckLobbyState();
		static void QueueStartMapping();
		static void QueueRemoveMapping();

		static void StartMapping(std::uint16_t port);
		static void MapPort(std::uint16_t port);
		static void RemoveMapping();
	};
}
