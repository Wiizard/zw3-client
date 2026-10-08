#include "Components/Modules/BotAI/Control/Ai.hpp"
#include "Components/Modules/BotAI/BotControl.hpp"
#include "Components/Modules/BotAI/Waypoints/Waypoints.hpp"
#include <Windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>

namespace Components::BotAI
{
	static constexpr float memoryCellUnits = 256.0f;
	static constexpr float memoryCellHeight = 128.0f;
	static constexpr float memoryMatchDecay = 0.85f;
	static constexpr float memoryFloor = 0.05f;
	static constexpr float memoryMinEvents = 10.0f;
	static constexpr int   memorySaveMs = 60000;
	static constexpr int   memoryRebuildMs = 5000;
	static constexpr int   killCreditMs = 300;
	static constexpr int   memoryMaxCells = 65536;
	static constexpr int   memoryVersion = 1;

	struct MemoryCell
	{
		float deaths;
		float kills;
	};

	struct MemoryRecord
	{
		unsigned int key;
		float deaths;
		float kills;
	};

	static std::unordered_map<unsigned int, MemoryCell> cells;
	static char memoryMap[64] = "";
	static float memoryEvents = 0.0f;
	static bool isMemoryDirty = false;
	static bool isMemoryChanged = false;
	static int memorySavedTime = 0;
	static int memoryBuiltTime = 0;
	static int memoryBuiltSerial = -1;
	static bool hasAreaMemory = false;
	static float areaDanger[Waypoints::maxAreas] = {};
	static float areaStrength[Waypoints::maxAreas] = {};
	static int areaMembers[Waypoints::maxAreas] = {};
	static bool wasAlive[maxClients] = {};
	static float lastFeet[maxClients][3] = {};

	static unsigned int CellKeyOf(const float* position)
	{
		const int cellX = static_cast<int>(std::floor(position[0] / memoryCellUnits));
		const int cellY = static_cast<int>(std::floor(position[1] / memoryCellUnits));
		const int cellZ = static_cast<int>(std::floor(position[2] / memoryCellHeight));
		return (static_cast<unsigned int>(cellX & 0x3FF) << 20) | (static_cast<unsigned int>(cellY & 0x3FF) << 10)
			| static_cast<unsigned int>(cellZ & 0x3FF);
	}

	static bool TryGetMemoryPath(const char* mapName, char* out, std::size_t outSize)
	{
		char fileName[96];
		_snprintf_s(fileName, sizeof(fileName), _TRUNCATE, "memory_%s.kwm", mapName);
		return BotsPath(fileName, out, outSize);
	}

	static void SaveMapMemory()
	{
		if (!*memoryMap || !isMemoryDirty)
		{
			return;
		}
		char path[MAX_PATH * 2];
		if (!TryGetMemoryPath(memoryMap, path, sizeof(path)))
		{
			return;
		}
		FILE* file = nullptr;
		if (fopen_s(&file, path, "wb") != 0 || !file)
		{
			BotLog("memory could not write %s", path);
			return;
		}
		const int count = static_cast<int>(cells.size());
		std::fwrite("KWM1", 1, 4, file);
		std::fwrite(&memoryVersion, sizeof(memoryVersion), 1, file);
		std::fwrite(&count, sizeof(count), 1, file);
		for (const auto& entry : cells)
		{
			const MemoryRecord record = { entry.first, entry.second.deaths, entry.second.kills };
			std::fwrite(&record, sizeof(record), 1, file);
		}
		std::fclose(file);
		isMemoryDirty = false;
		memorySavedTime = ServerTimeMs();
	}

	void LoadMapMemory(const char* mapName)
	{
		SaveMapMemory();
		cells.clear();
		memoryEvents = 0.0f;
		isMemoryDirty = false;
		isMemoryChanged = true;
		memoryBuiltSerial = -1;
		hasAreaMemory = false;
		memorySavedTime = ServerTimeMs();
		for (int k = 0; k < maxClients; ++k)
		{
			wasAlive[k] = false;
		}
		_snprintf_s(memoryMap, sizeof(memoryMap), _TRUNCATE, "%s", mapName);
		if (!*memoryMap)
		{
			return;
		}

		char path[MAX_PATH * 2];
		if (!TryGetMemoryPath(memoryMap, path, sizeof(path)))
		{
			return;
		}
		FILE* file = nullptr;
		if (fopen_s(&file, path, "rb") != 0 || !file)
		{
			return;
		}
		char magic[4] = {};
		int version = 0;
		int count = 0;
		const bool hasHeader = std::fread(magic, 1, 4, file) == 4 && std::fread(&version, sizeof(version), 1, file) == 1
			&& std::fread(&count, sizeof(count), 1, file) == 1;
		if (!hasHeader || std::memcmp(magic, "KWM1", 4) != 0 || version != memoryVersion || count < 0
			|| count > memoryMaxCells)
		{
			std::fclose(file);
			BotLog("memory %s is not a map memory this build reads, starting over", path);
			return;
		}
		for (int k = 0; k < count; ++k)
		{
			MemoryRecord record = {};
			if (std::fread(&record, sizeof(record), 1, file) != 1)
			{
				break;
			}
			const float deaths = record.deaths * memoryMatchDecay;
			const float kills = record.kills * memoryMatchDecay;
			if (!std::isfinite(deaths) || !std::isfinite(kills) || deaths + kills < memoryFloor)
			{
				continue;
			}
			cells[record.key] = MemoryCell{ deaths, kills };
			memoryEvents += deaths + kills;
		}
		std::fclose(file);
		isMemoryDirty = true;
		BotLog("memory %s: %d cells, %.0f deaths and kills remembered", memoryMap, static_cast<int>(cells.size()),
			memoryEvents);
	}

