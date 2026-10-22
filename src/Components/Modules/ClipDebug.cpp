#include "STDInclude.hpp"

#include "ClipDebug.hpp"
#include "ClipMap.hpp"
#include "CollmapCollision.hpp"
#include "CrateCollision.hpp"
#include "Events.hpp"
#include "Scheduler.hpp"

namespace Components
{
	using namespace ClipMap;

	constexpr std::uintptr_t CG_WorldPosToScreenPosReal = 0x1400A8040;
	constexpr std::uintptr_t ScrPlace_GetActivePlacement = 0x1400F2B90;
	constexpr std::uintptr_t cg_refdefViewOrigin = 0x1404E1158;
	constexpr std::uintptr_t R_AddCmdDrawQuadPic = 0x14001CCE0;
	constexpr std::uintptr_t s_renderCmdBufferSize = 0x1491458B0;
	constexpr std::uintptr_t s_cmdList = 0x1491458B8;

	constexpr std::int64_t renderCmdTail = 0x2000;
	constexpr std::int64_t overlayCmdReserve = 0x2000;
	constexpr std::int64_t quadCmdSize = 0x38;
	constexpr std::int64_t textCmdBase = 96;
	constexpr int labelChars = 48;

	using CG_WorldPosToScreenPosReal_t = bool(*)(int localClientNum, const void* placement, const float* worldPos, float* outScreenPos);
	using ScrPlace_GetActivePlacement_t = const void*(*)(int localClientNum);
	using R_AddCmdDrawQuadPic_t = void(*)(const float (*verts)[2], const float* color, Game::Material* material);

	constexpr int brushSolid = 0xFFFFFF;
	constexpr int nonSolidBrushModel = 1;
	constexpr float lineThickness = 1.5f;
	constexpr float labelRange = 600.0f;

	struct Color
	{
		float r;
		float g;
		float b;
		float a;
	};

	constexpr Color colorBox = { 0.3f, 1.0f, 0.3f, 1.0f };
	constexpr Color colorCrate = { 0.2f, 0.9f, 1.0f, 1.0f };
	constexpr Color colorBulletsOnly = { 0.6f, 0.6f, 0.6f, 0.7f };
	constexpr Color colorZombie = { 1.0f, 0.55f, 0.1f, 1.0f };
	constexpr Color colorCollmap = { 1.0f, 0.3f, 1.0f, 1.0f };
	constexpr Color colorMapBrush = { 1.0f, 1.0f, 0.3f, 0.8f };
	constexpr Color colorModelBox = { 1.0f, 0.3f, 0.3f, 1.0f };

	static const Game::dvar_t* debug_clips = nullptr;
	static const Game::dvar_t* debug_clips_range = nullptr;

	struct Frame
	{
		const void* placement;
		CG_WorldPosToScreenPosReal_t worldToScreen;
		R_AddCmdDrawQuadPic_t drawQuad;
		Game::Material* white;
		Game::Font_s* font;
		float textHeight;
		const float* camera;
		std::int64_t budget;
	};

	static std::int64_t FreeRenderCmdBytes()
	{
		const auto bufferSize = static_cast<std::int64_t>(*reinterpret_cast<const int*>(Utils::Hook::Rebase(s_renderCmdBufferSize)));
		const auto* const cmdList = *reinterpret_cast<const std::int64_t* const*>(Utils::Hook::Rebase(s_cmdList));
		return bufferSize - renderCmdTail + cmdList[2] - cmdList[1];
	}

	static void DrawLine(Frame& frame, const float* from, const float* to, const Color& color)
	{
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float length = std::sqrt(dx * dx + dy * dy);

		if (length < 0.001f || frame.budget < quadCmdSize)
		{
			return;
		}

		const float offsetX = -dy / length * lineThickness * 0.5f;
		const float offsetY = dx / length * lineThickness * 0.5f;
		const float corners[4][2] =
		{
			{ from[0] - offsetX, from[1] - offsetY },
			{ to[0] - offsetX, to[1] - offsetY },
			{ to[0] + offsetX, to[1] + offsetY },
			{ from[0] + offsetX, from[1] + offsetY },
		};

		frame.drawQuad(corners, &color.r, frame.white);
		frame.budget -= quadCmdSize;
	}

