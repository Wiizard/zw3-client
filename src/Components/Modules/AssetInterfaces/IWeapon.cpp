#include "STDInclude.hpp"

#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include <Utils/JSON.hpp>

#include "../AssetHandler.hpp"
#include "../Command.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"
#include "../Weapon.hpp"
#include "IWeapon.hpp"

namespace Assets
{
	constexpr int weaponJsonVersion = 1;
	constexpr std::size_t hideTagCount = 32;
	constexpr std::size_t animCount = 37;
	constexpr std::size_t modelCount = 16;
	constexpr std::size_t noteTrackCount = 16;
	constexpr std::size_t bounceSoundCount = 31;
	constexpr std::size_t bounceCount = 31;
	constexpr std::size_t damageLocationCount = 20;
	constexpr int ammoCounterClipAltWeapon = 6;

	static_assert(offsetof(Game::WeaponCompleteDef, szDisplayName) == 16);
	static_assert(offsetof(Game::WeaponCompleteDef, hideTags) == 24);
	static_assert(offsetof(Game::WeaponCompleteDef, szXAnims) == 32);
	static_assert(offsetof(Game::WeaponCompleteDef, szAltWeaponName) == 80);
	static_assert(offsetof(Game::WeaponCompleteDef, killIcon) == 96);
	static_assert(offsetof(Game::WeaponCompleteDef, dpadIcon) == 104);
	static_assert(offsetof(Game::WeaponCompleteDef, accuracyGraphKnotCount) == 132);
	static_assert(offsetof(Game::WeaponCompleteDef, accuracyGraphKnots) == 136);

	static_assert(offsetof(Game::WeaponDef, gunXModel) == 8);
	static_assert(offsetof(Game::WeaponDef, handXModel) == 16);
	static_assert(offsetof(Game::WeaponDef, szXAnimsRightHanded) == 24);
	static_assert(offsetof(Game::WeaponDef, szXAnimsLeftHanded) == 32);
	static_assert(offsetof(Game::WeaponDef, szModeName) == 40);
	static_assert(offsetof(Game::WeaponDef, notetrackSoundMapKeys) == 48);
	static_assert(offsetof(Game::WeaponDef, notetrackRumbleMapValues) == 72);
	static_assert(offsetof(Game::WeaponDef, viewFlashEffect) == 112);
	static_assert(offsetof(Game::WeaponDef, pickupSound) == 128);
	static_assert(offsetof(Game::WeaponDef, scanSound) == 496);
	static_assert(offsetof(Game::WeaponDef, bounceSound) == 504);
	static_assert(offsetof(Game::WeaponDef, viewShellEjectEffect) == 512);
	static_assert(offsetof(Game::WeaponDef, reticleSide) == 552);
	static_assert(offsetof(Game::WeaponDef, worldModel) == 736);
	static_assert(offsetof(Game::WeaponDef, worldKnifeModel) == 768);
	static_assert(offsetof(Game::WeaponDef, hudIcon) == 776);
	static_assert(offsetof(Game::WeaponDef, pickupIcon) == 792);
	static_assert(offsetof(Game::WeaponDef, ammoCounterIcon) == 808);
	static_assert(offsetof(Game::WeaponDef, szAmmoName) == 832);
	static_assert(offsetof(Game::WeaponDef, szClipName) == 848);
	static_assert(offsetof(Game::WeaponDef, szSharedAmmoCapName) == 872);
	static_assert(offsetof(Game::WeaponDef, overlayMaterial) == 1104);
	static_assert(offsetof(Game::WeaponDef, overlayMaterialEMPLowRes) == 1128);
	static_assert(offsetof(Game::WeaponDef, physCollmap) == 1312);
	static_assert(offsetof(Game::WeaponDef, projectileModel) == 1408);
	static_assert(offsetof(Game::WeaponDef, projExplosionEffect) == 1424);
	static_assert(offsetof(Game::WeaponDef, projDudSound) == 1448);
	static_assert(offsetof(Game::WeaponDef, parallelBounce) == 1472);
	static_assert(offsetof(Game::WeaponDef, perpendicularBounce) == 1480);
	static_assert(offsetof(Game::WeaponDef, projBeaconEffect) == 1496);
	static_assert(offsetof(Game::WeaponDef, projIgnitionEffect) == 1528);
	static_assert(offsetof(Game::WeaponDef, projIgnitionSound) == 1536);
	static_assert(offsetof(Game::WeaponDef, accuracyGraphName) == 1696);
	static_assert(offsetof(Game::WeaponDef, originalAccuracyGraphKnots) == 1712);
	static_assert(offsetof(Game::WeaponDef, szUseHintString) == 1808);
	static_assert(offsetof(Game::WeaponDef, szScript) == 1856);
	static_assert(offsetof(Game::WeaponDef, locationDamageMultipliers) == 1904);
	static_assert(offsetof(Game::WeaponDef, meleeImpactRumble) == 1920);
	static_assert(offsetof(Game::WeaponDef, tracerType) == 1928);
	static_assert(offsetof(Game::WeaponDef, turretOverheatSound) == 1960);
	static_assert(offsetof(Game::WeaponDef, turretBarrelSpinRumble) == 1976);
	static_assert(offsetof(Game::WeaponDef, turretBarrelSpinMaxSnd) == 2000);
	static_assert(offsetof(Game::WeaponDef, turretBarrelSpinUpSnd) == 2008);
	static_assert(offsetof(Game::WeaponDef, turretBarrelSpinDownSnd) == 2040);
	static_assert(offsetof(Game::WeaponDef, missileConeSoundAliasAtBase) == 2080);

	static const char* const weapTypeNames[] = { "WEAPTYPE_BULLET", "WEAPTYPE_GRENADE", "WEAPTYPE_PROJECTILE", "WEAPTYPE_RIOTSHIELD" };
	static const char* const weapClassNames[] = { "WEAPCLASS_RIFLE", "WEAPCLASS_SNIPER", "WEAPCLASS_MG", "WEAPCLASS_SMG", "WEAPCLASS_SPREAD", "WEAPCLASS_PISTOL", "WEAPCLASS_GRENADE", "WEAPCLASS_ROCKETLAUNCHER", "WEAPCLASS_TURRET", "WEAPCLASS_THROWINGKNIFE", "WEAPCLASS_NON_PLAYER", "WEAPCLASS_ITEM", "WEAPCLASS_NUM" };
	static const char* const penetrateTypeNames[] = { "PENETRATE_TYPE_NONE", "PENETRATE_TYPE_SMALL", "PENETRATE_TYPE_MEDIUM", "PENETRATE_TYPE_LARGE", "PENETRATE_TYPE_COUNT" };
	static const char* const inventoryTypeNames[] = { "WEAPINVENTORY_PRIMARY", "WEAPINVENTORY_OFFHAND", "WEAPINVENTORY_ITEM", "WEAPINVENTORY_ALTMODE", "WEAPINVENTORY_EXCLUSIVE", "WEAPINVENTORY_SCAVENGER", "WEAPINVENTORYCOUN" };
	static const char* const fireTypeNames[] = { "WEAPON_FIRETYPE_FULLAUTO", "WEAPON_FIRETYPE_SINGLESHOT", "WEAPON_FIRETYPE_BURSTFIRE2", "WEAPON_FIRETYPE_BURSTFIRE3", "WEAPON_FIRETYPE_BURSTFIRE4", "WEAPON_FIRETYPE_DOUBLEBARREL", "WEAPON_FIRETYPECOUNT" };
	static const char* const offhandClassNames[] = { "OFFHAND_CLASS_NONE", "OFFHAND_CLASS_FRAG_GRENADE", "OFFHAND_CLASS_SMOKE_GRENADE", "OFFHAND_CLASS_FLASH_GRENADE", "OFFHAND_CLASS_THROWINGKNIFE", "OFFHAND_CLASS_OTHER", "OFFHAND_CLASS_COUNT" };
	static const char* const stanceNames[] = { "WEAPSTANCE_STAND", "WEAPSTANCE_DUCK", "WEAPSTANCE_PRONE", "WEAPSTANCE_NUM" };
	static const char* const iconRatioNames[] = { "WEAPON_ICON_RATIO_1TO1", "WEAPON_ICON_RATIO_2TO1", "WEAPON_ICON_RATIO_4TO1" };
	static const char* const activeReticleNames[] = { "VEH_ACTIVE_RETICLE_NONE", "VEH_ACTIVE_RETICLE_PIP_ON_A_STICK", "VEH_ACTIVE_RETICLE_BOUNCING_DIAMOND", "VEH_ACTIVE_RETICLE_COUNT" };
	static const char* const ammoCounterClipNames[] = { "AMMO_COUNTER_CLIP_NONE", "AMMO_COUNTER_CLIP_MAGAZINE", "AMMO_COUNTER_CLIP_SHORTMAGAZINE", "AMMO_COUNTER_CLIP_SHOTGUN", "AMMO_COUNTER_CLIP_ROCKET", "AMMO_COUNTER_CLIP_BELTFED", "AMMO_COUNTER_CLIP_ALTWEAPON", "AMMO_COUNTER_CLIP_COUNT" };
	static const char* const projExplosionNames[] = { "WEAPPROJEXP_GRENADE", "WEAPPROJEXP_ROCKET", "WEAPPROJEXP_FLASHBANG", "WEAPPROJEXP_NONE", "WEAPPROJEXP_DUD", "WEAPPROJEXP_SMOKE", "WEAPPROJEXP_HEAVY", "WEAPPROJEXP_NUM" };
	static const char* const guidedMissileNames[] = { "MISSILE_GUIDANCE_NONE", "MISSILE_GUIDANCE_SIDEWINDER", "MISSILE_GUIDANCE_HELLFIRE", "MISSILE_GUIDANCE_JAVELIN", "MISSILE_GUIDANCE_COUNT" };
	static const char* const overlayReticleNames[] = { "WEAPOVERLAYRETICLE_NONE", "WEAPOVERLAYRETICLE_CROSSHAIR" };
	static const char* const overlayInterfaceNames[] = { "WEAPOVERLAYINTERFACE_NONE", "WEAPOVERLAYINTERFACE_JAVELIN", "WEAPOVERLAYINTERFACE_TURRETSCOPE" };
	static const char* const stickinessNames[] = { "WEAPSTICKINESS_NONE", "WEAPSTICKINESS_ALL", "WEAPSTICKINESS_ALL_ORIENT", "WEAPSTICKINESS_GROUND", "WEAPSTICKINESS_GROUND_WITH_YAW", "WEAPSTICKINESS_KNIFE", "WEAPSTICKINESS_COUNT" };

	static const char* const animNames[] =
	{
		"WEAP_ANIM_ROOT", "WEAP_ANIM_IDLE", "WEAP_ANIM_EMPTY_IDLE", "WEAP_ANIM_FIRE", "WEAP_ANIM_HOLD_FIRE", "WEAP_ANIM_LASTSHOT",
		"WEAP_ANIM_RECHAMBER", "WEAP_ANIM_MELEE", "WEAP_ANIM_MELEE_CHARGE", "WEAP_ANIM_RELOAD", "WEAP_ANIM_RELOAD_EMPTY",
		"WEAP_ANIM_RELOAD_START", "WEAP_ANIM_RELOAD_END", "WEAP_ANIM_RAISE", "WEAP_ANIM_FIRST_RAISE", "WEAP_ANIM_BREACH_RAISE",
		"WEAP_ANIM_DROP", "WEAP_ANIM_ALT_RAISE", "WEAP_ANIM_ALT_DROP", "WEAP_ANIM_QUICK_RAISE", "WEAP_ANIM_QUICK_DROP",
		"WEAP_ANIM_EMPTY_RAISE", "WEAP_ANIM_EMPTY_DROP", "WEAP_ANIM_SPRINT_IN", "WEAP_ANIM_SPRINT_LOOP", "WEAP_ANIM_SPRINT_OUT",
		"WEAP_ANIM_STUNNED_START", "WEAP_ANIM_STUNNED_LOOP", "WEAP_ANIM_STUNNED_END", "WEAP_ANIM_DETONATE",
		"WEAP_ANIM_NIGHTVISION_WEAR", "WEAP_ANIM_NIGHTVISION_REMOVE", "WEAP_ANIM_ADS_FIRE", "WEAP_ANIM_ADS_LASTSHOT",
		"WEAP_ANIM_ADS_RECHAMBER", "WEAP_ANIM_ADS_UP", "WEAP_ANIM_ADS_DOWN",
	};

