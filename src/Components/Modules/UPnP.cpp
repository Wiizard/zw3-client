#include <natupnp.h>

#include "UPnP.hpp"
#include "Events.hpp"
#include "Network.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "Dedicated.hpp"

#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

#include <netfw.h>
#include <set>
#include <sstream>

namespace Components
{
	Dvar::Var UPnP::NetUPnP;
	Dvar::Var UPnP::NetUPnPPrompted;
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
			WSADATA wsaData{};
			const auto startedWsa = WSAStartup(MAKEWORD(2, 2), &wsaData) == 0;

			char hostName[256]{};
			if (gethostname(hostName, sizeof(hostName)) == SOCKET_ERROR)
			{
				if (startedWsa) WSACleanup();
				return L"";
			}

			addrinfo hints{};
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;

			addrinfo* result = nullptr;
			if (getaddrinfo(hostName, nullptr, &hints, &result) != 0)
			{
				if (startedWsa) WSACleanup();
				return L"";
			}

			std::wstring localAddress;

			for (auto* ptr = result; ptr; ptr = ptr->ai_next)
			{
				auto* sockaddr = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
				const auto ip = ntohl(sockaddr->sin_addr.s_addr);

				const auto b1 = (ip >> 24) & 0xFF;
				const auto b2 = (ip >> 16) & 0xFF;
				const auto b3 = (ip >> 8) & 0xFF;
				const auto b4 = ip & 0xFF;

				if (b1 == 127 || b1 == 0)
				{
					continue;
				}

				localAddress = std::format(L"{}.{}.{}.{}", b1, b2, b3, b4);
				break;
			}

			freeaddrinfo(result);

			if (startedWsa)
			{
				WSACleanup();
			}

			return localAddress;
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

		constexpr auto FirewallRuleName = L"Zombie Warfare 3 UDP Server";

		bool PortIsInList(const std::wstring& ports, const std::uint16_t port)
		{
			std::wstringstream stream(ports);
			std::wstring token;

			while (std::getline(stream, token, L','))
			{
				const auto dash = token.find(L'-');

				if (dash != std::wstring::npos)
				{
					const auto start = static_cast<std::uint16_t>(std::stoi(token.substr(0, dash)));
					const auto end = static_cast<std::uint16_t>(std::stoi(token.substr(dash + 1)));

					if (port >= start && port <= end)
					{
						return true;
					}
				}
				else if (!token.empty() && static_cast<std::uint16_t>(std::stoi(token)) == port)
				{
					return true;
				}
			}

			return false;
		}

		std::wstring GetExistingFirewallPorts()
		{
			bool shouldUninitialize = false;
			if (!InitializeCOM(shouldUninitialize))
			{
				return L"";
			}

			INetFwPolicy2* policy = nullptr;
			auto result = CoCreateInstance(__uuidof(NetFwPolicy2), nullptr, CLSCTX_INPROC_SERVER, __uuidof(INetFwPolicy2), reinterpret_cast<void**>(&policy));
			if (FAILED(result) || !policy)
			{
				if (shouldUninitialize) CoUninitialize();
				return L"";
			}

			INetFwRules* rules = nullptr;
			result = policy->get_Rules(&rules);
			if (FAILED(result) || !rules)
			{
				policy->Release();
				if (shouldUninitialize) CoUninitialize();
				return L"";
			}

			INetFwRule* rule = nullptr;
			BStr ruleName(FirewallRuleName);
			result = rules->Item(ruleName, &rule);

			std::wstring ports;

			if (SUCCEEDED(result) && rule)
			{
				long protocol = 0;
				NET_FW_RULE_DIRECTION direction{};
				BSTR localPorts = nullptr;

				rule->get_Protocol(&protocol);
				rule->get_Direction(&direction);
				rule->get_LocalPorts(&localPorts);

				if (protocol == NET_FW_IP_PROTOCOL_UDP && direction == NET_FW_RULE_DIR_IN && localPorts)
				{
					ports = localPorts;
				}

				if (localPorts)
				{
					SysFreeString(localPorts);
				}

				rule->Release();
			}

			rules->Release();
			policy->Release();

			if (shouldUninitialize)
			{
				CoUninitialize();
			}

			return ports;
		}

