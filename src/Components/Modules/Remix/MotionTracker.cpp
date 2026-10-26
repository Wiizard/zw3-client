#include "STDInclude.hpp"

#include "MotionTracker.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <unordered_map>

#include "Core.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "FixedFunction.hpp"
#include "Hook.hpp"
#include "Iw4.hpp"
#include "PatchCalls.hpp"
#include "Read.hpp"
#include "Scene.hpp"

namespace Components::Remix
{
	using AddCode3DQuadSurf = std::int64_t(__fastcall*)(void*, std::uint64_t, void*, const float*, const float*, float,
		float, float, float, float, float, const float*, std::uintptr_t);
	using TessMarkSurfList = char(__fastcall*)(void*, void**, void*);

	struct TrackerScreen
	{
		float centre[3];
		float halfWidth;
		float halfHeight;
		bool isValid;
	};

	static AddCode3DQuadSurf originalAddQuad = nullptr;
	static TessMarkSurfList originalTessMarks = nullptr;
	static TrackerScreen screen{};
	static constexpr float overlayLift = 0.05f;
	static constexpr float overlayDimming = 186.0f;
	static float trackerBrightness = 1.0f;

	static std::uintptr_t ReadMaterialHandle(std::uintptr_t global)
	{
		return ReadAt<std::uintptr_t>(Engine::RebaseAddress(global));
	}

	static bool IsBackground(std::uintptr_t material)
	{
		return material && material == ReadMaterialHandle(Engine::CurrentBuild().motionTrackerBackgroundMaterial);
	}

