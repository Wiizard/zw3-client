#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "SteamMatchmaking.hpp"
#include "SteamUser.hpp"

#include "Components/Modules/Party.hpp"

namespace Steam
{
	void* const Matchmaking::vtable[] =
	{
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(CreateLobby),
		reinterpret_cast<void*>(JoinLobby),
		reinterpret_cast<void*>(LeaveLobby),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(GetNumLobbyMembers),
		reinterpret_cast<void*>(GetLobbyMemberByIndex),
		reinterpret_cast<void*>(GetLobbyData),
		reinterpret_cast<void*>(SetLobbyData),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(SetLobbyGameServer),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(SetLobbyMemberLimit),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(SetLobbyType),
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(GetLobbyOwner),
		reinterpret_cast<void*>(SetLobbyOwner),
	};

	Interface Matchmaking::object = { Matchmaking::vtable };

	Interface* Matchmaking::Get()
	{
		static_assert(std::size(vtable) == 37);
		return &object;
	}

	std::uint64_t Matchmaking::CreateLobby(Interface* self, [[maybe_unused]] int lobbyType, [[maybe_unused]] int maxMembers)
	{
		const std::uint64_t call = Callbacks::RegisterCall();

		SteamID lobby;
		lobby.accountID = 1337132;
		lobby.universe = 1;
		lobby.accountType = 8;
		lobby.accountInstance = 0x40000;

		auto* const created = ::Utils::Memory::Allocate<LobbyCreated>();
		created->m_eResult = 1;
		created->m_ulSteamIDLobby = lobby;

		Callbacks::ReturnCall(created, sizeof(LobbyCreated), LobbyCreated::CallbackID, call);

		JoinLobby(self, lobby);

		return call;
	}

	std::uint64_t Matchmaking::JoinLobby([[maybe_unused]] Interface* self, SteamID lobby)
	{
		const std::uint64_t call = Callbacks::RegisterCall();

		auto* const entered = ::Utils::Memory::Allocate<LobbyEnter>();
		entered->m_bLocked = false;
		entered->m_EChatRoomEnterResponse = 1;
		entered->m_rgfChatPermissions = 0xFFFFFFFF;
		entered->m_ulSteamIDLobby = lobby;

		Callbacks::ReturnCall(entered, sizeof(LobbyEnter), LobbyEnter::CallbackID, call);

		return call;
	}

	void Matchmaking::LeaveLobby([[maybe_unused]] Interface* self, SteamID lobby)
	{
		Components::Party::RemoveLobby(lobby);
	}

	int Matchmaking::GetNumLobbyMembers([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID lobby)
	{
		return 1;
	}

	SteamID* Matchmaking::GetLobbyMemberByIndex([[maybe_unused]] Interface* self, SteamID* result, [[maybe_unused]] SteamID lobby, [[maybe_unused]] int member)
	{
		*result = User::LocalId();
		return result;
	}

	const char* Matchmaking::GetLobbyData([[maybe_unused]] Interface* self, SteamID lobby, const char* key)
	{
		return Components::Party::GetLobbyInfo(lobby, key);
	}

	bool Matchmaking::SetLobbyData([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID lobby, [[maybe_unused]] const char* key, [[maybe_unused]] const char* value)
	{
		return true;
	}

	void Matchmaking::SetLobbyGameServer([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID lobby, [[maybe_unused]] unsigned int serverIp,
		[[maybe_unused]] unsigned short serverPort, [[maybe_unused]] SteamID server)
	{
	}

	bool Matchmaking::SetLobbyMemberLimit([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID lobby, [[maybe_unused]] int maxMembers)
	{
		return true;
	}

	bool Matchmaking::SetLobbyType([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID lobby, [[maybe_unused]] int lobbyType)
	{
		return true;
	}

	SteamID* Matchmaking::GetLobbyOwner([[maybe_unused]] Interface* self, SteamID* result, [[maybe_unused]] SteamID lobby)
	{
		*result = User::LocalId();
		return result;
	}

	bool Matchmaking::SetLobbyOwner([[maybe_unused]] Interface* self, [[maybe_unused]] SteamID lobby, [[maybe_unused]] SteamID newOwner)
	{
		return true;
	}
}
