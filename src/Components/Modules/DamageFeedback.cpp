#include "STDInclude.hpp"

#include "DamageFeedback.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Scheduler.hpp"
#include "ServerCommands.hpp"
#include "GSC/Script.hpp"

namespace Components
{
	Dvar::Var DamageFeedback::cg_hitmarkerStyle;
	Dvar::Var DamageFeedback::cg_hitmarkerScale;

	constexpr std::uintptr_t ScrPlace_GetActivePlacement = 0x1400F2B90;
	constexpr std::uintptr_t R_AddCmdDrawQuadPic = 0x14001CCE0;
	constexpr std::uintptr_t s_renderCmdBufferSize = 0x1491458B0;
	constexpr std::uintptr_t s_cmdList = 0x1491458B8;

	constexpr char hitmarkerServerCommand = 22;
	constexpr std::string_view hitmarkerSubcommand = "hitmarker";

	constexpr std::int64_t renderCmdTail = 0x2000;
	constexpr std::int64_t overlayCmdReserve = 0x2000;
	constexpr std::int64_t quadCmdSize = 0x38;

	constexpr int styleClassic = 0;
	constexpr int styleSmooth = 2;

	constexpr float keepHigherSeconds = 0.12f;
	constexpr float referenceHeight = 1080.0f;
	constexpr float minTickPixels = 1.5f;
	constexpr float shadowPixels = 2.0f;
	constexpr float shadowAlpha = 0.75f;
	constexpr float featherPixels = 1.5f;
	constexpr float featherAlphas[] = { 0.18f, 0.35f, 1.0f };
	constexpr float diagonal = 0.70710678f;
	constexpr float pairSpread = 0.12f;
	constexpr float minPairGapPixels = 1.0f;

	constexpr float classicSeconds = 1.0f;
	constexpr float classicOffset = 12.0f;
	constexpr float classicWidth = 24.0f;
	constexpr float classicHeight = 48.0f;
	constexpr float smoothStartScale = 1.5f;
	constexpr float smoothShrinkSeconds = 0.15f;

	constexpr float classicRedColor[3] = { 1.0f, 0.18f, 0.15f };

	using ScrPlace_GetActivePlacement_t = const float*(*)(int localClientNum);
	using R_AddCmdDrawQuadPic_t = void(*)(const float (*verts)[2], const float* color, Game::Material* material);

	using Clock = std::chrono::steady_clock;

	struct Style
	{
		std::string_view name;
		float color[3];
		float startInner;
		float startOuter;
		float endInner;
		float endOuter;
		float thickness;
		int lines;
		float settleSeconds;
		float holdSeconds;
		float fadeSeconds;
		int priority;
		const char* classicMaterial;
	};

	static const Style styles[] =
	{
		{ "hit", { 1.0f, 1.0f, 1.0f }, 19.0f, 36.0f, 12.0f, 23.0f, 2.0f, 1, 0.067f, 0.133f, 0.15f, 0, "damage_feedback" },
		{ "armor", { 1.0f, 1.0f, 1.0f }, 19.0f, 36.0f, 12.0f, 23.0f, 2.0f, 1, 0.067f, 0.133f, 0.15f, 1, "damage_feedback_j" },
		{ "headshot", { 1.0f, 0.82f, 0.12f }, 20.0f, 38.0f, 12.0f, 23.0f, 2.0f, 2, 0.1f, 0.1f, 0.18f, 2, "damage_feedback" },
		{ "kill", { 1.0f, 0.17f, 0.12f }, 24.0f, 44.0f, 12.0f, 24.0f, 2.0f, 2, 0.1f, 0.12f, 0.16f, 3, "damage_feedback" },
		{ "headshotkill", { 1.0f, 0.17f, 0.12f }, 26.0f, 46.0f, 12.0f, 24.0f, 2.5f, 2, 0.1f, 0.12f, 0.16f, 4, "damage_feedback" },
	};

	struct Marker
	{
		const Style* style = nullptr;
		Clock::time_point start{};
	};

	static Marker marker;

	static const Style* FindStyle(const std::string_view name)
	{
		for (const auto& style : styles)
		{
			if (style.name == name)
			{
				return &style;
			}
		}

		return &styles[0];
	}

