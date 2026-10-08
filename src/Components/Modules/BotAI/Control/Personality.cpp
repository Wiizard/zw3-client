#include "Components/Modules/BotAI/Control/Personality.hpp"
#include "Components/Modules/BotAI/Control/State.hpp"
#include <cstring>

namespace Components::BotAI
{
	static constexpr float wobbleRateMin = 0.6f;
	static constexpr float wobbleRateMax = 1.5f;
	static constexpr float turnShareMin = 0.12f;
	static constexpr float turnShareMax = 0.28f;
	static constexpr float turnRateScaleMin = 0.8f;
	static constexpr float turnRateScaleMax = 1.2f;

	struct NamePick
	{
		const char* name;
		int weight;
	};

	struct WeaponPick
	{
		const char* name;
		int weight;
		NamePick attachments[4];
	};

	struct KillstreakSet
	{
		const char* streaks[3];
	};

	struct LoadoutPool
	{
		const WeaponPick* primaries;
		int primaryCount;
		const WeaponPick* secondaries;
		int secondaryCount;
		const NamePick* equipment;
		int equipmentCount;
		const NamePick* perk1;
		int perk1Count;
		const NamePick* perk2;
		int perk2Count;
		const NamePick* perk3;
		int perk3Count;
		const NamePick* tacticals;
		int tacticalCount;
		const KillstreakSet* killstreakSets;
		int killstreakSetCount;
	};

	struct TraitBand
	{
		int min;
		int max;
	};

	struct ArchetypeDef
	{
		const char* name;
		int weight;
		TraitBand aggression;
		TraitBand caution;
		TraitBand objectiveDrive;
		TraitBand teamplay;
		TraitBand tricks;
		TraitBand grenadeHabit;
		TraitBand focus;
		TraitBand sightDiscipline;
		TraitBand sprintPercent;
		float hipScaleMin;
		float hipScaleMax;
		int quickscopePercent;
		float closeRange;
		float farRange;
		const LoadoutPool* pool;
	};


	static const WeaponPick rusherPrimaries[] = {
		{ "ump45", 30, { { "silencer", 50 }, { "none", 25 }, { "fmj", 15 }, { "rof", 10 } } },
		{ "mp5k",  20, { { "silencer", 40 }, { "rof", 25 }, { "none", 25 }, { "reflex", 10 } } },
		{ "p90",   18, { { "silencer", 45 }, { "rof", 25 }, { "none", 30 }, { nullptr, 0 } } },
		{ "kriss", 17, { { "silencer", 40 }, { "rof", 25 }, { "none", 35 }, { nullptr, 0 } } },
		{ "uzi",   15, { { "silencer", 40 }, { "none", 40 }, { "acog", 20 }, { nullptr, 0 } } },
	};

