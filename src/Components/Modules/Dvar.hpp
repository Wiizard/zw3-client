#pragma once

namespace Components
{
	class Dvar : public Component
	{
	public:
		class Var
		{
		public:
			Var() : dvar(nullptr) {}
			Var(Game::dvar_t* dvar) : dvar(dvar) {}
			Var(const std::string& name);

			template <typename T> T Get() const;

			void Set(bool value) const;
			void Set(int value) const;
			void Set(float value) const;
			void Set(const char* value) const;
			void Set(const std::string& value) const;

			[[nodiscard]] Game::dvar_t* Get() const { return this->dvar; }
			[[nodiscard]] bool IsValid() const { return this->dvar != nullptr; }

		private:
			Game::dvar_t* dvar;
		};

		Dvar();

		static Var Register(const char* name, bool value, unsigned int flags, const char* description);
		static Var Register(const char* name, int value, int min, int max, unsigned int flags, const char* description);
		static Var Register(const char* name, float value, float min, float max, unsigned int flags, const char* description);
		static Var Register(const char* name, const char* value, unsigned int flags, const char* description);

		static Var Find(const std::string& name);

		static Var Name;

	private:
		static Game::dvar_t* Dvar_RegisterName(const char* dvarName, const char* value, unsigned int flags, const char* description);
		static Game::dvar_t* Dvar_RegisterSVNetworkFps(const char* dvarName, int value, int min, int max, unsigned int flags, const char* description);
		static Game::dvar_t* Dvar_Register_cg_drawFPS(const char* dvarName, const char** valueList, int defaultIndex, unsigned int flags, const char* description);
		static Game::dvar_t* Dvar_Register_cg_fov(const char* dvarName, float value, float min, float max, unsigned int flags, const char* description);
		static Game::dvar_t* Dvar_Register_com_maxfps(const char* dvarName, int value, int min, int max, unsigned int flags, const char* description);
		static Game::dvar_t* Dvar_Register_profileMenuOption_volume(const char* dvarName, float value, float min, float max, unsigned int flags, const char* description);

		static void SetFromStringByNameSafeExternal(const char* dvarName, const char* string);
		static void SetFromStringByNameExternal(const char* dvarName, const char* string);

		static bool AreArchiveDvarsUnprotected();
		static bool IsSettingDvarsDisabled();
		static void DvarSetFromStringByName_Stub(const char* dvarName, const char* value);

		static const char* Dvar_EnumToString_Stub(const Game::dvar_t* dvar);
	};
}
