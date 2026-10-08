#include "STDInclude.hpp"

#include <random>

#include "MapRotation.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Logger.hpp"
#include "Party.hpp"

namespace Components
{
	Dvar::Var MapRotation::sv_mapRotation;
	Dvar::Var MapRotation::sv_mapRotationCurrent;
	Dvar::Var MapRotation::sv_randomMapRotation;
	Dvar::Var MapRotation::sv_dontRotate;
	Dvar::Var MapRotation::sv_nextMap;

	MapRotation::RotationData MapRotation::dedicatedRotation;

	constexpr std::uintptr_t Dvar_SetStringByName = 0x140287A70;

	constexpr std::uintptr_t GScr_ExitLevel_ExitLevelJump = 0x1401A4005;
	constexpr std::uintptr_t ExitLevel = 0x14019CB10;

	static Utils::Hook exitLevelHook;

	void MapRotation::RotationData::Randomize()
	{
		std::random_device randomDevice;
		std::mt19937 generator(randomDevice());

		std::ranges::shuffle(this->entries, generator);
	}

	void MapRotation::RotationData::AddEntry(const std::string& key, const std::string& value)
	{
		this->entries.emplace_back(key, value);
	}

	std::size_t MapRotation::RotationData::GetEntriesSize() const
	{
		return this->entries.size();
	}

	const MapRotation::RotationData::RotationEntry& MapRotation::RotationData::GetNextEntry()
	{
		const auto current = this->index;

		this->index = (this->index + 1) % this->entries.size();

		return this->entries.at(current);
	}

	const MapRotation::RotationData::RotationEntry& MapRotation::RotationData::PeekNextEntry() const
	{
		return this->entries.at(this->index);
	}

	void MapRotation::RotationData::SetHandler(const std::string& key, const RotationCallback& callback)
	{
		this->handlers[key] = callback;
	}

	void MapRotation::RotationData::CallHandler(const RotationEntry& entry) const
	{
		const auto handler = this->handlers.find(entry.first);

		if (handler == this->handlers.end())
		{
			return;
		}

		handler->second(entry.second);
	}

	bool MapRotation::RotationData::TryParse(const std::string& data, std::string& invalidKey)
	{
		const auto tokens = Utils::String::Split(data, ' ');

		for (std::size_t i = 0; !tokens.empty() && i < (tokens.size() - 1); i += 2)
		{
			const auto& key = tokens[i];
			const auto& value = tokens[i + 1];

			if (!this->ContainsHandler(key))
			{
				invalidKey = key;
				return false;
			}

			this->AddEntry(key, value);
		}

		return true;
	}

	bool MapRotation::RotationData::IsEmpty() const
	{
		return this->entries.empty();
	}

	bool MapRotation::RotationData::Contains(const std::string& key, const std::string& value) const
	{
		return std::ranges::any_of(this->entries, [&key, &value](const RotationEntry& entry)
		{
			return entry.first == key && entry.second == value;
		});
	}

	bool MapRotation::RotationData::ContainsHandler(const std::string& key) const
	{
		return this->handlers.contains(key);
	}

	nlohmann::json MapRotation::RotationData::ToJson() const
	{
		std::vector<std::string> maps;
		std::vector<std::string> gametypes;

		for (const auto& [key, value] : this->entries)
		{
			if (key == "map")
			{
				maps.emplace_back(value);
			}
			else if (key == "gametype")
			{
				gametypes.emplace_back(value);
			}
		}

		return nlohmann::json
		{
			{ "maps", maps },
			{ "gametypes", gametypes },
		};
	}

	void MapRotation::ParseRotation(const std::string& data)
	{
		std::string invalidKey;

		if (!dedicatedRotation.TryParse(data, invalidKey))
		{
			Logger::Error("Map Rotation Parse Error: Invalid key '{}'. sv_mapRotation contains invalid data!\n", invalidKey);
		}

		Logger::Debug("DedicatedRotation size after parsing is '{}'", dedicatedRotation.GetEntriesSize());
	}

