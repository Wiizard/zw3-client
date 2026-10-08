#include "Components/Modules/BotAI/Navgen/Internal.hpp"
#include "Components/Modules/BotAI/Navgen/Navgen.hpp"
#include "Components/Modules/BotAI/Iw4.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace Components::BotAI::Navgen
{
	static constexpr int cmName = 0;
	static constexpr int cmNumBrushes = 272;
	static constexpr int cmBrushes = 280;
	static constexpr int cmBrushBounds = 288;
	static constexpr int cmBrushContents = 296;
	static constexpr int brushStride = 48;
	static constexpr int brushSides = 8;
	static constexpr int brushSideStride = 16;
	static constexpr int boundsStride = 24;
	static constexpr int contentsSolid = 1;
	static constexpr float wallCellUnits = 512.0f;
	static constexpr float wallMaxHalfUnits = 4096.0f;
	static constexpr int wallMaxCellsPerAxis = 256;
	static constexpr int maxSlicePoints = 40;
	static constexpr float flatNormal = 0.0001f;

	static char wallMap[64] = "";
	static bool hasWalls = false;
	static float gridMin[2] = {};
	static int gridCells[2] = {};
	static std::vector<int> cellStarts;
	static std::vector<unsigned short> cellBrushes;
	static std::vector<int> brushStamps;
	static int stampCounter = 0;

	static const char* BrushAt(int brush)
	{
		const char* brushes = *reinterpret_cast<const char* const*>(cm + cmBrushes);
		return brushes + brushStride * brush;
	}

	static const float* BoundsAt(int brush)
	{
		const char* bounds = *reinterpret_cast<const char* const*>(cm + cmBrushBounds);
		return reinterpret_cast<const float*>(bounds + boundsStride * brush);
	}

	static bool IsWallBrush(int brush)
	{
		const int* contents = *reinterpret_cast<const int* const*>(cm + cmBrushContents);

		if ((contents[brush] & contentsSolid) == 0)
		{
			return false;
		}

		const float* bounds = BoundsAt(brush);
		return bounds[3] <= wallMaxHalfUnits && bounds[4] <= wallMaxHalfUnits;
	}

	static bool IsClipMapFor(const char* mapName)
	{
		if (!cm)
		{
			return false;
		}

		const char* clipName = *reinterpret_cast<const char* const*>(cm + cmName);

		if (!clipName)
		{
			return false;
		}

		char expected[96];
		_snprintf_s(expected, sizeof(expected), _TRUNCATE, "maps/mp/%s.d3dbsp", mapName);
		return _stricmp(clipName, expected) == 0;
	}

	static void CellOf(float x, float y, int* outX, int* outY)
	{
		int cellX = static_cast<int>((x - gridMin[0]) / wallCellUnits);
		int cellY = static_cast<int>((y - gridMin[1]) / wallCellUnits);

		if (cellX < 0)
		{
			cellX = 0;
		}

		if (cellY < 0)
		{
			cellY = 0;
		}

		if (cellX >= gridCells[0])
		{
			cellX = gridCells[0] - 1;
		}

		if (cellY >= gridCells[1])
		{
			cellY = gridCells[1] - 1;
		}

		*outX = cellX;
		*outY = cellY;
	}

	static bool BuildWallGrid()
	{
		const int count = *reinterpret_cast<const unsigned short*>(cm + cmNumBrushes);
		const bool hasArrays = *reinterpret_cast<const char* const*>(cm + cmBrushes)
			&& *reinterpret_cast<const char* const*>(cm + cmBrushBounds)
			&& *reinterpret_cast<const char* const*>(cm + cmBrushContents);

		if (count == 0 || !hasArrays)
		{
			return false;
		}

		float low[2] = { 1.0e30f, 1.0e30f };
		float high[2] = { -1.0e30f, -1.0e30f };
		int wallCount = 0;

		for (int brush = 0; brush < count; ++brush)
		{
			if (!IsWallBrush(brush))
			{
				continue;
			}

			const float* bounds = BoundsAt(brush);

			for (int axis = 0; axis < 2; ++axis)
			{
				const float minimum = bounds[axis] - bounds[3 + axis];
				const float maximum = bounds[axis] + bounds[3 + axis];

				if (minimum < low[axis])
				{
					low[axis] = minimum;
				}

				if (maximum > high[axis])
				{
					high[axis] = maximum;
				}
			}

			++wallCount;
		}

		if (wallCount == 0)
		{
			return false;
		}

		gridMin[0] = low[0];
		gridMin[1] = low[1];

		for (int axis = 0; axis < 2; ++axis)
		{
			int cells = static_cast<int>((high[axis] - low[axis]) / wallCellUnits) + 1;

			if (cells > wallMaxCellsPerAxis)
			{
				cells = wallMaxCellsPerAxis;
			}

			gridCells[axis] = cells;
		}

		const int cellTotal = gridCells[0] * gridCells[1];
		std::vector<int> cellCounts(cellTotal, 0);

		for (int pass = 0; pass < 2; ++pass)
		{
			if (pass == 1)
			{
				cellStarts.assign(cellTotal + 1, 0);

				for (int cell = 0; cell < cellTotal; ++cell)
				{
					cellStarts[cell + 1] = cellStarts[cell] + cellCounts[cell];
					cellCounts[cell] = 0;
				}

				cellBrushes.assign(cellStarts[cellTotal], 0);
			}

			for (int brush = 0; brush < count; ++brush)
			{
				if (!IsWallBrush(brush))
				{
					continue;
				}

				const float* bounds = BoundsAt(brush);
				int fromX = 0;
				int fromY = 0;
				int toX = 0;
				int toY = 0;
				CellOf(bounds[0] - bounds[3], bounds[1] - bounds[4], &fromX, &fromY);
				CellOf(bounds[0] + bounds[3], bounds[1] + bounds[4], &toX, &toY);

				for (int cellY = fromY; cellY <= toY; ++cellY)
				{
					for (int cellX = fromX; cellX <= toX; ++cellX)
					{
						const int cell = cellY * gridCells[0] + cellX;

						if (pass == 1)
						{
							cellBrushes[cellStarts[cell] + cellCounts[cell]] = static_cast<unsigned short>(brush);
						}

						++cellCounts[cell];
					}
				}
			}
		}

		brushStamps.assign(count, 0);
		stampCounter = 0;
		Report("navgen: walls from %d solid brushes of %d, %d x %d cells\n", wallCount, count, gridCells[0], gridCells[1]);
		return true;
	}

	void RefreshWalls(const char* mapName)
	{
		if (!mapName || !*mapName)
		{
			return;
		}

		if (std::strcmp(mapName, wallMap) != 0)
		{
			hasWalls = false;
			cellStarts.clear();
			cellBrushes.clear();
			brushStamps.clear();
		}

		if (hasWalls || !IsClipMapFor(mapName))
		{
			return;
		}

		_snprintf_s(wallMap, sizeof(wallMap), _TRUNCATE, "%s", mapName);
		hasWalls = BuildWallGrid();
	}

	bool HasWallGeometry()
	{
		return hasWalls;
	}

	static int SliceBrush(int brush, float z, float (*outPoints)[2])
	{
		const float* bounds = BoundsAt(brush);

		if (z < bounds[2] - bounds[5] || z > bounds[2] + bounds[5])
		{
			return 0;
		}

		float points[maxSlicePoints][2] = {
			{ bounds[0] - bounds[3], bounds[1] - bounds[4] },
			{ bounds[0] + bounds[3], bounds[1] - bounds[4] },
			{ bounds[0] + bounds[3], bounds[1] + bounds[4] },
			{ bounds[0] - bounds[3], bounds[1] + bounds[4] },
		};
		int count = 4;

		const char* brushData = BrushAt(brush);
		const int sideCount = *reinterpret_cast<const unsigned short*>(brushData);
		const char* sides = *reinterpret_cast<const char* const*>(brushData + brushSides);

		for (int side = 0; side < sideCount && sides; ++side)
		{
			const float* plane = *reinterpret_cast<const float* const*>(sides + brushSideStride * side);

			if (!plane)
			{
				continue;
			}

			const float a = plane[0];
			const float b = plane[1];
			const float c = plane[3] - plane[2] * z;

			if (std::fabs(a) < flatNormal && std::fabs(b) < flatNormal)
			{
				if (c < 0.0f)
				{
					return 0;
				}

				continue;
			}

			float clipped[maxSlicePoints][2];
			int clippedCount = 0;

			for (int i = 0; i < count && clippedCount < maxSlicePoints - 1; ++i)
			{
				const float* current = points[i];
				const float* next = points[(i + 1) % count];
				const float currentSide = a * current[0] + b * current[1] - c;
				const float nextSide = a * next[0] + b * next[1] - c;

				if (currentSide <= 0.0f)
				{
					clipped[clippedCount][0] = current[0];
					clipped[clippedCount][1] = current[1];
					++clippedCount;
				}

				if ((currentSide <= 0.0f) != (nextSide <= 0.0f))
				{
					const float share = currentSide / (currentSide - nextSide);
					clipped[clippedCount][0] = current[0] + share * (next[0] - current[0]);
					clipped[clippedCount][1] = current[1] + share * (next[1] - current[1]);
					++clippedCount;
				}
			}

			if (clippedCount < 3)
			{
				return 0;
			}

			for (int i = 0; i < clippedCount; ++i)
			{
				points[i][0] = clipped[i][0];
				points[i][1] = clipped[i][1];
			}

			count = clippedCount;
		}

		for (int i = 0; i < count; ++i)
		{
			outPoints[i][0] = points[i][0];
			outPoints[i][1] = points[i][1];
		}

		return count;
	}

	int SliceWallsNear(const float* around, float radius, float z, WallEdge* out, int maxEdges)
	{
		if (!hasWalls)
		{
			return 0;
		}

		++stampCounter;

		int fromX = 0;
		int fromY = 0;
		int toX = 0;
		int toY = 0;
		CellOf(around[0] - radius, around[1] - radius, &fromX, &fromY);
		CellOf(around[0] + radius, around[1] + radius, &toX, &toY);

		int edgeCount = 0;
		float points[maxSlicePoints][2];

		for (int cellY = fromY; cellY <= toY; ++cellY)
		{
			for (int cellX = fromX; cellX <= toX; ++cellX)
			{
				const int cell = cellY * gridCells[0] + cellX;

				for (int k = cellStarts[cell]; k < cellStarts[cell + 1]; ++k)
				{
					const int brush = cellBrushes[k];

					if (brushStamps[brush] == stampCounter)
					{
						continue;
					}

					brushStamps[brush] = stampCounter;

					const float* bounds = BoundsAt(brush);

					if (std::fabs(bounds[0] - around[0]) > bounds[3] + radius
						|| std::fabs(bounds[1] - around[1]) > bounds[4] + radius)
					{
						continue;
					}

					const int count = SliceBrush(brush, z, points);

					for (int i = 0; i < count && count >= 3 && edgeCount < maxEdges; ++i)
					{
						const float* from = points[i];
						const float* to = points[(i + 1) % count];
						out[edgeCount].from[0] = from[0];
						out[edgeCount].from[1] = from[1];
						out[edgeCount].to[0] = to[0];
						out[edgeCount].to[1] = to[1];
						++edgeCount;
					}
				}
			}
		}

		return edgeCount;
	}
}
