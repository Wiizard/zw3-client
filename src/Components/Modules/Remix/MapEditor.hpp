#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "MapSettings.hpp"

namespace Components::Remix
{
	struct MapDraft
	{
		std::vector<std::uint64_t> ignoredLights;
		std::vector<LightOverride> lightOverrides;
		std::vector<AuthoredLight> addedLights;
		std::vector<MapMarker> markers;
		std::vector<CullingArea> cullingAreas;
		std::vector<MapZone> zones;
	};

	MapDraft& GetMapDraft();

	bool IsMapDraftEmpty();

	void ClearMapDraft();

	std::string BuildMapDraftToml();

	bool TryAppendMapDraft(std::string& message);

	bool TryCopyToClipboard(const std::string& text);

	bool TryWriteMapSurvey(std::string& message);
}
