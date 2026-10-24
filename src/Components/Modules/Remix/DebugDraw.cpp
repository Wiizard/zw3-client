#include "STDInclude.hpp"

#include "DebugDraw.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "Core.hpp"
#include "Api.hpp"
#include "Lights.hpp"
#include "MapSettings.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	enum class DebugColour
	{
		MapLight,
		IgnoredLight,
		ChangedLight,
		AuthoredLight,
		Marker,
		Trigger,
		CullingArea,
	};

	static constexpr int debugColourCount = 7;
	static constexpr float debugColours[debugColourCount][3] = {
		{ 1.0f, 0.85f, 0.2f },
		{ 1.0f, 0.1f, 0.1f },
		{ 0.2f, 0.45f, 1.0f },
		{ 0.2f, 1.0f, 0.3f },
		{ 0.1f, 1.0f, 1.0f },
		{ 1.0f, 0.2f, 1.0f },
		{ 1.0f, 1.0f, 1.0f },
	};
	static constexpr std::uint64_t materialHashBase = 0x4D57325F44424731ull;
	static constexpr std::uint64_t meshHashBase = 0x4D57325F44424D31ull;
	static constexpr float crossSize = 8.0f;
	static constexpr int circleSegments = 24;

	struct DebugLine
	{
		float from[3];
		float to[3];
	};

	static DebugDrawSettings settings;
	static DebugDrawStats stats;
	static const remixapi_Interface* api = nullptr;
	static remixapi_MaterialHandle materials[debugColourCount] = {};
	static remixapi_MeshHandle previousMesh = nullptr;
	static std::vector<DebugLine> lines[debugColourCount];
	static std::uint64_t frameCounter = 0;

	static bool TryCreateMaterials()
	{
		for (int colour = 0; colour < debugColourCount; ++colour)
		{
			if (materials[colour])
			{
				continue;
			}

			remixapi_MaterialInfoOpaqueEXT opaque{};
			opaque.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO_OPAQUE_EXT;
			opaque.roughnessTexture = L"";
			opaque.metallicTexture = L"";
			opaque.heightTexture = L"";
			opaque.opacityConstant = 1.0f;
			opaque.roughnessConstant = 1.0f;
			opaque.metallicConstant = 0.0f;
			// Remix Plus reads these from 0.1000.1 callers, its defaults for older callers are on and 1. Stock ignores them.
			opaque.enableDlssControlMask = true;
			opaque.dlssControlMaskIntensity = 1.0f;
			opaque.dlssControlMaskToneStrength = 1.0f;
			opaque.dlssControlMaskStructuralStrength = 1.0f;

			remixapi_MaterialInfo info{};
			info.sType = REMIXAPI_STRUCT_TYPE_MATERIAL_INFO;
			info.pNext = &opaque;
			info.hash = materialHashBase + static_cast<std::uint64_t>(colour);
			info.albedoTexture = L"";
			info.normalTexture = L"";
			info.tangentTexture = L"";
			info.emissiveTexture = L"";
			info.emissiveIntensity = 5.0f;
			info.emissiveColorConstant = { debugColours[colour][0], debugColours[colour][1], debugColours[colour][2] };

			if (api->CreateMaterial(&info, &materials[colour]) != REMIXAPI_ERROR_CODE_SUCCESS)
			{
				materials[colour] = nullptr;
				stats.didFail = true;
				LogError() << "remix: could not create the debug drawing materials, debug shapes stay off";
				return false;
			}
		}

		return true;
	}

	static void AddLine(DebugColour colour, const float* from, const float* to)
	{
		DebugLine line;
		std::copy(from, from + 3, line.from);
		std::copy(to, to + 3, line.to);
		lines[static_cast<int>(colour)].push_back(line);
	}

	static void AddCross(DebugColour colour, const float* centre, float size)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			float from[3] = { centre[0], centre[1], centre[2] };
			float to[3] = { centre[0], centre[1], centre[2] };
			from[axis] -= size;
			to[axis] += size;
			AddLine(colour, from, to);
		}
	}

	static void AddBox(DebugColour colour, const float* low, const float* high)
	{
		for (int edge = 0; edge < 12; ++edge)
		{
			const int axis = edge / 4;
			const int corner = edge % 4;
			const int otherA = (axis + 1) % 3;
			const int otherB = (axis + 2) % 3;

			float from[3];
			from[axis] = low[axis];
			from[otherA] = low[otherA];
			from[otherB] = low[otherB];

			if (corner & 1)
			{
				from[otherA] = high[otherA];
			}

			if (corner & 2)
			{
				from[otherB] = high[otherB];
			}

			float to[3] = { from[0], from[1], from[2] };
			to[axis] = high[axis];
			AddLine(colour, from, to);
		}
	}

	static void AddSphere(DebugColour colour, const float* centre, float radius)
	{
		const float step = 6.2831853f / static_cast<float>(circleSegments);

		for (int axis = 0; axis < 3; ++axis)
		{
			const int first = (axis + 1) % 3;
			const int second = (axis + 2) % 3;

			for (int segment = 0; segment < circleSegments; ++segment)
			{
				const float angleA = step * static_cast<float>(segment);
				const float angleB = step * static_cast<float>(segment + 1);

				float from[3] = { centre[0], centre[1], centre[2] };
				float to[3] = { centre[0], centre[1], centre[2] };
				from[first] += std::cos(angleA) * radius;
				from[second] += std::sin(angleA) * radius;
				to[first] += std::cos(angleB) * radius;
				to[second] += std::sin(angleB) * radius;
				AddLine(colour, from, to);
			}
		}
	}

	static bool IsNear(const float* point, const float* camera, float range)
	{
		const float offset[3] = { point[0] - camera[0], point[1] - camera[1], point[2] - camera[2] };
		return offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2] <= range * range;
	}

	static void CollectShapes(const float* camera)
	{
		if (settings.isLights)
		{
			for (const MapLightInfo& light : GetMapLightInfo())
			{
				if (!IsNear(light.origin, camera, settings.lightRange))
				{
					continue;
				}

				DebugColour colour = DebugColour::MapLight;

				if (light.isAuthored)
				{
					colour = DebugColour::AuthoredLight;
				}
				else if (light.isIgnored)
				{
					colour = DebugColour::IgnoredLight;
				}
				else if (light.isChanged)
				{
					colour = DebugColour::ChangedLight;
				}

				AddCross(colour, light.origin, crossSize);
			}
		}

		const MapSettings& mapSettings = GetMapSettings();

		if (settings.isMarkers)
		{
			for (const MapMarker& marker : mapSettings.markers)
			{
				AddCross(DebugColour::Marker, marker.position, crossSize);
			}
		}

		if (settings.isTriggers)
		{
			for (const TriggerRule& rule : mapSettings.triggers)
			{
				if (rule.hasBox)
				{
					AddBox(DebugColour::Trigger, rule.boxMin, rule.boxMax);
				}

				if (rule.hasSphere)
				{
					AddSphere(DebugColour::Trigger, rule.centre, rule.radius);
				}
			}
		}

		if (settings.isCullingAreas)
		{
			for (const CullingArea& area : mapSettings.cullingAreas)
			{
				AddBox(DebugColour::CullingArea, area.boxMin, area.boxMax);
			}
		}
	}

	static void AppendQuad(const DebugLine& line, const float* camera, float width,
		std::vector<remixapi_HardcodedVertex>& vertices, std::vector<std::uint32_t>& indices)
	{
		const float direction[3] = { line.to[0] - line.from[0], line.to[1] - line.from[1], line.to[2] - line.from[2] };
		const float toCamera[3] = { camera[0] - line.from[0], camera[1] - line.from[1], camera[2] - line.from[2] };

		float side[3] = {
			direction[1] * toCamera[2] - direction[2] * toCamera[1],
			direction[2] * toCamera[0] - direction[0] * toCamera[2],
			direction[0] * toCamera[1] - direction[1] * toCamera[0],
		};

		const float length = std::sqrt(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);

		if (length < 0.0001f)
		{
			return;
		}

		for (float& component : side)
		{
			component = component / length * width * 0.5f;
		}

		const auto first = static_cast<std::uint32_t>(vertices.size());
		const float* ends[2] = { line.from, line.to };

		for (int corner = 0; corner < 4; ++corner)
		{
			const float* end = ends[corner >> 1];
			float sign = -1.0f;

			if (corner & 1)
			{
				sign = 1.0f;
			}

			remixapi_HardcodedVertex vertex{};
			vertex.position[0] = end[0] + side[0] * sign;
			vertex.position[1] = end[1] + side[1] * sign;
			vertex.position[2] = end[2] + side[2] * sign;
			vertex.normal[2] = 1.0f;
			vertex.texcoord[0] = static_cast<float>(corner >> 1);
			vertex.texcoord[1] = static_cast<float>(corner & 1);
			vertex.color = 0xFFFFFFFF;
			vertices.push_back(vertex);
		}

		const std::uint32_t quad[6] = { first, first + 1, first + 2, first + 3, first + 2, first + 1 };
		indices.insert(indices.end(), quad, quad + 6);
	}

	void DrawDebugShapes(IDirect3DDevice9* device)
	{
		api = TryGetApi(device);

		if (!api)
		{
			return;
		}

		if (previousMesh)
		{
			api->DestroyMesh(previousMesh);
			previousMesh = nullptr;
		}

		stats.lines = 0;

		const bool isAnyShown = settings.isLights || settings.isMarkers || settings.isTriggers || settings.isCullingAreas;
		float camera[3] = {};

		if (!isAnyShown || stats.didFail || !TryGetCameraOrigin(camera) || !TryCreateMaterials())
		{
			return;
		}

		for (std::vector<DebugLine>& colourLines : lines)
		{
			colourLines.clear();
		}

		CollectShapes(camera);

		std::vector<remixapi_HardcodedVertex> vertices[debugColourCount];
		std::vector<std::uint32_t> indices[debugColourCount];
		std::vector<remixapi_MeshInfoSurfaceTriangles> surfaces;

		for (int colour = 0; colour < debugColourCount; ++colour)
		{
			for (const DebugLine& line : lines[colour])
			{
				AppendQuad(line, camera, settings.lineWidth, vertices[colour], indices[colour]);
			}

			if (indices[colour].empty())
			{
				continue;
			}

			stats.lines += static_cast<int>(lines[colour].size());

			remixapi_MeshInfoSurfaceTriangles surface{};
			surface.vertices_values = vertices[colour].data();
			surface.vertices_count = vertices[colour].size();
			surface.indices_values = indices[colour].data();
			surface.indices_count = indices[colour].size();
			surface.material = materials[colour];
			surfaces.push_back(surface);
		}

		if (surfaces.empty())
		{
			return;
		}

		++frameCounter;

		remixapi_MeshInfo meshInfo{};
		meshInfo.sType = REMIXAPI_STRUCT_TYPE_MESH_INFO;
		meshInfo.hash = meshHashBase ^ (frameCounter << 8);
		meshInfo.surfaces_values = surfaces.data();
		meshInfo.surfaces_count = static_cast<std::uint32_t>(surfaces.size());

		if (api->CreateMesh(&meshInfo, &previousMesh) != REMIXAPI_ERROR_CODE_SUCCESS)
		{
			previousMesh = nullptr;
			return;
		}

		remixapi_InstanceInfo instance{};
		instance.sType = REMIXAPI_STRUCT_TYPE_INSTANCE_INFO;
		instance.categoryFlags = REMIXAPI_INSTANCE_CATEGORY_BIT_IGNORE_LIGHTS;
		instance.mesh = previousMesh;
		instance.transform.matrix[0][0] = 1.0f;
		instance.transform.matrix[1][1] = 1.0f;
		instance.transform.matrix[2][2] = 1.0f;
		instance.doubleSided = 1;

		api->DrawInstance(&instance);
	}

	void ReleaseDebugDraw()
	{
		if (api)
		{
			if (previousMesh)
			{
				api->DestroyMesh(previousMesh);
			}

			for (remixapi_MaterialHandle& material : materials)
			{
				if (material)
				{
					api->DestroyMaterial(material);
				}
			}
		}

		previousMesh = nullptr;

		for (remixapi_MaterialHandle& material : materials)
		{
			material = nullptr;
		}

		api = nullptr;
	}

	void SetDebugDrawSettings(const DebugDrawSettings& newSettings)
	{
		settings = newSettings;
		settings.lineWidth = std::clamp(newSettings.lineWidth, 0.1f, 16.0f);
		settings.lightRange = std::clamp(newSettings.lightRange, 100.0f, 50000.0f);
	}

	DebugDrawSettings GetDebugDrawSettings()
	{
		return settings;
	}

	DebugDrawStats GetDebugDrawStats()
	{
		return stats;
	}
}
