#include "Components/Modules/BotAI/Navgen/Internal.hpp"

#include <Windows.h>
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Components::BotAI::Navgen
{
	static constexpr float wallProbeUnits = Waypoints::wallReachUnit * Waypoints::wallReachOpen;
	static constexpr float wallShareProbeUnits = 96.0f;
	static constexpr float wallBackUnits = 48.0f;
	static constexpr int wallPenetrateType = 3;
	static constexpr float wallDepthScale = 2.0f;
	static constexpr float areaRadiusStart = 128.0f;
	static constexpr float areaRadiusGrowth = 1.25f;
	static constexpr float areaRiseMax = 48.0f;
	static constexpr int areaTarget = 2048;
	static constexpr int areaAttempts = 10;
	static constexpr float fenceSightUnits = 500.0f;
	static constexpr unsigned short noArea = 0xFFFF;
	static constexpr float fullTurnRad = 6.2831853f;
	static constexpr float wallbangPairUnits = 700.0f;
	static constexpr float wallbangRiseUnits = 160.0f;
	static constexpr float wallbangChestRise = 40.0f;
	static constexpr int wallbangDrawnShareByte = 253;

	struct AnnotationHeader
	{
		char magic[4];
		int revision;
		int graphRevision;
		unsigned int graphStamp;
		int nodeCount;
		int areaCount;
	};

	struct AnnotationData
	{
		std::vector<Waypoints::NodeWalls> walls;
		std::vector<unsigned short> nodeAreas;
		std::vector<Waypoints::AreaInfo> areas;
		std::vector<unsigned char> vis;
		std::vector<Waypoints::AreaWallbang> wallbangs;
	};

	static float SightReachFraction(const float* from, const float* to)
	{
		unsigned char trace[128] = {};
		const int ignore[4] = { -1, -1, 0, 0 };
		const float bounds[6] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
		++traceCount;
		SV_Trace(trace, from, to, bounds, ignore, maskSightThroughGlass, 0, nullptr, 1);
		float fraction = *reinterpret_cast<const float*>(trace + traceFraction);
		if (trace[traceStartSolid] != 0)
		{
			fraction = 0.0f;
		}
		const float renderFraction = RenderBlockedFraction(from, to);
		if (renderFraction < fraction)
		{
			fraction = renderFraction;
		}
		return fraction;
	}

	static bool AreaSight(const float* from, const float* to)
	{
		traceCount += 1;
		if (G_LocationalTracePassed(from, to, entityNumNone, entityNumNone, maskSightThroughGlass, nullptr) == 0)
		{
			return false;
		}
		if (IsRenderBlocked(from, to))
		{
			return false;
		}
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float dz = to[2] - from[2];
		if (dx * dx + dy * dy + dz * dz < fenceSightUnits * fenceSightUnits)
		{
			return true;
		}
		traceCount += 1;
		return G_LocationalTracePassed(from, to, entityNumNone, entityNumNone, contentsMissileClip, nullptr) != 0;
	}

	static void ProbeWallLevel(const float* eye, unsigned char* reach, unsigned char* share)
	{
		for (int direction = 0; direction < Waypoints::wallDirections; ++direction)
		{
			const float yaw = fullTurnRad * static_cast<float>(direction) / static_cast<float>(Waypoints::wallDirections);
			const float dirX = std::cos(yaw);
			const float dirY = std::sin(yaw);
			const float end[3] = { eye[0] + dirX * wallProbeUnits, eye[1] + dirY * wallProbeUnits, eye[2] };
			const float fraction = SightReachFraction(eye, end);
			if (fraction >= 1.0f)
			{
				reach[direction] = Waypoints::wallReachOpen;
				share[direction] = Waypoints::wallShareFull;
				continue;
			}
			const float units = fraction * wallProbeUnits;
			int reachSteps = static_cast<int>(units / Waypoints::wallReachUnit);
			if (reachSteps > Waypoints::wallReachOpen - 1)
			{
				reachSteps = Waypoints::wallReachOpen - 1;
			}
			reach[direction] = static_cast<unsigned char>(reachSteps);
			if (units > wallShareProbeUnits)
			{
				share[direction] = Waypoints::wallShareFull;
				continue;
			}
			const float beyond[3] = { eye[0] + dirX * (units + wallBackUnits), eye[1] + dirY * (units + wallBackUnits), eye[2] };
			float passed = PenetrationShare(beyond, eye, wallPenetrateType, wallDepthScale);
			if (passed < 0.0f)
			{
				passed = 0.0f;
			}
			if (passed > 1.0f)
			{
				passed = 1.0f;
			}
			share[direction] = static_cast<unsigned char>(passed * static_cast<float>(Waypoints::wallShareFull - 1) + 0.5f);
		}
	}

	static void ProbeWalls(int node, Waypoints::NodeWalls* out)
	{
		const float* origin = Waypoints::Origin(node);
		const float standEye[3] = { origin[0], origin[1], origin[2] + Waypoints::standEyeRise };
		const float crouchEye[3] = { origin[0], origin[1], origin[2] + Waypoints::crouchEyeRise };
		ProbeWallLevel(standEye, out->standReach, out->standShare);
		ProbeWallLevel(crouchEye, out->crouchReach, out->crouchShare);
	}

	static float Distance2Of(const float* a, const float* b)
	{
		const float dx = b[0] - a[0];
		const float dy = b[1] - a[1];
		return std::sqrt(dx * dx + dy * dy);
	}

	static float DistanceSq3Of(const float* a, const float* b)
	{
		const float dx = b[0] - a[0];
		const float dy = b[1] - a[1];
		const float dz = b[2] - a[2];
		return dx * dx + dy * dy + dz * dz;
	}

	static bool GrowAreas(float radius, std::vector<unsigned short>& nodeAreas, std::vector<Waypoints::AreaInfo>& areas)
	{
		const int count = Waypoints::Count();
		nodeAreas.assign(count, noArea);
		areas.clear();
		std::vector<int> frontier;
		frontier.reserve(1024);
		for (int seed = 0; seed < count; ++seed)
		{
			if (nodeAreas[seed] != noArea || Waypoints::ChildCount(seed) == 0)
			{
				continue;
			}
			if (static_cast<int>(areas.size()) >= Waypoints::maxAreas)
			{
				return false;
			}
			const unsigned short area = static_cast<unsigned short>(areas.size());
			const float* seedOrigin = Waypoints::Origin(seed);
			frontier.clear();
			frontier.push_back(seed);
			nodeAreas[seed] = area;
			for (std::size_t head = 0; head < frontier.size(); ++head)
			{
				const int node = frontier[head];
				for (int c = 0; c < Waypoints::ChildCount(node); ++c)
				{
					const int child = Waypoints::ChildAt(node, c);
					if (nodeAreas[child] != noArea || Waypoints::ChildCount(child) == 0)
					{
						continue;
					}
					if (Waypoints::KindOf(node, child) != Waypoints::LinkWalk)
					{
						continue;
					}
					const float* childOrigin = Waypoints::Origin(child);
					if (Distance2Of(seedOrigin, childOrigin) > radius || std::fabs(childOrigin[2] - seedOrigin[2]) > areaRiseMax)
					{
						continue;
					}
					nodeAreas[child] = area;
					frontier.push_back(child);
				}
			}
			Waypoints::AreaInfo info = {};
			info.node = seed;
			info.farNode = seed;
			info.memberCount = static_cast<int>(frontier.size());
			areas.push_back(info);
		}
		return static_cast<int>(areas.size()) <= areaTarget;
	}

	static void PickAreaSamples(const std::vector<unsigned short>& nodeAreas, std::vector<Waypoints::AreaInfo>& areas)
	{
		const int count = Waypoints::Count();
		const int areaCount = static_cast<int>(areas.size());
		std::vector<float> sums(static_cast<std::size_t>(areaCount) * 3, 0.0f);
		for (int n = 0; n < count; ++n)
		{
			const int area = nodeAreas[n];
			if (area >= areaCount)
			{
				continue;
			}
			const float* origin = Waypoints::Origin(n);
			for (int k = 0; k < 3; ++k)
			{
				sums[area * 3 + k] += origin[k];
			}
		}
		std::vector<float> bestSq(areaCount, 1.0e30f);
		for (int n = 0; n < count; ++n)
		{
			const int area = nodeAreas[n];
			if (area >= areaCount)
			{
				continue;
			}
			const float members = static_cast<float>(areas[area].memberCount);
			const float mean[3] = { sums[area * 3] / members, sums[area * 3 + 1] / members, sums[area * 3 + 2] / members };
			const float distanceSq = DistanceSq3Of(mean, Waypoints::Origin(n));
			if (distanceSq < bestSq[area])
			{
				bestSq[area] = distanceSq;
				areas[area].node = n;
			}
		}
		std::vector<float> farSq(areaCount, -1.0f);
		for (int n = 0; n < count; ++n)
		{
			const int area = nodeAreas[n];
			if (area >= areaCount)
			{
				continue;
			}
			const float distanceSq = DistanceSq3Of(Waypoints::Origin(areas[area].node), Waypoints::Origin(n));
			if (distanceSq > farSq[area])
			{
				farSq[area] = distanceSq;
				areas[area].farNode = n;
			}
		}
	}

	static void EyeOf(int node, float out[3])
	{
		const float* origin = Waypoints::Origin(node);
		out[0] = origin[0];
		out[1] = origin[1];
		out[2] = origin[2] + Waypoints::standEyeRise;
	}

	static long long BuildVisibility(const std::vector<Waypoints::AreaInfo>& areas, std::vector<unsigned char>& vis)
	{
		const int areaCount = static_cast<int>(areas.size());
		vis.assign(Waypoints::AreaVisBytes(areaCount), 0);
		std::vector<float> eyes(static_cast<std::size_t>(areaCount) * 6);
		for (int a = 0; a < areaCount; ++a)
		{
			EyeOf(areas[a].node, &eyes[a * 6]);
			EyeOf(areas[a].farNode, &eyes[a * 6 + 3]);
		}

		long long bit = 0;
		long long visibleCount = 0;
		int reportedTenth = 0;
		for (int a = 0; a < areaCount; ++a)
		{
			const float* nearA = &eyes[a * 6];
			const float* farA = &eyes[a * 6 + 3];
			const bool hasFarA = areas[a].farNode != areas[a].node;
			for (int b = a + 1; b < areaCount; ++b, ++bit)
			{
				const float* nearB = &eyes[b * 6];
				const float* farB = &eyes[b * 6 + 3];
				bool isVisible = AreaSight(nearA, nearB);
				if (!isVisible && (hasFarA || areas[b].farNode != areas[b].node))
				{
					isVisible = AreaSight(farA, farB);
				}
				if (isVisible)
				{
					vis[bit >> 3] = static_cast<unsigned char>(vis[bit >> 3] | (1 << (bit & 7)));
					++visibleCount;
				}
			}
			const int tenth = static_cast<int>((static_cast<long long>(a + 1) * 10) / areaCount);
			if (tenth > reportedTenth)
			{
				reportedTenth = tenth;
				Report("navgen: area visibility %d%%, %d traces...\n", tenth * 10, traceCount);
			}
		}
		return visibleCount;
	}

	static int BuildWallbangs(const std::vector<Waypoints::AreaInfo>& areas, std::vector<Waypoints::AreaWallbang>& out)
	{
		out.clear();
		const int areaCount = static_cast<int>(areas.size());
		const float pairSq = wallbangPairUnits * wallbangPairUnits;
		int probed = 0;
		int reportedTenth = 0;
		for (int a = 0; a < areaCount && static_cast<int>(out.size()) < Waypoints::maxWallbangPairs; ++a)
		{
			float eyeA[3];
			EyeOf(areas[a].node, eyeA);
			const float feetZ = Waypoints::Origin(areas[a].node)[2];
			for (int b = a + 1; b < areaCount && static_cast<int>(out.size()) < Waypoints::maxWallbangPairs; ++b)
			{
				const float* originB = Waypoints::Origin(areas[b].node);
				if (std::fabs(originB[2] - feetZ) > wallbangRiseUnits)
				{
					continue;
				}
				const float dx = originB[0] - eyeA[0];
				const float dy = originB[1] - eyeA[1];
				if (dx * dx + dy * dy > pairSq || Waypoints::AreasSee(a, b))
				{
					continue;
				}
				const float chestB[3] = { originB[0], originB[1], originB[2] + wallbangChestRise };
				++probed;
				const int shareByte = static_cast<int>(PenetrationShare(eyeA, chestB, wallPenetrateType, wallDepthScale)
					* static_cast<float>(Waypoints::wallShareFull) + 0.5f);
				if (shareByte <= 0 || shareByte >= wallbangDrawnShareByte)
				{
					continue;
				}
				Waypoints::AreaWallbang pair = {};
				pair.low = static_cast<unsigned short>(a);
				pair.high = static_cast<unsigned short>(b);
				pair.share = static_cast<unsigned char>(shareByte);
				if (shareByte > Waypoints::wallShareFull)
				{
					pair.share = static_cast<unsigned char>(Waypoints::wallShareFull);
				}
				out.push_back(pair);
			}
			const int tenth = static_cast<int>((static_cast<long long>(a + 1) * 10) / areaCount);
			if (tenth > reportedTenth)
			{
				reportedTenth = tenth;
				Report("navgen: wallbangs %d%%, %d pairs probed, %d kept...\n", tenth * 10, probed,
					   static_cast<int>(out.size()));
			}
		}
		return probed;
	}

	static bool InstallSolidWallbangs(std::vector<Waypoints::AreaWallbang>& pairs)
	{
		std::size_t kept = 0;
		for (std::size_t k = 0; k < pairs.size(); ++k)
		{
			if (pairs[k].share < wallbangDrawnShareByte)
			{
				pairs[kept] = pairs[k];
				++kept;
			}
		}
		pairs.resize(kept);
		return Waypoints::InstallWallbangs(pairs.data(), static_cast<int>(pairs.size()));
	}

	static void FileNameForAnnotations(const char* mapName, char* out, int outSize)
	{
		_snprintf_s(out, outSize, _TRUNCATE, "waypoints_%s.kwv", mapName);
	}

	static void SaveAnnotations(const char* mapName, const AnnotationData& data)
	{
		char fileName[96];
		FileNameForAnnotations(mapName, fileName, sizeof(fileName));
		char savePath[MAX_PATH * 2];
		if (!BotsPath(fileName, savePath, sizeof(savePath)))
		{
			Report("navgen: no main\\zw3\\bots folder for %s\n", fileName);
			return;
		}
		FILE* file = nullptr;
		if (fopen_s(&file, savePath, "wb") != 0 || !file)
		{
			Report("navgen: cannot open %s for writing\n", savePath);
			return;
		}
		AnnotationHeader header = {};
		header.magic[0] = 'K';
		header.magic[1] = 'W';
		header.magic[2] = 'V';
		header.magic[3] = '1';
		header.revision = annotationRevision;
		header.graphRevision = LoadedRevision();
		header.graphStamp = static_cast<unsigned int>(LoadedStamp());
		header.nodeCount = static_cast<int>(data.walls.size());
		header.areaCount = static_cast<int>(data.areas.size());
		std::fwrite(&header, sizeof(header), 1, file);
		std::fwrite(data.walls.data(), sizeof(Waypoints::NodeWalls), data.walls.size(), file);
		std::fwrite(data.nodeAreas.data(), sizeof(unsigned short), data.nodeAreas.size(), file);
		if (!data.areas.empty())
		{
			std::fwrite(data.areas.data(), sizeof(Waypoints::AreaInfo), data.areas.size(), file);
			std::fwrite(data.vis.data(), 1, data.vis.size(), file);
		}
		const int wallbangCount = static_cast<int>(data.wallbangs.size());
		std::fwrite(&wallbangCount, sizeof(wallbangCount), 1, file);
		if (wallbangCount > 0)
		{
			std::fwrite(data.wallbangs.data(), sizeof(Waypoints::AreaWallbang), data.wallbangs.size(), file);
		}
		std::fclose(file);
		Report("navgen: saved %s (%d nodes, %d areas, %d wallbang pairs)\n", savePath, header.nodeCount, header.areaCount,
			   wallbangCount);
	}

	bool TryLoadAnnotations(const char* mapName)
	{
		Waypoints::ClearAnnotations();
		if (!mapName || !*mapName || !Waypoints::IsLoaded() || LoadedStamp() == 0)
		{
			return false;
		}
		char fileName[96];
		FileNameForAnnotations(mapName, fileName, sizeof(fileName));
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

		AnnotationHeader header = {};
		const bool hasHeader = std::fread(&header, sizeof(header), 1, file) == 1;
		const bool isKnownRevision = header.revision == annotationRevision || header.revision == annotationRevisionNoWallbangs;
		const bool isMatch = hasHeader && std::memcmp(header.magic, "KWV1", 4) == 0 && isKnownRevision
			&& header.graphRevision == LoadedRevision() && header.graphStamp == static_cast<unsigned int>(LoadedStamp())
			&& header.nodeCount == Waypoints::Count() && header.areaCount >= 0 && header.areaCount <= Waypoints::maxAreas;
		if (!isMatch)
		{
			std::fclose(file);
			Report("navgen: %s does not match the graph in use, rebuilding it\n", fileName);
			return false;
		}

		AnnotationData data;
		data.walls.resize(header.nodeCount);
		data.nodeAreas.resize(header.nodeCount);
		data.areas.resize(header.areaCount);
		data.vis.resize(Waypoints::AreaVisBytes(header.areaCount));
		bool isRead = std::fread(data.walls.data(), sizeof(Waypoints::NodeWalls), data.walls.size(), file) == data.walls.size()
			&& std::fread(data.nodeAreas.data(), sizeof(unsigned short), data.nodeAreas.size(), file) == data.nodeAreas.size();
		if (isRead && header.areaCount > 0)
		{
			isRead = std::fread(data.areas.data(), sizeof(Waypoints::AreaInfo), data.areas.size(), file) == data.areas.size()
				&& std::fread(data.vis.data(), 1, data.vis.size(), file) == data.vis.size();
		}
		const bool hasWallbangs = header.revision == annotationRevision;
		if (isRead && hasWallbangs)
		{
			int wallbangCount = 0;
			isRead = std::fread(&wallbangCount, sizeof(wallbangCount), 1, file) == 1 && wallbangCount >= 0
				&& wallbangCount <= Waypoints::maxWallbangPairs;
			if (isRead)
			{
				data.wallbangs.resize(wallbangCount);
				isRead = std::fread(data.wallbangs.data(), sizeof(Waypoints::AreaWallbang), data.wallbangs.size(), file)
					== data.wallbangs.size();
			}
		}
		std::fclose(file);
		if (!isRead)
		{
			Report("navgen: %s is cut short, rebuilding it\n", fileName);
			return false;
		}
		for (const Waypoints::AreaInfo& info : data.areas)
		{
			if (info.node < 0 || info.node >= header.nodeCount || info.farNode < 0 || info.farNode >= header.nodeCount)
			{
				Report("navgen: %s names a node outside the graph, rebuilding it\n", fileName);
				return false;
			}
		}
		if (!Waypoints::InstallAnnotations(data.walls.data(), data.nodeAreas.data(), header.nodeCount, data.areas.data(),
										   header.areaCount, data.vis.data()))
		{
			return false;
		}

		if (!hasWallbangs)
		{
			const int startMs = NowMs();
			const int startTraces = traceCount;
			const int probed = BuildWallbangs(data.areas, data.wallbangs);
			Waypoints::InstallWallbangs(data.wallbangs.data(), static_cast<int>(data.wallbangs.size()));
			SaveAnnotations(mapName, data);
			Report("navgen: %s had no wallbang table: %d pairs probed, %d kept, %d ms, %d traces\n", fileName, probed,
				   static_cast<int>(data.wallbangs.size()), NowMs() - startMs, traceCount - startTraces);
		}
		else if (!InstallSolidWallbangs(data.wallbangs))
		{
			Report("navgen: %s has a wallbang table out of order, rebuilding it\n", fileName);
			return false;
		}
		Report("navgen: loaded %s: walls for %d nodes, %d areas, %d wallbang pairs\n", fileName, header.nodeCount,
			   header.areaCount, Waypoints::WallbangCount());
		return true;
	}

	void AnnotateGraph(const char* mapName)
	{
		const int count = Waypoints::Count();
		if (count <= 0 || !mapName || !*mapName)
		{
			return;
		}
		RefreshSightBlockers(mapName);

		const int startMs = NowMs();
		const int startTraces = traceCount;
		AnnotationData data;
		data.walls.resize(count);
		for (int n = 0; n < count; ++n)
		{
			if (Waypoints::ChildCount(n) == 0)
			{
				std::memset(&data.walls[n], Waypoints::wallReachOpen, sizeof(Waypoints::NodeWalls));
				continue;
			}
			ProbeWalls(n, &data.walls[n]);
			if ((n + 1) % 2000 == 0)
			{
				Report("navgen: walls %d of %d nodes, %d traces...\n", n + 1, count, traceCount);
			}
		}
		const int wallsMs = NowMs() - startMs;

		const int areaStartMs = NowMs();
		float radius = areaRadiusStart;
		for (int attempt = 0; attempt < areaAttempts; ++attempt)
		{
			if (GrowAreas(radius, data.nodeAreas, data.areas))
			{
				break;
			}
			radius *= areaRadiusGrowth;
		}
		if (static_cast<int>(data.areas.size()) > Waypoints::maxAreas)
		{
			data.areas.resize(Waypoints::maxAreas);
		}
		PickAreaSamples(data.nodeAreas, data.areas);
		const long long visibleCount = BuildVisibility(data.areas, data.vis);
		const int areasMs = NowMs() - areaStartMs;

		Waypoints::InstallAnnotations(data.walls.data(), data.nodeAreas.data(), count, data.areas.data(),
									  static_cast<int>(data.areas.size()), data.vis.data());
		const int wallbangStartMs = NowMs();
		const int probed = BuildWallbangs(data.areas, data.wallbangs);
		Waypoints::InstallWallbangs(data.wallbangs.data(), static_cast<int>(data.wallbangs.size()));
		const int wallbangsMs = NowMs() - wallbangStartMs;
		SaveAnnotations(mapName, data);

		const long long pairs = static_cast<long long>(data.areas.size()) * (static_cast<long long>(data.areas.size()) - 1) / 2;
		Report("navgen: annotated %s: walls for %d nodes in %d ms; %d areas (radius %.0f), %lld of %lld pairs in sight, "
			   "%d ms; %d wallbang pairs of %d probed, %d ms; %d traces\n",
			   mapName, count, wallsMs, static_cast<int>(data.areas.size()), radius, visibleCount, pairs, areasMs,
			   static_cast<int>(data.wallbangs.size()), probed, wallbangsMs, traceCount - startTraces);
	}
}
