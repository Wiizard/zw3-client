#include <natupnp.h>

#include "UPnP.hpp"
#include "Events.hpp"
#include "Network.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "Dedicated.hpp"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace Components
{
	Dvar::Var UPnP::NetUPnP;
	std::thread UPnP::MappingThread;
	std::atomic_bool UPnP::MappingActive{ false };
	std::atomic_bool UPnP::MappingInProgress{ false };
	std::atomic_bool UPnP::LobbyMappingTriggered{ false };
	std::uint16_t UPnP::MappedPort = 0;

	namespace
	{
		class BStr
		{
		public:
			explicit BStr(const wchar_t* value) : value_(SysAllocString(value)) {}
			~BStr()
			{
				if (this->value_)
				{
					SysFreeString(this->value_);
				}
			}

			BStr(const BStr&) = delete;
			BStr& operator=(const BStr&) = delete;

			operator BSTR() const
			{
				return this->value_;
			}

		private:
			BSTR value_;
		};

		bool InitializeCOM(bool& shouldUninitialize)
		{
			shouldUninitialize = false;

			const auto result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			if (SUCCEEDED(result))
			{
				shouldUninitialize = true;
				return true;
			}

			if (result == RPC_E_CHANGED_MODE)
			{
				return true;
			}

			Logger::Debug("UPnP: CoInitializeEx failed: {:#x}", static_cast<unsigned int>(result));
			return false;
		}

		std::wstring GetLocalAddressString()
		{
			for (int i = 0; i < *Game::numIP; ++i)
			{
				const auto& ip = Game::localIP[i];
				if (ip.bytes[0] == 127 || ip.full == 0)
				{
					continue;
				}

				return std::format(L"{}.{}.{}.{}", ip.bytes[0], ip.bytes[1], ip.bytes[2], ip.bytes[3]);
			}

			return L"";
		}

		IStaticPortMappingCollection* GetPortMappingCollection(IUPnPNAT** nat)
		{
			*nat = nullptr;

			auto result = CoCreateInstance(CLSID_UPnPNAT, nullptr, CLSCTX_INPROC_SERVER, IID_IUPnPNAT, reinterpret_cast<void**>(nat));
			if (FAILED(result) || !*nat)
			{
				Logger::Debug("UPnP: CoCreateInstance(CLSID_UPnPNAT) failed: {:#x}", static_cast<unsigned int>(result));
				return nullptr;
			}

			IStaticPortMappingCollection* mappings = nullptr;
			result = (*nat)->get_StaticPortMappingCollection(&mappings);
			if (FAILED(result) || !mappings)
			{
				Logger::Print("UPnP: no Internet Gateway Device with port mapping support was found. Make sure UPnP is enabled on your router and Windows network discovery services are running.\n");
				return nullptr;
			}

			return mappings;
		}
	}

	void UPnP::StartMapping()
	{
		if (!NetUPnP.get<bool>() || MappingActive || MappingInProgress)
		{
			return;
		}

		if (MappingThread.joinable())
		{
			MappingThread.join();
		}

		MappingInProgress = true;
		MappingThread = std::thread([]
			{
				MapPort(Network::GetPort());
				MappingInProgress = false;
			});
	}

	bool UPnP::IsPrivateLobbyOpen()
	{
		auto* menuPrivateLobby = Game::Menus_FindByName(Game::uiContext, "menu_xboxlive_privatelobby");
		if (menuPrivateLobby && Game::Menu_IsVisible(Game::uiContext, menuPrivateLobby))
		{
			return true;
		}

		auto* menuCreateServer = Game::Menus_FindByName(Game::uiContext, "createserver");
		return menuCreateServer && Game::Menu_IsVisible(Game::uiContext, menuCreateServer);
	}

	void UPnP::CheckLobbyState()
	{
		const auto* partyHost = Game::Dvar_FindVar("party_host");
		const auto isPartyHost = partyHost && Dvar::Var("party_host").get<bool>();
		const auto isPrivateLobbyOpen = IsPrivateLobbyOpen();
		if (!isPartyHost && !isPrivateLobbyOpen)
		{
			LobbyMappingTriggered = false;
			return;
		}

		StartMapping();
	}

	void UPnP::MapPort(const std::uint16_t port)
	{
		bool shouldUninitialize = false;
		if (!InitializeCOM(shouldUninitialize))
		{
			return;
		}

		IUPnPNAT* nat = nullptr;
		auto* mappings = GetPortMappingCollection(&nat);
		if (!mappings)
		{
			if (nat) nat->Release();
			if (shouldUninitialize) CoUninitialize();
			return;
		}

		const BStr protocol(L"UDP");
		const BStr description(L"ZW3 game server");
		const auto localAddress = GetLocalAddressString();
		if (localAddress.empty())
		{
			Logger::Print("UPnP: could not find a local LAN address to map UDP port {}.\n", port);
			mappings->Release();
			nat->Release();
			if (shouldUninitialize) CoUninitialize();
			return;
		}

		const BStr localClient(localAddress.c_str());
		IStaticPortMapping* mapping = nullptr;

		const auto result = mappings->Add(port, protocol, port, localClient, VARIANT_TRUE, description, &mapping);
		if (SUCCEEDED(result) && mapping)
		{
			MappedPort = port;
			MappingActive = true;
			Logger::Print("UPnP: mapped UDP port {} for incoming game connections.\n", port);
			mapping->Release();
		}
		else
		{
			Logger::Print("UPnP: failed to map UDP port {} ({:#x}). You may need to port forward manually.\n", port, static_cast<unsigned int>(result));
		}

		mappings->Release();
		nat->Release();

		if (shouldUninitialize)
		{
			CoUninitialize();
		}
	}

	void UPnP::RemoveMapping()
	{
		if (!MappingActive || !MappedPort)
		{
			return;
		}

		bool shouldUninitialize = false;
		if (!InitializeCOM(shouldUninitialize))
		{
			return;
		}

		IUPnPNAT* nat = nullptr;
		auto* mappings = GetPortMappingCollection(&nat);
		if (mappings)
		{
			const BStr protocol(L"UDP");
			const auto result = mappings->Remove(MappedPort, protocol);
			if (SUCCEEDED(result))
			{
				Logger::Print("UPnP: removed UDP port {} mapping.\n", MappedPort);
			}

			mappings->Release();
		}

		if (nat) nat->Release();
		if (shouldUninitialize) CoUninitialize();

		MappingActive = false;
		MappedPort = 0;
	}

	UPnP::UPnP()
	{
		NetUPnP = Dvar::Register<bool>("net_upnp", true, Game::DVAR_ARCHIVE, "Automatically map the net_port UDP port");

		Events::OnNetworkInit([]
			{
				StartMapping();
			});

		Events::OnSVInit(StartMapping);

		if (!Dedicated::IsEnabled())
		{
			Scheduler::Loop(CheckLobbyState, Scheduler::Pipeline::MAIN, 1s);
		}
	}

	void UPnP::preDestroy()
	{
		if (MappingThread.joinable())
		{
			MappingThread.join();
		}

		RemoveMapping();
	}
}
