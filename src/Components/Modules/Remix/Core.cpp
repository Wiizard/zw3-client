#include "STDInclude.hpp"

#include "Core.hpp"

#include <variant>

#include "Components/Modules/Logger.hpp"

namespace Components::Remix
{
	LogLine::~LogLine()
	{
		const std::string text = this->stream.str();

		switch (this->level)
		{
		case Level::Warning:
			Logger::Warning("{}\n", text);
			break;
		case Level::Error:
			Logger::Error("{}\n", text);
			break;
		default:
			Logger::Print("{}\n", text);
			break;
		}
	}

	LogLine& LogLine::operator<<(const std::wstring& value)
	{
		this->stream << Core::NarrowUtf8(value.c_str());
		return *this;
	}

	LogLine& LogLine::operator<<(const wchar_t* value)
	{
		this->stream << Core::NarrowUtf8(value);
		return *this;
	}
}

namespace Components::Remix::Core
{
	using Member = std::variant<bool Config::*, float Config::*, int Config::*>;

	struct Setting
	{
		const char* key;
		Member member;
	};

	static const Setting settingsTable[] =
	{
		{ "remix_fog", &Config::remixMapFog },
		{ "remix_fog_strength", &Config::remixFogStrength },
		{ "remix_glow", &Config::remixGlowBrightness },
		{ "remix_lights", &Config::remixLightBrightness },
		{ "remix_tracker", &Config::remixTrackerBrightness },
		{ "remix_sun", &Config::remixSunBrightness },
		{ "remix_sun_cap_on", &Config::remixSunCapped },
		{ "remix_sun_cap", &Config::remixSunCap },
		{ "remix_sun_tint_r", &Config::remixSunTintR },
		{ "remix_sun_tint_g", &Config::remixSunTintG },
		{ "remix_sun_tint_b", &Config::remixSunTintB },
		{ "remix_sun_override", &Config::remixSunOverridden },
		{ "remix_sun_yaw", &Config::remixSunYawDeg },
		{ "remix_sun_height", &Config::remixSunHeightDeg },
		{ "remix_weather", &Config::remixWeather },
		{ "remix_offscreen_world", &Config::remixOffscreenWorld },
		{ "remix_threaded_renderer", &Config::remixThreadedRenderer },
		{ "remix_shared_decals", &Config::remixSharedDecals },
		{ "remix_rain_outdoors", &Config::remixRainOutdoorsOnly },
		{ "remix_effects_offscreen", &Config::remixEffectsOffscreen },
		{ "remix_all_effect_lights", &Config::remixAllEffectLights },
		{ "remix_entities_offscreen", &Config::remixEntitiesOffscreen },
		{ "remix_steady_lod", &Config::remixSteadyModelDetail },
		{ "remix_body_shadow", &Config::remixPlayerBodyShadow },
		{ "remix_view_culling", &Config::remixViewCulling },
		{ "remix_grade", &Config::remixMapGrade },
		{ "remix_grade_strength", &Config::remixMapGradeStrength },
		{ "remix_draw_effects", &Config::remixDrawEffects },
		{ "remix_draw_decals", &Config::remixDrawDecals },
		{ "remix_draw_glass", &Config::remixDrawGlass },
		{ "remix_draw_clouds", &Config::remixDrawClouds },
		{ "remix_draw_sparks", &Config::remixDrawSparks },
		{ "remix_draw_tracers", &Config::remixDrawTracers },
		{ "remix_sky", &Config::remixSkyScale },
		{ "remix_tonemapper", &Config::remixTonemapper },
		{ "remix_auto_exposure", &Config::remixAutoExposure },
		{ "remix_ev_min", &Config::remixExposureMinEv },
		{ "remix_ev_max", &Config::remixExposureMaxEv },
		{ "remix_exposure_speed", &Config::remixExposureSpeed },
		{ "remix_local_exposure", &Config::remixLocalExposure },
		{ "remix_local_shadows", &Config::remixLocalShadows },
		{ "remix_local_highlights", &Config::remixLocalHighlights },
		{ "remix_local_contrast", &Config::remixLocalContrastBoosted },
		{ "remix_local_aces", &Config::remixLocalAces },
		{ "remix_exposure_bias", &Config::remixExposureBiasEv },
		{ "remix_grading", &Config::remixColourGrading },
		{ "remix_contrast", &Config::remixContrast },
		{ "remix_saturation", &Config::remixSaturation },
		{ "remix_tint_r", &Config::remixColourBalanceR },
		{ "remix_tint_g", &Config::remixColourBalanceG },
		{ "remix_tint_b", &Config::remixColourBalanceB },
		{ "remix_global_aces", &Config::remixGlobalAces },
		{ "remix_steady_tone_curve", &Config::remixSteadyToneCurve },
		{ "remix_bloom", &Config::remixBloom },
		{ "remix_bloom_strength", &Config::remixBloomIntensity },
		{ "remix_bloom_threshold", &Config::remixBloomThreshold },
		{ "remix_vignette", &Config::remixVignette },
		{ "remix_vignette_darkness", &Config::remixVignetteIntensity },
		{ "remix_vignette_size", &Config::remixVignetteRadius },
		{ "remix_vignette_softness", &Config::remixVignetteSoftness },
		{ "remix_aberration", &Config::remixChromaticAberration },
		{ "remix_aberration_amount", &Config::remixChromaticAmount },
		{ "remix_motion_blur", &Config::remixMotionBlur },
		{ "remix_haze", &Config::remixHaze },
		{ "remix_clay", &Config::remixWhiteMaterial },
		{ "remix_indirect", &Config::remixIndirectMode },
		{ "remix_bounces", &Config::remixMaxBounces },
		{ "remix_indirect_light", &Config::remixBounceLight },
		{ "remix_glass_shadows", &Config::remixGlassShadows },
		{ "remix_skin_scattering", &Config::remixSkinScattering },
		{ "remix_albedo", &Config::remixAlbedoScale },
		{ "remix_roughness", &Config::remixRoughnessScale },
		{ "remix_roughness_offset", &Config::remixRoughnessBias },
		{ "remix_metallic", &Config::remixMetallicScale },
		{ "remix_normals", &Config::remixNormalIntensity },
		{ "remix_neon_foliage", &Config::remixNeonFoliage },
		{ "remix_neon_intensity", &Config::remixNeonIntensity },
		{ "remix_neon_pulse", &Config::remixNeonPulse },
		{ "remix_neon_pulse_speed", &Config::remixNeonPulseSpeed },
		{ "remix_neon_pulse_depth", &Config::remixNeonPulseDepth },
		{ "remix_alien", &Config::remixAlien },
		{ "remix_neon_cache_off", &Config::remixNeonCacheOff },
		{ "remix_alien_style", &Config::remixAlienStyle },
		{ "remix_alien_speed", &Config::remixAlienSpeed },
		{ "remix_alien_depth", &Config::remixAlienDepth },
		{ "remix_alien_scale", &Config::remixAlienScale },
		{ "remix_alien_colours", &Config::remixAlienColours },
		{ "remix_alien_tint_r", &Config::remixAlienTintR },
		{ "remix_alien_tint_g", &Config::remixAlienTintG },
		{ "remix_alien_tint_b", &Config::remixAlienTintB },
		{ "remix_photo_mode", &Config::remixPhotoMode },
		{ "remix_photo_accumulate", &Config::remixPhotoAccumulate },
		{ "remix_photo_locked", &Config::remixPhotoLocked },
		{ "remix_laser_lights", &Config::remixLaserLights },
		{ "remix_fixture_lights", &Config::remixFixtureLights },
		{ "remix_fixture_brightness", &Config::remixFixtureBrightness },
		{ "remix_fixture_offset", &Config::remixFixtureOffset },
		{ "remix_fixture_size", &Config::remixFixtureSize },
		{ "remix_puddles", &Config::remixPuddles },
		{ "remix_parallax", &Config::remixParallax },
		{ "remix_parallax_depth", &Config::remixParallaxDepth },
		{ "remix_wet", &Config::remixWetMode },
		{ "remix_rain_everywhere", &Config::remixRainEverywhere },
		{ "remix_rain_amount", &Config::remixRainAmount },
		{ "remix_screen_drops", &Config::remixScreenDrops },
		{ "remix_time_of_day", &Config::remixTimeOfDay },
		{ "remix_time_of_day_hours", &Config::remixTimeOfDayHours },
		{ "remix_night", &Config::remixNight },
		{ "remix_night_sky", &Config::remixNightSky },
		{ "remix_god_rays", &Config::remixGodRays },
		{ "remix_god_ray_strength", &Config::remixGodRayStrength },
		{ "remix_mirror", &Config::remixMirror },
		{ "remix_mirror_metal", &Config::remixMirrorMetal },
		{ "remix_effect_glow", &Config::remixEffectGlow },
		{ "remix_shadow_lift", &Config::remixShadowLift },
		{ "remix_eye_glow_brightness", &Config::remixEyeGlowBrightness },
		{ "remix_eye_glow_size", &Config::remixEyeGlowSize },
		{ "remix_map_zones", &Config::remixMapZones },
		{ "remix_steady_transparency", &Config::remixSteadyTransparency },
		{ "remix_outdoor_ev", &Config::remixOutdoorEv },
		{ "remix_opacity_micromaps", &Config::remixOpacityMicromaps },
		{ "remix_sky_reprojected", &Config::remixSkyReprojected },
		{ "remix_spot_brightness", &Config::remixSpotBrightness },
		{ "remix_omni_brightness", &Config::remixOmniBrightness },
		{ "remix_light_size", &Config::remixLightSize },
		{ "remix_cone_offset", &Config::remixConeOffset },
		{ "remix_cone_softness", &Config::remixConeSoftness },
		{ "remix_light_focus", &Config::remixLightFocus },
		{ "remix_light_volumetric", &Config::remixLightVolumetric },
		{ "remix_dim_light_cutoff", &Config::remixDimLightCutoff },
		{ "remix_culling_radius", &Config::remixCullingRadius },
		{ "remix_prop_distance_scale", &Config::remixPropDistanceScale },
		{ "remix_di_samples", &Config::remixDiSamples },
	};