	void DamageFeedback::Scr_PlayHitmarker(const Game::scr_entref_t entref)
	{
		if (entref.classnum || entref.entnum >= Game::MAX_CLIENTS)
		{
			GSC::Script::Scr_ObjectError("not a player");
			return;
		}

		if (Game::Scr_GetNumParam() > 1)
		{
			GSC::Script::Scr_Error("usage: <player> PlayHitmarker( [type] )\n");
			return;
		}

		std::string_view type = styles[0].name;

		if (Game::Scr_GetNumParam() == 1 && Game::Scr_GetType(0) != Game::VAR_UNDEFINED)
		{
			if (Game::Scr_GetType(0) != Game::VAR_STRING)
			{
				GSC::Script::Scr_ParamError(0, "type must be a string");
				return;
			}

			type = FindStyle(Game::Scr_GetString(0))->name;
		}

		const int clientNum = static_cast<int>(entref.entnum);

		if (clientNum >= *Game::svs_clientCount || Game::svs_clients[clientNum].header.state < Game::CS_ACTIVE || Game::svs_clients[clientNum].bIsTestClient)
		{
			return;
		}

		Game::SV_GameSendServerCommand(clientNum, Game::SV_CMD_CAN_IGNORE, Utils::String::Format("{:c} {} {}", hitmarkerServerCommand, hitmarkerSubcommand, type));
	}

	bool DamageFeedback::OnHitmarkerCommand(const Command::Params* params)
	{
		if (params->Size() < 2 || params->Get(1) != hitmarkerSubcommand)
		{
			return false;
		}

		const auto* const style = FindStyle(params->Size() >= 3 ? params->Get(2) : "");
		const auto now = Clock::now();

		if (marker.style && marker.style->priority > style->priority && now - marker.start < std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(keepHigherSeconds)))
		{
			return true;
		}

