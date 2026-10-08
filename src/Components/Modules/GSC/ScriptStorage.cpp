#include "STDInclude.hpp"

#include "ScriptStorage.hpp"
#include "Script.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"

namespace Components::GSC
{
	std::unordered_map<std::string, std::string> ScriptStorage::data;

	void ScriptStorage::AddScriptFunctions()
	{
		Script::AddFunction("StorageSet", []
		{
			const auto* key = Game::Scr_GetString(0);
			const auto* value = Game::Scr_GetString(1);

			if (!key || !value)
			{
				Script::Scr_Error("StorageSet: Illegal parameters!");
				return;
			}

			data.insert_or_assign(key, value);
		});

		Script::AddFunction("StorageRemove", []
		{
			const auto* key = Game::Scr_GetString(0);

			if (!key)
			{
				Script::Scr_ParamError(0, "StorageRemove: Illegal parameter!");
				return;
			}

			if (!data.contains(key))
			{
				Script::Scr_Error(Utils::String::VA("StorageRemove: Store does not have key '%s'!", key));
				return;
			}

			data.erase(key);
		});

		Script::AddFunction("StorageGet", []
		{
			const auto* key = Game::Scr_GetString(0);

			if (!key)
			{
				Script::Scr_ParamError(0, "StorageGet: Illegal parameter!");
				return;
			}

			if (!data.contains(key))
			{
				Script::Scr_Error(Utils::String::VA("StorageGet: Store does not have key '%s'!", key));
			}

			const auto& value = data.at(key);
			Game::Scr_AddString(value.data());
		});

		Script::AddFunction("StorageHas", []
		{
			const auto* key = Game::Scr_GetString(0);

			if (!key)
			{
				Script::Scr_ParamError(0, "StorageHas: Illegal parameter!");
				return;
			}

			Game::Scr_AddBool(data.contains(key));
		});

		Script::AddFunction("StorageDump", []
		{
			if (data.empty())
			{
				Script::Scr_Error("StorageDump: ScriptStorage is empty!");
				return;
			}

			const nlohmann::json json = data;

			FileSystem::FileWriter(Game::SCRIPTDATA_DIR + "/scriptstorage.json"s).Write(json.dump());
		});

		Script::AddFunction("StorageLoad", []
		{
			FileSystem::File storageFile(Game::SCRIPTDATA_DIR + "/scriptstorage.json"s);

			if (!storageFile.Exists())
			{
				return;
			}

			const auto& buffer = storageFile.GetBuffer();

			try
			{
				const nlohmann::json storageDef = nlohmann::json::parse(buffer);
				const auto& newData = storageDef.get<std::unordered_map<std::string, std::string>>();
				data.insert(newData.begin(), newData.end());
			}
			catch (const std::exception& ex)
			{
				Logger::Error("JSON Parse Error: {}. File {} is invalid\n", ex.what(), storageFile.GetName());
			}
		});

		Script::AddFunction("StorageClear", []
		{
			data.clear();
		});
	}

	ScriptStorage::ScriptStorage()
	{
		AddScriptFunctions();
	}
}