	static Config config;
	static Config savedConfig;
	static Config globalConfig;
	static std::string activeMap;
	static bool hasMapConfig = false;
	static HMODULE runtime = nullptr;

	static std::string Trim(const std::string& value)
	{
		const std::size_t first = value.find_first_not_of(" \t\r\n");

		if (first == std::string::npos)
		{
			return {};
		}

		const std::size_t last = value.find_last_not_of(" \t\r\n");

		return value.substr(first, last - first + 1);
	}

	static bool ParseBool(const std::string& value, bool fallback)
	{
		const std::string lowered = Utils::String::ToLower(value);

		if (lowered == "1" || lowered == "true" || lowered == "yes" || lowered == "on")
		{
			return true;
		}

		if (lowered == "0" || lowered == "false" || lowered == "no" || lowered == "off")
		{
			return false;
		}

		return fallback;
	}

	static long ParseLong(const std::string& value, long fallback)
	{
		try
		{
			return std::stol(value);
		}
		catch (...)
		{
			return fallback;
		}
	}

	static float ParseFloat(const std::string& value, float fallback)
	{
		try
		{
			const float parsed = std::stof(value);

			if (!std::isfinite(parsed))
			{
				return fallback;
			}

			return parsed;
		}
		catch (...)
		{
			return fallback;
		}
	}