	static void DrawLabel(Frame& frame, const float* screen, const Color& color, const char* text)
	{
		const auto cost = static_cast<std::int64_t>((std::strlen(text) + textCmdBase) & ~std::size_t{ 3 });

		if (cost > frame.budget)
		{
			return;
		}

		frame.budget -= cost;
		Game::R_AddCmdDrawText(text, labelChars, frame.font, screen[0], screen[1] + frame.textHeight, 1.0f, 1.0f, 0.0f, &color.r, 0);
	}

	static void DrawBox(Frame& frame, const float* origin, const float* angles, const Game::Bounds& bounds, const Color& color, const char* label)
	{
		constexpr float toRadians = 3.14159265f / 180.0f;
		const float sp = std::sin(angles[0] * toRadians), cp = std::cos(angles[0] * toRadians);
		const float sy = std::sin(angles[1] * toRadians), cy = std::cos(angles[1] * toRadians);
		const float sr = std::sin(angles[2] * toRadians), cr = std::cos(angles[2] * toRadians);

		const float forward[3] = { cp * cy, cp * sy, -sp };
		const float right[3] = { -sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp };
		const float up[3] = { cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp };

		float screen[8][2]{};
		bool isOnScreen[8]{};

		for (int corner = 0; corner < 8; ++corner)
		{
			const float local[3] =
			{
				bounds.midPoint[0] + ((corner & 1) ? bounds.halfSize[0] : -bounds.halfSize[0]),
				bounds.midPoint[1] + ((corner & 2) ? bounds.halfSize[1] : -bounds.halfSize[1]),
				bounds.midPoint[2] + ((corner & 4) ? bounds.halfSize[2] : -bounds.halfSize[2]),
			};

			float world[3]{};

			for (int i = 0; i < 3; ++i)
			{
				world[i] = origin[i] + forward[i] * local[0] - right[i] * local[1] + up[i] * local[2];
			}

			isOnScreen[corner] = frame.worldToScreen(0, frame.placement, world, screen[corner]);
		}

		for (int corner = 0; corner < 8; ++corner)
		{
			for (const int axis : { 1, 2, 4 })
			{
				const int other = corner | axis;

				if (other != corner && isOnScreen[corner] && isOnScreen[other])
				{
					DrawLine(frame, screen[corner], screen[other], color);
				}
			}
		}

		if (!label)
		{
			return;
		}

		const float dx = origin[0] - frame.camera[0];
		const float dy = origin[1] - frame.camera[1];
		const float dz = origin[2] - frame.camera[2];

		if (dx * dx + dy * dy + dz * dz < labelRange * labelRange && isOnScreen[7])
		{
			DrawLabel(frame, screen[7], color, label);
		}
	}

	static const char* SizeLabel(const char* kind, const Game::Bounds& bounds)
	{
		return Utils::String::VA("%s %.0fx%.0fx%.0f", kind, bounds.halfSize[0] * 2.0f, bounds.halfSize[1] * 2.0f, bounds.halfSize[2] * 2.0f);
	}

	static const Game::Bounds* SubModelBounds(const unsigned int model)
	{
		const auto* const cmodels = Utils::Hook::Get<const cmodel_t*>(cm_cmodels);

		if (!cmodels || model >= Utils::Hook::Get<unsigned int>(cm_numSubModels))
		{
			return nullptr;
		}

		return &cmodels[model].bounds;
	}

	static Game::Bounds DecodeSolid(const int solid)
	{
		const float halfWidth = static_cast<float>(solid & 0xFF) - 1.0f;
		const float bottom = 1.0f - (static_cast<float>((solid >> 8) & 0xFF) - 1.0f);
		const float top = static_cast<float>((solid >> 16) & 0xFF) - 33.0f;

		Game::Bounds box{};
		box.midPoint[2] = (bottom + top) * 0.5f;
		box.halfSize[0] = halfWidth;
		box.halfSize[1] = halfWidth;
		box.halfSize[2] = (top - bottom) * 0.5f;
		return box;
	}

