#include "STDInclude.hpp"

#include "Weapon.hpp"
#include "WeaponFields.hpp"
#include "Events.hpp"
#include "Flags.hpp"
#include "Logger.hpp"

#include "GSC/Script.hpp"

extern "C"
{
	void CG_SelectWeaponIndexStub();

	std::uintptr_t Weapon_BG_GetWeaponDef = 0;
	std::uintptr_t Weapon_CG_SelectWeaponIndexFail = 0;
}

namespace Components
{
	constexpr std::uintptr_t BG_GetWeaponIndexForName = 0x14009C8B0;
	constexpr std::uintptr_t BG_FindWeaponIndexForName = 0x14009C750;
	constexpr std::uintptr_t BG_SetupWeaponIndex = 0x14009CC30;
	constexpr std::uintptr_t bg_lastParsedWeaponIndex = 0x140441320;
	constexpr std::uintptr_t bg_weaponCompleteDefs = 0x140441330;

	static unsigned int weaponSlotCount = Weapon::BASEGAME_WEAPON_LIMIT;

	constexpr std::uintptr_t Hunk_AllocLowAlign = 0x14027EAC0;

	constexpr std::uintptr_t FX_Register = 0x14014D4C0;
	constexpr std::uintptr_t SND_FindAlias = 0x140280710;
	constexpr std::uintptr_t DB_FindTracer = 0x14024FDE0;
	constexpr std::uintptr_t DB_IsXAssetDefault = 0x14012E180;

	constexpr std::uintptr_t SL_GetStringOfSize = 0x1402222C0;

	constexpr std::uintptr_t g_playerAnimTypeNamesCount = 0x140441118;
	constexpr std::uintptr_t g_playerAnimTypeNames = 0x140441120;

	constexpr unsigned int ASSET_TYPE_WEAPON = 0x1C;

	static Utils::Hook weaponIndexHook;

	constexpr std::uintptr_t G_GetWeaponIndexForName_Initializing = 0x1401884D7;
	constexpr std::uintptr_t G_ModelIndex_Initializing = 0x1401AC07B;
	static const std::uint8_t weaponInitializingJump[] = { 0x74, 0x0C };
	static const std::uint8_t modelInitializingJump[] = { 0x75, 0x05 };

	constexpr std::uintptr_t BG_LoadDefaultWeaponCompleteDef = 0x14009C0A0;
	constexpr std::uintptr_t BG_LoadDefaultWeaponCompleteDef_FindCall = 0x14009C0AC;
	constexpr std::uintptr_t BG_LoadPlayerAnimTypes = 0x14009C0C0;
	constexpr std::uintptr_t BG_LoadPlayerAnimTypesCall = 0x14009C322;
	constexpr std::uintptr_t DB_FindXAssetHeader = 0x14012D6D0;
	static const std::uint8_t loadDefaultWeapon[] = { 0x48, 0x8D, 0x15, 0x49, 0x0F, 0x34, 0x00, 0xB9, 0x1C, 0x00, 0x00, 0x00 };

	static Utils::Hook noneWeaponHook;

	Dvar::Var Weapon::cg_recoilMultiplier;

	constexpr std::uintptr_t BG_WeaponFireRecoil = 0x14009E320;
	constexpr std::uintptr_t BG_WeaponFireRecoilCalls[] = { 0x1400C3301, 0x14019422C };

	static Utils::Hook recoilHooks[std::size(BG_WeaponFireRecoilCalls)];

	Dvar::Var Weapon::bg_disableDoubleTaps;

	constexpr std::uintptr_t PM_Weapon = 0x140097CD0;
	constexpr std::uintptr_t PM_WeaponCalls[] = { 0x140094BA6, 0x140094DCA, 0x140095192 };

	static Utils::Hook weaponHooks[std::size(PM_WeaponCalls)];

	constexpr std::uintptr_t BG_GetWeaponDef = 0x14009C8A0;
	constexpr std::uintptr_t CG_SelectWeaponIndex_GetDefCall = 0x1400C5292;
	constexpr std::uintptr_t CG_SelectWeaponIndex_Fail = 0x1400C5311;
	static const std::uint8_t selectFail[] = { 0x32, 0xC0, 0xE9, 0x94, 0x01, 0x00, 0x00 };

	static Utils::Hook selectWeaponHook;

	constexpr std::uintptr_t WeaponEntCanBeGrabbed = 0x14008D6D0;
	constexpr std::uintptr_t WeaponEntCanBeGrabbedCalls[] = { 0x140089227, 0x140089249 };

	static Utils::Hook grabbedHooks[std::size(WeaponEntCanBeGrabbedCalls)];

	void Weapon::PM_Weapon_Stub(Game::pmove_s* pm, void* pml)
	{
		if (bg_disableDoubleTaps.IsValid() && bg_disableDoubleTaps.Get<bool>() && pm && pm->ps)
		{
			auto* const ps = pm->ps;
			const int rightHandState = ps->weapState[Game::WEAPON_HAND_RIGHT].weaponState;
			const bool isDropping = rightHandState == Game::WEAPON_DROPPING
				|| rightHandState == Game::WEAPON_DROPPING_QUICK
				|| rightHandState == Game::WEAPON_DROPPING_ALT;

			if (pm->cmd.weapon == ps->weapCommon.weapon && isDropping)
			{
				for (int hand = 0; hand < Game::NUM_WEAPON_HANDS; ++hand)
				{
					ps->weapState[hand].weaponState = Game::WEAPON_RAISING;
					ps->weapState[hand].weaponTime = Game::BG_GetWeaponDef(ps->weapCommon.weapon)->quickRaiseTime;
					ps->weapState[hand].weapAnim = Game::WEAP_ANIM_QUICK_DROP;
				}
			}
		}

		reinterpret_cast<void(*)(Game::pmove_s*, void*)>(Utils::Hook::Rebase(PM_Weapon))(pm, pml);
	}

