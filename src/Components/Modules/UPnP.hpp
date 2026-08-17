#pragma once

namespace Components
{
	class UPnP : public Component
	{
	public:
		UPnP();
		void preDestroy() override;

	private:
		static Dvar::Var NetUPnP;
		static Dvar::Var NetUPnPPrompted;
		static std::atomic_bool MappingActive;
		static std::atomic_bool MappingInProgress;
		static std::atomic_bool LobbyMappingTriggered;
		static std::uint16_t MappedPort;

		static void StartMapping();
		static void CheckLobbyState();
		static bool IsPrivateLobbyOpen();
		static void MapPort(std::uint16_t port);
		static void RemoveMapping();
	};
}