		bool AddFirewallRule(const std::uint16_t port)
		{
			auto existingPorts = GetExistingFirewallPorts();

			if (!existingPorts.empty() && PortIsInList(existingPorts, port))
			{
				return true;
			}

			std::wstring newPorts;

			if (!existingPorts.empty())
			{
				newPorts = existingPorts + L"," + std::to_wstring(port);
			}
			else
			{
				newPorts = std::to_wstring(port);
			}

			const auto command = std::format(
				L"/c netsh advfirewall firewall delete rule name=\"Zombie Warfare 3 UDP Server\" >nul 2>&1 & "
				L"netsh advfirewall firewall add rule name=\"Zombie Warfare 3 UDP Server\" dir=in action=allow protocol=UDP localport={}",
				newPorts
			);

			SHELLEXECUTEINFOW info{};
			info.cbSize = sizeof(info);
			info.fMask = SEE_MASK_NOCLOSEPROCESS;
			info.lpVerb = L"runas";
			info.lpFile = L"cmd.exe";
			info.lpParameters = command.c_str();
			info.nShow = SW_HIDE;

			if (!ShellExecuteExW(&info))
			{
				Logger::Print("UPnP: failed to request Windows Firewall permission. Run as administrator or add UDP port {} manually.\n", port);
				return false;
			}

			if (info.hProcess)
			{
				WaitForSingleObject(info.hProcess, 10000);
				CloseHandle(info.hProcess);
			}

			Logger::Print("UPnP: updated Windows Firewall UDP inbound rule ports: {}.\n", Utils::String::Convert(newPorts));
			return true;
		}
	}

	void UPnP::StartMapping()
	{
		if (!NetUPnP.get<bool>() || !NetUPnPPrompted.get<bool>() || MappingInProgress)
		{
			return;
		}

		const auto port = Network::GetPort();
		if (!port)
		{
			return;
		}

		if (MappingActive && MappedPort == port)
		{
			return;
		}

		if (MappingActive && MappedPort != port)
		{
			RemoveMapping();
		}

		MappingInProgress = true;

		try
		{
			AddFirewallRule(port);
			MapPort(port);
		}
		catch (const std::exception& e)
		{
			Logger::Print("UPnP: mapping failed: {}\n", e.what());
		}
		catch (...)
		{
			Logger::Print("UPnP: mapping failed with an unknown error.\n");
		}

		MappingInProgress = false;
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
		if (Dedicated::IsEnabled())
		{
			return;
		}

		const auto* partyHost = Game::Dvar_FindVar("party_host");
		const auto isPartyHost = partyHost && Dvar::Var("party_host").get<bool>();
		const auto isPrivateLobbyOpen = IsPrivateLobbyOpen();

		const auto shouldHaveMapping = isPartyHost || isPrivateLobbyOpen;

		if (!shouldHaveMapping)
		{
			if (LobbyMappingTriggered)
			{
				RemoveMapping();
				LobbyMappingTriggered = false;
			}

			return;
		}

		if (!NetUPnP.get<bool>() || !NetUPnPPrompted.get<bool>())
		{
			if (LobbyMappingTriggered)
			{
				RemoveMapping();
				LobbyMappingTriggered = false;
			}

			return;
		}

		LobbyMappingTriggered = true;
		StartMapping();
	}

	void UPnP::MapPort(const std::uint16_t port)
	{
		bool shouldUninitialize = false;
		if (!InitializeCOM(shouldUninitialize))
		{
			Logger::Print("UPnP: COM init failed.\n");
			return;
		}

		IUPnPNAT* nat = nullptr;
		auto* mappings = GetPortMappingCollection(&nat);
		if (!mappings)
		{
			Logger::Print("UPnP: mapping collection unavailable.\n");
			if (nat) nat->Release();
			if (shouldUninitialize) CoUninitialize();
			return;
		}

		const auto localAddress = GetLocalAddressString();

		if (localAddress.empty())
		{
			Logger::Print("UPnP: could not find local LAN address.\n");
			mappings->Release();
			nat->Release();
			if (shouldUninitialize) CoUninitialize();
			return;
		}

		const BStr protocol(L"UDP");
		const BStr description(L"ZW3 game server");
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
			Logger::Print("UPnP: failed to map UDP port {} ({:#x}).\n", port, static_cast<unsigned int>(result));
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
		NetUPnP = Dvar::Register<bool>("net_upnp", false, Game::DVAR_ARCHIVE, "Use UPnP to automatically forward the game's UDP port while hosting. Requires router support and Windows Firewall permission.");
		NetUPnPPrompted = Dvar::Register<bool>("net_upnp_prompted", false, Game::DVAR_ARCHIVE, "Whether the UPnP setup explanation has been acknowledged.");

		Events::OnSVInit([]()
			{
				Scheduler::Once([]()
					{
						StartMapping();
					}, Scheduler::Pipeline::SERVER, 500ms);
			});

		if (!Dedicated::IsEnabled())
		{
			Scheduler::Loop(CheckLobbyState, Scheduler::Pipeline::MAIN, 500ms);
		}
	}

	void UPnP::preDestroy()
	{
		RemoveMapping();
	}
}