	static bool TryApplyTableKey(Config& target, const std::string& key, const std::string& value)
	{
		for (const Setting& setting : settingsTable)
		{
			if (key != setting.key)
			{
				continue;
			}

			std::visit([&](auto field)
			{
				using T = std::remove_cvref_t<decltype(target.*field)>;

				if constexpr (std::is_same_v<T, bool>)
				{
					target.*field = ParseBool(value, target.*field);
				}
				else if constexpr (std::is_same_v<T, float>)
				{
					target.*field = ParseFloat(value, target.*field);
				}
				else
				{
					target.*field = static_cast<T>(ParseLong(value, static_cast<long>(target.*field)));
				}
			}, setting.member);

			return true;
		}

		return false;
	}

	static std::string FormatSetting(const Config& source, const Member& member)
	{
		return std::visit([&source](auto field) -> std::string
		{
			using T = std::remove_cvref_t<decltype(source.*field)>;

			if constexpr (std::is_same_v<T, bool>)
			{
				if (source.*field)
				{
					return "1";
				}

				return "0";
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				char text[32];
				std::snprintf(text, sizeof(text), "%g", static_cast<double>(source.*field));
				return text;
			}
			else
			{
				return std::to_string(source.*field);
			}
		}, member);
	}

	static std::wstring IniPath()
	{
		return ModuleDirectory() + L"zw3_remix\\remix.ini";
	}

