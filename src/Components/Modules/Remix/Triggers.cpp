#include "STDInclude.hpp"

#include "Triggers.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <deque>
#include <mutex>

#include "Core.hpp"
#include "Engine.hpp"
#include "Iw4.hpp"
#include "MapSettings.hpp"
#include "Options.hpp"
#include "Outdoor.hpp"
#include "PatchCalls.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	using PlaySoundAliasInternal = int(__fastcall*)(std::uintptr_t, std::uintptr_t, float, float, float, int, const float*,
		int*, int, char, float, int, int);

	static constexpr std::size_t maxPendingSounds = 64;
	static constexpr std::size_t maxRecentSounds = 32;

	struct RuleState
	{
		bool isActive = false;
		ULONGLONG lastHeardMs = 0;
		bool hasHeard = false;
		unsigned long long activations = 0;
	};

	static PlaySoundAliasInternal originalPlaySoundAlias = nullptr;
	static std::mutex soundMutex;
	static std::vector<std::string> pendingSounds;
	static std::deque<std::string> recentSounds;
	static std::vector<RuleState> ruleStates;
	static unsigned rulesVersion = 0;
	static TriggerStats stats;

	static int __fastcall PlaySoundAliasDetour(std::uintptr_t alias, std::uintptr_t aliasSecond, float lerp,
		float volumeScale, float pitch, int entity, const float* origin, int* handle, int timeshift, char isMaster,
		float masterVolume, int fadeTime, int chainDepth)
	{
		const auto* name = ReadAt<const char*>(alias + Iw4::soundAliasName);

		if (name)
		{
			const std::lock_guard<std::mutex> lock(soundMutex);

			if (pendingSounds.size() < maxPendingSounds)
			{
				pendingSounds.emplace_back(name);
			}
		}

		return originalPlaySoundAlias(alias, aliasSecond, lerp, volumeScale, pitch, entity, origin, handle, timeshift,
			isMaster, masterVolume, fadeTime, chainDepth);
	}

	bool InstallTriggerHooks()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();
		constexpr int callCount = static_cast<int>(std::size(build.SND_PlaySoundAlias_InternalCalls));

		stats.isSoundHooked = TryPatchCallGroup(build.SND_PlaySoundAlias_InternalCalls, callCount,
			reinterpret_cast<const void*>(&PlaySoundAliasDetour), reinterpret_cast<void**>(&originalPlaySoundAlias));

		Log() << "remix: sound triggers see every sound alias the game starts (" << stats.isSoundHooked << ", "
			  << callCount << " SND_PlaySoundAlias_Internal calls)";

		return stats.isSoundHooked;
	}

	static bool StartsWithIgnoringCase(const std::string& text, const std::string& prefix)
	{
		if (prefix.size() > text.size())
		{
			return false;
		}

		return _strnicmp(text.c_str(), prefix.c_str(), prefix.size()) == 0;
	}

	static std::string ReadVisionName()
	{
		const std::uintptr_t visionName = Engine::CurrentBuild().visionNameNaked;

		if (visionName == 0)
		{
			return {};
		}

		const auto* text = reinterpret_cast<const char*>(Engine::RebaseAddress(visionName));
		return std::string(text, strnlen(text, Iw4::visionNameBytes));
	}

	static bool IsInsideArea(const TriggerRule& rule, const float* camera)
	{
		if (rule.hasBox)
		{
			bool isInside = true;

			for (int axis = 0; axis < 3; ++axis)
			{
				const float low = std::min(rule.boxMin[axis], rule.boxMax[axis]);
				const float high = std::max(rule.boxMin[axis], rule.boxMax[axis]);

				if (camera[axis] < low || camera[axis] > high)
				{
					isInside = false;
				}
			}

			if (isInside)
			{
				return true;
			}
		}

		if (rule.hasSphere)
		{
			const float offset[3] = { camera[0] - rule.centre[0], camera[1] - rule.centre[1], camera[2] - rule.centre[2] };
			return offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2] <= rule.radius * rule.radius;
		}

		return false;
	}

	static void ResetRules()
	{
		for (std::size_t i = 0; i < ruleStates.size(); ++i)
		{
			ClearLayer(firstTriggerLayer + static_cast<int>(i), 0.0f);
		}

		ruleStates.assign(GetMapSettings().triggers.size(), RuleState{});
		rulesVersion = GetMapSettingsVersion();
	}

	void UpdateTriggers()
	{
		if (rulesVersion != GetMapSettingsVersion())
		{
			ResetRules();
		}

		std::vector<std::string> heard;

		{
			const std::lock_guard<std::mutex> lock(soundMutex);
			heard.swap(pendingSounds);
		}

		stats.soundsHeard += heard.size();

		for (const std::string& sound : heard)
		{
			recentSounds.push_back(sound);

			if (recentSounds.size() > maxRecentSounds)
			{
				recentSounds.pop_front();
			}
		}

		stats.visionName = ReadVisionName();

		float camera[3] = {};
		const bool hasCamera = TryGetCameraOrigin(camera);
		bool isOutdoors = false;
		stats.hasOutdoorMap = hasCamera && TryTestPointOutdoors(camera, isOutdoors);
		stats.isCameraOutdoors = isOutdoors;

		const std::vector<TriggerRule>& rules = GetMapSettings().triggers;
		const ULONGLONG now = GetTickCount64();

		for (std::size_t i = 0; i < rules.size() && i < ruleStates.size(); ++i)
		{
			const TriggerRule& rule = rules[i];
			RuleState& state = ruleStates[i];
			bool isActive = false;

			switch (rule.event)
			{
			case TriggerEvent::Vision:
				isActive = StartsWithIgnoringCase(stats.visionName, rule.match);
				break;

			case TriggerEvent::Sound:
				for (const std::string& sound : heard)
				{
					if (StartsWithIgnoringCase(sound, rule.match))
					{
						state.lastHeardMs = now;
						state.hasHeard = true;
					}
				}

				isActive = state.hasHeard && now - state.lastHeardMs <= static_cast<ULONGLONG>(rule.holdSeconds * 1000.0f);
				break;

			case TriggerEvent::Indoors:
				isActive = stats.hasOutdoorMap && !isOutdoors;
				break;

			case TriggerEvent::Outdoors:
				isActive = stats.hasOutdoorMap && isOutdoors;
				break;

			case TriggerEvent::Area:
				isActive = hasCamera && IsInsideArea(rule, camera);
				break;
			}

			if (isActive == state.isActive)
			{
				continue;
			}

			state.isActive = isActive;
			const int layer = firstTriggerLayer + static_cast<int>(i);

			if (isActive)
			{
				++state.activations;
				Log() << "remix: trigger " << rule.name << " started";

				if (!rule.remixOptions.empty())
				{
					SetLayerOptions(layer, rule.remixOptions, rule.blendSeconds);
				}

				continue;
			}

			Log() << "remix: trigger " << rule.name << " ended";
			ClearLayer(layer, rule.blendSeconds);
		}
	}

	bool IsTriggerActive(const std::string& name)
	{
		const std::vector<TriggerRule>& rules = GetMapSettings().triggers;

		for (std::size_t i = 0; i < rules.size() && i < ruleStates.size(); ++i)
		{
			if (ruleStates[i].isActive && _stricmp(rules[i].name.c_str(), name.c_str()) == 0)
			{
				return true;
			}
		}

		return false;
	}

	std::vector<TriggerState> GetTriggerStates()
	{
		std::vector<TriggerState> result;
		const std::vector<TriggerRule>& rules = GetMapSettings().triggers;

		for (std::size_t i = 0; i < rules.size() && i < ruleStates.size(); ++i)
		{
			TriggerState state;
			state.name = rules[i].name;
			state.isActive = ruleStates[i].isActive;
			state.isGlobal = rules[i].isGlobal;
			state.activations = ruleStates[i].activations;
			result.push_back(state);
		}

		return result;
	}

	std::vector<std::string> GetRecentSounds()
	{
		return std::vector<std::string>(recentSounds.begin(), recentSounds.end());
	}

	TriggerStats GetTriggerStats()
	{
		return stats;
	}
}
