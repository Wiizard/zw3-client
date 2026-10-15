#include "STDInclude.hpp"

#include "ZombieNavigation.hpp"

namespace Components::ZombieNavigation
{
	namespace
	{
		struct Token
		{
			std::size_t begin;
			std::size_t end;
			std::string_view text;
		};

		std::vector<Token> Tokens(const std::string_view source)
		{
			std::vector<Token> result;
			for (std::size_t cursor = 0; cursor < source.size();)
			{
				const auto start = cursor;
				const char character = source[cursor++];
				if (std::isspace(static_cast<unsigned char>(character)))
				{
					continue;
				}
				if (character == '/' && cursor < source.size() && source[cursor] == '/')
				{
					while (cursor < source.size() && source[cursor] != '\n') ++cursor;
					continue;
				}
				if (character == '/' && cursor < source.size() && source[cursor] == '*')
				{
					const auto end = source.find("*/", cursor + 1);
					cursor = end == source.npos ? source.size() : end + 2;
					continue;
				}
				if (character == '"')
				{
					while (cursor < source.size())
					{
						const char next = source[cursor++];
						if (next == '\\' && cursor < source.size()) ++cursor;
						else if (next == '"') break;
					}
				}
				else if (std::isalnum(static_cast<unsigned char>(character)) || character == '_')
				{
					while (cursor < source.size() && (std::isalnum(static_cast<unsigned char>(source[cursor])) || source[cursor] == '_')) ++cursor;
				}
				result.push_back({ start, cursor, source.substr(start, cursor - start) });
			}
			return result;
		}

		std::optional<std::pair<std::size_t, std::size_t>> FunctionBody(const std::string_view source, const std::string_view name)
		{
			const auto tokens = Tokens(source);
			for (std::size_t index = 0; index + 2 < tokens.size(); ++index)
			{
				if (tokens[index].text != name || tokens[index + 1].text != "(") continue;
				std::size_t brace = index + 2;
				int parentheses = 1;
				while (brace < tokens.size() && parentheses)
				{
					if (tokens[brace].text == "(") ++parentheses;
					if (tokens[brace].text == ")") --parentheses;
					++brace;
				}
				if (brace >= tokens.size() || tokens[brace].text != "{") continue;
				int depth = 1;
				for (std::size_t end = brace + 1; end < tokens.size(); ++end)
				{
					if (tokens[end].text == "{") ++depth;
					if (tokens[end].text == "}") --depth;
					if (!depth) return std::pair(tokens[brace].end, tokens[end].begin);
				}
			}
			return std::nullopt;
		}

		bool ReplaceBody(std::string& source, const std::string_view function, const std::string_view body)
		{
			const auto range = FunctionBody(source, function);
			if (!range) return false;
			source.replace(range->first, range->second - range->first, body);
			return true;
		}

		bool ReplaceTokens(std::string& source, const std::string_view function, const std::string_view needle, const std::string_view replacement)
		{
			const auto range = FunctionBody(source, function);
			if (!range) return false;
			const auto tokens = Tokens(std::string_view(source).substr(range->first, range->second - range->first));
			const auto pattern = Tokens(needle);
			for (std::size_t index = 0; index + pattern.size() <= tokens.size(); ++index)
			{
				bool matches = true;
				for (std::size_t part = 0; part < pattern.size(); ++part)
				{
					if (tokens[index + part].text != pattern[part].text) { matches = false; break; }
				}
				if (!matches) continue;
				const auto start = tokens[index].begin;
				const auto end = tokens[index + pattern.size() - 1].end;
				source.replace(range->first + start, end - start, replacement);
				return true;
			}
			return false;
		}

		constexpr std::string_view roamTarget = R"gsc(
    zw\zombies::zw3SyncNavigationBarriers();
    goal = self ZW3CrewRoamGoal();
    if(goal < 0 || !isDefined(level.waypoints[goal]))
    {
        self.botRoamTarget = undefined;
        return undefined;
    }
    if(!isDefined(self.botRoamTarget) || self.botRoamTarget.number != goal)
    {
        self.botRoamTarget = spawnStruct();
        self.botRoamTarget.isBotRoam = true;
        self.botRoamTarget.number = goal;
        self.botRoamTarget.origin = level.waypoints[goal].origin;
    }
    return self.botRoamTarget;
)gsc";

