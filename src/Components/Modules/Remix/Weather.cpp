#include "STDInclude.hpp"

#include "Weather.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "Api.hpp"
#include "Device.hpp"
#include "Iw4.hpp"
#include "FixedFunction.hpp"
#include "Look.hpp"
#include "Outdoor.hpp"
#include "Read.hpp"
#include "Scene.hpp"
#include "TextureTags.hpp"

namespace Components::Remix
{
	struct CloudVertex
	{
		float position[3];
		D3DCOLOR colour;
		float texCoord[2];
	};

	static constexpr DWORD cloudVertexFormat = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
	static constexpr int maxCloudParticles = 1024;
	static constexpr int gridColumns = 8;
	static constexpr int gridRows = 8;
	static constexpr int gridLayers = 16;
	static constexpr float sameEpsilon = 0.001f;
	static constexpr float outdoorTowardEye = 32.0f;
	static constexpr float nearFadeStart = 6.0f;
	static constexpr float nearFadeEnd = 48.0f;
	static constexpr float farFadeStart = 1500.0f;
	static constexpr float farFadeEnd = 2500.0f;
	static constexpr float sceneMinZ = 0.015625f;
	// Particle cloud budget (mp_zombie_diner sent Remix 22k-43k cloud quads a frame, every particle of every cloud):
	// particles further than keepAllDistance and outside a cone around the view are left out, and past
	// thinStartDistance a steady share is left out, falling with the square of the distance, the rest drawn that much
	// more opaque (up to maximumAlphaBoost) so the cloud keeps its density.
	static constexpr float keepAllDistance = 128.0f;
	static constexpr float viewConeCos = 0.34f;
	static constexpr float thinStartDistance = 400.0f;
	static constexpr float maximumAlphaBoost = 2.0f;
	static constexpr float sceneMaxZ = 1.0f;

	static WeatherStats stats;
	static std::array<std::array<float, 3>, maxCloudParticles> localPositions;
	static bool hasLocalPositions = false;
	static std::vector<CloudVertex> vertices;
	static std::vector<std::uint16_t> indices;

	static constexpr UINT rainTextureWidth = 4;
	static constexpr UINT rainTextureHeight = 64;
	// The streak box around the eye. Past about 300 units streaks fade out and are under a pixel wide, so the box is
	// kept to that and the drop count follows its volume: the same density near you for about a third of the streaks.
	static constexpr float rainHalfWidth = 300.0f;
	static constexpr float rainHalfHeight = 250.0f;
	static constexpr float rainFallSpeed = 520.0f;
	static constexpr float rainWindSpeed = 45.0f;
	static constexpr float rainStreakLength = 40.0f;
	static constexpr float rainStreakWidth = 0.25f;
	// Far streaks widen up to this many times so they stay about a pixel wide instead of flickering out.
	static constexpr float rainWidthGrowthDistance = 120.0f;
	static constexpr float maximumRainWidthGrowth = 4.0f;
	static constexpr int rainAlpha = 85;
	// Albedo, not light: the streaks are Remix particles, so the scene's sun, sky and lamps light them.
	static constexpr D3DCOLOR rainColour = 0x00B4BECC;
	// How hard zw3 rains on Remix Plus when the map or a weather preset rains and Rain on any map is off.
	static constexpr float mapRainAmount = 3.0f;
	static constexpr float drizzleRainAmount = 1.0f;
	static constexpr float rainstormRainAmount = 10.0f;
	static constexpr float thunderstormRainAmount = 14.0f;
	static constexpr float fogDrizzleRainAmount = 0.5f;
	// 1024 drops per amount filled the old 450 x 450 x 350 box; the count scales with the box so the density stays.
	static constexpr float referenceRainVolume = 450.0f * 450.0f * 350.0f;
	static constexpr int rainDropsPerAmount = static_cast<int>(1024.0f * rainHalfWidth * rainHalfWidth * rainHalfHeight
		/ referenceRainVolume);
	static constexpr float maximumRainAmount = 16.0f;
	// Four vertices a drop through 16 bit indices: 16384 drops is the most one draw can address.
	static constexpr int maxRainDrops = 16384;
	static constexpr float gustStrength = 0.6f;
	static constexpr float gustRate = 0.23f;
	static constexpr float crossWindSpeed = 25.0f;
	static constexpr float crossWindRate = 0.17f;

	struct RainDrop
	{
		float local[3];
		float speed;
		float length;
		float width;
		float brightness;
	};

	static std::vector<RainDrop> rainDrops;
	static std::vector<CloudVertex> rainVertices;
	static std::vector<std::uint16_t> rainIndices;
	static IDirect3DTexture9* rainTexture = nullptr;
	static IDirect3DStateBlock9* rainState = nullptr;
	static bool didRainTextureFail = false;
	static std::uintptr_t rainyMapIdentity = 0;

	struct ScreenDrop
	{
		float x;
		float y;
		float radius;
		float age;
		float life;
		float slideDelay;
		float speed;
	};

