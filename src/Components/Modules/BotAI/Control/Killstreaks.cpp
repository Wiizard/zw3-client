#include "Components/Modules/BotAI/Control/Ai.hpp"
#include <cmath>

namespace Components::BotAI
{
	static constexpr int ksIdle         = 0;
	static constexpr int ksRaising      = 1;
	static constexpr int ksAwaitMap     = 2;
	static constexpr int ksSendLocation = 3;
	static constexpr int ksAwaitRide    = 4;
	static constexpr int ksRiding       = 5;
	static constexpr int ksFiring       = 6;
	static constexpr int ksCancel       = 7;
	static constexpr int ksRestow       = 8;
	static constexpr int ksCategoryInstant = 0;
	static constexpr int ksCategoryMap     = 1;
	static constexpr int ksCategoryRide    = 2;
	static constexpr int ksCooldownFrames = 200;

	static constexpr int ksTapFrames      = 10;
	static constexpr int ksFiringFrames   = 60;
	static constexpr float skyProbeLift = 32.0f;
	static constexpr float skyProbeReach = 8000.0f;
	static constexpr float markerLandUnits = 450.0f;
	static constexpr int ksRemoteHoldFrames = 32;


	static int ReadRemoteControlEnt(int clientNum)
	{
		return *reinterpret_cast<const int*>(PlayerStateOf(clientNum) + psRemoteControlEnt);
	}


	static bool IsKillstreakWeaponName(const char* name)
	{
		if (!name)
		{
			return false;
		}
		return HasSubstring(name, "killstreak_") || HasSubstring(name, "airdrop_marker");
	}


	static int KillstreakCategory(const char* name)
	{
		if (HasSubstring(name, "airstrike"))
		{
			return ksCategoryMap;
		}
		if (HasSubstring(name, "predator") || HasSubstring(name, "ac130"))
		{
			return ksCategoryRide;
		}
		return ksCategoryInstant;
	}


	static unsigned short FindBankedKillstreak(const char* playerState)
	{
		const int found = FindHeldWeapon(playerState, [](int index)
		{
			return IsKillstreakWeaponName(WeaponNameOf(index));
		});
		return static_cast<unsigned short>(found);
	}


	static bool StillHoldsWeapon(const char* playerState, unsigned short weapon)
	{
		const int found = FindHeldWeapon(playerState, [weapon](int index)
		{
			return index == weapon;
		});
		return found != 0;
	}


	static bool PickStrikeTarget(int clientNum, const PlayerView& self, float out[3])
	{
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		float best = 0.0f;
		bool found = false;

		for (int i = 0; i < numClients; ++i)
		{
			if (i == clientNum)
			{
				continue;
			}

			const PlayerView other = ReadPlayerView(i);
			if (!IsEnemy(self, other))
			{
				continue;
			}

			const float dx = other.eye[0] - self.eye[0];
			const float dy = other.eye[1] - self.eye[1];
			const float distanceSq = dx * dx + dy * dy;
			if (!found || distanceSq < best)
			{
				best = distanceSq;
				out[0] = other.eye[0];
				out[1] = other.eye[1];
				out[2] = other.eye[2];
				found = true;
			}
		}
		return found;
	}


	static signed char FractionToLocByte(float fraction)
	{
		if (fraction < 0.0f)
		{
			fraction = 0.0f;
		}
		if (fraction > 1.0f)
		{
			fraction = 1.0f;
		}

		const int value = static_cast<int>(fraction * 255.0f) - 128;
		if (value < -128)
		{
			return -128;
		}
		if (value > 127)
		{
			return 127;
		}
		return static_cast<signed char>(value);
	}


	static void EncodeStrikeLocation(const float* world, signed char out[3])
	{
		if (!botsCompassMapped)
		{
			out[0] = FractionToLocByte(0.5f);
			out[1] = FractionToLocByte(0.5f);
			out[2] = static_cast<signed char>(static_cast<int>(NextRand() % 255u) - 127);
			return;
		}

		const float x0 = 0.0f;
		const float y0 = 0.0f;
		const float width = 0.0f;
		const float height = 0.0f;
		const float northSin = 0.0f;
		const float northCos = 0.0f;

		const float relX = world[0] - x0;
		const float relY = y0 - world[1];
		const float u = northCos * relX + northSin * relY;
		const float v = northCos * relY - northSin * relX;

		out[0] = FractionToLocByte(width != 0.0f ? u / width : 0.5f);
		out[1] = FractionToLocByte(height != 0.0f ? v / height : 0.5f);

		out[2] = static_cast<signed char>(static_cast<int>(NextRand() % 255u) - 127);
	}


