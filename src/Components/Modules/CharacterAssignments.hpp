#pragma once

#include <Utils/InfoString.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Components::CharacterAssignments
{
	enum class Character : std::uint8_t
	{
		None = 0,
		Richtofen,
		Dempsey,
		Nikolai,
		Takeo
	};

	inline constexpr std::array<Character, 4> Characters
	{
		Character::Richtofen,
		Character::Dempsey,
		Character::Nikolai,
		Character::Takeo
	};

	inline std::array<std::atomic<std::uint8_t>, Game::MAX_CLIENTS> ClientCharacters{};
	inline std::array<std::atomic_bool, Game::MAX_CLIENTS> BotReservations{};
	inline std::array<std::atomic<int>, Game::MAX_CLIENTS> BotReservationStarted{};
	inline std::atomic<int> DesiredPartySize{ 1 };

	inline std::mutex StateMutex;
	inline std::unordered_map<std::uint64_t, Character> RealCharacters;
	inline std::uint64_t PendingReplacementXuid = 0;
	inline Character PendingReplacementCharacter = Character::None;
	inline int PendingReplacementStarted = 0;
	inline std::unordered_map<std::uint64_t, int> RecentReplacements;

	inline std::string Normalize(const std::string& value)
	{
		std::string result;
		result.reserve(value.size());

		for (std::size_t i = 0; i < value.size(); ++i)
		{
			if (value[i] == '^' && i + 1 < value.size())
			{
				++i;
				continue;
			}

			result.push_back(static_cast<char>(
				std::tolower(static_cast<unsigned char>(value[i]))));
		}

		const auto first = result.find_first_not_of(" \t");
		if (first == std::string::npos)
		{
			return {};
		}

		const auto last = result.find_last_not_of(" \t");
		return result.substr(first, last - first + 1);
	}

	inline Character Parse(const std::string& value)
	{
		const auto normalized = Normalize(value);
		if (normalized == "richtofen") return Character::Richtofen;
		if (normalized == "dempsey") return Character::Dempsey;
		if (normalized == "nikolai") return Character::Nikolai;
		if (normalized == "takeo") return Character::Takeo;
		return Character::None;
	}

	inline const char* ToString(const Character character)
	{
		switch (character)
		{
		case Character::Richtofen: return "Richtofen";
		case Character::Dempsey: return "Dempsey";
		case Character::Nikolai: return "Nikolai";
		case Character::Takeo: return "Takeo";
		default: return "None";
		}
	}

	inline bool IsValid(const Character character)
	{
		return character != Character::None;
	}

	inline void SetDesiredPartySize(const int size)
	{
		DesiredPartySize.store(std::clamp(size, 1, 4), std::memory_order_release);
	}

	inline int GetDesiredPartySize()
	{
		return std::clamp(DesiredPartySize.load(std::memory_order_acquire), 1, 4);
	}

	inline Character GetClientCharacterId(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return Character::None;
		}

		return static_cast<Character>(
			ClientCharacters[clientNum].load(std::memory_order_acquire));
	}

	inline std::string GetClientCharacter(const int clientNum)
	{
		return ToString(GetClientCharacterId(clientNum));
	}

	inline void SetClientCharacter(const int clientNum, const Character character)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return;
		}

		ClientCharacters[clientNum].store(
			static_cast<std::uint8_t>(character), std::memory_order_release);
	}

	inline void ClearClientCharacter(const int clientNum)
	{
		SetClientCharacter(clientNum, Character::None);
	}

	inline bool IsBotReserved(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return false;
		}

		return BotReservations[clientNum].load(std::memory_order_acquire);
	}

	inline void SetBotReserved(const int clientNum, const bool reserved)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return;
		}

		BotReservations[clientNum].store(reserved, std::memory_order_release);
		BotReservationStarted[clientNum].store(
			reserved ? Game::Sys_Milliseconds() : 0, std::memory_order_release);
	}

	inline std::uint64_t GetClientXuid(const Game::client_s& client)
	{
		if (client.steamID != 0)
		{
			return client.steamID;
		}

		Utils::InfoString info(client.userinfo);
		const auto value = info.get("xuid");
		if (value.empty())
		{
			return 0;
		}

		return std::strtoull(value.c_str(), nullptr, 16);
	}

	inline bool IsCharacterUsedLocked(const Character character,
		const std::uint64_t ignoredXuid = 0, const int ignoredClientNum = -1)
	{
		if (!IsValid(character))
		{
			return false;
		}

		if (PendingReplacementXuid != 0 && PendingReplacementXuid != ignoredXuid &&
			PendingReplacementCharacter == character)
		{
			return true;
		}

		for (const auto& [xuid, assigned] : RealCharacters)
		{
			if (xuid != ignoredXuid && assigned == character)
			{
				return true;
			}
		}

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			if (clientNum != ignoredClientNum &&
				GetClientCharacterId(clientNum) == character)
			{
				return true;
			}
		}

		return false;
	}

	inline bool IsCharacterUsed(const Character character,
		const int ignoredClientNum = -1)
	{
		std::scoped_lock lock(StateMutex);
		return IsCharacterUsedLocked(character, 0, ignoredClientNum);
	}

	inline Character FirstFreeCharacterLocked(const std::uint64_t ignoredXuid = 0,
		const int ignoredClientNum = -1, const std::size_t start = 0)
	{
		for (std::size_t offset = 0; offset < Characters.size(); ++offset)
		{
			const auto character = Characters[(start + offset) % Characters.size()];
			if (!IsCharacterUsedLocked(character, ignoredXuid, ignoredClientNum))
			{
				return character;
			}
		}

		return Character::None;
	}

	inline Character EnsureRealCharacter(const std::uint64_t xuid,
		const Character preferred = Character::None, const int clientNum = -1)
	{
		if (xuid == 0)
		{
			return Character::None;
		}

		std::scoped_lock lock(StateMutex);
		if (const auto found = RealCharacters.find(xuid);
			found != RealCharacters.end())
		{
			return found->second;
		}

		Character character = Character::None;
		if (IsValid(preferred) &&
			!IsCharacterUsedLocked(preferred, xuid, clientNum))
		{
			character = preferred;
		}

		if (!IsValid(character))
		{
			character = FirstFreeCharacterLocked(xuid, clientNum,
				static_cast<std::size_t>(xuid % Characters.size()));
		}

		if (IsValid(character))
		{
			RealCharacters[xuid] = character;
		}

		return character;
	}

	inline void ForgetRealCharacter(const std::uint64_t xuid)
	{
		if (xuid == 0)
		{
			return;
		}

		std::scoped_lock lock(StateMutex);
		RealCharacters.erase(xuid);
		RecentReplacements.erase(xuid);
		if (PendingReplacementXuid == xuid)
		{
			PendingReplacementXuid = 0;
			PendingReplacementCharacter = Character::None;
			PendingReplacementStarted = 0;
		}
	}

	inline void PruneRealCharacters(
		const std::unordered_set<std::uint64_t>& activeXuids)
	{
		std::scoped_lock lock(StateMutex);
		for (auto it = RealCharacters.begin(); it != RealCharacters.end();)
		{
			if (!activeXuids.contains(it->first))
			{
				RecentReplacements.erase(it->first);
				it = RealCharacters.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	inline void CleanupReplacementStateLocked(const int now)
	{
		for (auto it = RecentReplacements.begin();
			it != RecentReplacements.end();)
		{
			if (now - it->second >= 30000)
			{
				it = RecentReplacements.erase(it);
			}
			else
			{
				++it;
			}
		}

		if (PendingReplacementXuid != 0 && PendingReplacementStarted > 0 &&
			now - PendingReplacementStarted >= 30000)
		{
			PendingReplacementXuid = 0;
			PendingReplacementCharacter = Character::None;
			PendingReplacementStarted = 0;
		}
	}

	inline bool BeginReplacement(const std::uint64_t xuid,
		const Character character, const int now)
	{
		if (xuid == 0)
		{
			return false;
		}

		std::scoped_lock lock(StateMutex);
		CleanupReplacementStateLocked(now);
		if (PendingReplacementXuid != 0 || RecentReplacements.contains(xuid))
		{
			return false;
		}

		PendingReplacementXuid = xuid;
		PendingReplacementCharacter = character;
		PendingReplacementStarted = now;
		return true;
	}

	inline bool IsReplacementPendingOrRecent(const std::uint64_t xuid,
		const int now)
	{
		if (xuid == 0)
		{
			return true;
		}

		std::scoped_lock lock(StateMutex);
		CleanupReplacementStateLocked(now);
		return PendingReplacementXuid == xuid || RecentReplacements.contains(xuid);
	}

	inline bool HasPendingAdmission(const int now)
	{
		std::scoped_lock lock(StateMutex);
		CleanupReplacementStateLocked(now);
		return PendingReplacementXuid != 0;
	}

	inline Character ResolveClientCharacter(const int clientNum)
	{
		if (clientNum < 0 || clientNum >= Game::MAX_CLIENTS)
		{
			return Character::None;
		}

		const auto& client = Game::svs_clients[clientNum];
		const auto existing = GetClientCharacterId(clientNum);
		const auto xuid = GetClientXuid(client);

		if (xuid == 0)
		{
			if (client.bIsTestClient || IsValid(existing))
			{
				return existing;
			}

			if (client.header.state >= Game::CS_CONNECTED)
			{
				std::scoped_lock lock(StateMutex);
				const auto character = FirstFreeCharacterLocked(0, clientNum,
					static_cast<std::size_t>(clientNum) % Characters.size());
				if (IsValid(character))
				{
					SetClientCharacter(clientNum, character);
				}
				return character;
			}

			return Character::None;
		}

		std::scoped_lock lock(StateMutex);
		const int now = Game::Sys_Milliseconds();
		CleanupReplacementStateLocked(now);

		Character character = Character::None;
		const bool completingAdmission = PendingReplacementXuid == xuid;
		if (completingAdmission && IsValid(PendingReplacementCharacter))
		{
			character = PendingReplacementCharacter;
			RealCharacters[xuid] = character;
		}
		else if (const auto found = RealCharacters.find(xuid);
			found != RealCharacters.end())
		{
			character = found->second;
		}
		else if (IsValid(existing) &&
			!IsCharacterUsedLocked(existing, xuid, clientNum))
		{
			character = existing;
			RealCharacters[xuid] = character;
		}
		else
		{
			character = FirstFreeCharacterLocked(xuid, clientNum,
				static_cast<std::size_t>(xuid % Characters.size()));
			if (IsValid(character))
			{
				RealCharacters[xuid] = character;
			}
		}

		if (completingAdmission)
		{
			RecentReplacements[xuid] = now;
			PendingReplacementXuid = 0;
			PendingReplacementCharacter = Character::None;
			PendingReplacementStarted = 0;
		}

		if (IsValid(character))
		{
			SetBotReserved(clientNum, false);
			SetClientCharacter(clientNum, character);
		}

		return character;
	}

	inline bool IsXuidConnected(const std::uint64_t xuid,
		const int ignoredClientNum = -1)
	{
		if (xuid == 0)
		{
			return false;
		}

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			if (clientNum == ignoredClientNum)
			{
				continue;
			}

			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state >= Game::CS_CONNECTED &&
				GetClientXuid(client) == xuid)
			{
				return true;
			}
		}

		return false;
	}

	inline int CountConnectedRealPlayers()
	{
		std::unordered_set<std::uint64_t> xuids;
		std::unordered_set<std::string> activeFallbackNames;

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			const auto& client = Game::svs_clients[clientNum];
			if (client.header.state < Game::CS_CONNECTED)
			{
				continue;
			}

			const auto xuid = GetClientXuid(client);
			if (xuid != 0)
			{
				xuids.insert(xuid);
			}
			else if (!client.bIsTestClient &&
				client.header.state >= Game::CS_ACTIVE && client.name[0])
			{
				activeFallbackNames.insert(Normalize(client.name));
			}
		}

		return std::clamp(static_cast<int>(
			xuids.size() + activeFallbackNames.size()), 0, 4);
	}

	inline void PruneStaleBotReservations(const int now)
	{
		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			if (!IsBotReserved(clientNum))
			{
				continue;
			}

			const auto& client = Game::svs_clients[clientNum];
			const int started = BotReservationStarted[clientNum].load(
				std::memory_order_acquire);
			if (client.header.state == Game::CS_FREE && started > 0 &&
				now - started >= 2000)
			{
				SetBotReserved(clientNum, false);
				ClearClientCharacter(clientNum);
			}
		}
	}

	inline int CountReservedBots()
	{
		PruneStaleBotReservations(Game::Sys_Milliseconds());

		int count = 0;
		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			if (IsBotReserved(clientNum))
			{
				++count;
			}
		}
		return count;
	}

	inline int GetDesiredBotCount(const int now)
	{
		const int realPlayers = CountConnectedRealPlayers();
		const int pendingAdmissions = HasPendingAdmission(now) ? 1 : 0;
		return std::clamp(GetDesiredPartySize() - realPlayers - pendingAdmissions,
			0, 3);
	}

	inline void ClearClientSlot(const int clientNum)
	{
		SetBotReserved(clientNum, false);
		ClearClientCharacter(clientNum);
	}

	inline void ResetAll()
	{
		std::scoped_lock lock(StateMutex);
		RealCharacters.clear();
		PendingReplacementXuid = 0;
		PendingReplacementCharacter = Character::None;
		PendingReplacementStarted = 0;
		RecentReplacements.clear();
		DesiredPartySize.store(1, std::memory_order_release);

		for (int clientNum = 0; clientNum < Game::MAX_CLIENTS; ++clientNum)
		{
			SetBotReserved(clientNum, false);
			ClearClientCharacter(clientNum);
		}
	}
}