	static_assert(std::size(animNames) == animCount);

	static const char* const weaponAttachments[] =
	{
		"acog", "akimbo", "eotech", "fmj", "gl", "grip", "heartbeat", "reflex", "rof", "scope", "shotgun", "silencer", "tactical", "thermal", "xmags",
	};

	enum class FieldKind
	{
		Int,
		Float,
		Bool,
		Enum,
		String,
		Asset,
		FloatArray,
		FloatBuffer,
		ScriptStrings,
		Anims,
		XModels,
		SoundBuffer,
		SoundArray,
		AccuracyGraphs,
		OriginalAccuracyGraphs,
	};

	struct JsonField
	{
		const char* name;
		FieldKind kind;
		bool isWeapDef;
		std::size_t offset;
		std::size_t count = 0;
		Game::XAssetType assetType = Game::ASSET_TYPE_COUNT;
		const char* const* names = nullptr;
	};

	struct WeaponSound
	{
		Game::snd_alias_list_t* Game::WeaponDef::* field;
		std::uint32_t Game::X86::WeaponDef::* field32;
	};

#define WEAPON_FIELD(kind, member) JsonField{ #member, FieldKind::kind, false, offsetof(Game::WeaponCompleteDef, member) }
#define WEAPON_ARRAY(kind, member, count) JsonField{ #member, FieldKind::kind, false, offsetof(Game::WeaponCompleteDef, member), count }
#define WEAPON_ASSET(member, type) JsonField{ #member, FieldKind::Asset, false, offsetof(Game::WeaponCompleteDef, member), 0, Game::type }
#define DEF_FIELD(kind, member) JsonField{ #member, FieldKind::kind, true, offsetof(Game::WeaponDef, member) }
#define DEF_ARRAY(kind, member, count) JsonField{ #member, FieldKind::kind, true, offsetof(Game::WeaponDef, member), count }
#define DEF_ASSET(member, type) JsonField{ #member, FieldKind::Asset, true, offsetof(Game::WeaponDef, member), 0, Game::type }
#define DEF_ENUM(member, names) JsonField{ #member, FieldKind::Enum, true, offsetof(Game::WeaponDef, member), std::size(names), Game::ASSET_TYPE_COUNT, names }
#define DEF_NOTETRACKS(member, type) JsonField{ #member, FieldKind::ScriptStrings, true, offsetof(Game::WeaponDef, member), noteTrackCount, Game::type }
#define DEF_SOUND(member) DEF_ASSET(member, ASSET_TYPE_SOUND)
#define WEAPON_SOUND(member) WeaponSound{ &Game::WeaponDef::member, &Game::X86::WeaponDef::member }