	static const WeaponPick rusherSecondaries[] = {
		{ "spas12",    25, { { "none", 60 }, { "grip", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "striker",   15, { { "none", 50 }, { "grip", 50 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "model1887", 15, { { "akimbo", 60 }, { "none", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "ranger",    10, { { "akimbo", 70 }, { "none", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "tmp",       10, { { "akimbo", 60 }, { "silencer", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "glock",     10, { { "akimbo", 60 }, { "silencer", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "usp",       15, { { "akimbo", 40 }, { "tactical", 30 }, { "none", 30 }, { nullptr, 0 } } },
	};

	static const NamePick rusherEquipment[] = {
		{ "semtex_mp", 40 }, { "throwingknife_mp", 35 }, { "frag_grenade_mp", 25 },
	};

	static const NamePick rusherPerk1[] = { { "specialty_marathon", 60 }, { "specialty_fastreload", 40 } };
	static const NamePick rusherPerk2[] = { { "specialty_lightweight", 55 }, { "specialty_bulletdamage", 30 }, { "specialty_coldblooded", 15 } };
	static const NamePick rusherPerk3[] = { { "specialty_extendedmelee", 50 }, { "specialty_heartbreaker", 35 }, { "specialty_bulletaccuracy", 15 } };
	static const NamePick rusherTacticals[] = { { "concussion_grenade", 65 }, { "flash_grenade", 35 } };


	static const WeaponPick slayerPrimaries[] = {
		{ "masada", 20, { { "silencer", 35 }, { "reflex", 25 }, { "none", 25 }, { "acog", 15 } } },
		{ "scar",   16, { { "silencer", 30 }, { "reflex", 30 }, { "none", 25 }, { "eotech", 15 } } },
		{ "tavor",  14, { { "silencer", 35 }, { "reflex", 30 }, { "none", 35 }, { nullptr, 0 } } },
		{ "ak47",   12, { { "none", 40 }, { "silencer", 25 }, { "reflex", 20 }, { "acog", 15 } } },
		{ "m4",     12, { { "reflex", 30 }, { "silencer", 30 }, { "none", 25 }, { "eotech", 15 } } },
		{ "famas",  10, { { "none", 40 }, { "reflex", 30 }, { "silencer", 30 }, { nullptr, 0 } } },
		{ "m16",     8, { { "none", 40 }, { "acog", 30 }, { "reflex", 30 }, { nullptr, 0 } } },
		{ "fn2000",  8, { { "silencer", 40 }, { "none", 35 }, { "reflex", 25 }, { nullptr, 0 } } },
	};

	static const WeaponPick slayerSecondaries[] = {
		{ "usp",         30, { { "none", 40 }, { "akimbo", 30 }, { "tactical", 30 }, { nullptr, 0 } } },
		{ "beretta",     15, { { "none", 50 }, { "tactical", 30 }, { "akimbo", 20 }, { nullptr, 0 } } },
		{ "deserteagle", 15, { { "none", 70 }, { "tactical", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "coltanaconda", 8, { { "none", 60 }, { "tactical", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "spas12",      12, { { "none", 60 }, { "grip", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "pp2000",      10, { { "silencer", 50 }, { "akimbo", 30 }, { "none", 20 }, { nullptr, 0 } } },
		{ "beretta393",  10, { { "silencer", 40 }, { "none", 35 }, { "akimbo", 25 }, { nullptr, 0 } } },
	};

	static const NamePick slayerEquipment[] = {
		{ "frag_grenade_mp", 40 }, { "semtex_mp", 35 }, { "claymore_mp", 15 }, { "throwingknife_mp", 10 },
	};

	static const NamePick slayerPerk1[] = { { "specialty_fastreload", 55 }, { "specialty_scavenger", 30 }, { "specialty_marathon", 15 } };
	static const NamePick slayerPerk2[] = { { "specialty_bulletdamage", 55 }, { "specialty_hardline", 20 }, { "specialty_coldblooded", 25 } };
	static const NamePick slayerPerk3[] = { { "specialty_bulletaccuracy", 40 }, { "specialty_extendedmelee", 30 }, { "specialty_heartbreaker", 30 } };
	static const NamePick slayerTacticals[] = { { "flash_grenade", 45 }, { "concussion_grenade", 55 } };


	static const WeaponPick anchorPrimaries[] = {
		{ "rpd",    22, { { "grip", 45 }, { "reflex", 25 }, { "none", 15 }, { "silencer", 15 } } },
		{ "m240",   16, { { "grip", 50 }, { "reflex", 25 }, { "acog", 25 }, { nullptr, 0 } } },
		{ "aug",    14, { { "grip", 40 }, { "reflex", 30 }, { "none", 30 }, { nullptr, 0 } } },
		{ "mg4",    12, { { "grip", 45 }, { "reflex", 30 }, { "eotech", 25 }, { nullptr, 0 } } },
		{ "sa80",    8, { { "grip", 50 }, { "reflex", 25 }, { "none", 25 }, { nullptr, 0 } } },
		{ "masada", 14, { { "acog", 40 }, { "reflex", 30 }, { "silencer", 30 }, { nullptr, 0 } } },
		{ "scar",   14, { { "acog", 40 }, { "reflex", 35 }, { "none", 25 }, { nullptr, 0 } } },
	};

	static const WeaponPick anchorSecondaries[] = {
		{ "usp",         35, { { "none", 50 }, { "tactical", 30 }, { "akimbo", 20 }, { nullptr, 0 } } },
		{ "deserteagle", 20, { { "none", 70 }, { "tactical", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "beretta",     15, { { "none", 60 }, { "tactical", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "spas12",      15, { { "grip", 50 }, { "none", 50 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "coltanaconda", 15, { { "none", 60 }, { "tactical", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const NamePick anchorEquipment[] = {
		{ "claymore_mp", 50 }, { "c4_mp", 20 }, { "frag_grenade_mp", 30 },
	};

	static const NamePick anchorPerk1[] = { { "specialty_scavenger", 55 }, { "specialty_fastreload", 45 } };
	static const NamePick anchorPerk2[] = { { "specialty_bulletdamage", 50 }, { "specialty_coldblooded", 30 }, { "specialty_hardline", 20 } };
	static const NamePick anchorPerk3[] = { { "specialty_bulletaccuracy", 40 }, { "specialty_detectexplosive", 30 }, { "specialty_heartbreaker", 30 } };
	static const NamePick anchorTacticals[] = { { "concussion_grenade", 65 }, { "flash_grenade", 35 } };


	static const WeaponPick flankerPrimaries[] = {
		{ "ump45",  22, { { "silencer", 80 }, { "none", 20 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "masada", 18, { { "silencer", 85 }, { "none", 15 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "scar",   14, { { "silencer", 85 }, { "none", 15 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "tavor",  14, { { "silencer", 80 }, { "reflex", 20 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "fn2000", 10, { { "silencer", 80 }, { "none", 20 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "p90",    12, { { "silencer", 85 }, { "none", 15 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "kriss",  10, { { "silencer", 80 }, { "none", 20 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const WeaponPick flankerSecondaries[] = {
		{ "usp",        35, { { "silencer", 50 }, { "akimbo", 25 }, { "tactical", 25 }, { nullptr, 0 } } },
		{ "pp2000",     20, { { "silencer", 70 }, { "akimbo", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "beretta",    15, { { "silencer", 70 }, { "none", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "glock",      15, { { "silencer", 60 }, { "akimbo", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "beretta393", 15, { { "silencer", 70 }, { "none", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const NamePick flankerEquipment[] = {
		{ "throwingknife_mp", 35 }, { "semtex_mp", 35 }, { "claymore_mp", 20 }, { "frag_grenade_mp", 10 },
	};

	static const NamePick flankerPerk1[] = { { "specialty_marathon", 50 }, { "specialty_fastreload", 30 }, { "specialty_scavenger", 20 } };
	static const NamePick flankerPerk2[] = { { "specialty_coldblooded", 60 }, { "specialty_lightweight", 30 }, { "specialty_bulletdamage", 10 } };
	static const NamePick flankerPerk3[] = { { "specialty_heartbreaker", 60 }, { "specialty_extendedmelee", 25 }, { "specialty_bulletaccuracy", 15 } };
	static const NamePick flankerTacticals[] = { { "flash_grenade", 55 }, { "concussion_grenade", 45 } };


	static const WeaponPick sniperPrimaries[] = {
		{ "cheytac", 100, { { "xmags", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const WeaponPick sniperSecondaries[] = {
		{ "usp",          35, { { "akimbo", 55 }, { "tactical", 30 }, { "none", 15 }, { nullptr, 0 } } },
		{ "deserteagle",  30, { { "none", 45 }, { "akimbo", 30 }, { "tactical", 25 }, { nullptr, 0 } } },
		{ "coltanaconda", 20, { { "none", 45 }, { "akimbo", 35 }, { "tactical", 20 }, { nullptr, 0 } } },
		{ "beretta",      15, { { "akimbo", 50 }, { "tactical", 30 }, { "none", 20 }, { nullptr, 0 } } },
	};

	static const NamePick sniperEquipment[] = {
		{ "throwingknife_mp", 100 },
	};

	static const NamePick sniperPerk1[] = { { "specialty_fastreload", 93 }, { "specialty_marathon", 7 } };
	static const NamePick sniperPerk2[] = { { "specialty_bulletdamage", 100 } };
	static const NamePick sniperPerk3[] = { { "specialty_bulletaccuracy", 100 } };
	static const NamePick sniperTacticals[] = { { "concussion_grenade", 65 }, { "flash_grenade", 35 } };


	static const WeaponPick supportPrimaries[] = {
		{ "rpd",    22, { { "grip", 50 }, { "reflex", 30 }, { "silencer", 20 }, { nullptr, 0 } } },
		{ "aug",    16, { { "grip", 45 }, { "reflex", 30 }, { "none", 25 }, { nullptr, 0 } } },
		{ "m240",   12, { { "grip", 50 }, { "acog", 25 }, { "reflex", 25 }, { nullptr, 0 } } },
		{ "sa80",   10, { { "grip", 50 }, { "reflex", 50 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "masada", 15, { { "gl", 35 }, { "reflex", 35 }, { "silencer", 30 }, { nullptr, 0 } } },
		{ "ak47",   13, { { "gl", 40 }, { "none", 30 }, { "reflex", 30 }, { nullptr, 0 } } },
		{ "m4",     12, { { "gl", 35 }, { "reflex", 35 }, { "silencer", 30 }, { nullptr, 0 } } },
	};

	static const WeaponPick supportSecondaries[] = {
		{ "usp",         35, { { "none", 50 }, { "tactical", 30 }, { "akimbo", 20 }, { nullptr, 0 } } },
		{ "beretta",     20, { { "none", 60 }, { "tactical", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "deserteagle", 15, { { "none", 70 }, { "tactical", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "spas12",      15, { { "grip", 50 }, { "none", 50 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "aa12",        15, { { "grip", 50 }, { "none", 50 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const NamePick supportEquipment[] = {
		{ "claymore_mp", 40 }, { "frag_grenade_mp", 30 }, { "c4_mp", 15 }, { "semtex_mp", 15 },
	};

	static const NamePick supportPerk1[] = { { "specialty_scavenger", 60 }, { "specialty_fastreload", 40 } };
	static const NamePick supportPerk2[] = { { "specialty_hardline", 40 }, { "specialty_bulletdamage", 35 }, { "specialty_explosivedamage", 25 } };
	static const NamePick supportPerk3[] = { { "specialty_detectexplosive", 35 }, { "specialty_bulletaccuracy", 35 }, { "specialty_localjammer", 30 } };
	static const NamePick supportTacticals[] = { { "concussion_grenade", 60 }, { "flash_grenade", 40 } };


	static const WeaponPick sniperLobbyPrimaries[] = {
		{ "cheytac", 100, { { "xmags", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const WeaponPick sniperLobbySecondaries[] = {
		{ "usp",          40, { { "akimbo", 70 }, { "tactical", 30 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "deserteagle",  25, { { "none", 50 }, { "akimbo", 30 }, { "tactical", 20 }, { nullptr, 0 } } },
		{ "coltanaconda", 20, { { "akimbo", 50 }, { "none", 30 }, { "tactical", 20 }, { nullptr, 0 } } },
		{ "beretta",      15, { { "akimbo", 60 }, { "tactical", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const NamePick sniperLobbyEquipment[] = {
		{ "throwingknife_mp", 100 },
	};

	static const NamePick sniperLobbyPerk1[] = { { "specialty_fastreload", 93 }, { "specialty_marathon", 7 } };
	static const NamePick sniperLobbyPerk2[] = { { "specialty_bulletdamage", 100 } };
	static const NamePick sniperLobbyPerk3[] = { { "specialty_bulletaccuracy", 100 } };
	static const NamePick sniperLobbyTacticals[] = { { "concussion_grenade", 75 }, { "flash_grenade", 25 } };


	static const WeaponPick tuberPrimaries[] = {
		{ "m4",     22, { { "gl", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "masada", 18, { { "gl", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "ak47",   16, { { "gl", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "scar",   16, { { "gl", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "famas",  10, { { "gl", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "tavor",  10, { { "gl", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "m16",     8, { { "gl", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const WeaponPick tuberSecondaries[] = {
		{ "m79", 70, { { "none", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "usp", 30, { { "none", 60 }, { "tactical", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const NamePick tuberEquipment[] = {
		{ "semtex_mp", 50 }, { "frag_grenade_mp", 30 }, { "claymore_mp", 20 },
	};

	static const NamePick tuberPerk1[] = { { "specialty_onemanarmy", 75 }, { "specialty_scavenger", 25 } };
	static const NamePick tuberPerk2[] = { { "specialty_explosivedamage", 100 } };
	static const NamePick tuberPerk3[] = { { "specialty_bulletaccuracy", 40 }, { "specialty_localjammer", 30 }, { "specialty_heartbreaker", 30 } };
	static const NamePick tuberTacticals[] = { { "concussion_grenade", 60 }, { "flash_grenade", 40 } };


	static const WeaponPick kniferPrimaries[] = {
		{ "ump45",  40, { { "silencer", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "p90",    30, { { "silencer", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "spas12", 30, { { "grip", 50 }, { "none", 50 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const WeaponPick kniferSecondaries[] = {
		{ "usp",         60, { { "tactical", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "beretta",     25, { { "tactical", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "deserteagle", 15, { { "tactical", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const NamePick kniferEquipment[] = {
		{ "throwingknife_mp", 70 }, { "semtex_mp", 30 },
	};

	static const NamePick kniferPerk1[] = { { "specialty_marathon", 100 } };
	static const NamePick kniferPerk2[] = { { "specialty_lightweight", 100 } };
	static const NamePick kniferPerk3[] = { { "specialty_extendedmelee", 100 } };
	static const NamePick kniferTacticals[] = { { "concussion_grenade", 70 }, { "flash_grenade", 30 } };


	static const WeaponPick shotgunnerPrimaries[] = {
		{ "ump45", 40, { { "silencer", 60 }, { "none", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "ak47",  30, { { "none", 60 }, { "silencer", 40 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "m4",    30, { { "reflex", 50 }, { "none", 50 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const WeaponPick shotgunnerSecondaries[] = {
		{ "model1887", 45, { { "akimbo", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "ranger",    35, { { "akimbo", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
		{ "spas12",    20, { { "grip", 100 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 } } },
	};

	static const NamePick shotgunnerEquipment[] = {
		{ "semtex_mp", 50 }, { "throwingknife_mp", 30 }, { "frag_grenade_mp", 20 },
	};

	static const NamePick shotgunnerPerk1[] = { { "specialty_marathon", 60 }, { "specialty_fastreload", 40 } };
	static const NamePick shotgunnerPerk2[] = { { "specialty_lightweight", 60 }, { "specialty_bulletdamage", 40 } };
	static const NamePick shotgunnerPerk3[] = { { "specialty_extendedmelee", 40 }, { "specialty_bulletaccuracy", 30 }, { "specialty_heartbreaker", 30 } };
	static const NamePick shotgunnerTacticals[] = { { "concussion_grenade", 60 }, { "flash_grenade", 40 } };


	static const KillstreakSet streakSets[] = {
		{ { "uav", "airdrop", "predator_missile" } },
		{ { "uav", "predator_missile", "harrier_airstrike" } },
		{ { "uav", "counter_uav", "precision_airstrike" } },
		{ { "uav", "predator_missile", "helicopter" } },
		{ { "counter_uav", "harrier_airstrike", "helicopter_flares" } },
		{ { "uav", "precision_airstrike", "ac130" } },
		{ { "uav", "harrier_airstrike", "stealth_airstrike" } },
		{ { "uav", "airdrop", "helicopter" } },
	};

	static const KillstreakSet supportStreakSets[] = {
		{ { "uav", "counter_uav", "airdrop" } },
		{ { "uav", "airdrop", "helicopter_flares" } },
		{ { "counter_uav", "airdrop", "emp" } },
		{ { "uav", "counter_uav", "precision_airstrike" } },
	};

	static const NamePick deathstreakNames[] = {
		{ "specialty_copycat", 70 }, { "specialty_grenadepulldeath", 30 },
	};


	template <int PrimaryCount, int SecondaryCount, int EquipmentCount, int Perk1Count, int Perk2Count,
			  int Perk3Count, int TacticalCount, int StreakCount>
	static constexpr LoadoutPool MakePool(const WeaponPick (&primaries)[PrimaryCount],
										  const WeaponPick (&secondaries)[SecondaryCount],
										  const NamePick (&equipment)[EquipmentCount],
										  const NamePick (&perk1)[Perk1Count],
										  const NamePick (&perk2)[Perk2Count],
										  const NamePick (&perk3)[Perk3Count],
										  const NamePick (&tacticals)[TacticalCount],
										  const KillstreakSet (&streaks)[StreakCount])
	{
		return { primaries, PrimaryCount, secondaries, SecondaryCount, equipment, EquipmentCount,
				 perk1, Perk1Count, perk2, Perk2Count, perk3, Perk3Count, tacticals, TacticalCount,
				 streaks, StreakCount };
	}

	static const LoadoutPool rusherPool = MakePool(rusherPrimaries, rusherSecondaries, rusherEquipment,
		rusherPerk1, rusherPerk2, rusherPerk3, rusherTacticals, streakSets);
	static const LoadoutPool slayerPool = MakePool(slayerPrimaries, slayerSecondaries, slayerEquipment,
		slayerPerk1, slayerPerk2, slayerPerk3, slayerTacticals, streakSets);
	static const LoadoutPool anchorPool = MakePool(anchorPrimaries, anchorSecondaries, anchorEquipment,
		anchorPerk1, anchorPerk2, anchorPerk3, anchorTacticals, streakSets);
	static const LoadoutPool flankerPool = MakePool(flankerPrimaries, flankerSecondaries, flankerEquipment,
		flankerPerk1, flankerPerk2, flankerPerk3, flankerTacticals, streakSets);
	static const LoadoutPool sniperPool = MakePool(sniperPrimaries, sniperSecondaries, sniperEquipment,
		sniperPerk1, sniperPerk2, sniperPerk3, sniperTacticals, streakSets);
	static const LoadoutPool supportPool = MakePool(supportPrimaries, supportSecondaries, supportEquipment,
		supportPerk1, supportPerk2, supportPerk3, supportTacticals, supportStreakSets);
	static const LoadoutPool sniperLobbyPool = MakePool(sniperLobbyPrimaries, sniperLobbySecondaries,
		sniperLobbyEquipment, sniperLobbyPerk1, sniperLobbyPerk2, sniperLobbyPerk3, sniperLobbyTacticals,
		streakSets);
	static const LoadoutPool tuberPool = MakePool(tuberPrimaries, tuberSecondaries, tuberEquipment,
		tuberPerk1, tuberPerk2, tuberPerk3, tuberTacticals, streakSets);
	static const LoadoutPool kniferPool = MakePool(kniferPrimaries, kniferSecondaries, kniferEquipment,
		kniferPerk1, kniferPerk2, kniferPerk3, kniferTacticals, streakSets);
	static const LoadoutPool shotgunnerPool = MakePool(shotgunnerPrimaries, shotgunnerSecondaries, shotgunnerEquipment,
		shotgunnerPerk1, shotgunnerPerk2, shotgunnerPerk3, shotgunnerTacticals, streakSets);


	static const ArchetypeDef archetypeTable[ArchetypeCount] = {
		{ "rusher", 22,
		  { 70, 95 }, { 5, 30 }, { 30, 60 }, { 20, 50 }, { 40, 90 }, { 30, 70 }, { 30, 60 }, { 20, 50 },
		  { 55, 72 }, 1.1f, 1.5f, 0, 250.0f, 800.0f, &rusherPool },
		{ "slayer", 28,
		  { 45, 75 }, { 25, 55 }, { 30, 60 }, { 30, 60 }, { 20, 60 }, { 30, 70 }, { 45, 75 }, { 40, 70 },
		  { 35, 55 }, 0.7f, 1.2f, 0, 400.0f, 1200.0f, &slayerPool },
		{ "anchor", 14,
		  { 15, 40 }, { 65, 95 }, { 50, 85 }, { 40, 70 }, { 5, 30 }, { 40, 80 }, { 60, 90 }, { 55, 85 },
		  { 20, 38 }, 0.6f, 1.0f, 0, 500.0f, 1600.0f, &anchorPool },
		{ "flanker", 12,
		  { 55, 85 }, { 35, 65 }, { 20, 50 }, { 10, 40 }, { 30, 70 }, { 20, 50 }, { 40, 70 }, { 40, 70 },
		  { 45, 62 }, 0.9f, 1.4f, 0, 300.0f, 1000.0f, &flankerPool },
		{ "sniper", 16,
		  { 20, 50 }, { 55, 90 }, { 15, 45 }, { 15, 45 }, { 20, 60 }, { 20, 50 }, { 60, 90 }, { 60, 90 },
		  { 40, 58 }, 0.8f, 1.2f, 100, 500.0f, 2000.0f, &sniperPool },
		{ "support", 8,
		  { 25, 55 }, { 40, 70 }, { 55, 90 }, { 70, 100 }, { 10, 40 }, { 50, 90 }, { 40, 70 }, { 40, 70 },
		  { 22, 42 }, 0.6f, 1.0f, 0, 500.0f, 1600.0f, &supportPool },
		{ "tuber", 6,
		  { 40, 70 }, { 30, 60 }, { 20, 50 }, { 20, 50 }, { 10, 40 }, { 70, 100 }, { 40, 70 }, { 30, 60 },
		  { 35, 55 }, 0.8f, 1.1f, 0, 350.0f, 1400.0f, &tuberPool },
		{ "knifer", 6,
		  { 85, 100 }, { 0, 15 }, { 20, 50 }, { 10, 40 }, { 50, 90 }, { 30, 70 }, { 50, 80 }, { 20, 50 },
		  { 85, 95 }, 1.5f, 1.5f, 0, 0.0f, 60.0f, &kniferPool },
		{ "shotgunner", 6,
		  { 70, 95 }, { 5, 30 }, { 30, 60 }, { 20, 50 }, { 40, 80 }, { 30, 70 }, { 30, 60 }, { 20, 50 },
		  { 55, 72 }, 1.2f, 1.5f, 0, 80.0f, 350.0f, &shotgunnerPool },
	};


	const char* ArchetypeName(int archetype)
	{
		if (archetype < 0 || archetype >= ArchetypeCount)
		{
			return "?";
		}
		return archetypeTable[archetype].name;
	}


	static int RollBand(const TraitBand& band)
	{
		return IrandMs(band.min, band.max);
	}


	static const NamePick& DrawName(const NamePick* picks, int count)
	{
		int total = 0;
		for (int i = 0; i < count; ++i)
		{
			total += picks[i].weight;
		}
		int roll = static_cast<int>(NextRand() % static_cast<unsigned int>(total > 0 ? total : 1));
		for (int i = 0; i < count; ++i)
		{
			roll -= picks[i].weight;
			if (roll < 0)
			{
				return picks[i];
			}
		}
		return picks[count - 1];
	}


	static const WeaponPick& DrawWeapon(const WeaponPick* picks, int count)
	{
		int total = 0;
		for (int i = 0; i < count; ++i)
		{
			total += picks[i].weight;
		}
		int roll = static_cast<int>(NextRand() % static_cast<unsigned int>(total > 0 ? total : 1));
		for (int i = 0; i < count; ++i)
		{
			roll -= picks[i].weight;
			if (roll < 0)
			{
				return picks[i];
			}
		}
		return picks[count - 1];
	}


	static const char* DrawAttachment(const WeaponPick& weapon)
	{
		int count = 0;
		while (count < 4 && weapon.attachments[count].name)
		{
			++count;
		}
		if (count == 0)
		{
			return "none";
		}
		return DrawName(weapon.attachments, count).name;
	}


	static void RollLoadout(const LoadoutPool& pool, Loadout* out)
	{
		const WeaponPick& primary = DrawWeapon(pool.primaries, pool.primaryCount);
		out->primary = primary.name;
		out->primaryAttachment = DrawAttachment(primary);

		const WeaponPick& secondary = DrawWeapon(pool.secondaries, pool.secondaryCount);
		out->secondary = secondary.name;
		out->secondaryAttachment = DrawAttachment(secondary);

		out->equipment = DrawName(pool.equipment, pool.equipmentCount).name;
		out->perk1 = DrawName(pool.perk1, pool.perk1Count).name;
		if (std::strcmp(out->perk1, "specialty_onemanarmy") == 0)
		{
			out->secondary = "onemanarmy";
			out->secondaryAttachment = "none";
		}
		out->perk2 = DrawName(pool.perk2, pool.perk2Count).name;
		out->perk3 = DrawName(pool.perk3, pool.perk3Count).name;
		out->deathstreak = DrawName(deathstreakNames, sizeof(deathstreakNames) / sizeof(deathstreakNames[0])).name;
		out->tactical = DrawName(pool.tacticals, pool.tacticalCount).name;

		const KillstreakSet& streaks = pool.killstreakSets[NextRand() % static_cast<unsigned int>(pool.killstreakSetCount)];
		out->killstreaks[0] = streaks.streaks[0];
		out->killstreaks[1] = streaks.streaks[1];
		out->killstreaks[2] = streaks.streaks[2];
	}


	static int SettingStamp()
	{
		return tuning.botClass + tuning.sniperLobby * 1000;
	}


	bool PersonalitySettingChanged(int clientNum)
	{
		return bots[clientNum].personalitySettingSeen != SettingStamp();
	}


	void RollPersonality(int clientNum)
	{
		BotState& bot = bots[clientNum];
		bot.personalitySettingSeen = SettingStamp();

		const bool sniperLobby = tuning.sniperLobby != 0;
		int archetype = ArchetypeSniper;
		if (!sniperLobby)
		{
			const int forced = tuning.botClass;
			if (forced >= 1 && forced <= ArchetypeCount)
			{
				archetype = forced - 1;
			}
			else
			{
				int totalWeight = 0;
				for (const ArchetypeDef& def : archetypeTable)
				{
					totalWeight += def.weight;
				}
				int roll = static_cast<int>(NextRand() % static_cast<unsigned int>(totalWeight));
				archetype = ArchetypeCount - 1;
				for (int k = 0; k < ArchetypeCount; ++k)
				{
					roll -= archetypeTable[k].weight;
					if (roll < 0)
					{
						archetype = k;
						break;
					}
				}
			}
		}

		const ArchetypeDef& def = archetypeTable[archetype];
		Personality& p = bot.personality;
		p.archetype = archetype;
		p.aggression = RollBand(def.aggression);
		p.caution = RollBand(def.caution);
		p.objectiveDrive = RollBand(def.objectiveDrive);
		p.teamplay = RollBand(def.teamplay);
		p.tricks = RollBand(def.tricks);
		p.grenadeHabit = RollBand(def.grenadeHabit);
		p.focus = RollBand(def.focus);
		p.sightDiscipline = RollBand(def.sightDiscipline);
		p.sprintPercent = RollBand(def.sprintPercent);
		p.hipScale = Flrand(def.hipScaleMin, def.hipScaleMax);
		p.closeRange = def.closeRange;
		p.farRange = def.farRange;
		p.wobblePhase = Flrand(0.0f, 6.2831853f);
		p.wobbleYawRate = Flrand(wobbleRateMin, wobbleRateMax);
		p.wobblePitchRate = Flrand(wobbleRateMin, wobbleRateMax);
		p.turnShare = Flrand(turnShareMin, turnShareMax);
		p.turnRateScale = Flrand(turnRateScaleMin, turnRateScaleMax);
		p.isTuber = archetype == ArchetypeTuber;
		p.isMeleeOnly = archetype == ArchetypeKnifer;
		p.holdsSecondary = archetype == ArchetypeKnifer || archetype == ArchetypeShotgunner;

		p.isQuickscoper = RollPercent(def.quickscopePercent);

		if (p.isQuickscoper)
		{
			p.aggression = IrandMs(55, 90);
			p.caution = IrandMs(15, 45);
			p.tricks = IrandMs(60, 100);
			p.sprintPercent = IrandMs(45, 62);
		}

		if (sniperLobby)
		{
			p.isQuickscoper = true;
			p.aggression = IrandMs(60, 95);
			p.caution = IrandMs(10, 40);
			p.tricks = IrandMs(70, 100);
			p.sprintPercent = IrandMs(45, 62);
			RollLoadout(sniperLobbyPool, &bot.loadout);
		}
		else
		{
			RollLoadout(*def.pool, &bot.loadout);
		}

		p.grenadesPerLife = 1 + p.grenadeHabit / 35;
		if (sniperLobby)
		{
			p.isTuber = false;
			p.isMeleeOnly = false;
			p.holdsSecondary = false;
		}

		BotLog("personality client %d %s%s agg %d cau %d obj %d team %d tricks %d nade %d focus %d sight %d sprint %d hip %.2f loadout %s/%s %s/%s %s %s %s %s %s streaks %s %s %s",
			clientNum, def.name, p.isQuickscoper ? " quickscoper" : "",
			p.aggression, p.caution, p.objectiveDrive, p.teamplay, p.tricks, p.grenadeHabit, p.focus,
			p.sightDiscipline, p.sprintPercent, p.hipScale,
			bot.loadout.primary, bot.loadout.primaryAttachment, bot.loadout.secondary, bot.loadout.secondaryAttachment,
			bot.loadout.equipment, bot.loadout.perk1, bot.loadout.perk2, bot.loadout.perk3, bot.loadout.tactical,
			bot.loadout.killstreaks[0], bot.loadout.killstreaks[1], bot.loadout.killstreaks[2]);
	}


	void RerollLoadout(int clientNum)
	{
		BotState& bot = bots[clientNum];
		const char* keptStreaks[3] = { bot.loadout.killstreaks[0], bot.loadout.killstreaks[1], bot.loadout.killstreaks[2] };
		if (tuning.sniperLobby != 0)
		{
			RollLoadout(sniperLobbyPool, &bot.loadout);
		}
		else
		{
			RollLoadout(*archetypeTable[bot.personality.archetype].pool, &bot.loadout);
		}
		for (int i = 0; i < 3; ++i)
		{
			bot.loadout.killstreaks[i] = keptStreaks[i];
		}
		BotLog("class client %d changes to %s/%s %s/%s %s %s %s %s", clientNum, bot.loadout.primary,
			bot.loadout.primaryAttachment, bot.loadout.secondary, bot.loadout.secondaryAttachment,
			bot.loadout.equipment, bot.loadout.perk1, bot.loadout.perk2, bot.loadout.perk3);
	}
}
