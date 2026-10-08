#pragma once

namespace Components::BotAI
{
	static constexpr int fireFullAuto     = 0;
	static constexpr int fireSingle       = 1;
	static constexpr int fireBurst        = 2;
	static constexpr int fireDoubleBarrel = 3;

	struct ViewKick
	{
		float pitchMin;
		float pitchMax;
		float yawMin;
		float yawMax;
		float centerSpeed;
	};

	struct HipSpread
	{
		float standMin;
		float duckedMin;
		float proneMin;
		float standMax;
		float duckedMax;
		float proneMax;
		float decayRate;
		float fireAdd;
		float turnAdd;
		float moveAdd;
		float duckedDecay;
		float proneDecay;
	};

	struct GunStats
	{
		const char* gun;
		int fireType;
		int clipSize;
		int xmagsClipSize;
		int fireTimeMs;
		int rofFireTimeMs;
		int rechamberTimeMs;
		int reloadTimeMs;
		int reloadEmptyTimeMs;
		int reloadAddTimeMs;
		int dropTimeMs;
		int raiseTimeMs;
		int quickDropTimeMs;
		int quickRaiseTimeMs;
		int adsInTimeMs;
		int adsOutTimeMs;
		int scopeAdsInTimeMs;
		int scopeAdsOutTimeMs;
		float adsSpread;
		int damage;
		int minDamage;
		float maxDamageRange;
		float minDamageRange;
		float silencedMaxDamageRange;
		float silencedMinDamageRange;
		float headMulti;
		int pellets;
		HipSpread hip;
		HipSpread akimboHip;
		ViewKick adsKick;
		ViewKick hipKick;
		float acogCenterDelta;
		float thermalCenterDelta;
		float gripCenterDelta;
	};

	bool TryGetGunStats(const char* weaponName, GunStats* out);

	bool TryGetViewKick(const char* weaponName, bool isAds, ViewKick* out);
}
