#pragma once

namespace Components::Remix
{
	class LogLine
	{
	public:
		enum class Level
		{
			Info,
			Warning,
			Error,
		};

		explicit LogLine(Level level) : level(level) {}
		LogLine(const LogLine&) = delete;
		LogLine& operator=(const LogLine&) = delete;
		~LogLine();

		template <typename T>
		LogLine& operator<<(const T& value)
		{
			this->stream << value;
			return *this;
		}

		LogLine& operator<<(std::ios_base& (*manipulator)(std::ios_base&))
		{
			this->stream << manipulator;
			return *this;
		}

		LogLine& operator<<(const std::wstring& value);

		LogLine& operator<<(const wchar_t* value);

	private:
		Level level;
		std::ostringstream stream;
	};

	inline LogLine Log()
	{
		return LogLine(LogLine::Level::Info);
	}

	inline LogLine LogWarn()
	{
		return LogLine(LogLine::Level::Warning);
	}

	inline LogLine LogError()
	{
		return LogLine(LogLine::Level::Error);
	}
}

namespace Components::Remix::Core
{
	struct Config
	{
		std::string remixDll = ".trex\\d3d9.dll";

		bool remixMapFog = true;
		float remixFogStrength = 1.0f;
		float remixGlowBrightness = 1.0f;
		float remixLightBrightness = 1.0f;
		float remixTrackerBrightness = 1.0f;
		float remixSunBrightness = 1.0f;
		bool remixSunCapped = false;
		float remixSunCap = 3.0f;
		float remixSunTintR = 1.0f;
		float remixSunTintG = 1.0f;
		float remixSunTintB = 1.0f;
		bool remixSunOverridden = false;
		float remixSunYawDeg = 0.0f;
		float remixSunHeightDeg = 45.0f;
		bool remixWeather = true;
		bool remixOffscreenWorld = false;
		bool remixSharedDecals = true;
		bool remixRainOutdoorsOnly = true;
		bool remixEffectsOffscreen = false;
		bool remixAllEffectLights = false;
		bool remixEntitiesOffscreen = false;
		bool remixSteadyModelDetail = true;
		bool remixPlayerBodyShadow = false;
		int remixViewCulling = 0;
		bool remixMapGrade = true;
		float remixMapGradeStrength = 0.5f;
		bool remixDrawEffects = true;
		bool remixDrawDecals = true;
		bool remixDrawGlass = true;
		bool remixDrawClouds = true;
		bool remixDrawSparks = true;
		bool remixDrawTracers = true;
		float remixSkyScale = 1.0f;
		int remixTonemapper = 0;
		bool remixAutoExposure = true;
		float remixExposureMinEv = -2.0f;
		float remixExposureMaxEv = 5.0f;
		float remixExposureSpeed = 3.0f;
		float remixLocalExposure = 0.75f;
		float remixLocalShadows = 2.0f;
		float remixLocalHighlights = 4.0f;
		bool remixLocalContrastBoosted = false;
		bool remixLocalAces = true;
		float remixExposureBiasEv = 0.0f;
		bool remixColourGrading = false;
		float remixContrast = 1.0f;
		float remixSaturation = 1.0f;
		float remixColourBalanceR = 1.0f;
		float remixColourBalanceG = 1.0f;
		float remixColourBalanceB = 1.0f;
		bool remixGlobalAces = false;
		bool remixBloom = true;
		float remixBloomIntensity = 1.0f;
		float remixBloomThreshold = 0.25f;
		bool remixVignette = false;
		float remixVignetteIntensity = 0.8f;
		float remixVignetteRadius = 0.8f;
		float remixVignetteSoftness = 0.2f;
		bool remixChromaticAberration = false;
		float remixChromaticAmount = 0.02f;
		bool remixMotionBlur = false;
		bool remixHaze = false;
		bool remixWhiteMaterial = false;
		int remixIndirectMode = 0;
		int remixMaxBounces = 2;
		bool remixBounceLight = true;
		bool remixGlassShadows = false;
		bool remixSkinScattering = false;
		float remixAlbedoScale = 1.0f;
		float remixRoughnessScale = 1.0f;
		float remixRoughnessBias = 0.0f;
		float remixMetallicScale = 1.0f;
		float remixNormalIntensity = 2.0f;
		bool remixNeonFoliage = false;
		float remixNeonIntensity = 3.0f;
		bool remixNeonPulse = false;
		float remixNeonPulseSpeed = 0.5f;
		float remixNeonPulseDepth = 0.7f;
		bool remixAlien = false;
		bool remixNeonCacheOff = true;
		int remixAlienStyle = 1;
		float remixAlienSpeed = 0.3f;
		float remixAlienDepth = 0.8f;
		float remixAlienScale = 1.0f;
		int remixAlienColours = 0;
		float remixAlienTintR = 0.2f;
		float remixAlienTintG = 1.0f;
		float remixAlienTintB = 0.6f;
		bool remixPhotoMode = false;
		bool remixPhotoAccumulate = true;
		bool remixPhotoLocked = false;
		bool remixLaserLights = false;
		bool remixFixtureLights = true;
		float remixFixtureBrightness = 1.0f;
		bool remixPuddles = false;
		bool remixParallax = false;
		float remixParallaxDepth = 1.0f;
		int remixWetMode = 0;
		bool remixRainEverywhere = false;
		float remixRainAmount = 1.0f;
		bool remixTimeOfDay = false;
		float remixTimeOfDayHours = 17.5f;
		bool remixNight = false;
		float remixNightSky = 0.03f;
		bool remixGodRays = false;
		float remixGodRayStrength = 2.0f;
		bool remixMirror = false;
		float remixMirrorMetal = 0.0f;
		float remixEffectGlow = 0.2f;
		bool remixSkyReprojected = false;
		float remixSpotBrightness = 1.0f;
		float remixOmniBrightness = 1.0f;
		float remixLightSize = 4.0f;
		float remixConeOffset = 0.0f;
		float remixConeSoftness = 0.0f;
		float remixLightFocus = 0.0f;
		float remixLightVolumetric = 1.0f;
		float remixDimLightCutoff = 0.0f;
		float remixCullingRadius = 1000.0f;
		float remixPropDistanceScale = 1.0f;
		int remixDiSamples = 0;
	};

	const Config& GetConfig();

	Config& MutableConfig();

	void LoadConfig();

	void ReloadConfig();

	void SaveConfig(std::string& message);

	void LoadConfigForMap(const std::string& mapName);

	const std::string& GetActiveMap();

	bool HasMapConfig();

	void SaveMapConfig(std::string& message);

	void ForgetMapConfig(std::string& message);

	bool HasUnsavedChanges();

	void ResetAllSettings(Config& config);

	bool IsRemixMode();

	bool IsRemixLoaded();

	HMODULE ResolveForwardTarget();

	void SetRuntime(HMODULE runtime);

	std::wstring ModuleDirectory();

	std::string NarrowUtf8(const wchar_t* value);
}
