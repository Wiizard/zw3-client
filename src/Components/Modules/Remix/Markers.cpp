#include "STDInclude.hpp"

#include "Markers.hpp"

#include <cmath>

#include "MapSettings.hpp"
#include "Triggers.hpp"
#include "Scene.hpp"
#include "Weather.hpp"

namespace Components::Remix
{
	static constexpr float markerHalfSize = 0.1337f;
	static constexpr float markerGrowthPerId = 0.001f;
	static constexpr float sceneMinZ = 0.015625f;
	static constexpr float sceneMaxZ = 1.0f;
	static constexpr DWORD markerVertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;

	struct MarkerVertex
	{
		float position[3];
		D3DCOLOR colour;
		float texCoord[2];
	};

	static IDirect3DStateBlock9* markerState = nullptr;
	static MarkerStats stats;

	static D3DMATRIX MarkerMatrix(const MapMarker& marker)
	{
		const float degToRad = 3.14159265f / 180.0f;
		const float pitch = marker.rotationDeg[0] * degToRad;
		const float yaw = marker.rotationDeg[1] * degToRad;
		const float roll = marker.rotationDeg[2] * degToRad;

		const float sp = std::sin(pitch);
		const float cp = std::cos(pitch);
		const float sy = std::sin(yaw);
		const float cy = std::cos(yaw);
		const float sr = std::sin(roll);
		const float cr = std::cos(roll);

		const float forward[3] = { cp * cy, cp * sy, -sp };
		const float left[3] = { sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, sr * cp };
		const float up[3] = { cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };

		D3DMATRIX matrix{};
		matrix._11 = forward[0] * marker.scale[0];
		matrix._12 = forward[1] * marker.scale[0];
		matrix._13 = forward[2] * marker.scale[0];
		matrix._21 = left[0] * marker.scale[1];
		matrix._22 = left[1] * marker.scale[1];
		matrix._23 = left[2] * marker.scale[1];
		matrix._31 = up[0] * marker.scale[2];
		matrix._32 = up[1] * marker.scale[2];
		matrix._33 = up[2] * marker.scale[2];
		matrix._41 = marker.position[0];
		matrix._42 = marker.position[1];
		matrix._43 = marker.position[2];
		matrix._44 = 1.0f;

		return matrix;
	}

	static bool IsMarkerShown(const MapMarker& marker, const float* camera, bool hasCamera)
	{
		if (marker.whenRaining && *marker.whenRaining != IsRaining())
		{
			return false;
		}

		if (!marker.trigger.empty() && !IsTriggerActive(marker.trigger))
		{
			return false;
		}

		if (marker.maxDistance <= 0.0f || !hasCamera)
		{
			return true;
		}

		const float offset[3] = {
			marker.position[0] - camera[0],
			marker.position[1] - camera[1],
			marker.position[2] - camera[2],
		};

		return offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]
			<= marker.maxDistance * marker.maxDistance;
	}

	void DrawMapMarkers(IDirect3DDevice9* device)
	{
		const MapSettings& mapSettings = GetMapSettings();
		stats.markers = static_cast<int>(mapSettings.markers.size());
		stats.drawnMarkers = 0;

		if (!device || mapSettings.markers.empty())
		{
			return;
		}

		if (!markerState && FAILED(device->CreateStateBlock(D3DSBT_ALL, &markerState)))
		{
			markerState = nullptr;
			return;
		}

		float camera[3] = {};
		const bool hasCamera = TryGetCameraOrigin(camera);
		bool hasCapturedState = false;

		for (const MapMarker& marker : mapSettings.markers)
		{
			if (!IsMarkerShown(marker, camera, hasCamera))
			{
				continue;
			}

			if (!hasCapturedState)
			{
				hasCapturedState = true;
				markerState->Capture();

				D3DVIEWPORT9 viewport{};
				device->GetViewport(&viewport);
				viewport.MinZ = sceneMinZ;
				viewport.MaxZ = sceneMaxZ;

				device->SetVertexShader(nullptr);
				device->SetPixelShader(nullptr);
				device->SetFVF(markerVertexFormat);
				device->SetViewport(&viewport);
				device->SetTexture(0, nullptr);
				device->SetRenderState(D3DRS_LIGHTING, FALSE);
				device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
				device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
				device->SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
				device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
				device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
				device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
				device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
				device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
				device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
				device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
				device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
				device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
			}

			const float id = static_cast<float>(marker.id);
			const float half = markerHalfSize + id * markerGrowthPerId;
			const DWORD channel = static_cast<DWORD>(marker.id) & 0xFF;

			const MarkerVertex quad[4] = {
				{ { -half, -half, 0.0f }, D3DCOLOR_XRGB(channel, 0, 0), { 0.0f, id / 100.0f } },
				{ { half, -half, 0.0f }, D3DCOLOR_XRGB(0, channel, 0), { id / 100.0f, 0.0f } },
				{ { -half, half, 0.0f }, D3DCOLOR_XRGB(channel, 0, channel), { 0.0f, id / 100.0f } },
				{ { half, half, 0.0f }, D3DCOLOR_XRGB(0, 0, channel), { 0.0f, id / 100.0f } },
			};

			const D3DMATRIX world = MarkerMatrix(marker);
			device->SetTransform(D3DTS_WORLD, &world);
			device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(MarkerVertex));
			++stats.drawnMarkers;
		}

		if (hasCapturedState)
		{
			markerState->Apply();
		}
	}

	void ReleaseMarkers()
	{
		if (markerState)
		{
			markerState->Release();
			markerState = nullptr;
		}
	}

	MarkerStats GetMarkerStats()
	{
		return stats;
	}
}
