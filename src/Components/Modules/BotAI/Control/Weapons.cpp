#include "Components/Modules/BotAI/Control/Ai.hpp"
#include <cmath>

namespace Components::BotAI
{
	static constexpr float noAdsDistance    = 200.0f;
	static constexpr float hipBodyHalfWidth = 20.0f;
	static constexpr float fullDamageRange  = 2000.0f;
	static constexpr float distMultiMin     = 0.5f;
	static constexpr float shortGunReachMulti = 1.5f;
	static constexpr float shotgunBodyArea = 1800.0f;
	static constexpr float hipRangeCapRifle = 150.0f;
	static constexpr float hipRangeCapSmg = 400.0f;
	static constexpr float shotgunShotDamageMin = 50.0f;
	static constexpr float shotgunReachStep = 25.0f;
	static constexpr float shotgunReachMax = 1500.0f;


	static const char* const fullAutoGuns[] = {
		"aa12", "ak47", "aug", "fn2000", "glock", "kriss", "m4", "m240", "masada",
		"mg4", "mp5k", "p90", "pp2000", "rpd", "sa80", "scar", "tavor", "tmp",
		"ump45", "uzi", "ac130", "heli", "ak47classic", "ak74u", "peacekeeper",
	};


	static bool NameTokenMatches(const char* name, const char* token)
	{
		int i = 0;
		for (; token[i]; ++i)
		{
			if (name[i] != token[i])
			{
				return false;
			}
		}
		return name[i] == '_' || name[i] == '\0';
	}


	static int ClipRounds(const char* playerState, const char* weaponDef)
	{
		const int pool = *reinterpret_cast<const int*>(weaponDef + weapDefClipPool);
		for (int i = 0; i < psAmmoSlots; ++i)
		{
			const int offset = psAmmoPoolId + psAmmoStride * i;
			if (*reinterpret_cast<const int*>(playerState + offset) == pool)
			{
				return *reinterpret_cast<const int*>(playerState + psAmmoClip - psAmmoPoolId + offset)
					+ *reinterpret_cast<const int*>(playerState + psAmmoClipLeft - psAmmoPoolId + offset);
			}
		}
		return 0;
	}


	static int StockRounds(const char* playerState, const char* weaponDef)
	{
		const int pool = *reinterpret_cast<const int*>(weaponDef + weapDefAmmoPool);
		for (int i = 0; i < psAmmoSlots; ++i)
		{
			const int offset = psStockPoolId + psStockStride * i;
			if (*reinterpret_cast<const int*>(playerState + offset) == pool)
			{
				return *reinterpret_cast<const int*>(playerState + psStockCount - psStockPoolId + offset);
			}
		}
		return 0;
	}


	bool IsDryGun(const char* playerState, int weaponIndex)
	{
		const char* weaponDef = WeaponDefOf(weaponIndex);
		if (!weaponDef)
		{
			return false;
		}
		const int weapClass = *reinterpret_cast<const int*>(weaponDef + weapDefClass);
		if (weapClass < weapClassRifle || weapClass > weapClassPistol)
		{
			return false;
		}
		return ClipRounds(playerState, weaponDef) == 0 && StockRounds(playerState, weaponDef) == 0;
	}


	WeaponInfo ReadWeapon(const char* playerState)
	{
		WeaponInfo info = {};
		info.distMulti = 1.0f;
		info.index = *reinterpret_cast<const unsigned short*>(playerState + psWeapon);
		if (info.index <= 0)
		{
			return info;
		}

		const char* weaponDef = WeaponDefOf(info.index);
		if (!weaponDef)
		{
			info.index = 0;
			return info;
		}

		info.weapClass = *reinterpret_cast<const int*>(weaponDef + weapDefClass);
		info.clipRounds = ClipRounds(playerState, weaponDef);
		info.hasAmmo = info.clipRounds > 0;

		const char* name = WeaponNameOf(info.index);
		if (name)
		{
			info.isAkimbo = HasSubstring(name, "_akimbo_");
			info.isSilenced = HasSubstring(name, "silencer");
			info.hasStats = TryGetGunStats(name, &info.stats);
			if (info.hasStats)
			{
				info.isFullAuto = info.stats.fireType == fireFullAuto;
				info.isBurst = info.stats.fireType == fireBurst;
			}
			else
			{
				for (const char* const gun : fullAutoGuns)
				{
					if (NameTokenMatches(name, gun))
					{
						info.isFullAuto = true;
						break;
					}
				}
			}
		}

		info.hipRange = noAdsDistance;
		switch (info.weapClass)
		{
		case weapClassRifle:
			info.distMulti = 0.9f;
			info.hipRange = 350.0f;
			break;
		case weapClassMg:
			info.hipRange = 350.0f;
			break;
		case weapClassSmg:
			info.distMulti = 0.7f;
			info.hipRange = 500.0f;
			break;
		case weapClassPistol:
			info.distMulti = 0.5f;
			info.hipRange = 250.0f;
			break;
		case weapClassSniper:
			info.hipRange = 0.0f;
			break;
		default:
			break;
		}

		if (info.hasStats && info.weapClass != weapClassSniper && info.stats.hip.standMin > 0.0f)
		{
			info.hipRange = hipBodyHalfWidth / std::tan(info.stats.hip.standMin / radToDeg);
		}

		if (info.hasStats && info.stats.minDamageRange > 0.0f)
		{
			float share = std::sqrt(info.stats.minDamageRange / fullDamageRange);
			if (share < distMultiMin)
			{
				share = distMultiMin;
			}
			if (share > 1.0f)
			{
				share = 1.0f;
			}
			info.distMulti = share;
		}

		return info;
	}