	void* Weapon::LoadNoneWeaponHook()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(BG_LoadPlayerAnimTypes))();

		return Game::DB_FindXAssetHeader(ASSET_TYPE_WEAPON, "none");
	}

	int Weapon::WeaponEntCanBeGrabbed_Stub(const Game::entityState_s* es, const Game::playerState_s* ps, int touched, unsigned int weapon)
	{
		const bool isPickupDisabled = (ps->weapCommon.weapFlags & Game::PWF_DISABLE_WEAPON_PICKUP) != 0;

		if (!touched && es->eType != Game::ET_MISSILE && isPickupDisabled)
		{
			return 0;
		}

		return reinterpret_cast<int(*)(const Game::entityState_s*, const Game::playerState_s*, int, unsigned int)>(
			Utils::Hook::Rebase(WeaponEntCanBeGrabbed))(es, ps, touched, weapon);
	}

	void Weapon::AddScriptMethods()
	{
		GSC::Script::AddMethod("DisableWeaponPickup", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			ent->client->ps.weapCommon.weapFlags |= Game::PWF_DISABLE_WEAPON_PICKUP;
		});

		GSC::Script::AddMethod("EnableWeaponPickup", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			ent->client->ps.weapCommon.weapFlags &= ~Game::PWF_DISABLE_WEAPON_PICKUP;
		});

		GSC::Script::AddMethod("AreControlsFrozen", [](const Game::scr_entref_t entref)
		{
			const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

			Game::Scr_AddBool((ent->client->flags & Game::CF_BIT_FROZEN) != 0);
		});

		GSC::Script::AddMethod("InitialWeaponRaise", PlayerCmd_InitialWeaponRaise);
		GSC::Script::AddMethod("FreezeControlsAllowLook", PlayerCmd_FreezeControlsAllowLook);
	}

	void Weapon::PlayerCmd_InitialWeaponRaise(const Game::scr_entref_t entref)
	{
		auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);
		const auto* weapon = Game::Scr_GetString(0);
		const auto index = Game::G_GetWeaponIndexForName(weapon);

		auto* ps = &ent->client->ps;

		if (!Game::BG_IsWeaponValid(ps, index))
		{
			GSC::Script::Scr_Error(Utils::String::VA("invalid InitialWeaponRaise: %s", weapon));
			return;
		}

		if (!index)
		{
			return;
		}

		auto* equippedWeapon = Game::BG_GetEquippedWeaponState(ps, index);

		if (!equippedWeapon)
		{
			return;
		}

		equippedWeapon->usedBefore = false;
		Game::PlayerCmd_switchToWeapon(entref);
	}

	void Weapon::PlayerCmd_FreezeControlsAllowLook(const Game::scr_entref_t entref)
	{
		const auto* ent = GSC::Script::Scr_GetPlayerEntity(entref);

		if (Game::Scr_GetInt(0))
		{
			ent->client->ps.weapCommon.weapFlags |= Game::PWF_DISABLE_WEAPONS;
			ent->client->flags |= Game::CF_BIT_DISABLE_USABILITY;
		}
		else
		{
			ent->client->ps.weapCommon.weapFlags &= ~Game::PWF_DISABLE_WEAPONS;
			ent->client->flags &= ~Game::CF_BIT_DISABLE_USABILITY;
		}
	}

	void Weapon::BG_WeaponFireRecoil_Stub(void* ps, float* recoilSpeed, float* kickAVel, unsigned int* holdrand, int hand)
	{
		float adjustedRecoilSpeed[3]{};
		float adjustedKick[3]{};

		reinterpret_cast<void(*)(void*, float*, float*, unsigned int*, int)>(Utils::Hook::Rebase(BG_WeaponFireRecoil))(
			ps, adjustedRecoilSpeed, adjustedKick, holdrand, hand);

		float multiplier = 1.0f;

		if (cg_recoilMultiplier.IsValid())
		{
			multiplier = cg_recoilMultiplier.Get<float>();
		}

		for (int axis = 0; axis < 3; ++axis)
		{
			recoilSpeed[axis] = adjustedRecoilSpeed[axis] * multiplier;
			kickAVel[axis] = adjustedKick[axis] * multiplier;
		}
	}

	static const char* const weaponTypeNames[] = { "bullet", "grenade", "projectile", "riotshield" };
	static const char* const weaponClassNames[] = { "rifle", "sniper", "mg", "smg", "spread", "pistol", "grenade", "rocketlauncher", "turret", "throwingknife", "non-player", "item" };
	static const char* const overlayReticleNames[] = { "none", "crosshair" };
	static const char* const penetrateTypeNames[] = { "none", "small", "medium", "large" };
	static const char* const impactTypeNames[] = { "none", "bullet_small", "bullet_large", "bullet_ap", "bullet_explode", "shotgun", "shotgun_explode", "grenade_bounce", "grenade_explode", "rocket_explode", "projectile_dud" };
	static const char* const stanceNames[] = { "stand", "duck", "prone" };
	static const char* const projExplosionNames[] = { "grenade", "rocket", "flashbang", "none", "dud", "smoke", "heavy explosive" };
	static const char* const offhandClassNames[] = { "None", "Frag Grenade", "Smoke Grenade", "Flash Grenade", "Throwing Knife", "Other" };
	static const char* const activeReticleNames[] = { "None", "Pip-On-A-Stick", "Bouncing diamond" };
	static const char* const guidedMissileNames[] = { "None", "Sidewinder", "Hellfire", "Javelin" };
	static const char* const stickinessNames[] = { "Don't stick", "Stick to all", "Stick to all, orient to surface", "Stick to ground", "Stick to ground, maintain yaw", "Knife" };
	static const char* const overlayInterfaceNames[] = { "None", "Javelin", "Turret Scope" };
	static const char* const inventoryTypeNames[] = { "primary", "offhand", "item", "altmode", "exclusive", "scavenger" };
	static const char* const fireTypeNames[] = { "Full Auto", "Single Shot", "2-Round Burst", "3-Round Burst", "4-Round Burst", "Double Barrel" };
	static const char* const ammoCounterClipNames[] = { "None", "Magazine", "ShortMagazine", "Shotgun", "Rocket", "Beltfed", "AltWeapon" };
	static const char* const iconRatioNames[] = { "1:1", "2:1", "4:1" };

	static const char* const surfaceTypeNames[] =
	{
		"default", "bark", "brick", "carpet", "cloth", "concrete", "dirt", "flesh",
		"foliage", "glass", "grass", "gravel", "ice", "metal", "mud", "paper",
		"plaster", "rock", "sand", "snow", "water", "wood", "asphalt", "ceramic",
		"plastic", "rubber", "cushion", "fruit", "paintedmetal", "riotshield", "slush",
	};

	static void* HunkAlloc(std::size_t size)
	{
		return reinterpret_cast<void*(*)(std::size_t)>(Utils::Hook::Rebase(Hunk_AllocLowAlign))(size);
	}

	static const char* HunkString(const std::string& text)
	{
		auto* const copy = static_cast<char*>(HunkAlloc(text.size() + 1));

		if (!copy)
		{
			return "";
		}

		std::memcpy(copy, text.data(), text.size() + 1);
		return copy;
	}

	static bool ReadWeaponFile(const char* name, std::unordered_map<std::string, std::string>& fields)
	{
		const std::string path = "weapons/mp/" + std::string(name);
		void* buffer = nullptr;
		const int length = Game::FS_ReadFile(path.data(), &buffer);

		if (length <= 0 || !buffer)
		{
			return false;
		}

		const std::string text(static_cast<const char*>(buffer), static_cast<std::size_t>(length));
		Game::FS_FreeFile(buffer);

		constexpr std::string_view header = "WEAPONFILE";

		if (!text.starts_with(header))
		{
			Logger::Error("weapon: {} does not start with WEAPONFILE\n", path);
			return false;
		}

		std::size_t at = header.size();

		while (at < text.size())
		{
			if (text[at] != '\\')
			{
				break;
			}

			const std::size_t keyStart = at + 1;
			const std::size_t keyEnd = text.find('\\', keyStart);

			if (keyEnd == std::string::npos)
			{
				break;
			}

			const std::size_t valueEnd = text.find('\\', keyEnd + 1);
			const std::string key = Utils::String::ToLower(text.substr(keyStart, keyEnd - keyStart));
			const std::string value = text.substr(keyEnd + 1,
				(valueEnd == std::string::npos ? text.size() : valueEnd) - keyEnd - 1);

			fields.try_emplace(key, value);
			at = (valueEnd == std::string::npos) ? text.size() : valueEnd;
		}

		return !fields.empty();
	}

	static int EnumIndex(const std::string& value, const char* const* names, std::size_t count)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			if (_stricmp(value.data(), names[i]) == 0)
			{
				return static_cast<int>(i);
			}
		}

		return -1;
	}

	static int PlayerAnimTypeIndex(const std::string& value)
	{
		const int count = Utils::Hook::Get<int>(g_playerAnimTypeNamesCount);
		const auto* const names = reinterpret_cast<const char* const*>(Utils::Hook::Rebase(g_playerAnimTypeNames));

		for (int i = 0; i < count; ++i)
		{
			if (names[i] && _stricmp(value.data(), names[i]) == 0)
			{
				return i;
			}
		}

		return -1;
	}

	static std::vector<std::string> Tokenize(const std::string& value)
	{
		std::vector<std::string> tokens;
		std::size_t at = 0;

		while (at < value.size())
		{
			while (at < value.size() && std::isspace(static_cast<unsigned char>(value[at])))
			{
				++at;
			}

			const std::size_t start = at;

			while (at < value.size() && !std::isspace(static_cast<unsigned char>(value[at])))
			{
				++at;
			}

			if (at > start)
			{
				tokens.push_back(value.substr(start, at - start));
			}
		}

		return tokens;
	}

	static unsigned short ScriptString(const std::string& text)
	{
		const auto getString = reinterpret_cast<unsigned short(*)(const char*, unsigned int, int)>(
			Utils::Hook::Rebase(SL_GetStringOfSize));

		return getString(text.data(), 0, static_cast<int>(text.size()) + 1);
	}

	static void* FindSoundAlias(const std::string& name)
	{
		return reinterpret_cast<void*(*)(const char*)>(Utils::Hook::Rebase(SND_FindAlias))(name.data());
	}

	static void** BuildBounceSounds(const std::string& value)
	{
		auto** const sounds = static_cast<void**>(HunkAlloc(std::size(surfaceTypeNames) * sizeof(void*)));

		if (!sounds)
		{
			return nullptr;
		}

		void* const fallback = FindSoundAlias(value + "_default");

		for (std::size_t i = 0; i < std::size(surfaceTypeNames); ++i)
		{
			void* const alias = FindSoundAlias(value + "_" + surfaceTypeNames[i]);
			sounds[i] = alias ? alias : fallback;
		}

		return sounds;
	}

	static bool ApplyField(Game::WeaponFullDef* full, const WeaponField& field, const std::string& value, const char* weaponName)
	{
		auto* const at = reinterpret_cast<std::uint8_t*>(full) + field.offset;

		const auto writeEnum = [&](const char* const* names, std::size_t count, const char* what)
		{
			const int index = EnumIndex(value, names, count);

			if (index < 0)
			{
				Logger::Error("weapon: {} has an unknown {} '{}'\n", weaponName, what, value);
				return;
			}

			*reinterpret_cast<int*>(at) = index;
		};

		switch (field.type)
		{
		case 0:
			*reinterpret_cast<const char**>(at) = value.empty() ? "" : HunkString(value);
			break;
		case 4:
			*reinterpret_cast<int*>(at) = std::atoi(value.data());
			break;
		case 6:
			*reinterpret_cast<bool*>(at) = std::atoi(value.data()) != 0;
			break;
		case 7:
			*reinterpret_cast<float*>(at) = static_cast<float>(std::atof(value.data()));
			break;
		case 9:
			*reinterpret_cast<int*>(at) = static_cast<int>(static_cast<double>(static_cast<float>(std::atof(value.data()))) * 1000.0);
			break;
		case 10:
			*reinterpret_cast<void**>(at) = reinterpret_cast<void*(*)(const char*)>(Utils::Hook::Rebase(FX_Register))(value.data());
			break;
		case 11:
		{
			auto* const model = Game::R_RegisterModel(value.data());

			if (!model)
			{
				Logger::Error("weapon: {} wants the model '{}', which is not loaded\n", weaponName, value);
				return false;
			}

			*reinterpret_cast<Game::XModel**>(at) = model;
			break;
		}
		case 12:
			*reinterpret_cast<void**>(at) = Game::Material_RegisterHandle(value.data(), 0);
			break;
		case 13:
			Logger::Error("weapon: {} sets physCollmap '{}', which this port cannot resolve\n", weaponName, value);
			break;
		case 14:
			*reinterpret_cast<void**>(at) = FindSoundAlias(value);
			break;
		case 15:
			*reinterpret_cast<void**>(at) = reinterpret_cast<void*(*)(const char*)>(Utils::Hook::Rebase(DB_FindTracer))(value.data());
			break;
		case 16:
			writeEnum(weaponTypeNames, std::size(weaponTypeNames), "weapon type");
			break;
		case 17:
			writeEnum(weaponClassNames, std::size(weaponClassNames), "weapon class");
			break;
		case 18:
			writeEnum(overlayReticleNames, std::size(overlayReticleNames), "overlay reticle");
			break;
		case 19:
			writeEnum(penetrateTypeNames, std::size(penetrateTypeNames), "penetration type");
			break;
		case 20:
			writeEnum(impactTypeNames, std::size(impactTypeNames), "impact type");
			break;
		case 21:
			writeEnum(stanceNames, std::size(stanceNames), "stance");
			break;
		case 22:
			writeEnum(projExplosionNames, std::size(projExplosionNames), "projectile explosion");
			break;
		case 23:
			writeEnum(offhandClassNames, std::size(offhandClassNames), "offhand class");
			break;
		case 24:
		{
			const int index = PlayerAnimTypeIndex(value);

			if (index < 0)
			{
				Logger::Error("weapon: {} has an unknown playerAnimType '{}'\n", weaponName, value);
				break;
			}

			*reinterpret_cast<int*>(at) = index;
			break;
		}
		case 25:
			writeEnum(activeReticleNames, std::size(activeReticleNames), "active reticle type");
			break;
		case 26:
			writeEnum(guidedMissileNames, std::size(guidedMissileNames), "guided missile type");
			break;
		case 27:
			*reinterpret_cast<void***>(at) = BuildBounceSounds(value);
			break;
		case 28:
			writeEnum(stickinessNames, std::size(stickinessNames), "stickiness");
			break;
		case 29:
			writeEnum(overlayInterfaceNames, std::size(overlayInterfaceNames), "ads overlay interface");
			break;
		case 30:
			writeEnum(inventoryTypeNames, std::size(inventoryTypeNames), "inventory type");
			break;
		case 31:
			writeEnum(fireTypeNames, std::size(fireTypeNames), "fire type");
			break;
		case 32:
			writeEnum(ammoCounterClipNames, std::size(ammoCounterClipNames), "ammo counter clip type");
			break;
		case 33:
		case 34:
		case 35:
		case 36:
		case 37:
			writeEnum(iconRatioNames, std::size(iconRatioNames), "icon ratio");
			break;
		case 38:
		{
			const auto tokens = Tokenize(value);

			for (std::size_t i = 0; i < tokens.size() && i < std::size(full->hideTags); ++i)
			{
				full->hideTags[i] = ScriptString(Utils::String::ToLower(tokens[i]));
			}
			break;
		}
		case 39:
		case 40:
		{
			const auto tokens = Tokenize(value);
			auto* const keys = (field.type == 39) ? full->notetrackSoundMapKeys : full->notetrackRumbleMapKeys;
			auto* const values = (field.type == 39) ? full->notetrackSoundMapValues : full->notetrackRumbleMapValues;

			for (std::size_t i = 0; i + 1 < tokens.size() && i / 2 < std::size(full->notetrackSoundMapKeys); i += 2)
			{
				keys[i / 2] = ScriptString(Utils::String::ToLower(tokens[i]));
				values[i / 2] = ScriptString(Utils::String::ToLower(tokens[i + 1]));
			}
			break;
		}
		default:
			Logger::Error("weapon: {} has field '{}' of unknown type {}\n", weaponName, field.name, field.type);
			break;
		}

		return true;
	}

	static void FinishWeaponDef(Game::WeaponFullDef* full, const char* name)
	{
		Game::WeaponDef* const weapDef = &full->weapDef;
		Game::WeaponCompleteDef* const complete = &full->complete;

		if (_stricmp(name, "defaultweapon_mp") != 0)
		{
			if (!weapDef->viewLastShotEjectEffect)
			{
				weapDef->viewLastShotEjectEffect = weapDef->viewShellEjectEffect;
			}

			if (!weapDef->worldLastShotEjectEffect)
			{
				weapDef->worldLastShotEjectEffect = weapDef->worldShellEjectEffect;
			}

			if (!weapDef->raiseSound)
			{
				weapDef->raiseSound = static_cast<Game::snd_alias_list_t*>(FindSoundAlias("weap_raise"));
			}

			if (!weapDef->putawaySound)
			{
				weapDef->putawaySound = static_cast<Game::snd_alias_list_t*>(FindSoundAlias("weap_putaway"));
			}

			if (!weapDef->pickupSound)
			{
				weapDef->pickupSound = static_cast<Game::snd_alias_list_t*>(FindSoundAlias("weap_pickup"));
			}

			if (!weapDef->ammoPickupSound)
			{
				weapDef->ammoPickupSound = static_cast<Game::snd_alias_list_t*>(FindSoundAlias("weap_ammo_pickup"));
			}

			if (!weapDef->emptyFireSound)
			{
				weapDef->emptyFireSound = static_cast<Game::snd_alias_list_t*>(FindSoundAlias("weap_dryfire_smg_npc"));
			}
		}

		weapDef->fOOPosAnimLength[0] = complete->iAdsTransInTime > 0
			? 1.0f / static_cast<float>(complete->iAdsTransInTime)
			: 1.0f / 300.0f;
		weapDef->fOOPosAnimLength[1] = complete->iAdsTransOutTime > 0
			? 1.0f / static_cast<float>(complete->iAdsTransOutTime)
			: 1.0f / 500.0f;

		if (weapDef->fMaxDamageRange <= 0.0f)
		{
			weapDef->fMaxDamageRange = 999999.0f;
		}

		if (weapDef->fMinDamageRange <= 0.0f)
		{
			weapDef->fMinDamageRange = 999999.12f;
		}

		if (weapDef->enemyCrosshairRange > 15000.0f)
		{
			Logger::Error("weapon: {} has an enemy crosshair range of {}, over the 15000 limit\n", name, weapDef->enemyCrosshairRange);
		}

		if (weapDef->offhandClass && !weapDef->bClipOnly)
		{
			Logger::Error("weapon: {} is an offhand weapon but is not clip only\n", name);
		}

		if (weapDef->sharedAmmo)
		{
			weapDef->szAmmoName = HunkString(Utils::String::ToLower(weapDef->szAmmoName ? weapDef->szAmmoName : ""));
			weapDef->szClipName = HunkString(Utils::String::ToLower(weapDef->szClipName ? weapDef->szClipName : ""));
		}
		else
		{
			weapDef->szAmmoName = "";
			weapDef->szClipName = "";
		}
	}

	Game::WeaponFullDef* Weapon::ParseWeaponFile(const char* name)
	{
		std::unordered_map<std::string, std::string> fields;

		if (!ReadWeaponFile(name, fields))
		{
			return nullptr;
		}

		auto* const full = static_cast<Game::WeaponFullDef*>(HunkAlloc(sizeof(Game::WeaponFullDef)));

		if (!full)
		{
			Logger::Error("weapon: no hunk left for {}\n", name);
			return nullptr;
		}

		full->complete.szInternalName = HunkString(name);
		full->complete.weapDef = &full->weapDef;
		full->complete.hideTags = full->hideTags;
		full->complete.szXAnims = full->szXAnims;
		full->weapDef.gunXModel = full->gunXModel;
		full->weapDef.szXAnimsRightHanded = full->szXAnimsRightHanded;
		full->weapDef.szXAnimsLeftHanded = full->szXAnimsLeftHanded;
		full->weapDef.notetrackSoundMapKeys = full->notetrackSoundMapKeys;
		full->weapDef.notetrackSoundMapValues = full->notetrackSoundMapValues;
		full->weapDef.notetrackRumbleMapKeys = full->notetrackRumbleMapKeys;
		full->weapDef.notetrackRumbleMapValues = full->notetrackRumbleMapValues;
		full->weapDef.worldModel = full->worldModel;
		full->weapDef.parallelBounce = full->parallelBounce;
		full->weapDef.perpendicularBounce = full->perpendicularBounce;
		full->weapDef.locationDamageMultipliers = full->locationDamageMultipliers;

		for (const WeaponField& field : weaponFields)
		{
			if (field.type == 0)
			{
				*reinterpret_cast<const char**>(reinterpret_cast<std::uint8_t*>(full) + field.offset) = "";
			}
		}

		for (const WeaponField& field : weaponFields)
		{
			const auto value = fields.find(Utils::String::ToLower(field.name));

			if (value == fields.end() || value->second.empty())
			{
				continue;
			}

			if (!ApplyField(full, field, value->second, name))
			{
				return nullptr;
			}
		}

		FinishWeaponDef(full, name);

		return full;
	}

	Game::WeaponCompleteDef* Weapon::LoadWeaponCompleteDef(const char* name)
	{
		auto* const parsed = ParseWeaponFile(name);

		if (parsed)
		{
			return &parsed->complete;
		}

		auto* const zone = static_cast<Game::WeaponCompleteDef*>(Game::DB_FindXAssetHeader(ASSET_TYPE_WEAPON, name));
		const bool isDefault = reinterpret_cast<bool(*)(unsigned int, const char*)>(
			Utils::Hook::Rebase(DB_IsXAssetDefault))(ASSET_TYPE_WEAPON, name);

		if (zone && !isDefault)
		{
			return zone;
		}

		if (!zone)
		{
			return nullptr;
		}

		auto* const standIn = static_cast<Game::WeaponCompleteDef*>(HunkAlloc(sizeof(Game::WeaponCompleteDef)));
		auto* const standInDef = static_cast<Game::WeaponDef*>(HunkAlloc(sizeof(Game::WeaponDef)));

		if (!standIn || !standInDef)
		{
			return nullptr;
		}

		std::memcpy(standIn, zone, sizeof(Game::WeaponCompleteDef));
		std::memcpy(standInDef, zone->weapDef, sizeof(Game::WeaponDef));
		standIn->weapDef = standInDef;
		standIn->szInternalName = HunkString(name);
		standIn->szAltWeaponName = "";
		standIn->altWeaponIndex = 0;

		Logger::Print("weapon: {} has no definition anywhere, standing in the default weapon\n", name);

		return standIn;
	}

	enum class LimitArray
	{
		CompleteDefs,
		WeaponDefs,
		AmmoTypes,
		SharedAmmoCaps,
		Clips,
		WeaponInfo,
		WeaponStatic,
		HintMaterials,
		PrimaryForAlt,
		Count,
	};

	enum class SiteBase
	{
		Rip,
		Image,
		Cg,
	};

	struct LimitArrayLayout
	{
		std::uintptr_t address;
		std::size_t oldSize;
		std::size_t newSize;
	};

	struct LimitSite
	{
		std::uintptr_t address;
		std::uint8_t length;
		std::uint8_t operandOffset;
		LimitArray array;
		SiteBase base;
		std::int32_t offset;
	};

	constexpr std::uintptr_t imageBase = 0x140000000;
	constexpr std::uintptr_t cgArray = 0x1404769A0;

	constexpr std::size_t hintMaterialCount = Weapon::BASEGAME_WEAPON_LIMIT + 5;
	constexpr std::size_t newHintMaterialCount = Weapon::WEAPON_LIMIT + 5;

	static const LimitArrayLayout limitArrays[] =
	{
		{ 0x140441330, Weapon::BASEGAME_WEAPON_LIMIT * 8, Weapon::WEAPON_LIMIT * 8 },
		{ 0x140443EF0, Weapon::BASEGAME_WEAPON_LIMIT * 8, Weapon::WEAPON_LIMIT * 8 },
		{ 0x140446CB0, Weapon::BASEGAME_WEAPON_LIMIT * 8, Weapon::WEAPON_LIMIT * 8 },
		{ 0x140449880, Weapon::BASEGAME_WEAPON_LIMIT * 8, Weapon::WEAPON_LIMIT * 8 },
		{ 0x14044C440, Weapon::BASEGAME_WEAPON_LIMIT * 8, Weapon::WEAPON_LIMIT * 8 },
		{ 0x14058B8A0, Weapon::BASEGAME_WEAPON_LIMIT * 0x38, Weapon::WEAPON_LIMIT * 0x38 },
		{ 0x1406B4B20, Weapon::BASEGAME_WEAPON_LIMIT * 24, Weapon::WEAPON_LIMIT * 24 },
		{ 0x14046B3E0, hintMaterialCount * 8, newHintMaterialCount * 8 },
		{ 0x1404EC2CC, Weapon::BASEGAME_WEAPON_LIMIT * 2, Weapon::WEAPON_LIMIT * 2 },
	};

	static_assert(std::size(limitArrays) == static_cast<std::size_t>(LimitArray::Count));

	static const LimitSite limitSites[] =
	{
		{ 0x14009C276, 7, 3, LimitArray::CompleteDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C28A, 7, 3, LimitArray::WeaponDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C29E, 7, 3, LimitArray::Clips, SiteBase::Rip, 0x0 },
		{ 0x14009C2B2, 7, 3, LimitArray::SharedAmmoCaps, SiteBase::Rip, 0x0 },
		{ 0x14009C2C6, 7, 3, LimitArray::AmmoTypes, SiteBase::Rip, 0x0 },
		{ 0x14009C2DD, 7, 3, LimitArray::CompleteDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C2FC, 7, 3, LimitArray::WeaponDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C303, 7, 3, LimitArray::AmmoTypes, SiteBase::Rip, 0x0 },
		{ 0x14009C30A, 7, 3, LimitArray::SharedAmmoCaps, SiteBase::Rip, 0x0 },
		{ 0x14009C311, 7, 3, LimitArray::Clips, SiteBase::Rip, 0x0 },
		{ 0x14009C338, 7, 3, LimitArray::CompleteDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C35C, 7, 3, LimitArray::WeaponDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C3C0, 8, 4, LimitArray::CompleteDefs, SiteBase::Image, 0x8 },
		{ 0x14009C3CC, 8, 4, LimitArray::WeaponDefs, SiteBase::Image, 0x8 },
		{ 0x14009C3F0, 8, 4, LimitArray::AmmoTypes, SiteBase::Image, 0x0 },
		{ 0x14009C450, 8, 4, LimitArray::AmmoTypes, SiteBase::Image, 0x0 },
		{ 0x14009C45C, 8, 4, LimitArray::AmmoTypes, SiteBase::Image, 0x0 },
		{ 0x14009C473, 8, 4, LimitArray::WeaponDefs, SiteBase::Image, 0x8 },
		{ 0x14009C4A0, 8, 4, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009C501, 8, 4, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009C50D, 8, 4, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009C523, 8, 4, LimitArray::WeaponDefs, SiteBase::Image, 0x8 },
		{ 0x14009C544, 8, 4, LimitArray::Clips, SiteBase::Image, 0x0 },
		{ 0x14009C5A1, 8, 4, LimitArray::Clips, SiteBase::Image, 0x0 },
		{ 0x14009C5AE, 8, 4, LimitArray::Clips, SiteBase::Image, 0x0 },
		{ 0x14009C632, 7, 3, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009C670, 8, 4, LimitArray::WeaponDefs, SiteBase::Image, 0x8 },
		{ 0x14009C6B1, 8, 4, LimitArray::CompleteDefs, SiteBase::Image, 0x8 },
		{ 0x14009C6C0, 8, 4, LimitArray::CompleteDefs, SiteBase::Image, 0x8 },
		{ 0x14009C76D, 7, 3, LimitArray::CompleteDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C803, 7, 3, LimitArray::WeaponDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C823, 7, 3, LimitArray::SharedAmmoCaps, SiteBase::Rip, 0x0 },
		{ 0x14009C892, 7, 3, LimitArray::CompleteDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C8A0, 7, 3, LimitArray::WeaponDefs, SiteBase::Rip, 0x0 },
		{ 0x14009C8F3, 7, 3, LimitArray::CompleteDefs, SiteBase::Rip, 0x0 },
		{ 0x14009CA32, 7, 3, LimitArray::CompleteDefs, SiteBase::Rip, 0x0 },
		{ 0x14009CC56, 7, 3, LimitArray::WeaponDefs, SiteBase::Image, 0x0 },
		{ 0x14009CC61, 7, 3, LimitArray::CompleteDefs, SiteBase::Image, 0x0 },
		{ 0x14009CCA8, 8, 4, LimitArray::AmmoTypes, SiteBase::Image, 0x0 },
		{ 0x14009CCF3, 8, 4, LimitArray::AmmoTypes, SiteBase::Image, 0x0 },
		{ 0x14009CCFD, 8, 4, LimitArray::AmmoTypes, SiteBase::Image, 0x0 },
		{ 0x14009CD59, 8, 4, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009CD8C, 8, 4, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009CD9C, 8, 4, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009CDD3, 7, 3, LimitArray::SharedAmmoCaps, SiteBase::Image, 0x0 },
		{ 0x14009CE10, 8, 4, LimitArray::WeaponDefs, SiteBase::Image, 0x8 },
		{ 0x14009CE44, 8, 4, LimitArray::CompleteDefs, SiteBase::Image, 0x8 },
		{ 0x14009CEE5, 8, 4, LimitArray::Clips, SiteBase::Image, 0x0 },
		{ 0x14009CF2E, 8, 4, LimitArray::Clips, SiteBase::Image, 0x0 },
		{ 0x14009CF38, 8, 4, LimitArray::Clips, SiteBase::Image, 0x0 },
		{ 0x1400C058B, 8, 4, LimitArray::WeaponInfo, SiteBase::Image, 0x10 },
		{ 0x1400C100C, 8, 4, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C3D64, 8, 4, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C407B, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x10 },
		{ 0x1400C41E6, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C421C, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C426F, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C4A0C, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400C4A2C, 7, 3, LimitArray::WeaponStatic, SiteBase::Image, 0x0 },
		{ 0x1400C4C1A, 8, 4, LimitArray::HintMaterials, SiteBase::Image, 0x20 },
		{ 0x1400C52CA, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C52D5, 10, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C52E9, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C538F, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C5412, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C7514, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400C7616, 8, 3, LimitArray::WeaponInfo, SiteBase::Image, 0x19 },
		{ 0x1400C7A41, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400C7ACB, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400C7CBF, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400C81AE, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400C838E, 7, 3, LimitArray::WeaponStatic, SiteBase::Rip, 0x0 },
		{ 0x1400C83F7, 7, 3, LimitArray::WeaponStatic, SiteBase::Rip, 0x0 },
		{ 0x1400C849E, 7, 3, LimitArray::WeaponStatic, SiteBase::Rip, 0x0 },
		{ 0x1400C8764, 9, 5, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400C879A, 8, 4, LimitArray::PrimaryForAlt, SiteBase::Cg, 0x0 },
		{ 0x1400D4B5D, 8, 4, LimitArray::WeaponStatic, SiteBase::Image, 0x0 },
		{ 0x1400D6522, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400D654C, 7, 3, LimitArray::WeaponStatic, SiteBase::Rip, 0x0 },
		{ 0x1400D6E3A, 7, 3, LimitArray::PrimaryForAlt, SiteBase::Rip, 0x0 },
		{ 0x1400D9957, 7, 3, LimitArray::HintMaterials, SiteBase::Rip, 0x18 },
		{ 0x1400D996F, 7, 3, LimitArray::HintMaterials, SiteBase::Rip, 0x20 },
		{ 0x1400DA017, 7, 3, LimitArray::PrimaryForAlt, SiteBase::Rip, 0x0 },
		{ 0x1400DA1E8, 7, 3, LimitArray::WeaponStatic, SiteBase::Rip, 0x0 },
		{ 0x1400DA1FA, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400DB12A, 8, 4, LimitArray::HintMaterials, SiteBase::Image, 0x0 },
		{ 0x1400DB1C3, 8, 4, LimitArray::WeaponInfo, SiteBase::Image, 0x20 },
		{ 0x1400DB2AF, 8, 4, LimitArray::WeaponInfo, SiteBase::Image, 0x20 },
		{ 0x1400DCEFE, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400DD082, 7, 3, LimitArray::WeaponInfo, SiteBase::Rip, 0x0 },
		{ 0x1400F6B86, 7, 3, LimitArray::PrimaryForAlt, SiteBase::Rip, 0x0 },
	};

	constexpr std::uintptr_t memsetAddress = 0x140327FA0;
	constexpr std::uintptr_t CG_ClearCgCalls[] = { 0x1400D64DA, 0x1400DA2DC };

	static Utils::Hook clearCgHooks[std::size(CG_ClearCgCalls)];

	static std::uint8_t* movedLimitArrays[static_cast<std::size_t>(LimitArray::Count)]{};

	static std::int64_t EncodeLimitSite(const LimitSite& site, std::uintptr_t array, std::uintptr_t siteAddress, std::uintptr_t image, std::uintptr_t cg)
	{
		const auto target = static_cast<std::int64_t>(array) + site.offset;

		switch (site.base)
		{
		case SiteBase::Rip:
			return target - static_cast<std::int64_t>(siteAddress + site.length);
		case SiteBase::Image:
			return target - static_cast<std::int64_t>(image);
		case SiteBase::Cg:
			return target - static_cast<std::int64_t>(cg);
		}

		return 0;
	}

	static Game::WeaponCompleteDef** WeaponCompleteDefs()
	{
		std::uint8_t* const moved = movedLimitArrays[static_cast<std::size_t>(LimitArray::CompleteDefs)];

		if (moved)
		{
			return reinterpret_cast<Game::WeaponCompleteDef**>(moved);
		}

		return reinterpret_cast<Game::WeaponCompleteDef**>(Utils::Hook::Rebase(bg_weaponCompleteDefs));
	}

	void* Weapon::CG_ClearCg_Hook(void* dest, int value, std::size_t size)
	{
		void* const result = reinterpret_cast<void*(*)(void*, int, std::size_t)>(Utils::Hook::Rebase(memsetAddress))(dest, value, size);

		const LimitArrayLayout& layout = limitArrays[static_cast<std::size_t>(LimitArray::PrimaryForAlt)];
		std::memset(movedLimitArrays[static_cast<std::size_t>(LimitArray::PrimaryForAlt)], value, layout.newSize);

		return result;
	}

	bool Weapon::MoveLimitArrays()
	{
		const std::uintptr_t liveImage = Utils::Hook::Rebase(imageBase);
		const std::uintptr_t liveCg = Utils::Hook::Rebase(cgArray);

		for (const LimitSite& site : limitSites)
		{
			const LimitArrayLayout& layout = limitArrays[static_cast<std::size_t>(site.array)];
			const std::int64_t expected = EncodeLimitSite(site, layout.address, site.address, imageBase, cgArray);
			const auto current = Utils::Hook::Get<std::int32_t>(site.address + site.operandOffset);

			if (current != expected)
			{
				Logger::Error("weapon: 0x{:X} does not read as expected, the weapon arrays were not moved\n", site.address);
				return false;
			}
		}

		for (const std::uintptr_t call : CG_ClearCgCalls)
		{
			if (!Utils::Hook::BranchesTo(call, memsetAddress, HOOK_CALL))
			{
				Logger::Error("weapon: CG_Init or CG_Shutdown does not clear cg as expected, the weapon arrays were not moved\n");
				return false;
			}
		}

		std::size_t blockSize = 0;
		std::size_t offsets[std::size(limitArrays)]{};

		for (std::size_t i = 0; i < std::size(limitArrays); ++i)
		{
			offsets[i] = blockSize;
			blockSize += (limitArrays[i].newSize + 15) & ~static_cast<std::size_t>(15);
		}

		auto* const block = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(BG_SetupWeaponIndex, blockSize));

		if (!block)
		{
			Logger::Error("weapon: no memory free within reach of the image, the weapon arrays were not moved\n");
			return false;
		}

		std::int32_t operands[std::size(limitSites)]{};

		for (std::size_t i = 0; i < std::size(limitSites); ++i)
		{
			const LimitSite& site = limitSites[i];
			const auto array = reinterpret_cast<std::uintptr_t>(block + offsets[static_cast<std::size_t>(site.array)]);
			const std::int64_t encoded = EncodeLimitSite(site, array, Utils::Hook::Rebase(site.address), liveImage, liveCg);

			if (encoded < INT32_MIN || encoded > INT32_MAX)
			{
				VirtualFree(block, 0, MEM_RELEASE);
				Logger::Error("weapon: the moved arrays are out of reach of 0x{:X}, not moved\n", site.address);
				return false;
			}

			operands[i] = static_cast<std::int32_t>(encoded);
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(CG_ClearCgCalls); ++i)
		{
			isSeated = clearCgHooks[i].Initialize(CG_ClearCgCalls[i], reinterpret_cast<void*>(CG_ClearCg_Hook), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (Utils::Hook& hook : clearCgHooks)
			{
				hook.Uninstall();
			}

			VirtualFree(block, 0, MEM_RELEASE);
			Logger::Error("weapon: could not hook the cg clears, the weapon arrays were not moved\n");
			return false;
		}

		for (std::size_t i = 0; i < std::size(limitArrays); ++i)
		{
			movedLimitArrays[i] = block + offsets[i];
			std::memcpy(movedLimitArrays[i], reinterpret_cast<const void*>(Utils::Hook::Rebase(limitArrays[i].address)), limitArrays[i].oldSize);
		}

		for (Utils::Hook& hook : clearCgHooks)
		{
			hook.Quick();
		}

		for (std::size_t i = 0; i < std::size(limitSites); ++i)
		{
			Utils::Hook::Set<std::int32_t>(limitSites[i].address + limitSites[i].operandOffset, operands[i]);
		}

		return true;
	}

	constexpr std::uintptr_t BG_GetMaxPickupableAmmo = 0x14009B4F0;
	constexpr std::uintptr_t BG_GetTotalAmmoReserve = 0x14009B880;

	static const std::uint8_t maxPickupableAmmoEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x4C, 0x24, 0x08 };
	static const std::uint8_t totalAmmoReserveEntry[] = { 0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x55, 0xB8, 0xE0, 0x2B, 0x00, 0x00 };

	static Utils::Hook maxPickupableAmmoHook;
	static Utils::Hook totalAmmoReserveHook;

	static const Game::GlobalAmmo* FindAmmo(const Game::playerState_s* ps, int ammoIndex)
	{
		for (const Game::GlobalAmmo& ammo : ps->weapCommon.ammoNotInClip)
		{
			if (ammo.ammoType == ammoIndex)
			{
				return &ammo;
			}
		}

		return nullptr;
	}

	static const Game::ClipAmmo* FindClip(const Game::playerState_s* ps, int clipIndex)
	{
		for (const Game::ClipAmmo& clip : ps->weapCommon.ammoInClip)
		{
			if (clip.clipIndex == clipIndex)
			{
				return &clip;
			}
		}

		return nullptr;
	}

	int Weapon::BG_GetMaxPickupableAmmo_Hk(Game::playerState_s* ps, unsigned int weapon)
	{
		const Game::WeaponDef* const weapDef = Game::BG_GetWeaponDef(weapon);

		if (weapDef->iSharedAmmoCapIndex >= 0)
		{
			int room = Game::BG_GetSharedAmmoCapSize(weapDef->iSharedAmmoCapIndex);
			std::array<bool, WEAPON_LIMIT> isAmmoCounted{};

			for (const unsigned int held : ps->weaponsEquipped)
			{
				if (!held)
				{
					continue;
				}

				const Game::WeaponDef* const heldDef = Game::BG_GetWeaponDef(held);

				if (heldDef->iSharedAmmoCapIndex != weapDef->iSharedAmmoCapIndex)
				{
					continue;
				}

				if (heldDef->bClipOnly)
				{
					const Game::ClipAmmo* const clip = FindClip(ps, heldDef->iClipIndex);

					if (clip)
					{
						room -= clip->ammoCount[0];
					}

					const Game::PlayerEquippedWeaponState* const state = Game::BG_GetEquippedWeaponState(ps, held);

					if (state && state->dualWielding && clip)
					{
						room -= clip->ammoCount[1];
					}

					continue;
				}

				if (isAmmoCounted[heldDef->iAmmoIndex])
				{
					continue;
				}

				isAmmoCounted[heldDef->iAmmoIndex] = true;

				const Game::GlobalAmmo* const ammo = FindAmmo(ps, heldDef->iAmmoIndex);

				if (ammo)
				{
					room -= ammo->ammoCount;
				}
			}

			return room;
		}

		if (weapDef->bClipOnly)
		{
			const int clipSize = Game::BG_GetClipSize(ps, weapon);
			const Game::ClipAmmo* const clip = FindClip(ps, weapDef->iClipIndex);

			int room = clipSize;

			if (clip)
			{
				room -= clip->ammoCount[0];
			}

			if (ps->weapCommon.lastWeaponHand == 1)
			{
				room += clipSize;

				if (clip)
				{
					room -= clip->ammoCount[1];
				}
			}

			return room;
		}

		int playerMax = 0;

		for (const unsigned int held : ps->weaponsEquipped)
		{
			if (!held)
			{
				continue;
			}

			const Game::WeaponDef* const heldDef = Game::BG_GetWeaponDef(held);

			if (heldDef->iAmmoIndex != weapDef->iAmmoIndex)
			{
				continue;
			}

			const Game::WeaponCompleteDef* const completeDef = Game::BG_GetWeaponCompleteDef(weapon);

			if (heldDef->inventoryType == Game::WEAPINVENTORY_ALTMODE && completeDef->altWeaponIndex == held)
			{
				continue;
			}

			if (heldDef->iSharedAmmoCapIndex >= 0)
			{
				playerMax = Game::BG_GetSharedAmmoCapSize(heldDef->iSharedAmmoCapIndex);
				break;
			}

			playerMax += weapDef->iMaxAmmo;
		}

		const Game::GlobalAmmo* const ammo = FindAmmo(ps, weapDef->iAmmoIndex);

		if (ammo)
		{
			playerMax -= ammo->ammoCount;
		}

		return playerMax;
	}

	int Weapon::BG_GetTotalAmmoReserve_Hk(const Game::playerState_s* ps, unsigned int weapon)
	{
		const Game::WeaponDef* const weapDef = Game::BG_GetWeaponDef(weapon);

		if (weapDef->iSharedAmmoCapIndex < 0)
		{
			if (weapDef->bClipOnly)
			{
				const Game::ClipAmmo* const clip = FindClip(ps, weapDef->iClipIndex);

				if (!clip)
				{
					return 0;
				}

				return clip->ammoCount[0] + clip->ammoCount[1];
			}

			const Game::GlobalAmmo* const ammo = FindAmmo(ps, weapDef->iAmmoIndex);

			if (!ammo)
			{
				return 0;
			}

			return ammo->ammoCount;
		}

		int total = 0;
		std::array<bool, WEAPON_LIMIT> isClipCounted{};
		std::array<bool, WEAPON_LIMIT> isAmmoCounted{};

		for (const unsigned int held : ps->weaponsEquipped)
		{
			if (!held)
			{
				continue;
			}

			const Game::WeaponDef* const heldDef = Game::BG_GetWeaponDef(held);

			if (heldDef->iSharedAmmoCapIndex != weapDef->iSharedAmmoCapIndex)
			{
				continue;
			}

			if (heldDef->bClipOnly)
			{
				if (isClipCounted[heldDef->iClipIndex])
				{
					continue;
				}

				isClipCounted[heldDef->iClipIndex] = true;

				const Game::ClipAmmo* const clip = FindClip(ps, heldDef->iClipIndex);

				if (clip)
				{
					total += clip->ammoCount[0] + clip->ammoCount[1];
				}

				continue;
			}

			if (isAmmoCounted[heldDef->iAmmoIndex])
			{
				continue;
			}

			isAmmoCounted[heldDef->iAmmoIndex] = true;

			const Game::GlobalAmmo* const ammo = FindAmmo(ps, heldDef->iAmmoIndex);

			if (ammo)
			{
				total += ammo->ammoCount;
			}
		}

		return total;
	}

	bool Weapon::RebuildAmmoCounts()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(BG_GetMaxPickupableAmmo, maxPickupableAmmoEntry, sizeof(maxPickupableAmmoEntry))
			&& Utils::Hook::MatchesBytes(BG_GetTotalAmmoReserve, totalAmmoReserveEntry, sizeof(totalAmmoReserveEntry));

		if (!isExpected)
		{
			Logger::Error("weapon: the ammo count functions do not read as expected, the weapon limit stays at 1400\n");
			return false;
		}

		bool isSeated = maxPickupableAmmoHook.Initialize(BG_GetMaxPickupableAmmo, reinterpret_cast<void*>(BG_GetMaxPickupableAmmo_Hk), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = totalAmmoReserveHook.Initialize(BG_GetTotalAmmoReserve, reinterpret_cast<void*>(BG_GetTotalAmmoReserve_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			maxPickupableAmmoHook.Uninstall();
			totalAmmoReserveHook.Uninstall();
			Logger::Error("weapon: could not hook the ammo count functions, the weapon limit stays at 1400\n");
			return false;
		}

		maxPickupableAmmoHook.Quick();
		totalAmmoReserveHook.Quick();
		return true;
	}

	struct LimitImmediate
	{
		std::uintptr_t address;
		std::uint8_t operandOffset;
		std::uint32_t expected;
		std::uint32_t replacement;
	};

	static const LimitImmediate limitImmediates[] =
	{
		{ 0x1400891BB, 2, 0x578, 0x960 },
		{ 0x14008A3F0, 2, 0x578, 0x960 },
		{ 0x14008AB44, 2, 0x578, 0x960 },
		{ 0x14008D693, 2, 0x578, 0x960 },
		{ 0x1400D551B, 2, 0x578, 0x960 },
		{ 0x1400E33B4, 2, 0x578, 0x960 },
		{ 0x1400E33E2, 2, 0x578, 0x960 },
		{ 0x1400E3498, 2, 0x578, 0x960 },
		{ 0x14016C36E, 2, 0x578, 0x960 },
		{ 0x14016C5AB, 3, 0x578, 0x960 },
		{ 0x14016D588, 2, 0x578, 0x960 },
		{ 0x14016D7C4, 2, 0x578, 0x960 },
		{ 0x14016DE8A, 2, 0x578, 0x960 },
		{ 0x14016DF42, 2, 0x578, 0x960 },
		{ 0x14016E024, 2, 0x578, 0x960 },
		{ 0x14016E843, 3, 0x578, 0x960 },
		{ 0x14016EDFC, 3, 0x578, 0x960 },
		{ 0x14016EF44, 2, 0x578, 0x960 },
		{ 0x14016F0A4, 2, 0x578, 0x960 },
		{ 0x14017DA7F, 2, 0x578, 0x960 },
		{ 0x14018C15B, 2, 0x578, 0x960 },
		{ 0x14018C320, 2, 0x578, 0x960 },
		{ 0x14018C36F, 2, 0x578, 0x960 },
		{ 0x140194FEF, 2, 0x578, 0x960 },
		{ 0x14019506B, 2, 0x578, 0x960 },
		{ 0x1400AEFDC, 2, 0x578, 0x960 },
		{ 0x1400AEFF5, 2, 0xFFFFFA80, 0xFFFFF698 },
		{ 0x1400D695E, 2, 0x578, 0x960 },
		{ 0x1400C49BA, 1, 0x578, 0x960 },
		{ 0x1401A39EC, 1, 0x578, 0x960 },
		{ 0x1400DB150, 1, 0x577, 0x95F },
		{ 0x1400AD449, 1, 0x576, 0x95E },
		{ 0x14009C27D, 2, 0x2BC0, 0x4B00 },
		{ 0x14009C291, 2, 0x2BC0, 0x4B00 },
		{ 0x14009C2A5, 2, 0x2BC0, 0x4B00 },
		{ 0x14009C2B9, 2, 0x2BC0, 0x4B00 },
		{ 0x14009C2CD, 2, 0x2BC0, 0x4B00 },
		{ 0x1400D652B, 3, 0x13240, 0x20D00 },
		{ 0x1400D6532, 2, 0x13240, 0x20D00 },
		{ 0x1400D6553, 2, 0x8340, 0xE100 },
		{ 0x1400DA1EF, 2, 0x8340, 0xE100 },
		{ 0x1400DA203, 3, 0x13240, 0x20D00 },
		{ 0x1400DA20A, 2, 0x13240, 0x20D00 },
		{ 0x1400891AA, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14008AB33, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x1400D5508, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x1400D552D, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x1400E33A3, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x1400E33D0, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x1400E347B, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016C349, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016D572, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016D7B3, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016DE79, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016DF29, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016E013, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016ECC9, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016EDE4, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016EF33, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016EF6B, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016F093, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14016F0D3, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14017DA67, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14018C14A, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14018C309, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x14018C35E, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x140194FDE, 1, 0x5D9F7391, 0x1B4E81B5 },
		{ 0x140195054, 1, 0x5D9F7391, 0x1B4E81B5 },
	};

	struct LimitShift
	{
		std::uintptr_t address;
		std::uint8_t operandOffset;
	};

	static const LimitShift limitShifts[] =
	{
		{ 0x1400891B1, 2 }, { 0x14008AB3A, 2 }, { 0x1400D5511, 2 }, { 0x1400D553B, 2 }, { 0x1400E33AA, 2 },
		{ 0x1400E33D8, 2 }, { 0x1400E348E, 2 }, { 0x14016C364, 2 }, { 0x14016D57E, 2 }, { 0x14016D7BA, 2 },
		{ 0x14016DE80, 2 }, { 0x14016DF38, 2 }, { 0x14016E01A, 2 }, { 0x14016ECEE, 2 }, { 0x14016EDEF, 3 },
		{ 0x14016EF3A, 2 }, { 0x14016EF7A, 2 }, { 0x14016F09A, 2 }, { 0x14016F0DB, 2 }, { 0x14017DA75, 2 },
		{ 0x14018C151, 2 }, { 0x14018C316, 2 }, { 0x14018C365, 2 }, { 0x140194FE5, 2 }, { 0x140195061, 2 },
	};

	constexpr std::uint8_t divideShift = 9;
	constexpr std::uint8_t newDivideShift = 8;

	bool Weapon::RaiseLimit()
	{
		for (const LimitImmediate& immediate : limitImmediates)
		{
			if (Utils::Hook::Get<std::uint32_t>(immediate.address + immediate.operandOffset) != immediate.expected)
			{
				Logger::Error("weapon: 0x{:X} does not read as expected, the weapon limit stays at 1400\n", immediate.address);
				return false;
			}
		}

		for (const LimitShift& shift : limitShifts)
		{
			if (Utils::Hook::Get<std::uint8_t>(shift.address + shift.operandOffset) != divideShift)
			{
				Logger::Error("weapon: 0x{:X} does not read as expected, the weapon limit stays at 1400\n", shift.address);
				return false;
			}
		}

		for (const LimitImmediate& immediate : limitImmediates)
		{
			Utils::Hook::Set<std::uint32_t>(immediate.address + immediate.operandOffset, immediate.replacement);
		}

		for (const LimitShift& shift : limitShifts)
		{
			Utils::Hook::Set<std::uint8_t>(shift.address + shift.operandOffset, newDivideShift);
		}

		weaponSlotCount = WEAPON_LIMIT;
		return true;
	}

	bool Weapon::IsLimitRaised()
	{
		return weaponSlotCount == WEAPON_LIMIT;
	}

	unsigned int Weapon::BG_GetWeaponIndexForName_Hook(const char* name, void(*registerCallback)(unsigned int))
	{
		if (!name || !*name || _stricmp(name, "none") == 0)
		{
			return 0;
		}

		const auto findWeaponIndex = reinterpret_cast<unsigned int(*)(const char*)>(Utils::Hook::Rebase(BG_FindWeaponIndexForName));
		const unsigned int found = findWeaponIndex(name);

		if (found)
		{
			return found;
		}

		auto* const def = LoadWeaponCompleteDef(name);

		if (!def)
		{
			return 0;
		}

		auto* const lastParsed = reinterpret_cast<unsigned int*>(Utils::Hook::Rebase(bg_lastParsedWeaponIndex));

		if (*lastParsed + 1 >= weaponSlotCount)
		{
			Logger::Error("weapon: no slot left for {}, all {} are taken\n", name, weaponSlotCount);
			return 0;
		}

		const unsigned int index = ++*lastParsed;
		WeaponCompleteDefs()[index] = def;
		reinterpret_cast<void(*)(unsigned int)>(Utils::Hook::Rebase(BG_SetupWeaponIndex))(index);

		def->altWeaponIndex = 0;

		if (def->szAltWeaponName && *def->szAltWeaponName)
		{
			const unsigned int alt = BG_GetWeaponIndexForName_Hook(def->szAltWeaponName, registerCallback);

			if (!alt)
			{
				Logger::Error("weapon: {} wants the alt weapon {}, which did not load\n", name, def->szAltWeaponName);
			}

			def->altWeaponIndex = alt;
		}

		if (registerCallback)
		{
			registerCallback(index);
		}

		return index;
	}

	Weapon::Weapon()
	{
		if (!Flags::HasFlag("steamdemo") && !Flags::HasFlag("retaildemo"))
		{
			if (MoveLimitArrays() && RebuildAmmoCounts())
			{
				RaiseLimit();
			}
		}

		if (!weaponIndexHook.Initialize(BG_GetWeaponIndexForName, reinterpret_cast<void*>(BG_GetWeaponIndexForName_Hook), HOOK_JUMP)
			->Install()->IsInstalled())
		{
			Logger::Error("weapon: could not hook BG_GetWeaponIndexForName, so IW4x's own weapons cannot load\n");
			return;
		}

		weaponIndexHook.Quick();

		if (IsLimitRaised())
		{
			Game::ReallocateAssetPool(Game::ASSET_TYPE_WEAPON, WEAPON_LIMIT);
		}

		const bool isRegisterExpected = Utils::Hook::MatchesBytes(G_GetWeaponIndexForName_Initializing, weaponInitializingJump, sizeof(weaponInitializingJump))
			&& Utils::Hook::MatchesBytes(G_ModelIndex_Initializing, modelInitializingJump, sizeof(modelInitializingJump));

		if (isRegisterExpected)
		{
			Utils::Hook::Nop(G_GetWeaponIndexForName_Initializing, sizeof(weaponInitializingJump));
			Utils::Hook::Set<std::uint8_t>(G_ModelIndex_Initializing, 0xEB);
		}
		else
		{
			Logger::Error("weapon: G_GetWeaponIndexForName or G_ModelIndex is not the expected code, weapons and models only register while a level loads\n");
		}

		const bool isNoneWeaponExpected = Utils::Hook::MatchesBytes(BG_LoadDefaultWeaponCompleteDef, loadDefaultWeapon, sizeof(loadDefaultWeapon))
			&& Utils::Hook::BranchesTo(BG_LoadDefaultWeaponCompleteDef_FindCall, DB_FindXAssetHeader, HOOK_JUMP)
			&& Utils::Hook::BranchesTo(BG_LoadPlayerAnimTypesCall, BG_LoadPlayerAnimTypes, HOOK_CALL);

		if (!isNoneWeaponExpected)
		{
			Logger::Error("weapon: BG_ClearWeaponDef is not the expected code, the player anim types still load after the default weapon\n");
		}
		else if (!noneWeaponHook.Initialize(BG_LoadDefaultWeaponCompleteDef, reinterpret_cast<void*>(LoadNoneWeaponHook), HOOK_JUMP)->Install()->IsInstalled())
		{
			Logger::Error("weapon: could not hook BG_LoadDefaultWeaponCompleteDef, the player anim types still load after the default weapon\n");
		}
		else
		{
			noneWeaponHook.Quick();
			Utils::Hook::Nop(BG_LoadPlayerAnimTypesCall, 5);
		}

		Events::OnDvarInit([]
		{
			cg_recoilMultiplier = Dvar::Register("cg_recoilMultiplier", 1.0f, 0.0f, 1000.0f, Game::DVAR_CHEAT,
				"The scale applied to the player recoil when firing");
			bg_disableDoubleTaps = Dvar::Register("bg_disableDoubleTaps", false, Game::DVAR_CODINFO,
				"Enables MW3-style weapon swapping mechanics");
		});

		AddScriptMethods();

		bool isWeaponSeated = true;

		for (std::size_t i = 0; i < std::size(PM_WeaponCalls); ++i)
		{
			if (!Utils::Hook::BranchesTo(PM_WeaponCalls[i], PM_Weapon, HOOK_CALL))
			{
				isWeaponSeated = false;
				break;
			}
		}

		if (isWeaponSeated)
		{
			for (std::size_t i = 0; i < std::size(PM_WeaponCalls); ++i)
			{
				isWeaponSeated = weaponHooks[i].Initialize(PM_WeaponCalls[i], reinterpret_cast<void*>(PM_Weapon_Stub), HOOK_CALL)
					->Install()->IsInstalled() && isWeaponSeated;
			}
		}

		if (isWeaponSeated)
		{
			for (auto& hook : weaponHooks)
			{
				hook.Quick();
			}
		}
		else
		{
			for (auto& hook : weaponHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("weapon: could not hook PmoveSingle's PM_Weapon calls, bg_disableDoubleTaps does nothing\n");
		}

		const bool isSelectExpected = Utils::Hook::BranchesTo(CG_SelectWeaponIndex_GetDefCall, BG_GetWeaponDef, HOOK_CALL)
			&& Utils::Hook::MatchesBytes(CG_SelectWeaponIndex_Fail, selectFail, sizeof(selectFail));

		if (isSelectExpected)
		{
			Weapon_BG_GetWeaponDef = Utils::Hook::Rebase(BG_GetWeaponDef);
			Weapon_CG_SelectWeaponIndexFail = Utils::Hook::Rebase(CG_SelectWeaponIndex_Fail);

			if (selectWeaponHook.Initialize(CG_SelectWeaponIndex_GetDefCall, CG_SelectWeaponIndexStub, HOOK_CALL)->Install()->IsInstalled())
			{
				selectWeaponHook.Quick();
			}
			else
			{
				Logger::Error("weapon: could not seat the CG_SelectWeaponIndex guard, selecting a weapon with no def still crashes\n");
			}
		}
		else
		{
			Logger::Error("weapon: CG_SelectWeaponIndex is not the expected code, selecting a weapon with no def still crashes\n");
		}

		bool isGrabbedSeated = true;

		for (const std::uintptr_t site : WeaponEntCanBeGrabbedCalls)
		{
			if (!Utils::Hook::BranchesTo(site, WeaponEntCanBeGrabbed, HOOK_CALL))
			{
				isGrabbedSeated = false;
				break;
			}
		}

		if (isGrabbedSeated)
		{
			for (std::size_t i = 0; i < std::size(WeaponEntCanBeGrabbedCalls); ++i)
			{
				isGrabbedSeated = grabbedHooks[i].Initialize(WeaponEntCanBeGrabbedCalls[i], reinterpret_cast<void*>(WeaponEntCanBeGrabbed_Stub), HOOK_CALL)
					->Install()->IsInstalled() && isGrabbedSeated;
			}
		}

		if (isGrabbedSeated)
		{
			for (auto& hook : grabbedHooks)
			{
				hook.Quick();
			}
		}
		else
		{
			for (auto& hook : grabbedHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("weapon: could not hook BG_CanItemBeGrabbed's WeaponEntCanBeGrabbed calls, DisableWeaponPickup does nothing\n");
		}

		for (const std::uintptr_t site : BG_WeaponFireRecoilCalls)
		{
			if (!Utils::Hook::BranchesTo(site, BG_WeaponFireRecoil, HOOK_CALL))
			{
				Logger::Error("weapon: 0x{:X} no longer calls BG_WeaponFireRecoil, cg_recoilMultiplier does nothing\n", site);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(BG_WeaponFireRecoilCalls); ++i)
		{
			isSeated = recoilHooks[i].Initialize(BG_WeaponFireRecoilCalls[i], reinterpret_cast<void*>(BG_WeaponFireRecoil_Stub), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : recoilHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("weapon: could not seat the recoil hooks, cg_recoilMultiplier does nothing\n");
			return;
		}

		for (auto& hook : recoilHooks)
		{
			hook.Quick();
		}
	}
}
