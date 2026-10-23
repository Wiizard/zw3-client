#include "STDInclude.hpp"

#include "MapEditor.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

#include <algorithm>
#include <map>
#include <unordered_map>

#include "Core.hpp"
#include "Engine.hpp"
#include "Iw4.hpp"
#include "Lights.hpp"
#include "Outdoor.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	static MapDraft draft;

	static std::string FormatVector(const float* values)
	{
		char text[96];
		std::snprintf(text, sizeof(text), "[%.2f, %.2f, %.2f]", values[0], values[1], values[2]);
		return text;
	}

	static std::string FormatHash(std::uint64_t hash)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "\"0x%016llX\"", static_cast<unsigned long long>(hash));
		return text;
	}

	static std::string FormatNumber(float value)
	{
		char text[32];
		std::snprintf(text, sizeof(text), "%.3f", value);
		return text;
	}

	struct SurveyCell
	{
		float mins[3];
		float maxs[3];
		std::vector<int> neighbours;
		std::map<std::string, int> materials;
		std::map<std::string, int> models;
		int surfaces = 0;
		int fixtures = 0;
		int outdoorSamples = 0;
		int samples = 0;
	};

	static std::vector<std::pair<std::string, int>> MostCommon(const std::map<std::string, int>& counts, std::size_t limit)
	{
		std::vector<std::pair<std::string, int>> sorted(counts.begin(), counts.end());
		std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) { return left.second > right.second; });

		if (sorted.size() > limit)
		{
			sorted.resize(limit);
		}

		return sorted;
	}

	bool TryWriteMapSurvey(std::string& message)
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();
		const auto world = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.gfxWorld));
		const auto worldDpvs = ReadAt<std::uintptr_t>(Engine::RebaseAddress(build.g_worldDpvs));
		const std::string mapName = GetMapName();

		if (!world || !worldDpvs || mapName.empty())
		{
			message = "No map is loaded.";
			return false;
		}

		const int cellCount = ReadAt<int>(world + Iw4::worldCellCount);
		const auto cells = ReadAt<std::uintptr_t>(world + Iw4::worldCells);
		const auto treeCounts = ReadAt<std::uintptr_t>(world + Iw4::worldCellTreeCounts);
		const auto trees = ReadAt<std::uintptr_t>(world + Iw4::worldCellTrees);
		const auto sortedSurfIndex = ReadAt<std::uintptr_t>(worldDpvs + Iw4::worldDpvsSortedSurfIndex);
		const auto surfaces = ReadAt<std::uintptr_t>(worldDpvs + Iw4::worldDpvsSurfaces);
		const auto drawInsts = ReadAt<std::uintptr_t>(world + Iw4::worldSModelDrawInsts);

		if (cellCount <= 0 || !cells || !treeCounts || !trees || !sortedSurfIndex || !surfaces)
		{
			message = "The map's cell data could not be read.";
			return false;
		}

		std::vector<SurveyCell> survey(static_cast<std::size_t>(cellCount));

		for (int c = 0; c < cellCount; ++c)
		{
			SurveyCell& entry = survey[c];
			const std::uintptr_t cell = cells + c * Iw4::cellStride;
			float midPoint[3];
			float halfSize[3];
			std::memcpy(midPoint, reinterpret_cast<const void*>(cell + Iw4::cellMidPoint), sizeof(midPoint));
			std::memcpy(halfSize, reinterpret_cast<const void*>(cell + Iw4::cellHalfSize), sizeof(halfSize));

			for (int axis = 0; axis < 3; ++axis)
			{
				entry.mins[axis] = midPoint[axis] - halfSize[axis];
				entry.maxs[axis] = midPoint[axis] + halfSize[axis];
			}

			const int portalCount = ReadAt<int>(cell + Iw4::cellPortalCount);
			const auto portals = ReadAt<std::uintptr_t>(cell + Iw4::cellPortals);

			for (int p = 0; p < portalCount && portals; ++p)
			{
				const int neighbour = ReadAt<std::uint16_t>(portals + p * Iw4::portalStride + Iw4::portalCellIndex);

				if (std::find(entry.neighbours.begin(), entry.neighbours.end(), neighbour) == entry.neighbours.end())
				{
					entry.neighbours.push_back(neighbour);
				}
			}

			const int treeCount = ReadAt<int>(treeCounts + c * sizeof(int));
			const auto tree = ReadAt<std::uintptr_t>(trees + c * sizeof(std::uintptr_t));

			if (tree && treeCount > 0)
			{
				const int surfaceCount = ReadAt<std::uint16_t>(tree + Iw4::aabbTreeSurfaceCount);
				const int startSurface = ReadAt<std::uint16_t>(tree + Iw4::aabbTreeStartSurfIndex);
				entry.surfaces = surfaceCount;

				for (int s = 0; s < surfaceCount; ++s)
				{
					const auto surfaceIndex = ReadAt<std::uint16_t>(sortedSurfIndex + (startSurface + s) * sizeof(std::uint16_t));
					const auto material = ReadAt<std::uintptr_t>(surfaces + surfaceIndex * Iw4::gfxSurfaceStride
						+ Iw4::gfxSurfaceMaterial);
					const char* name = material ? ReadAt<const char*>(material) : nullptr;
					++entry.materials[name ? name : "?"];
				}

				for (int n = 0; n < treeCount; ++n)
				{
					const std::uintptr_t node = tree + n * Iw4::aabbTreeStride;
					const int modelCount = ReadAt<std::uint16_t>(node + Iw4::aabbTreeSModelCount);
					const auto modelIndexes = ReadAt<std::uintptr_t>(node + Iw4::aabbTreeSModelIndexes);

					for (int m = 0; m < modelCount && modelIndexes && drawInsts; ++m)
					{
						const auto instance = ReadAt<std::uint16_t>(modelIndexes + m * sizeof(std::uint16_t));
						const auto model = ReadAt<std::uintptr_t>(drawInsts + instance * Iw4::smodelDrawInstStride
							+ Iw4::smodelDrawInstModel);
						const char* name = model ? ReadAt<const char*>(model + Iw4::xModelName) : nullptr;
						++entry.models[name ? name : "?"];
					}
				}
			}

			for (int gx = 0; gx < 4; ++gx)
			{
				for (int gy = 0; gy < 4; ++gy)
				{
					const float point[3] = {
						entry.mins[0] + (entry.maxs[0] - entry.mins[0]) * (gx + 0.5f) / 4.0f,
						entry.mins[1] + (entry.maxs[1] - entry.mins[1]) * (gy + 0.5f) / 4.0f,
						std::min(entry.mins[2] + 60.0f, entry.maxs[2]),
					};
					bool isOutdoors = false;

					if (TryTestPointOutdoors(point, isOutdoors))
					{
						++entry.samples;
						entry.outdoorSamples += isOutdoors ? 1 : 0;
					}
				}
			}
		}

		for (const MapLightInfo& light : GetMapLightInfo())
		{
			if (!light.isFixture)
			{
				continue;
			}

			for (SurveyCell& entry : survey)
			{
				const bool isInside = light.origin[0] >= entry.mins[0] && light.origin[0] <= entry.maxs[0]
					&& light.origin[1] >= entry.mins[1] && light.origin[1] <= entry.maxs[1]
					&& light.origin[2] >= entry.mins[2] && light.origin[2] <= entry.maxs[2];

				if (isInside)
				{
					++entry.fixtures;
					break;
				}
			}
		}

		const std::wstring fileName = L"zw3_remix_survey_" + std::wstring(mapName.begin(), mapName.end()) + L".txt";
		std::ofstream file(Core::ModuleDirectory() + fileName, std::ios::trunc);

		if (!file)
		{
			message = "Could not write the survey file.";
			return false;
		}

		file << "map " << mapName << ", " << cellCount << " cells (portal zones)\n";
		file << "each cell: bounds, size, centre, neighbours through portals, world surfaces and their materials, static"
				" models, lit fixtures and outdoor share (4x4 samples at eye height, - when the map has no outdoor mask)\n";

		for (int c = 0; c < cellCount; ++c)
		{
			const SurveyCell& entry = survey[c];
			char line[320];
			std::snprintf(line, sizeof(line),
				"\n== cell %d  min (%.0f %.0f %.0f) max (%.0f %.0f %.0f)  size %.0f x %.0f x %.0f  centre (%.0f %.0f %.0f)\n",
				c, entry.mins[0], entry.mins[1], entry.mins[2], entry.maxs[0], entry.maxs[1], entry.maxs[2],
				entry.maxs[0] - entry.mins[0], entry.maxs[1] - entry.mins[1], entry.maxs[2] - entry.mins[2],
				(entry.mins[0] + entry.maxs[0]) * 0.5f, (entry.mins[1] + entry.maxs[1]) * 0.5f,
				(entry.mins[2] + entry.maxs[2]) * 0.5f);
			file << line;

			file << "neighbours:";

			for (const int neighbour : entry.neighbours)
			{
				file << " " << neighbour;
			}

			file << "\nsurfaces " << entry.surfaces << ", fixtures " << entry.fixtures << ", outdoor ";

			if (entry.samples > 0)
			{
				file << (entry.outdoorSamples * 100 / entry.samples) << "%\n";
			}
			else
			{
				file << "-\n";
			}

			file << "materials:";

			for (const auto& [name, count] : MostCommon(entry.materials, 10))
			{
				file << " " << name << " x" << count << ";";
			}

			file << "\nmodels:";

			for (const auto& [name, count] : MostCommon(entry.models, 10))
			{
				file << " " << name << " x" << count << ";";
			}

			file << "\n";
		}

		message = "Wrote zw3_remix_survey_" + mapName + ".txt (" + std::to_string(cellCount) + " cells).";
		Log() << "remix: " << message;
		return true;
	}

	MapDraft& GetMapDraft()
	{
		return draft;
	}

	bool IsMapDraftEmpty()
	{
		return draft.ignoredLights.empty() && draft.lightOverrides.empty() && draft.addedLights.empty()
			&& draft.markers.empty() && draft.cullingAreas.empty() && draft.zones.empty();
	}

	void ClearMapDraft()
	{
		draft = MapDraft{};
	}

	std::string BuildMapDraftToml()
	{
		std::string text;

		for (const std::uint64_t hash : draft.ignoredLights)
		{
			text += "\n[[lights.override]]\nhash = " + FormatHash(hash) + "\nignore = true\n";
		}

		for (const LightOverride& change : draft.lightOverrides)
		{
			text += "\n[[lights.override]]\nhash = " + FormatHash(change.hash) + "\n";

			if (change.brightness)
			{
				text += "brightness = " + FormatNumber(*change.brightness) + "\n";
			}

			if (change.hasColour)
			{
				text += "colour = " + FormatVector(change.colour) + "\n";
			}

			if (change.radius)
			{
				text += "radius = " + FormatNumber(*change.radius) + "\n";
			}

			if (change.coneDeg)
			{
				text += "cone = " + FormatNumber(*change.coneDeg) + "\n";
			}

			if (change.volumetric)
			{
				text += "volumetric = " + FormatNumber(*change.volumetric) + "\n";
			}
		}

		for (const AuthoredLight& light : draft.addedLights)
		{
			text += "\n[[lights.add]]\nposition = " + FormatVector(light.position) + "\ncolour = "
				+ FormatVector(light.colour) + "\nintensity = " + FormatNumber(light.intensity) + "\nradius = "
				+ FormatNumber(light.radius) + "\n";

			if (light.hasDirection)
			{
				text += "direction = " + FormatVector(light.direction) + "\ncone = " + FormatNumber(light.coneDeg)
					+ "\nsoftness = " + FormatNumber(light.softness) + "\n";
			}

			if (light.volumetric != 1.0f)
			{
				text += "volumetric = " + FormatNumber(light.volumetric) + "\n";
			}
		}

		for (const MapMarker& marker : draft.markers)
		{
			text += "\n[[markers]]\nid = " + std::to_string(marker.id) + "\nposition = " + FormatVector(marker.position)
				+ "\nrotation = " + FormatVector(marker.rotationDeg) + "\n";
		}

		for (const CullingArea& area : draft.cullingAreas)
		{
			text += "\n[[culling.areas]]\nmin = " + FormatVector(area.boxMin) + "\nmax = " + FormatVector(area.boxMax)
				+ "\nradius = " + FormatNumber(area.radius) + "\n";
		}

		for (const MapZone& zone : draft.zones)
		{
			text += "\n[[zones]]\nname = \"" + zone.name + "\"\n";

			if (!zone.cells.empty())
			{
				text += "cells = [";

				for (std::size_t i = 0; i < zone.cells.size(); ++i)
				{
					text += (i ? ", " : "") + std::to_string(zone.cells[i]);
				}

				text += "]\n";
			}

			if (zone.hasBox)
			{
				text += "min = " + FormatVector(zone.boxMin) + "\nmax = " + FormatVector(zone.boxMax) + "\n";
			}

			if (zone.exposureEv)
			{
				text += "exposure_ev = " + FormatNumber(*zone.exposureEv) + "\n";
			}

			if (zone.shadowLift)
			{
				text += "shadow_lift = " + FormatNumber(*zone.shadowLift) + "\n";
			}

			if (zone.effectGlow)
			{
				text += "effect_glow = " + FormatNumber(*zone.effectGlow) + "\n";
			}

			if (zone.lampBrightness)
			{
				text += "lamp_brightness = " + FormatNumber(*zone.lampBrightness) + "\n";
			}

			if (zone.hasLampColour)
			{
				text += "lamp_colour = " + FormatVector(zone.lampColour) + "\n";
			}

			if (zone.contrast)
			{
				text += "contrast = " + FormatNumber(*zone.contrast) + "\n";
			}

			if (zone.saturation)
			{
				text += "saturation = " + FormatNumber(*zone.saturation) + "\n";
			}

			if (zone.hasTint)
			{
				text += "tint = " + FormatVector(zone.tint) + "\n";
			}

			text += "blend_seconds = " + FormatNumber(zone.blendSeconds) + "\n";
		}

		return text;
	}

	bool TryAppendMapDraft(std::string& message)
	{
		const std::string mapName = GetMapName();

		if (mapName.empty())
		{
			message = "No map is loaded.";
			return false;
		}

		if (IsMapDraftEmpty())
		{
			message = "Nothing to write.";
			return false;
		}

		const std::wstring folder = GetRemixDataDirectory();
		CreateDirectoryW(folder.c_str(), nullptr);
		CreateDirectoryW((folder + L"maps").c_str(), nullptr);

		const std::wstring path = GetMapSettingsPath(mapName);
		std::ofstream file(path, std::ios::app | std::ios::binary);

		if (!file)
		{
			message = "Could not open zw3_remix\\maps\\" + mapName + ".toml for writing.";
			return false;
		}

		file << BuildMapDraftToml();
		file.close();

		if (!file)
		{
			message = "Writing zw3_remix\\maps\\" + mapName + ".toml failed.";
			return false;
		}

		Log() << "remix: the map editor appended its changes to " << path;
		message = "Added to zw3_remix\\maps\\" + mapName + ".toml and reloaded.";
		ClearMapDraft();
		ReloadMapSettings();
		return true;
	}

	bool TryCopyToClipboard(const std::string& text)
	{
		if (!OpenClipboard(nullptr))
		{
			return false;
		}

		EmptyClipboard();

		const HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);

		if (!memory)
		{
			CloseClipboard();
			return false;
		}

		void* bytes = GlobalLock(memory);
		std::memcpy(bytes, text.c_str(), text.size() + 1);
		GlobalUnlock(memory);

		if (!SetClipboardData(CF_TEXT, memory))
		{
			GlobalFree(memory);
			CloseClipboard();
			return false;
		}

		CloseClipboard();
		return true;
	}
}
