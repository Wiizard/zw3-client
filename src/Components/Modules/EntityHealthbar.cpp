#include "STDInclude.hpp"

#include "EntityHealthbar.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Scheduler.hpp"
#include "ServerCommands.hpp"
#include "GSC/Script.hpp"

namespace Components
{
	Dvar::Var EntityHealthbar::cg_drawHealthbars;
	Dvar::Var EntityHealthbar::cg_healthbarMaxDist;
	Dvar::Var EntityHealthbar::cg_healthbarAimWidth;
	Dvar::Var EntityHealthbar::cg_healthbarAimAngle;

	constexpr std::uintptr_t Scr_GetEntity = 0x1401A9A00;
	constexpr std::uintptr_t CG_WorldPosToScreenPosReal = 0x1400A8040;
	constexpr std::uintptr_t ScrPlace_GetActivePlacement = 0x1400F2B90;
	constexpr std::uintptr_t CG_DObjGetWorldTagPos = 0x1400D40B0;
	constexpr std::uintptr_t CG_RelativeTeamColor = 0x1400A7D00;
	constexpr std::uintptr_t CG_LocationalTrace = 0x1400CA0C0;
	constexpr std::uintptr_t R_AddCmdDrawQuadPic = 0x14001CCE0;
	constexpr std::uintptr_t cg_entities = 0x14059EB20;
	constexpr std::uintptr_t cg_clientNum = 0x140479D10;
	constexpr std::uintptr_t scr_const_j_head = 0x1417CC48C;
	constexpr std::uintptr_t s_renderCmdBufferSize = 0x1491458B0;
	constexpr std::uintptr_t s_cmdList = 0x1491458B8;

	constexpr char healthbarServerCommand = 22;
	constexpr std::string_view healthbarSubcommand = "healthbar";
	constexpr std::size_t commandTextLimit = 900;
	constexpr std::size_t maxNameLength = 24;

	constexpr int noOwner = -1;
	constexpr int fullFill = 1000;

	constexpr std::int64_t renderCmdTail = 0x2000;
	constexpr std::int64_t overlayCmdReserve = 0x2000;
	constexpr std::int64_t quadCmdSize = 0x38;
	constexpr std::int64_t textCmdBase = 96;

	constexpr int contentsSolid = 1;
	constexpr float sightSlack = 8.0f;
	constexpr float headLift = 14.0f;
	constexpr float originLift = 85.0f;
	constexpr float nearestDistance = 16.0f;
	constexpr float aimSampleLift = 16.0f;
	constexpr std::size_t maxShown = 24;

	constexpr float fadeInSeconds = 0.5f;
	constexpr float fadeOutSeconds = 0.8f;
	constexpr float chipHoldSeconds = 0.35f;
	constexpr float chipDrainPerSecond = 1200.0f;

	constexpr float barWidth = 52.0f;
	constexpr float barHeight = 2.2f;
	constexpr float tickWidth = 0.7f;
	constexpr float tickOverhang = 1.1f;
	constexpr float nameHeight = 7.0f;
	constexpr float nameGap = 1.4f;

	constexpr float defaultColor[3] = { 0.95f, 0.34f, 0.29f };
	constexpr float trackColor[4] = { 0.1f, 0.1f, 0.1f, 0.55f };
	constexpr float outlineColor[4] = { 0.0f, 0.0f, 0.0f, 0.85f };
	constexpr float chipColor[4] = { 1.0f, 1.0f, 1.0f, 0.95f };
	constexpr float tickColor[4] = { 1.0f, 0.82f, 0.18f, 1.0f };

