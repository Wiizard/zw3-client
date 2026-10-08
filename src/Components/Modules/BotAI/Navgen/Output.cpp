#include "Components/Modules/BotAI/Navgen/Internal.hpp"

#include <Windows.h>
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include "Components/Modules/BotAI/Iw4.hpp"
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <direct.h>
#include <io.h>

namespace Components::BotAI::Navgen
{
	static FILE* reportFile = nullptr;

	static void EmitLine(bool toConsole, const char* format, va_list args)
	{
		char line[512];
		_vsnprintf_s(line, sizeof(line), _TRUNCATE, format, args);

		if (toConsole)
		{
			Print("%s", line);
		}
		if (reportFile)
		{
			std::fputs(line, reportFile);
			std::fflush(reportFile);
		}
	}

	void Report(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		EmitLine(true, format, args);
		va_end(args);
	}

	void ReportFile(const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		EmitLine(false, format, args);
		va_end(args);
	}

	void ReportCapped(int line, int consoleCap, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		EmitLine(line < consoleCap, format, args);
		va_end(args);
	}

	int NowMs()
	{
		return Sys_Milliseconds();
	}

	static void StampText(unsigned long stamp, char* out, int outSize)
	{
		if (!stamp)
		{
			_snprintf_s(out, outSize, _TRUNCATE, "unstamped");
			return;
		}
		const time_t when = static_cast<time_t>(stamp);
		tm local = {};
		localtime_s(&local, &when);
		std::strftime(out, outSize, "%Y-%m-%d %H:%M", &local);
	}


	static Waypoints::Node finalNodes[maxFinalNodes];
	static unsigned int finalChildren[maxFinalChildren];
	static int finalNodeCount = 0;
	static int finalChildCount = 0;
	static int rawIndexOfFinal[maxFinalNodes];
	static int regionCount = 0;

	static bool hasLoadedFile = false;
	static int loadedRevision = 0;
	static unsigned long loadedStamp = 0;

	struct RunSummary
	{
		int nodes;
		int links;
		int walkLinks;
		int oneWayWalks;
		int crouchLinks;
		int glassLinks;
		int mantleLinks;
		int dropLinks;
		int ladderLinks;
		int jumpLinks;
		int steepWalks;
		int mantlesOver60;
		int components;
		int strandedSpawns;
		int spawnsCovered;
		int spawnTotal;
		int objectivesCovered;
		int objectiveTotal;
		int holes;
		int holeCells;
		int holesAboveGraph;
		int holesInClip;
		int hurtCells;
		int glassCrossings;
		int msSeed;
		int msFill;
		int msPolicy;
		int msPrune;
		int msBuild;
		int msReport;
		int msGlass;
		int msHoles;
		int msAnnotate;
		int msTotal;
		bool isSaved;
	};
	static RunSummary summary;

	static bool HasFinalLink(int from, int to)
	{
		const Waypoints::Node& node = finalNodes[from];
		for (int c = 0; c < node.childCount; ++c)
		{
			if (static_cast<int>(finalChildren[node.firstChild + c] & Waypoints::linkIndexMask) == to)
			{
				return true;
			}
		}
		return false;
	}

	static bool BuildFinalGraph()
	{
		static int remap[maxRawNodes];

		finalNodeCount = 0;
		finalChildCount = 0;

		for (int i = 0; i < rawCount; ++i)
		{
			remap[i] = -1;
			if (rawNodes[i].removed)
			{
				continue;
			}
			if (finalNodeCount >= maxFinalNodes)
			{
				return false;
			}
			rawIndexOfFinal[finalNodeCount] = i;
			remap[i] = finalNodeCount++;
		}

		int written = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (remap[i] < 0)
			{
				continue;
			}

			Waypoints::Node& node = finalNodes[remap[i]];
			node.origin[0] = rawNodes[i].origin[0];
			node.origin[1] = rawNodes[i].origin[1];
			node.origin[2] = rawNodes[i].origin[2];
			node.type = rawNodes[i].type;
			node.region = 0;
			node.firstChild = static_cast<unsigned int>(finalChildCount);

			int childCount = 0;
			for (int c = 0; c < rawNodes[i].linkCount; ++c)
			{
				const int target = remap[rawNodes[i].links[c]];
				if (target < 0 || finalChildCount >= maxFinalChildren)
				{
					continue;
				}

				unsigned int flags = 0;
				const unsigned char kind = rawNodes[i].linkKind[c];
				if (kind == Waypoints::LinkMantle)
				{
					flags |= Waypoints::linkMantle;
				}
				else if (kind == Waypoints::LinkDrop)
				{
					flags |= Waypoints::linkDrop;
				}
				else if (kind == Waypoints::LinkLadder)
				{
					flags |= Waypoints::linkLadder;
				}
				else if (kind == Waypoints::LinkJump)
				{
					flags |= Waypoints::linkJump;
				}
				if (rawNodes[i].linkFlags[c] & RawLinkCrouch)
				{
					flags |= Waypoints::linkCrouch;
				}
				if (rawNodes[i].linkFlags[c] & RawLinkJumpFirst)
				{
					flags |= Waypoints::linkJump;
				}
				if (rawNodes[i].linkFlags[c] & RawLinkGlass)
				{
					flags |= Waypoints::linkGlass;
				}
				finalChildren[finalChildCount++] = static_cast<unsigned int>(target) | flags;
				++childCount;
			}
			node.childCount = static_cast<unsigned char>(childCount);
			++written;
		}