	void MapRotation::RandomizeMapRotation()
	{
		if (!sv_randomMapRotation.Get<bool>())
		{
			Logger::Debug("Map rotation was not randomized");
			return;
		}

		Logger::Print("Randomizing the map rotation\n");
		dedicatedRotation.Randomize();
	}

	void MapRotation::LoadMapRotation()
	{
		static bool isLoaded = false;

		if (isLoaded)
		{
			return;
		}

		isLoaded = true;

		const std::string mapRotation = sv_mapRotation.Get<const char*>();

		if (!mapRotation.empty())
		{
			Logger::Debug("sv_mapRotation is not empty. Parsing...");
			ParseRotation(mapRotation);
			RandomizeMapRotation();
			return;
		}

		if (!dedicatedRotation.IsEmpty())
		{
			RandomizeMapRotation();
		}
	}

	void MapRotation::AddMapRotationCommands()
	{
		Command::AddSV("map_rotate", []([[maybe_unused]] const Command::Params* params)
		{
			SV_MapRotate_f();
		});

		Command::AddSV("addMap", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				Logger::Print("{} <map name> : add a map to the map rotation\n", params->Get(0));
				return;
			}

			dedicatedRotation.AddEntry("map", params->Get(1));
		});

		Command::AddSV("addGametype", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				Logger::Print("{} <gametype> : add a game mode to the map rotation\n", params->Get(0));
				return;
			}

			dedicatedRotation.AddEntry("gametype", params->Get(1));
		});
	}

	bool MapRotation::Contains(const std::string& key, const std::string& value)
	{
		return dedicatedRotation.Contains(key, value);
	}

	nlohmann::json MapRotation::ToJson()
	{
		return dedicatedRotation.ToJson();
	}

	bool MapRotation::ShouldRotate()
	{
		if (!Dedicated::IsEnabled() && Party::IsHostingParty())
		{
			Logger::Warning("Not performing map rotation as we are hosting a party!\n");
			sv_dontRotate.Set(true);
			return false;
		}

		if (Dedicated::IsEnabled() && sv_dontRotate.Get<bool>())
		{
			Logger::Print("Not performing map rotation as sv_dontRotate is true\n");
			sv_dontRotate.Set(true);
			return false;
		}

		if (Party::IsEnabled() && Party::IsHostingParty())
		{
			Logger::Warning("Not performing map rotation as we are hosting a lobby server!\n");
			return false;
		}

		return true;
	}

	void MapRotation::ApplyMap(const std::string& map)
	{
		if (Dvar::Var("sv_cheats").Get<bool>())
		{
			Command::Execute(std::format("devmap {}", map), true);
			return;
		}

		Command::Execute(std::format("map {}", map), true);
	}

	void MapRotation::ApplyGametype(const std::string& gametype)
	{
		reinterpret_cast<void(*)(const char*, const char*)>(Utils::Hook::Rebase(Dvar_SetStringByName))("g_gametype", gametype.data());
	}

	void MapRotation::ApplyExec(const std::string& name)
	{
		Command::Execute(std::format("exec game_settings/{}", name), false);
	}

	void MapRotation::RestartCurrentMap()
	{
		std::string mapname = Dvar::Var("mapname").Get<const char*>();

		if (mapname.empty())
		{
			Logger::Print("mapname dvar is empty! Defaulting to mp_afghan\n");
			mapname = "mp_afghan";
		}

		ApplyMap(mapname);
	}

	void MapRotation::ApplyRotation(RotationData& rotation)
	{
		std::size_t i = 0;

		while (i < rotation.GetEntriesSize())
		{
			const auto& entry = rotation.GetNextEntry();

			rotation.CallHandler(entry);
			Logger::Print("MapRotation: applying key '{}' with value '{}'\n", entry.first, entry.second);

			if (entry.first == "map")
			{
				break;
			}

			++i;
		}

		if (i == rotation.GetEntriesSize())
		{
			Logger::Error("Map rotation does not contain any map. Restarting\n");
			RestartCurrentMap();
		}
	}

	void MapRotation::ApplyMapRotationCurrent(const std::string& data)
	{
		Logger::Warning("You are using deprecated sv_mapRotationCurrent\n");

		RotationData rotationCurrent;
		rotationCurrent.SetHandler("map", ApplyMap);
		rotationCurrent.SetHandler("gametype", ApplyGametype);
		rotationCurrent.SetHandler("exec", ApplyExec);

		Logger::Debug("Parsing sv_mapRotationCurrent");

		std::string invalidKey;

		if (!rotationCurrent.TryParse(data, invalidKey))
		{
			Logger::Error("Map Rotation Parse Error: Invalid key '{}'. sv_mapRotationCurrent contains invalid data!\n", invalidKey);
		}

		sv_mapRotationCurrent.Set("");

		if (rotationCurrent.IsEmpty())
		{
			Logger::Print("sv_mapRotationCurrent is empty or contains invalid data. Restarting map\n");
			RestartCurrentMap();
			return;
		}

		ApplyRotation(rotationCurrent);
	}

	void MapRotation::SetNextMap(const RotationData& rotation)
	{
		const auto& entry = rotation.PeekNextEntry();

		if (entry.first == "map")
		{
			sv_nextMap.Set(entry.second);
			return;
		}

		ClearNextMap();
	}

	void MapRotation::SetNextMap(const char* value)
	{
		sv_nextMap.Set(value);
	}

	void MapRotation::ClearNextMap()
	{
		sv_nextMap.Set("");
	}

	void MapRotation::SV_MapRotate_f()
	{
		if (!ShouldRotate())
		{
			return;
		}

		Logger::Print("Rotating map...\n");

		const std::string mapRotationCurrent = sv_mapRotationCurrent.Get<const char*>();

		if (!mapRotationCurrent.empty())
		{
			Logger::Debug("Applying sv_mapRotationCurrent");
			ApplyMapRotationCurrent(mapRotationCurrent);
			ClearNextMap();
			return;
		}

		LoadMapRotation();

		if (dedicatedRotation.IsEmpty())
		{
			Logger::Print("sv_mapRotation is empty or contains invalid data. Restarting map\n");
			RestartCurrentMap();
			SetNextMap("map_restart");
			return;
		}

		ApplyRotation(dedicatedRotation);
		SetNextMap(dedicatedRotation);
	}

	void MapRotation::RegisterMapRotationDvars()
	{
		sv_mapRotation = Dvar::Register("sv_mapRotation", "", Game::DVAR_NONE, "List of maps for the server to play");
		sv_mapRotationCurrent = Dvar::Register("sv_mapRotationCurrent", "", Game::DVAR_NONE, "Current map in the map rotation");

		sv_randomMapRotation = Dvar::Register("sv_randomMapRotation", false, Game::DVAR_ARCHIVE, "Randomize map rotation when true");
		sv_dontRotate = Dvar::Register("sv_dontRotate", false, Game::DVAR_NONE, "Do not perform map rotation");
		sv_nextMap = Dvar::Register("sv_nextMap", "", Game::DVAR_SERVERINFO, "");
	}

	void MapRotation::ExitLevel_Hk()
	{
		Game::Cbuf_AddText(0, "map_rotate\n");

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(ExitLevel))();
	}

	MapRotation::MapRotation()
	{
		if (!Events::IsInstalled())
		{
			Logger::Error("maprotation: events are not installed, there is no map_rotate\n");
			return;
		}

		if (!Utils::Hook::BranchesTo(GScr_ExitLevel_ExitLevelJump, ExitLevel, HOOK_JUMP)
			|| !exitLevelHook.Initialize(GScr_ExitLevel_ExitLevelJump, ExitLevel_Hk, HOOK_JUMP)->Install()->IsInstalled())
		{
			Logger::Error("maprotation: GScr_ExitLevel does not read as expected, a match that ends does not rotate\n");
		}
		else
		{
			exitLevelHook.Quick();
		}

		Events::OnSVInit(AddMapRotationCommands);

		dedicatedRotation.SetHandler("map", ApplyMap);
		dedicatedRotation.SetHandler("gametype", ApplyGametype);
		dedicatedRotation.SetHandler("exec", ApplyExec);

		Events::OnDvarInit(RegisterMapRotationDvars);
	}
}