	static constexpr UINT dropTextureSize = 64;
	static constexpr std::size_t maxScreenDrops = 64;
	static constexpr float dropsPerSecond = 5.0f;
	static constexpr float dropFadeInSeconds = 0.15f;
	static constexpr float dropFadeOutSeconds = 1.2f;
	static constexpr float slidingRadius = 0.014f;
	static constexpr DWORD dropVertexFormat = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;

	struct DropVertex
	{
		float position[4];
		D3DCOLOR colour;
		float texCoord[2];
	};

	static std::vector<ScreenDrop> screenDrops;
	static IDirect3DTexture9* dropTexture = nullptr;
	static IDirect3DStateBlock9* dropState = nullptr;
	static bool didDropTextureFail = false;
	static ULONGLONG lastDropTick = 0;
	static float dropSpawnCarry = 0.0f;
	static std::uint32_t dropRandomState = 0x9E3779B9u;

	static void BuildLocalPositions()
	{
		std::uint32_t seed = 0x2545F491u;
		int count = 0;

		for (int layer = 0; layer < gridLayers; ++layer)
		{
			for (int row = 0; row < gridRows; ++row)
			{
				for (int column = 0; column < gridColumns; ++column)
				{
					const int cell[3] = { column, row, layer };
					const int cells[3] = { gridColumns, gridRows, gridLayers };

					for (int axis = 0; axis < 3; ++axis)
					{
						seed = seed * 1664525u + 1013904223u;
						const float jitter = static_cast<float>(seed >> 8) / 16777216.0f;
						localPositions[count][axis] = (static_cast<float>(cell[axis]) + jitter) * 2.0f
							/ static_cast<float>(cells[axis]) - 1.0f;
					}

					++count;
				}
			}
		}

		std::sort(localPositions.begin(), localPositions.end(), [](const auto& left, const auto& right)
		{
			const float leftLength = left[0] * left[0] + left[1] * left[1] + left[2] * left[2];
			const float rightLength = right[0] * right[0] + right[1] * right[1] + right[2] * right[2];
			return leftLength < rightLength;
		});

		indices.resize(static_cast<std::size_t>(maxCloudParticles) * 6);

		for (int particle = 0; particle < maxCloudParticles; ++particle)
		{
			const auto base = static_cast<std::uint16_t>(particle * 4);
			std::uint16_t* quad = indices.data() + static_cast<std::size_t>(particle) * 6;
			quad[0] = base;
			quad[1] = static_cast<std::uint16_t>(base + 1);
			quad[2] = static_cast<std::uint16_t>(base + 2);
			quad[3] = static_cast<std::uint16_t>(base + 2);
			quad[4] = static_cast<std::uint16_t>(base + 1);
			quad[5] = static_cast<std::uint16_t>(base + 3);
		}

		vertices.resize(static_cast<std::size_t>(maxCloudParticles) * 4);
		hasLocalPositions = true;
	}

	static void QuatToAxis(const float* quat, float axis[3][3])
	{
		const float x = quat[0];
		const float y = quat[1];
		const float z = quat[2];
		const float w = quat[3];

		axis[0][0] = 1.0f - 2.0f * (y * y + z * z);
		axis[0][1] = 2.0f * (x * y + w * z);
		axis[0][2] = 2.0f * (x * z - w * y);

		axis[1][0] = 2.0f * (x * y - w * z);
		axis[1][1] = 1.0f - 2.0f * (x * x + z * z);
		axis[1][2] = 2.0f * (y * z + w * x);

		axis[2][0] = 2.0f * (x * z + w * y);
		axis[2][1] = 2.0f * (y * z - w * x);
		axis[2][2] = 1.0f - 2.0f * (x * x + y * y);
	}