	static void NoteEvent(const float* position, bool isKill)
	{
		if (static_cast<int>(cells.size()) >= memoryMaxCells && cells.find(CellKeyOf(position)) == cells.end())
		{
			return;
		}
		MemoryCell& cell = cells[CellKeyOf(position)];
		if (isKill)
		{
			cell.kills += 1.0f;
		}
		else
		{
			cell.deaths += 1.0f;
		}
		memoryEvents += 1.0f;
		isMemoryDirty = true;
		isMemoryChanged = true;
	}

	static int KillerOf(int victim, int now)
	{
		if (!IsHuman(victim) && bots[victim].lastAttacker >= 0 && bots[victim].lastAttacker != victim)
		{
			return bots[victim].lastAttacker;
		}
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int k = 0; k < numClients && k < maxClients; ++k)
		{
			if (k != victim && bots[k].input.isActive && bots[k].targetClient == victim
				&& now - bots[k].lastFireTime < killCreditMs)
			{
				return k;
			}
		}
		return -1;
	}

	static void RebuildAreaMemory()
	{
		const int areaCount = Waypoints::AreaCount();
		for (int area = 0; area < areaCount; ++area)
		{
			areaDanger[area] = 0.0f;
			areaStrength[area] = 0.0f;
			areaMembers[area] = 0;
		}
		hasAreaMemory = false;
		if (areaCount == 0 || memoryEvents < memoryMinEvents)
		{
			return;
		}

		const int nodeCount = Waypoints::Count();
		for (int node = 0; node < nodeCount; ++node)
		{
			const int area = Waypoints::AreaOf(node);
			if (area < 0)
			{
				continue;
			}
			++areaMembers[area];
			const auto found = cells.find(CellKeyOf(Waypoints::Origin(node)));
			if (found == cells.end())
			{
				continue;
			}
			areaDanger[area] += found->second.deaths;
			areaStrength[area] += found->second.kills;
		}

		float maxDanger = 0.0f;
		float maxStrength = 0.0f;
		for (int area = 0; area < areaCount; ++area)
		{
			if (areaMembers[area] == 0)
			{
				continue;
			}
			areaDanger[area] /= static_cast<float>(areaMembers[area]);
			areaStrength[area] /= static_cast<float>(areaMembers[area]);
			if (areaDanger[area] > maxDanger)
			{
				maxDanger = areaDanger[area];
			}
			if (areaStrength[area] > maxStrength)
			{
				maxStrength = areaStrength[area];
			}
		}
		for (int area = 0; area < areaCount; ++area)
		{
			if (maxDanger > 0.0f)
			{
				areaDanger[area] /= maxDanger;
			}
			if (maxStrength > 0.0f)
			{
				areaStrength[area] /= maxStrength;
			}
		}
		hasAreaMemory = maxDanger > 0.0f || maxStrength > 0.0f;
	}

	void TrackMapMemory()
	{
		if (!*memoryMap)
		{
			return;
		}
		const int now = ServerTimeMs();
		const int numClients = *reinterpret_cast<int*>(svs_numClients);
		for (int k = 0; k < numClients && k < maxClients; ++k)
		{
			const char* entity = reinterpret_cast<char*>(g_entities) + gentityStride * k;
			const char* client = *reinterpret_cast<char* const*>(entity + gentityClient);
			if (!client)
			{
				wasAlive[k] = false;
				continue;
			}
			const PlayerView view = ReadPlayerView(k);
			if (view.isPlaying)
			{
				wasAlive[k] = true;
				FeetOf(view, lastFeet[k]);
				continue;
			}
			const bool isConnected = *reinterpret_cast<const int*>(client + gclientConnected) != 0;
			const bool isDead = isConnected && *reinterpret_cast<const int*>(entity + gentityHealth) <= 0;
			if (wasAlive[k] && isDead)
			{
				NoteEvent(lastFeet[k], false);
				const int killer = KillerOf(k, now);
				if (killer >= 0 && killer < maxClients && wasAlive[killer])
				{
					NoteEvent(lastFeet[killer], true);
				}
			}
			wasAlive[k] = false;
		}

		const int serial = Waypoints::AnnotationSerial();
		const bool isAreaStale = serial != memoryBuiltSerial;
		if (isAreaStale || (isMemoryChanged && now - memoryBuiltTime >= memoryRebuildMs))
		{
			RebuildAreaMemory();
			memoryBuiltSerial = serial;
			memoryBuiltTime = now;
			isMemoryChanged = false;
		}
		if (isMemoryDirty && now - memorySavedTime >= memorySaveMs)
		{
			SaveMapMemory();
		}
	}

	bool HasMapMemory()
	{
		return hasAreaMemory;
	}

	float AreaDanger(int area)
	{
		if (!hasAreaMemory || area < 0 || area >= Waypoints::AreaCount())
		{
			return 0.0f;
		}
		return areaDanger[area];
	}

	float AreaStrength(int area)
	{
		if (!hasAreaMemory || area < 0 || area >= Waypoints::AreaCount())
		{
			return 0.0f;
		}
		return areaStrength[area];
	}
}
