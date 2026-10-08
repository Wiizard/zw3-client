#include "STDInclude.hpp"

#include "Branding.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"

namespace Components
{
	Game::dvar_t* Branding::cg_drawVersion = nullptr;
	Game::dvar_t* Branding::cg_drawVersionX = nullptr;
	Game::dvar_t* Branding::cg_drawVersionY = nullptr;

#if defined(DEBUG)
	constexpr const char* buildType = "Call of Duty: Zombie Warfare 3 (Debug)";
#else
	constexpr const char* buildType = "Call of Duty: Zombie Warfare 3";
#endif

	constexpr const char* buildNumberText = "";
	constexpr const char* consoleTitle = "Call of Duty: Zombie Warfare 3 [Console]";
	constexpr const char* dedicatedConsoleTitle = "Call of Duty: Zombie Warfare 3 [Dedicated]";

	constexpr std::uintptr_t Com_Init_Dvar_SetStringCall = 0x1401F589D;

	static const std::uint8_t setStringCall[] = { 0xE8, 0x6E, 0x21, 0x09, 0x00 };

	constexpr std::uintptr_t platformString = 0x14038A608;

	constexpr std::uintptr_t version = 0x141BD9AA0;

	constexpr std::uintptr_t getBuildNumber = 0x1401E6D10;

	static const std::uint8_t getBuildNumberEntry[] = { 0x48, 0x83, 0xEC, 0x28, 0x4C, 0x8D, 0x0D };

	constexpr std::uintptr_t LSP_WritePacketHeader_MSG_WriteStringCall = 0x1401B196E;

	static const std::uint8_t writeStringCall[] = { 0xE8, 0xED, 0x11, 0x05, 0x00 };

	constexpr std::uintptr_t ui_buildLocationRegisterCall = 0x140271570;

	static const std::uint8_t registerVec2Call[] = { 0xE8, 0x7B, 0x50, 0x01, 0x00 };

	constexpr std::uintptr_t UI_RefreshViewport_UI_DrawTextCall = 0x140270DAA;

	static const std::uint8_t drawTextCall[] = { 0xE8, 0x11, 0xC3, 0xFF, 0xFF };

	static const Utils::Hook::LeaSite Sys_CreateConsoleWindow_TitleLea = { 0x1402A94D6, Utils::Hook::leaR8, 0x1403A9BF8 };

	constexpr unsigned int dvarRom = 0x2000;

	constexpr std::uintptr_t fullScreenUiFlag = 0x140C5CEA8;
	constexpr std::uintptr_t clsState = 0x1406CECF8;
	constexpr int connectionActive = 9;

	constexpr std::uintptr_t scrPlaceFullUnsafe = 0x1406CDD60;

	constexpr std::uintptr_t UI_GetFontHandle = 0x14026EE10;
	constexpr std::uintptr_t UI_TextWidth = 0x140273090;
	constexpr std::uintptr_t UI_TextHeight = 0x140273050;

	using UI_DrawText_t = void(*)(const float* placement, const char* text, int maxChars, Game::Font_s* font,
		float x, float y, int horzAlign, int vertAlign, float scale, const float* color, int style);

	constexpr std::uintptr_t UI_DrawText = 0x14026D0C0;

	constexpr int alignRight = 3;
	constexpr int alignBottom = 3;

	static Utils::Hook getBuildNumberHook;
	static Utils::Hook setVersionHook;
	static Utils::Hook lspVersionHook;
	static Utils::Hook buildLocationHook;
	static Utils::Hook buildNumberDrawHook;

	void Branding::CG_DrawVersion()
	{
		const auto* const versionDvar = Utils::Hook::Get<const Game::dvar_t*>(version);

		if (!versionDvar || !versionDvar->current.string)
		{
			return;
		}

		constexpr float fontScale = 0.25f;
		constexpr int maxChars = std::numeric_limits<int>::max();
		constexpr float shadowColor[4] = { 0.0f, 0.0f, 0.0f, 0.69f };
		constexpr float color[4] = { 0.4f, 0.69f, 1.0f, 0.69f };

		const char* const text = versionDvar->current.string;
		const auto* const placement = reinterpret_cast<const float*>(Utils::Hook::Rebase(scrPlaceFullUnsafe));

		auto* const font = reinterpret_cast<Game::Font_s*(*)(const float*, int, float)>(Utils::Hook::Rebase(UI_GetFontHandle))(placement, 0, 0.583f);
		const int width = reinterpret_cast<int(*)(const char*, int, Game::Font_s*, float)>(Utils::Hook::Rebase(UI_TextWidth))(text, 0, font, fontScale);
		const int height = reinterpret_cast<int(*)(Game::Font_s*, float)>(Utils::Hook::Rebase(UI_TextHeight))(font, fontScale);

		const float offsetX = cg_drawVersionX->current.value;
		const float offsetY = cg_drawVersionY->current.value;
		const auto drawText = reinterpret_cast<UI_DrawText_t>(Utils::Hook::Rebase(UI_DrawText));

		drawText(placement, text, maxChars, font, 1.0f - (offsetX + static_cast<float>(width)),
			1.0f - (offsetY + static_cast<float>(height)), alignRight, alignBottom, fontScale, shadowColor, 0);
		drawText(placement, text, maxChars, font, (0.0f - static_cast<float>(width)) - offsetX,
			(0.0f - static_cast<float>(height)) - offsetY, alignRight, alignBottom, fontScale, color, 0);
	}