	static float Saturate(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	static void ComputeQuadBasis(std::uintptr_t cloud, const float* right, const float* up, float* sizeAxis, float* lengthAxis)
	{
		float origin[3];
		float tail[3];
		std::memcpy(origin, reinterpret_cast<const void*>(cloud + Iw4::particleCloudOrigin), sizeof(origin));
		std::memcpy(tail, reinterpret_cast<const void*>(cloud + Iw4::particleCloudTail), sizeof(tail));
		const float size = ReadAt<float>(cloud + Iw4::particleCloudSize);
		const float length = ReadAt<float>(cloud + Iw4::particleCloudLength);

		float planeSize[2] = { size, 0.0f };
		float planeLength[2] = { 0.0f, length };

		const bool isSameTail = std::fabs(tail[0] - origin[0]) <= sameEpsilon && std::fabs(tail[1] - origin[1]) <= sameEpsilon
			&& std::fabs(tail[2] - origin[2]) <= sameEpsilon;

		if (size != length && !isSameTail)
		{
			float motion[3] = { tail[0] - origin[0], tail[1] - origin[1], tail[2] - origin[2] };
			const float motionLength = std::sqrt(motion[0] * motion[0] + motion[1] * motion[1] + motion[2] * motion[2]);

			for (float& component : motion)
			{
				component = component / motionLength * length;
			}

			const float x = motion[0] * right[0] + motion[1] * right[1] + motion[2] * right[2];
			const float y = motion[0] * up[0] + motion[1] * up[1] + motion[2] * up[2];

			if (std::fabs(x) >= sameEpsilon || std::fabs(y) >= sameEpsilon)
			{
				const float projected = std::sqrt(x * x + y * y);
				const float scale = size / projected;

				planeSize[0] = y * scale;
				planeSize[1] = -x * scale;
				planeLength[0] = x;
				planeLength[1] = y;

				if (size > projected)
				{
					planeLength[0] = x * scale;
					planeLength[1] = y * scale;
				}
			}
		}

		for (int axis = 0; axis < 3; ++axis)
		{
			sizeAxis[axis] = planeSize[0] * right[axis] + planeSize[1] * up[axis];
			lengthAxis[axis] = planeLength[0] * right[axis] + planeLength[1] * up[axis];
		}
	}

	bool TryDrawWeatherCloud(IDirect3DDevice9* device, std::uintptr_t cloud, std::uintptr_t material, bool isOutdoorMasked)
	{
		const auto flags = ReadAt<std::uint8_t>(cloud + Iw4::particleCloudFlags);
		IDirect3DTexture9* colorMap = GetMaterialColorMap(material);
		float camera[3];

		if ((flags & Iw4::particleCloudSparkFlag) != 0 || !colorMap || !TryGetCameraOrigin(camera))
		{
			++stats.cloudsPassedToEngine;
			return false;
		}

		// While Remix Plus rains itself, the map's rain is still noted (auto weather follows it) but not drawn.
		if (const char* materialName = ReadAt<const char*>(material);
			materialName && std::string_view(materialName).find("rain") != std::string_view::npos)
		{
			rainyMapIdentity = GetMapIdentity();

			// Remix Plus rains with path traced particles (Weather menu; Auto follows this map rain), so the game's
			// flat rain quads are never drawn there.
			if (IsRemixPlus())
			{
				return true;
			}
		}

		if (!hasLocalPositions)
		{
			BuildLocalPositions();
		}

		D3DMATRIX view{};
		device->GetTransform(D3DTS_VIEW, &view);

		const float right[3] = { view._11, view._21, view._31 };
		const float up[3] = { view._12, view._22, view._32 };
		const float forward[3] = { view._13, view._23, view._33 };

		float quat[4];
		float origin[3];
		std::memcpy(quat, reinterpret_cast<const void*>(cloud + Iw4::particleCloudQuat), sizeof(quat));
		std::memcpy(origin, reinterpret_cast<const void*>(cloud + Iw4::particleCloudOrigin), sizeof(origin));
		const float scale = ReadAt<float>(cloud + Iw4::particleCloudScale);
		const auto colour = ReadAt<std::uint32_t>(cloud + Iw4::particleCloudColor);
		const int particleCount = maxCloudParticles >> (flags & Iw4::particleCloudCountShiftMask);

		float axis[3][3];
		QuatToAxis(quat, axis);

		float sizeAxis[3];
		float lengthAxis[3];
		ComputeQuadBasis(cloud, right, up, sizeAxis, lengthAxis);

		const float cloudAlpha = static_cast<float>(colour >> 24);
		int drawn = 0;

		for (int particle = 0; particle < particleCount; ++particle)
		{
			const float* local = localPositions[particle].data();
			float centre[3];

			for (int component = 0; component < 3; ++component)
			{
				centre[component] = origin[component] + scale * (local[0] * axis[0][component] + local[1] * axis[1][component]
					+ local[2] * axis[2][component]);
			}

			const float dx = centre[0] - camera[0];
			const float dy = centre[1] - camera[1];
			const float dz = centre[2] - camera[2];
			const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);

			if (distance > keepAllDistance && (dx * forward[0] + dy * forward[1] + dz * forward[2]) < viewConeCos * distance)
			{
				++stats.particlesOffscreen;
				continue;
			}

			float keep = 1.0f;

			if (distance > thinStartDistance)
			{
				keep = thinStartDistance * thinStartDistance / (distance * distance);
				const std::uint32_t seed = (static_cast<std::uint32_t>(particle) * 2654435761u)
					^ (static_cast<std::uint32_t>(cloud >> 4) * 40503u);
				const float pick = static_cast<float>((seed * 1664525u + 1013904223u) >> 8) / 16777216.0f;

				if (pick >= keep)
				{
					++stats.particlesThinned;
					continue;
				}
			}

			const float fade = Saturate((distance - nearFadeStart) / (nearFadeEnd - nearFadeStart))
				* Saturate((farFadeEnd - distance) / (farFadeEnd - farFadeStart));
			const float boost = std::min(1.0f / keep, maximumAlphaBoost);
			const int alpha = std::min(static_cast<int>(cloudAlpha * fade * boost + 0.5f), 255);

			if (alpha <= 0)
			{
				++stats.particlesFaded;
				continue;
			}

			if (isOutdoorMasked)
			{
				const float lookup[3] = {
					centre[0] - outdoorTowardEye * forward[0],
					centre[1] - outdoorTowardEye * forward[1],
					centre[2] - outdoorTowardEye * forward[2],
				};

				if (!IsWeatherPointOutdoors(lookup))
				{
					++stats.particlesUnderCover;
					continue;
				}
			}

			const D3DCOLOR particleColour = (static_cast<D3DCOLOR>(alpha) << 24) | (colour & 0x00FFFFFFu);
			CloudVertex* quad = vertices.data() + static_cast<std::size_t>(drawn) * 4;

			for (int corner = 0; corner < 4; ++corner)
			{
				const float u = static_cast<float>(corner >> 1);
				const float v = static_cast<float>(corner & 1);

				for (int component = 0; component < 3; ++component)
				{
					quad[corner].position[component] = centre[component] + (u - 0.5f) * sizeAxis[component]
						+ (v - 0.5f) * lengthAxis[component];
				}

				quad[corner].colour = particleColour;
				quad[corner].texCoord[0] = u;
				quad[corner].texCoord[1] = v;
			}

			++drawn;
		}

		++stats.cloudsDrawn;

		const char* materialName = ReadAt<const char*>(material);

		if (materialName && std::string_view(materialName).find("rain") != std::string_view::npos)
		{
			rainyMapIdentity = GetMapIdentity();
		}

		if (drawn == 0)
		{
			return true;
		}

		IDirect3DVertexShader9* vertexShader = nullptr;
		IDirect3DPixelShader9* pixelShader = nullptr;
		IDirect3DVertexDeclaration9* declaration = nullptr;
		IDirect3DVertexBuffer9* stream = nullptr;
		IDirect3DIndexBuffer9* indexBuffer = nullptr;
		IDirect3DBaseTexture9* boundTexture = nullptr;
		UINT streamOffset = 0;
		UINT streamStride = 0;
		DWORD vertexFormat = 0;
		DWORD lighting = TRUE;
		DWORD colourOperation = D3DTOP_MODULATE;
		DWORD colourArgument1 = D3DTA_TEXTURE;
		DWORD colourArgument2 = D3DTA_CURRENT;
		DWORD alphaOperation = D3DTOP_SELECTARG1;
		DWORD alphaArgument1 = D3DTA_TEXTURE;
		DWORD alphaArgument2 = D3DTA_CURRENT;
		DWORD textureTransformFlags = D3DTTFF_DISABLE;
		DWORD secondColourOperation = D3DTOP_DISABLE;
		DWORD secondAlphaOperation = D3DTOP_DISABLE;
		D3DMATRIX world{};
		D3DVIEWPORT9 viewport{};

		device->GetVertexShader(&vertexShader);
		device->GetPixelShader(&pixelShader);
		device->GetVertexDeclaration(&declaration);
		device->GetFVF(&vertexFormat);
		device->GetStreamSource(0, &stream, &streamOffset, &streamStride);
		device->GetIndices(&indexBuffer);
		device->GetTexture(0, &boundTexture);
		device->GetRenderState(D3DRS_LIGHTING, &lighting);
		device->GetTextureStageState(0, D3DTSS_COLOROP, &colourOperation);
		device->GetTextureStageState(0, D3DTSS_COLORARG1, &colourArgument1);
		device->GetTextureStageState(0, D3DTSS_COLORARG2, &colourArgument2);
		device->GetTextureStageState(0, D3DTSS_ALPHAOP, &alphaOperation);
		device->GetTextureStageState(0, D3DTSS_ALPHAARG1, &alphaArgument1);
		device->GetTextureStageState(0, D3DTSS_ALPHAARG2, &alphaArgument2);
		device->GetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, &textureTransformFlags);
		device->GetTextureStageState(1, D3DTSS_COLOROP, &secondColourOperation);
		device->GetTextureStageState(1, D3DTSS_ALPHAOP, &secondAlphaOperation);
		device->GetTransform(D3DTS_WORLD, &world);
		device->GetViewport(&viewport);