		marker.style = style;
		marker.start = now;
		return true;
	}

	static std::int64_t FreeRenderCmdBytes()
	{
		const auto bufferSize = static_cast<std::int64_t>(*reinterpret_cast<const int*>(Utils::Hook::Rebase(s_renderCmdBufferSize)));
		const auto* const cmdList = *reinterpret_cast<const std::int64_t* const*>(Utils::Hook::Rebase(s_cmdList));
		return bufferSize - renderCmdTail + cmdList[2] - cmdList[1];
	}

	static void DrawSegment(const R_AddCmdDrawQuadPic_t drawQuad, Game::Material* material, const float* from, const float* to, const float thickness, const float* color)
	{
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float length = std::sqrt(dx * dx + dy * dy);

		if (length < 0.001f)
		{
			return;
		}

		const float offsetX = -dy / length * thickness * 0.5f;
		const float offsetY = dx / length * thickness * 0.5f;
		const float corners[4][2] =
		{
			{ from[0] - offsetX, from[1] - offsetY },
			{ to[0] - offsetX, to[1] - offsetY },
			{ to[0] + offsetX, to[1] + offsetY },
			{ from[0] + offsetX, from[1] + offsetY },
		};

		drawQuad(corners, color, material);
	}


	static void DrawSoftSegment(const R_AddCmdDrawQuadPic_t drawQuad, Game::Material* material, const float* from, const float* to, const float thickness, const float feather, const float* color)
	{
		const float dx = to[0] - from[0];
		const float dy = to[1] - from[1];
		const float length = std::sqrt(dx * dx + dy * dy);

		if (length < 0.001f)
		{
			return;
		}

		const float dirX = dx / length;
		const float dirY = dy / length;
		const auto layers = static_cast<int>(std::size(featherAlphas));

		for (int layer = 0; layer < layers; ++layer)
		{
			const float extra = feather * static_cast<float>(layers - 1 - layer) / static_cast<float>(layers - 1);
			const float start[2] = { from[0] - dirX * extra, from[1] - dirY * extra };
			const float end[2] = { to[0] + dirX * extra, to[1] + dirY * extra };
			const float layerColor[4] = { color[0], color[1], color[2], color[3] * featherAlphas[layer] };

			DrawSegment(drawQuad, material, start, end, thickness + extra * 2.0f, layerColor);
		}
	}

	void DamageFeedback::Draw()
	{
		if (!marker.style || !Game::CL_IsCgameInitialized(0))
		{
			return;
		}

		const auto& style = *marker.style;
		const int styleIndex = cg_hitmarkerStyle.Get<int>();
		const bool isClassic = styleIndex == styleClassic || styleIndex == styleSmooth;
		const float age = std::chrono::duration<float>(Clock::now() - marker.start).count();
		const float lifetime = isClassic ? classicSeconds : style.settleSeconds + style.holdSeconds + style.fadeSeconds;

		if (age >= lifetime)
		{
			marker.style = nullptr;
			return;
		}

		if (FreeRenderCmdBytes() - overlayCmdReserve < quadCmdSize * 48)
		{
			return;
		}

		const auto* const placement = reinterpret_cast<ScrPlace_GetActivePlacement_t>(Utils::Hook::Rebase(ScrPlace_GetActivePlacement))(0);
		const auto drawQuad = reinterpret_cast<R_AddCmdDrawQuadPic_t>(Utils::Hook::Rebase(R_AddCmdDrawQuadPic));
		const auto& refdef = Game::CL_GetLocalClientGlobals(0)->refdef;
		const float centerX = static_cast<float>(refdef.x) + static_cast<float>(refdef.width) * 0.5f;
		const float centerY = static_cast<float>(refdef.y) + static_cast<float>(refdef.height) * 0.5f;

		if (isClassic)
		{
			const bool isSmooth = styleIndex == styleSmooth;
			const bool isKill = style.priority >= FindStyle("kill")->priority;
			const bool isTinted = !isSmooth || isKill;
			const float scale = isSmooth ? smoothStartScale + (1.0f - smoothStartScale) * std::min(age / smoothShrinkSeconds, 1.0f) : 1.0f;
			const float alpha = 1.0f - age / classicSeconds;
			const float* const tint = isTinted ? classicRedColor : nullptr;
			const float color[4] = { tint ? tint[0] : 1.0f, tint ? tint[1] : 1.0f, tint ? tint[2] : 1.0f, alpha };
			const float left = centerX - classicOffset * scale * placement[0];
			const float top = centerY - classicOffset * scale * placement[1];
			const float right = left + classicWidth * scale * placement[0];
			const float bottom = top + classicHeight * scale * placement[1];
			const float corners[4][2] = { { left, top }, { right, top }, { right, bottom }, { left, bottom } };

			drawQuad(corners, color, Game::Material_RegisterHandle(style.classicMaterial, 7));
			return;
		}

		const float settle = std::clamp(age / style.settleSeconds, 0.0f, 1.0f);
		const float alpha = age < style.settleSeconds + style.holdSeconds ? 1.0f : 1.0f - (age - style.settleSeconds - style.holdSeconds) / style.fadeSeconds;
		const float unit = static_cast<float>(refdef.height) / referenceHeight * cg_hitmarkerScale.Get<float>();
		const float inner = (style.startInner + (style.endInner - style.startInner) * settle) * unit;
		const float outer = (style.startOuter + (style.endOuter - style.startOuter) * settle) * unit;
		const float thickness = std::max(style.thickness * unit, minTickPixels);
		const float pairGap = std::max(minPairGapPixels * unit, minPairGapPixels);
		const float feather = std::max(featherPixels * unit, 1.0f);
		const float spread = style.lines > 1 ? std::max(inner * pairSpread, (thickness + pairGap) * 0.5f) : 0.0f;
		const float shadow[4] = { 0.0f, 0.0f, 0.0f, shadowAlpha * alpha };
		const float color[4] = { style.color[0], style.color[1], style.color[2], alpha };
		auto* const white = Game::Material_RegisterHandle("white", 7);

		for (int pass = 0; pass < 2; ++pass)
		{
			for (const float sx : { -1.0f, 1.0f })
			{
				for (const float sy : { -1.0f, 1.0f })
				{
					const float sideX = -sy * diagonal;
					const float sideY = sx * diagonal;

					for (int line = 0; line < style.lines; ++line)
					{
						const float offset = style.lines > 1 ? (line == 0 ? -spread : spread) : 0.0f;
						const float from[2] = { centerX + sx * diagonal * inner + sideX * offset, centerY + sy * diagonal * inner + sideY * offset };
						const float to[2] = { centerX + sx * diagonal * outer + sideX * offset, centerY + sy * diagonal * outer + sideY * offset };

						if (pass == 0)
						{
							DrawSoftSegment(drawQuad, white, from, to, thickness + shadowPixels, feather, shadow);
						}
						else
						{
							DrawSoftSegment(drawQuad, white, from, to, thickness, feather, color);
						}
					}
				}
			}
		}
	}
	DamageFeedback::DamageFeedback()
	{
		GSC::Script::AddMethod("PlayHitmarker", Scr_PlayHitmarker);

		if (Dedicated::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			cg_hitmarkerStyle = Dvar::Register("cg_hitmarkerStyle", 0, 0, 2, Game::DVAR_ARCHIVE, "Hitmarker style: 0 classic, 1 modern, 2 smooth");
			cg_hitmarkerScale = Dvar::Register("cg_hitmarkerScale", 1.0f, 0.25f, 4.0f, Game::DVAR_ARCHIVE, "Size of the modern hitmarker");
		});

		ServerCommands::OnCommand(hitmarkerServerCommand, OnHitmarkerCommand);
		Scheduler::Loop(Draw, Scheduler::Pipeline::RENDERER);

		Events::OnCGameInit([]
		{
			marker = {};
		});
	}
}