	using CG_WorldPosToScreenPosReal_t = bool(*)(int localClientNum, const void* placement, const float* worldPos, float* outScreenPos);
	using ScrPlace_GetActivePlacement_t = const float*(*)(int localClientNum);
	using CG_DObjGetWorldTagPos_t = int(*)(const Game::cpose_t* pose, Game::DObj* obj, unsigned int tagName, float* pos);
	using CG_RelativeTeamColor_t = void(*)(int localClientNum, int clientNum, const char* prefix, float* color);
	using CG_LocationalTrace_t = void(*)(Game::trace_t* trace, const float* start, const float* end, int passEntityNum, int contentMask);
	using R_AddCmdDrawQuadPic_t = void(*)(const float (*verts)[2], const float* color, Game::Material* material);

	using Clock = std::chrono::steady_clock;

	struct Healthbar
	{
		int owner = noOwner;
		int fill = fullFill;
		bool hasColor = false;
		float color[3]{};
		std::string name;

		bool operator==(const Healthbar&) const = default;
	};

	struct Chip
	{
		float start = 0.0f;
		Clock::time_point holdUntil{};
	};

	struct Candidate
	{
		float distanceSquared;
		float screen[2];
		float alpha;
		float color[3];
		float fill;
		float chip;
		const std::string* name;
	};

	static std::map<int, Healthbar> serverBars;
	static std::set<int> changedEntities;
	static bool isClientReplayed[Game::MAX_CLIENTS];

	static std::unordered_map<int, Healthbar> clientBars;
	static std::unordered_map<int, Chip> clientChips;
	struct Visibility
	{
		float alpha = 0.0f;
		Clock::time_point updated{};
	};

	static std::unordered_map<int, Visibility> visibility;

	static void ClearClientBars()
	{
		clientBars.clear();
		clientChips.clear();
		visibility.clear();
	}

	static std::string CleanName(const char* text)
	{
		std::string name;

		for (const char* c = text; *c && name.size() < maxNameLength; ++c)
		{
			if (static_cast<unsigned char>(*c) >= ' ' && *c != '"' && *c != '\\' && *c != ',')
			{
				name += *c;
			}
		}

		return name;
	}

	static void AppendEntry(std::string& entry, const int entNum, const Healthbar* bar)
	{
		if (!bar)
		{
			entry = Utils::String::Format("x {}", entNum);
			return;
		}

		const auto color = bar->hasColor ? Utils::String::Format("{:.3f},{:.3f},{:.3f}", bar->color[0], bar->color[1], bar->color[2]) : std::string("-");
		entry = Utils::String::Format("b {} {} {} {} \"{}\"", entNum, bar->owner, bar->fill, color, bar->name);
	}

	static void SendEntries(const int clientNum, const std::vector<std::string>& entries)
	{
		std::string text;

		const auto flush = [&]
		{
			if (text.empty())
			{
				return;
			}

			Game::SV_GameSendServerCommand(clientNum, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} {}{}", healthbarServerCommand, healthbarSubcommand, text));
			text.clear();
		};

		for (const auto& entry : entries)
		{
			if (text.size() + entry.size() + 1 > commandTextLimit)
			{
				flush();
			}

			text += ' ';
			text += entry;
		}

		flush();
	}

	static void SetServerBar(const int entNum, const std::optional<Healthbar>& bar)
	{
		const auto found = serverBars.find(entNum);

		if (!bar)
		{
			if (found == serverBars.end())
			{
				return;
			}

			serverBars.erase(found);
			changedEntities.insert(entNum);
			return;
		}

		if (found != serverBars.end() && found->second == *bar)
		{
			return;
		}

		serverBars[entNum] = *bar;
		changedEntities.insert(entNum);
	}