	static float ShotgunReach(const GunStats& stats)
	{
		const float coneTan = std::tan(stats.hip.standMin / radToDeg);
		for (float distance = shotgunReachStep; distance < shotgunReachMax; distance += shotgunReachStep)
		{
			float pelletDamage = static_cast<float>(stats.damage);
			if (distance >= stats.minDamageRange)
			{
				pelletDamage = static_cast<float>(stats.minDamage);
			}
			else if (distance > stats.maxDamageRange && stats.minDamageRange > stats.maxDamageRange)
			{
				const float share = (distance - stats.maxDamageRange) / (stats.minDamageRange - stats.maxDamageRange);
				pelletDamage += (static_cast<float>(stats.minDamage) - pelletDamage) * share;
			}
			const float coneRadius = distance * coneTan;
			float hitShare = 1.0f;
			if (coneRadius > 0.0f)
			{
				hitShare = shotgunBodyArea / (3.14159265f * coneRadius * coneRadius);
			}
			if (hitShare > 1.0f)
			{
				hitShare = 1.0f;
			}
			if (static_cast<float>(stats.pellets) * hitShare * pelletDamage < shotgunShotDamageMin)
			{
				return distance - shotgunReachStep;
			}
		}
		return shotgunReachMax;
	}


	bool IsInRange(const WeaponInfo& weapon, float distanceSq)
	{
		const bool isShortGun = weapon.weapClass == weapClassPistol || weapon.weapClass == weapClassSmg;
		if (isShortGun && !weapon.isAkimbo && weapon.hasStats && weapon.stats.minDamageRange > 0.0f)
		{
			const float reach = weapon.stats.minDamageRange * shortGunReachMulti;
			return distanceSq <= reach * reach;
		}
		if (weapon.weapClass != weapClassSpread && !weapon.isAkimbo)
		{
			return true;
		}
		if (weapon.weapClass == weapClassSpread && weapon.hasStats && weapon.stats.pellets > 1)
		{
			const float reach = ShotgunReach(weapon.stats);
			return distanceSq <= reach * reach;
		}
		return distanceSq <= (maxShotgunDistance * maxShotgunDistance);
	}


	bool CanAds(const WeaponInfo& weapon, float distanceSq, float hipScale)
	{
		if (weapon.weapClass == weapClassSpread || weapon.weapClass == weapClassGrenade
			|| weapon.isAkimbo)
		{
			return false;
		}
		float hipRange = weapon.hipRange * hipScale;
		float hipCap = hipRangeCapRifle;
		if (weapon.weapClass == weapClassSmg)
		{
			hipCap = hipRangeCapSmg;
		}
		if (hipRange > hipCap)
		{
			hipRange = hipCap;
		}
		return distanceSq > hipRange * hipRange;
	}


	static bool IsPistol(int index)
	{
		const char* weaponDef = WeaponDefOf(index);
		return weaponDef && *reinterpret_cast<const int*>(weaponDef + weapDefClass) == weapClassPistol;
	}


	unsigned short FindSidearm(const char* playerState)
	{
		return static_cast<unsigned short>(FindHeldWeapon(playerState, IsPistol));
	}


	static bool IsShotgun(int index)
	{
		const char* weaponDef = WeaponDefOf(index);
		return weaponDef && *reinterpret_cast<const int*>(weaponDef + weapDefClass) == weapClassSpread;
	}


	unsigned short FindShotgun(const char* playerState)
	{
		return static_cast<unsigned short>(FindHeldWeapon(playerState, IsShotgun));
	}


	int ClipRoundsOf(const char* playerState, int weaponIndex)
	{
		const char* weaponDef = WeaponDefOf(weaponIndex);
		if (!weaponDef)
		{
			return 0;
		}
		return ClipRounds(playerState, weaponDef);
	}


	int StockRoundsOf(const char* playerState, int weaponIndex)
	{
		const char* weaponDef = WeaponDefOf(weaponIndex);
		if (!weaponDef)
		{
			return 0;
		}
		return StockRounds(playerState, weaponDef);
	}


	static bool IsLobbedLethal(int index)
	{
		const char* name = WeaponNameOf(index);
		return name && (HasSubstring(name, "frag_grenade") || HasSubstring(name, "semtex"));
	}


	int FindLethalOffhand(const char* playerState)
	{
		return FindHeldWeapon(playerState, IsLobbedLethal);
	}


	static bool IsTactical(int index)
	{
		const char* name = WeaponNameOf(index);
		return name
			&& (HasSubstring(name, "concussion") || HasSubstring(name, "flash_grenade")
				|| HasSubstring(name, "smoke_grenade"));
	}


	int FindTacticalOffhand(const char* playerState)
	{
		return FindHeldWeapon(playerState, IsTactical);
	}


	bool HasLethalOffhand(const char* playerState)
	{
		return FindLethalOffhand(playerState) != 0;
	}


	bool HasTacticalOffhand(const char* playerState)
	{
		return FindTacticalOffhand(playerState) != 0;
	}
}