		constexpr std::string_view botTarget = R"gsc(
    if(self.isDown == 1) return self;
    zw\zombies::zw3SyncNavigationBarriers();
    if(isDefined(self.openedPap) && self.openedPap.active) return self.openedPap;
    if(isDefined(self.openedBox) && self.openedBox.state == "result") return self.openedBox;
    drop = self getPowerupTarget();
    if(isDefined(drop) && self ZW3CrewCanReach(drop.origin)) return drop;
    revivePlayer = self getDownedPlayerTarget();
    if(isDefined(revivePlayer)) return revivePlayer;
    if(!self.needsAmmo)
    {
        close = self isZombieClose();
        if(isDefined(close))
        {
            zombies = self zw\zombies::getCloseZombies(500);
            zombie = self getClosest(zombies);
            if(isDefined(zombie)) return zombie;
        }
    }
    pap = self getPapTarget();
    if(isDefined(pap) && self ZW3CrewCanReach(pap.origin)) return pap;
    wallWeapon = self getWallWeaponTarget();
    if(isDefined(wallWeapon) && self ZW3CrewCanReach(wallWeapon.origin)) return wallWeapon;
    mysteryBox = self getMysteryBoxTarget();
    if(isDefined(mysteryBox) && self ZW3CrewCanReach(mysteryBox.origin)) return mysteryBox;
    perkMachine = self getPerkTarget();
    if(isDefined(perkMachine) && self ZW3CrewCanReach(perkMachine.origin)) return perkMachine;
    return self getIdleRoamTarget();
)gsc";

		constexpr std::string_view barrierSync = R"gsc(