		return written > 0;
	}


	static constexpr int regionConsoleCap = 12;

	static void ComputeFinalRegions()
	{
		static int reverseStart[maxFinalNodes + 1];
		static int reverseCursor[maxFinalNodes];
		static int reverseSource[maxFinalChildren];
		static bool reachForward[maxFinalNodes];
		static bool reachBack[maxFinalNodes];
		static int queue[maxFinalNodes];

		regionCount = 0;
		summary.strandedSpawns = 0;
		for (int i = 0; i < finalNodeCount; ++i)
		{
			finalNodes[i].region = 0;
		}

		for (int i = 0; i <= finalNodeCount; ++i)
		{
			reverseStart[i] = 0;
		}
		for (int i = 0; i < finalNodeCount; ++i)
		{
			const Waypoints::Node& node = finalNodes[i];
			for (int c = 0; c < node.childCount; ++c)
			{
				const int child = finalChildren[node.firstChild + c] & Waypoints::linkIndexMask;
				++reverseStart[child + 1];
			}
		}
		for (int i = 0; i < finalNodeCount; ++i)
		{
			reverseStart[i + 1] += reverseStart[i];
			reverseCursor[i] = reverseStart[i];
		}
		for (int i = 0; i < finalNodeCount; ++i)
		{
			const Waypoints::Node& node = finalNodes[i];
			for (int c = 0; c < node.childCount; ++c)
			{
				const int child = finalChildren[node.firstChild + c] & Waypoints::linkIndexMask;
				reverseSource[reverseCursor[child]++] = i;
			}
		}

		int triedHubs[8];
		int triedCount = 0;
		int hub = -1;
		int mutualSize = 0;
		while (triedCount < 8)
		{
			hub = -1;
			for (int i = 0; i < finalNodeCount; ++i)
			{
				bool tried = false;
				for (int t = 0; t < triedCount; ++t)
				{
					if (triedHubs[t] == i)
					{
						tried = true;
					}
				}
				if (!tried && (hub < 0 || finalNodes[i].childCount > finalNodes[hub].childCount))
				{
					hub = i;
				}
			}
			if (hub < 0)
			{
				break;
			}
			triedHubs[triedCount++] = hub;

			for (int i = 0; i < finalNodeCount; ++i)
			{
				reachForward[i] = false;
				reachBack[i] = false;
			}

			int head = 0;
			int tail = 0;
			reachForward[hub] = true;
			queue[tail++] = hub;
			while (head < tail)
			{
				const Waypoints::Node& node = finalNodes[queue[head++]];
				for (int c = 0; c < node.childCount; ++c)
				{
					const int child = finalChildren[node.firstChild + c] & Waypoints::linkIndexMask;
					if (!reachForward[child])
					{
						reachForward[child] = true;
						queue[tail++] = child;
					}
				}
			}

			head = 0;
			tail = 0;
			reachBack[hub] = true;
			queue[tail++] = hub;
			while (head < tail)
			{
				const int current = queue[head++];
				for (int r = reverseStart[current]; r < reverseStart[current + 1]; ++r)
				{
					const int source = reverseSource[r];
					if (!reachBack[source])
					{
						reachBack[source] = true;
						queue[tail++] = source;
					}
				}
			}

			mutualSize = 0;
			for (int i = 0; i < finalNodeCount; ++i)
			{
				if (reachForward[i] && reachBack[i])
				{
					++mutualSize;
				}
			}
			if (mutualSize * 2 >= finalNodeCount)
			{
				break;
			}
			hub = -1;
		}
		if (hub < 0)
		{
			Report("navgen: directed (final ids): no dominant mass found, regions not stamped\n");
			return;
		}
		Report("navgen: directed (final ids): mass %d of %d around node %d\n", mutualSize, finalNodeCount, hub);

		int cannotReachNodes = 0;
		int cannotReachRegions = 0;
		int unreachableNodes = 0;
		int unreachableRegions = 0;
		for (int pass = 0; pass < 2; ++pass)
		{
			const char* verdict = "cannot reach the mass";
			if (pass == 1)
			{
				verdict = "the mass cannot reach it";
			}

			for (int seed = 0; seed < finalNodeCount; ++seed)
			{
				bool isInSet = !reachBack[seed];
				if (pass == 1)
				{
					isInSet = reachBack[seed] && !reachForward[seed];
				}
				if (!isInSet || finalNodes[seed].region != 0)
				{
					continue;
				}

				++regionCount;
				unsigned char regionId = 255;
				if (regionCount < 255)
				{
					regionId = static_cast<unsigned char>(regionCount);
				}

				int size = 0;
				int spawns = 0;
				float low[3] = { 0.0f, 0.0f, 0.0f };
				float high[3] = { 0.0f, 0.0f, 0.0f };
				int head = 0;
				int tail = 0;
				finalNodes[seed].region = regionId;
				queue[tail++] = seed;
				while (head < tail)
				{
					const int current = queue[head++];
					const float* origin = finalNodes[current].origin;
					for (int axis = 0; axis < 3; ++axis)
					{
						if (size == 0 || origin[axis] < low[axis])
						{
							low[axis] = origin[axis];
						}
						if (size == 0 || origin[axis] > high[axis])
						{
							high[axis] = origin[axis];
						}
					}
					++size;
					if (rawIndexOfFinal[current] >= 0 && rawNodes[rawIndexOfFinal[current]].isSeed)
					{
						++spawns;
					}

					const Waypoints::Node& node = finalNodes[current];
					for (int c = 0; c < node.childCount; ++c)
					{
						const int child = finalChildren[node.firstChild + c] & Waypoints::linkIndexMask;
						bool isChildInSet = !reachBack[child];
						if (pass == 1)
						{
							isChildInSet = reachBack[child] && !reachForward[child];
						}
						if (isChildInSet && finalNodes[child].region == 0)
						{
							finalNodes[child].region = regionId;
							queue[tail++] = child;
						}
					}
					for (int r = reverseStart[current]; r < reverseStart[current + 1]; ++r)
					{
						const int source = reverseSource[r];
						bool isSourceInSet = !reachBack[source];
						if (pass == 1)
						{
							isSourceInSet = reachBack[source] && !reachForward[source];
						}
						if (isSourceInSet && finalNodes[source].region == 0)
						{
							finalNodes[source].region = regionId;
							queue[tail++] = source;
						}
					}
				}

				if (pass == 0)
				{
					cannotReachNodes += size;
					++cannotReachRegions;
					summary.strandedSpawns += spawns;
				}
				else
				{
					unreachableNodes += size;
					++unreachableRegions;
				}
				char spawnText[32] = "";
				if (spawns)
				{
					_snprintf_s(spawnText, sizeof(spawnText), _TRUNCATE, ", holds %d spawn(s)", spawns);
				}
				ReportCapped(regionCount - 1, regionConsoleCap,
							 "navgen:   region %d: %d node(s) within %.0f %.0f %.0f .. %.0f %.0f %.0f, %s%s\n",
							 regionCount, size, low[0], low[1], low[2], high[0], high[1], high[2],
							 verdict, spawnText);
			}
		}
		Report("navgen: directed (final ids): %d node(s) in %d region(s) cannot reach the mass (%d spawn(s) stranded), "
			   "%d node(s) in %d region(s) the mass cannot reach\n",
			   cannotReachNodes, cannotReachRegions, summary.strandedSpawns, unreachableNodes, unreachableRegions);

		for (int i = 0; i < finalNodeCount; ++i)
		{
			if (rawIndexOfFinal[i] >= 0)
			{
				rawNodes[rawIndexOfFinal[i]].region = finalNodes[i].region;
			}
		}
	}

	int RegionCount()
	{
		return regionCount;
	}


	void FileNameFor(const char* mapName, char* out, int outSize)
	{
		_snprintf_s(out, outSize, _TRUNCATE, "waypoints_%s.kwp", mapName);
	}

	void SaveActiveGraph(const char* mapName)
	{
		char fileName[96];
		FileNameFor(mapName, fileName, sizeof(fileName));

		char path[128];
		char previousPath[128];
		_snprintf_s(path, sizeof(path), _TRUNCATE, "players/%s", fileName);
		_snprintf_s(previousPath, sizeof(previousPath), _TRUNCATE, "players/%s.prev", fileName);
		std::remove(previousPath);
		if (std::rename(path, previousPath) == 0)
		{
			Report("navgen: kept the previous %s as .prev\n", path);
		}
		else
		{
			char cwd[260] = "?";
			_getcwd(cwd, sizeof(cwd));
			const char* presence = "no such file from here";
			if (_access(path, 0) == 0)
			{
				presence = "the file is there";
			}
			Report("navgen: could not keep %s as .prev (errno %d, cwd %s, %s)\n", path, errno, cwd, presence);
		}

		char savePath[MAX_PATH * 2];
		if (!BotsPath(fileName, savePath, sizeof(savePath)))
		{
			Report("navgen: no main\\zw3\\bots folder for %s\n", fileName);
			return;
		}

		FILE* handle = nullptr;
		if (fopen_s(&handle, savePath, "w") != 0 || !handle)
		{
			Report("navgen: cannot open %s for writing\n", savePath);
			return;
		}

		const auto print = fprintf;
		const int count = Waypoints::Count();
		const int objectiveCount = Waypoints::ObjectiveCount();
		print(handle, "kwp 4 %s %d %d %lu %d\n", mapName, count, loadedRevision, loadedStamp, objectiveCount);
		for (int i = 0; i < count; ++i)
		{
			const float* origin = Waypoints::Origin(i);
			print(handle, "%.1f %.1f %.1f %d %d %d", origin[0], origin[1], origin[2],
				  Waypoints::TypeOf(i), Waypoints::RegionOf(i), Waypoints::ChildCount(i));
			for (int c = 0; c < Waypoints::ChildCount(i); ++c)
			{
				print(handle, " %u", static_cast<unsigned int>(Waypoints::ChildAt(i, c)) | Waypoints::ChildFlags(i, c));
			}
			print(handle, "\n");
		}
		for (int i = 0; i < objectiveCount; ++i)
		{
			const Waypoints::Objective& objective = Waypoints::ObjectiveAt(i);
			const char* gametype = objective.gametype;
			if (!*gametype)
			{
				gametype = "-";
			}
			print(handle, "%s %s %.1f %.1f %.1f %d\n", objective.name, gametype,
				  objective.origin[0], objective.origin[1], objective.origin[2],
				  Waypoints::Nearest(objective.origin));
		}

		std::fclose(handle);
		Report("navgen: saved %s (gen rev %d, %d nodes, %d objective(s))\n",
			   savePath, loadedRevision, count, objectiveCount);
	}


	static bool ReadFloat(char** cursor, float* out)
	{
		char* end = nullptr;
		*out = std::strtof(*cursor, &end);
		if (end == *cursor)
		{
			return false;
		}
		*cursor = end;
		return true;
	}

	static bool ReadInt(char** cursor, int* out)
	{
		char* end = nullptr;
		*out = static_cast<int>(std::strtol(*cursor, &end, 10));
		if (end == *cursor)
		{
			return false;
		}
		*cursor = end;
		return true;
	}

	static bool ReadUnsigned(char** cursor, unsigned long* out)
	{
		char* end = nullptr;
		*out = std::strtoul(*cursor, &end, 10);
		if (end == *cursor)
		{
			return false;
		}
		*cursor = end;
		return true;
	}

	static bool ReadWord(char** cursor, char* out, int outSize)
	{
		char* at = *cursor;
		while (*at == ' ' || *at == '\t' || *at == '\r' || *at == '\n')
		{
			++at;
		}
		if (!*at)
		{
			return false;
		}
		int used = 0;
		while (*at && *at != ' ' && *at != '\t' && *at != '\r' && *at != '\n')
		{
			if (used < outSize - 1)
			{
				out[used++] = *at;
			}
			++at;
		}
		out[used] = 0;
		*cursor = at;
		return true;
	}


	bool TryLoadFile(const char* mapName)
	{
		hasLoadedFile = false;
		loadedRevision = 0;
		loadedStamp = 0;
		regionCount = 0;
		if (!mapName || !*mapName)
		{
			return false;
		}

		char fileName[96];
		FileNameFor(mapName, fileName, sizeof(fileName));

		char loadPath[MAX_PATH * 2];
		if (!BotsPath(fileName, loadPath, sizeof(loadPath)))
		{
			return false;
		}

		FILE* file = nullptr;
		if (fopen_s(&file, loadPath, "rb") != 0 || !file)
		{
			return false;
		}
		std::fseek(file, 0, SEEK_END);
		const long length = std::ftell(file);
		std::fseek(file, 0, SEEK_SET);
		if (length <= 0)
		{
			std::fclose(file);
			return false;
		}
		char* text = static_cast<char*>(std::malloc(static_cast<size_t>(length) + 1));
		if (!text)
		{
			std::fclose(file);
			return false;
		}
		const size_t got = std::fread(text, 1, static_cast<size_t>(length), file);
		std::fclose(file);
		text[got] = 0;

		char* cursor = text;
		int version = 0;
		if (std::strncmp(cursor, "kwp 1 ", 6) == 0)
		{
			version = 1;
		}
		else if (std::strncmp(cursor, "kwp 2 ", 6) == 0)
		{
			version = 2;
		}
		else if (std::strncmp(cursor, "kwp 3 ", 6) == 0)
		{
			version = 3;
		}
		else if (std::strncmp(cursor, "kwp 4 ", 6) == 0)
		{
			version = 4;
		}

		int nodeCount = 0;
		int revision = 0;
		unsigned long stamp = 0;
		int objectiveCount = 0;
		int rejectedNode = -1;
		char reason[96] = "";
		if (!version)
		{
			_snprintf_s(reason, sizeof(reason), _TRUNCATE, "unknown header");
		}
		else
		{
			cursor += 6;
			while (*cursor && *cursor != ' ')
			{
				++cursor;
			}
			if (!ReadInt(&cursor, &nodeCount) || nodeCount <= 0 || nodeCount > Waypoints::nodeCap)
			{
				_snprintf_s(reason, sizeof(reason), _TRUNCATE, "node count %d outside 1..%d",
							nodeCount, Waypoints::nodeCap);
			}
			else if (version >= 3 && (!ReadInt(&cursor, &revision) || !ReadUnsigned(&cursor, &stamp)))
			{
				_snprintf_s(reason, sizeof(reason), _TRUNCATE, "kwp %d header without a revision and a stamp", version);
			}
			else if (version >= 4 && (!ReadInt(&cursor, &objectiveCount) || objectiveCount < 0
									  || objectiveCount > Waypoints::maxObjectives))
			{
				_snprintf_s(reason, sizeof(reason), _TRUNCATE, "objective count outside 0..%d",
							Waypoints::maxObjectives);
			}
		}

		if (!*reason)
		{
			finalNodeCount = 0;
			finalChildCount = 0;
			for (int i = 0; i < nodeCount && !*reason; ++i)
			{
				Waypoints::Node& node = finalNodes[i];
				int type = 0;
				int region = 0;
				int childCount = 0;
				const bool hasFields = ReadFloat(&cursor, &node.origin[0]) && ReadFloat(&cursor, &node.origin[1])
					&& ReadFloat(&cursor, &node.origin[2]) && ReadInt(&cursor, &type)
					&& (version < 3 || ReadInt(&cursor, &region)) && ReadInt(&cursor, &childCount);
				if (!hasFields)
				{
					rejectedNode = i;
					_snprintf_s(reason, sizeof(reason), _TRUNCATE, "node line cut short");
					break;
				}
				if (type < 0 || type > Waypoints::NodeJavelin)
				{
					rejectedNode = i;
					_snprintf_s(reason, sizeof(reason), _TRUNCATE, "unknown node type %d", type);
					break;
				}
				if (childCount < 0 || childCount > Waypoints::maxFileLinks)
				{
					rejectedNode = i;
					_snprintf_s(reason, sizeof(reason), _TRUNCATE, "child count %d over the file cap of %d",
								childCount, Waypoints::maxFileLinks);
					break;
				}
				if (finalChildCount + childCount > maxFinalChildren)
				{
					rejectedNode = i;
					_snprintf_s(reason, sizeof(reason), _TRUNCATE, "children over the cap of %d", maxFinalChildren);
					break;
				}

				node.type = static_cast<unsigned char>(type);
				node.region = 0;
				if (region > 0)
				{
					if (region > 255)
					{
						region = 255;
					}
					node.region = static_cast<unsigned char>(region);
					if (region > regionCount)
					{
						regionCount = region;
					}
				}
				node.firstChild = static_cast<unsigned int>(finalChildCount);
				node.childCount = static_cast<unsigned char>(childCount);
				for (int c = 0; c < childCount; ++c)
				{
					unsigned long raw = 0;
					if (!ReadUnsigned(&cursor, &raw))
					{
						rejectedNode = i;
						_snprintf_s(reason, sizeof(reason), _TRUNCATE, "child %d of %d missing", c, childCount);
						break;
					}
					unsigned int entry = static_cast<unsigned int>(raw);
					if (version == 1)
					{
						entry = static_cast<unsigned int>(raw & 0x3FFFu);
						if (raw & 0x8000u)
						{
							entry |= Waypoints::linkMantle;
						}
						if (raw & 0x4000u)
						{
							entry |= Waypoints::linkDrop;
						}
					}
					if ((entry & Waypoints::linkIndexMask) >= static_cast<unsigned int>(nodeCount))
					{
						rejectedNode = i;
						_snprintf_s(reason, sizeof(reason), _TRUNCATE, "child index %u out of range",
									entry & Waypoints::linkIndexMask);
						break;
					}
					finalChildren[finalChildCount++] = entry;
				}
			}
		}

		static Waypoints::Objective objectives[Waypoints::maxObjectives];
		for (int i = 0; i < objectiveCount && !*reason; ++i)
		{
			Waypoints::Objective& objective = objectives[i];
			int node = 0;
			bool hasFields = ReadWord(&cursor, objective.name, sizeof(objective.name))
				&& ReadWord(&cursor, objective.gametype, sizeof(objective.gametype));
			bool hasOrigin = hasFields && ReadFloat(&cursor, &objective.origin[0]);
			for (int extra = 0; hasFields && !hasOrigin && extra < 3; ++extra)
			{
				char skipped[16];
				hasFields = ReadWord(&cursor, skipped, sizeof(skipped));
				hasOrigin = hasFields && ReadFloat(&cursor, &objective.origin[0]);
			}
			hasFields = hasOrigin && ReadFloat(&cursor, &objective.origin[1])
				&& ReadFloat(&cursor, &objective.origin[2]) && ReadInt(&cursor, &node);
			if (!hasFields)
			{
				_snprintf_s(reason, sizeof(reason), _TRUNCATE, "objective line %d of %d cut short", i, objectiveCount);
				break;
			}
			if (std::strcmp(objective.gametype, "-") == 0)
			{
				objective.gametype[0] = 0;
			}
			objective.node = -1;
		}

		std::free(text);

		if (*reason)
		{
			if (rejectedNode >= 0)
			{
				Report("navgen: players/%s rejected at node %d: %s; the baked set is the fallback\n",
					   fileName, rejectedNode, reason);
			}
			else
			{
				Report("navgen: players/%s rejected: %s; the baked set is the fallback\n", fileName, reason);
			}
			return false;
		}

		finalNodeCount = nodeCount;
		for (int i = 0; i < nodeCount; ++i)
		{
			rawIndexOfFinal[i] = -1;
		}
		if (!Waypoints::InstallGenerated(finalNodes, finalNodeCount, finalChildren, finalChildCount))
		{
			Report("navgen: players/%s did not install (%d nodes, %d children)\n",
				   fileName, finalNodeCount, finalChildCount);
			return false;
		}

		Waypoints::SetObjectives(objectives, objectiveCount);

		hasLoadedFile = true;
		loadedRevision = revision;
		loadedStamp = stamp;
		char made[32];
		StampText(stamp, made, sizeof(made));
		Report("navgen: loaded players/%s: %d nodes, %d objective(s), gen rev %d, made %s\n",
			   fileName, nodeCount, objectiveCount, revision, made);
		if (IsLoadedStale())
		{
			Report("navgen: stale gen rev %d, generator %d: hold bots_generatenodes at 1 to regenerate\n",
				   revision, generatorRevision);
		}
		return true;
	}

	bool IsLoadedStale()
	{
		return hasLoadedFile && loadedRevision < generatorRevision;
	}

	int LoadedRevision()
	{
		if (!hasLoadedFile)
		{
			return 0;
		}
		return loadedRevision;
	}

	unsigned long LoadedStamp()
	{
		if (!hasLoadedFile)
		{
			return 0;
		}
		return loadedStamp;
	}


	static void PrintBounds(const char* label)
	{
		float low[3] = { 0.0f, 0.0f, 0.0f };
		float high[3] = { 0.0f, 0.0f, 0.0f };
		int live = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			if (rawNodes[i].removed)
			{
				continue;
			}
			for (int axis = 0; axis < 3; ++axis)
			{
				if (live == 0 || rawNodes[i].origin[axis] < low[axis])
				{
					low[axis] = rawNodes[i].origin[axis];
				}
				if (live == 0 || rawNodes[i].origin[axis] > high[axis])
				{
					high[axis] = rawNodes[i].origin[axis];
				}
			}
			++live;
		}
		Report("navgen: %s: %d nodes within %.0f %.0f %.0f .. %.0f %.0f %.0f\n",
					   label, live, low[0], low[1], low[2], high[0], high[1], high[2]);
	}


	static constexpr int spawnConsoleCap = 8;

	static void ReportSpawnCoverage()
	{
		int spawnsCovered = 0;
		int spawnTotal = 0;
		int objectivesCovered = 0;
		int objectiveTotal = 0;
		int reported = 0;
		for (int s = 0; s < seedCount; ++s)
		{
			const bool isObjective = seedIsObjective[s];
			if (isObjective)
			{
				++objectiveTotal;
			}
			else
			{
				++spawnTotal;
			}

			float nearestSq = -1.0f;
			for (int i = 0; i < finalNodeCount; ++i)
			{
				const float dx = finalNodes[i].origin[0] - seedPositions[s][0];
				const float dy = finalNodes[i].origin[1] - seedPositions[s][1];
				const float dz = finalNodes[i].origin[2] - seedPositions[s][2];
				const float distanceSq = dx * dx + dy * dy + dz * dz;
				if (nearestSq < 0.0f || distanceSq < nearestSq)
				{
					nearestSq = distanceSq;
				}
			}

			if (nearestSq >= 0.0f && nearestSq <= seedCoverage * seedCoverage)
			{
				if (isObjective)
				{
					++objectivesCovered;
				}
				else
				{
					++spawnsCovered;
				}
				continue;
			}

			float nearest = -1.0f;
			if (nearestSq >= 0.0f)
			{
				nearest = std::sqrt(nearestSq);
			}
			const char* what = "spawn";
			if (isObjective)
			{
				what = "objective";
			}
			ReportCapped(reported, spawnConsoleCap,
						 "navgen:   no node within %.0f of the %s at %.0f %.0f %.0f (nearest %.0f)\n",
						 seedCoverage, what, seedPositions[s][0], seedPositions[s][1], seedPositions[s][2], nearest);

			int seedNode = -1;
			float seedNearestSq = seedCoverage * seedCoverage;
			for (int i = 0; i < rawCount; ++i)
			{
				const float dx = rawNodes[i].origin[0] - seedPositions[s][0];
				const float dy = rawNodes[i].origin[1] - seedPositions[s][1];
				const float dz = rawNodes[i].origin[2] - seedPositions[s][2];
				const float distanceSq = dx * dx + dy * dy;
				if (rawNodes[i].isSeed && std::fabs(dz) <= 80.0f && distanceSq < seedNearestSq)
				{
					seedNearestSq = distanceSq;
					seedNode = i;
				}
			}
			if (seedNode < 0)
			{
				ReportCapped(reported, spawnConsoleCap,
							 "navgen:     no seed node was placed there (no standing room at the spawn)\n");
			}
			else
			{
				const char* fate = "alive";
				if (rawNodes[seedNode].removed)
				{
					fate = RemoveStageName(rawNodes[seedNode].removedBy);
				}
				ReportCapped(reported, spawnConsoleCap,
							 "navgen:     its seed node had %d link(s) after the fill, now %s\n",
							 rawNodes[seedNode].fillLinks, fate);
			}
			++reported;
		}

		summary.spawnsCovered = spawnsCovered;
		summary.spawnTotal = spawnTotal;
		summary.objectivesCovered = objectivesCovered;
		summary.objectiveTotal = objectiveTotal;
		const char* warning = "";
		if (spawnsCovered != spawnTotal || objectivesCovered != objectiveTotal)
		{
			warning = " -- FIX THIS BEFORE JUDGING THE BOTS";
		}
		Report("navgen: spawn coverage: %d of %d spawns and %d of %d objectives have a node within %.0f units%s\n",
			   spawnsCovered, spawnTotal, objectivesCovered, objectiveTotal, seedCoverage, warning);
	}


	static constexpr float steepGrade = 0.7002f;

	static void ReportLinkCensus()
	{
		int walkRise19 = 0;
		int walkRise37 = 0;
		int walkRiseOver60 = 0;
		int mantleBuckets[5] = {};
		float dropMin = 0.0f;
		float dropMax = 0.0f;
		float jumpMin = 0.0f;
		float jumpMax = 0.0f;

		summary.nodes = finalNodeCount;
		summary.links = finalChildCount;
		for (int i = 0; i < finalNodeCount; ++i)
		{
			const Waypoints::Node& node = finalNodes[i];
			for (int c = 0; c < node.childCount; ++c)
			{
				const unsigned int entry = finalChildren[node.firstChild + c];
				const int child = entry & Waypoints::linkIndexMask;
				const unsigned int flags = entry & ~Waypoints::linkIndexMask;
				const Waypoints::LinkKind kind = Waypoints::KindFromFlags(flags);
				const float rise = finalNodes[child].origin[2] - node.origin[2];
				const float run = Distance2(node.origin, finalNodes[child].origin);
				if (flags & Waypoints::linkCrouch)
				{
					++summary.crouchLinks;
				}
				if (flags & Waypoints::linkGlass)
				{
					++summary.glassLinks;
				}

				if (kind == Waypoints::LinkWalk)
				{
					++summary.walkLinks;
					if (!HasFinalLink(child, i))
					{
						++summary.oneWayWalks;
					}
					if (rise > stepUpMax && rise <= hopRiseMax)
					{
						++walkRise19;
					}
					else if (rise > hopRiseMax && rise <= mantleUpMax)
					{
						++walkRise37;
					}
					else if (rise > mantleUpMax)
					{
						++walkRiseOver60;
					}
					if (run > 1.0f && std::fabs(rise) / run > steepGrade)
					{
						++summary.steepWalks;
					}
				}
				else if (kind == Waypoints::LinkMantle)
				{
					++summary.mantleLinks;
					int bucket = 4;
					if (rise <= 30.0f)
					{
						bucket = 0;
					}
					else if (rise <= 40.0f)
					{
						bucket = 1;
					}
					else if (rise <= 50.0f)
					{
						bucket = 2;
					}
					else if (rise <= mantleUpMax)
					{
						bucket = 3;
					}
					++mantleBuckets[bucket];
					if (rise > mantleUpMax)
					{
						++summary.mantlesOver60;
					}
				}
				else if (kind == Waypoints::LinkDrop)
				{
					const float fall = -rise;
					if (summary.dropLinks == 0 || fall < dropMin)
					{
						dropMin = fall;
					}
					if (summary.dropLinks == 0 || fall > dropMax)
					{
						dropMax = fall;
					}
					++summary.dropLinks;
				}
				else if (kind == Waypoints::LinkLadder)
				{
					++summary.ladderLinks;
				}
				else if (kind == Waypoints::LinkJump)
				{
					if (summary.jumpLinks == 0 || rise < jumpMin)
					{
						jumpMin = rise;
					}
					if (summary.jumpLinks == 0 || rise > jumpMax)
					{
						jumpMax = rise;
					}
					++summary.jumpLinks;
				}
			}
		}

		Report("navgen: links: %d walk (%d one-way, %d crouch, %d glass), %d mantle, %d drop, %d ladder, %d jump\n",
			   summary.walkLinks, summary.oneWayWalks, summary.crouchLinks, summary.glassLinks,
			   summary.mantleLinks, summary.dropLinks, summary.ladderLinks, summary.jumpLinks);
		Report("navgen: walk rises: %d at 19-36, %d at 37-60, %d over 60; %d walk link(s) steeper than 35 degrees\n",
			   walkRise19, walkRise37, walkRiseOver60, summary.steepWalks);
		const char* warning = "";
		if (mantleBuckets[4])
		{
			warning = " (started from a jump: Mantle_CheckLedge again in the air)";
		}
		Report("navgen: mantle rises: %d to 30, %d at 31-40, %d at 41-50, %d at 51-60, %d over 60%s\n",
			   mantleBuckets[0], mantleBuckets[1], mantleBuckets[2], mantleBuckets[3], mantleBuckets[4], warning);
		Report("navgen: drops fall %.0f..%.0f (%d), jumps rise %.0f..%.0f (%d)\n",
			   dropMin, dropMax, summary.dropLinks, jumpMin, jumpMax, summary.jumpLinks);
	}


	static void ReportComponents()
	{
		static int unionParent[maxFinalNodes];
		static int componentSize[maxFinalNodes];
		for (int i = 0; i < finalNodeCount; ++i)
		{
			unionParent[i] = i;
			componentSize[i] = 0;
		}
		for (int i = 0; i < finalNodeCount; ++i)
		{
			for (int c = 0; c < finalNodes[i].childCount; ++c)
			{
				int a = i;
				int b = finalChildren[finalNodes[i].firstChild + c] & Waypoints::linkIndexMask;
				while (unionParent[a] != a)
				{
					a = unionParent[a];
				}
				while (unionParent[b] != b)
				{
					b = unionParent[b];
				}
				if (a != b)
				{
					unionParent[b] = a;
				}
			}
		}
		for (int i = 0; i < finalNodeCount; ++i)
		{
			int root = i;
			while (unionParent[root] != root)
			{
				root = unionParent[root];
			}
			unionParent[i] = root;
			++componentSize[root];
		}

		int components = 0;
		int largest = 0;
		for (int i = 0; i < finalNodeCount; ++i)
		{
			if (unionParent[i] != i)
			{
				continue;
			}
			++components;
			if (componentSize[i] > largest)
			{
				largest = componentSize[i];
			}
		}
		summary.components = components;
		const char* warning = "";
		if (components > 1)
		{
			warning = " -- MAP IS FRAGMENTED, some areas unreachable";
		}
		Report("navgen: %d component(s), largest %d of %d%s\n", components, largest, finalNodeCount, warning);
	}


	static void StampGlassLinks();

	static void GenerateGraph(const char* mapName, const char* reason, unsigned long stamp)
	{
		ResetGraph();
		ResetTraceStats();
		regionCount = 0;

		Report("navgen: generating waypoints for %s (generator rev %d, %s)...\n", mapName, generatorRevision, reason);
		int phaseStart = NowMs();
		SeedGraph();
		summary.msSeed = NowMs() - phaseStart;
		if (rawCount == 0)
		{
			Report("navgen: no seeds found, is a map running?\n");
			return;
		}

		phaseStart = NowMs();
		int expanded = 0;
		int node = 0;
		while (PopFrontier(&node))
		{
			ExpandNode(node);

			if ((++expanded % 500) == 0)
			{
				Report("navgen: %d nodes, %d traces...\n", rawCount, traceCount);
			}
		}
		for (int i = 0; i < rawCount; ++i)
		{
			rawNodes[i].fillLinks = rawNodes[i].linkCount;
		}
		summary.msFill = NowMs() - phaseStart;
		Report("navgen: flood fill done, %d raw nodes, %d traces\n", rawCount, traceCount);
		ReportFillStats();
		if (statNodeCapHits || statLinkCapHits)
		{
			Report("navgen: WARNING caps hit: %d node(s) refused at the raw cap of %d, "
				   "%d link(s) refused at %d per node\n",
				   statNodeCapHits, maxRawNodes, statLinkCapHits, maxNodeLinks);
			for (int i = 0; i < RefusedLinkCount(); ++i)
			{
				float from[3];
				float to[3];
				unsigned char kind = 0;
				RefusedLinkAt(i, from, to, &kind);
				const char* kindName = Waypoints::KindName(kind);
				ReportCapped(i, 8, "navgen:   refused %s %.0f %.0f %.0f -> %.0f %.0f %.0f\n",
							 kindName, from[0], from[1], from[2], to[0], to[1], to[2]);
			}
		}
		Report("navgen: ray checks: %d no-ground, %d start-solid, %d slope-reject, %d no-normal\n",
			   statRayMiss, statRayStartSolid, statRaySlope, statRayZero);
		Report("navgen: mantle probe: %d faces seen, %d links made\n",
			   statMantleFaces, statMantleLinks);
		PrintBounds("raw graph");

		phaseStart = NowMs();
		const int mantleLinksRemoved = TrimMantleShortcuts();
		const int mantleDuplicatesRemoved = TrimMantleDuplicates();
		const int crouchLinksRemoved = TrimCrouchShortcuts();
		const int pocketsRemoved = TrimMantlePockets();
		summary.msPolicy = NowMs() - phaseStart;
		Report("navgen: mantle policy: %d shortcut link(s) dropped, %d duplicate(s) onto one ledge dropped, "
			   "%d pocket node(s) removed; %d crouch shortcut(s) dropped\n",
			   mantleLinksRemoved, mantleDuplicatesRemoved, pocketsRemoved, crouchLinksRemoved);

		phaseStart = NowMs();
		const int pruned = PruneGraph();
		summary.msPrune = NowMs() - phaseStart;
		Report("navgen: pruned %d nodes\n", pruned);
		PrintBounds("final graph");

		phaseStart = NowMs();
		SuppressGlass(false);
		StampGlassLinks();
		if (!BuildFinalGraph())
		{
			Report("navgen: graph did not fit (%d nodes, cap %d), not saved\n",
				   finalNodeCount, maxFinalNodes);
			return;
		}
		ComputeFinalRegions();

		if (!Waypoints::InstallGenerated(finalNodes, finalNodeCount, finalChildren, finalChildCount))
		{
			Report("navgen: install failed\n");
			return;
		}

		static Waypoints::Objective objectives[Waypoints::maxObjectives];
		int objectiveCount = 0;
		for (int i = 0; i < ObjectiveCount() && objectiveCount < Waypoints::maxObjectives; ++i)
		{
			Waypoints::Objective& objective = objectives[objectiveCount++];
			const float* position = ObjectivePosition(i);
			objective.origin[0] = position[0];
			objective.origin[1] = position[1];
			objective.origin[2] = position[2];
			_snprintf_s(objective.name, sizeof(objective.name), _TRUNCATE, "%s", ObjectiveName(i));
			_snprintf_s(objective.gametype, sizeof(objective.gametype), _TRUNCATE, "%s", ObjectiveGametype(i));
			objective.node = -1;
		}
		Waypoints::SetObjectives(objectives, objectiveCount);

		hasLoadedFile = true;
		loadedRevision = generatorRevision;
		loadedStamp = stamp;
		SaveActiveGraph(mapName);
		summary.isSaved = true;
		summary.msBuild = NowMs() - phaseStart;
		Report("navgen: live with %d nodes, %d links\n", finalNodeCount, finalChildCount);

		phaseStart = NowMs();
		ReportSpawnCoverage();
		ReportLinkCensus();
		ReportComponents();
		summary.msReport = NowMs() - phaseStart;
	}


	void ReportBegin(const char* mapName, bool append)
	{
		char path[128];
		_snprintf_s(path, sizeof(path), _TRUNCATE, "players/navgen_%s.log", mapName);

		const char* mode = "a";
		if (!append)
		{
			char previousPath[128];
			_snprintf_s(previousPath, sizeof(previousPath), _TRUNCATE, "%s.prev", path);
			std::remove(previousPath);
			if (std::rename(path, previousPath) == 0)
			{
				Report("navgen: kept the previous %s as .prev\n", path);
			}
			else if (errno != ENOENT)
			{
				Report("navgen: could not keep %s as .prev (errno %d)\n", path, errno);
			}
			mode = "w";
		}
		if (fopen_s(&reportFile, path, mode) != 0)
		{
			reportFile = nullptr;
		}
	}

	void ReportEnd()
	{
		if (reportFile)
		{
			std::fclose(reportFile);
			reportFile = nullptr;
		}
	}


	static void StampGlassLinks()
	{
		if (GlassPieceCount() == 0)
		{
			return;
		}
		static const float bodyHeights[3] = { 24.0f, 40.0f, 62.0f };
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 6.0f, 6.0f, 6.0f };
		int stamped = 0;
		for (int i = 0; i < rawCount; ++i)
		{
			const RawNode& node = rawNodes[i];
			if (node.removed)
			{
				continue;
			}
			for (int c = 0; c < node.linkCount; ++c)
			{
				const int target = node.links[c];
				if (rawNodes[target].removed)
				{
					continue;
				}
				for (const float height : bodyHeights)
				{
					const float start[3] = { node.origin[0], node.origin[1], node.origin[2] + height };
					const float end[3] = { rawNodes[target].origin[0], rawNodes[target].origin[1],
										   rawNodes[target].origin[2] + height };
					if (GlassHitId(start, end, bounds) == 0)
					{
						continue;
					}
					SetLinkFlags(i, target, static_cast<unsigned char>(LinkFlagsOf(i, target) | RawLinkGlass));
					++stamped;
					break;
				}
			}
		}
		Report("navgen: glass: %d link(s) cross an unbroken pane (%d in the map), stamped on the link\n",
			   stamped, GlassPieceCount());
	}


	static constexpr int glassConsoleCap = 60;

	static void ReportGlassCrossings()
	{
		int crossings = 0;
		for (int i = 0; i < finalNodeCount; ++i)
		{
			const Waypoints::Node& node = finalNodes[i];
			for (int c = 0; c < node.childCount; ++c)
			{
				const unsigned int entry = finalChildren[node.firstChild + c];
				if ((entry & Waypoints::linkGlass) == 0)
				{
					continue;
				}
				const int child = entry & Waypoints::linkIndexMask;
				const Waypoints::Node& other = finalNodes[child];
				const char* oneWay = " (one-way)";
				if (HasFinalLink(child, i))
				{
					oneWay = "";
				}
				ReportCapped(crossings, glassConsoleCap,
							 "navgen:   link %d -> %d%s crosses glass near %.0f %.0f %.0f\n",
							 i, child, oneWay, (node.origin[0] + other.origin[0]) * 0.5f,
							 (node.origin[1] + other.origin[1]) * 0.5f, (node.origin[2] + other.origin[2]) * 0.5f);
				++crossings;
			}
		}
		summary.glassCrossings = crossings;
		const char* capNote = "";
		if (crossings > glassConsoleCap)
		{
			capNote = ", the rest in the report file";
		}
		Report("navgen: %d link(s) cross unbroken glass (%d panes in the map)%s\n",
			   crossings, GlassPieceCount(), capNote);
	}


	static constexpr float holeCell   = 80.0f;
	static constexpr float holeReach  = 70.0f;
	static constexpr int   holeMinCells = 2;
	static constexpr int   holeConsoleCap = 12;
	static constexpr int   holeLayers = 4;
	static constexpr float holeLayerDrop = 90.0f;
	static constexpr float holeLevelBand = 100.0f;
	static constexpr float holeGraphReachXy = 300.0f;
	static constexpr float holeGraphReachZ = 150.0f;
	static constexpr int   holeMargin = 2;
	static constexpr int   holeWalkSteps = 10;
	static constexpr int   maxHoles = 64;
	static constexpr int   maxCells = 128;
	static constexpr unsigned char holeIdNone = 255;
	static constexpr unsigned char holeIdDropped = 254;

	enum HoleCellState : unsigned char
	{
		HoleCellNone = 0,
		HoleCellCovered,
		HoleCellHole,
		HoleCellHurt,
	};

	struct Hole
	{
		int cells;
		float x;
		float y;
		float z;
		float firstX;
		float firstY;
		float firstZ;
		int rawByStage[RemoveStageCount];
		bool isAboveGraph;
		bool isInClip;
		bool isBeyondHurt;
	};

	static Hole lastHoles[maxHoles];
	static int lastHoleCount = 0;

	int HoleCount()
	{
		return lastHoleCount;
	}

	bool HoleAt(int index, float outCentre[3], int* outCells, int* outKind)
	{
		if (index < 0 || index >= lastHoleCount)
		{
			return false;
		}
		const Hole& hole = lastHoles[index];
		outCentre[0] = hole.x;
		outCentre[1] = hole.y;
		outCentre[2] = hole.z;
		*outCells = hole.cells;
		*outKind = 0;
		if (hole.isInClip)
		{
			*outKind = 2;
		}
		else if (hole.isAboveGraph)
		{
			*outKind = 1;
		}
		else if (hole.isBeyondHurt)
		{
			*outKind = 3;
		}
		return true;
	}

	static bool HoleWalkFrom(const Hole& hole, const float* start, int label, char* outLine, int outSize, bool* outIsHurt)
	{
		*outLine = 0;
		*outIsHurt = false;

		float cursor[3] = { start[0], start[1], start[2] };
		const float dx = hole.x - cursor[0];
		const float dy = hole.y - cursor[1];
		const float distance = std::sqrt(dx * dx + dy * dy);
		if (distance < 1.0f)
		{
			return false;
		}
		const float dirX = dx / distance;
		const float dirY = dy / distance;
		int steps = static_cast<int>(distance / gridStep) + 1;
		if (steps > holeWalkSteps)
		{
			steps = holeWalkSteps;
		}

		for (int step = 1; step <= steps; ++step)
		{
			float x = std::round((cursor[0] + dirX * gridStep) / gridStep) * gridStep;
			float y = std::round((cursor[1] + dirY * gridStep) / gridStep) * gridStep;
			char verdict[160] = "";
			bool isHurt = false;
			if (probeVerbose)
			{
				Report("navgen:   step %d at %.0f %.0f from floor %.1f:\n", step, x, y, cursor[2]);
			}

			GroundHit ground = SnapToGround(x, y, cursor[2], 80.0f);
			if (!ground.isValid && ground.isRayStartSolid)
			{
				float shiftedX = x;
				float shiftedY = y;
				const GroundHit shifted = SnapBesideSolid(x, y, cursor[2], cursor, &shiftedX, &shiftedY);
				if (shifted.isValid)
				{
					if (probeVerbose)
					{
						Report("navgen:     the line is inside solid, floor taken beside it at %.0f %.0f (floor %.1f)\n",
							   shiftedX, shiftedY, shifted.z);
					}
					x = shiftedX;
					y = shiftedY;
					ground = shifted;
				}
			}
			bool isDrop = false;
			if (!ground.isValid)
			{
				ground = SnapToGround(x, y, cursor[2] - 100.0f, 220.0f);
				isDrop = true;
			}
			if (!ground.isValid)
			{
				const char* why = "no floor from 80 below to 320 below";
				if (ground.isSlope)
				{
					why = "the floor is a slide bank";
				}
				else if (ground.isRayStartSolid)
				{
					why = "inside solid";
				}
				_snprintf_s(verdict, sizeof(verdict), _TRUNCATE, "%s", why);
			}
			else if (!HasPathFooting(x, y, ground.z))
			{
				_snprintf_s(verdict, sizeof(verdict), _TRUNCATE, "no footing at floor %.0f", ground.z);
			}
			else if (!HasShoulderRoom(x, y, ground.z, ground.needsCrouch))
			{
				_snprintf_s(verdict, sizeof(verdict), _TRUNCATE, "no shoulder room at floor %.0f", ground.z);
			}
			else
			{
				const float candidate[3] = { x, y, ground.z };
				if (IsInHurtVolume(candidate))
				{
					_snprintf_s(verdict, sizeof(verdict), _TRUNCATE, "in a hurt volume");
					isHurt = true;
				}
				else
				{
					const float rise = ground.z - cursor[2];
					bool isJoined = !isDrop && (CanWalk(cursor, candidate) || CanWalkCrouched(cursor, candidate));
					if (!isJoined && !isDrop && rise > stepUpMax && rise <= hopRiseMax)
					{
						isJoined = CanHop(cursor, candidate);
					}
					if (!isJoined)
					{
						isJoined = CanDrop(cursor, candidate);
					}
					if (!isJoined)
					{
						isJoined = CanSlideDown(cursor, candidate);
					}
					if (isJoined)
					{
						if (probeVerbose)
						{
							Report("navgen:     joined, floor %.1f\n", ground.z);
						}
						cursor[0] = x;
						cursor[1] = y;
						cursor[2] = ground.z;
						continue;
					}
					_snprintf_s(verdict, sizeof(verdict), _TRUNCATE,
								"neither walkable nor droppable, rise %+.0f to floor %.0f", rise, ground.z);
				}
			}
			if (!isHurt)
			{
				const MantleGeometry geometry = ProbeMantle(cursor, dirX, dirY);
				if (geometry.isValid && geometry.hasFooting)
				{
					if (probeVerbose)
					{
						Report("navgen:     mantled, lands at %.0f %.0f %.1f\n",
							   geometry.landing[0], geometry.landing[1], geometry.landing[2]);
					}
					cursor[0] = geometry.landing[0];
					cursor[1] = geometry.landing[1];
					cursor[2] = geometry.landing[2];
					continue;
				}
				const char* why = geometry.refusal;
				if (geometry.isValid)
				{
					why = "no footing at the landing";
				}
				const size_t used = std::strlen(verdict);
				_snprintf_s(verdict + used, sizeof(verdict) - used, _TRUNCATE, "; mantle: %s", why);
			}
			_snprintf_s(outLine, outSize, _TRUNCATE, "node %d, step %d at %.0f %.0f: %s", label, step, x, y, verdict);
			*outIsHurt = isHurt;
			return false;
		}
		_snprintf_s(outLine, outSize, _TRUNCATE,
					"node %d: every gate passed for %d step(s) toward %.0f %.0f; the hole begins further in",
					label, steps, hole.x, hole.y);
		return true;
	}

	static bool HoleGateWalk(const Hole& hole, char* outLine, int outSize)
	{
		*outLine = 0;
		static const float levelBands[2] = { 60.0f, holeGraphReachZ };
		int starts[4] = { -1, -1, -1, -1 };
		float startSq[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		for (const float band : levelBands)
		{
			for (int i = 0; i < finalNodeCount; ++i)
			{
				if (std::fabs(finalNodes[i].origin[2] - hole.z) > band)
				{
					continue;
				}
				const float dx = finalNodes[i].origin[0] - hole.x;
				const float dy = finalNodes[i].origin[1] - hole.y;
				const float distanceSq = dx * dx + dy * dy;

				int quadrant = 0;
				if (dx < 0.0f)
				{
					quadrant += 1;
				}
				if (dy < 0.0f)
				{
					quadrant += 2;
				}
				if (starts[quadrant] < 0 || distanceSq < startSq[quadrant])
				{
					starts[quadrant] = i;
					startSq[quadrant] = distanceSq;
				}
			}

			int found = 0;
			for (const int start : starts)
			{
				if (start >= 0)
				{
					++found;
				}
			}
			if (found > 0)
			{
				break;
			}
		}

		bool anyHurt = false;
		int walks = 0;
		int used = 0;
		for (int attempt = 0; attempt < 4 && walks < 3; ++attempt)
		{
			int pick = -1;
			for (int q = 0; q < 4; ++q)
			{
				if (starts[q] >= 0 && (pick < 0 || startSq[q] < startSq[pick]))
				{
					pick = q;
				}
			}
			if (pick < 0)
			{
				break;
			}
			const int start = starts[pick];
			starts[pick] = -1;
			++walks;

			bool isHurt = false;
			char line[144] = "";
			if (HoleWalkFrom(hole, finalNodes[start].origin, start, line, sizeof(line), &isHurt))
			{
				_snprintf_s(outLine, outSize, _TRUNCATE, "%s", line);
				return false;
			}
			if (isHurt)
			{
				anyHurt = true;
			}
			if (*line && used + 8 < outSize)
			{
				const char* separator = "";
				if (used > 0)
				{
					separator = "; ";
				}
				const int written = _snprintf_s(outLine + used, static_cast<size_t>(outSize - used),
												_TRUNCATE, "%s%s", separator, line);
				if (written > 0)
				{
					used += written;
				}
			}
		}
		return anyHurt;
	}

	bool MarchFromNode(const float* start, int label, float x, float y, float z, char* outLine, int outSize)
	{
		Hole hole = {};
		hole.x = x;
		hole.y = y;
		hole.z = z;
		bool isHurt = false;
		probeVerbose = true;
		const bool passed = HoleWalkFrom(hole, start, label, outLine, outSize, &isHurt);
		probeVerbose = false;
		return passed;
	}

	static void ReportCoverageHoles()
	{
		const int scanStart = NowMs();
		float low[3] = { 0.0f, 0.0f, 0.0f };
		float high[3] = { 0.0f, 0.0f, 0.0f };
		for (int i = 0; i < finalNodeCount; ++i)
		{
			for (int axis = 0; axis < 3; ++axis)
			{
				if (i == 0 || finalNodes[i].origin[axis] < low[axis])
				{
					low[axis] = finalNodes[i].origin[axis];
				}
				if (i == 0 || finalNodes[i].origin[axis] > high[axis])
				{
					high[axis] = finalNodes[i].origin[axis];
				}
			}
		}
		low[0] -= holeMargin * holeCell;
		low[1] -= holeMargin * holeCell;
		high[0] += holeMargin * holeCell;
		high[1] += holeMargin * holeCell;

		const int cols = static_cast<int>((high[0] - low[0]) / holeCell) + 1;
		const int rows = static_cast<int>((high[1] - low[1]) / holeCell) + 1;
		if (cols > maxCells || rows > maxCells)
		{
			Report("navgen: coverage scan skipped, map too large for the %d-cell grid\n", maxCells);
			lastHoleCount = 0;
			return;
		}

		static unsigned char cellState[maxCells][maxCells][holeLayers];
		static float cellFloor[maxCells][maxCells][holeLayers];
		int holeCells = 0;
		int hurtCells = 0;
		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < cols; ++c)
			{
				const float x = low[0] + (static_cast<float>(c) + 0.5f) * holeCell;
				const float y = low[1] + (static_cast<float>(r) + 0.5f) * holeCell;
				float rayFrom = high[2] + 120.0f;
				for (int layer = 0; layer < holeLayers; ++layer)
				{
					cellState[r][c][layer] = HoleCellNone;
					cellFloor[r][c][layer] = 0.0f;
					const float depth = rayFrom - (low[2] - 120.0f);
					if (depth <= 0.0f)
					{
						break;
					}
					float floorZ = 0.0f;
					float normalZ = 0.0f;
					if (!GroundRay(x, y, rayFrom, depth, &floorZ, &normalZ))
					{
						break;
					}

					static const float layerDrops[4] = { 12.0f, 30.0f, 60.0f, holeLayerDrop };
					rayFrom = floorZ - holeLayerDrop;
					for (const float drop : layerDrops)
					{
						if (!IsInsideSolid(x, y, floorZ - drop))
						{
							rayFrom = floorZ - drop;
							break;
						}
					}

					const float point[3] = { x, y, floorZ };
					if (IsInHurtVolume(point))
					{
						cellState[r][c][layer] = HoleCellHurt;
						++hurtCells;
						continue;
					}
					const GroundHit ground = SnapToGround(x, y, floorZ, 8.0f);
					if (!ground.isValid || !HasPathFooting(x, y, ground.z))
					{
						continue;
					}

					bool isCovered = false;
					for (int i = 0; i < finalNodeCount && !isCovered; ++i)
					{
						const float dx = finalNodes[i].origin[0] - x;
						const float dy = finalNodes[i].origin[1] - y;
						const float dz = finalNodes[i].origin[2] - ground.z;
						isCovered = dx * dx + dy * dy + dz * dz <= holeReach * holeReach;
					}
					cellFloor[r][c][layer] = ground.z;
					if (isCovered)
					{
						cellState[r][c][layer] = HoleCellCovered;
					}
					else
					{
						cellState[r][c][layer] = HoleCellHole;
						++holeCells;
					}
				}
			}
		}

		static unsigned char holeIdOf[maxCells][maxCells][holeLayers];
		static int stack[maxCells * maxCells * holeLayers];
		static int members[maxCells * maxCells * holeLayers];
		Hole* holes = lastHoles;
		int holeCount = 0;
		int droppedHoles = 0;
		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < cols; ++c)
			{
				for (int layer = 0; layer < holeLayers; ++layer)
				{
					holeIdOf[r][c][layer] = holeIdNone;
				}
			}
		}
		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < cols; ++c)
			{
				for (int layer = 0; layer < holeLayers; ++layer)
				{
					if (cellState[r][c][layer] != HoleCellHole || holeIdOf[r][c][layer] != holeIdNone)
					{
						continue;
					}

					const unsigned char id = static_cast<unsigned char>(holeCount < maxHoles ? holeCount : holeIdDropped);
					int count = 0;
					float sumX = 0.0f;
					float sumY = 0.0f;
					float sumZ = 0.0f;
					int top = 0;
					stack[top++] = (r * maxCells + c) * holeLayers + layer;
					holeIdOf[r][c][layer] = id;
					while (top > 0)
					{
						const int cell = stack[--top];
						const int cl = cell % holeLayers;
						const int cc = (cell / holeLayers) % maxCells;
						const int cr = (cell / holeLayers) / maxCells;
						members[count++] = cell;
						sumX += low[0] + (static_cast<float>(cc) + 0.5f) * holeCell;
						sumY += low[1] + (static_cast<float>(cr) + 0.5f) * holeCell;
						sumZ += cellFloor[cr][cc][cl];

						static const int stepOffsets[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
						for (const int* step : stepOffsets)
						{
							const int nr = cr + step[0];
							const int nc = cc + step[1];
							if (nr < 0 || nc < 0 || nr >= rows || nc >= cols)
							{
								continue;
							}
							for (int nl = 0; nl < holeLayers; ++nl)
							{
								if (cellState[nr][nc][nl] != HoleCellHole || holeIdOf[nr][nc][nl] != holeIdNone
									|| std::fabs(cellFloor[nr][nc][nl] - cellFloor[cr][cc][cl]) > holeLevelBand)
								{
									continue;
								}
								holeIdOf[nr][nc][nl] = id;
								stack[top++] = (nr * maxCells + nc) * holeLayers + nl;
							}
						}
					}

					if (count < holeMinCells || holeCount >= maxHoles)
					{
						if (count >= holeMinCells)
						{
							++droppedHoles;
						}
						for (int m = 0; m < count; ++m)
						{
							const int cell = members[m];
							holeIdOf[(cell / holeLayers) / maxCells][(cell / holeLayers) % maxCells][cell % holeLayers] = holeIdDropped;
						}
						continue;
					}

					Hole& hole = holes[holeCount++];
					hole = {};
					hole.cells = count;
					hole.x = sumX / static_cast<float>(count);
					hole.y = sumY / static_cast<float>(count);
					hole.z = sumZ / static_cast<float>(count);
					hole.firstX = low[0] + (static_cast<float>(c) + 0.5f) * holeCell;
					hole.firstY = low[1] + (static_cast<float>(r) + 0.5f) * holeCell;
					hole.firstZ = cellFloor[r][c][layer];
				}
			}
		}

		for (int i = 0; i < rawCount; ++i)
		{
			const int c = static_cast<int>((rawNodes[i].origin[0] - low[0]) / holeCell);
			const int r = static_cast<int>((rawNodes[i].origin[1] - low[1]) / holeCell);
			if (c < 0 || r < 0 || c >= cols || r >= rows)
			{
				continue;
			}
			for (int layer = 0; layer < holeLayers; ++layer)
			{
				const unsigned char id = holeIdOf[r][c][layer];
				if (id >= maxHoles || std::fabs(rawNodes[i].origin[2] - cellFloor[r][c][layer]) > holeLevelBand)
				{
					continue;
				}
				unsigned char stage = RemovedNever;
				if (rawNodes[i].removed && rawNodes[i].removedBy < RemoveStageCount)
				{
					stage = rawNodes[i].removedBy;
				}
				++holes[id].rawByStage[stage];
			}
		}

		int aboveGraph = 0;
		int inClip = 0;
		for (int h = 0; h < holeCount; ++h)
		{
			Hole& hole = holes[h];
			hole.isAboveGraph = true;
			for (int i = 0; i < finalNodeCount && hole.isAboveGraph; ++i)
			{
				const float dx = finalNodes[i].origin[0] - hole.x;
				const float dy = finalNodes[i].origin[1] - hole.y;
				if (std::fabs(finalNodes[i].origin[2] - hole.z) <= holeGraphReachZ
					&& dx * dx + dy * dy <= holeGraphReachXy * holeGraphReachXy)
				{
					hole.isAboveGraph = false;
				}
			}
			hole.isInClip = IsInsidePlayerClip(hole.firstX, hole.firstY, hole.firstZ + 40.0f);
			if (hole.isAboveGraph)
			{
				++aboveGraph;
			}
			if (hole.isInClip)
			{
				++inClip;
			}
		}

		for (int i = 1; i < holeCount; ++i)
		{
			for (int j = i; j > 0 && holes[j].cells > holes[j - 1].cells; --j)
			{
				const Hole swap = holes[j];
				holes[j] = holes[j - 1];
				holes[j - 1] = swap;
			}
		}
		const int scanMs = NowMs() - scanStart;

		const int walkStart = NowMs();
		int beyondHurt = 0;
		for (int i = 0; i < holeCount; ++i)
		{
			int rawTotal = 0;
			for (int stage = 0; stage < RemoveStageCount; ++stage)
			{
				rawTotal += holes[i].rawByStage[stage];
			}
			char postMortem[320] = "";
			if (rawTotal == 0 && !holes[i].isInClip && !holes[i].isAboveGraph)
			{
				holes[i].isBeyondHurt = HoleGateWalk(holes[i], postMortem, sizeof(postMortem));
				if (holes[i].isBeyondHurt)
				{
					++beyondHurt;
				}
			}

			const char* kind = "at graph level";
			if (holes[i].isInClip)
			{
				kind = "in player clip";
			}
			else if (holes[i].isAboveGraph)
			{
				kind = "above the graph";
			}
			else if (holes[i].isBeyondHurt)
			{
				kind = "beyond a hurt volume";
			}
			ReportCapped(i, holeConsoleCap, "navgen:   hole of ~%d x %d units around %.0f %.0f, floor %.0f, %s\n",
						 static_cast<int>(holeCell), holes[i].cells * static_cast<int>(holeCell),
						 holes[i].x, holes[i].y, holes[i].z, kind);
			if (rawTotal == 0)
			{
				ReportCapped(i, holeConsoleCap, "navgen:     the fill never placed a node in it\n");
				if (*postMortem)
				{
					ReportCapped(i, holeConsoleCap, "navgen:     post-mortem: %s\n", postMortem);
				}
				continue;
			}
			char breakdown[256] = "";
			int used = 0;
			for (int stage = 0; stage < RemoveStageCount; ++stage)
			{
				if (holes[i].rawByStage[stage] == 0)
				{
					continue;
				}
				used += snprintf(breakdown + used, sizeof(breakdown) - used, "%s%d %s",
								 used ? ", " : "", holes[i].rawByStage[stage],
								 RemoveStageName(static_cast<unsigned char>(stage)));
				if (used >= static_cast<int>(sizeof(breakdown)))
				{
					break;
				}
			}
			ReportCapped(i, holeConsoleCap, "navgen:     %d raw node(s) were placed in it: %s\n", rawTotal, breakdown);
		}
		lastHoleCount = holeCount;
		summary.holes = holeCount;
		summary.holeCells = holeCells;
		summary.holesAboveGraph = aboveGraph;
		summary.holesInClip = inClip;
		summary.hurtCells = hurtCells;
		Report("navgen: coverage: %d standable cell(s) of %d units with no node within %.0f, in %d hole(s) of %d+ cells "
			   "(%d above the graph, %d in player clip, %d beyond a hurt volume, %d at graph level%s); "
			   "%d cell(s) in hurt volumes skipped; scan %d ms, post-mortems %d ms\n",
			   holeCells, static_cast<int>(holeCell), holeReach, holeCount, holeMinCells,
			   aboveGraph, inClip, beyondHurt, holeCount - aboveGraph - inClip - beyondHurt,
			   droppedHoles ? ", more past the 64 kept" : "", hurtCells, scanMs, NowMs() - walkStart);
	}


	static void WriteSummaryLine(const char* mapName, unsigned long stamp)
	{
		FILE* file = nullptr;
		char summaryPath[MAX_PATH * 2];
		if (!BotsPath("navgen_summary.log", summaryPath, sizeof(summaryPath)))
		{
			return;
		}
		if (fopen_s(&file, summaryPath, "a") != 0 || !file)
		{
			return;
		}

		char made[32];
		StampText(stamp, made, sizeof(made));
		const char* saved = "";
		if (!summary.isSaved)
		{
			saved = " NOT SAVED";
		}
		std::fprintf(file,
					 "%s %s rev %d: %d nodes, %d links (%d walk, %d one-way, %d crouch, %d glass, %d mantle, %d drop, "
					 "%d ladder, %d jump), %d steep walk(s), %d mantle(s) over 60, %d component(s), %d region(s), "
					 "%d stranded spawn(s), spawns %d/%d, objectives %d/%d, holes %d (%d cells, %d above the graph, "
					 "%d in clip), glass crossings %d, %d ms%s\n",
					 made, mapName, generatorRevision, summary.nodes, summary.links, summary.walkLinks,
					 summary.oneWayWalks, summary.crouchLinks, summary.glassLinks, summary.mantleLinks,
					 summary.dropLinks, summary.ladderLinks, summary.jumpLinks, summary.steepWalks,
					 summary.mantlesOver60, summary.components, regionCount, summary.strandedSpawns,
					 summary.spawnsCovered, summary.spawnTotal, summary.objectivesCovered, summary.objectiveTotal,
					 summary.holes, summary.holeCells, summary.holesAboveGraph, summary.holesInClip,
					 summary.glassCrossings, summary.msTotal, saved);
		std::fclose(file);
	}


	void Generate(const char* mapName, const char* reason)
	{
		summary = {};
		const int startMs = NowMs();
		const unsigned long stamp = static_cast<unsigned long>(std::time(nullptr));

		ReportBegin(mapName, false);
		GenerateGraph(mapName, reason, stamp);

		SuppressDynamicSolids(false);
		SuppressGlass(false);

		if (summary.isSaved)
		{
			int phaseStart = NowMs();
			ReportGlassCrossings();
			summary.msGlass = NowMs() - phaseStart;

			phaseStart = NowMs();
			ReportCoverageHoles();
			summary.msHoles = NowMs() - phaseStart;

			phaseStart = NowMs();
			AnnotateGraph(mapName);
			summary.msAnnotate = NowMs() - phaseStart;
		}
		summary.msTotal = NowMs() - startMs;
		Report("navgen: timing: seed %d ms, fill %d, mantle policy %d, prune %d, build and save %d, "
			   "report %d, glass %d, holes %d, walls and sight %d; %d ms in all\n",
			   summary.msSeed, summary.msFill, summary.msPolicy, summary.msPrune, summary.msBuild,
			   summary.msReport, summary.msGlass, summary.msHoles, summary.msAnnotate, summary.msTotal);

		WriteSummaryLine(mapName, stamp);
		ReportEnd();
		Print("navgen: report saved to players/navgen_%s.log\n", mapName);
	}
}
