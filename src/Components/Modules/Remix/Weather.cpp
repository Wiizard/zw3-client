#include "STDInclude.hpp"

#include "Weather.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

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
	static constexpr float sceneMaxZ = 1.0f;

	static WeatherStats stats;
	static std::array<std::array<float, 3>, maxCloudParticles> localPositions;
	static bool hasLocalPositions = false;
	static std::vector<CloudVertex> vertices;
	static std::vector<std::uint16_t> indices;

	static constexpr UINT rainTextureWidth = 4;
	static constexpr UINT rainTextureHeight = 64;
	static constexpr float rainHalfWidth = 700.0f;
	static constexpr float rainHalfHeight = 450.0f;
	static constexpr float rainFallSpeed = 520.0f;
	static constexpr float rainWindSpeed = 45.0f;
	static constexpr float rainStreakLength = 30.0f;
	static constexpr float rainStreakWidth = 0.7f;
	static constexpr int rainAlpha = 150;
	static constexpr D3DCOLOR rainColour = 0x00C8D2DC;
	static constexpr int rainDropsPerAmount = 1024;
	static constexpr float maximumRainAmount = 10.0f;
	static constexpr int maxRainDrops = 10240;
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
			const float fade = Saturate((distance - nearFadeStart) / (nearFadeEnd - nearFadeStart))
				* Saturate((farFadeEnd - distance) / (farFadeEnd - farFadeStart));
			const int alpha = static_cast<int>(cloudAlpha * fade + 0.5f);

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

	void DrawRainEverywhere(IDirect3DDevice9* device)
	{
		const LookSettings look = GetLookSettings();
		float camera[3];

		if (!look.isRainEverywhere || !device || didRainTextureFail || !TryGetCameraOrigin(camera))
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
		const float amount = std::clamp(look.rainAmount, 0.0f, maximumRainAmount);
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

			for (float& component : side)
			{
				component = component / sideLength * rainStreakWidth * raindrop.width;
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

	void ReleaseWeather()
	{
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
