#include "STDInclude.hpp"

#include "MapSettings.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <toml.hpp>

#include "Api.hpp"
#include "Core.hpp"
#include "Engine.hpp"
#include "Culling.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	static constexpr unsigned fileCheckInterval = 60;

	static MapSettings settings;
	static std::uintptr_t loadedIdentity = 0;
	static FILETIME loadedWriteTime{};
	static bool hasLoadedWriteTime = false;
	static FILETIME globalWriteTime{};
	static bool hasGlobalWriteTime = false;
	static bool isReloadRequested = false;
	static unsigned frameCounter = 0;
	static unsigned settingsVersion = 1;

	static bool TryGetWriteTime(const std::wstring& path, FILETIME& writeTime)
	{
		WIN32_FILE_ATTRIBUTE_DATA data{};

		if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data))
		{
			return false;
		}

		writeTime = data.ftLastWriteTime;
		return true;
	}

	static const toml::value* FindKey(const toml::value& table, const char* key)
	{
		if (!table.is_table() || !table.contains(key))
		{
			return nullptr;
		}

		return &table.at(key);
	}

	static bool TryReadNumber(const toml::value& table, const char* key, float& result)
	{
		const toml::value* found = FindKey(table, key);

		if (!found)
		{
			return false;
		}

		if (found->is_floating())
		{
			result = static_cast<float>(found->as_floating());
			return true;
		}

		if (found->is_integer())
		{
			result = static_cast<float>(found->as_integer());
			return true;
		}

		LogWarn() << "remix: map settings key " << key << " should be a number";
		return false;
	}

	static bool TryReadBool(const toml::value& table, const char* key, bool& result)
	{
		const toml::value* found = FindKey(table, key);

		if (!found)
		{
			return false;
		}

		if (!found->is_boolean())
		{
			LogWarn() << "remix: map settings key " << key << " should be true or false";
			return false;
		}

		result = found->as_boolean();
		return true;
	}

	static bool TryReadVector(const toml::value& table, const char* key, float* result, int count)
	{
		const toml::value* found = FindKey(table, key);

		if (!found)
		{
			return false;
		}

		if (!found->is_array() || found->as_array().size() != static_cast<std::size_t>(count))
		{
			LogWarn() << "remix: map settings key " << key << " should be a list of " << count << " numbers";
			return false;
		}

		const toml::array& items = found->as_array();

		for (int i = 0; i < count; ++i)
		{
			if (items[i].is_floating())
			{
				result[i] = static_cast<float>(items[i].as_floating());
			}
			else if (items[i].is_integer())
			{
				result[i] = static_cast<float>(items[i].as_integer());
			}
			else
			{
				LogWarn() << "remix: map settings key " << key << " should be a list of " << count << " numbers";
				return false;
			}
		}

		return true;
	}

	static bool TryFormatValue(const toml::value& value, std::string& text)
	{
		char number[48];

		if (value.is_boolean())
		{
			if (value.as_boolean())
			{
				text = "True";
			}
			else
			{
				text = "False";
			}

			return true;
		}

		if (value.is_integer())
		{
			std::snprintf(number, sizeof(number), "%lld", static_cast<long long>(value.as_integer()));
			text = number;
			return true;
		}

		if (value.is_floating())
		{
			std::snprintf(number, sizeof(number), "%.4f", value.as_floating());
			text = number;
			return true;
		}

		if (value.is_string())
		{
			text = value.as_string();
			return true;
		}

		if (value.is_array())
		{
			text.clear();

			for (const toml::value& item : value.as_array())
			{
				std::string part;

				if (!TryFormatValue(item, part))
				{
					return false;
				}

				if (!text.empty())
				{
					text += ", ";
				}

				text += part;
			}

			return true;
		}

		return false;
	}

	static void ReadSun(const toml::value& root)
	{
		const toml::value* sun = FindKey(root, "sun");

		if (!sun)
		{
			return;
		}

		float number = 0.0f;

		if (TryReadNumber(*sun, "brightness", number))
		{
			settings.sun.brightness = number;
		}

		if (TryReadNumber(*sun, "max_radiance", number))
		{
			settings.sun.maxRadiance = number;
		}

		settings.sun.hasTint = TryReadVector(*sun, "tint", settings.sun.tint, 3);

		const bool hasYaw = TryReadNumber(*sun, "yaw", settings.sun.yawDeg);
		const bool hasPitch = TryReadNumber(*sun, "pitch", settings.sun.pitchDeg);
		settings.sun.hasDirection = hasYaw && hasPitch;
		settings.sun.hasColour = TryReadVector(*sun, "colour", settings.sun.colour, 3);
	}

	static void ReadFogAndCulling(const toml::value& root)
	{
		if (const toml::value* fog = FindKey(root, "fog"))
		{
			bool isEnabled = true;
			float strength = 1.0f;

			if (TryReadBool(*fog, "enabled", isEnabled))
			{
				settings.isFogEnabled = isEnabled;
			}

			if (TryReadNumber(*fog, "strength", strength))
			{
				settings.fogStrength = strength;
			}
		}

		if (const toml::value* culling = FindKey(root, "culling"))
		{
			float level = 0.0f;

			if (TryReadNumber(*culling, "level", level))
			{
				const int wholeLevel = static_cast<int>(level);

				if (wholeLevel >= 0 && wholeLevel < cullingLevelCount)
				{
					settings.cullingLevel = wholeLevel;
				}
				else
				{
					LogWarn() << "remix: map settings culling level " << wholeLevel << " is not 0 to "
							  << cullingLevelCount - 1;
				}
			}

			float number = 0.0f;

			if (TryReadNumber(*culling, "radius", number))
			{
				settings.cullingRadius = number;
			}

			if (TryReadNumber(*culling, "prop_distance_scale", number))
			{
				settings.propDistanceScale = number;
			}

			if (const toml::value* kept = FindKey(*culling, "keep_models"))
			{
				if (kept->is_array())
				{
					for (const toml::value& entry : kept->as_array())
					{
						if (entry.is_string())
						{
							settings.keptModels.push_back(entry.as_string());
						}
					}
				}
			}

			if (const toml::value* areas = FindKey(*culling, "areas"))
			{
				if (areas->is_array())
				{
					for (const toml::value& entry : areas->as_array())
					{
						CullingArea area;
						const bool hasBox = TryReadVector(entry, "min", area.boxMin, 3)
							&& TryReadVector(entry, "max", area.boxMax, 3);

						if (!hasBox || !TryReadNumber(entry, "radius", area.radius))
						{
							LogWarn() << "remix: a culling area needs min, max and radius, it is skipped";
							continue;
						}

						settings.cullingAreas.push_back(area);
					}
				}
			}
		}
	}

	static void FlattenOptions(const toml::value& table, const std::string& prefix, OptionList& result)
	{
		for (const auto& [key, value] : table.as_table())
		{
			const std::string name = prefix + key;

			if (value.is_table())
			{
				FlattenOptions(value, name + ".", result);
				continue;
			}

			std::string text;

			if (!TryFormatValue(value, text))
			{
				LogWarn() << "remix: map settings remix option " << name << " has a value we cannot send";
				continue;
			}

			result.emplace_back(name, text);
		}
	}

	static void ReadOptionTable(const toml::value* remixTable, OptionList& result, float& blendSeconds)
	{
		if (!remixTable || !remixTable->is_table())
		{
			return;
		}

		TryReadNumber(*remixTable, "blend_seconds", blendSeconds);

		for (const auto& [key, value] : remixTable->as_table())
		{
			if (key == "blend_seconds" || key == "conf")
			{
				continue;
			}

			if (value.is_table())
			{
				FlattenOptions(value, key + ".", result);
				continue;
			}

			std::string text;

			if (!TryFormatValue(value, text))
			{
				LogWarn() << "remix: map settings remix option " << key << " has a value we cannot send";
				continue;
			}

			result.emplace_back(key, text);
		}

		const toml::value* confFiles = FindKey(*remixTable, "conf");

		if (!confFiles || !confFiles->is_array())
		{
			return;
		}

		for (const toml::value& entry : confFiles->as_array())
		{
			if (!entry.is_string())
			{
				continue;
			}

			const std::string& name = entry.as_string();
			OptionList fileOptions;
			const std::wstring path = GetRemixDataDirectory() + L"presets\\" + std::wstring(name.begin(), name.end());

			if (!TryReadConfFile(path, fileOptions))
			{
				LogWarn() << "remix: a map or trigger names " << name << ", which is not in zw3_remix\\presets";
				continue;
			}

			result.insert(result.end(), fileOptions.rbegin(), fileOptions.rend());
		}
	}

	static void ReadRemixOptions(const toml::value& root)
	{
		ReadOptionTable(FindKey(root, "remix"), settings.remixOptions, settings.remixBlendSeconds);
	}

	static bool TryReadEvent(const std::string& text, TriggerEvent& event)
	{
		static const std::pair<const char*, TriggerEvent> events[] = {
			{ "vision", TriggerEvent::Vision },
			{ "sound", TriggerEvent::Sound },
			{ "indoors", TriggerEvent::Indoors },
			{ "outdoors", TriggerEvent::Outdoors },
			{ "area", TriggerEvent::Area },
		};

		for (const auto& [name, value] : events)
		{
			if (_stricmp(text.c_str(), name) == 0)
			{
				event = value;
				return true;
			}
		}

		return false;
	}

	static void ReadTriggers(const toml::value& root, bool isGlobal)
	{
		const toml::value* triggers = FindKey(root, "triggers");

		if (!triggers || !triggers->is_array())
		{
			return;
		}

		for (const toml::value& entry : triggers->as_array())
		{
			TriggerRule rule;
			rule.isGlobal = isGlobal;

			const toml::value* name = FindKey(entry, "name");
			const toml::value* event = FindKey(entry, "on");

			if (!name || !name->is_string() || !event || !event->is_string() || !TryReadEvent(event->as_string(), rule.event))
			{
				LogWarn() << "remix: a trigger needs a name and an on of vision, sound, indoors, outdoors or area, it is"
							 " skipped";
				continue;
			}

			rule.name = name->as_string();

			if (const toml::value* match = FindKey(entry, "match"))
			{
				if (match->is_string())
				{
					rule.match = match->as_string();
				}
			}

			TryReadNumber(entry, "hold_seconds", rule.holdSeconds);
			TryReadNumber(entry, "blend_seconds", rule.blendSeconds);
			rule.hasBox = TryReadVector(entry, "min", rule.boxMin, 3) && TryReadVector(entry, "max", rule.boxMax, 3);
			rule.hasSphere = TryReadVector(entry, "centre", rule.centre, 3) && TryReadNumber(entry, "radius", rule.radius);

			float optionBlend = rule.blendSeconds;
			ReadOptionTable(FindKey(entry, "remix"), rule.remixOptions, optionBlend);

			const bool needsMatch = rule.event == TriggerEvent::Vision || rule.event == TriggerEvent::Sound;
			const bool needsShape = rule.event == TriggerEvent::Area;

			if ((needsMatch && rule.match.empty()) || (needsShape && !rule.hasBox && !rule.hasSphere))
			{
				LogWarn() << "remix: trigger " << rule.name << " is missing its match or its area, it is skipped";
				continue;
			}

			settings.triggers.push_back(rule);
		}
	}

	static void ReadMarkers(const toml::value& root)
	{
		const toml::value* markers = FindKey(root, "markers");

		if (!markers || !markers->is_array())
		{
			return;
		}

		for (const toml::value& entry : markers->as_array())
		{
			MapMarker marker;
			float id = 0.0f;

			if (!TryReadNumber(entry, "id", id) || !TryReadVector(entry, "position", marker.position, 3))
			{
				LogWarn() << "remix: a marker needs an id and a position, it is skipped";
				continue;
			}

			marker.id = static_cast<int>(id);
			TryReadVector(entry, "rotation", marker.rotationDeg, 3);

			const toml::value* scale = FindKey(entry, "scale");
			float uniformScale = 1.0f;

			if (scale && scale->is_array())
			{
				TryReadVector(entry, "scale", marker.scale, 3);
			}
			else if (scale && TryReadNumber(entry, "scale", uniformScale))
			{
				marker.scale[0] = uniformScale;
				marker.scale[1] = uniformScale;
				marker.scale[2] = uniformScale;
			}

			TryReadNumber(entry, "max_distance", marker.maxDistance);

			bool whenRaining = false;

			if (TryReadBool(entry, "when_raining", whenRaining))
			{
				marker.whenRaining = whenRaining;
			}

			if (const toml::value* trigger = FindKey(entry, "trigger"))
			{
				if (trigger->is_string())
				{
					marker.trigger = trigger->as_string();
				}
			}

			settings.markers.push_back(marker);
		}
	}

	static bool TryParseFile(const std::wstring& path, toml::value& root, std::string& error)
	{
		std::ifstream file(path, std::ios::binary);

		if (!file)
		{
			return false;
		}

		const std::size_t slash = path.find_last_of(L'\\');
		const std::wstring fileName = path.substr(slash + 1);

		auto parsed = toml::try_parse(file, Core::NarrowUtf8(fileName.c_str()));

		if (parsed.is_err())
		{
			error = toml::format_error(parsed.unwrap_err().front());
			return false;
		}

		root = parsed.unwrap();
		return true;
	}

	static void LoadGlobalTriggers()
	{
		const std::wstring path = GetRemixDataDirectory() + L"triggers.toml";
		hasGlobalWriteTime = TryGetWriteTime(path, globalWriteTime);

		toml::value root;
		std::string error;

		if (!TryParseFile(path, root, error))
		{
			if (!error.empty())
			{
				LogError() << "remix: zw3_remix\\triggers.toml did not parse, its triggers are not used: " << error;
			}

			return;
		}

		ReadTriggers(root, true);
	}

	static void ReadDvars(const toml::value& root)
	{
		const toml::value* dvars = FindKey(root, "dvars");

		if (!dvars || !dvars->is_table())
		{
			return;
		}

		for (const auto& [key, value] : dvars->as_table())
		{
			std::string text;

			if (value.is_boolean())
			{
				if (value.as_boolean())
				{
					text = "1";
				}
				else
				{
					text = "0";
				}
			}
			else if (!TryFormatValue(value, text))
			{
				continue;
			}

			settings.dvars.emplace_back(key, text);
		}
	}

	static bool TryReadHash(const toml::value& value, std::uint64_t& hash)
	{
		if (value.is_integer())
		{
			hash = static_cast<std::uint64_t>(value.as_integer());
			return true;
		}

		if (!value.is_string())
		{
			return false;
		}

		const std::string& text = value.as_string();
		char* end = nullptr;
		hash = std::strtoull(text.c_str(), &end, 16);

		return end != text.c_str() && *end == '\0';
	}

	static void ReadZones(const toml::value& root)
	{
		const toml::value* zones = FindKey(root, "zones");

		if (!zones || !zones->is_array())
		{
			return;
		}

		for (const toml::value& entry : zones->as_array())
		{
			MapZone zone;

			if (const toml::value* name = FindKey(entry, "name"); name && name->is_string())
			{
				zone.name = name->as_string();
			}

			if (const toml::value* cells = FindKey(entry, "cells"); cells && cells->is_array())
			{
				for (const toml::value& cell : cells->as_array())
				{
					if (cell.is_integer())
					{
						zone.cells.push_back(static_cast<int>(cell.as_integer()));
					}
				}
			}

			zone.hasBox = TryReadVector(entry, "min", zone.boxMin, 3) && TryReadVector(entry, "max", zone.boxMax, 3);

			if (zone.cells.empty() && !zone.hasBox)
			{
				LogWarn() << "remix: zone '" << zone.name << "' has no cells and no min/max box, it is skipped";
				continue;
			}

			float number = 0.0f;

			if (TryReadNumber(entry, "exposure_ev", number))
			{
				zone.exposureEv = number;
			}

			if (TryReadNumber(entry, "shadow_lift", number))
			{
				zone.shadowLift = number;
			}

			if (TryReadNumber(entry, "effect_glow", number))
			{
				zone.effectGlow = number;
			}

			if (TryReadNumber(entry, "lamp_brightness", number) || TryReadNumber(entry, "fixture_brightness", number))
			{
				zone.lampBrightness = number;
			}

			zone.hasLampColour = TryReadVector(entry, "lamp_colour", zone.lampColour, 3);

			if (TryReadNumber(entry, "contrast", number))
			{
				zone.contrast = number;
			}

			if (TryReadNumber(entry, "saturation", number))
			{
				zone.saturation = number;
			}

			zone.hasTint = TryReadVector(entry, "tint", zone.tint, 3);

			if (TryReadNumber(entry, "blend_seconds", number))
			{
				zone.blendSeconds = std::max(number, 0.0f);
			}

			settings.zones.push_back(zone);
		}
	}

	static void ReadMaterials(const toml::value& root)
	{
		const toml::value* materials = FindKey(root, "materials");

		if (!materials || !materials->is_table())
		{
			return;
		}

		const toml::value* cutout = FindKey(*materials, "cutout");

		if (!cutout || !cutout->is_array())
		{
			return;
		}

		for (const toml::value& entry : cutout->as_array())
		{
			if (entry.is_string())
			{
				settings.cutoutMaterials.push_back(entry.as_string());
			}
		}
	}

	static void ReadTextures(const toml::value& root)
	{
		static const std::pair<const char*, TextureTag> tagNames[] =
		{
			{ "sky", TextureTag::Sky },
			{ "world_space_ui", TextureTag::WorldSpaceUi },
			{ "beam", TextureTag::Beam },
			{ "decal", TextureTag::Decal },
			{ "particle", TextureTag::Particle },
			{ "water", TextureTag::Water },
			{ "ui", TextureTag::Ui },
			{ "ignore", TextureTag::Ignore },
		};

		const toml::value* textures = FindKey(root, "textures");

		if (!textures || !textures->is_table())
		{
			return;
		}

		for (const auto& [key, value] : textures->as_table())
		{
			const auto* const found = std::find_if(std::begin(tagNames), std::end(tagNames), [&key](const auto& entry)
			{
				return key == entry.first;
			});

			if (found == std::end(tagNames) || !value.is_array())
			{
				LogWarn() << "remix: map settings [textures] has no tag named " << key;
				continue;
			}

			for (const toml::value& entry : value.as_array())
			{
				std::uint64_t hash = 0;

				if (TryReadHash(entry, hash))
				{
					settings.textureTags.emplace_back(hash, found->second);
				}
			}
		}
	}

	static void ReadLightShape(const toml::value& table, AuthoredLight& light)
	{
		TryReadVector(table, "position", light.position, 3);
		TryReadVector(table, "colour", light.colour, 3);
		TryReadNumber(table, "intensity", light.intensity);
		TryReadNumber(table, "radius", light.radius);
		light.hasDirection = TryReadVector(table, "direction", light.direction, 3);
		TryReadNumber(table, "cone", light.coneDeg);
		TryReadNumber(table, "softness", light.softness);
		TryReadNumber(table, "focus", light.focus);
		TryReadNumber(table, "volumetric", light.volumetric);
		TryReadBool(table, "loop", light.isLooped);

		if (const toml::value* trigger = FindKey(table, "trigger"))
		{
			if (trigger->is_string())
			{
				light.trigger = trigger->as_string();
			}
		}

		if (const toml::value* model = FindKey(table, "model"))
		{
			if (model->is_string())
			{
				light.model = model->as_string();
			}
		}

		const toml::value* keyframes = FindKey(table, "keyframes");

		if (!keyframes || !keyframes->is_array())
		{
			return;
		}

		for (const toml::value& entry : keyframes->as_array())
		{
			LightKeyframe keyframe;
			TryReadNumber(entry, "time", keyframe.timeSeconds);
			TryReadNumber(entry, "intensity", keyframe.intensity);
			keyframe.hasColour = TryReadVector(entry, "colour", keyframe.colour, 3);
			light.keyframes.push_back(keyframe);
		}
	}

	static void ReadLights(const toml::value& root)
	{
		const toml::value* lights = FindKey(root, "lights");

		if (!lights)
		{
			return;
		}

		if (const toml::value* ignore = FindKey(*lights, "ignore"))
		{
			if (ignore->is_array())
			{
				for (const toml::value& entry : ignore->as_array())
				{
					std::uint64_t hash = 0;

					if (TryReadHash(entry, hash))
					{
						settings.ignoredLights.push_back(hash);
					}
				}
			}
		}

		if (const toml::value* overrides = FindKey(*lights, "override"))
		{
			if (overrides->is_array())
			{
				for (const toml::value& entry : overrides->as_array())
				{
					const toml::value* hashValue = FindKey(entry, "hash");
					LightOverride change;

					if (!hashValue || !TryReadHash(*hashValue, change.hash))
					{
						LogWarn() << "remix: a map settings light override has no hash, it is skipped";
						continue;
					}

					bool isIgnored = false;

					if (TryReadBool(entry, "ignore", isIgnored) && isIgnored)
					{
						settings.ignoredLights.push_back(change.hash);
						continue;
					}

					float number = 0.0f;

					if (TryReadNumber(entry, "brightness", number))
					{
						change.brightness = number;
					}

					if (TryReadNumber(entry, "radius", number))
					{
						change.radius = number;
					}

					if (TryReadNumber(entry, "cone", number))
					{
						change.coneDeg = number;
					}

					if (TryReadNumber(entry, "softness", number))
					{
						change.softness = number;
					}

					if (TryReadNumber(entry, "focus", number))
					{
						change.focus = number;
					}

					if (TryReadNumber(entry, "volumetric", number))
					{
						change.volumetric = number;
					}

					change.hasColour = TryReadVector(entry, "colour", change.colour, 3);
					settings.lightOverrides.push_back(change);
				}
			}
		}

		if (const toml::value* added = FindKey(*lights, "add"))
		{
			if (added->is_array())
			{
				for (const toml::value& entry : added->as_array())
				{
					AuthoredLight light;
					ReadLightShape(entry, light);
					settings.addedLights.push_back(light);
				}
			}
		}

		// type 2 is a spot, 3 an omni (GFX_LIGHT_TYPE_SPOT / _OMNI); values are the engine's, unconverted.
		if (const toml::value* primary = FindKey(*lights, "primary"))
		{
			if (primary->is_array())
			{
				for (const toml::value& entry : primary->as_array())
				{
					PrimaryLight light;
					float type = 0.0f;

					if (!TryReadNumber(entry, "type", type) || !TryReadVector(entry, "origin", light.origin, 3)
						|| !TryReadVector(entry, "colour", light.colour, 3) || !TryReadNumber(entry, "radius", light.radius))
					{
						LogWarn() << "remix: a map settings primary light lacks type, origin, colour or radius, it is skipped";
						continue;
					}

					light.type = static_cast<std::uint8_t>(type);
					TryReadVector(entry, "direction", light.direction, 3);
					TryReadNumber(entry, "cos_outer", light.cosOuter);
					TryReadNumber(entry, "cos_inner", light.cosInner);
					settings.primaryLights.push_back(light);
				}
			}
		}

		if (const toml::value* attached = FindKey(*lights, "attach"))
		{
			if (attached->is_array())
			{
				for (const toml::value& entry : attached->as_array())
				{
					AuthoredLight light;
					ReadLightShape(entry, light);

					if (light.model.empty())
					{
						LogWarn() << "remix: a map settings attached light names no model, it is skipped";
						continue;
					}

					TryReadVector(entry, "offset", light.position, 3);

					settings.attachedLights.push_back(light);
				}
			}
		}
	}

	static void LoadMapFile(const std::string& mapName)
	{
		settings = MapSettings{};
		settings.mapName = mapName;

		const std::wstring path = GetMapSettingsPath(mapName);
		hasLoadedWriteTime = TryGetWriteTime(path, loadedWriteTime);

		LoadGlobalTriggers();

		toml::value root;

		if (!TryParseFile(path, root, settings.loadError))
		{
			if (settings.loadError.empty())
			{
				Log() << "remix: no map settings file for " << mapName << " (" << path << ")";
				return;
			}

			LogError() << "remix: map settings for " << mapName << " did not parse, nothing from it is used: "
					   << settings.loadError;
			return;
		}

		settings.hasFile = true;

		ReadSun(root);
		ReadFogAndCulling(root);
		ReadRemixOptions(root);
		ReadDvars(root);
		ReadLights(root);
		ReadMarkers(root);
		ReadTriggers(root, false);
		ReadTextures(root);
		ReadMaterials(root);
		ReadZones(root);

		Log() << "remix: map settings for " << mapName << " loaded, " << settings.remixOptions.size()
			  << " Remix options, " << settings.dvars.size() << " dvars, " << settings.ignoredLights.size()
			  << " lights ignored, " << settings.lightOverrides.size() << " changed, " << settings.addedLights.size()
			  << " added, " << settings.attachedLights.size() << " attached to models, " << settings.markers.size()
			  << " markers, " << settings.triggers.size() << " triggers";
	}

	static std::vector<std::pair<std::uint64_t, TextureTag>> appliedTextureTags;

	static void ApplyTextureTags(IDirect3DDevice9* device, const std::vector<std::pair<std::uint64_t, TextureTag>>& tags)
	{
		const remixapi_Interface* api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		for (const auto& [hash, tag] : appliedTextureTags)
		{
			char text[24];
			std::snprintf(text, sizeof(text), "-0x%016llX", static_cast<unsigned long long>(hash));
			api->SetConfigVariable(TextureTagOption(tag), text);
		}

		for (const auto& [hash, tag] : tags)
		{
			char text[24];
			std::snprintf(text, sizeof(text), "0x%016llX", static_cast<unsigned long long>(hash));
			api->SetConfigVariable(TextureTagOption(tag), text);
		}

		appliedTextureTags = tags;
	}

	static void ApplyMapFile(IDirect3DDevice9* device)
	{
		ApplyTextureTags(device, settings.textureTags);

		if (settings.remixOptions.empty())
		{
			ClearLayer(mapLayer, settings.remixBlendSeconds);
		}
		else
		{
			SetLayerOptions(mapLayer, settings.remixOptions, settings.remixBlendSeconds);
		}

		SetMapCullingLevel(settings.cullingLevel);

		for (const auto& [name, value] : settings.dvars)
		{
			if (!Engine::SetDvar(name.c_str(), value.c_str()))
			{
				LogWarn() << "remix: map settings dvar " << name << " was not set, the game has no such dvar";
			}
		}
	}

	static void ForgetMap(IDirect3DDevice9* device)
	{
		ApplyTextureTags(device, {});
		settings = MapSettings{};
		hasLoadedWriteTime = false;
		ClearLayer(mapLayer, 0.0f);
		SetMapCullingLevel(std::nullopt);
		++settingsVersion;
	}

	void UpdateMapSettings(IDirect3DDevice9* device)
	{
		if (!device)
		{
			return;
		}

		// With no map loaded, the menu's own scene (the lobby's Kino) uses that map's file; its identity is a fixed
		// value no world pointer can have.
		static constexpr std::uintptr_t menuSceneIdentity = 1;
		const std::string menuSceneMap = GetMenuSceneMap();
		std::uintptr_t identity = GetMapIdentity();

		if (identity == 0 && !menuSceneMap.empty())
		{
			identity = menuSceneIdentity;
		}

		if (identity == 0)
		{
			if (loadedIdentity != 0)
			{
				loadedIdentity = 0;
				ForgetMap(device);
			}

			return;
		}

		bool shouldLoad = identity != loadedIdentity || isReloadRequested;

		if (!shouldLoad && ++frameCounter % fileCheckInterval == 0)
		{
			FILETIME writeTime{};
			const bool hasWriteTime = TryGetWriteTime(GetMapSettingsPath(settings.mapName), writeTime);

			const bool hasChanged = hasWriteTime != hasLoadedWriteTime
				|| (hasWriteTime && CompareFileTime(&writeTime, &loadedWriteTime) != 0);

			FILETIME globalTime{};
			const bool hasGlobalTime = TryGetWriteTime(GetRemixDataDirectory() + L"triggers.toml", globalTime);

			const bool hasGlobalChanged = hasGlobalTime != hasGlobalWriteTime
				|| (hasGlobalTime && CompareFileTime(&globalTime, &globalWriteTime) != 0);

			shouldLoad = hasChanged || hasGlobalChanged;
		}

		if (!shouldLoad)
		{
			return;
		}

		loadedIdentity = identity;
		isReloadRequested = false;

		LoadMapFile(identity == menuSceneIdentity ? menuSceneMap : GetMapName());
		ApplyMapFile(device);
		++settingsVersion;
	}

	const MapSettings& GetMapSettings()
	{
		return settings;
	}

	unsigned GetMapSettingsVersion()
	{
		return settingsVersion;
	}

	void ReloadMapSettings()
	{
		isReloadRequested = true;
	}

	std::wstring GetMapSettingsPath(const std::string& mapName)
	{
		return GetRemixDataDirectory() + L"maps\\" + std::wstring(mapName.begin(), mapName.end()) + L".toml";
	}
}