	static void SteerRemoteRide(int clientNum, const PlayerView& self, BotInput& input)
	{
		input.remoteAngles[0] = 0;
		input.remoteAngles[1] = 0;

		const int index = ReadRemoteControlEnt(clientNum) & 0x7FF;
		if (index <= 0 || index >= 2046)
		{
			return;
		}

		const char* rocket = reinterpret_cast<char*>(g_entities) + gentityStride * index;
		const float* origin = reinterpret_cast<const float*>(rocket + gentityOrigin);
		const float* angles = reinterpret_cast<const float*>(rocket + gentityAngles);
		if (!IsFiniteVec(origin) || !IsFiniteVec(angles))
		{
			return;
		}

		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		PlayerView target = {};
		float bestDot = 0.6f;
		bool found = false;

		for (int i = 0; i < numClients; ++i)
		{
			if (i == clientNum)
			{
				continue;
			}

			const PlayerView other = ReadPlayerView(i);
			if (!IsEnemy(self, other))
			{
				continue;
			}

			const float dot = ConeDot(origin, other.eye, angles[1], angles[0]);
			if (dot > bestDot)
			{
				bestDot = dot;
				target = other;
				found = true;
			}
		}

		if (!found)
		{
			return;
		}

		if (!SightLine(origin, target.eye, index))
		{
			return;
		}

		const float dx = target.eye[0] - origin[0];
		const float dy = target.eye[1] - origin[1];
		const float dz = target.eye[2] - origin[2];
		const float flat = std::sqrt(dx * dx + dy * dy);

		const float desiredYaw = std::atan2(dy, dx) * radToDeg;
		const float desiredPitch = -std::atan2(dz, flat) * radToDeg;

		const float errPitch = AngleDelta(angles[0], desiredPitch);
		const float errYaw = AngleDelta(angles[1], desiredYaw);
		const float errLen = std::sqrt(errPitch * errPitch + errYaw * errYaw);
		if (errLen < 0.5f)
		{
			return;
		}

		float speed = 100.0f;
		if (errLen < 10.0f)
		{
			speed = (errLen / 10.0f) * 100.0f;
		}

		input.remoteAngles[0] = static_cast<signed char>(errPitch / errLen * speed);
		input.remoteAngles[1] = static_cast<signed char>(errYaw / errLen * speed);
	}


	static bool IsUnderOpenSky(const float* point)
	{
		const float start[3] = { point[0], point[1], point[2] + skyProbeLift };
		const float end[3] = { point[0], point[1], point[2] + skyProbeReach };
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		const int ignore[4] = { -1, -1, 0, 0 };
		unsigned char trace[128] = {};
		SV_Trace(trace, start, end, bounds, ignore, maskSight, 0, nullptr, 1);
		if (*reinterpret_cast<const float*>(trace + traceFraction) >= 1.0f)
		{
			return true;
		}
		return (*reinterpret_cast<const int*>(trace + traceSurfaceFlags) & surfaceSky) != 0;
	}


	static bool IsDropSpotOpen(const PlayerView& self, float viewYaw)
	{
		const float feet[3] = { self.eye[0], self.eye[1], self.feetZ };
		const float rad = viewYaw / radToDeg;
		const float ahead[3] = { self.eye[0] + std::cos(rad) * markerLandUnits, self.eye[1] + std::sin(rad) * markerLandUnits,
								 self.feetZ };
		return IsUnderOpenSky(feet) && IsUnderOpenSky(ahead);
	}


	static void FinishKillstreak(int clientNum, BotInput& input)
	{
		bots[clientNum].ksPhase = ksIdle;
		bots[clientNum].ksCooldown = ksCooldownFrames;
		input.weapon = 0;
		input.remoteAngles[0] = 0;
		input.remoteAngles[1] = 0;
	}


	static void BeginRestow(int clientNum, const char* playerState, BotInput& input)
	{
		const unsigned short realWeapon = FirstHeldRealWeapon(playerState);
		if (!realWeapon)
		{
			FinishKillstreak(clientNum, input);
			return;
		}

		bots[clientNum].ksWeapon = realWeapon;
		bots[clientNum].ksPhase = ksRestow;
		bots[clientNum].ksTimer = 60;
		input.remoteAngles[0] = 0;
		input.remoteAngles[1] = 0;
	}


