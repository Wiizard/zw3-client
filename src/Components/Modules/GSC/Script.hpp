#pragma once

namespace Components::GSC
{
	class Script : public Component
	{
	public:
		Script();

		using scriptNames = std::vector<std::string>;
		static void AddFunction(const std::string& name, Game::BuiltinFunction func, bool type = false, bool isBuiltIn = false);
		static void AddMethod(const std::string& name, Game::BuiltinMethod func, bool type = false, bool isBuiltIn = false);

		static void AddFuncMultiple(Game::BuiltinFunction func, bool type, scriptNames aliases);
		static void AddMethMultiple(Game::BuiltinMethod func, bool type, scriptNames aliases);

		static void Scr_Error(const char* error);
		static void Scr_ParamError(unsigned int paramIndex, const char* error);
		static void Scr_ObjectError(const char* error);

		static Game::gentity_s* Scr_GetPlayerEntity(Game::scr_entref_t entref);

	private:
		struct ScriptFunction
		{
			Game::BuiltinFunction actionFunc;
			bool type;
			scriptNames aliases;
		};

		struct ScriptMethod
		{
			Game::BuiltinMethod actionFunc;
			bool type;
			scriptNames aliases;
		};

		static std::vector<ScriptFunction> commonOverridenFunctions;
		static std::vector<ScriptMethod> commonOverridenMethods;

		static std::vector<ScriptFunction> customScrFunctions;
		static std::vector<ScriptMethod> customScrMethods;

		static std::unordered_map<std::string, int> scriptMainHandles;
		static std::unordered_map<std::string, int> scriptInitHandles;

		static void LoadCustomScriptsFromFolder(const char* dir);
		static void LoadCustomScripts();

		static void Scr_LoadGameType_Stub();
		static void Scr_StartupGameType_Stub();
		static void GScr_LoadGameTypeScript_Stub();

		static Game::BuiltinFunction Common_GetFunctionStub(const char** pName, int* type);
		static Game::BuiltinMethod Common_GetMethodStub(const char** pName);

		static Game::BuiltinFunction BuiltIn_GetFunctionStub(const char** pName, int* type);
		static Game::BuiltinMethod BuiltIn_GetMethodStub(const char** pName);

		static unsigned int SetExpFogStub();

		static void PrintError(const char* error);
	};
}
