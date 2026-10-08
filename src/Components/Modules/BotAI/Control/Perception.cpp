#include "Components/Modules/BotAI/Control/Ai.hpp"
#include <cmath>

namespace Components::BotAI
{
	bool IsFiniteVec(const float* value)
	{
		return std::isfinite(value[0]) && std::isfinite(value[1]) && std::isfinite(value[2]);
	}


	PlayerView ReadPlayerView(int clientNum)
	{
		PlayerView view = {};

		const char* entity = reinterpret_cast<char*>(g_entities) + gentityStride * clientNum;
		const char* client = *reinterpret_cast<char* const*>(entity + gentityClient);
		if (!client)
		{
			return view;
		}

		view.team = *reinterpret_cast<const int*>(client + gclientTeam);

		view.isPlaying = *reinterpret_cast<const int*>(client + gclientConnected) != 0
			&& *reinterpret_cast<const int*>(client + gclientSessionState) == 0
			&& *reinterpret_cast<const int*>(entity + gentityHealth) > 0;

		const float* origin = reinterpret_cast<const float*>(client + psOrigin);
		view.eye[0] = origin[0];
		view.eye[1] = origin[1];
		view.eye[2] = origin[2] + *reinterpret_cast<const float*>(client + psViewHeight);
		view.feetZ = origin[2];
		const int pmFlags = *reinterpret_cast<const int*>(client + psPmFlags);
		view.isOnLadder = (pmFlags & pmFlagLadder) != 0;
		view.isMantling = (pmFlags & pmFlagMantle) != 0;

		if (!IsFiniteVec(view.eye))
		{
			view.isPlaying = false;
		}
		return view;
	}


	float AngleDelta(float from, float to)
	{
		float delta = std::fmod(to - from + 180.0f, 360.0f);
		if (delta < 0.0f)
		{
			delta += 360.0f;
		}
		return delta - 180.0f;
	}


	float TurnStep(float delta, float share, float rateDeg)
	{
		float step = delta * share;
		if (step > rateDeg)
		{
			step = rateDeg;
		}
		if (step < -rateDeg)
		{
			step = -rateDeg;
		}
		if (std::fabs(delta) > turnMinDeg && std::fabs(step) < turnMinDeg)
		{
			step = delta > 0.0f ? turnMinDeg : -turnMinDeg;
		}
		return step;
	}


	void NoteTurn(BotState& bot, const char* tag, float yaw)
	{
		if (bot.turnCount < turnNoteSlots)
		{
			bot.turnTags[bot.turnCount] = tag;
			bot.turnYaws[bot.turnCount] = yaw;
		}
		++bot.turnCount;
	}


	void TurnView(int clientNum, BotInput& input, float yaw, float pitch, const char* tag)
	{
		BotState& bot = bots[clientNum];
		NoteTurn(bot, tag, yaw);
		const Personality& personality = bot.personality;

		float share = navTurnFraction;
		if (personality.turnShare > 0.0f)
		{
			share = personality.turnShare;
		}
		float rate = turnRateDegBySkill[bot.skillIndex];
		if (personality.turnRateScale > 0.0f)
		{
			rate *= personality.turnRateScale;
		}
		const bool isReacting = ServerTimeMs() < bot.hurtUntilTime;
		if (!isReacting && rate > casualTurnMaxDeg)
		{
			rate = casualTurnMaxDeg;
		}

		const float yawDelta = AngleDelta(input.angles[1], yaw);
		if (std::fabs(yawDelta) > turnEaseStartDeg)
		{
			bot.turnEase += turnEaseStep;
			if (bot.turnEase > 1.0f)
			{
				bot.turnEase = 1.0f;
			}
		}
		else
		{
			bot.turnEase -= turnEaseDecay;
		}
		if (bot.turnEase < turnEaseMin)
		{
			bot.turnEase = turnEaseMin;
		}
		share *= bot.turnEase;

		const float yawStep = TurnStep(yawDelta, share, rate);
		input.angles[1] += yawStep;
		input.angles[0] += TurnStep(AngleDelta(input.angles[0], pitch), share, rate);
		input.angles[2] = 0.0f;
		KeepMoveHeading(input, yawStep);
	}


