#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"

namespace Components::BotAI::Waypoints
{
	extern const MapData mp_afghanData;
	extern const MapData mp_boneyardData;
	extern const MapData mp_brecourtData;
	extern const MapData mp_checkpointData;
	extern const MapData mp_derailData;
	extern const MapData mp_estateData;
	extern const MapData mp_favelaData;
	extern const MapData mp_highriseData;
	extern const MapData mp_invasionData;
	extern const MapData mp_nightshiftData;
	extern const MapData mp_quarryData;
	extern const MapData mp_rundownData;
	extern const MapData mp_rustData;
	extern const MapData mp_subbaseData;
	extern const MapData mp_terminalData;
	extern const MapData mp_underpassData;
	extern const MapData mp_abandonData;
	extern const MapData mp_blocData;
	extern const MapData mp_compactData;
	extern const MapData mp_complexData;
	extern const MapData mp_crashData;
	extern const MapData mp_fuel2Data;
	extern const MapData mp_overgrownData;
	extern const MapData mp_stormData;
	extern const MapData mp_strikeData;
	extern const MapData mp_trailerparkData;
	extern const MapData mp_vacantData;

	extern const MapData* const knownMaps[];
	extern const int knownMapCount;

	const MapData* const knownMaps[] = {
		&mp_afghanData,
		&mp_boneyardData,
		&mp_brecourtData,
		&mp_checkpointData,
		&mp_derailData,
		&mp_estateData,
		&mp_favelaData,
		&mp_highriseData,
		&mp_invasionData,
		&mp_nightshiftData,
		&mp_quarryData,
		&mp_rundownData,
		&mp_rustData,
		&mp_subbaseData,
		&mp_terminalData,
		&mp_underpassData,
		&mp_abandonData,
		&mp_blocData,
		&mp_compactData,
		&mp_complexData,
		&mp_crashData,
		&mp_fuel2Data,
		&mp_overgrownData,
		&mp_stormData,
		&mp_strikeData,
		&mp_trailerparkData,
		&mp_vacantData,
	};
	const int knownMapCount = 27;
}