	static std::wstring MapIniPath(const std::string& mapName)
	{
		return ModuleDirectory() + L"zw3_remix\\maps\\" + std::wstring(mapName.begin(), mapName.end()) + L".ini";
	}

	static bool TryParseIni(const std::wstring& path, Config& target)
	{
		std::ifstream file(path);

		if (!file)
		{
			return false;
		}

		std::string line;

		while (std::getline(file, line))
		{
			const std::string trimmed = Trim(line);

			if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#' || trimmed[0] == '[')
			{
				continue;
			}

			const std::size_t equals = trimmed.find('=');

			if (equals == std::string::npos)
			{
				LogWarn() << "remix: skipping malformed line '" << trimmed << "' in " << path;
				continue;
			}

			const std::string key = Utils::String::ToLower(Trim(trimmed.substr(0, equals)));
			const std::string value = Trim(trimmed.substr(equals + 1));

			if (key == "remix_dll")
			{
				target.remixDll = value;
				continue;
			}

			if (!TryApplyTableKey(target, key, value))
			{
				LogWarn() << "remix: ignoring unknown key '" << key << "' in " << path;
			}
		}

		return true;
	}

	const Config& GetConfig()
	{
		return config;
	}

	Config& MutableConfig()
	{
		return config;
	}

	bool HasUnsavedChanges()
	{
		for (const Setting& setting : settingsTable)
		{
			if (FormatSetting(config, setting.member) != FormatSetting(savedConfig, setting.member))
			{
				return true;
			}
		}

		return false;
	}

	void ResetAllSettings(Config& target)
	{
		const std::string remixDll = target.remixDll;
		target = Config{};
		target.remixDll = remixDll;
	}

	void ReloadConfig()
	{
		LoadConfigForMap(activeMap);
	}

	void SaveConfig(std::string& message)
	{
		if (hasMapConfig)
		{
			SaveMapConfig(message);
			return;
		}

		std::vector<std::string> lines;

		{
			std::ifstream existing(IniPath());
			std::string line;

			while (std::getline(existing, line))
			{
				lines.push_back(line);
			}
		}

		std::set<std::string> written;

		for (std::string& line : lines)
		{
			const std::string trimmed = Trim(line);
			const std::size_t equals = trimmed.find('=');

			if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#' || equals == std::string::npos)
			{
				continue;
			}

			const std::string key = Utils::String::ToLower(Trim(trimmed.substr(0, equals)));

			for (const Setting& setting : settingsTable)
			{
				if (key == setting.key)
				{
					line = key + " = " + FormatSetting(config, setting.member);
					written.insert(key);
				}
			}
		}

		for (const Setting& setting : settingsTable)
		{
			if (!written.contains(setting.key))
			{
				lines.push_back(std::string(setting.key) + " = " + FormatSetting(config, setting.member));
			}
		}

		CreateDirectoryW((ModuleDirectory() + L"zw3_remix").c_str(), nullptr);

		std::ofstream file(IniPath(), std::ios::trunc);

		if (!file)
		{
			message = "Could not write zw3_remix\\remix.ini";
			LogError() << "remix: could not write zw3_remix\\remix.ini";
			return;
		}

		for (const std::string& line : lines)
		{
			file << line << "\n";
		}

		savedConfig = config;
		globalConfig = config;
		message = "Saved to zw3_remix\\remix.ini";
		Log() << "remix: settings saved to zw3_remix\\remix.ini";
	}

