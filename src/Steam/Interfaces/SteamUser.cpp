#include "STDInclude.hpp"

#include "Steam/Steam.hpp"
#include "SteamUser.hpp"

#include "Components/Modules/Auth.hpp"
#include "Components/Modules/Dedicated.hpp"
#include "Components/Modules/Singleton.hpp"

namespace Steam
{
	void* const User::vtable[] =
	{
		reinterpret_cast<void*>(UnusedSlot),
		reinterpret_cast<void*>(BLoggedOn),
		reinterpret_cast<void*>(GetSteamID),
		reinterpret_cast<void*>(InitiateGameConnection),
		reinterpret_cast<void*>(TerminateGameConnection),
	};

	Interface User::object = { User::vtable };

	Interface* User::Get()
	{
		static_assert(std::size(vtable) == 5);
		return &object;
	}

	SteamID User::LocalId()
	{
		static std::uint64_t idBits = 0;

		if (!idBits)
		{
			if (Components::Singleton::IsFirstInstance() && !Components::Dedicated::IsEnabled())
			{
				idBits = Components::Auth::GetKeyHash();
			}
			else
			{
				idBits = (static_cast<std::uint64_t>(Game::Sys_Milliseconds()) << 32) | timeGetTime();
			}
		}

		SteamID id;
		id.bits = idBits;
		return id;
	}

	bool User::BLoggedOn([[maybe_unused]] Interface* self)
	{
		return true;
	}

	SteamID* User::GetSteamID([[maybe_unused]] Interface* self, SteamID* result)
	{
		*result = LocalId();
		return result;
	}

	int User::InitiateGameConnection([[maybe_unused]] Interface* self, [[maybe_unused]] void* authBlob, [[maybe_unused]] int maxAuthBlob,
		[[maybe_unused]] SteamID gameServer, [[maybe_unused]] unsigned int serverIp, [[maybe_unused]] unsigned short serverPort, [[maybe_unused]] bool isSecure)
	{
		return 0;
	}

	void User::TerminateGameConnection([[maybe_unused]] Interface* self, [[maybe_unused]] unsigned int serverIp, [[maybe_unused]] unsigned short serverPort)
	{
	}
}
