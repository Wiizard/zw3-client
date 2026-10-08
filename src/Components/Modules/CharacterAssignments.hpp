#pragma once

#include <Utils/InfoString.hpp>

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

	inline constexpr std::array<Character, 4> characters
	{
		Character::Richtofen,
		Character::Dempsey,
		Character::Nikolai,
		Character::Takeo
	};

	inline std::array<std::atomic<std::uint8_t>, Game::MAX_CLIENTS> clientCharacters{};
	inline std::array<std::atomic_bool, Game::MAX_CLIENTS> botReservations{};
	inline std::array<std::atomic<int>, Game::MAX_CLIENTS> botReservationStarted{};
	inline std::atomic<int> desiredPartySize{ 1 };

	inline std::mutex stateMutex;
	inline std::unordered_map<std::uint64_t, Character> realCharacters;
	inline std::uint64_t pendingReplacementXuid = 0;
	inline Character pendingReplacementCharacter = Character::None;
	inline int pendingReplacementStarted = 0;
	inline std::unordered_map<std::uint64_t, int> recentReplacements;

	constexpr int replacementLifetimeMs = 30000;
	constexpr int staleBotReservationMs = 2000;
	constexpr int maxPartySize = 4;

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

			result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(value[i]))));
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

		if (normalized == "richtofen")
		{
			return Character::Richtofen;
		}

		if (normalized == "dempsey")
		{
			return Character::Dempsey;
		}

		if (normalized == "nikolai")
		{
			return Character::Nikolai;
		}

		if (normalized == "takeo")
		{
			return Character::Takeo;
		}

		return Character::None;
	}

	inline const char* ToString(const Character character)
	{
		switch (character)
		{
		case Character::Richtofen:
			return "Richtofen";
		case Character::Dempsey:
			return "Dempsey";
		case Character::Nikolai:
			return "Nikolai";
		case Character::Takeo:
			return "Takeo";
		default:
			return "None";
		}
	}

	inline bool IsValid(const Character character)
	{
		return character != Character::None;
	}

	inline bool IsClientNum(const int clientNum)
	{
		return clientNum >= 0 && clientNum < static_cast<int>(Game::MAX_CLIENTS);
	}

	inline void SetDesiredPartySize(const int size)
	{
		desiredPartySize.store(std::clamp(size, 1, maxPartySize), std::memory_order_release);
	}

	inline int GetDesiredPartySize()
	{
		return std::clamp(desiredPartySize.load(std::memory_order_acquire), 1, maxPartySize);
	}

	inline Character GetClientCharacterId(const int clientNum)
	{
		if (!IsClientNum(clientNum))
		{
			return Character::None;
		}

		return static_cast<Character>(clientCharacters[clientNum].load(std::memory_order_acquire));
	}

	inline std::string GetClientCharacter(const int clientNum)
	{
		return ToString(GetClientCharacterId(clientNum));
	}

	inline void SetClientCharacter(const int clientNum, const Character character)
	{
		if (!IsClientNum(clientNum))
		{
			return;
		}

		clientCharacters[clientNum].store(static_cast<std::uint8_t>(character), std::memory_order_release);
	}

	inline void ClearClientCharacter(const int clientNum)
	{
		SetClientCharacter(clientNum, Character::None);
	}

	inline bool IsBotReserved(const int clientNum)
	{
		if (!IsClientNum(clientNum))
		{
			return false;
		}

		return botReservations[clientNum].load(std::memory_order_acquire);
	}

	inline void SetBotReserved(const int clientNum, const bool isReserved)
	{
		if (!IsClientNum(clientNum))
		{
			return;
		}

		int started = 0;

		if (isReserved)
		{
			started = Game::Sys_Milliseconds();
		}

		botReservations[clientNum].store(isReserved, std::memory_order_release);
		botReservationStarted[clientNum].store(started, std::memory_order_release);
	}

	inline std::uint64_t GetClientXuid(const Game::client_s& client)
	{
		if (client.steamID != 0)
		{
			return client.steamID;
		}

		const Utils::InfoString info(client.userinfo);
		const auto value = info.Get("xuid");

		if (value.empty())
		{
			return 0;
		}

		return std::strtoull(value.data(), nullptr, 16);
	}

	inline bool IsCharacterUsedLocked(const Character character, const std::uint64_t ignoredXuid = 0, const int ignoredClientNum = -1)
	{
		if (!IsValid(character))
		{
			return false;
		}

		if (pendingReplacementXuid != 0 && pendingReplacementXuid != ignoredXuid && pendingReplacementCharacter == character)
		{
			return true;
		}

		for (const auto& [xuid, assigned] : realCharacters)
		{
			if (xuid != ignoredXuid && assigned == character)
			{
				return true;
			}
		}

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			if (clientNum != ignoredClientNum && GetClientCharacterId(clientNum) == character)
			{
				return true;
			}
		}

		return false;
	}

	inline bool IsCharacterUsed(const Character character, const int ignoredClientNum = -1)
	{
		std::scoped_lock lock(stateMutex);
		return IsCharacterUsedLocked(character, 0, ignoredClientNum);
	}

	inline Character FirstFreeCharacterLocked(const std::uint64_t ignoredXuid = 0, const int ignoredClientNum = -1, const std::size_t start = 0)
	{
		for (std::size_t offset = 0; offset < characters.size(); ++offset)
		{
			const auto character = characters[(start + offset) % characters.size()];

			if (!IsCharacterUsedLocked(character, ignoredXuid, ignoredClientNum))
			{
				return character;
			}
		}

		return Character::None;
	}

	inline Character EnsureRealCharacter(const std::uint64_t xuid, const Character preferred = Character::None, const int clientNum = -1)
	{
		if (xuid == 0)
		{
			return Character::None;
		}

		std::scoped_lock lock(stateMutex);

		if (const auto found = realCharacters.find(xuid); found != realCharacters.end())
		{
			return found->second;
		}

		Character character = Character::None;

		if (IsValid(preferred) && !IsCharacterUsedLocked(preferred, xuid, clientNum))
		{
			character = preferred;
		}

		if (!IsValid(character))
		{
			character = FirstFreeCharacterLocked(xuid, clientNum, static_cast<std::size_t>(xuid % characters.size()));
		}

		if (IsValid(character))
		{
			realCharacters[xuid] = character;
		}

		return character;
	}

	inline void ForgetRealCharacter(const std::uint64_t xuid)
	{
		if (xuid == 0)
		{
			return;
		}

		std::scoped_lock lock(stateMutex);
		realCharacters.erase(xuid);
		recentReplacements.erase(xuid);

		if (pendingReplacementXuid == xuid)
		{
			pendingReplacementXuid = 0;
			pendingReplacementCharacter = Character::None;
			pendingReplacementStarted = 0;
		}
	}

	inline void PruneRealCharacters(const std::unordered_set<std::uint64_t>& activeXuids)
	{
		std::scoped_lock lock(stateMutex);

		for (auto it = realCharacters.begin(); it != realCharacters.end();)
		{
			if (!activeXuids.contains(it->first))
			{
				recentReplacements.erase(it->first);
				it = realCharacters.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	inline void CleanupReplacementStateLocked(const int now)
	{
		for (auto it = recentReplacements.begin(); it != recentReplacements.end();)
		{
			if (now - it->second >= replacementLifetimeMs)
			{
				it = recentReplacements.erase(it);
			}
			else
			{
				++it;
			}
		}

		if (pendingReplacementXuid != 0 && pendingReplacementStarted > 0 && now - pendingReplacementStarted >= replacementLifetimeMs)
		{
			pendingReplacementXuid = 0;
			pendingReplacementCharacter = Character::None;
			pendingReplacementStarted = 0;
		}
	}

	inline bool BeginReplacement(const std::uint64_t xuid, const Character character, const int now)
	{
		if (xuid == 0)
		{
			return false;
		}

		std::scoped_lock lock(stateMutex);
		CleanupReplacementStateLocked(now);

		if (pendingReplacementXuid != 0 || recentReplacements.contains(xuid))
		{
			return false;
		}

		pendingReplacementXuid = xuid;
		pendingReplacementCharacter = character;
		pendingReplacementStarted = now;
		return true;
	}

	inline bool IsReplacementPendingOrRecent(const std::uint64_t xuid, const int now)
	{
		if (xuid == 0)
		{
			return true;
		}

		std::scoped_lock lock(stateMutex);
		CleanupReplacementStateLocked(now);
		return pendingReplacementXuid == xuid || recentReplacements.contains(xuid);
	}

	inline bool HasPendingAdmission(const int now)
	{
		std::scoped_lock lock(stateMutex);
		CleanupReplacementStateLocked(now);
		return pendingReplacementXuid != 0;
	}

	inline Character ResolveClientCharacter(const int clientNum)
	{
		if (!IsClientNum(clientNum))
		{
			return Character::None;
		}

		const auto& client = Game::svs_clients[clientNum];
		const auto existing = GetClientCharacterId(clientNum);
		const auto xuid = GetClientXuid(client);

		if (xuid == 0)
		{
			if (IsValid(existing))
			{
				return existing;
			}

			if (client.header.state < Game::CS_CONNECTED && !client.bIsTestClient)
			{
				return Character::None;
			}

			std::scoped_lock lock(stateMutex);
			const auto character = FirstFreeCharacterLocked(0, clientNum, static_cast<std::size_t>(clientNum) % characters.size());

			if (IsValid(character))
			{
				SetClientCharacter(clientNum, character);

				if (client.bIsTestClient)
				{
					SetBotReserved(clientNum, true);
				}
			}

			return character;
		}

		std::scoped_lock lock(stateMutex);
		const int now = Game::Sys_Milliseconds();
		CleanupReplacementStateLocked(now);

		Character character = Character::None;
		const bool isCompletingAdmission = pendingReplacementXuid == xuid;

		if (isCompletingAdmission && IsValid(pendingReplacementCharacter))
		{
			character = pendingReplacementCharacter;
			realCharacters[xuid] = character;
		}
		else if (const auto found = realCharacters.find(xuid); found != realCharacters.end())
		{
			character = found->second;
		}
		else if (IsValid(existing) && !IsCharacterUsedLocked(existing, xuid, clientNum))
		{
			character = existing;
			realCharacters[xuid] = character;
		}
		else
		{
			character = FirstFreeCharacterLocked(xuid, clientNum, static_cast<std::size_t>(xuid % characters.size()));

			if (IsValid(character))
			{
				realCharacters[xuid] = character;
			}
		}

		if (isCompletingAdmission)
		{
			recentReplacements[xuid] = now;
			pendingReplacementXuid = 0;
			pendingReplacementCharacter = Character::None;
			pendingReplacementStarted = 0;
		}

		if (IsValid(character))
		{
			SetBotReserved(clientNum, false);
			SetClientCharacter(clientNum, character);
		}

		return character;
	}

	inline bool IsXuidConnected(const std::uint64_t xuid, const int ignoredClientNum = -1)
	{
		if (xuid == 0)
		{
			return false;
		}

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			if (clientNum == ignoredClientNum)
			{
				continue;
			}

			const auto& client = Game::svs_clients[clientNum];

			if (client.header.state >= Game::CS_CONNECTED && GetClientXuid(client) == xuid)
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

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
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
			else if (!client.bIsTestClient && client.header.state >= Game::CS_ACTIVE && client.name[0])
			{
				activeFallbackNames.insert(Normalize(client.name));
			}
		}

		return std::clamp(static_cast<int>(xuids.size() + activeFallbackNames.size()), 0, maxPartySize);
	}

	inline void PruneStaleBotReservations(const int now)
	{
		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			if (!IsBotReserved(clientNum))
			{
				continue;
			}

			const auto& client = Game::svs_clients[clientNum];
			const int started = botReservationStarted[clientNum].load(std::memory_order_acquire);

			if (client.header.state == Game::CS_FREE && started > 0 && now - started >= staleBotReservationMs)
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

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
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
		int pendingAdmissions = 0;

		if (HasPendingAdmission(now))
		{
			pendingAdmissions = 1;
		}

		return std::clamp(GetDesiredPartySize() - realPlayers - pendingAdmissions, 0, maxPartySize - 1);
	}

	inline void ClearClientSlot(const int clientNum)
	{
		SetBotReserved(clientNum, false);
		ClearClientCharacter(clientNum);
	}

	inline void ResetAll()
	{
		std::scoped_lock lock(stateMutex);
		realCharacters.clear();
		pendingReplacementXuid = 0;
		pendingReplacementCharacter = Character::None;
		pendingReplacementStarted = 0;
		recentReplacements.clear();
		desiredPartySize.store(1, std::memory_order_release);

		for (int clientNum = 0; clientNum < static_cast<int>(Game::MAX_CLIENTS); ++clientNum)
		{
			SetBotReserved(clientNum, false);
			ClearClientCharacter(clientNum);
		}
	}
}