	bool RunKillstreakAi(int clientNum, BotInput& input)
	{
		BotState& bot = bots[clientNum];
		if (bot.ksCooldown > 0)
		{
			--bot.ksCooldown;
		}

		const char* playerState = PlayerStateOf(clientNum);
		const PlayerView self = ReadPlayerView(clientNum);
		if (!self.isPlaying)
		{
			bot.ksPhase = ksIdle;
			return false;
		}

		if (bot.ksPhase == ksIdle)
		{
			const bool mayStart = bot.ksCooldown == 0
				&& bot.targetClient < 0
				&& bot.grenadeThrowFrames == 0
				&& bot.tacticalThrowFrames == 0
				&& bot.yyFrames == 0
				&& ((debugTick + clientNum) % 20) == 0;
			if (!mayStart)
			{
				return false;
			}

			const unsigned short streakWeapon = FindBankedKillstreak(playerState);
			if (!streakWeapon)
			{
				return false;
			}
			const char* bankedName = WeaponNameOf(streakWeapon);
			if (bankedName && HasSubstring(bankedName, "airdrop_marker") && !IsDropSpotOpen(self, input.angles[1]))
			{
				return false;
			}

			bot.ksWeapon = streakWeapon;
			bot.ksCategory = KillstreakCategory(
				WeaponNameOf(streakWeapon));

			float strikePos[3];
			if (bot.ksCategory == ksCategoryMap && !PickStrikeTarget(clientNum, self, strikePos))
			{
				return false;
			}

			bot.ksPhase = ksRaising;
			bot.ksTimer = 100;
			bot.ksRemoteBaseline = ReadRemoteControlEnt(clientNum);
			bot.ksBoostFrames = 30;
		}

		const bool isRide = bot.ksPhase == ksAwaitRide || bot.ksPhase == ksRiding;
		input.buttons = 0;
		input.forward = 0;
		input.right = 0;
		if (!isRide)
		{
			Idle(clientNum, self, input, true);
			input.buttons &= (cmdButtonCrouch | cmdButtonProne | cmdButtonUp);
		}
		input.weapon = bot.ksWeapon;

		--bot.ksTimer;
		if (bot.ksTimer <= 0)
		{
			if (bot.ksPhase == ksRestow)
			{
				FinishKillstreak(clientNum, input);
				return false;
			}
			BeginRestow(clientNum, playerState, input);
			return true;
		}

		if (bot.ksPhase != ksAwaitRide && bot.ksPhase != ksRiding && bot.ksPhase != ksRestow
			&& !StillHoldsWeapon(playerState, bot.ksWeapon))
		{
			BeginRestow(clientNum, playerState, input);
			return true;
		}

		const unsigned short heldWeapon =
			*reinterpret_cast<const unsigned short*>(playerState + psWeapon);

		switch (bot.ksPhase)
		{
		case ksRaising:
			if (heldWeapon == bot.ksWeapon)
			{
				if (bot.ksCategory == ksCategoryMap)
				{
					bot.ksPhase = ksAwaitMap;
					bot.ksTimer = 80;
				}
				else if (bot.ksCategory == ksCategoryRide)
				{
					bot.ksPhase = ksAwaitRide;
					bot.ksTimer = 100;
				}
				else
				{
					bot.ksPhase = ksFiring;
					bot.ksTimer = ksFiringFrames;
					const char* streakName = WeaponNameOf(bot.ksWeapon);
					const bool needsTrigger = streakName
						&& (HasSubstring(streakName, "airdrop_marker") || HasSubstring(streakName, "sentry"));
					if (!needsTrigger)
					{
						bot.ksTimer = ksRemoteHoldFrames;
					}
				}
			}
			return true;

		case ksFiring:
			{
				const char* streakName =
					WeaponNameOf(bot.ksWeapon);
				const bool needsTrigger = streakName
					&& (HasSubstring(streakName, "airdrop_marker") || HasSubstring(streakName, "sentry"));
				if (needsTrigger && (bot.ksTimer % ksTapFrames) == 0)
				{
					input.buttons |= cmdButtonAttack;
				}
				if (streakName && HasSubstring(streakName, "airdrop_marker"))
				{
					if (!IsDropSpotOpen(self, input.angles[1]))
					{
						BeginRestow(clientNum, playerState, input);
						return true;
					}
					input.right = 0;
					input.angles[0] = 0.0f;
					input.forward = -80;
					bot.wasWalking = false;
					bot.stuckFrames = 0;
					bot.approachStall = 0;
				}
			}
			return true;

		case ksAwaitMap:
			if (*reinterpret_cast<const int*>(playerState + psLocationSelection) != 0)
			{
				float strikePos[3];
				if (PickStrikeTarget(clientNum, self, strikePos))
				{
					EncodeStrikeLocation(strikePos, input.selectedLocation);
					bot.ksPhase = ksSendLocation;
					bot.ksTimer = 6;
				}
				else
				{
					bot.ksPhase = ksCancel;
					bot.ksTimer = 6;
				}
			}
			return true;

		case ksSendLocation:
			input.buttons |= cmdButtonSelectLocation;
			return true;

		case ksCancel:
			input.buttons |= cmdButtonCancelLocation;
			return true;

		case ksAwaitRide:
			if (ReadRemoteControlEnt(clientNum) != bot.ksRemoteBaseline)
			{
				bot.ksPhase = ksRiding;
				bot.ksTimer = 600;
			}
			return true;

		case ksRestow:
			if (heldWeapon == bot.ksWeapon)
			{
				FinishKillstreak(clientNum, input);
				return false;
			}
			return true;

		case ksRiding:
			{
				input.weapon = 0;

				const int remoteControlEnt = ReadRemoteControlEnt(clientNum);
				if (remoteControlEnt == bot.ksRemoteBaseline || remoteControlEnt == entityNumNone)
				{
					BeginRestow(clientNum, playerState, input);
					return true;
				}

				input.buttons |= cmdButtonRemoteAngles;
				if (bot.ksBoostFrames > 0)
				{
					--bot.ksBoostFrames;
				}
				else
				{
					input.buttons |= cmdButtonAttack;
				}

				SteerRemoteRide(clientNum, self, input);
			}
			return true;

		default:
			return false;
		}
	}
}