	static const JsonField weaponFields[] =
	{
		WEAPON_FIELD(String, szDisplayName),
		WEAPON_ARRAY(ScriptStrings, hideTags, hideTagCount),
		WEAPON_FIELD(Anims, szXAnims),
		WEAPON_FIELD(Float, fAdsZoomFov),
		WEAPON_FIELD(Int, iAdsTransInTime),
		WEAPON_FIELD(Int, iAdsTransOutTime),
		WEAPON_FIELD(Int, iClipSize),
		WEAPON_FIELD(Int, impactType),
		WEAPON_FIELD(Int, iFireTime),
		WEAPON_FIELD(Int, dpadIconRatio),
		WEAPON_FIELD(Float, penetrateMultiplier),
		WEAPON_FIELD(Float, fAdsViewKickCenterSpeed),
		WEAPON_FIELD(Float, fHipViewKickCenterSpeed),
		WEAPON_FIELD(String, szAltWeaponName),
		WEAPON_FIELD(Int, altWeaponIndex),
		WEAPON_FIELD(Int, iAltRaiseTime),
		WEAPON_ASSET(killIcon, ASSET_TYPE_MATERIAL),
		WEAPON_ASSET(dpadIcon, ASSET_TYPE_MATERIAL),
		WEAPON_FIELD(Int, fireAnimLength),
		WEAPON_FIELD(Int, iFirstRaiseTime),
		WEAPON_FIELD(Int, ammoDropStockMax),
		WEAPON_FIELD(Float, adsDofStart),
		WEAPON_FIELD(Float, adsDofEnd),
		JsonField{ "accuracyGraphs", FieldKind::AccuracyGraphs, false, 0 },
		WEAPON_FIELD(Bool, motionTracker),
		WEAPON_FIELD(Bool, enhanced),
		WEAPON_FIELD(Bool, dpadIconShowsAmmo),

		DEF_FIELD(String, szOverlayName),
		DEF_ARRAY(XModels, gunXModel, modelCount),
		DEF_ASSET(handXModel, ASSET_TYPE_XMODEL),
		DEF_FIELD(Anims, szXAnimsRightHanded),
		DEF_FIELD(Anims, szXAnimsLeftHanded),
		DEF_FIELD(String, szModeName),
		DEF_NOTETRACKS(notetrackSoundMapKeys, ASSET_TYPE_COUNT),
		DEF_NOTETRACKS(notetrackSoundMapValues, ASSET_TYPE_SOUND),
		DEF_NOTETRACKS(notetrackRumbleMapKeys, ASSET_TYPE_COUNT),
		DEF_NOTETRACKS(notetrackRumbleMapValues, ASSET_TYPE_COUNT),
		DEF_FIELD(Int, playerAnimType),
		DEF_ENUM(weapType, weapTypeNames),
		DEF_ENUM(weapClass, weapClassNames),
		DEF_ENUM(penetrateType, penetrateTypeNames),
		DEF_ENUM(inventoryType, inventoryTypeNames),
		DEF_ENUM(fireType, fireTypeNames),
		DEF_ENUM(offhandClass, offhandClassNames),
		DEF_ENUM(stance, stanceNames),
		DEF_ASSET(viewFlashEffect, ASSET_TYPE_FX),
		DEF_ASSET(worldFlashEffect, ASSET_TYPE_FX),
		DEF_SOUND(pickupSound),
		DEF_SOUND(pickupSoundPlayer),
		DEF_SOUND(ammoPickupSound),
		DEF_SOUND(ammoPickupSoundPlayer),
		DEF_SOUND(projectileSound),
		DEF_SOUND(pullbackSound),
		DEF_SOUND(pullbackSoundPlayer),
		DEF_SOUND(fireSound),
		DEF_SOUND(fireSoundPlayer),
		DEF_SOUND(fireSoundPlayerAkimbo),
		DEF_SOUND(fireLoopSound),
		DEF_SOUND(fireLoopSoundPlayer),
		DEF_SOUND(fireStopSound),
		DEF_SOUND(fireStopSoundPlayer),
		DEF_SOUND(fireLastSound),
		DEF_SOUND(fireLastSoundPlayer),
		DEF_SOUND(emptyFireSound),
		DEF_SOUND(emptyFireSoundPlayer),
		DEF_SOUND(meleeSwipeSound),
		DEF_SOUND(meleeSwipeSoundPlayer),
		DEF_SOUND(meleeHitSound),
		DEF_SOUND(meleeMissSound),
		DEF_SOUND(rechamberSound),
		DEF_SOUND(rechamberSoundPlayer),
		DEF_SOUND(reloadSound),
		DEF_SOUND(reloadSoundPlayer),
		DEF_SOUND(reloadEmptySound),
		DEF_SOUND(reloadEmptySoundPlayer),
		DEF_SOUND(reloadStartSound),
		DEF_SOUND(reloadStartSoundPlayer),
		DEF_SOUND(reloadEndSound),
		DEF_SOUND(reloadEndSoundPlayer),
		DEF_SOUND(detonateSound),
		DEF_SOUND(detonateSoundPlayer),
		DEF_SOUND(nightVisionWearSound),
		DEF_SOUND(nightVisionWearSoundPlayer),
		DEF_SOUND(nightVisionRemoveSound),
		DEF_SOUND(nightVisionRemoveSoundPlayer),
		DEF_SOUND(altSwitchSound),
		DEF_SOUND(altSwitchSoundPlayer),
		DEF_SOUND(raiseSound),
		DEF_SOUND(raiseSoundPlayer),
		DEF_SOUND(firstRaiseSound),
		DEF_SOUND(firstRaiseSoundPlayer),
		DEF_SOUND(putawaySound),
		DEF_SOUND(putawaySoundPlayer),
		DEF_SOUND(scanSound),
		DEF_ARRAY(SoundBuffer, bounceSound, bounceSoundCount),
		DEF_ASSET(viewShellEjectEffect, ASSET_TYPE_FX),
		DEF_ASSET(worldShellEjectEffect, ASSET_TYPE_FX),
		DEF_ASSET(viewLastShotEjectEffect, ASSET_TYPE_FX),
		DEF_ASSET(worldLastShotEjectEffect, ASSET_TYPE_FX),
		DEF_ASSET(reticleCenter, ASSET_TYPE_MATERIAL),
		DEF_ASSET(reticleSide, ASSET_TYPE_MATERIAL),
		DEF_FIELD(Int, iReticleCenterSize),
		DEF_FIELD(Int, iReticleSideSize),
		DEF_FIELD(Int, iReticleMinOfs),
		DEF_ENUM(activeReticleType, activeReticleNames),
		DEF_ARRAY(FloatArray, vStandMove, 3),
		DEF_ARRAY(FloatArray, vStandRot, 3),
		DEF_ARRAY(FloatArray, strafeMove, 3),
		DEF_ARRAY(FloatArray, strafeRot, 3),
		DEF_ARRAY(FloatArray, vDuckedOfs, 3),
		DEF_ARRAY(FloatArray, vDuckedMove, 3),
		DEF_ARRAY(FloatArray, vDuckedRot, 3),
		DEF_ARRAY(FloatArray, vProneOfs, 3),
		DEF_ARRAY(FloatArray, vProneMove, 3),
		DEF_ARRAY(FloatArray, vProneRot, 3),
		DEF_FIELD(Float, fPosMoveRate),
		DEF_FIELD(Float, fPosProneMoveRate),
		DEF_FIELD(Float, fStandMoveMinSpeed),
		DEF_FIELD(Float, fDuckedMoveMinSpeed),
		DEF_FIELD(Float, fProneMoveMinSpeed),
		DEF_FIELD(Float, fPosRotRate),
		DEF_FIELD(Float, fPosProneRotRate),
		DEF_FIELD(Float, fStandRotMinSpeed),
		DEF_FIELD(Float, fDuckedRotMinSpeed),
		DEF_FIELD(Float, fProneRotMinSpeed),
		DEF_ARRAY(XModels, worldModel, modelCount),
		DEF_ASSET(worldClipModel, ASSET_TYPE_XMODEL),
		DEF_ASSET(rocketModel, ASSET_TYPE_XMODEL),
		DEF_ASSET(knifeModel, ASSET_TYPE_XMODEL),
		DEF_ASSET(worldKnifeModel, ASSET_TYPE_XMODEL),
		DEF_ASSET(hudIcon, ASSET_TYPE_MATERIAL),
		DEF_ENUM(hudIconRatio, iconRatioNames),
		DEF_ASSET(pickupIcon, ASSET_TYPE_MATERIAL),
		DEF_ENUM(pickupIconRatio, iconRatioNames),
		DEF_ASSET(ammoCounterIcon, ASSET_TYPE_MATERIAL),
		DEF_ENUM(ammoCounterIconRatio, iconRatioNames),
		DEF_ENUM(ammoCounterClip, ammoCounterClipNames),
		DEF_FIELD(Int, iStartAmmo),
		DEF_FIELD(String, szAmmoName),
		DEF_FIELD(Int, iAmmoIndex),
		DEF_FIELD(String, szClipName),
		DEF_FIELD(Int, iClipIndex),
		DEF_FIELD(Int, iMaxAmmo),
		DEF_FIELD(Int, shotCount),
		DEF_FIELD(String, szSharedAmmoCapName),
		DEF_FIELD(Int, iSharedAmmoCapIndex),
		DEF_FIELD(Int, iSharedAmmoCap),
		DEF_FIELD(Int, damage),
		DEF_FIELD(Int, playerDamage),
		DEF_FIELD(Int, iMeleeDamage),
		DEF_FIELD(Int, iDamageType),
		DEF_FIELD(Int, iFireDelay),
		DEF_FIELD(Int, iMeleeDelay),
		DEF_FIELD(Int, meleeChargeDelay),
		DEF_FIELD(Int, iDetonateDelay),
		DEF_FIELD(Int, iRechamberTime),
		DEF_FIELD(Int, rechamberTimeOneHanded),
		DEF_FIELD(Int, iRechamberBoltTime),
		DEF_FIELD(Int, iHoldFireTime),
		DEF_FIELD(Int, iDetonateTime),
		DEF_FIELD(Int, iMeleeTime),
		DEF_FIELD(Int, meleeChargeTime),
		DEF_FIELD(Int, iReloadTime),
		DEF_FIELD(Int, reloadShowRocketTime),
		DEF_FIELD(Int, iReloadEmptyTime),
		DEF_FIELD(Int, iReloadAddTime),
		DEF_FIELD(Int, iReloadStartTime),
		DEF_FIELD(Int, iReloadStartAddTime),
		DEF_FIELD(Int, iReloadEndTime),
		DEF_FIELD(Int, iDropTime),
		DEF_FIELD(Int, iRaiseTime),
		DEF_FIELD(Int, iAltDropTime),
		DEF_FIELD(Int, quickDropTime),
		DEF_FIELD(Int, quickRaiseTime),
		DEF_FIELD(Int, iBreachRaiseTime),
		DEF_FIELD(Int, iEmptyRaiseTime),
		DEF_FIELD(Int, iEmptyDropTime),
		DEF_FIELD(Int, sprintInTime),
		DEF_FIELD(Int, sprintLoopTime),
		DEF_FIELD(Int, sprintOutTime),
		DEF_FIELD(Int, stunnedTimeBegin),
		DEF_FIELD(Int, stunnedTimeLoop),
		DEF_FIELD(Int, stunnedTimeEnd),
		DEF_FIELD(Int, nightVisionWearTime),
		DEF_FIELD(Int, nightVisionWearTimeFadeOutEnd),
		DEF_FIELD(Int, nightVisionWearTimePowerUp),
		DEF_FIELD(Int, nightVisionRemoveTime),
		DEF_FIELD(Int, nightVisionRemoveTimePowerDown),
		DEF_FIELD(Int, nightVisionRemoveTimeFadeInStart),
		DEF_FIELD(Int, fuseTime),
		DEF_FIELD(Int, aiFuseTime),
		DEF_FIELD(Float, autoAimRange),
		DEF_FIELD(Float, aimAssistRange),
		DEF_FIELD(Float, aimAssistRangeAds),
		DEF_FIELD(Float, aimPadding),
		DEF_FIELD(Float, enemyCrosshairRange),
		DEF_FIELD(Float, moveSpeedScale),
		DEF_FIELD(Float, adsMoveSpeedScale),
		DEF_FIELD(Float, sprintDurationScale),
		DEF_FIELD(Float, fAdsZoomInFrac),
		DEF_FIELD(Float, fAdsZoomOutFrac),
		DEF_ASSET(overlayMaterial, ASSET_TYPE_MATERIAL),
		DEF_ASSET(overlayMaterialLowRes, ASSET_TYPE_MATERIAL),
		DEF_ASSET(overlayMaterialEMP, ASSET_TYPE_MATERIAL),
		DEF_ASSET(overlayMaterialEMPLowRes, ASSET_TYPE_MATERIAL),
		DEF_ENUM(overlayReticle, overlayReticleNames),
		DEF_ENUM(overlayInterface, overlayInterfaceNames),
		DEF_FIELD(Float, overlayWidth),
		DEF_FIELD(Float, overlayHeight),
		DEF_FIELD(Float, overlayWidthSplitscreen),
		DEF_FIELD(Float, overlayHeightSplitscreen),
		DEF_FIELD(Float, fAdsBobFactor),
		DEF_FIELD(Float, fAdsViewBobMult),
		DEF_FIELD(Float, fHipSpreadStandMin),
		DEF_FIELD(Float, fHipSpreadDuckedMin),
		DEF_FIELD(Float, fHipSpreadProneMin),
		DEF_FIELD(Float, hipSpreadStandMax),
		DEF_FIELD(Float, hipSpreadDuckedMax),
		DEF_FIELD(Float, hipSpreadProneMax),
		DEF_FIELD(Float, fHipSpreadDecayRate),
		DEF_FIELD(Float, fHipSpreadFireAdd),
		DEF_FIELD(Float, fHipSpreadTurnAdd),
		DEF_FIELD(Float, fHipSpreadMoveAdd),
		DEF_FIELD(Float, fHipSpreadDuckedDecay),
		DEF_FIELD(Float, fHipSpreadProneDecay),
		DEF_FIELD(Float, fHipReticleSidePos),
		DEF_FIELD(Float, fAdsIdleAmount),
		DEF_FIELD(Float, fHipIdleAmount),
		DEF_FIELD(Float, adsIdleSpeed),
		DEF_FIELD(Float, hipIdleSpeed),
		DEF_FIELD(Float, fIdleCrouchFactor),
		DEF_FIELD(Float, fIdleProneFactor),
		DEF_FIELD(Float, fGunMaxPitch),
		DEF_FIELD(Float, fGunMaxYaw),
		DEF_FIELD(Float, swayMaxAngle),
		DEF_FIELD(Float, swayLerpSpeed),
		DEF_FIELD(Float, swayPitchScale),
		DEF_FIELD(Float, swayYawScale),
		DEF_FIELD(Float, swayHorizScale),
		DEF_FIELD(Float, swayVertScale),
		DEF_FIELD(Float, swayShellShockScale),
		DEF_FIELD(Float, adsSwayMaxAngle),
		DEF_FIELD(Float, adsSwayLerpSpeed),
		DEF_FIELD(Float, adsSwayPitchScale),
		DEF_FIELD(Float, adsSwayYawScale),
		DEF_FIELD(Float, adsSwayHorizScale),
		DEF_FIELD(Float, adsSwayVertScale),
		DEF_FIELD(Float, adsViewErrorMin),
		DEF_FIELD(Float, adsViewErrorMax),
		DEF_FIELD(Float, dualWieldViewModelOffset),
		DEF_ENUM(killIconRatio, iconRatioNames),
		DEF_FIELD(Int, iReloadAmmoAdd),
		DEF_FIELD(Int, iReloadStartAdd),
		DEF_FIELD(Int, ammoDropStockMin),
		DEF_FIELD(Int, ammoDropClipPercentMin),
		DEF_FIELD(Int, ammoDropClipPercentMax),
		DEF_FIELD(Int, iExplosionRadius),
		DEF_FIELD(Int, iExplosionRadiusMin),
		DEF_FIELD(Int, iExplosionInnerDamage),
		DEF_FIELD(Int, iExplosionOuterDamage),
		DEF_FIELD(Float, damageConeAngle),
		DEF_FIELD(Float, bulletExplDmgMult),
		DEF_FIELD(Float, bulletExplRadiusMult),
		DEF_FIELD(Int, iProjectileSpeed),
		DEF_FIELD(Int, iProjectileSpeedUp),
		DEF_FIELD(Int, iProjectileSpeedForward),
		DEF_FIELD(Int, iProjectileActivateDist),
		DEF_FIELD(Float, projLifetime),
		DEF_FIELD(Float, timeToAccelerate),
		DEF_FIELD(Float, projectileCurvature),
		DEF_ASSET(projectileModel, ASSET_TYPE_XMODEL),
		DEF_ENUM(projExplosion, projExplosionNames),
		DEF_ASSET(projExplosionEffect, ASSET_TYPE_FX),
		DEF_ASSET(projDudEffect, ASSET_TYPE_FX),
		DEF_SOUND(projExplosionSound),
		DEF_SOUND(projDudSound),
		DEF_ENUM(stickiness, stickinessNames),
		DEF_FIELD(Float, lowAmmoWarningThreshold),
		DEF_FIELD(Float, ricochetChance),
		DEF_ARRAY(FloatBuffer, parallelBounce, bounceCount),
		DEF_ARRAY(FloatBuffer, perpendicularBounce, bounceCount),
		DEF_ASSET(projTrailEffect, ASSET_TYPE_FX),
		DEF_ASSET(projBeaconEffect, ASSET_TYPE_FX),
		DEF_ARRAY(FloatArray, vProjectileColor, 3),
		DEF_ENUM(guidedMissileType, guidedMissileNames),
		DEF_FIELD(Float, maxSteeringAccel),
		DEF_FIELD(Int, projIgnitionDelay),
		DEF_ASSET(projIgnitionEffect, ASSET_TYPE_FX),
		DEF_SOUND(projIgnitionSound),
		DEF_FIELD(Float, fAdsAimPitch),
		DEF_FIELD(Float, fAdsCrosshairInFrac),
		DEF_FIELD(Float, fAdsCrosshairOutFrac),
		DEF_FIELD(Int, adsGunKickReducedKickBullets),
		DEF_FIELD(Float, adsGunKickReducedKickPercent),
		DEF_FIELD(Float, fAdsGunKickPitchMin),
		DEF_FIELD(Float, fAdsGunKickPitchMax),
		DEF_FIELD(Float, fAdsGunKickYawMin),
		DEF_FIELD(Float, fAdsGunKickYawMax),
		DEF_FIELD(Float, fAdsGunKickAccel),
		DEF_FIELD(Float, fAdsGunKickSpeedMax),
		DEF_FIELD(Float, fAdsGunKickSpeedDecay),
		DEF_FIELD(Float, fAdsGunKickStaticDecay),
		DEF_FIELD(Float, fAdsViewKickPitchMin),
		DEF_FIELD(Float, fAdsViewKickPitchMax),
		DEF_FIELD(Float, fAdsViewKickYawMin),
		DEF_FIELD(Float, fAdsViewKickYawMax),
		DEF_FIELD(Float, fAdsViewScatterMin),
		DEF_FIELD(Float, fAdsViewScatterMax),
		DEF_FIELD(Float, fAdsSpread),
		DEF_FIELD(Int, hipGunKickReducedKickBullets),
		DEF_FIELD(Float, hipGunKickReducedKickPercent),
		DEF_FIELD(Float, fHipGunKickPitchMin),
		DEF_FIELD(Float, fHipGunKickPitchMax),
		DEF_FIELD(Float, fHipGunKickYawMin),
		DEF_FIELD(Float, fHipGunKickYawMax),
		DEF_FIELD(Float, fHipGunKickAccel),
		DEF_FIELD(Float, fHipGunKickSpeedMax),
		DEF_FIELD(Float, fHipGunKickSpeedDecay),
		DEF_FIELD(Float, fHipGunKickStaticDecay),
		DEF_FIELD(Float, fHipViewKickPitchMin),
		DEF_FIELD(Float, fHipViewKickPitchMax),
		DEF_FIELD(Float, fHipViewKickYawMin),
		DEF_FIELD(Float, fHipViewKickYawMax),
		DEF_FIELD(Float, fHipViewScatterMin),
		DEF_FIELD(Float, fHipViewScatterMax),
		DEF_FIELD(Float, fightDist),
		DEF_FIELD(Float, maxDist),
		JsonField{ "originalAccuracyGraphs", FieldKind::OriginalAccuracyGraphs, true, 0 },
		DEF_FIELD(Int, iPositionReloadTransTime),
		DEF_FIELD(Float, leftArc),
		DEF_FIELD(Float, rightArc),
		DEF_FIELD(Float, topArc),
		DEF_FIELD(Float, bottomArc),
		DEF_FIELD(Float, accuracy),
		DEF_FIELD(Float, aiSpread),
		DEF_FIELD(Float, playerSpread),
		DEF_ARRAY(FloatArray, minTurnSpeed, 2),
		DEF_ARRAY(FloatArray, maxTurnSpeed, 2),
		DEF_FIELD(Float, pitchConvergenceTime),
		DEF_FIELD(Float, yawConvergenceTime),
		DEF_FIELD(Float, suppressTime),
		DEF_FIELD(Float, maxRange),
		DEF_FIELD(Float, fAnimHorRotateInc),
		DEF_FIELD(Float, fPlayerPositionDist),
		DEF_FIELD(String, szUseHintString),
		DEF_FIELD(String, dropHintString),
		DEF_FIELD(Int, iUseHintStringIndex),
		DEF_FIELD(Int, dropHintStringIndex),
		DEF_FIELD(Float, horizViewJitter),
		DEF_FIELD(Float, vertViewJitter),
		DEF_FIELD(Float, scanSpeed),
		DEF_FIELD(Float, scanAccel),
		DEF_FIELD(Int, scanPauseTime),
		DEF_FIELD(String, szScript),
		DEF_ARRAY(FloatArray, fOOPosAnimLength, 2),
		DEF_FIELD(Int, minDamage),
		DEF_FIELD(Int, minPlayerDamage),
		DEF_FIELD(Float, fMaxDamageRange),
		DEF_FIELD(Float, fMinDamageRange),
		DEF_FIELD(Float, destabilizationRateTime),
		DEF_FIELD(Float, destabilizationCurvatureMax),
		DEF_FIELD(Int, destabilizeDistance),
		DEF_ARRAY(FloatBuffer, locationDamageMultipliers, damageLocationCount),
		DEF_FIELD(String, fireRumble),
		DEF_FIELD(String, meleeImpactRumble),
		DEF_ASSET(tracerType, ASSET_TYPE_TRACER),
	};

