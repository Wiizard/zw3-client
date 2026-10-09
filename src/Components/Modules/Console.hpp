#pragma once

namespace Components
{
	class Console : public Component
	{
	public:
		struct Color
		{
			float r;
			float g;
			float b;
			float a;
		};

		Console();

		static bool IsOpen();

		using KeyObserver = std::function<bool(int key, int down)>;
		static void OnKey(const KeyObserver& observer);

		static void Print(const char* text);

		static void FreeNativeConsole();

	private:
		static bool isOpen;
		static std::vector<KeyObserver> keyObservers;
		static bool isBig;
		static bool isInstalled;

		static Utils::Hook keyEventHook;
		static Utils::Hook charEventHook;
		static Utils::Hook consolePrintHooks[3];

		static std::atomic_bool isShutdownRequested;
		static std::atomic_bool isWatchdogStarted;

		static void RequestShutdown(DWORD watchdogDelayMs);
		static void StartShutdownWatchdog(DWORD watchdogDelayMs);
		static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);

		static void Create();
		static const char* Input();
		static void Error(const char* fmt, ...);
		static void PrintCurses(const char* text);
		static void ShowPrompt();
		static void RefreshStatus();
		static void RefreshOutput();
		static void ScrollOutput(int amount);

		static void PushLine(const char* text, int length);
		static void AddLine(const char* text);
		static void FlushPendingLine();
		static void ClearScrollback();
		static void ScrollBy(int lines);

		static void ResetInput();
		static void LoadInput(const char* text);
		static void InsertInputChar(char character);
		static void PasteClipboard();
		static int MeasureInput(int from, int count);
		static int MeasurePrefix(const char* text, int count);
		static int HitColumn(const char* text, int length, float offsetX);
		static void AdjustInputScroll(float fieldWidth);

		static void PushHistory(const char* text);
		static void HistoryUp();
		static void HistoryDown();
		static void ExecuteInput();

		static void AddMatch(const char* name);
		static void CollectMatches(const char* prefix);
		static const char* NamePrefix();
		static bool IsTypingName();
		static void* FindDvar(const char* name);
		static void* FindCommand(const char* name);
		static void DvarValueString(void* dvar, int valueOffset, char* out, std::size_t outSize);
		static const char* FindBuiltinDescription(const char* name);

		static void SetOpen(bool open);
		static void FollowEngineClose();
		static void ToggleMode(bool big);
		static bool HandleKey(int key, int down);
		static void HandleChar(int character);

		static Game::Material* WhiteMaterial();
		static Game::Font_s* ConsoleFont();
		static bool IsRenderReady();
		static int TextWidth(const char* text);
		static int TextHeight();

		static void DrawRect(float x, float y, float w, float h, const Color& color);
		static void DrawText(const char* text, float x, float y, const Color& color);
		static void DrawTextWithCursor(const char* text, int maxChars, float x, float y,
			const Color& color, int cursorPos, char cursorChar);
		static void DrawBox(float x, float y, float w, float h, const Color& color);
		static void DrawHintBox(float x, float curY, int rows);
		static void DrawHintText(const char* text, float x, float curY, const Color& color);
		static void UpdateConsoleRect();
		static void DrawFrame();
		static void DrawInputHints(float hintX, float hintY, float inputY);

		static void CL_KeyEvent_Hook(int localClientNum, int key, int down, unsigned int time);
		static void CL_CharEvent_Hook(int localClientNum, int character);
		static void* CL_ConsolePrint_AddLine_Hook(int localClientNum, int channel, const char* text,
			int duration, int pixelWidth, unsigned char color, int flags);
	};
}
