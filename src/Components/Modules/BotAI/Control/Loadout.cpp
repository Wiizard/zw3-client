#include "Components/Modules/BotAI/Control/Loadout.hpp"
#include "Components/Modules/BotAI/Control/State.hpp"

namespace Components::BotAI
{
	using StructuredDataDef_GetAsset_t   = void* (__cdecl*)(const char* name, unsigned int bufferSize);

	using SV_GetPersistentData_t         = unsigned char* (__cdecl*)(int clientNum);

	struct StructuredDataBuffer
	{
		unsigned char* data;
		int size;
	};

	struct StructuredDataLookup
	{
		void* def;
		void* type;
		int offset;
		int state;
	};

	struct PathSegment
	{
		const char* name;
		int index;
	};

	struct StringWrite
	{
		PathSegment path[6];
		int segmentCount;
		const char* value;
	};

	static constexpr int maxExperience = 2516000;
	static const PathSegment experiencePath[] = { {"experience"} };

	static bool TryNavigate(void* def, StructuredDataLookup* lookup, const PathSegment* path, int segmentCount)
	{
		StructuredDataInitLookup(def, lookup);

		for (int i = 0; i < segmentCount; ++i)
		{
			if (path[i].name)
			{
				StructuredDataLookupString(lookup, path[i].name);
			}
			else
			{
				StructuredDataLookupInt(lookup, path[i].index);
			}

			if (lookup->state != 0)
			{
				return false;
			}
		}

		return true;
	}

	static bool TrySetString(void* def, StructuredDataBuffer* buffer, unsigned char* modifiedFlags,
							 const PathSegment* path, int segmentCount, const char* value)
	{
		StructuredDataLookup lookup;
		if (!TryNavigate(def, &lookup, path, segmentCount))
		{
			return false;
		}

		const int setResult = StructuredDataSetString(
			&lookup, buffer, modifiedFlags, value);
		return setResult < 2;
	}

	static bool TrySetNumber(void* def, StructuredDataBuffer* buffer, unsigned char* modifiedFlags,
							 const PathSegment* path, int segmentCount, int value)
	{
		StructuredDataLookup lookup;
		if (!TryNavigate(def, &lookup, path, segmentCount))
		{
			return false;
		}

		const StructuredDataSetNumber_t setters[] = {
			StructuredDataSetInt, StructuredDataSetBool, StructuredDataSetShort };

		for (const StructuredDataSetNumber_t setter : setters)
		{
			const int result = setter(&lookup, buffer, modifiedFlags, value);
			if (result < 2)
			{
				return true;
			}
		}

		return false;
	}

	void ClearPlayerData(int clientNum)
	{
		unsigned char* data =
			SV_GetPersistentDataBuffer(clientNum);
		for (int i = 0; i < playerDataSize; ++i)
		{
			data[i] = 0;
		}

		unsigned char* modifiedFlags =
			SV_GetPersistentDataFlags(clientNum);
		for (int i = 0; i < playerDataSize / 8; ++i)
		{
			modifiedFlags[i] = 0;
		}
	}

	bool TryWriteLoadout(int clientNum, const Loadout& loadout)
	{
		void* def = StructuredDataDefGetAsset(
			"mp/playerdata.def", playerDataSize);
		if (!def)
		{
			return false;
		}

		StructuredDataBuffer buffer = {
			SV_GetPersistentDataBuffer(clientNum),
			playerDataSize,
		};
		unsigned char* modifiedFlags =
			SV_GetPersistentDataFlags(clientNum);

		if (!TrySetNumber(def, &buffer, modifiedFlags, experiencePath, 1, maxExperience))
		{
			return false;
		}

		const StringWrite fields[] = {
			{ { {"customClasses"}, {nullptr, 0}, {"weaponSetups"}, {nullptr, 0}, {"weapon"} },                   5, loadout.primary },
			{ { {"customClasses"}, {nullptr, 0}, {"weaponSetups"}, {nullptr, 0}, {"attachment"}, {nullptr, 0} }, 6, loadout.primaryAttachment },
			{ { {"customClasses"}, {nullptr, 0}, {"weaponSetups"}, {nullptr, 0}, {"attachment"}, {nullptr, 1} }, 6, "none" },
			{ { {"customClasses"}, {nullptr, 0}, {"weaponSetups"}, {nullptr, 1}, {"weapon"} },                   5, loadout.secondary },
			{ { {"customClasses"}, {nullptr, 0}, {"weaponSetups"}, {nullptr, 1}, {"attachment"}, {nullptr, 0} }, 6, loadout.secondaryAttachment },
			{ { {"customClasses"}, {nullptr, 0}, {"weaponSetups"}, {nullptr, 1}, {"attachment"}, {nullptr, 1} }, 6, "none" },
			{ { {"customClasses"}, {nullptr, 0}, {"perks"}, {nullptr, 0} },                                      4, loadout.equipment },
			{ { {"customClasses"}, {nullptr, 0}, {"perks"}, {nullptr, 1} },                                      4, loadout.perk1 },
			{ { {"customClasses"}, {nullptr, 0}, {"perks"}, {nullptr, 2} },                                      4, loadout.perk2 },
			{ { {"customClasses"}, {nullptr, 0}, {"perks"}, {nullptr, 3} },                                      4, loadout.perk3 },
			{ { {"customClasses"}, {nullptr, 0}, {"perks"}, {nullptr, 4} },                                      4, loadout.deathstreak },
			{ { {"customClasses"}, {nullptr, 0}, {"specialGrenade"} },                                           3, loadout.tactical },
			{ { {"killstreaks"}, {nullptr, 0} },                                                                 2, loadout.killstreaks[0] },
			{ { {"killstreaks"}, {nullptr, 1} },                                                                 2, loadout.killstreaks[1] },
			{ { {"killstreaks"}, {nullptr, 2} },                                                                 2, loadout.killstreaks[2] },
		};

		for (const StringWrite& field : fields)
		{
			if (!TrySetString(def, &buffer, modifiedFlags, field.path, field.segmentCount, field.value))
			{
				BotLog("loadout client %d rejected %s", clientNum, field.value ? field.value : "(null)");
				return false;
			}
		}

		return true;
	}

	static int IsItemUnlockedForBots(unsigned int entref)
	{
		const int entityNum = static_cast<int>(entref & 0xFFFFu);
		const bool isEntity = (entref >> 16) == 0;
		if (isEntity && entityNum < *reinterpret_cast<int*>(svs_numClients) && IsFillBot(entityNum))
		{
			Scr_AddBool(1);
			return 1;
		}
		return GScr_IsItemUnlocked(entref);
	}

	bool InstallUnlockHook()
	{
		if (Utils::Hook::Get<std::uintptr_t>(GScr_IsItemUnlockedSlot) != Utils::Hook::Rebase(GScr_IsItemUnlockedAddress))
		{
			return false;
		}

		Utils::Hook::Set<void*>(GScr_IsItemUnlockedSlot, reinterpret_cast<void*>(IsItemUnlockedForBots));
		return true;
	}

	void UninstallUnlockHook()
	{
		Utils::Hook::Set<std::uintptr_t>(GScr_IsItemUnlockedSlot, Utils::Hook::Rebase(GScr_IsItemUnlockedAddress));
	}
}