	static const JsonField turretFields[] =
	{
		DEF_FIELD(Float, turretScopeZoomRate),
		DEF_FIELD(Float, turretScopeZoomMin),
		DEF_FIELD(Float, turretScopeZoomMax),
		DEF_FIELD(Float, turretOverheatUpRate),
		DEF_FIELD(Float, turretOverheatDownRate),
		DEF_FIELD(Float, turretOverheatPenalty),
		DEF_SOUND(turretOverheatSound),
		DEF_ASSET(turretOverheatEffect, ASSET_TYPE_FX),
		DEF_FIELD(String, turretBarrelSpinRumble),
		DEF_FIELD(Float, turretBarrelSpinSpeed),
		DEF_FIELD(Float, turretBarrelSpinUpTime),
		DEF_FIELD(Float, turretBarrelSpinDownTime),
		DEF_SOUND(turretBarrelSpinMaxSnd),
		DEF_ARRAY(SoundArray, turretBarrelSpinUpSnd, 4),
		DEF_ARRAY(SoundArray, turretBarrelSpinDownSnd, 4),
		DEF_FIELD(Bool, turretBarrelSpinEnabled),
	};

	static const JsonField rocketFields[] =
	{
		DEF_SOUND(missileConeSoundAlias),
		DEF_SOUND(missileConeSoundAliasAtBase),
		DEF_FIELD(Float, missileConeSoundRadiusAtTop),
		DEF_FIELD(Float, missileConeSoundRadiusAtBase),
		DEF_FIELD(Float, missileConeSoundHeight),
		DEF_FIELD(Float, missileConeSoundOriginOffset),
		DEF_FIELD(Float, missileConeSoundVolumescaleAtCore),
		DEF_FIELD(Float, missileConeSoundVolumescaleAtEdge),
		DEF_FIELD(Float, missileConeSoundVolumescaleCoreSize),
		DEF_FIELD(Float, missileConeSoundPitchAtTop),
		DEF_FIELD(Float, missileConeSoundPitchAtBottom),
		DEF_FIELD(Float, missileConeSoundPitchTopSize),
		DEF_FIELD(Float, missileConeSoundPitchBottomSize),
		DEF_FIELD(Float, missileConeSoundCrossfadeTopSize),
		DEF_FIELD(Float, missileConeSoundCrossfadeBottomSize),
		DEF_FIELD(Bool, missileConeSoundEnabled),
		DEF_FIELD(Bool, missileConeSoundPitchshiftEnabled),
		DEF_FIELD(Bool, missileConeSoundCrossfadeEnabled),
	};

	static const JsonField flagFields[] =
	{
		DEF_FIELD(Bool, sharedAmmo),
		DEF_FIELD(Bool, lockonSupported),
		DEF_FIELD(Bool, requireLockonToFire),
		DEF_FIELD(Bool, bigExplosion),
		DEF_FIELD(Bool, noAdsWhenMagEmpty),
		DEF_FIELD(Bool, avoidDropCleanup),
		DEF_FIELD(Bool, inheritsPerks),
		DEF_FIELD(Bool, crosshairColorChange),
		DEF_FIELD(Bool, bRifleBullet),
		DEF_FIELD(Bool, armorPiercing),
		DEF_FIELD(Bool, bBoltAction),
		DEF_FIELD(Bool, aimDownSight),
		DEF_FIELD(Bool, bRechamberWhileAds),
		DEF_FIELD(Bool, bBulletExplosiveDamage),
		DEF_FIELD(Bool, bCookOffHold),
		DEF_FIELD(Bool, bClipOnly),
		DEF_FIELD(Bool, noAmmoPickup),
		DEF_FIELD(Bool, adsFireOnly),
		DEF_FIELD(Bool, cancelAutoHolsterWhenEmpty),
		DEF_FIELD(Bool, disableSwitchToWhenEmpty),
		DEF_FIELD(Bool, suppressAmmoReserveDisplay),
		DEF_FIELD(Bool, laserSightDuringNightvision),
		DEF_FIELD(Bool, markableViewmodel),
		DEF_FIELD(Bool, noDualWield),
		DEF_FIELD(Bool, flipKillIcon),
		DEF_FIELD(Bool, bNoPartialReload),
		DEF_FIELD(Bool, bSegmentedReload),
		DEF_FIELD(Bool, blocksProne),
		DEF_FIELD(Bool, silenced),
		DEF_FIELD(Bool, isRollingGrenade),
		DEF_FIELD(Bool, projExplosionEffectForceNormalUp),
		DEF_FIELD(Bool, bProjImpactExplode),
		DEF_FIELD(Bool, stickToPlayers),
		DEF_FIELD(Bool, hasDetonator),
		DEF_FIELD(Bool, disableFiring),
		DEF_FIELD(Bool, timedDetonation),
		DEF_FIELD(Bool, rotate),
		DEF_FIELD(Bool, holdButtonToThrow),
		DEF_FIELD(Bool, freezeMovementWhenFiring),
		DEF_FIELD(Bool, thermalScope),
		DEF_FIELD(Bool, altModeSameWeapon),
		DEF_FIELD(Bool, offhandHoldIsCancelable),
	};

	static constexpr WeaponSound weaponSounds[] =
	{
		WEAPON_SOUND(pickupSound),
		WEAPON_SOUND(pickupSoundPlayer),
		WEAPON_SOUND(ammoPickupSound),
		WEAPON_SOUND(ammoPickupSoundPlayer),
		WEAPON_SOUND(projectileSound),
		WEAPON_SOUND(pullbackSound),
		WEAPON_SOUND(pullbackSoundPlayer),
		WEAPON_SOUND(fireSound),
		WEAPON_SOUND(fireSoundPlayer),
		WEAPON_SOUND(fireSoundPlayerAkimbo),
		WEAPON_SOUND(fireLoopSound),
		WEAPON_SOUND(fireLoopSoundPlayer),
		WEAPON_SOUND(fireStopSound),
		WEAPON_SOUND(fireStopSoundPlayer),
		WEAPON_SOUND(fireLastSound),
		WEAPON_SOUND(fireLastSoundPlayer),
		WEAPON_SOUND(emptyFireSound),
		WEAPON_SOUND(emptyFireSoundPlayer),
		WEAPON_SOUND(meleeSwipeSound),
		WEAPON_SOUND(meleeSwipeSoundPlayer),
		WEAPON_SOUND(meleeHitSound),
		WEAPON_SOUND(meleeMissSound),
		WEAPON_SOUND(rechamberSound),
		WEAPON_SOUND(rechamberSoundPlayer),
		WEAPON_SOUND(reloadSound),
		WEAPON_SOUND(reloadSoundPlayer),
		WEAPON_SOUND(reloadEmptySound),
		WEAPON_SOUND(reloadEmptySoundPlayer),
		WEAPON_SOUND(reloadStartSound),
		WEAPON_SOUND(reloadStartSoundPlayer),
		WEAPON_SOUND(reloadEndSound),
		WEAPON_SOUND(reloadEndSoundPlayer),
		WEAPON_SOUND(detonateSound),
		WEAPON_SOUND(detonateSoundPlayer),
		WEAPON_SOUND(nightVisionWearSound),
		WEAPON_SOUND(nightVisionWearSoundPlayer),
		WEAPON_SOUND(nightVisionRemoveSound),
		WEAPON_SOUND(nightVisionRemoveSoundPlayer),
		WEAPON_SOUND(altSwitchSound),
		WEAPON_SOUND(altSwitchSoundPlayer),
		WEAPON_SOUND(raiseSound),
		WEAPON_SOUND(raiseSoundPlayer),
		WEAPON_SOUND(firstRaiseSound),
		WEAPON_SOUND(firstRaiseSoundPlayer),
		WEAPON_SOUND(putawaySound),
		WEAPON_SOUND(putawaySoundPlayer),
		WEAPON_SOUND(scanSound),
	};

	static_assert(std::size(weaponSounds) == 47);

#undef WEAPON_FIELD
#undef WEAPON_ARRAY
#undef WEAPON_ASSET
#undef DEF_FIELD
#undef DEF_ARRAY
#undef DEF_ASSET
#undef DEF_ENUM
#undef DEF_NOTETRACKS
#undef DEF_SOUND
#undef WEAPON_SOUND