	void KeepMoveHeading(BotInput& input, float yawStepDeg)
	{
		if (input.forward == 0 && input.right == 0)
		{
			return;
		}
		const float forward = static_cast<float>(input.forward);
		const float right = static_cast<float>(input.right);
		const float magnitude = std::sqrt(forward * forward + right * right);
		const float relative = std::atan2(-right, forward) - yawStepDeg / radToDeg;
		float newForward = std::cos(relative) * magnitude;
		float newRight = -std::sin(relative) * magnitude;
		if (newForward > 127.0f)
		{
			newForward = 127.0f;
		}
		if (newForward < -127.0f)
		{
			newForward = -127.0f;
		}
		if (newRight > 127.0f)
		{
			newRight = 127.0f;
		}
		if (newRight < -127.0f)
		{
			newRight = -127.0f;
		}
		input.forward = static_cast<signed char>(newForward);
		input.right = static_cast<signed char>(newRight);
	}


	void MoveTowards(float travelYaw, float viewYaw, BotInput& input)
	{
		const float relative = AngleDelta(viewYaw, travelYaw) / radToDeg;
		input.forward = static_cast<signed char>(std::cos(relative) * 127.0f);
		input.right = static_cast<signed char>(-std::sin(relative) * 127.0f);
	}


	void FeetOf(const PlayerView& view, float out[3])
	{
		out[0] = view.eye[0];
		out[1] = view.eye[1];
		out[2] = view.feetZ;
	}


	bool HasSubstring(const char* text, const char* needle)
	{
		for (int i = 0; text[i]; ++i)
		{
			int j = 0;
			while (needle[j] && text[i + j] == needle[j])
			{
				++j;
			}
			if (!needle[j])
			{
				return true;
			}
		}
		return false;
	}


	bool IsEnemy(const PlayerView& self, const PlayerView& other)
	{
		if (!other.isPlaying)
		{
			return false;
		}
		return self.team == 0 || other.team != self.team;
	}


	int SightOn(int clientNum, const PlayerView& self, const PlayerView& other, bool useTraces)
	{
		if (!useTraces)
		{
			return 1;
		}
		if (SightLine(self.eye, other.eye, clientNum))
		{
			return 1;
		}

		float shin[3];
		FeetOf(other, shin);
		shin[2] += shinRise;
		if (SightLine(self.eye, shin, clientNum))
		{
			return 2;
		}
		return 0;
	}


	bool IsHuman(int clientNum)
	{
		const char* slot = reinterpret_cast<char*>(svs_clients) + svClientStride * clientNum;
		return *reinterpret_cast<const int*>(slot + svClientIsTest) == 0;
	}


	float ViewYawOf(int clientNum)
	{
		const float* angles = reinterpret_cast<const float*>(PlayerStateOf(clientNum) + psViewAngles);
		return angles[1];
	}


	bool IsShellshocked(int clientNum, int now)
	{
		const char* playerState = PlayerStateOf(clientNum);
		const int shellshockEnd = *reinterpret_cast<const int*>(playerState + psShellshockTime)
			+ *reinterpret_cast<const int*>(playerState + psShellshockDuration);
		return (*reinterpret_cast<const int*>(playerState + psPmFlags) & 0x8000) != 0 && now < shellshockEnd;
	}


	bool IsViewOpen(int clientNum, const PlayerView& self, float yawDeg, float range)
	{
		const float rad = yawDeg / radToDeg;
		const float point[3] = { self.eye[0] + std::cos(rad) * range, self.eye[1] + std::sin(rad) * range, self.eye[2] };
		return SightLine(self.eye, point, clientNum);
	}


	float DistanceSq2D(const float* a, const float* b)
	{
		const float dx = b[0] - a[0];
		const float dy = b[1] - a[1];
		return dx * dx + dy * dy;
	}


	float ConeDot(const float* from, const float* to, float viewYaw, float viewPitch)
	{
		float dir[3] = { to[0] - from[0], to[1] - from[1], to[2] - from[2] };
		const float length = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
		if (length < 0.001f)
		{
			return 1.0f;
		}
		dir[0] /= length;
		dir[1] /= length;
		dir[2] /= length;

		const float yaw = viewYaw / radToDeg;
		const float pitch = viewPitch / radToDeg;
		const float forward[3] = {
			std::cos(pitch) * std::cos(yaw),
			std::cos(pitch) * std::sin(yaw),
			-std::sin(pitch),
		};

		return dir[0] * forward[0] + dir[1] * forward[1] + dir[2] * forward[2];
	}
}