	void EntityHealthbar::Scr_SetModelHealthbar(const Game::scr_entref_t entref)
	{
		if (entref.classnum)
		{
			GSC::Script::Scr_ObjectError("not an entity");
			return;
		}

		const int entNum = entref.entnum;
		const auto numParam = Game::Scr_GetNumParam();

		if (numParam == 0)
		{
			SetServerBar(entNum, std::nullopt);
			return;
		}

		if (numParam < 3 || numParam > 5)
		{
			GSC::Script::Scr_Error("usage: <entity> SetModelHealthbar( <owner>, <health>, <max health>, [color], [name] )\n");
			return;
		}

		Healthbar bar{};

		if (Game::Scr_GetType(0) != Game::VAR_UNDEFINED)
		{
			const auto* const owner = Utils::Hook::Call<Game::gentity_s*(unsigned int)>(Scr_GetEntity)(0);

			if (owner->s.number < 0 || owner->s.number >= static_cast<int>(Game::MAX_CLIENTS))
			{
				GSC::Script::Scr_ParamError(0, "owner must be a player or undefined");
				return;
			}

			bar.owner = owner->s.number;
		}

		const float health = Game::Scr_GetFloat(1);
		const float maxHealth = Game::Scr_GetFloat(2);

		if (!std::isfinite(health) || !std::isfinite(maxHealth) || maxHealth <= 0.0f)
		{
			GSC::Script::Scr_ParamError(2, "max health must be above zero");
			return;
		}

		const float fraction = std::clamp(health / maxHealth, 0.0f, 1.0f);
		bar.fill = static_cast<int>(std::ceil(fraction * static_cast<float>(fullFill)));

		if (numParam >= 4 && Game::Scr_GetType(3) != Game::VAR_UNDEFINED)
		{
			if (Game::Scr_GetType(3) != Game::VAR_VECTOR)
			{
				GSC::Script::Scr_ParamError(3, "color must be a vector");
				return;
			}

			float color[3]{};
			Game::Scr_GetVector(3, color);

			bar.hasColor = true;

			for (int i = 0; i < 3; ++i)
			{
				bar.color[i] = std::isfinite(color[i]) ? std::clamp(color[i], 0.0f, 1.0f) : 1.0f;
			}
		}

		if (numParam == 5 && Game::Scr_GetType(4) != Game::VAR_UNDEFINED)
		{
			if (Game::Scr_GetType(4) != Game::VAR_STRING)
			{
				GSC::Script::Scr_ParamError(4, "name must be a string");
				return;
			}

			bar.name = CleanName(Game::Scr_GetString(4));
		}

		SetServerBar(entNum, bar);
	}

	void EntityHealthbar::SendChanges()
	{
		for (auto i = serverBars.begin(); i != serverBars.end();)
		{
			if (Game::g_entities[i->first].r.isInUse)
			{
				++i;
				continue;
			}

			changedEntities.insert(i->first);
			i = serverBars.erase(i);
		}

		if (changedEntities.empty())
		{
			return;
		}

		std::vector<std::string> entries;
		entries.reserve(changedEntities.size());

		for (const auto entNum : changedEntities)
		{
			const auto found = serverBars.find(entNum);
			AppendEntry(entries.emplace_back(), entNum, found == serverBars.end() ? nullptr : &found->second);
		}

		changedEntities.clear();
		SendEntries(-1, entries);
	}

	void EntityHealthbar::ReplayToNewClients()
	{
		const int clientCount = std::clamp(*Game::svs_clientCount, 0, static_cast<int>(Game::MAX_CLIENTS));

		for (int clientNum = 0; clientNum < clientCount; ++clientNum)
		{
			const Game::client_s& client = Game::svs_clients[clientNum];

			if (client.header.state < Game::CS_ACTIVE)
			{
				isClientReplayed[clientNum] = false;
				continue;
			}

			if (isClientReplayed[clientNum])
			{
				continue;
			}

			isClientReplayed[clientNum] = true;

			if (client.bIsTestClient || serverBars.empty())
			{
				continue;
			}

			std::vector<std::string> entries;
			entries.reserve(serverBars.size());

			for (const auto& [entNum, bar] : serverBars)
			{
				AppendEntry(entries.emplace_back(), entNum, &bar);
			}

			SendEntries(clientNum, entries);
		}
	}

	static bool ParseInt(const char* text, int& value)
	{
		const std::string_view view = text;
		return std::from_chars(view.data(), view.data() + view.size(), value).ec == std::errc{};
	}