	void LoadConfig()
	{
		LoadConfigForMap(activeMap);
	}

	void LoadConfigForMap(const std::string& mapName)
	{
		const std::string remixDll = config.remixDll;
		Config merged{};
		merged.remixDll = remixDll;

		if (!TryParseIni(IniPath(), merged))
		{
			Log() << "remix: no zw3_remix\\remix.ini, using the default settings";
		}

		globalConfig = merged;
		hasMapConfig = !mapName.empty() && TryParseIni(MapIniPath(mapName), merged);

		if (hasMapConfig)
		{
			Log() << "remix: settings for " << mapName << " read from zw3_remix\\maps\\" << mapName << ".ini over remix.ini";
		}

		activeMap = mapName;
		config = merged;
		savedConfig = merged;
	}

	const std::string& GetActiveMap()
	{
		return activeMap;
	}

	bool HasMapConfig()
	{
		return hasMapConfig;
	}

	void SaveMapConfig(std::string& message)
	{
		if (activeMap.empty())
		{
			message = "No map is loaded";
			return;
		}

		std::vector<std::string> lines;

		for (const Setting& setting : settingsTable)
		{
			const std::string value = FormatSetting(config, setting.member);

			if (value != FormatSetting(globalConfig, setting.member))
			{
				lines.push_back(std::string(setting.key) + " = " + value);
			}
		}

		CreateDirectoryW((ModuleDirectory() + L"zw3_remix\\maps").c_str(), nullptr);
		const std::wstring path = MapIniPath(activeMap);

		if (lines.empty())
		{
			DeleteFileW(path.c_str());
			hasMapConfig = false;
			savedConfig = config;
			message = activeMap + " matches the global settings, no map file needed";
			return;
		}

		std::ofstream file(path, std::ios::trunc);

		if (!file)
		{
			message = "Could not write zw3_remix\\maps\\" + activeMap + ".ini";
			return;
		}

		file << "; settings for " << activeMap << " that differ from zw3_remix\\remix.ini\n";

		for (const std::string& line : lines)
		{
			file << line << "\n";
		}

		hasMapConfig = true;
		savedConfig = config;
		message = "Saved " + std::to_string(lines.size()) + " map settings to zw3_remix\\maps\\" + activeMap + ".ini";
		Log() << "remix: " << message;
	}

	void ForgetMapConfig(std::string& message)
	{
		if (activeMap.empty())
		{
			return;
		}

		DeleteFileW(MapIniPath(activeMap).c_str());
		LoadConfigForMap(activeMap);
		message = activeMap + " now uses the global settings";
	}

	bool IsRemixMode()
	{
		return runtime != nullptr;
	}

	bool IsRemixLoaded()
	{
		return runtime != nullptr;
	}

	HMODULE ResolveForwardTarget()
	{
		return runtime;
	}

	void SetRuntime(HMODULE module)
	{
		runtime = module;
	}

	std::wstring ModuleDirectory()
	{
		static const std::wstring directory = []
		{
			wchar_t path[MAX_PATH]{};
			const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);

			if (length == 0 || length == MAX_PATH)
			{
				return std::wstring(L".\\");
			}

			std::wstring result(path, length);
			const std::size_t slash = result.find_last_of(L"\\/");

			if (slash == std::wstring::npos)
			{
				return std::wstring(L".\\");
			}

			return result.substr(0, slash + 1);
		}();

		return directory;
	}

	std::string NarrowUtf8(const wchar_t* value)
	{
		if (!value || !*value)
		{
			return {};
		}

		const int size = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);

		if (size <= 1)
		{
			return {};
		}

		std::string result(static_cast<std::size_t>(size - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), size, nullptr, nullptr);

		return result;
	}
}