		D3DMATRIX identity{};
		identity._11 = 1.0f;
		identity._22 = 1.0f;
		identity._33 = 1.0f;
		identity._44 = 1.0f;

		D3DVIEWPORT9 sceneViewport = viewport;
		sceneViewport.MinZ = sceneMinZ;
		sceneViewport.MaxZ = sceneMaxZ;

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(cloudVertexFormat);
		device->SetTransform(D3DTS_WORLD, &identity);
		device->SetViewport(&sceneViewport);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->SetTexture(0, colorMap);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(drawn) * 4, static_cast<UINT>(drawn) * 2,
			indices.data(), D3DFMT_INDEX16, vertices.data(), sizeof(CloudVertex));

		device->SetTextureStageState(1, D3DTSS_ALPHAOP, secondAlphaOperation);
		device->SetTextureStageState(1, D3DTSS_COLOROP, secondColourOperation);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, textureTransformFlags);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, alphaArgument2);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, alphaArgument1);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, alphaOperation);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, colourArgument2);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, colourArgument1);
		device->SetTextureStageState(0, D3DTSS_COLOROP, colourOperation);
		device->SetTexture(0, boundTexture);
		device->SetRenderState(D3DRS_LIGHTING, lighting);
		device->SetViewport(&viewport);
		device->SetTransform(D3DTS_WORLD, &world);
		device->SetStreamSource(0, stream, streamOffset, streamStride);
		device->SetIndices(indexBuffer);

		if (declaration)
		{
			device->SetVertexDeclaration(declaration);
		}
		else
		{
			device->SetFVF(vertexFormat);
		}

		device->SetPixelShader(pixelShader);
		device->SetVertexShader(vertexShader);

		if (vertexShader)
		{
			vertexShader->Release();
		}

		if (pixelShader)
		{
			pixelShader->Release();
		}

		if (declaration)
		{
			declaration->Release();
		}

		if (stream)
		{
			stream->Release();
		}

		if (indexBuffer)
		{
			indexBuffer->Release();
		}

		if (boundTexture)
		{
			boundTexture->Release();
		}

		stats.particlesDrawn += static_cast<unsigned long long>(drawn);
		return true;
	}

	static IDirect3DTexture9* TryBuildRainTexture(IDirect3DDevice9* device)
	{
		std::vector<std::uint32_t> texels(static_cast<std::size_t>(rainTextureWidth) * rainTextureHeight);

		for (UINT y = 0; y < rainTextureHeight; ++y)
		{
			const float along = (static_cast<float>(y) + 0.5f) / static_cast<float>(rainTextureHeight);
			const float profile = std::sqrt(std::sin(3.14159265f * along));

			for (UINT x = 0; x < rainTextureWidth; ++x)
			{
				const float across = (static_cast<float>(x) + 0.5f) / static_cast<float>(rainTextureWidth) * 2.0f - 1.0f;
				const float edge = 1.0f - across * across;
				const auto alpha = static_cast<std::uint32_t>(std::clamp(profile * edge, 0.0f, 1.0f) * 255.0f + 0.5f);
				texels[static_cast<std::size_t>(y) * rainTextureWidth + x] = (alpha << 24) | 0x00FFFFFFu;
			}
		}

		D3DSURFACE_DESC description{};
		description.Format = D3DFMT_A8R8G8B8;
		description.Width = rainTextureWidth;
		description.Height = rainTextureHeight;

		std::uint64_t hash = 0;

		if (!Device::TryHashTopLevel(texels.data(), description, hash))
		{
			return nullptr;
		}

		IDirect3DTexture9* texture = nullptr;

		if (FAILED(device->CreateTexture(rainTextureWidth, rainTextureHeight, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
				&texture, nullptr)))
		{
			return nullptr;
		}

		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
		{
			texture->Release();
			return nullptr;
		}

		for (UINT y = 0; y < rainTextureHeight; ++y)
		{
			std::memcpy(static_cast<unsigned char*>(locked.pBits) + static_cast<std::size_t>(y) * locked.Pitch,
				texels.data() + static_cast<std::size_t>(y) * rainTextureWidth, rainTextureWidth * sizeof(std::uint32_t));
		}

		texture->UnlockRect(0);
		TagGeneratedTexture(device, hash, TextureTag::Particle);
		return texture;
	}

	static void BuildRainDrops()
	{
		std::uint32_t seed = 0x9E3779B9u;
		rainDrops.resize(maxRainDrops);

		const auto next = [&seed]()
		{
			seed = seed * 1664525u + 1013904223u;
			return static_cast<float>(seed >> 8) / 16777216.0f;
		};

		for (RainDrop& drop : rainDrops)
		{
			for (float& axis : drop.local)
			{
				axis = next() * 2.0f - 1.0f;
			}

			drop.speed = 0.75f + 0.5f * next();
			drop.length = 0.6f + 0.8f * next();
			drop.width = 0.5f + 0.4f * next();
			drop.brightness = 0.6f + 0.4f * next();
		}

		rainIndices.resize(static_cast<std::size_t>(maxRainDrops) * 6);

		for (int drop = 0; drop < maxRainDrops; ++drop)
		{
			const auto base = static_cast<std::uint16_t>(drop * 4);
			std::uint16_t* quad = rainIndices.data() + static_cast<std::size_t>(drop) * 6;
			quad[0] = base;
			quad[1] = static_cast<std::uint16_t>(base + 1);
			quad[2] = static_cast<std::uint16_t>(base + 2);
			quad[3] = static_cast<std::uint16_t>(base + 2);
			quad[4] = static_cast<std::uint16_t>(base + 1);
			quad[5] = static_cast<std::uint16_t>(base + 3);
		}

		rainVertices.resize(static_cast<std::size_t>(maxRainDrops) * 4);
	}

	static float WrapAround(float offset, float halfSize)
	{
		const float size = 2.0f * halfSize;
		return offset - size * std::floor((offset + halfSize) / size);
	}

	// zw3's rain amount this frame: Rain on any map's slider, or on Remix Plus (whose map rain is not drawn by the
	// game) the map's rain or the weather preset's. 0 is no rain.
	static float GetRainAmount(const LookSettings& look)
	{
		if (look.isRainEverywhere)
		{
			return std::clamp(look.rainAmount, 0.0f, maximumRainAmount);
		}

		if (!IsRemixPlus() || IsPlusPrecipitationActive() || !IsAnyRain())
		{
			return 0.0f;
		}

		// Auto rains like the map (it picks Rainstorm only for the sky); a chosen preset rains as hard as its name.
		static constexpr int autoWeather = 0;
		const PlusWeatherPreset* preset = GetPlusWeather();
		const std::string_view name = preset && look.plusWeather != autoWeather ? preset->name : "";

		if (name == "drizzle")
		{
			return drizzleRainAmount;
		}

		if (name == "foggy")
		{
			return fogDrizzleRainAmount;
		}

		if (name == "rainstorm")
		{
			return rainstormRainAmount;
		}

		if (name == "thunderstorm")
		{
			return thunderstormRainAmount;
		}

		return mapRainAmount;
	}

	void DrawRainEverywhere(IDirect3DDevice9* device)
	{
		const LookSettings look = GetLookSettings();
		const float amount = GetRainAmount(look);
		float camera[3];

		if (amount <= 0.0f || !device || didRainTextureFail || !TryGetCameraOrigin(camera))
		{
			return;
		}

		if (!rainTexture)
		{
			rainTexture = TryBuildRainTexture(device);

			if (!rainTexture)
			{
				didRainTextureFail = true;
				return;
			}
		}

		if (!rainState && FAILED(device->CreateStateBlock(D3DSBT_ALL, &rainState)))
		{
			rainState = nullptr;
			return;
		}

		if (rainDrops.empty())
		{
			BuildRainDrops();
		}

		const float seconds = static_cast<float>(GetTickCount64() % 600000ull) / 1000.0f;
		const float halfSize[3] = { rainHalfWidth, rainHalfWidth, rainHalfHeight };
		const float windNow[2] = {
			rainWindSpeed * (1.0f + gustStrength * std::sin(gustRate * seconds)),
			crossWindSpeed * std::sin(crossWindRate * seconds + 1.0f),
		};
		const float windTravel[2] = {
			rainWindSpeed * (seconds - gustStrength * std::cos(gustRate * seconds) / gustRate),
			-crossWindSpeed * std::cos(crossWindRate * seconds + 1.0f) / crossWindRate,
		};
		const int particleCount = std::min(static_cast<int>(static_cast<float>(rainDropsPerAmount) * amount), maxRainDrops);
		int drawn = 0;

		for (int particle = 0; particle < particleCount; ++particle)
		{
			const RainDrop& raindrop = rainDrops[particle];
			const float fallSpeed = rainFallSpeed * raindrop.speed;
			const float travel[3] = { windTravel[0], windTravel[1], -fallSpeed * seconds };
			const float velocity[3] = { windNow[0], windNow[1], -fallSpeed };
			const float speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] + velocity[2] * velocity[2]);
			const float fall[3] = { velocity[0] / speed, velocity[1] / speed, velocity[2] / speed };
			float centre[3];
			float distanceSquared = 0.0f;

			for (int axis = 0; axis < 3; ++axis)
			{
				const float anchored = raindrop.local[axis] * halfSize[axis] + travel[axis];
				const float offset = WrapAround(anchored - camera[axis], halfSize[axis]);
				centre[axis] = camera[axis] + offset;
				distanceSquared += offset * offset;
			}

			const float distance = std::sqrt(distanceSquared);
			const float fade = Saturate((distance - nearFadeStart) / (nearFadeEnd - nearFadeStart))
				* Saturate((rainHalfWidth - distance) / (rainHalfWidth * 0.3f));
			const int alpha = static_cast<int>(static_cast<float>(rainAlpha) * raindrop.brightness * fade + 0.5f);

			if (alpha <= 0 || !IsWeatherPointOutdoors(centre))
			{
				continue;
			}

			const float toEye[3] = { camera[0] - centre[0], camera[1] - centre[1], camera[2] - centre[2] };
			float side[3] = {
				fall[1] * toEye[2] - fall[2] * toEye[1],
				fall[2] * toEye[0] - fall[0] * toEye[2],
				fall[0] * toEye[1] - fall[1] * toEye[0],
			};
			const float sideLength = std::sqrt(side[0] * side[0] + side[1] * side[1] + side[2] * side[2]);

			if (sideLength < sameEpsilon)
			{
				continue;
			}

			const float widthGrowth = std::clamp(distance / rainWidthGrowthDistance, 1.0f, maximumRainWidthGrowth);

			for (float& component : side)
			{
				component = component / sideLength * rainStreakWidth * raindrop.width * widthGrowth;
			}

			const float streakLength = rainStreakLength * raindrop.length * raindrop.speed;
			const D3DCOLOR particleColour = (static_cast<D3DCOLOR>(alpha) << 24) | rainColour;
			CloudVertex* quad = rainVertices.data() + static_cast<std::size_t>(drawn) * 4;

			for (int corner = 0; corner < 4; ++corner)
			{
				const float u = static_cast<float>(corner >> 1);
				const float v = static_cast<float>(corner & 1);

				for (int component = 0; component < 3; ++component)
				{
					quad[corner].position[component] = centre[component] + (u - 0.5f) * side[component]
						+ (v - 0.5f) * streakLength * fall[component];
				}

				quad[corner].colour = particleColour;
				quad[corner].texCoord[0] = u;
				quad[corner].texCoord[1] = v;
			}

			++drawn;
		}

		if (drawn == 0)
		{
			return;
		}

		rainState->Capture();

		D3DMATRIX identity{};
		identity._11 = 1.0f;
		identity._22 = 1.0f;
		identity._33 = 1.0f;
		identity._44 = 1.0f;

		D3DVIEWPORT9 viewport{};
		device->GetViewport(&viewport);
		viewport.MinZ = sceneMinZ;
		viewport.MaxZ = sceneMaxZ;

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(cloudVertexFormat);
		device->SetTransform(D3DTS_WORLD, &identity);
		device->SetViewport(&viewport);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
		device->SetTexture(0, rainTexture);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		device->DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST, 0, static_cast<UINT>(drawn) * 4, static_cast<UINT>(drawn) * 2,
			rainIndices.data(), D3DFMT_INDEX16, rainVertices.data(), sizeof(CloudVertex));

		rainState->Apply();
		stats.particlesDrawn += static_cast<unsigned long long>(drawn);
	}

	bool IsRaining()
	{
		if (GetLookSettings().isRainEverywhere)
		{
			return true;
		}

		return rainyMapIdentity != 0 && rainyMapIdentity == GetMapIdentity();
	}

	static float NextDropRandom()
	{
		dropRandomState ^= dropRandomState << 13;
		dropRandomState ^= dropRandomState >> 17;
		dropRandomState ^= dropRandomState << 5;
		return static_cast<float>(dropRandomState & 0xFFFFFF) / 16777215.0f;
	}

	static IDirect3DTexture9* TryBuildDropTexture(IDirect3DDevice9* device)
	{
		IDirect3DTexture9* texture = nullptr;

		if (FAILED(device->CreateTexture(dropTextureSize, dropTextureSize, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
				&texture, nullptr)))
		{
			return nullptr;
		}

		D3DLOCKED_RECT locked{};

		if (FAILED(texture->LockRect(0, &locked, nullptr, 0)))
		{
			texture->Release();
			return nullptr;
		}

		for (UINT y = 0; y < dropTextureSize; ++y)
		{
			auto* row = reinterpret_cast<std::uint32_t*>(static_cast<unsigned char*>(locked.pBits) + y * locked.Pitch);

			for (UINT x = 0; x < dropTextureSize; ++x)
			{
				const float u = (static_cast<float>(x) + 0.5f) / dropTextureSize * 2.0f - 1.0f;
				const float v = (static_cast<float>(y) + 0.5f) / dropTextureSize * 2.0f - 1.0f;
				const float distance = std::sqrt(u * u + v * v);

				if (distance >= 1.0f)
				{
					row[x] = 0;
					continue;
				}

				const float edge = std::clamp((1.0f - distance) / 0.12f, 0.0f, 1.0f);
				const float rim = std::clamp((distance - 0.7f) / 0.3f, 0.0f, 1.0f);
				const float highlightDistance = std::sqrt((u + 0.35f) * (u + 0.35f) + (v + 0.4f) * (v + 0.4f));
				const float highlight = std::clamp(1.0f - highlightDistance / 0.28f, 0.0f, 1.0f);
				const float crescentDistance = std::sqrt((u - 0.2f) * (u - 0.2f) + (v - 0.25f) * (v - 0.25f));
				const float crescent = std::clamp(1.0f - std::fabs(crescentDistance - 0.55f) / 0.15f, 0.0f, 1.0f)
					* std::clamp(u + v, 0.0f, 1.0f);

				const float light = std::clamp(highlight * highlight * 1.0f + crescent * 0.55f, 0.0f, 1.0f);
				const float body = 0.18f + rim * 0.35f;
				const float alpha = std::clamp((body + light * 0.7f) * edge, 0.0f, 1.0f);
				const float shade = std::clamp(0.12f + light * 0.88f, 0.0f, 1.0f);

				const auto channel = static_cast<std::uint32_t>(shade * 255.0f + 0.5f);
				const auto blue = static_cast<std::uint32_t>(std::min(255.0f, shade * 255.0f + 18.0f));
				row[x] = (static_cast<std::uint32_t>(alpha * 255.0f + 0.5f) << 24) | (channel << 16) | (channel << 8) | blue;
			}
		}

		texture->UnlockRect(0);
		return texture;
	}

	static bool IsCameraInRain()
	{
		float camera[3];
		return IsRaining() && TryGetCameraOrigin(camera) && IsWeatherPointOutdoors(camera);
	}

	static void UpdateScreenDrops(float seconds, bool isWet, float amount)
	{
		if (isWet)
		{
			dropSpawnCarry += seconds * dropsPerSecond * std::clamp(amount, 0.3f, 3.0f);
		}
		else
		{
			dropSpawnCarry = 0.0f;
		}

		while (dropSpawnCarry >= 1.0f && screenDrops.size() < maxScreenDrops)
		{
			dropSpawnCarry -= 1.0f;

			ScreenDrop drop{};
			drop.x = NextDropRandom();
			drop.y = NextDropRandom() * 0.9f;
			drop.radius = 0.006f + NextDropRandom() * NextDropRandom() * 0.018f;
			drop.life = 1.5f + NextDropRandom() * 3.5f;
			drop.slideDelay = 0.4f + NextDropRandom() * 1.2f;
			drop.speed = drop.radius >= slidingRadius ? 0.04f + NextDropRandom() * 0.12f : 0.0f;
			screenDrops.push_back(drop);
		}

		dropSpawnCarry = std::min(dropSpawnCarry, 1.0f);

		for (ScreenDrop& drop : screenDrops)
		{
			drop.age += seconds;

			if (drop.speed > 0.0f && drop.age > drop.slideDelay)
			{
				drop.speed += seconds * 0.15f;
				drop.y += drop.speed * seconds;
			}
		}

		std::erase_if(screenDrops, [](const ScreenDrop& drop)
		{
			return drop.age >= drop.life || drop.y - drop.radius > 1.0f;
		});
	}

	void DrawScreenDrops(IDirect3DDevice9* device)
	{
		const LookSettings look = GetLookSettings();
		const ULONGLONG now = GetTickCount64();
		const float seconds = lastDropTick == 0 ? 0.0f : std::min(static_cast<float>(now - lastDropTick) / 1000.0f, 0.1f);
		lastDropTick = now;

		if (!device || !look.isScreenDrops || didDropTextureFail || IsRemixPlus())
		{
			screenDrops.clear();
			return;
		}

		UpdateScreenDrops(seconds, IsCameraInRain(), look.rainAmount);

		if (screenDrops.empty())
		{
			return;
		}

		if (!dropTexture)
		{
			dropTexture = TryBuildDropTexture(device);

			if (!dropTexture)
			{
				didDropTextureFail = true;
				return;
			}
		}

		if (!dropState && FAILED(device->CreateStateBlock(D3DSBT_ALL, &dropState)))
		{
			dropState = nullptr;
			return;
		}

		D3DVIEWPORT9 viewport{};
		device->GetViewport(&viewport);

		const float width = static_cast<float>(viewport.Width);
		const float height = static_cast<float>(viewport.Height);
		std::vector<DropVertex> triangles;
		triangles.reserve(screenDrops.size() * 6);

		for (const ScreenDrop& drop : screenDrops)
		{
			const float fadeIn = std::clamp(drop.age / dropFadeInSeconds, 0.0f, 1.0f);
			const float fadeOut = std::clamp((drop.life - drop.age) / dropFadeOutSeconds, 0.0f, 1.0f);
			const auto alpha = static_cast<std::uint32_t>(fadeIn * fadeOut * 255.0f + 0.5f);
			const D3DCOLOR colour = (alpha << 24) | 0x00FFFFFFu;

			const bool isSliding = drop.speed > 0.0f && drop.age > drop.slideDelay;
			const float halfWidth = drop.radius * height;
			const float halfHeight = halfWidth * (isSliding ? 1.35f : 1.0f);
			const float centreX = static_cast<float>(viewport.X) + drop.x * width;
			const float centreY = static_cast<float>(viewport.Y) + drop.y * height;

			const DropVertex corners[4] = {
				{ { centreX - halfWidth - 0.5f, centreY - halfHeight - 0.5f, 0.0f, 1.0f }, colour, { 0.0f, 0.0f } },
				{ { centreX + halfWidth - 0.5f, centreY - halfHeight - 0.5f, 0.0f, 1.0f }, colour, { 1.0f, 0.0f } },
				{ { centreX - halfWidth - 0.5f, centreY + halfHeight - 0.5f, 0.0f, 1.0f }, colour, { 0.0f, 1.0f } },
				{ { centreX + halfWidth - 0.5f, centreY + halfHeight - 0.5f, 0.0f, 1.0f }, colour, { 1.0f, 1.0f } },
			};

			triangles.push_back(corners[0]);
			triangles.push_back(corners[1]);
			triangles.push_back(corners[2]);
			triangles.push_back(corners[2]);
			triangles.push_back(corners[1]);
			triangles.push_back(corners[3]);
		}

		dropState->Capture();

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(dropVertexFormat);
		device->SetTexture(0, dropTexture);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN
			| D3DCOLORWRITEENABLE_BLUE);
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		device->DrawPrimitiveUP(D3DPT_TRIANGLELIST, static_cast<UINT>(triangles.size() / 3), triangles.data(),
			sizeof(DropVertex));

		dropState->Apply();
	}

	void ReleaseWeather()
	{
		if (dropTexture)
		{
			dropTexture->Release();
			dropTexture = nullptr;
		}

		if (dropState)
		{
			dropState->Release();
			dropState = nullptr;
		}

		screenDrops.clear();
		didDropTextureFail = false;

		if (rainTexture)
		{
			rainTexture->Release();
			rainTexture = nullptr;
		}

		if (rainState)
		{
			rainState->Release();
			rainState = nullptr;
		}

		didRainTextureFail = false;
	}

	WeatherStats GetWeatherStats()
	{
		return stats;
	}
}
