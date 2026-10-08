#include "STDInclude.hpp"

#include <bit>

#include "Script.hpp"
#include "../Logger.hpp"

namespace Components::GSC
{
	std::vector<Script::ScriptFunction> Script::customScrFunctions;
	std::vector<Script::ScriptMethod> Script::customScrMethods;

	std::vector<Script::ScriptFunction> Script::commonOverridenFunctions;
	std::vector<Script::ScriptMethod> Script::commonOverridenMethods;

	std::unordered_map<std::string, int> Script::scriptMainHandles;
	std::unordered_map<std::string, int> Script::scriptInitHandles;

	constexpr std::uintptr_t Scr_GetFunction_CommonCall = 0x1401A736A;
	constexpr std::uintptr_t Common_GetFunction = 0x140181760;
	constexpr std::uintptr_t Scr_GetFunction_ObjectiveCall = 0x1401A737E;
	constexpr std::uintptr_t Objective_GetFunction = 0x1401A89C0;

	constexpr std::uintptr_t Scr_GetMethod_CommonCall = 0x1401A745A;
	constexpr std::uintptr_t Common_GetMethod = 0x140181830;
	constexpr std::uintptr_t Scr_GetMethod_LastTableCall = 0x1401A74C0;
	constexpr std::uintptr_t LastTable_GetMethod = 0x14016B960;

	constexpr std::uintptr_t G_InitGame_LoadGameTypeCall = 0x14019D5B8;
	constexpr std::uintptr_t Scr_LoadGameType = 0x1401A7630;
	constexpr std::uintptr_t G_InitGame_StartupGameTypeCall = 0x14019D5C2;
	constexpr std::uintptr_t Scr_StartupGameType = 0x1401A7D50;

	constexpr std::uintptr_t GScr_LoadScripts_GameTypeScriptCall = 0x1401A6CA1;
	constexpr std::uintptr_t GScr_LoadGameTypeScript = 0x1401A6710;

	constexpr std::uintptr_t Scr_SetExponentialFog_NumParamCall = 0x14017F5F4;
	constexpr std::uintptr_t Scr_GetNumParam = 0x14022A1B0;