	template <typename T> static T& FieldAt(void* base, std::size_t offset)
	{
		return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(base) + offset);
	}

	template <typename T> static const T& FieldAt(const void* base, std::size_t offset)
	{
		return *reinterpret_cast<const T*>(static_cast<const std::uint8_t*>(base) + offset);
	}

	static void* FieldBase(Game::WeaponCompleteDef* weapon, const JsonField& field)
	{
		if (field.isWeapDef)
		{
			return weapon->weapDef;
		}

		return weapon;
	}

	static const void* FieldBase(const Game::WeaponCompleteDef* weapon, const JsonField& field)
	{
		if (!weapon)
		{
			return nullptr;
		}

		if (field.isWeapDef)
		{
			return weapon->weapDef;
		}

		return weapon;
	}

	static std::string GetWeaponPath(const std::string& name)
	{
		return std::format("weapons/{}.iw4x.json", name);
	}

	static const char* AssetName(Game::XAssetType type, void* asset)
	{
		const Game::XAsset entry{ static_cast<unsigned int>(type), asset };
		return Game::DB_GetXAssetName(&entry);
	}

	static void* FindLoadedAsset(Game::XAssetType type, const char* name)
	{
		return Components::AssetHandler::FindLoadedAsset(type, name).data;
	}

	static void DumpChild(Game::XAssetType type, void* asset)
	{
		if (!asset || !Components::AssetHandler::CanDump(type))
		{
			return;
		}

		Components::AssetHandler::DumpAsset({ static_cast<unsigned int>(type), asset });
	}

	static void* FindForZone(Game::XAssetType type, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		return Components::AssetHandler::FindAssetForZone(type, name, builder).data;
	}

	static bool TryReadFloats(const rapidjson::Value& array, float* values, std::size_t count)
	{
		for (rapidjson::SizeType i = 0; i < array.Size() && i < count; ++i)
		{
			if (!array[i].IsNumber())
			{
				return false;
			}

			values[i] = array[i].Get<float>();
		}

		return true;
	}

	static bool TryReadAssets(const rapidjson::Value& array, void** assets, std::size_t count, Game::XAssetType type, Components::ZoneBuilder::Zone* builder)
	{
		for (rapidjson::SizeType i = 0; i < array.Size() && i < count; ++i)
		{
			if (array[i].IsNull())
			{
				assets[i] = nullptr;
				continue;
			}

			if (!array[i].IsString())
			{
				return false;
			}

			assets[i] = FindForZone(type, array[i].GetString(), builder);
		}

		return true;
	}

	static bool TryReadField(const rapidjson::Value& json, const JsonField& field, Game::WeaponCompleteDef* weapon, Components::ZoneBuilder::Zone* builder)
	{
		auto* const allocator = builder->GetAllocator();
		auto* const base = FieldBase(weapon, field);

		if (field.kind == FieldKind::OriginalAccuracyGraphs)
		{
			weapon->weapDef->accuracyGraphName[0] = allocator->DuplicateString("");
			weapon->weapDef->accuracyGraphName[1] = allocator->DuplicateString("");
			weapon->weapDef->originalAccuracyGraphKnotCount[0] = 0;
			weapon->weapDef->originalAccuracyGraphKnotCount[1] = 0;
			return true;
		}

		if (!json.HasMember(field.name))
		{
			return true;
		}

		const auto& value = json[field.name];

		switch (field.kind)
		{
		case FieldKind::Int:
		{
			if (!value.IsNumber())
			{
				return false;
			}

			FieldAt<std::int32_t>(base, field.offset) = static_cast<std::int32_t>(value.GetDouble());
			return true;
		}
		case FieldKind::Float:
		{
			if (!value.IsNumber())
			{
				return false;
			}

			FieldAt<float>(base, field.offset) = value.Get<float>();
			return true;
		}
		case FieldKind::Bool:
		{
			if (value.IsBool())
			{
				FieldAt<bool>(base, field.offset) = value.GetBool();
				return true;
			}

			if (!value.IsNumber())
			{
				return false;
			}

			FieldAt<bool>(base, field.offset) = value.GetDouble() != 0.0;
			return true;
		}
		case FieldKind::Enum:
		{
			if (value.IsNull())
			{
				return true;
			}

			if (!value.IsString())
			{
				return false;
			}

			std::int32_t index = 0;

			for (std::size_t i = 0; i < field.count; ++i)
			{
				if (std::strcmp(field.names[i], value.GetString()) == 0)
				{
					index = static_cast<std::int32_t>(i);
					break;
				}
			}

			FieldAt<std::int32_t>(base, field.offset) = index;
			return true;
		}
		case FieldKind::String:
		{
			if (value.IsNull())
			{
				FieldAt<const char*>(base, field.offset) = nullptr;
				return true;
			}

			if (!value.IsString())
			{
				return false;
			}

			FieldAt<const char*>(base, field.offset) = allocator->DuplicateString(value.GetString());
			return true;
		}
		case FieldKind::Asset:
		{
			if (value.IsNull())
			{
				FieldAt<void*>(base, field.offset) = nullptr;
				return true;
			}

			if (!value.IsString())
			{
				return false;
			}

			FieldAt<void*>(base, field.offset) = FindForZone(field.assetType, value.GetString(), builder);
			return true;
		}
		case FieldKind::FloatArray:
		{
			if (!value.IsArray())
			{
				return false;
			}

			if (value.Size() != field.count)
			{
				return true;
			}

			return TryReadFloats(value, &FieldAt<float>(base, field.offset), field.count);
		}
		case FieldKind::FloatBuffer:
		{
			if (value.IsNull())
			{
				FieldAt<float*>(base, field.offset) = nullptr;
				return true;
			}

			if (!value.IsArray())
			{
				return false;
			}

			auto* const values = allocator->AllocateArray<float>(field.count);
			FieldAt<float*>(base, field.offset) = values;
			return TryReadFloats(value, values, field.count);
		}
		case FieldKind::ScriptStrings:
		{
			if (value.IsNull())
			{
				FieldAt<unsigned short*>(base, field.offset) = nullptr;
				return true;
			}

			if (!value.IsArray())
			{
				return false;
			}

			auto* const strings = allocator->AllocateArray<unsigned short>(field.count);
			FieldAt<unsigned short*>(base, field.offset) = strings;

			for (rapidjson::SizeType i = 0; i < value.Size() && i < field.count; ++i)
			{
				if (!value[i].IsString())
				{
					continue;
				}

				strings[i] = static_cast<unsigned short>(Game::SL_GetString(value[i].GetString(), 0));

				if (field.assetType != Game::ASSET_TYPE_COUNT && !FindForZone(field.assetType, value[i].GetString(), builder))
				{
					Components::Logger::Error("Could not find sound {} from notetracks!\n", value[i].GetString());
				}
			}

			return true;
		}
		case FieldKind::Anims:
		{
			if (value.IsNull())
			{
				FieldAt<const char**>(base, field.offset) = nullptr;
				return true;
			}

			if (!value.IsObject())
			{
				return false;
			}

			auto* const anims = allocator->AllocateArray<const char*>(animCount);

			if (FieldAt<const char**>(base, field.offset))
			{
				std::memcpy(anims, FieldAt<const char**>(base, field.offset), animCount * sizeof(const char*));
			}

			FieldAt<const char**>(base, field.offset) = anims;

			for (const auto& member : value.GetObject())
			{
				if (!member.value.IsString())
				{
					continue;
				}

				for (std::size_t i = 0; i < animCount; ++i)
				{
					if (std::strcmp(animNames[i], member.name.GetString()) != 0)
					{
						continue;
					}

					anims[i] = allocator->DuplicateString(member.value.GetString());

					if (*anims[i] && !FindForZone(Game::ASSET_TYPE_XANIMPARTS, anims[i], builder))
					{
						Components::Logger::Error("Could not find anim {}!\n", anims[i]);
					}

					break;
				}
			}

			return true;
		}
		case FieldKind::XModels:
		case FieldKind::SoundBuffer:
		{
			if (value.IsNull())
			{
				FieldAt<void**>(base, field.offset) = nullptr;
				return true;
			}

			if (!value.IsArray())
			{
				return false;
			}

			Game::XAssetType type = Game::ASSET_TYPE_SOUND;

			if (field.kind == FieldKind::XModels)
			{
				type = Game::ASSET_TYPE_XMODEL;
			}

			auto* const assets = allocator->AllocateArray<void*>(field.count);
			FieldAt<void**>(base, field.offset) = assets;
			return TryReadAssets(value, assets, field.count, type, builder);
		}
		case FieldKind::SoundArray:
		{
			if (!value.IsArray())
			{
				return false;
			}

			return TryReadAssets(value, &FieldAt<void*>(base, field.offset), field.count, Game::ASSET_TYPE_SOUND, builder);
		}
		case FieldKind::AccuracyGraphs:
		{
			if (value.IsArray())
			{
				weapon->accuracyGraphKnotCount[0] = 0;
				weapon->accuracyGraphKnotCount[1] = 0;
			}

			return true;
		}
		default:
			return true;
		}
	}

	static bool TryReadFields(const rapidjson::Value& json, std::span<const JsonField> fields, Game::WeaponCompleteDef* weapon, Components::ZoneBuilder::Zone* builder)
	{
		for (const auto& field : fields)
		{
			if (!TryReadField(json, field, weapon, builder))
			{
				Components::Logger::Error("Weapon {} has a malformed {}\n", weapon->szInternalName, field.name);
				return false;
			}
		}

		return true;
	}

	static Game::WeaponCompleteDef* TryReadVariant(const rapidjson::Value& json, const Game::WeaponCompleteDef* original, Components::ZoneBuilder::Zone* builder)
	{
		auto* const allocator = builder->GetAllocator();

		if (!json.IsObject() || !json.HasMember("szInternalName") || !json["szInternalName"].IsString())
		{
			Components::Logger::Error("A weapon variant has no szInternalName\n");
			return nullptr;
		}

		auto* const weapon = allocator->Allocate<Game::WeaponCompleteDef>();
		auto* const weapDef = allocator->Allocate<Game::WeaponDef>();

		if (original)
		{
			*weapon = *original;
			*weapDef = *original->weapDef;
		}

		weapon->weapDef = weapDef;

		const std::string jsonName = json["szInternalName"].GetString();
		std::string originalName = jsonName;

		if (original)
		{
			originalName = original->szInternalName;
		}

		std::string suffix;

		if (originalName.ends_with("_mp"))
		{
			suffix = "_mp";
		}

		const std::string baseName = originalName.substr(0, originalName.size() - suffix.size());
		std::string fullName = jsonName;

		if (original)
		{
			fullName = std::format("{}_{}{}", baseName, jsonName, suffix);
		}

		weapon->szInternalName = allocator->DuplicateString(fullName);

		if (!TryReadFields(json, weaponFields, weapon, builder))
		{
			return nullptr;
		}

		if (weapDef->weapClass == Game::WEAPCLASS_TURRET && !TryReadFields(json, turretFields, weapon, builder))
		{
			return nullptr;
		}

		if (weapDef->weapClass == Game::WEAPCLASS_ROCKETLAUNCHER && !TryReadFields(json, rocketFields, weapon, builder))
		{
			return nullptr;
		}

		if (!TryReadFields(json, flagFields, weapon, builder))
		{
			return nullptr;
		}

		if (!weapon->szAltWeaponName)
		{
			weapon->szAltWeaponName = allocator->DuplicateString("");
		}

		if (original && *weapon->szAltWeaponName)
		{
			FindForZone(Game::ASSET_TYPE_WEAPON, weapon->szAltWeaponName, builder);
		}

		if (json.HasMember("variants") && json["variants"].IsArray())
		{
			for (const auto& variantJson : json["variants"].GetArray())
			{
				auto* const variant = TryReadVariant(variantJson, weapon, builder);

				if (variant)
				{
					const Game::XAsset asset{ static_cast<unsigned int>(Game::ASSET_TYPE_WEAPON), variant };
					Components::AssetHandler::ZoneMark(asset, builder);
					builder->AddRawAsset(Game::ASSET_TYPE_WEAPON, variant);
				}
			}
		}

		return weapon;
	}

	static Game::WeaponCompleteDef* TryReadWeapon(const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File file(GetWeaponPath(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		rapidjson::Document document;
		document.Parse(file.GetBuffer().data(), file.GetBuffer().size());

		if (document.HasParseError() || !document.IsObject())
		{
			Components::Logger::Error("Invalid weapon json for {} (Is it zonebuilder format?)\n", name);
			return nullptr;
		}

		if (!document.HasMember("version") || !document["version"].IsInt() || document["version"].GetInt() != weaponJsonVersion)
		{
			Components::Logger::Error("Invalid weapon json version for {}, expected {}\n", name, weaponJsonVersion);
			return nullptr;
		}

		if (!document.HasMember("weapon"))
		{
			Components::Logger::Error("Invalid weapon json for {}, it has no weapon\n", name);
			return nullptr;
		}

		return TryReadVariant(document["weapon"], nullptr, builder);
	}

	static rapidjson::Value StringOrNull(const char* text, Utils::JSON::Allocator& allocator)
	{
		if (!text)
		{
			return rapidjson::Value(rapidjson::kNullType);
		}

		return rapidjson::Value(text, allocator);
	}

	static rapidjson::Value AssetNames(void* const* assets, std::size_t count, Game::XAssetType type, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value names(rapidjson::kArrayType);

		for (std::size_t i = 0; i < count; ++i)
		{
			if (!assets[i])
			{
				names.PushBack(rapidjson::Value(rapidjson::kNullType), allocator);
				continue;
			}

			DumpChild(type, assets[i]);
			names.PushBack(StringOrNull(AssetName(type, assets[i]), allocator), allocator);
		}

		return names;
	}

	static rapidjson::Value KnotFirsts(float (*knots)[2], std::size_t count, Utils::JSON::Allocator& allocator)
	{
		rapidjson::Value firsts(rapidjson::kArrayType);

		for (std::size_t i = 0; knots && i < count; ++i)
		{
			firsts.PushBack(knots[i][0], allocator);
		}

		return firsts;
	}

	static void WriteField(rapidjson::Value& container, Utils::JSON::Allocator& allocator, const JsonField& field, const Game::WeaponCompleteDef* weapon, const Game::WeaponCompleteDef* original)
	{
		const auto* const base = FieldBase(weapon, field);
		const auto* const originalBase = FieldBase(original, field);
		const auto key = rapidjson::StringRef(field.name);

		switch (field.kind)
		{
		case FieldKind::Int:
		{
			const auto value = FieldAt<std::int32_t>(base, field.offset);
			std::int32_t reference = 0;

			if (originalBase)
			{
				reference = FieldAt<std::int32_t>(originalBase, field.offset);
			}

			if (value != reference)
			{
				container.AddMember(key, value, allocator);
			}

			break;
		}
		case FieldKind::Float:
		{
			const auto value = FieldAt<float>(base, field.offset);
			float reference = 0.0f;

			if (originalBase)
			{
				reference = FieldAt<float>(originalBase, field.offset);
			}

			if (std::memcmp(&value, &reference, sizeof(float)) != 0)
			{
				container.AddMember(key, value, allocator);
			}

			break;
		}
		case FieldKind::Bool:
		{
			const auto value = FieldAt<bool>(base, field.offset);
			bool reference = false;

			if (originalBase)
			{
				reference = FieldAt<bool>(originalBase, field.offset);
			}

			if (value != reference)
			{
				container.AddMember(key, value, allocator);
			}

			break;
		}
		case FieldKind::Enum:
		{
			const auto value = FieldAt<std::int32_t>(base, field.offset);

			if (originalBase && value == FieldAt<std::int32_t>(originalBase, field.offset))
			{
				break;
			}

			if (value < 0 || static_cast<std::size_t>(value) >= field.count)
			{
				Components::Logger::Error("Weapon {} has {} {}, which has no name, it is not dumped\n", weapon->szInternalName, field.name, value);
				break;
			}

			container.AddMember(key, rapidjson::StringRef(field.names[value]), allocator);
			break;
		}
		case FieldKind::String:
		{
			const auto* const value = FieldAt<const char*>(base, field.offset);
			const char* reference = nullptr;

			if (originalBase)
			{
				reference = FieldAt<const char*>(originalBase, field.offset);
			}

			if (value && (!reference || std::strcmp(value, reference) != 0))
			{
				container.AddMember(key, rapidjson::Value(value, allocator), allocator);
			}

			if (!value && reference)
			{
				container.AddMember(key, rapidjson::Value(rapidjson::kNullType), allocator);
			}

			break;
		}
		case FieldKind::Asset:
		{
			auto* const value = FieldAt<void*>(base, field.offset);
			void* reference = nullptr;

			if (originalBase)
			{
				reference = FieldAt<void*>(originalBase, field.offset);
			}

			if (value && value != reference)
			{
				DumpChild(field.assetType, value);
				container.AddMember(key, StringOrNull(AssetName(field.assetType, value), allocator), allocator);
			}

			if (!value && reference)
			{
				container.AddMember(key, rapidjson::Value(rapidjson::kNullType), allocator);
			}

			break;
		}
		case FieldKind::FloatArray:
		{
			const auto* const values = &FieldAt<float>(base, field.offset);

			if (originalBase && std::memcmp(values, &FieldAt<float>(originalBase, field.offset), field.count * sizeof(float)) == 0)
			{
				break;
			}

			container.AddMember(key, Utils::JSON::MakeArray(values, field.count, allocator), allocator);
			break;
		}
		case FieldKind::FloatBuffer:
		{
			const auto* const values = FieldAt<float*>(base, field.offset);
			const float* reference = nullptr;

			if (originalBase)
			{
				reference = FieldAt<float*>(originalBase, field.offset);
			}

			if (values && (!reference || std::memcmp(values, reference, field.count * sizeof(float)) != 0))
			{
				container.AddMember(key, Utils::JSON::MakeArray(values, field.count, allocator), allocator);
			}

			if (!values && reference)
			{
				container.AddMember(key, rapidjson::Value(rapidjson::kNullType), allocator);
			}

			break;
		}
		case FieldKind::ScriptStrings:
		{
			const auto* const strings = FieldAt<unsigned short*>(base, field.offset);
			const unsigned short* reference = nullptr;

			if (originalBase)
			{
				reference = FieldAt<unsigned short*>(originalBase, field.offset);
			}

			if (!strings && reference)
			{
				container.AddMember(key, rapidjson::Value(rapidjson::kNullType), allocator);
			}

			if (!strings || (originalBase && strings == reference))
			{
				break;
			}

			rapidjson::Value array(rapidjson::kArrayType);

			for (std::size_t i = 0; i < field.count; ++i)
			{
				if (strings[i])
				{
					array.PushBack(rapidjson::Value(Game::SL_ConvertToString(strings[i]), allocator), allocator);
				}
			}

			container.AddMember(key, array, allocator);
			break;
		}
		case FieldKind::Anims:
		{
			const auto* const anims = FieldAt<const char**>(base, field.offset);
			const char* const* reference = nullptr;

			if (originalBase)
			{
				reference = FieldAt<const char**>(originalBase, field.offset);
			}

			if (!anims && reference)
			{
				container.AddMember(key, rapidjson::Value(rapidjson::kNullType), allocator);
			}

			if (!anims)
			{
				break;
			}

			rapidjson::Value object(rapidjson::kObjectType);

			for (std::size_t i = 0; i < animCount; ++i)
			{
				bool isChanged = !reference;

				if (reference)
				{
					isChanged = (anims[i] == nullptr) != (reference[i] == nullptr) || (anims[i] && std::strcmp(anims[i], reference[i]) != 0);
				}

				if (!isChanged)
				{
					continue;
				}

				object.AddMember(rapidjson::StringRef(animNames[i]), StringOrNull(anims[i], allocator), allocator);

				if (anims[i] && *anims[i])
				{
					DumpChild(Game::ASSET_TYPE_XANIMPARTS, FindLoadedAsset(Game::ASSET_TYPE_XANIMPARTS, anims[i]));
				}
			}

			if (!originalBase || object.MemberCount() > 0)
			{
				container.AddMember(key, object, allocator);
			}

			break;
		}
		case FieldKind::XModels:
		case FieldKind::SoundBuffer:
		{
			void* const* const assets = FieldAt<void**>(base, field.offset);
			void* const* reference = nullptr;

			if (originalBase)
			{
				reference = FieldAt<void**>(originalBase, field.offset);
			}

			if (!assets && reference)
			{
				container.AddMember(key, rapidjson::Value(rapidjson::kNullType), allocator);
			}

			if (!assets || (reference && std::memcmp(assets, reference, field.count * sizeof(void*)) == 0))
			{
				break;
			}

			Game::XAssetType type = Game::ASSET_TYPE_SOUND;

			if (field.kind == FieldKind::XModels)
			{
				type = Game::ASSET_TYPE_XMODEL;
			}

			container.AddMember(key, AssetNames(assets, field.count, type, allocator), allocator);
			break;
		}
		case FieldKind::SoundArray:
		{
			void* const* const sounds = &FieldAt<void*>(base, field.offset);

			if (originalBase && std::memcmp(sounds, &FieldAt<void*>(originalBase, field.offset), field.count * sizeof(void*)) == 0)
			{
				break;
			}

			container.AddMember(key, AssetNames(sounds, field.count, Game::ASSET_TYPE_SOUND, allocator), allocator);
			break;
		}
		case FieldKind::AccuracyGraphs:
		{
			bool shouldWrite = !original;
			rapidjson::Value graphs(rapidjson::kArrayType);

			for (std::size_t i = 0; i < 2; ++i)
			{
				const auto count = weapon->accuracyGraphKnotCount[i];
				graphs.PushBack(KnotFirsts(weapon->accuracyGraphKnots[i], count, allocator), allocator);

				if (original && (count != original->accuracyGraphKnotCount[i] || weapon->accuracyGraphKnots[i] != original->accuracyGraphKnots[i]))
				{
					shouldWrite = true;
				}
			}

			if (shouldWrite)
			{
				container.AddMember(key, graphs, allocator);
			}

			break;
		}
		case FieldKind::OriginalAccuracyGraphs:
		{
			const auto* const weapDef = weapon->weapDef;
			bool shouldWrite = !original;
			rapidjson::Value graphs(rapidjson::kArrayType);

			for (std::size_t i = 0; i < 2; ++i)
			{
				const auto count = std::min(weapDef->originalAccuracyGraphKnotCount[i], weapon->accuracyGraphKnotCount[i]);
				rapidjson::Value graph(rapidjson::kObjectType);
				graph.AddMember("knots", KnotFirsts(weapDef->originalAccuracyGraphKnots[i], count, allocator), allocator);
				graph.AddMember("accuracyGraphName", StringOrNull(weapDef->accuracyGraphName[i], allocator), allocator);
				graphs.PushBack(graph, allocator);

				if (original && weapDef->originalAccuracyGraphKnots[i] != original->weapDef->originalAccuracyGraphKnots[i])
				{
					shouldWrite = true;
				}
			}

			if (shouldWrite)
			{
				container.AddMember(key, graphs, allocator);
			}

			break;
		}
		default:
			break;
		}
	}

	static void WriteFields(rapidjson::Value& container, Utils::JSON::Allocator& allocator, std::span<const JsonField> fields, const Game::WeaponCompleteDef* weapon, const Game::WeaponCompleteDef* original)
	{
		for (const auto& field : fields)
		{
			WriteField(container, allocator, field, weapon, original);
		}
	}

	static void WriteVariant(rapidjson::Value& container, Utils::JSON::Allocator& allocator, const Game::WeaponCompleteDef* weapon, const Game::WeaponCompleteDef* original, const std::string& name)
	{
		container.AddMember("szInternalName", rapidjson::Value(name.data(), allocator), allocator);

		WriteFields(container, allocator, weaponFields, weapon, original);

		const bool hasAltWeapon = weapon->szAltWeaponName && *weapon->szAltWeaponName;

		if (original && hasAltWeapon && weapon->weapDef->inventoryType != Game::WEAPINVENTORY_ALTMODE)
		{
			auto* const altWeapon = FindLoadedAsset(Game::ASSET_TYPE_WEAPON, weapon->szAltWeaponName);

			if (altWeapon)
			{
				DumpChild(Game::ASSET_TYPE_WEAPON, altWeapon);
			}
			else
			{
				Components::Logger::Error("Could not find alt weapon {} for weapon {}!\n", weapon->szAltWeaponName, weapon->szInternalName);
			}
		}

		if (weapon->weapDef->weapClass == Game::WEAPCLASS_TURRET)
		{
			WriteFields(container, allocator, turretFields, weapon, original);
		}

		if (weapon->weapDef->weapClass == Game::WEAPCLASS_ROCKETLAUNCHER)
		{
			WriteFields(container, allocator, rocketFields, weapon, original);
		}

		WriteFields(container, allocator, flagFields, weapon, original);

		const std::string fullName = weapon->szInternalName;
		std::string suffix;

		if (fullName.ends_with("_mp"))
		{
			suffix = "_mp";
		}

		const std::string baseName = fullName.substr(0, fullName.size() - suffix.size());
		rapidjson::Value variants(rapidjson::kArrayType);

		for (const char* attachment : weaponAttachments)
		{
			const auto variantName = std::format("{}_{}{}", baseName, attachment, suffix);
			const auto* const variant = static_cast<const Game::WeaponCompleteDef*>(FindLoadedAsset(Game::ASSET_TYPE_WEAPON, variantName.data()));

			if (!variant || !variant->weapDef)
			{
				continue;
			}

			rapidjson::Value variantContainer(rapidjson::kObjectType);
			WriteVariant(variantContainer, allocator, variant, weapon, attachment);
			variants.PushBack(variantContainer, allocator);
		}

		if (variants.Size() > 0)
		{
			container.AddMember("variants", variants, allocator);
		}
	}

	static void WriteString(Utils::Stream* buffer, const char* text, std::uint32_t* field)
	{
		if (!text)
		{
			return;
		}

		buffer->SaveString(text);
		Utils::Stream::ClearPointer(field);
	}

	static void WriteSound(Utils::Stream* buffer, const Game::snd_alias_list_t* sound, std::uint32_t* field)
	{
		if (!sound)
		{
			return;
		}

		buffer->Align(Utils::Stream::ALIGN_4);
		buffer->SaveMax(sizeof(std::uint32_t));
		buffer->SaveString(sound->aliasName);
		Utils::Stream::ClearPointer(field);
	}

	static void WriteAsset(Components::ZoneBuilder::Zone* builder, Game::XAssetType type, void* asset, std::uint32_t* field)
	{
		if (!asset)
		{
			return;
		}

		*field = builder->SaveSubAsset(type, asset);
	}

	static void WriteModels(Components::ZoneBuilder::Zone* builder, Utils::Stream* buffer, Game::XModel* const* models, std::uint32_t* field)
	{
		if (!models)
		{
			return;
		}

		buffer->Align(Utils::Stream::ALIGN_4);
		auto* const pointerTable = buffer->Dest<std::uint32_t>();
		buffer->SaveMax(modelCount * sizeof(std::uint32_t));

		for (std::size_t i = 0; i < modelCount; ++i)
		{
			if (!models[i])
			{
				pointerTable[i] = 0;
			}
			else
			{
				pointerTable[i] = builder->SaveSubAsset(Game::ASSET_TYPE_XMODEL, models[i]);
			}
		}

		Utils::Stream::ClearPointer(field);
	}

	static void WriteAnims(Utils::Stream* buffer, const char* const* anims, std::uint32_t* field)
	{
		if (!anims)
		{
			return;
		}

		buffer->Align(Utils::Stream::ALIGN_4);
		auto* const pointerTable = buffer->Dest<std::uint32_t>();
		buffer->SaveMax(animCount * sizeof(std::uint32_t));

		for (std::size_t i = 0; i < animCount; ++i)
		{
			if (!anims[i])
			{
				pointerTable[i] = 0;
			}
			else
			{
				buffer->SaveString(anims[i]);
			}
		}

		Utils::Stream::ClearPointer(field);
	}

	static void WriteScriptStrings(Components::ZoneBuilder::Zone* builder, Utils::Stream* buffer, const unsigned short* strings, std::size_t count, std::uint32_t* field)
	{
		if (!strings)
		{
			return;
		}

		buffer->Align(Utils::Stream::ALIGN_2);
		auto* const scriptStringTable = buffer->Dest<unsigned short>();
		buffer->SaveArray(strings, count);

		for (std::size_t i = 0; i < count; ++i)
		{
			builder->MapScriptString(scriptStringTable[i]);
		}

		Utils::Stream::ClearPointer(field);
	}

	static void WriteFloats(Utils::Stream* buffer, const float* values, std::size_t count, std::uint32_t* field)
	{
		if (!values)
		{
			return;
		}

		buffer->Align(Utils::Stream::ALIGN_4);
		buffer->SaveArray(values, count);
		Utils::Stream::ClearPointer(field);
	}

	static void WriteKnots(Utils::Stream* buffer, float (*knots)[2], std::size_t count, std::uint32_t* field)
	{
		if (!knots)
		{
			return;
		}

		buffer->Align(Utils::Stream::ALIGN_4);
		buffer->SaveArray(knots, count);
		Utils::Stream::ClearPointer(field);
	}

	void IWeapon::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->weapon = TryReadWeapon(name, builder);

		if (header->weapon)
		{
			return;
		}

		Components::AssetHandler::ExposeTemporaryAssets(true);
		auto* const parsed = Components::Weapon::ParseWeaponFile(name.data());
		Components::AssetHandler::ExposeTemporaryAssets(false);

		if (parsed)
		{
			header->weapon = &parsed->complete;
			return;
		}

		header->weapon = Components::AssetHandler::FindLoadedAsset(this->GetType(), name.data()).weapon;
	}

	void IWeapon::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const asset = header.weapon;
		auto* const def = asset->weapDef;

		if (asset->hideTags)
		{
			for (std::size_t i = 0; i < hideTagCount && asset->hideTags[i]; ++i)
			{
				builder->AddScriptString(asset->hideTags[i]);
			}
		}

		if (def->notetrackSoundMapKeys)
		{
			for (std::size_t i = 0; i < noteTrackCount && def->notetrackSoundMapKeys[i]; ++i)
			{
				builder->AddScriptString(def->notetrackSoundMapKeys[i]);
			}
		}

		if (def->notetrackSoundMapValues)
		{
			for (std::size_t i = 0; i < noteTrackCount && def->notetrackSoundMapValues[i]; ++i)
			{
				const std::string soundName = Game::SL_ConvertToString(def->notetrackSoundMapValues[i]);
				builder->LoadAssetByName(Game::ASSET_TYPE_SOUND, soundName, false);
				builder->AddScriptString(soundName);
			}
		}

		if (def->notetrackRumbleMapKeys)
		{
			for (std::size_t i = 0; i < noteTrackCount && def->notetrackRumbleMapKeys[i]; ++i)
			{
				builder->AddScriptString(def->notetrackRumbleMapKeys[i]);
			}
		}

		if (def->notetrackRumbleMapValues)
		{
			for (std::size_t i = 0; i < noteTrackCount && def->notetrackRumbleMapValues[i]; ++i)
			{
				const std::string rumbleName = Game::SL_ConvertToString(def->notetrackRumbleMapValues[i]);
				builder->LoadAssetByName(Game::ASSET_TYPE_RAWFILE, std::format("rumble/{}", rumbleName), false);
				builder->AddScriptString(rumbleName);
			}
		}

		Game::Material* const materials[] =
		{
			asset->killIcon, asset->dpadIcon, def->reticleCenter, def->reticleSide, def->hudIcon, def->pickupIcon, def->ammoCounterIcon,
			def->overlayMaterial, def->overlayMaterialLowRes, def->overlayMaterialEMP, def->overlayMaterialEMPLowRes,
		};

		for (auto* const material : materials)
		{
			if (material)
			{
				builder->LoadAsset(Game::ASSET_TYPE_MATERIAL, material);
			}
		}

		if (def->gunXModel)
		{
			for (std::size_t i = 0; i < modelCount; ++i)
			{
				if (def->gunXModel[i])
				{
					builder->LoadAsset(Game::ASSET_TYPE_XMODEL, def->gunXModel[i]);
				}
			}
		}

		if (def->handXModel)
		{
			builder->LoadAsset(Game::ASSET_TYPE_XMODEL, def->handXModel);
		}

		if (def->worldModel)
		{
			for (std::size_t i = 0; i < modelCount; ++i)
			{
				if (def->worldModel[i])
				{
					builder->LoadAsset(Game::ASSET_TYPE_XMODEL, def->worldModel[i]);
				}
			}
		}

		Game::XModel* const models[] = { def->worldClipModel, def->rocketModel, def->knifeModel, def->worldKnifeModel, def->projectileModel };

		for (auto* const model : models)
		{
			if (model)
			{
				builder->LoadAsset(Game::ASSET_TYPE_XMODEL, model);
			}
		}

		if (def->physCollmap)
		{
			builder->LoadAsset(Game::ASSET_TYPE_PHYSCOLLMAP, def->physCollmap);
		}

		if (def->tracerType)
		{
			builder->LoadAsset(Game::ASSET_TYPE_TRACER, def->tracerType);
		}

		Game::FxEffectDef* const effects[] =
		{
			def->viewFlashEffect, def->worldFlashEffect, def->viewShellEjectEffect, def->worldShellEjectEffect, def->viewLastShotEjectEffect,
			def->worldLastShotEjectEffect, def->projExplosionEffect, def->projDudEffect, def->projTrailEffect, def->projBeaconEffect,
			def->projIgnitionEffect, def->turretOverheatEffect,
		};

		for (auto* const effect : effects)
		{
			if (effect)
			{
				builder->LoadAsset(Game::ASSET_TYPE_FX, effect);
			}
		}

		for (const auto& sound : weaponSounds)
		{
			if (def->*sound.field)
			{
				builder->LoadAsset(Game::ASSET_TYPE_SOUND, def->*sound.field, false);
			}
		}

		if (def->bounceSound)
		{
			for (std::size_t i = 0; i < bounceSoundCount; ++i)
			{
				if (def->bounceSound[i])
				{
					builder->LoadAsset(Game::ASSET_TYPE_SOUND, def->bounceSound[i], false);
				}
			}
		}

		Game::snd_alias_list_t* const sounds[] =
		{
			def->projExplosionSound, def->projDudSound, def->projIgnitionSound, def->turretOverheatSound, def->turretBarrelSpinMaxSnd,
			def->turretBarrelSpinUpSnd[0], def->turretBarrelSpinDownSnd[0], def->turretBarrelSpinUpSnd[1], def->turretBarrelSpinDownSnd[1],
			def->turretBarrelSpinUpSnd[2], def->turretBarrelSpinDownSnd[2], def->turretBarrelSpinUpSnd[3], def->turretBarrelSpinDownSnd[3],
			def->missileConeSoundAlias, def->missileConeSoundAliasAtBase,
		};

		for (auto* const sound : sounds)
		{
			if (sound)
			{
				builder->LoadAsset(Game::ASSET_TYPE_SOUND, sound, false);
			}
		}

		const char* const* const animTables[] = { def->szXAnimsLeftHanded, def->szXAnimsRightHanded, asset->szXAnims };

		for (std::size_t i = 0; i < animCount; ++i)
		{
			for (const auto* const anims : animTables)
			{
				if (anims && anims[i] && *anims[i])
				{
					builder->LoadAssetByName(Game::ASSET_TYPE_XANIMPARTS, anims[i], false);
				}
			}
		}

		if (asset->szAltWeaponName && *asset->szAltWeaponName && def->ammoCounterClip != ammoCounterClipAltWeapon)
		{
			builder->LoadAssetByName(Game::ASSET_TYPE_WEAPON, asset->szAltWeaponName, false);
		}
	}

	void IWeapon::WriteWeaponDef(const Game::WeaponCompleteDef* weapon, Components::ZoneBuilder::Zone* builder, Utils::Stream* buffer)
	{
		const auto* const def = weapon->weapDef;
		auto* const dest = buffer->Dest<Game::X86::WeaponDef>();
		const auto converted = Game::X86::Convert(*def);
		buffer->Save(&converted);

		WriteString(buffer, def->szOverlayName, &dest->szOverlayName);
		WriteModels(builder, buffer, def->gunXModel, &dest->gunXModel);
		WriteAsset(builder, Game::ASSET_TYPE_XMODEL, def->handXModel, &dest->handXModel);
		WriteAnims(buffer, def->szXAnimsRightHanded, &dest->szXAnimsRightHanded);
		WriteAnims(buffer, def->szXAnimsLeftHanded, &dest->szXAnimsLeftHanded);
		WriteString(buffer, def->szModeName, &dest->szModeName);
		WriteScriptStrings(builder, buffer, def->notetrackSoundMapKeys, noteTrackCount, &dest->notetrackSoundMapKeys);
		WriteScriptStrings(builder, buffer, def->notetrackSoundMapValues, noteTrackCount, &dest->notetrackSoundMapValues);
		WriteScriptStrings(builder, buffer, def->notetrackRumbleMapKeys, noteTrackCount, &dest->notetrackRumbleMapKeys);
		WriteScriptStrings(builder, buffer, def->notetrackRumbleMapValues, noteTrackCount, &dest->notetrackRumbleMapValues);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->viewFlashEffect, &dest->viewFlashEffect);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->worldFlashEffect, &dest->worldFlashEffect);

		for (const auto& sound : weaponSounds)
		{
			WriteSound(buffer, def->*sound.field, &(dest->*sound.field32));
		}

		if (def->bounceSound)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			auto* const pointerTable = buffer->Dest<std::uint32_t>();
			buffer->SaveMax(bounceSoundCount * sizeof(std::uint32_t));

			for (std::size_t i = 0; i < bounceSoundCount; ++i)
			{
				if (!def->bounceSound[i])
				{
					pointerTable[i] = 0;
				}
				else
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					buffer->SaveMax(sizeof(std::uint32_t));
					buffer->SaveString(def->bounceSound[i]->aliasName);
				}
			}

			Utils::Stream::ClearPointer(&dest->bounceSound);
		}

		WriteAsset(builder, Game::ASSET_TYPE_FX, def->viewShellEjectEffect, &dest->viewShellEjectEffect);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->worldShellEjectEffect, &dest->worldShellEjectEffect);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->viewLastShotEjectEffect, &dest->viewLastShotEjectEffect);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->worldLastShotEjectEffect, &dest->worldLastShotEjectEffect);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->reticleCenter, &dest->reticleCenter);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->reticleSide, &dest->reticleSide);
		WriteModels(builder, buffer, def->worldModel, &dest->worldModel);
		WriteAsset(builder, Game::ASSET_TYPE_XMODEL, def->worldClipModel, &dest->worldClipModel);
		WriteAsset(builder, Game::ASSET_TYPE_XMODEL, def->rocketModel, &dest->rocketModel);
		WriteAsset(builder, Game::ASSET_TYPE_XMODEL, def->knifeModel, &dest->knifeModel);
		WriteAsset(builder, Game::ASSET_TYPE_XMODEL, def->worldKnifeModel, &dest->worldKnifeModel);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->hudIcon, &dest->hudIcon);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->pickupIcon, &dest->pickupIcon);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->ammoCounterIcon, &dest->ammoCounterIcon);
		WriteString(buffer, def->szAmmoName, &dest->szAmmoName);
		WriteString(buffer, def->szClipName, &dest->szClipName);
		WriteString(buffer, def->szSharedAmmoCapName, &dest->szSharedAmmoCapName);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->overlayMaterial, &dest->overlayMaterial);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->overlayMaterialLowRes, &dest->overlayMaterialLowRes);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->overlayMaterialEMP, &dest->overlayMaterialEMP);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, def->overlayMaterialEMPLowRes, &dest->overlayMaterialEMPLowRes);
		WriteAsset(builder, Game::ASSET_TYPE_PHYSCOLLMAP, def->physCollmap, &dest->physCollmap);
		WriteAsset(builder, Game::ASSET_TYPE_XMODEL, def->projectileModel, &dest->projectileModel);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->projExplosionEffect, &dest->projExplosionEffect);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->projDudEffect, &dest->projDudEffect);
		WriteSound(buffer, def->projExplosionSound, &dest->projExplosionSound);
		WriteSound(buffer, def->projDudSound, &dest->projDudSound);
		WriteFloats(buffer, def->parallelBounce, bounceCount, &dest->parallelBounce);
		WriteFloats(buffer, def->perpendicularBounce, bounceCount, &dest->perpendicularBounce);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->projTrailEffect, &dest->projTrailEffect);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->projBeaconEffect, &dest->projBeaconEffect);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->projIgnitionEffect, &dest->projIgnitionEffect);
		WriteSound(buffer, def->projIgnitionSound, &dest->projIgnitionSound);
		WriteString(buffer, def->accuracyGraphName[0], &dest->accuracyGraphName[0]);
		WriteKnots(buffer, def->originalAccuracyGraphKnots[0], weapon->accuracyGraphKnotCount[0], &dest->originalAccuracyGraphKnots[0]);
		WriteString(buffer, def->accuracyGraphName[1], &dest->accuracyGraphName[1]);
		WriteKnots(buffer, def->originalAccuracyGraphKnots[1], weapon->accuracyGraphKnotCount[1], &dest->originalAccuracyGraphKnots[1]);
		WriteString(buffer, def->szUseHintString, &dest->szUseHintString);
		WriteString(buffer, def->dropHintString, &dest->dropHintString);
		WriteString(buffer, def->szScript, &dest->szScript);
		WriteFloats(buffer, def->locationDamageMultipliers, damageLocationCount, &dest->locationDamageMultipliers);
		WriteString(buffer, def->fireRumble, &dest->fireRumble);
		WriteString(buffer, def->meleeImpactRumble, &dest->meleeImpactRumble);
		WriteAsset(builder, Game::ASSET_TYPE_TRACER, def->tracerType, &dest->tracerType);
		WriteSound(buffer, def->turretOverheatSound, &dest->turretOverheatSound);
		WriteAsset(builder, Game::ASSET_TYPE_FX, def->turretOverheatEffect, &dest->turretOverheatEffect);
		WriteString(buffer, def->turretBarrelSpinRumble, &dest->turretBarrelSpinRumble);
		WriteSound(buffer, def->turretBarrelSpinMaxSnd, &dest->turretBarrelSpinMaxSnd);

		for (std::size_t i = 0; i < std::size(def->turretBarrelSpinUpSnd); ++i)
		{
			WriteSound(buffer, def->turretBarrelSpinUpSnd[i], &dest->turretBarrelSpinUpSnd[i]);
		}

		for (std::size_t i = 0; i < std::size(def->turretBarrelSpinDownSnd); ++i)
		{
			WriteSound(buffer, def->turretBarrelSpinDownSnd[i], &dest->turretBarrelSpinDownSnd[i]);
		}

		WriteSound(buffer, def->missileConeSoundAlias, &dest->missileConeSoundAlias);
		WriteSound(buffer, def->missileConeSoundAliasAtBase, &dest->missileConeSoundAliasAtBase);
	}

	void IWeapon::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.weapon;
		auto* const dest = buffer->Dest<Game::X86::WeaponCompleteDef>();
		const auto converted = Game::X86::Convert(*asset);
		buffer->Save(&converted);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->szInternalName)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->szInternalName));
			Utils::Stream::ClearPointer(&dest->szInternalName);
		}

		if (asset->weapDef)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			this->WriteWeaponDef(asset, builder, buffer);
			Utils::Stream::ClearPointer(&dest->weapDef);
		}

		WriteString(buffer, asset->szDisplayName, &dest->szDisplayName);
		WriteScriptStrings(builder, buffer, asset->hideTags, hideTagCount, &dest->hideTags);
		WriteAnims(buffer, asset->szXAnims, &dest->szXAnims);
		WriteString(buffer, asset->szAltWeaponName, &dest->szAltWeaponName);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, asset->killIcon, &dest->killIcon);
		WriteAsset(builder, Game::ASSET_TYPE_MATERIAL, asset->dpadIcon, &dest->dpadIcon);
		WriteKnots(buffer, asset->accuracyGraphKnots[0], asset->accuracyGraphKnotCount[0], &dest->accuracyGraphKnots[0]);
		WriteKnots(buffer, asset->accuracyGraphKnots[1], asset->accuracyGraphKnotCount[1], &dest->accuracyGraphKnots[1]);

		buffer->PopBlock();
	}

	void IWeapon::Dump(Game::XAssetHeader header)
	{
		const auto* const weapon = header.weapon;

		if (!weapon->szInternalName || !weapon->weapDef)
		{
			return;
		}

		rapidjson::Document output(rapidjson::kObjectType);
		auto& allocator = output.GetAllocator();

		output.AddMember("version", weaponJsonVersion, allocator);
		output.AddMember("name", rapidjson::Value(weapon->szInternalName, allocator), allocator);

		rapidjson::Value container(rapidjson::kObjectType);
		WriteVariant(container, allocator, weapon, nullptr, weapon->szInternalName);
		output.AddMember("weapon", container, allocator);

		rapidjson::StringBuffer text;
		rapidjson::PrettyWriter<rapidjson::StringBuffer, rapidjson::UTF8<>, rapidjson::UTF8<>, rapidjson::CrtAllocator, rapidjson::kWriteNanAndInfFlag> writer(text);
		output.Accept(writer);

		Utils::IO::WriteFile(std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetWeaponPath(weapon->szInternalName)), text.GetString());
	}

	IWeapon::IWeapon()
	{
		Components::Command::Add("dumpweapon", [this](const Components::Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const std::string weapon = params->Get(1);
			auto* const header = FindLoadedAsset(this->GetType(), weapon.data());

			if (!header)
			{
				Components::Logger::Print("Could not find weapon {}!\n", weapon);
				return;
			}

			this->Dump({ header });
		});
	}
}