zw3SyncNavigationBarriers()
{
    if(!ZW3NavNeedsBarrierSync())
        return;
    ZW3NavBeginBarriers();
    if(isDefined(level.debrisReal))
    {
        foreach(barrier in level.debrisReal)
        {
            if(!isDefined(barrier))
                continue;
            barrier ZW3NavBarrier();
        }
    }
    for(kind = 0; kind < 2; kind++)
    {
        name = "zombie_door";
        if(kind == 1)
            name = "zombie_debris";
        foreach(trigger in getEntArray(name, "targetname"))
        {
            if(!isDefined(trigger.target))
                continue;
            foreach(door in getEntArray(trigger.target, "targetname"))
            {
                if(isDefined(door))
                    door ZW3NavBarrier();
            }
        }
    }
    ZW3NavEndBarriers();
}
)gsc";

		constexpr std::string_view spawnSelection = R"gsc(
    zw3SyncNavigationBarriers();
    if(!isDefined(maxDistance)) maxDistance = 1000;
    if(!isDefined(maxHeight)) maxHeight = 200;
    if(!isDefined(victim) || !isAlive(victim) || victim.team == "spectator")
    {
        players = [];
        foreach(player in level.players)
        {
            if(isAlive(player) && player.team != "spectator")
                players[players.size] = player;
        }
        if(players.size == 0) return undefined;
        victim = players[randomInt(players.size)];
    }
    spawns = level.spawns_zombies;
    minDistance = 400;
    if(isDefined(type) && level.spawns_dogs.size > 0 && (type == "dog" || type == "hitler" || type == "spider" || type == "samantha" || type == "avogadro" || type == "thief" || type == "brutus" || type == "dragon" || type == "witch" || type == "keeper" || type == "crawler"))
    {
        spawns = level.spawns_dogs;
        minDistance = 800;
    }
    valid = [];
    fallback = [];
    nearby = [];
    bestTier = 10;
    foreach(point in spawns)
    {
        if(!isDefined(point) || !ZW3NavCanSpawn(point.origin, victim.origin))
            continue;
        occupied = false;
        foreach(zombie in level.zombies)
        {
            if(isDefined(zombie) && isDefined(zombie.origin) && DistanceSquared(zombie.origin, point.origin) < 10000)
            {
                occupied = true;
                break;
            }
        }
        if(occupied) continue;
        distance = DistanceSquared(victim.origin, point.origin);
        if(distance < minDistance * minDistance)
        {
            nearby[nearby.size] = point;
            continue;
        }
        fallback[fallback.size] = point;
        height = abs(victim.origin[2] - point.origin[2]);
        tier = 0;
        while(tier < 9 && (distance > (maxDistance + tier * 300) * (maxDistance + tier * 300) || height > maxHeight + tier * 50))
            tier++;
        if(tier > bestTier || distance > (maxDistance + tier * 300) * (maxDistance + tier * 300) || height > maxHeight + tier * 50)
            continue;
        if(tier < bestTier)
        {
            valid = [];
            bestTier = tier;
        }
        valid[valid.size] = point;
    }
    if(valid.size > 0) return valid[randomInt(valid.size)];
    if(fallback.size > 0) return fallback[randomInt(fallback.size)];
    if(nearby.size > 0) return nearby[randomInt(nearby.size)];
    return undefined;
)gsc";
	}

	std::optional<std::string> PatchScript(const std::string_view filename, const std::string_view source)
	{
		std::string path(filename);
		std::ranges::replace(path, '\\', '/');
		std::ranges::transform(path, path.begin(), [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
		if (path != "zw/bots.gsc" && path != "zw/zombies.gsc"
			&& path != "zw/mapedits/waypoints.gsc" && path != "zw/mapedits/mapedits.gsc")
		{
			return std::nullopt;
		}
		std::string result(source);
		if (path == "zw/bots.gsc")
		{
			if (!ReplaceBody(result, "getBotTarget", botTarget)) return std::nullopt;
			if (!ReplaceBody(result, "getIdleRoamTarget", roamTarget))
			{
				result += "\ngetIdleRoamTarget() {";
				result.append(roamTarget);
				result += "}\n";
			}
			const auto turn = "\n self botangles(self.angles[0], self ZW3CrewRoamYaw(), self.angles[2]);\n";
			if (!ReplaceBody(result, "rotateToRoamTarget", turn))
			{
				result += std::string("\nrotateToRoamTarget() {") + turn + "}\n";
			}
			if (const auto range = FunctionBody(result, "getBotTargetType"))
			{
				result.insert(range->first, "\n if(isDefined(target) && isDefined(target.isBotRoam)) return \"roam\";\n");
			}
			else return std::nullopt;
			if (const auto range = FunctionBody(result, "rotateToTarget"))
			{
				result.insert(range->first, "\n if(getBotTargetType(self.botTarget) == \"roam\") { self rotateToRoamTarget(); return; }\n");
			}
			else return std::nullopt;
			if (!ReplaceTokens(result, "setMoveMovement", "type = getBotTargetType(self.botTarget);", R"gsc(
    type = getBotTargetType(self.botTarget);
    if(type == "roam") { self botmovement(self ZW3CrewRoamSpeed(), 0); return; }
)gsc")) return std::nullopt;
			ReplaceTokens(result, "moveBot", "self.botTarget = getBotTarget();", "self.botTarget = self getBotTarget();");
			ReplaceTokens(result, "moveBot", "if(!isDefined(self.botTarget)) { continue; }", "if(!isDefined(self.botTarget)) { self botmovement(0,0); continue; }");
			ReplaceTokens(result, "getDownedPlayerTarget", "if(player.isDown == 0) continue;", "if(!isDefined(player.isDown) || player.isDown != 1) continue;");
			if (const auto range = FunctionBody(result, "debugZombie"))
			{
				result.insert(range->first, "\n if(getBotTargetType(self.botTarget) == \"roam\") return;\n");
			}
		}
		else if (path == "zw/zombies.gsc")
		{
			if (FunctionBody(result, "zw3SyncNavigationBarriers")) return std::nullopt;
			if (!ReplaceBody(result, "getZombieSpawnPoint", spawnSelection)
				|| !ReplaceTokens(result, "spawnZombie", R"gsc(
    if(!isDefined(spawnPoint))
        spawnPoint = getZombieSpawnPoint(undefined,undefined,undefined,type);
    if(!isDefined(spawnPoint))
    {
        wait 1;
        return spawnZombie(type, countToZombieAmount, hasPowerup, spawnPoint, health, playSpawnAnim, awareness);
    }
)gsc", R"gsc(
    zw3SyncNavigationBarriers();
    if(isDefined(spawnPoint) && !ZW3NavSpawnAllowed(spawnPoint.origin))
        spawnPoint = undefined;
    while(!isDefined(spawnPoint))
    {
        spawnPoint = getZombieSpawnPoint(undefined,undefined,undefined,type);
        if(!isDefined(spawnPoint)) wait 1;
    }
)gsc")) return std::nullopt;
			result.append(barrierSync);
		}
		else if (path == "zw/mapedits/waypoints.gsc")
		{
			if (!ReplaceTokens(result, "setZombieWayPoints", "records = strTok(rawData, \";\");", "ZW3NavLoad(rawData); records = strTok(rawData, \";\");")) return std::nullopt;
			if (const auto range = FunctionBody(result, "linkWaypoints"))
			{
				result.insert(range->second, "\n ZW3NavLink(a, b);\n");
			}
			if (const auto range = FunctionBody(result, "addChild"))
			{
				result.insert(range->second, "\n ZW3NavChild(parent, child);\n");
			}
			if (const auto range = FunctionBody(result, "addWaypoint"))
			{
				result.insert(range->second, R"gsc(
    ZW3NavNode(number, origin);
    if(isDefined(children))
    {
        foreach(child in children) ZW3NavChild(number, child);
    }
)gsc");
			}
		}
		else
		{
			// Keep every collision block, rather than just the purchase anchor.
			// Consume only transactions; held-use checks such as revives stay intact.
			if (!ReplaceTokens(result, "Weapon_Buy", "if(player.isDown) return;", R"gsc(
    if(player.isDown) return;
    if(self.weaponProcessActive || !(player ZW3ConsumeUse())) return;
)gsc")
				|| !ReplaceTokens(result, "activateWindows", "chunk.isMoving = true;", R"gsc(
    if(!(player ZW3ConsumeUse())) break;
    chunk.isMoving = true;
)gsc")
				|| !ReplaceTokens(result, "prepareBox", "self.state = \"gambling\";", R"gsc(
    if(!(player ZW3ConsumeUse())) return;
    self.state = "gambling";
)gsc")
				|| !ReplaceTokens(result, "readyToTake", "weapon = self.weapon;", R"gsc(
    if(!(player ZW3ConsumeUse())) continue;
    weapon = self.weapon;
)gsc")
				|| !ReplaceTokens(result, "Machine_Trigger", "if(player.score >= cost)", R"gsc(
    if(!(player ZW3ConsumeUse())) continue;
    if(player.score >= cost)
)gsc")
				|| !ReplaceTokens(result, "tryDeleteDebris", "self.score -= cost;", R"gsc(
    if(!(self ZW3ConsumeUse())) return;
    self.score -= cost;
)gsc")
				|| !ReplaceTokens(result, "openDoorLogic", "player.score -= cost;", R"gsc(
    if(!(player isBot()) && !(player ZW3ConsumeUse())) continue;
    player.score -= cost;
)gsc")) return std::nullopt;
			if (!ReplaceTokens(result, "_CreateDebris", "level.debrisReal[level.debrisReal.size] = blockitems[0];", R"gsc(
    foreach(zw3Block in blockItems) blockitems[0] ZW3NavDebrisBlock(zw3Block getEntityNumber());
    level.debrisReal[level.debrisReal.size] = blockitems[0];
)gsc")) return std::nullopt;
		}
		return result;
	}
}