	constexpr std::uintptr_t ScriptCompile_FunctionOverrideJnz = 0x14021D0E0;
	constexpr std::uintptr_t ScriptCompile_MethodOverrideJnz = 0x14021D0FB;
	static const std::uint8_t functionOverrideJnz[] = { 0x0F, 0x85, 0xB3, 0x00, 0x00, 0x00 };
	static const std::uint8_t methodOverrideJnz[] = { 0x0F, 0x85, 0x98, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t DeveloperScriptJz[] = { 0x1401A51FF, 0x1401A596F, 0x1401A65C2 };
	static const std::uint8_t developerScriptJz[] = { 0x74, 0x05 };

	constexpr int listAll = 2;
	constexpr int listTag = 10;

	constexpr std::size_t maxOsPath = 256;

	static Utils::Hook commonGetFunctionHook;
	static Utils::Hook builtInGetFunctionHook;
	static Utils::Hook commonGetMethodHook;
	static Utils::Hook builtInGetMethodHook;
	static Utils::Hook loadGameTypeHook;
	static Utils::Hook startupGameTypeHook;
	static Utils::Hook loadGameTypeScriptHook;
	static Utils::Hook setExpFogHook;

	void Script::PrintError(const char* error)
	{
		if (!(*Game::com_developer)->current.integer)
		{
			return;
		}

		Logger::Error("script runtime error: {}\n", error);
	}

	void Script::Scr_Error(const char* error)
	{
		PrintError(error);
		Game::Scr_ErrorInternal();
	}

	void Script::Scr_ParamError([[maybe_unused]] const unsigned int paramIndex, const char* error)
	{
		PrintError(error);
		Game::Scr_ErrorInternal();
	}

	void Script::Scr_ObjectError(const char* error)
	{
		PrintError(error);
		Game::Scr_ErrorInternal();
	}

	Game::gentity_s* Script::Scr_GetPlayerEntity(Game::scr_entref_t entref)
	{
		if (entref.classnum)
		{
			Scr_ObjectError("not an entity");
			return nullptr;
		}

		assert(entref.entnum < Game::MAX_GENTITIES);

		auto* ent = &Game::g_entities[entref.entnum];

		if (!ent->client)
		{
			Scr_ObjectError(Utils::String::VA("entity %hu is not a player", entref.entnum));
			return nullptr;
		}

		return ent;
	}

	void Script::Scr_LoadGameType_Stub()
	{
		for (const auto& handle : scriptMainHandles)
		{
			const auto id = Game::Scr_ExecThread(handle.second, 0);
			Game::Scr_FreeThread(id);
		}

		reinterpret_cast<void(*)()>(loadGameTypeHook.GetOriginal())();
	}

	void Script::Scr_StartupGameType_Stub()
	{
		for (const auto& handle : scriptInitHandles)
		{
			const auto id = Game::Scr_ExecThread(handle.second, 0);
			Game::Scr_FreeThread(id);
		}

		reinterpret_cast<void(*)()>(startupGameTypeHook.GetOriginal())();
	}

	void Script::LoadCustomScriptsFromFolder(const char* dir)
	{
		char path[maxOsPath]{};
		char searchPath[maxOsPath]{};

		strncpy_s(searchPath, dir, _TRUNCATE);
		strncat_s(searchPath, "/", _TRUNCATE);

		int numFiles = 0;
		char** const files = Game::FS_ListFiles(searchPath, "gsc", listAll, &numFiles, listTag);

		for (int i = 0; i < numFiles; ++i)
		{
			const char* scriptFile = files[i];
			Logger::Print("Loading script {}...\n", scriptFile);

			const int length = sprintf_s(path, "%s/%s", dir, scriptFile);

			if (length == -1)
			{
				continue;
			}

			path[length - 4] = '\0';

			if (!Game::Scr_LoadScript(path))
			{
				Logger::Print("Script {} encountered an error while loading. A compilation error is the most likely cause\n", path);
				continue;
			}

			Logger::Print("Script {}.gsc loaded successfully.\n", path);

			const int initHandle = Game::Scr_GetFunctionHandle(path, "init");

			if (initHandle != 0)
			{
				Logger::Debug("Loaded '{}::init'", path);
				scriptInitHandles.insert_or_assign(path, initHandle);
			}

			const int mainHandle = Game::Scr_GetFunctionHandle(path, "main");

			if (mainHandle != 0)
			{
				Logger::Debug("Loaded '{}::main'", path);
				scriptMainHandles.insert_or_assign(path, mainHandle);
			}
		}

		if (files)
		{
			Game::Sys_FreeFileList(files);
		}
	}

	void Script::LoadCustomScripts()
	{
		char dir[maxOsPath]{};

		LoadCustomScriptsFromFolder("scripts");
		LoadCustomScriptsFromFolder("scripts/mp");

		const Game::dvar_t* const mapName = Game::Dvar_FindVar("mapname");

		if (mapName)
		{
			sprintf_s(dir, "scripts/mp/%s", mapName->current.string);
			LoadCustomScriptsFromFolder(dir);
		}

		const Game::dvar_t* const gameType = Game::Dvar_FindVar("g_gametype");

		if (gameType)
		{
			sprintf_s(dir, "scripts/mp/%s", gameType->current.string);
			LoadCustomScriptsFromFolder(dir);
		}
	}

	void Script::GScr_LoadGameTypeScript_Stub()
	{
		scriptMainHandles.clear();
		scriptInitHandles.clear();

		LoadCustomScripts();

		reinterpret_cast<void(*)()>(loadGameTypeScriptHook.GetOriginal())();
	}

	void Script::AddFunction(const std::string& name, const Game::BuiltinFunction func, const bool type, const bool isBuiltIn)
	{
		ScriptFunction toAdd;
		toAdd.actionFunc = func;
		toAdd.type = type;
		toAdd.aliases.push_back(Utils::String::ToLower(name));

		if (isBuiltIn)
		{
			commonOverridenFunctions.emplace_back(toAdd);
		}
		else
		{
			customScrFunctions.emplace_back(toAdd);
		}
	}

	void Script::AddMethod(const std::string& name, const Game::BuiltinMethod func, const bool type, const bool isBuiltIn)
	{
		ScriptMethod toAdd;
		toAdd.actionFunc = func;
		toAdd.type = type;
		toAdd.aliases.push_back(Utils::String::ToLower(name));

		if (isBuiltIn)
		{
			commonOverridenMethods.emplace_back(toAdd);
		}
		else
		{
			customScrMethods.emplace_back(toAdd);
		}
	}

	void Script::AddFuncMultiple(const Game::BuiltinFunction func, const bool type, scriptNames aliases)
	{
		ScriptFunction toAdd;
		toAdd.actionFunc = func;
		toAdd.type = type;

		for (const auto& alias : aliases)
		{
			toAdd.aliases.push_back(Utils::String::ToLower(alias));
		}

		customScrFunctions.emplace_back(toAdd);
	}

	void Script::AddMethMultiple(const Game::BuiltinMethod func, const bool type, scriptNames aliases)
	{
		ScriptMethod toAdd;
		toAdd.actionFunc = func;
		toAdd.type = type;

		for (const auto& alias : aliases)
		{
			toAdd.aliases.push_back(Utils::String::ToLower(alias));
		}

		customScrMethods.emplace_back(toAdd);
	}

	Game::BuiltinFunction Script::Common_GetFunctionStub(const char** pName, int* type)
	{
		if (pName != nullptr)
		{
			const auto name = Utils::String::ToLower(*pName);

			for (const auto& func : commonOverridenFunctions)
			{
				if (std::ranges::find(func.aliases, name) != func.aliases.end())
				{
					*type = func.type;
					return func.actionFunc;
				}
			}
		}
		else
		{
			for (const auto& func : commonOverridenFunctions)
			{
				Game::Scr_RegisterFunction(reinterpret_cast<void*>(func.actionFunc), func.aliases.at(0).data());
			}
		}

		return reinterpret_cast<Game::BuiltinFunction(*)(const char**, int*)>(commonGetFunctionHook.GetOriginal())(pName, type);
	}

	Game::BuiltinFunction Script::BuiltIn_GetFunctionStub(const char** pName, int* type)
	{
		const auto found = reinterpret_cast<Game::BuiltinFunction(*)(const char**, int*)>(builtInGetFunctionHook.GetOriginal())(pName, type);

		if (found)
		{
			return found;
		}

		if (pName != nullptr)
		{
			const auto name = Utils::String::ToLower(*pName);

			for (const auto& func : customScrFunctions)
			{
				if (std::ranges::find(func.aliases, name) != func.aliases.end())
				{
					*type = func.type;
					return func.actionFunc;
				}
			}
		}
		else
		{
			for (const auto& func : customScrFunctions)
			{
				Game::Scr_RegisterFunction(reinterpret_cast<void*>(func.actionFunc), func.aliases.at(0).data());
			}
		}

		return nullptr;
	}

	Game::BuiltinMethod Script::Common_GetMethodStub(const char** pName)
	{
		if (pName != nullptr)
		{
			const auto name = Utils::String::ToLower(*pName);

			for (const auto& meth : commonOverridenMethods)
			{
				if (std::ranges::find(meth.aliases, name) != meth.aliases.end())
				{
					return meth.actionFunc;
				}
			}
		}
		else
		{
			for (const auto& meth : commonOverridenMethods)
			{
				Game::Scr_RegisterFunction(reinterpret_cast<void*>(meth.actionFunc), meth.aliases.at(0).data());
			}
		}

		return reinterpret_cast<Game::BuiltinMethod(*)(const char**)>(commonGetMethodHook.GetOriginal())(pName);
	}

	Game::BuiltinMethod Script::BuiltIn_GetMethodStub(const char** pName)
	{
		const auto found = reinterpret_cast<Game::BuiltinMethod(*)(const char**)>(builtInGetMethodHook.GetOriginal())(pName);

		if (found)
		{
			return found;
		}

		if (pName != nullptr)
		{
			const auto name = Utils::String::ToLower(*pName);

			for (const auto& meth : customScrMethods)
			{
				if (std::ranges::find(meth.aliases, name) != meth.aliases.end())
				{
					return meth.actionFunc;
				}
			}
		}
		else
		{
			for (const auto& meth : customScrMethods)
			{
				Game::Scr_RegisterFunction(reinterpret_cast<void*>(meth.actionFunc), meth.aliases.at(0).data());
			}
		}

		return nullptr;
	}

	unsigned int Script::SetExpFogStub()
	{
		if (Game::Scr_GetNumParam() == 6)
		{
			Game::VariableValue*& top = *Game::scrVmPub_top;

			std::memmove(&top[-4], &top[-5], sizeof(Game::VariableValue) * 6);
			top += 1;
			top[-6].type = Game::VAR_FLOAT;
			top[-6].u.floatValue = 0.0f;

			++*Game::scrVmPub_outparamcount;
		}

		return Game::Scr_GetNumParam();
	}

	constexpr std::uintptr_t Scr_BeginLoadScripts_HunkSize = 0x14021DB94;
	static const std::uint8_t programHunkSize[] = { 0xB9, 0x00, 0x00, 0x18, 0x00 };
	constexpr std::uint32_t programBufferSize = 0x1000000;

	constexpr std::uintptr_t MT_Alloc = 0x14021E0A0;
	constexpr std::uintptr_t MT_Free = 0x14021E700;
	constexpr std::uintptr_t Sys_OutOfMemErrorInternal = 0x1402A5770;
	static const std::uint8_t mtAllocBytes[] =
	{
		0x48, 0x83, 0xEC, 0x28, 0xE8, 0x17, 0x00, 0x00, 0x00, 0x8B, 0xC0, 0x48, 0x8D, 0x0C, 0x40, 0x48,
		0x8D, 0x05, 0x4A, 0x15, 0xC1, 0x01, 0x48, 0x8D, 0x04, 0x88, 0x48, 0x83, 0xC4, 0x28, 0xC3,
	};
	static const std::uint8_t mtFreeBytes[] =
	{
		0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x44, 0x8B, 0xC2, 0x48, 0x8D, 0x3D,
		0xEC, 0x0E, 0xC1, 0x01, 0x48, 0x2B, 0xCF,
	};
	constexpr std::size_t memoryNodeSize = 12;

	static HANDLE scriptHeap = nullptr;
	static Utils::Hook mtAllocHook;
	static Utils::Hook mtFreeHook;

	static std::size_t MemoryTreeBlockSize(const int numBytes)
	{
		const std::size_t nodes = (static_cast<std::size_t>(numBytes) + memoryNodeSize - 1) / memoryNodeSize;

		return std::bit_ceil(nodes) * memoryNodeSize;
	}

	static void* MT_Alloc_Hk(const int numBytes, [[maybe_unused]] const int type)
	{
		void* const block = HeapAlloc(scriptHeap, 0, MemoryTreeBlockSize(numBytes));

		if (!block)
		{
			reinterpret_cast<void(*)(const char*, int)>(Utils::Hook::Rebase(Sys_OutOfMemErrorInternal))(__FILE__, __LINE__);
		}

		return block;
	}

	static void MT_Free_Hk(void* block, [[maybe_unused]] const int numBytes)
	{
		HeapFree(scriptHeap, 0, block);
	}

	static void MoveMemoryTreeBlocks()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(MT_Alloc, mtAllocBytes, sizeof(mtAllocBytes))
			&& Utils::Hook::MatchesBytes(MT_Free, mtFreeBytes, sizeof(mtFreeBytes));

		if (!isExpected)
		{
			Logger::Error("script: MT_Alloc or MT_Free does not read as expected, vectors and thread stacks stay in the script string memory\n");
			return;
		}

		scriptHeap = HeapCreate(0, 0, 0);

		if (!scriptHeap)
		{
			Logger::Error("script: could not create the script heap, vectors and thread stacks stay in the script string memory\n");
			return;
		}

		bool isSeated = mtAllocHook.Initialize(MT_Alloc, reinterpret_cast<void*>(MT_Alloc_Hk), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = mtFreeHook.Initialize(MT_Free, reinterpret_cast<void*>(MT_Free_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			mtAllocHook.Uninstall();
			mtFreeHook.Uninstall();

			HeapDestroy(scriptHeap);
			scriptHeap = nullptr;

			Logger::Error("script: could not redirect MT_Alloc and MT_Free, vectors and thread stacks stay in the script string memory\n");
			return;
		}

		mtAllocHook.Quick();
		mtFreeHook.Quick();
	}

	Script::Script()
	{
		if (Utils::Hook::MatchesBytes(Scr_BeginLoadScripts_HunkSize, programHunkSize, sizeof(programHunkSize)))
		{
			Utils::Hook::Set<std::uint32_t>(Scr_BeginLoadScripts_HunkSize + 1, programBufferSize);
		}
		else
		{
			Logger::Error("script: the program buffer's Hunk_UserCreate does not read as expected, it stays 1.5 MiB\n");
		}

		MoveMemoryTreeBlocks();

		struct CallSite
		{
			Utils::Hook* hook;
			std::uintptr_t site;
			std::uintptr_t target;
			void* stub;
		};

		const CallSite callSites[] =
		{
			{ &commonGetFunctionHook, Scr_GetFunction_CommonCall, Common_GetFunction, reinterpret_cast<void*>(Common_GetFunctionStub) },
			{ &builtInGetFunctionHook, Scr_GetFunction_ObjectiveCall, Objective_GetFunction, reinterpret_cast<void*>(BuiltIn_GetFunctionStub) },
			{ &commonGetMethodHook, Scr_GetMethod_CommonCall, Common_GetMethod, reinterpret_cast<void*>(Common_GetMethodStub) },
			{ &builtInGetMethodHook, Scr_GetMethod_LastTableCall, LastTable_GetMethod, reinterpret_cast<void*>(BuiltIn_GetMethodStub) },
			{ &loadGameTypeHook, G_InitGame_LoadGameTypeCall, Scr_LoadGameType, reinterpret_cast<void*>(Scr_LoadGameType_Stub) },
			{ &startupGameTypeHook, G_InitGame_StartupGameTypeCall, Scr_StartupGameType, reinterpret_cast<void*>(Scr_StartupGameType_Stub) },
			{ &loadGameTypeScriptHook, GScr_LoadScripts_GameTypeScriptCall, GScr_LoadGameTypeScript, reinterpret_cast<void*>(GScr_LoadGameTypeScript_Stub) },
			{ &setExpFogHook, Scr_SetExponentialFog_NumParamCall, Scr_GetNumParam, reinterpret_cast<void*>(SetExpFogStub) },
		};

		for (const auto& callSite : callSites)
		{
			if (!Utils::Hook::BranchesTo(callSite.site, callSite.target, HOOK_CALL))
			{
				Logger::Error("script: 0x{:X} no longer calls 0x{:X}, no script function of ours will load\n", callSite.site, callSite.target);
				return;
			}
		}

		const bool isCompilerIntact = Utils::Hook::MatchesBytes(ScriptCompile_FunctionOverrideJnz, functionOverrideJnz, sizeof(functionOverrideJnz))
			&& Utils::Hook::MatchesBytes(ScriptCompile_MethodOverrideJnz, methodOverrideJnz, sizeof(methodOverrideJnz));

		const bool isPersistentCheckIntact = std::ranges::all_of(DeveloperScriptJz, [](const std::uintptr_t site)
		{
			return Utils::Hook::MatchesBytes(site, developerScriptJz, sizeof(developerScriptJz));
		});

		if (!isCompilerIntact || !isPersistentCheckIntact)
		{
			Logger::Error("script: ScriptCompile or the persistent data checks do not read as expected, no script function of ours will load\n");
			return;
		}

		bool isSeated = true;

		for (const auto& callSite : callSites)
		{
			isSeated = callSite.hook->Initialize(callSite.site, callSite.stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (const auto& callSite : callSites)
			{
				callSite.hook->Uninstall();
			}

			Logger::Error("script: could not seat every hook, no script function of ours will load\n");
			return;
		}

		for (const auto& callSite : callSites)
		{
			callSite.hook->Quick();
		}

		Utils::Hook::Nop(ScriptCompile_FunctionOverrideJnz, sizeof(functionOverrideJnz));
		Utils::Hook::Nop(ScriptCompile_MethodOverrideJnz, sizeof(methodOverrideJnz));

		for (const std::uintptr_t site : DeveloperScriptJz)
		{
			Utils::Hook::Set<std::uint8_t>(site, 0xEB);
		}
	}
}