	static bool ParseColor(const char* text, float* color)
	{
		const std::string_view view = text;
		std::size_t begin = 0;

		for (int i = 0; i < 3; ++i)
		{
			const auto end = i < 2 ? view.find(',', begin) : view.size();

			if (end == std::string_view::npos)
			{
				return false;
			}

			float value = 0.0f;

			if (std::from_chars(view.data() + begin, view.data() + end, value).ec != std::errc{} || !std::isfinite(value))
			{
				return false;
			}

			color[i] = std::clamp(value, 0.0f, 1.0f);
			begin = end + 1;
		}

		return true;
	}

	static float CurrentChip(const Chip& chip, const float fill, const Clock::time_point now)
	{
		if (now < chip.holdUntil)
		{
			return std::max(chip.start, fill);
		}

		const float drained = chip.start - std::chrono::duration<float>(now - chip.holdUntil).count() * chipDrainPerSecond;
		return std::max(drained, fill);
	}

	static void UpdateChip(const int entNum, const int oldFill, const int newFill)
	{
		const auto now = Clock::now();
		auto& chip = clientChips[entNum];

		if (newFill >= oldFill)
		{
			chip.start = std::min(CurrentChip(chip, static_cast<float>(newFill), now), static_cast<float>(newFill));
			chip.holdUntil = now;
			return;
		}

		chip.start = std::max(CurrentChip(chip, static_cast<float>(oldFill), now), static_cast<float>(oldFill));
		chip.holdUntil = now + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<float>(chipHoldSeconds));
	}

	bool EntityHealthbar::OnHealthbarCommand(const Command::Params* params)
	{
		if (params->Size() < 2 || params->Get(1) != healthbarSubcommand)
		{
			return false;
		}

		int index = 2;

		while (index + 1 < params->Size())
		{
			const std::string_view op = params->Get(index);
			int entNum = 0;

			if (!ParseInt(params->Get(index + 1), entNum) || entNum < 0 || entNum >= static_cast<int>(Game::MAX_GENTITIES))
			{
				break;
			}

			if (op == "x")
			{
				clientBars.erase(entNum);
				clientChips.erase(entNum);
				visibility.erase(entNum);
				index += 2;
				continue;
			}

			if (op != "b" || index + 6 > params->Size())
			{
				break;
			}

			Healthbar bar{};

			if (!ParseInt(params->Get(index + 2), bar.owner) || !ParseInt(params->Get(index + 3), bar.fill))
			{
				break;
			}

			bar.owner = bar.owner >= 0 && bar.owner < static_cast<int>(Game::MAX_CLIENTS) ? bar.owner : noOwner;
			bar.fill = std::clamp(bar.fill, 0, fullFill);
			bar.hasColor = ParseColor(params->Get(index + 4), bar.color);
			bar.name = CleanName(params->Get(index + 5));

			const auto previous = clientBars.find(entNum);
			const int oldFill = previous == clientBars.end() ? bar.fill : previous->second.fill;
			UpdateChip(entNum, oldFill, bar.fill);

			clientBars[entNum] = std::move(bar);
			index += 6;
		}

		return true;
	}

	static std::int64_t FreeRenderCmdBytes()
	{
		const auto bufferSize = static_cast<std::int64_t>(*reinterpret_cast<const int*>(Utils::Hook::Rebase(s_renderCmdBufferSize)));
		const auto* const cmdList = *reinterpret_cast<const std::int64_t* const*>(Utils::Hook::Rebase(s_cmdList));
		return bufferSize - renderCmdTail + cmdList[2] - cmdList[1];
	}

	static std::int64_t TextCost(const std::string& text)
	{
		return static_cast<std::int64_t>((text.size() + textCmdBase) & ~std::size_t{ 3 });
	}

	static void DrawRect(const R_AddCmdDrawQuadPic_t drawQuad, Game::Material* material, const float x, const float y, const float width, const float height, const float* color)
	{
		const float corners[4][2] =
		{
			{ x, y },
			{ x + width, y },
			{ x + width, y + height },
			{ x, y + height },
		};

		drawQuad(corners, color, material);
	}

	static bool IsNearAimRay(const float* camera, const float* forward, const float* point, const float halfWidth, const float spread)
	{
		const float offset[3] = { point[0] - camera[0], point[1] - camera[1], point[2] - camera[2] };
		const float along = offset[0] * forward[0] + offset[1] * forward[1] + offset[2] * forward[2];

		if (along <= 0.0f)
		{
			return false;
		}

		const float radius = std::max(halfWidth, along * spread);
		const float lengthSquared = offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2];
		return lengthSquared - along * along <= radius * radius;
	}

	void EntityHealthbar::Draw()
	{
		if (clientBars.empty() || !cg_drawHealthbars.Get<bool>() || !Game::CL_IsCgameInitialized(0))
		{
			return;
		}

		const auto* const placement = reinterpret_cast<ScrPlace_GetActivePlacement_t>(Utils::Hook::Rebase(ScrPlace_GetActivePlacement))(0);
		const auto worldToScreen = reinterpret_cast<CG_WorldPosToScreenPosReal_t>(Utils::Hook::Rebase(CG_WorldPosToScreenPosReal));
		const auto getTagPos = reinterpret_cast<CG_DObjGetWorldTagPos_t>(Utils::Hook::Rebase(CG_DObjGetWorldTagPos));
		const auto teamColor = reinterpret_cast<CG_RelativeTeamColor_t>(Utils::Hook::Rebase(CG_RelativeTeamColor));
		const auto trace = reinterpret_cast<CG_LocationalTrace_t>(Utils::Hook::Rebase(CG_LocationalTrace));
		const auto drawQuad = reinterpret_cast<R_AddCmdDrawQuadPic_t>(Utils::Hook::Rebase(R_AddCmdDrawQuadPic));
		const auto* const entities = reinterpret_cast<const Game::centity_s*>(Utils::Hook::Rebase(cg_entities));
		const auto* const cg = Game::CL_GetLocalClientGlobals(0);
		const float* const camera = cg->refdef.view.org;
		const float* const forward = cg->refdef.view.axis[0];
		const int localClientNum = Utils::Hook::Get<int>(cg_clientNum);
		const auto headTag = Utils::Hook::Get<unsigned short>(scr_const_j_head);
		const auto now = Clock::now();

		const float maxDistance = cg_healthbarMaxDist.Get<float>();
		const float aimHalfWidth = cg_healthbarAimWidth.Get<float>() * 0.5f;
		const float aimSpread = std::tan(cg_healthbarAimAngle.Get<float>() * 0.5f * 3.14159265f / 180.0f);

		std::vector<Candidate> candidates;

		for (const auto& [entNum, bar] : clientBars)
		{
			if (entNum == localClientNum)
			{
				continue;
			}

			const auto& cent = entities[entNum];

			if (!cent.nextValid)
			{
				visibility.erase(entNum);
				continue;
			}

			float head[3]{};
			auto* const dobj = Game::Com_GetClientDObj(entNum, 0);
			const bool hasHead = dobj && headTag && getTagPos(&cent.pose, dobj, headTag, head);

			if (!hasHead)
			{
				head[0] = cent.pose.origin[0];
				head[1] = cent.pose.origin[1];
				head[2] = cent.pose.origin[2] + originLift - headLift;
			}

			const float position[3] = { head[0], head[1], head[2] + headLift };
			const float dx = head[0] - camera[0];
			const float dy = head[1] - camera[1];
			const float dz = head[2] - camera[2];
			const float distanceSquared = dx * dx + dy * dy + dz * dz;

			if (distanceSquared < nearestDistance * nearestDistance || distanceSquared > maxDistance * maxDistance)
			{
				visibility.erase(entNum);
				continue;
			}

			const float feet[3] = { cent.pose.origin[0], cent.pose.origin[1], cent.pose.origin[2] + aimSampleLift };
			const float middle[3] = { (feet[0] + head[0]) * 0.5f, (feet[1] + head[1]) * 0.5f, (feet[2] + head[2]) * 0.5f };
			const bool isAimedAt = IsNearAimRay(camera, forward, head, aimHalfWidth, aimSpread) || IsNearAimRay(camera, forward, middle, aimHalfWidth, aimSpread) || IsNearAimRay(camera, forward, feet, aimHalfWidth, aimSpread);

			auto shown = visibility.find(entNum);

			if (!isAimedAt && shown == visibility.end())
			{
				continue;
			}

			Game::trace_t sight{};
			sight.fraction = 1.0f;
			trace(&sight, camera, head, localClientNum, contentsSolid);

			const float distance = std::sqrt(distanceSquared);

			if (sight.fraction < 1.0f && (1.0f - sight.fraction) * distance > sightSlack)
			{
				if (shown != visibility.end())
				{
					visibility.erase(shown);
				}

				continue;
			}

			if (shown == visibility.end())
			{
				shown = visibility.emplace(entNum, Visibility{ 0.0f, now }).first;
			}

			auto& state = shown->second;
			const float elapsed = std::chrono::duration<float>(now - state.updated).count();
			state.updated = now;
			state.alpha = isAimedAt ? std::min(state.alpha + elapsed / fadeInSeconds, 1.0f) : state.alpha - elapsed / fadeOutSeconds;

			if (state.alpha <= 0.0f && !isAimedAt)
			{
				visibility.erase(shown);
				continue;
			}

			const float alpha = state.alpha;
			Candidate candidate{};

			if (!worldToScreen(0, placement, position, candidate.screen))
			{
				continue;
			}

			candidate.distanceSquared = distanceSquared;
			candidate.alpha = std::clamp(alpha, 0.0f, 1.0f);
			candidate.fill = static_cast<float>(bar.fill);
			candidate.name = &bar.name;

			const auto chip = clientChips.find(entNum);
			candidate.chip = chip == clientChips.end() ? candidate.fill : CurrentChip(chip->second, candidate.fill, now);

			if (bar.hasColor)
			{
				std::copy_n(bar.color, 3, candidate.color);
			}
			else if (bar.owner != noOwner)
			{
				float color[4]{};
				teamColor(0, bar.owner, "g_TeamColor", color);
				std::copy_n(color, 3, candidate.color);
			}
			else
			{
				std::copy_n(defaultColor, 3, candidate.color);
			}

			candidates.push_back(candidate);
		}

		if (candidates.empty())
		{
			return;
		}

		std::ranges::sort(candidates, {}, &Candidate::distanceSquared);

		auto* const white = Game::Material_RegisterHandle("white", 7);
		auto* const font = Game::R_RegisterFont("fonts/objectivefont", 0);
		const float scale = placement[1];
		const float width = std::round(barWidth * scale);
		const float height = std::max(std::round(barHeight * scale), 2.0f);
		const float outline = 1.0f;
		const float tick = std::max(std::round(tickWidth * scale), 2.0f);
		const float overhang = std::max(std::round(tickOverhang * scale), 1.0f);
		const float textScale = nameHeight * scale / static_cast<float>(std::max(Game::R_TextHeight(font), 1));

		auto budget = FreeRenderCmdBytes() - overlayCmdReserve;
		const auto shown = std::min(candidates.size(), maxShown);

		for (std::size_t i = 0; i < shown; ++i)
		{
			const auto& candidate = candidates[i];
			const auto cost = quadCmdSize * 8 + (candidate.name->empty() ? 0 : TextCost(*candidate.name));

			if (budget < cost)
			{
				break;
			}

			budget -= cost;

			const float alpha = candidate.alpha;
			const float left = std::floor(candidate.screen[0] - width * 0.5f);
			const float top = std::floor(candidate.screen[1] - height);
			const float fillWidth = std::round(width * candidate.fill / static_cast<float>(fullFill));
			const float chipWidth = std::round(width * candidate.chip / static_cast<float>(fullFill));

			const float track[4] = { trackColor[0], trackColor[1], trackColor[2], trackColor[3] * alpha };
			const float edge[4] = { outlineColor[0], outlineColor[1], outlineColor[2], outlineColor[3] * alpha };
			const float fill[4] = { candidate.color[0], candidate.color[1], candidate.color[2], alpha };
			const float chip[4] = { chipColor[0], chipColor[1], chipColor[2], chipColor[3] * alpha };
			const float marker[4] = { tickColor[0], tickColor[1], tickColor[2], tickColor[3] * alpha };

			DrawRect(drawQuad, white, left, top, width, height, track);
			DrawRect(drawQuad, white, left - outline, top - outline, width + outline * 2.0f, outline, edge);
			DrawRect(drawQuad, white, left - outline, top + height, width + outline * 2.0f, outline, edge);
			DrawRect(drawQuad, white, left - outline, top, outline, height, edge);
			DrawRect(drawQuad, white, left + width, top, outline, height, edge);

			if (chipWidth > fillWidth)
			{
				DrawRect(drawQuad, white, left + fillWidth, top, chipWidth - fillWidth, height, chip);
			}

			if (fillWidth > 0.0f)
			{
				DrawRect(drawQuad, white, left, top, fillWidth, height, fill);
			}

			if (chipWidth > fillWidth)
			{
				DrawRect(drawQuad, white, left + fillWidth - std::floor(tick * 0.5f), top - overhang, tick, height + overhang * 2.0f, marker);
			}

			if (!candidate.name->empty())
			{
				const float nameColor[4] = { candidate.color[0], candidate.color[1], candidate.color[2], alpha };
				Game::R_AddCmdDrawText(candidate.name->data(), static_cast<int>(maxNameLength), font, left, std::floor(top - outline - nameGap * scale), textScale, textScale, 0.0f, nameColor, Game::ITEM_TEXTSTYLE_SHADOWED);
			}
		}
	}

	EntityHealthbar::EntityHealthbar()
	{
		GSC::Script::AddMethod("SetModelHealthbar", Scr_SetModelHealthbar);

		Scheduler::Loop(SendChanges, Scheduler::Pipeline::SERVER);
		Scheduler::Loop(ReplayToNewClients, Scheduler::Pipeline::SERVER);

		Events::OnVMShutdown([]
		{
			serverBars.clear();
			changedEntities.clear();
			std::ranges::fill(isClientReplayed, false);
		});

		if (Dedicated::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			cg_drawHealthbars = Dvar::Register("cg_drawHealthbars", false, Game::DVAR_ARCHIVE, "Draw health bars over entities that scripts give one");
			cg_healthbarMaxDist = Dvar::Register("cg_healthbarMaxDist", 2000.0f, 0.0f, 100000.0f, Game::DVAR_ARCHIVE, "Furthest a health bar is drawn, in units");
			cg_healthbarAimWidth = Dvar::Register("cg_healthbarAimWidth", 32.0f, 0.0f, 1000.0f, Game::DVAR_ARCHIVE, "Width of the aim zone that shows health bars up close, in units");
			cg_healthbarAimAngle = Dvar::Register("cg_healthbarAimAngle", 0.75f, 0.0f, 45.0f, Game::DVAR_ARCHIVE, "Angle the aim zone widens by with distance, in degrees");
		});

		ServerCommands::OnCommand(healthbarServerCommand, OnHealthbarCommand);
		Scheduler::Loop(Draw, Scheduler::Pipeline::RENDERER);

		Events::OnCGameInit(ClearClientBars);

		Events::OnCLDisconnected([]([[maybe_unused]] bool wasConnected)
		{
			ClearClientBars();
		});
	}
}