	void ClipDebug::Draw()
	{
		if (!debug_clips || !debug_clips->current.integer || !Game::CL_IsCgameInitialized(0))
		{
			return;
		}

		Frame frame{};
		frame.placement = reinterpret_cast<ScrPlace_GetActivePlacement_t>(Utils::Hook::Rebase(ScrPlace_GetActivePlacement))(0);
		frame.worldToScreen = reinterpret_cast<CG_WorldPosToScreenPosReal_t>(Utils::Hook::Rebase(CG_WorldPosToScreenPosReal));
		frame.drawQuad = reinterpret_cast<R_AddCmdDrawQuadPic_t>(Utils::Hook::Rebase(R_AddCmdDrawQuadPic));
		frame.white = Game::Material_RegisterHandle("white", 7);
		frame.font = Game::R_RegisterFont("fonts/smallFont", 0);
		frame.textHeight = static_cast<float>(Game::R_TextHeight(frame.font));
		frame.camera = reinterpret_cast<const float*>(Utils::Hook::Rebase(cg_refdefViewOrigin));
		frame.budget = FreeRenderCmdBytes() - overlayCmdReserve;

		const bool isShowingMapBrushes = debug_clips->current.integer >= 2;
		const float range = static_cast<float>(debug_clips_range ? debug_clips_range->current.integer : 2000);
		const auto crate = CrateCollision::GetIndex();
		const auto* const entities = reinterpret_cast<const Game::centity_s*>(Utils::Hook::Rebase(cg_entities));

		for (std::size_t i = 0; i < Game::MAX_GENTITIES && frame.budget > 0; ++i)
		{
			const auto& cent = entities[i];
			const auto& es = cent.nextState;

			if (!cent.nextValid || !es.solid || es.eType == Game::ET_PLAYER)
			{
				continue;
			}

			const float dx = cent.pose.origin[0] - frame.camera[0];
			const float dy = cent.pose.origin[1] - frame.camera[1];
			const float dz = cent.pose.origin[2] - frame.camera[2];

			if (dx * dx + dy * dy + dz * dz > range * range)
			{
				continue;
			}

			if (es.solid == brushSolid)
			{
				const auto model = static_cast<unsigned int>(es.index);
				const auto* const bounds = SubModelBounds(model);

				if (!bounds)
				{
					continue;
				}

				const bool isBulletsOnly = (es.lerp.eFlags & nonSolidBrushModel) != 0;

				if (CollmapCollision::IsBox(model))
				{
					DrawBox(frame, cent.pose.origin, cent.pose.angles, *bounds, isBulletsOnly ? colorBulletsOnly : colorBox, SizeLabel("box", *bounds));
				}
				else if (crate && model == crate)
				{
					DrawBox(frame, cent.pose.origin, cent.pose.angles, *bounds, isBulletsOnly ? colorBulletsOnly : colorCrate, isBulletsOnly ? "hitbox" : "crate");
				}
				else if (isShowingMapBrushes)
				{
					DrawBox(frame, cent.pose.origin, cent.pose.angles, *bounds, colorMapBrush, Utils::String::VA("*%u", model));
				}

				continue;
			}

			if (const auto collmap = CollmapCollision::ClientCollmapModel(&cent))
			{
				if (const auto* const bounds = SubModelBounds(collmap))
				{
					DrawBox(frame, cent.pose.origin, cent.pose.angles, *bounds, colorCollmap, "collmap");
				}

				continue;
			}

			const auto box = DecodeSolid(es.solid);

			if (box.halfSize[0] < 1.0f || box.halfSize[2] <= 0.0f)
			{
				continue;
			}

			constexpr float noAngles[3]{};
			const bool isScriptModel = es.eType == Game::ET_SCRIPTMOVER || es.eType == Game::ET_GENERAL;
			DrawBox(frame, cent.pose.origin, noAngles, box, isScriptModel ? colorZombie : colorModelBox, SizeLabel("solid", box));
		}
	}

	ClipDebug::ClipDebug()
	{
		Events::OnDvarInit([]
		{
			debug_clips = Game::Dvar_RegisterInt("debug_clips", 0, 0, 2, Game::DVAR_NONE,
				"0 off; 1 collision boxes (green), crates (cyan), bullet-only hitboxes (grey), zombies (orange), collmaps (magenta) and other solid models (red); "
				"2 the map's own brush models too (yellow)");
			debug_clips_range = Game::Dvar_RegisterInt("debug_clips_range", 2000, 100, 20000, Game::DVAR_NONE, "how far from the camera debug_clips draws, in units");
		});

		Scheduler::Loop(Draw, Scheduler::Pipeline::RENDERER);
	}
}