	void Branding::CG_DrawVersion_Hk()
	{
		if (!cg_drawVersion || !cg_drawVersion->current.enabled)
		{
			return;
		}

		if (!Utils::Hook::Get<int>(fullScreenUiFlag) || Utils::Hook::Get<int>(clsState) != connectionActive)
		{
			return;
		}

		CG_DrawVersion();
	}

	const char* Branding::GetBuildNumber()
	{
		return "ZW3-client (built " __DATE__ " " __TIME__ ")";
	}

	const char* Branding::GetVersionString()
	{
		const auto* const platform = reinterpret_cast<const char*>(Utils::Hook::Rebase(platformString));

		return Utils::String::VA("%s %s build %s %s", buildType, "(Beta)", GetBuildNumber(), platform);
	}

	void Branding::Dvar_SetVersionString(const Game::dvar_t* dvar, [[maybe_unused]] const char* value)
	{
		Game::Dvar_SetString(dvar, GetVersionString());
	}

	void Branding::MSG_WriteVersionStringHeader(Game::msg_t* msg, [[maybe_unused]] const char* string)
	{
		reinterpret_cast<void(*)(Game::msg_t*, const char*)>(lspVersionHook.GetOriginal())(msg, GetVersionString());
	}

	Game::dvar_t* Branding::Dvar_RegisterUIBuildLocation(const char* dvarName, [[maybe_unused]] float x, [[maybe_unused]] float y,
		float min, float max, [[maybe_unused]] unsigned int flags, const char* description)
	{
		return Game::Dvar_RegisterVec2(dvarName, -60.0f, 472.0f, min, max, dvarRom, description);
	}

	void Branding::UI_DrawBuildNumber_Hk(const float* placement, [[maybe_unused]] const char* text, int maxChars, Game::Font_s* font,
		float x, float y, int horzAlign, int vertAlign, float scale, [[maybe_unused]] const float* color, int style)
	{
		static const float buildLocColor[4] = { 1.0f, 1.0f, 1.0f, 0.8f };

		reinterpret_cast<UI_DrawText_t>(buildNumberDrawHook.GetOriginal())(placement, buildNumberText, maxChars, font,
			x, y, horzAlign, vertAlign, scale, buildLocColor, style);
	}

	void Branding::RegisterBrandingDvars()
	{
#if defined(DEBUG)
		constexpr bool isDrawnByDefault = true;
#else
		constexpr bool isDrawnByDefault = false;
#endif

		cg_drawVersion = Dvar::Register("cg_drawVersion", isDrawnByDefault, Game::DVAR_NONE, "Draw the game version").Get();
		cg_drawVersionX = Dvar::Register("cg_drawVersionX", 10.0f, 0.0f, 512.0f, Game::DVAR_NONE, "X offset for the version string").Get();
		cg_drawVersionY = Dvar::Register("cg_drawVersionY", 455.0f, 0.0f, 512.0f, Game::DVAR_NONE, "Y offset for the version string").Get();
	}

	Branding::Branding()
	{
		const bool isExpected = Utils::Hook::IsLeaIntact(Sys_CreateConsoleWindow_TitleLea)
			&& Utils::Hook::MatchesBytes(getBuildNumber, getBuildNumberEntry, sizeof(getBuildNumberEntry))
			&& Utils::Hook::MatchesBytes(Com_Init_Dvar_SetStringCall, setStringCall, sizeof(setStringCall))
			&& Utils::Hook::MatchesBytes(LSP_WritePacketHeader_MSG_WriteStringCall, writeStringCall, sizeof(writeStringCall))
			&& Utils::Hook::MatchesBytes(ui_buildLocationRegisterCall, registerVec2Call, sizeof(registerVec2Call))
			&& Utils::Hook::MatchesBytes(UI_RefreshViewport_UI_DrawTextCall, drawTextCall, sizeof(drawTextCall));

		if (!isExpected)
		{
			Logger::Error("branding: the version code does not read as expected, the game keeps its own version text\n");
			return;
		}

		bool isSeated = getBuildNumberHook.Initialize(getBuildNumber, reinterpret_cast<void*>(GetBuildNumber), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = setVersionHook.Initialize(Com_Init_Dvar_SetStringCall, reinterpret_cast<void*>(Dvar_SetVersionString), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = lspVersionHook.Initialize(LSP_WritePacketHeader_MSG_WriteStringCall, reinterpret_cast<void*>(MSG_WriteVersionStringHeader), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = buildLocationHook.Initialize(ui_buildLocationRegisterCall, reinterpret_cast<void*>(Dvar_RegisterUIBuildLocation), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = buildNumberDrawHook.Initialize(UI_RefreshViewport_UI_DrawTextCall, reinterpret_cast<void*>(UI_DrawBuildNumber_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		const char* title = consoleTitle;

		if (Dedicated::IsEnabled())
		{
			title = dedicatedConsoleTitle;
		}

		if (!isSeated || !Utils::Hook::TryPointLeaAt(Sys_CreateConsoleWindow_TitleLea, Utils::Hook::PlaceNearImage(title)))
		{
			getBuildNumberHook.Uninstall();
			setVersionHook.Uninstall();
			lspVersionHook.Uninstall();
			buildLocationHook.Uninstall();
			buildNumberDrawHook.Uninstall();

			Logger::Error("branding: could not seat the version hooks, the game keeps its own version text\n");
			return;
		}

		getBuildNumberHook.Quick();
		setVersionHook.Quick();
		lspVersionHook.Quick();
		buildLocationHook.Quick();
		buildNumberDrawHook.Quick();

		Scheduler::Loop(CG_DrawVersion_Hk, Scheduler::Pipeline::RENDERER);
	}
}
