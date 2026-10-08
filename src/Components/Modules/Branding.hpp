#pragma once

namespace Components
{
	class Branding : public Component
	{
	public:
		Branding();

		static const char* GetBuildNumber();
		static const char* GetVersionString();

	private:
		static Game::dvar_t* cg_drawVersion;
		static Game::dvar_t* cg_drawVersionX;
		static Game::dvar_t* cg_drawVersionY;

		static void CG_DrawVersion();
		static void CG_DrawVersion_Hk();

		static void Dvar_SetVersionString(const Game::dvar_t* dvar, const char* value);
		static void MSG_WriteVersionStringHeader(Game::msg_t* msg, const char* string);

		static Game::dvar_t* Dvar_RegisterUIBuildLocation(const char* dvarName, float x, float y, float min, float max, unsigned int flags, const char* description);
		static void UI_DrawBuildNumber_Hk(const float* placement, const char* text, int maxChars, Game::Font_s* font,
			float x, float y, int horzAlign, int vertAlign, float scale, const float* color, int style);

		static void RegisterBrandingDvars();
	};
}