	static bool IsBlipOrSweep(std::uintptr_t material)
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		return material
			&& (material == ReadMaterialHandle(build.motionTrackerSweepMaterial)
				|| material == ReadMaterialHandle(build.motionTrackerEnemyPingMaterial)
				|| material == ReadMaterialHandle(build.motionTrackerFriendlyPingMaterial));
	}

	static float Dot(const float* a, const float* b)
	{
		return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	}

	static float TowardViewer(const float* centre, const float* normal)
	{
		float eye[3];

		if (!TryGetCameraOrigin(eye))
		{
			return 0.0f;
		}

		const float toEye[3] = { eye[0] - centre[0], eye[1] - centre[1], eye[2] - centre[2] };

		if (Dot(normal, toEye) < 0.0f)
		{
			return -overlayLift;
		}

		return overlayLift;
	}

	static float Interpolate(float valueAtPositive, float valueAtNegative, float halfSize, float position)
	{
		return valueAtPositive + (valueAtNegative - valueAtPositive) * (halfSize - position) / (2.0f * halfSize);
	}

	static std::int64_t __fastcall AddQuadDetour(void* buffer, std::uint64_t baseVertex, void* indices, const float* centre,
		const float* axis, float width, float height, float s0, float t0, float s1, float t1, const float* colour,
		std::uintptr_t material)
	{
		const bool isBackground = IsBackground(material);

		if (!isBackground && !IsBlipOrSweep(material))
		{
			return originalAddQuad(buffer, baseVertex, indices, centre, axis, width, height, s0, t0, s1, t1, colour,
				material);
		}

		const float lift = TowardViewer(centre, axis);

		if (isBackground)
		{
			screen.centre[0] = centre[0];
			screen.centre[1] = centre[1];
			screen.centre[2] = centre[2];
			screen.halfWidth = width * 0.5f;
			screen.halfHeight = height * 0.5f;
			screen.isValid = true;

			const float lifted[3] = { centre[0] + axis[0] * lift, centre[1] + axis[1] * lift, centre[2] + axis[2] * lift };

			return originalAddQuad(buffer, baseVertex, indices, lifted, axis, width, height, s0, t0, s1, t1, colour,
				material);
		}

		const float halfWidth = width * 0.5f;
		const float halfHeight = height * 0.5f;

		if (!screen.isValid || halfWidth <= 0.0f || halfHeight <= 0.0f)
		{
			return originalAddQuad(buffer, baseVertex, indices, centre, axis, width, height, s0, t0, s1, t1, colour,
				material);
		}

		const float* widthAxis = axis + 3;
		const float* heightAxis = axis + 6;
		const float offset[3] = { centre[0] - screen.centre[0], centre[1] - screen.centre[1], centre[2] - screen.centre[2] };
		const float alongWidth = Dot(offset, widthAxis);
		const float alongHeight = Dot(offset, heightAxis);

		const float widthLow = std::max(alongWidth - halfWidth, -screen.halfWidth);
		const float widthHigh = std::min(alongWidth + halfWidth, screen.halfWidth);
		const float heightLow = std::max(alongHeight - halfHeight, -screen.halfHeight);
		const float heightHigh = std::min(alongHeight + halfHeight, screen.halfHeight);

		if (widthHigh <= widthLow || heightHigh <= heightLow)
		{
			return originalAddQuad(buffer, baseVertex, indices, centre, axis, 0.0f, 0.0f, s0, t0, s1, t1, colour,
				material);
		}

		const float clippedS0 = Interpolate(s0, s1, halfWidth, widthHigh - alongWidth);
		const float clippedS1 = Interpolate(s0, s1, halfWidth, widthLow - alongWidth);
		const float clippedT0 = Interpolate(t0, t1, halfHeight, heightHigh - alongHeight);
		const float clippedT1 = Interpolate(t0, t1, halfHeight, heightLow - alongHeight);

		const float widthMiddle = (widthLow + widthHigh) * 0.5f;
		const float heightMiddle = (heightLow + heightHigh) * 0.5f;

		float clippedCentre[3];

		for (int i = 0; i < 3; ++i)
		{
			clippedCentre[i] = screen.centre[i] + widthAxis[i] * widthMiddle + heightAxis[i] * heightMiddle + axis[i] * lift;
		}

		return originalAddQuad(buffer, baseVertex, indices, clippedCentre, axis, widthHigh - widthLow,
			heightHigh - heightLow, clippedS0, clippedT0, clippedS1, clippedT1, colour, material);
	}

	static void ReorderSpriteQuadsUnsafe(void** iterator)
	{
		auto cursor = reinterpret_cast<std::uintptr_t>(iterator[0]);
		const auto end = reinterpret_cast<std::uintptr_t>(iterator[1]);

		if (!cursor || cursor >= end)
		{
			return;
		}

		const auto key = *reinterpret_cast<const std::uint32_t*>(cursor);

		while (cursor < end && *reinterpret_cast<const std::uint32_t*>(cursor) == key)
		{
			const auto triangles = *reinterpret_cast<const std::uint32_t*>(cursor + Iw4::codeSurfTriangleCount);
			auto* indices = *reinterpret_cast<std::uint16_t**>(cursor + Iw4::codeSurfIndices);
			const auto argCount = *reinterpret_cast<const std::uint16_t*>(cursor + Iw4::codeSurfArgCount);

			if (indices && argCount == 0 && (triangles % 2) == 0)
			{
				for (std::uint32_t quad = 0; quad < triangles / 2; ++quad)
				{
					std::uint16_t* corner = indices + quad * 6;
					const std::uint16_t first = corner[0];
					const bool isSpriteOrder = corner[1] == first + 1 && corner[2] == first + 2 && corner[3] == first + 2
						&& corner[4] == first + 3 && corner[5] == first;
					const bool isCodeMeshOrder = corner[1] == first + 1 && corner[2] == first + 3
						&& corner[3] == first + 3 && corner[4] == first + 1 && corner[5] == first + 2;

					if (isSpriteOrder)
					{
						corner[3] = first;
						corner[4] = static_cast<std::uint16_t>(first + 2);
						corner[5] = static_cast<std::uint16_t>(first + 3);
					}
					else if (isCodeMeshOrder)
					{
						corner[0] = static_cast<std::uint16_t>(first + 3);
						corner[1] = first;
						corner[2] = static_cast<std::uint16_t>(first + 1);
						corner[3] = static_cast<std::uint16_t>(first + 3);
						corner[4] = static_cast<std::uint16_t>(first + 1);
						corner[5] = static_cast<std::uint16_t>(first + 2);
					}
				}
			}

			cursor += Iw4::codeSurfStride;
		}
	}

	static void ReorderSpriteQuads(void** iterator)
	{
		__try
		{
			ReorderSpriteQuadsUnsafe(iterator);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
		}
	}

	static bool isHidingLampFlares = true;

	void SetLampFlaresHidden(bool isHidden)
	{
		isHidingLampFlares = isHidden;
	}

	bool AreLampFlaresHidden()
	{
		return isHidingLampFlares;
	}

	static bool IsEyeOffsetLampFlare(std::uintptr_t material)
	{
		// A material address reused by zombie eyes must not retain an old lamp-flare classification.
		const char* name = ReadAt<const char*>(material);
		const auto techniqueSet = ReadAt<std::uintptr_t>(material + Iw4::materialTechniqueSet);
		const char* techniqueSetName = techniqueSet ? ReadAt<const char*>(techniqueSet) : nullptr;
		bool isFlare = false;

		if (name && techniqueSetName && std::strstr(techniqueSetName, "eyeoffset"))
		{
			const std::string_view materialName(name);
			const auto has = [&materialName](std::string_view part) { return materialName.find(part) != std::string_view::npos; };

			const bool isLampGlow = has("glow") || has("flare") || has("halo") || has("corona") || has("light");
			const bool isWanted = has("eyeglow") || has("eye_glow") || has("muz") || has("fire") || has("spark")
				|| has("explo") || has("blood") || has("tracer");

			isFlare = isLampGlow && !isWanted;
		}

		return isFlare;
	}

	static std::uintptr_t FindScriptRunEnd(std::uintptr_t cursor, std::uintptr_t end, ScriptEffectKind& kind)
	{
		const auto key = ReadAt<std::uint32_t>(cursor);
		kind = GetScriptEffectSurfaceKind(ReadAt<const void*>(cursor + Iw4::codeSurfIndices));
		cursor += Iw4::codeSurfStride;

		while (cursor < end && ReadAt<std::uint32_t>(cursor) == key
			&& GetScriptEffectSurfaceKind(ReadAt<const void*>(cursor + Iw4::codeSurfIndices)) == kind)
		{
			cursor += Iw4::codeSurfStride;
		}

		return cursor;
	}

	static char DrawEffectRun(void* context, void** iterator, void* placement, std::uintptr_t material, ScriptEffectKind kind)
	{
		IDirect3DDevice9* sceneDevice = GetSceneDevice();
		IDirect3DTexture9* twin = nullptr;
		const char* action = "";

		if (sceneDevice && kind != ScriptEffectKind::None)
		{
			const bool isEye = kind == ScriptEffectKind::Eye;
			twin = isEye ? TryGetEyeColorMapTwin(sceneDevice, material) : TryGetParticleColorMapTwin(sceneDevice, material);
			std::uint64_t twinHash = 0;

			if (twin && Device::TryGetTextureHash(twin, twinHash))
			{
				static char twinAction[96];
				std::snprintf(twinAction, sizeof(twinAction), isEye
					? "zombie eye, no soft-particle fade copy 0x%016llX"
					: "zw3 script effect, particle copy 0x%016llX",
					static_cast<unsigned long long>(twinHash));
				action = twinAction;
			}
			else
			{
				action = isEye ? "zombie eye, no soft-particle fade copy FAILED" : "zw3 script effect, particle copy FAILED";
			}
		}
		else if (sceneDevice && isHidingLampFlares && IsEyeOffsetLampFlare(material))
		{
			twin = TryGetIgnoredColorMapTwin(sceneDevice, material);

			if (twin)
			{
				action = "hidden as a floating lamp flare";
			}
		}

		RecordDumpEffect(sceneDevice, material, action);

		if (!twin)
		{
			return originalTessMarks(context, iterator, placement);
		}

		IDirect3DBaseTexture9* boundTexture = nullptr;
		sceneDevice->GetTexture(0, &boundTexture);
		sceneDevice->SetTexture(0, twin);

		const char result = originalTessMarks(context, iterator, placement);

		sceneDevice->SetTexture(0, boundTexture);

		if (boundTexture)
		{
			boundTexture->Release();
		}

		return result;
	}

	static char __fastcall TessMarksDetour(void* context, void** iterator, void* placement)
	{
		const auto contextAddress = reinterpret_cast<std::uintptr_t>(context);
		const auto state = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsState);
		const auto material = ReadAt<std::uintptr_t>(state + Iw4::stateMaterial);

		if (!IsBackground(material) && !IsBlipOrSweep(material))
		{
			ReorderSpriteQuads(iterator);

			void* const end = iterator[1];
			const auto endAddress = reinterpret_cast<std::uintptr_t>(end);
			auto cursor = reinterpret_cast<std::uintptr_t>(iterator[0]);

			if (!cursor || cursor >= endAddress)
			{
				return originalTessMarks(context, iterator, placement);
			}

			const auto key = ReadAt<std::uint32_t>(cursor);
			char result = 0;

			while (true)
			{
				ScriptEffectKind kind = ScriptEffectKind::None;
				const std::uintptr_t runEnd = FindScriptRunEnd(cursor, endAddress, kind);

				iterator[1] = reinterpret_cast<void*>(runEnd);
				result = DrawEffectRun(context, iterator, placement, material, kind);
				iterator[1] = end;

				cursor = reinterpret_cast<std::uintptr_t>(iterator[0]);

				if (cursor != runEnd || cursor >= endAddress || ReadAt<std::uint32_t>(cursor) != key)
				{
					return result;
				}
			}
		}

		const auto source = ReadAt<std::uintptr_t>(contextAddress + Iw4::drawListArgsSource);
		auto* cameraView = reinterpret_cast<int*>(source + Iw4::sourceSceneDepthRange);
		const int savedCameraView = *cameraView;

		IDirect3DDevice9* device = GetSceneDevice();

		if (!device)
		{
			*cameraView = 0;
			const char result = originalTessMarks(context, iterator, placement);
			*cameraView = savedCameraView;

			return result;
		}

		DWORD colourOperation = D3DTOP_MODULATE;
		DWORD colourArgument1 = D3DTA_TEXTURE;
		DWORD colourArgument2 = D3DTA_CURRENT;
		DWORD textureFactor = 0xFFFFFFFF;

		device->GetTextureStageState(0, D3DTSS_COLOROP, &colourOperation);
		device->GetTextureStageState(0, D3DTSS_COLORARG1, &colourArgument1);
		device->GetTextureStageState(0, D3DTSS_COLORARG2, &colourArgument2);
		device->GetRenderState(D3DRS_TEXTUREFACTOR, &textureFactor);

		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_TFACTOR);
		const int dimming = static_cast<int>(std::clamp(overlayDimming * trackerBrightness, 0.0f, 255.0f));
		device->SetRenderState(D3DRS_TEXTUREFACTOR, D3DCOLOR_ARGB(255, dimming, dimming, dimming));
		Device::BeginOverlayBlendRemap(device);

		*cameraView = 0;
		const char result = originalTessMarks(context, iterator, placement);
		*cameraView = savedCameraView;

		Device::EndOverlayBlendRemap(device);
		device->SetRenderState(D3DRS_TEXTUREFACTOR, textureFactor);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, colourArgument2);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, colourArgument1);
		device->SetTextureStageState(0, D3DTSS_COLOROP, colourOperation);

		return result;
	}

	void SetTrackerBrightness(float brightness)
	{
		trackerBrightness = std::clamp(brightness, 0.0f, 255.0f / overlayDimming);
	}

	float GetTrackerBrightness()
	{
		return trackerBrightness;
	}

	bool InstallMotionTrackerHooks()
	{
		const Iw4::GameBuild& build = Engine::CurrentBuild();

		if (build.motionTrackerBackgroundMaterial == 0)
		{
			return false;
		}

		if (!TryPatchCallGroup(build.R_AddCode3DQuadSurfCalls, 3, reinterpret_cast<const void*>(&AddQuadDetour),
				reinterpret_cast<void**>(&originalAddQuad)))
		{
			LogWarn() << "remix: the motion tracker quad calls did not match, its blips stay unclipped";
			return false;
		}

		if (!TryPatchCallGroup(build.R_TessMarkSurfListCalls, 2, reinterpret_cast<const void*>(&TessMarksDetour),
				reinterpret_cast<void**>(&originalTessMarks)))
		{
			LogWarn() << "remix: the code mesh draw calls did not match, the motion tracker overlay stays a world draw";
			return false;
		}

		Log() << "remix: heartbeat sensor overlay clipped to its screen on the CPU (Remix has no stencil) and drawn in"
				 " the viewmodel depth range so it stays on the device";

		return true;
	}
}
