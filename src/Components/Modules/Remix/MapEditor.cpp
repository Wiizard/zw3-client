#include "STDInclude.hpp"

#include "MapEditor.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>

#include "Core.hpp"
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

	MapDraft& GetMapDraft()
	{
		return draft;
	}

	bool IsMapDraftEmpty()
	{
		return draft.ignoredLights.empty() && draft.lightOverrides.empty() && draft.addedLights.empty()
			&& draft.markers.empty() && draft.cullingAreas.empty();
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
