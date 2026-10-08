#include "STDInclude.hpp"

#include "Console.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "TextRenderer.hpp"

#ifdef MOUSE_MOVED
	#undef MOUSE_MOVED
#endif

#include <curses.h>

namespace Components
{
	bool Console::isOpen = false;
	std::vector<Console::KeyObserver> Console::keyObservers;
	bool Console::isBig = false;
	bool Console::isInstalled = false;

	Utils::Hook Console::keyEventHook;
	Utils::Hook Console::charEventHook;
	Utils::Hook Console::consolePrintHooks[3];

	std::atomic_bool Console::isShutdownRequested = false;
	std::atomic_bool Console::isWatchdogStarted = false;

	constexpr int maxLines = 2048;
	constexpr int lineLength = 256;
	constexpr int inputCapacity = 256;
	constexpr int historyMax = 32;
	constexpr int maxMatches = 24;

	static char lineBuffer[maxLines][lineLength];
	static int writeHead = 0;
	static int lineBufferCount = 0;
	static int scrollOffset = 0;
	static int visibleLineCount = 0;
	static SRWLOCK lineBufferLock = SRWLOCK_INIT;

	static char inputBuffer[inputCapacity];
	static int inputLength = 0;
	static int inputCursor = 0;
	static int inputScroll = 0;
	static int inputDrawWidth = 0;
	static bool isOverstrike = false;

	static char historyBuffer[historyMax][inputCapacity];
	static int historyCount = 0;
	static int historyBrowse = -1;

	static char matchBuffer[maxMatches][64];
	static int matchCount = 0;
	static int matchTotal = 0;

	static float minX = 0.0f;
	static float minY = 0.0f;
	static float maxX = 0.0f;
	static float maxY = 0.0f;
	static float fontHeight = 16.0f;

	constexpr float textScale = 1.0f;
	constexpr float pad = 6.0f;
	constexpr float inputBandHeight = 32.0f;
	constexpr float valueColumnOffset = 348.0f;
	constexpr float scrollBarWidth = 10.0f;
	constexpr int nameClamp = 28;
	constexpr int valueClamp = 40;

	constexpr const char* promptText = "Call of Duty: Zombie Warfare 3> ";
	constexpr const char* versionText = "Call of Duty: Zombie Warfare 3";

	constexpr Console::Color inputBoxColor{ 0.25f, 0.25f, 0.20f, 1.00f };
	constexpr Console::Color inputHintBoxColor{ 0.40f, 0.40f, 0.35f, 1.00f };
	constexpr Console::Color outputWindowColor{ 0.35f, 0.35f, 0.30f, 0.75f };
	constexpr Console::Color outputBarColor{ 1.00f, 1.00f, 0.95f, 0.60f };
	constexpr Console::Color outputSliderColor{ 0.15f, 0.15f, 0.10f, 0.60f };
	constexpr Console::Color outputTextColor{ 1.00f, 1.00f, 1.00f, 1.00f };
	constexpr Console::Color versionColor{ 1.00f, 1.00f, 0.00f, 1.00f };
	constexpr Console::Color dvarNameColor{ 1.00f, 1.00f, 0.80f, 1.00f };
	constexpr Console::Color dvarValueColor{ 1.00f, 1.00f, 1.00f, 1.00f };
	constexpr Console::Color commandNameColor{ 0.80f, 0.80f, 1.00f, 1.00f };
	constexpr Console::Color descriptionColor{ 0.80f, 0.80f, 1.00f, 1.00f };

	constexpr std::uintptr_t cls_whiteMaterial = 0x140C5CF08;
	constexpr std::uintptr_t cls_consoleFont = 0x140C5CF18;
	constexpr std::uintptr_t keyCatchers = 0x1406CECF0;
	constexpr std::uintptr_t cmd_functions = 0x141BBC798;
	constexpr std::uintptr_t dvarHashTable = 0x1466E3A60;

	constexpr int dvarName = 0x0;
	constexpr int dvarType = 0xC;
	constexpr int dvarCurrent = 0x10;
	constexpr int dvarReset = 0x30;
	constexpr int dvarHashNext = 0x58;
	constexpr int dvarHashBuckets = 1024;

	constexpr unsigned char dvarTypeBool = 0;
	constexpr unsigned char dvarTypeFloat = 1;
	constexpr unsigned char dvarTypeInt = 5;
	constexpr unsigned char dvarTypeEnum = 6;
	constexpr unsigned char dvarTypeString = 7;

	constexpr int cmdNext = 0x0;
	constexpr int cmdName = 0x8;

	constexpr int keyTab = 0x9;
	constexpr int keyEnter = 0xD;
	constexpr int keyEscape = 0x1B;
	constexpr int keyConsole = 0x7E;
	constexpr int keyBackspace = 0x7F;
	constexpr int keyUpArrow = 0x9A;
	constexpr int keyDownArrow = 0x9B;
	constexpr int keyLeftArrow = 0x9C;
	constexpr int keyRightArrow = 0x9D;
	constexpr int keyInsert = 0xA1;
	constexpr int keyPageDown = 0xA3;
	constexpr int keyPageUp = 0xA4;
	constexpr int keyHome = 0xA5;
	constexpr int keyEnd = 0xA6;
	constexpr int keyMouseWheelDown = 0xCD;
	constexpr int keyMouseWheelUp = 0xCE;

	constexpr std::uintptr_t CL_KeyEventCall = 0x1401F402D;
	constexpr std::uintptr_t CL_CharEventCall = 0x1401F4018;
	constexpr std::uintptr_t CL_ConsolePrint_AddLineCalls[] = { 0x1400EB814, 0x1400EB88B, 0x1400EBFCC };

	constexpr std::uintptr_t Sys_Error = 0x1402A4F90;
	static const std::uint8_t sysErrorEntry[] = { 0x48, 0x89, 0x4C, 0x24, 0x08 };

	static Utils::Hook sysErrorHook;

	static void StdOutError(const char* fmt, ...)
	{
		char buffer[4096]{};

		va_list ap;
		va_start(ap, fmt);
		vsnprintf_s(buffer, _TRUNCATE, fmt, ap);
		va_end(ap);

		perror(buffer);
		std::fflush(stderr);

		ExitProcess(1);
	}

	constexpr int outputHeight = 250;

	static WINDOW* outputWindow = nullptr;
	static WINDOW* inputWindow = nullptr;
	static WINDOW* infoWindow = nullptr;

	static int outputTop = 0;
	static int outBuffer = 0;
	static int lastRefresh = 0;
	static int consoleWidth = 80;
	static int consoleHeight = 25;

	static char promptLine[1024]{};
	static char lastPromptLine[1024]{};
	static int promptLength = 0;
	static bool hasPrompt = false;

	static std::recursive_mutex cursesMutex;

	constexpr std::uintptr_t Sys_GetEvent_Sys_ConsoleInputCall = 0x1402A5218;
	constexpr std::uintptr_t Sys_ConsoleInput = 0x1402A9310;

	static Utils::Hook consoleInputHook;

	void Console::ShowPrompt()
	{
		wattron(inputWindow, COLOR_PAIR(10) | A_BOLD);
		wprintw(inputWindow, "%s", promptText);
	}

	void Console::RefreshOutput()
	{
		int top = 0;

		if (outputTop > 0)
		{
			top = outputTop - 1;
		}

		prefresh(outputWindow, top, 0, 1, 0, consoleHeight - 2, consoleWidth - 1);
	}

	void Console::ScrollOutput(int amount)
	{
		const int maxTop = outputHeight - (consoleHeight - 2);

		outputTop += amount;

		if (outputTop > maxTop)
		{
			outputTop = maxTop;
		}
		else if (outputTop < 0)
		{
			outputTop = 0;
		}

		if (outBuffer >= 0)
		{
			outBuffer += amount;

			if (outBuffer >= consoleHeight)
			{
				outBuffer = -1;
			}

			if (outputTop < consoleHeight)
			{
				outputTop = 0;
			}
		}
	}

	void Console::RefreshStatus()
	{
		std::lock_guard lock(cursesMutex);

		if (!infoWindow || !hasPrompt)
		{
			return;
		}

		const std::string hostname = TextRenderer::StripColors(Dvar::Var("sv_hostname").Get<std::string>());
		const std::string mapname = Dvar::Var("mapname").Get<std::string>();

		SetConsoleTitleA(hostname.data());

		int clientCount = 0;
		int maxClientCount = *Game::svs_clientCount;

		if (maxClientCount)
		{
			for (int i = 0; i < maxClientCount; ++i)
			{
				if (Game::svs_clients[i].header.state >= Game::CS_CONNECTED)
				{
					++clientCount;
				}
			}
		}
		else
		{
			const Dvar::Var partyMaxPlayers("party_maxplayers");
			maxClientCount = 18;

			if (partyMaxPlayers.IsValid())
			{
				maxClientCount = partyMaxPlayers.Get<int>();
			}

			clientCount = Game::PartyClient_CountMembersEvenIfInactive(Game::g_lobbyData);
		}

		const char* shownMap = "none";

		if (!mapname.empty())
		{
			shownMap = mapname.data();
		}

		wclear(infoWindow);
		wprintw(infoWindow, "%s : %d/%d players : map %s", hostname.data(), clientCount, maxClientCount, shownMap);
		wnoutrefresh(infoWindow);
	}

	const char* Console::Input()
	{
		std::lock_guard lock(cursesMutex);

		if (!inputWindow)
		{
			return nullptr;
		}

		if (!hasPrompt)
		{
			ShowPrompt();
			wrefresh(inputWindow);
			hasPrompt = true;
		}

		const auto currentTime = static_cast<int>(GetTickCount64());

		if (currentTime - lastRefresh > 250)
		{
			RefreshOutput();
			lastRefresh = currentTime;
		}

		const int key = wgetch(inputWindow);

		if (key == ERR)
		{
			return nullptr;
		}

		switch (key)
		{
		case '\r':
		case 459:
		{
			wattron(outputWindow, COLOR_PAIR(10) | A_BOLD);
			wprintw(outputWindow, "%s", "]");

			if (promptLength)
			{
				wprintw(outputWindow, "%s", promptLine);
			}

			wprintw(outputWindow, "%s", "\n");
			wattroff(outputWindow, A_BOLD);
			wclear(inputWindow);

			ShowPrompt();
			wrefresh(inputWindow);

			ScrollOutput(1);
			RefreshOutput();

			if (promptLength)
			{
				strcpy_s(lastPromptLine, promptLine);
				strcat_s(promptLine, "\n");
				promptLength = 0;
				return promptLine;
			}

			break;
		}
		case 'c' - 'a' + 1:
		case 27:
		{
			promptLine[0] = '\0';
			promptLength = 0;

			wclear(inputWindow);
			ShowPrompt();
			wrefresh(inputWindow);
			break;
		}
		case 8:
		{
			if (promptLength > 0)
			{
				--promptLength;
				promptLine[promptLength] = '\0';

				wprintw(inputWindow, "%c %c", static_cast<char>(key), static_cast<char>(key));
				wrefresh(inputWindow);
			}

			break;
		}
		case KEY_PPAGE:
		{
			ScrollOutput(-1);
			RefreshOutput();
			break;
		}
		case KEY_NPAGE:
		{
			ScrollOutput(1);
			RefreshOutput();
			break;
		}
		case KEY_UP:
		{
			wclear(inputWindow);
			ShowPrompt();
			wprintw(inputWindow, "%s", lastPromptLine);
			wrefresh(inputWindow);

			strcpy_s(promptLine, lastPromptLine);
			promptLength = static_cast<int>(std::strlen(promptLine));
			break;
		}
		default:
		{
			if (key <= 127 && promptLength < 1022)
			{
				promptLine[promptLength++] = static_cast<char>(key);
				promptLine[promptLength] = '\0';

				wprintw(inputWindow, "%c", static_cast<char>(key));
				wrefresh(inputWindow);
			}

			break;
		}
		}

		return nullptr;
	}

	void Console::Create()
	{
		std::lock_guard lock(cursesMutex);

		if (!GetConsoleWindow() && !AllocConsole())
		{
			Logger::Error("console: could not open a console window: {}\n", GetLastError());
			return;
		}

		if (GetFileType(GetStdHandle(STD_INPUT_HANDLE)) != FILE_TYPE_CHAR)
		{
			MessageBoxA(nullptr, "Console not supported, please use '-stdout'!", "Zombie Warfare 3", MB_ICONERROR);
			TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
		}

		outputTop = 0;
		outBuffer = 0;
		lastRefresh = 0;
		promptLength = 0;
		hasPrompt = false;

		CONSOLE_SCREEN_BUFFER_INFO info;

		if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
		{
			consoleWidth = info.dwSize.X;
			consoleHeight = info.srWindow.Bottom - info.srWindow.Top + 1;
		}

		DWORD inputMode = 0;
		const HANDLE consoleInput = GetStdHandle(STD_INPUT_HANDLE);

		if (GetConsoleMode(consoleInput, &inputMode))
		{
			SetConsoleMode(consoleInput, (inputMode & ~ENABLE_QUICK_EDIT_MODE) | ENABLE_EXTENDED_FLAGS);
		}

		initscr();
		raw();
		noecho();

		outputWindow = newpad(outputHeight, consoleWidth);
		inputWindow = newwin(1, consoleWidth, consoleHeight - 1, 0);
		infoWindow = newwin(1, consoleWidth, 0, 0);

		scrollok(outputWindow, true);
		idlok(outputWindow, true);
		scrollok(inputWindow, true);
		nodelay(inputWindow, true);
		keypad(inputWindow, true);

		if (has_colors())
		{
			start_color();
			init_pair(1, COLOR_BLACK, COLOR_WHITE);
			init_pair(2, COLOR_WHITE, COLOR_BLACK);
			init_pair(3, COLOR_RED, COLOR_BLACK);
			init_pair(4, COLOR_GREEN, COLOR_BLACK);
			init_pair(5, COLOR_YELLOW, COLOR_BLACK);
			init_pair(6, COLOR_BLUE, COLOR_BLACK);
			init_pair(7, COLOR_CYAN, COLOR_BLACK);
			init_pair(8, COLOR_RED, COLOR_BLACK);
			init_pair(9, COLOR_WHITE, COLOR_BLACK);
			init_pair(10, COLOR_WHITE, COLOR_BLACK);
		}

		wbkgd(infoWindow, COLOR_PAIR(1));

		wrefresh(infoWindow);
		wrefresh(inputWindow);

		AcquireSRWLockShared(&lineBufferLock);

		for (int i = lineBufferCount; i > 0; --i)
		{
			PrintCurses(lineBuffer[(writeHead - i + maxLines) % maxLines]);
			PrintCurses("\n");
		}

		ReleaseSRWLockShared(&lineBufferLock);

		RefreshOutput();
	}

	void Console::Error(const char* fmt, ...)
	{
		char buffer[4096]{};

		va_list ap;
		va_start(ap, fmt);
		vsnprintf_s(buffer, _TRUNCATE, fmt, ap);
		va_end(ap);

		Logger::Error("{}\n", buffer);

		{
			std::lock_guard lock(cursesMutex);

			if (outputWindow)
			{
				RefreshOutput();
			}
		}

		TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
	}

	void Console::PrintCurses(const char* text)
	{
		std::lock_guard lock(cursesMutex);

		if (!outputWindow)
		{
			return;
		}

		const char* character = text;

		while (*character != '\0')
		{
			if (*character == '^')
			{
				++character;

				if (*character == '\0')
				{
					break;
				}

				const int color = *character - '0';

				if (color < 9 && color > 0)
				{
					wattron(outputWindow, COLOR_PAIR(color + 2));
					++character;
					continue;
				}
			}

			waddch(outputWindow, *character);
			++character;
		}

		wattron(outputWindow, COLOR_PAIR(9));

		RefreshOutput();
	}

	struct BuiltinCommand
	{
		const char* name;
		const char* description;
	};

	static const BuiltinCommand builtins[] =
	{
		{ "clear", "clears the console scrollback" },
		{ "echo", "echo <text>, prints text to the console" },
		{ "quit", "exits the game" },
	};

	Game::Material* Console::WhiteMaterial()
	{
		auto* const slot = reinterpret_cast<Game::Material**>(Utils::Hook::Rebase(cls_whiteMaterial));

		if (*slot)
		{
			return *slot;
		}

		return Game::Material_RegisterHandle("white", 7);
	}

	Game::Font_s* Console::ConsoleFont()
	{
		auto* const slot = reinterpret_cast<Game::Font_s**>(Utils::Hook::Rebase(cls_consoleFont));

		if (*slot)
		{
			return *slot;
		}

		return Game::R_RegisterFont("fonts/consoleFont", 7);
	}

	bool Console::IsRenderReady()
	{
		return WhiteMaterial() != nullptr && ConsoleFont() != nullptr;
	}

	int Console::TextWidth(const char* text)
	{
		auto* const font = ConsoleFont();

		if (!font || !text)
		{
			return 0;
		}

		return static_cast<int>(Game::R_TextWidth(text, 0x7FFFFFFF, font) * textScale);
	}

	int Console::TextHeight()
	{
		auto* const font = ConsoleFont();

		if (!font)
		{
			return 0;
		}

		return static_cast<int>(Game::R_TextHeight(font) * textScale);
	}

	void Console::FreeNativeConsole()
	{
		if (!Flags::HasFlag("stdout") && (!Dedicated::IsEnabled() || Flags::HasFlag("console")))
		{
			FreeConsole();
		}
	}

	bool Console::IsOpen()
	{
		return isOpen;
	}

	void Console::OnKey(const KeyObserver& observer)
	{
		keyObservers.push_back(observer);
	}

	static int LengthBeforeInlineMaterial(const char* text, int length)
	{
		for (int i = 0; i + 1 < length; ++i)
		{
			if (text[i] == '^' && (text[i + 1] == 1 || text[i + 1] == 2))
			{
				return i;
			}
		}

		return length;
	}

	void Console::PushLine(const char* text, int length)
	{
		length = LengthBeforeInlineMaterial(text, length);

		if (length >= lineLength)
		{
			length = lineLength - 1;
		}

		std::memcpy(lineBuffer[writeHead], text, length);
		lineBuffer[writeHead][length] = '\0';

		writeHead = (writeHead + 1) % maxLines;

		if (lineBufferCount < maxLines)
		{
			++lineBufferCount;
		}
	}

	void Console::Print(const char* text)
	{
		if (!text)
		{
			return;
		}

		static const bool shouldPrintToStdout = Flags::HasFlag("stdout");

		if (shouldPrintToStdout)
		{
			std::printf("%s", text);
			std::fflush(stdout);
		}

		PrintCurses(text);

		Logger::NetworkLog(text, false);

		AcquireSRWLockExclusive(&lineBufferLock);

		const char* lineStart = text;

		while (true)
		{
			const char* const lineEnd = std::strchr(lineStart, '\n');

			if (!lineEnd)
			{
				const auto remaining = static_cast<int>(std::strlen(lineStart));

				if (remaining > 0)
				{
					PushLine(lineStart, remaining);
				}

				break;
			}

			PushLine(lineStart, static_cast<int>(lineEnd - lineStart));
			lineStart = lineEnd + 1;
		}

		ReleaseSRWLockExclusive(&lineBufferLock);
	}

	void Console::ClearScrollback()
	{
		AcquireSRWLockExclusive(&lineBufferLock);

		writeHead = 0;
		lineBufferCount = 0;
		scrollOffset = 0;

		ReleaseSRWLockExclusive(&lineBufferLock);
	}

	void Console::ScrollBy(int lines)
	{
		scrollOffset += lines;

		const int maximum = std::max(0, lineBufferCount - visibleLineCount);

		scrollOffset = std::clamp(scrollOffset, 0, maximum);
	}

	void Console::ResetInput()
	{
		inputBuffer[0] = '\0';
		inputLength = 0;
		inputCursor = 0;
		inputScroll = 0;
		inputDrawWidth = 0;
	}

	void Console::LoadInput(const char* text)
	{
		std::snprintf(inputBuffer, sizeof(inputBuffer), "%s", text);
		inputLength = static_cast<int>(std::strlen(inputBuffer));
		inputCursor = inputLength;
		inputScroll = 0;
	}

	void Console::InsertInputChar(char character)
	{
		if (isOverstrike && inputCursor < inputLength)
		{
			inputBuffer[inputCursor] = character;
			++inputCursor;
			return;
		}

		if (inputLength + 1 >= inputCapacity)
		{
			return;
		}

		std::memmove(inputBuffer + inputCursor + 1, inputBuffer + inputCursor,
			static_cast<std::size_t>(inputLength - inputCursor) + 1);

		inputBuffer[inputCursor] = character;
		++inputCursor;
		++inputLength;
	}

	int Console::MeasureInput(int from, int count)
	{
		char slice[inputCapacity];
		std::snprintf(slice, sizeof(slice), "%.*s", count, inputBuffer + from);
		return TextWidth(slice);
	}

	void Console::AdjustInputScroll(float fieldWidth)
	{
		if (static_cast<float>(TextWidth(inputBuffer)) < fieldWidth)
		{
			inputScroll = 0;
			inputDrawWidth = inputLength;
			return;
		}

		while (inputScroll > 0
			&& static_cast<float>(MeasureInput(inputScroll - 1, inputLength)) < fieldWidth)
		{
			--inputScroll;
		}

		while (inputScroll < inputCursor
			&& static_cast<float>(MeasureInput(inputScroll, inputCursor - inputScroll)) > fieldWidth)
		{
			++inputScroll;
		}

		if (inputScroll > inputCursor)
		{
			inputScroll = inputCursor;
		}

		inputDrawWidth = inputCursor - inputScroll;

		while (inputScroll + inputDrawWidth < inputLength
			&& static_cast<float>(MeasureInput(inputScroll, inputDrawWidth + 1)) <= fieldWidth)
		{
			++inputDrawWidth;
		}
	}

	void Console::PushHistory(const char* text)
	{
		if (!text || !*text)
		{
			return;
		}

		if (historyCount > 0 && std::strcmp(historyBuffer[0], text) == 0)
		{
			return;
		}

		const int keep = std::min(historyCount, historyMax - 1);

		for (int i = keep; i > 0; --i)
		{
			std::memcpy(historyBuffer[i], historyBuffer[i - 1], inputCapacity);
		}

		std::snprintf(historyBuffer[0], inputCapacity, "%s", text);

		if (historyCount < historyMax)
		{
			++historyCount;
		}
	}

	void Console::HistoryUp()
	{
		if (historyCount == 0 || historyBrowse + 1 >= historyCount)
		{
			return;
		}

		++historyBrowse;
		LoadInput(historyBuffer[historyBrowse]);
	}

	void Console::HistoryDown()
	{
		if (historyBrowse < 0)
		{
			return;
		}

		--historyBrowse;

		if (historyBrowse < 0)
		{
			ResetInput();
			return;
		}

		LoadInput(historyBuffer[historyBrowse]);
	}

	void* Console::FindDvar(const char* name)
	{
		return Game::Dvar_FindVar(name);
	}

	void* Console::FindCommand(const char* name)
	{
		void* command = *reinterpret_cast<void**>(Utils::Hook::Rebase(cmd_functions));

		while (command)
		{
			const char* const entry =
				*reinterpret_cast<const char* const*>(static_cast<char*>(command) + cmdName);

			if (entry && _stricmp(entry, name) == 0)
			{
				return command;
			}

			command = *reinterpret_cast<void**>(static_cast<char*>(command) + cmdNext);
		}

		return nullptr;
	}

	void Console::DvarValueString(void* dvar, int valueOffset, char* out, std::size_t outSize)
	{
		const unsigned char type = *(static_cast<unsigned char*>(dvar) + dvarType);
		const void* const value = static_cast<char*>(dvar) + valueOffset;

		switch (type)
		{
		case dvarTypeBool:
			std::snprintf(out, outSize, "%d", *static_cast<const unsigned char*>(value) ? 1 : 0);
			return;

		case dvarTypeFloat:
			std::snprintf(out, outSize, "%g", *static_cast<const float*>(value));
			return;

		case dvarTypeInt:
		case dvarTypeEnum:
			std::snprintf(out, outSize, "%d", *static_cast<const int*>(value));
			return;

		case dvarTypeString:
		{
			const char* const text = *static_cast<const char* const*>(value);
			std::snprintf(out, outSize, "%s", text ? text : "");
			return;
		}

		default:
			std::snprintf(out, outSize, "?");
			return;
		}
	}

	const char* Console::FindBuiltinDescription(const char* name)
	{
		for (const auto& builtin : builtins)
		{
			if (_stricmp(builtin.name, name) == 0)
			{
				return builtin.description;
			}
		}

		return nullptr;
	}

	void Console::AddMatch(const char* name)
	{
		++matchTotal;

		if (matchCount < maxMatches)
		{
			std::snprintf(matchBuffer[matchCount], sizeof(matchBuffer[0]), "%s", name);
			++matchCount;
		}
	}

	void Console::CollectMatches(const char* prefix)
	{
		matchCount = 0;
		matchTotal = 0;

		const auto prefixLength = std::strlen(prefix);

		if (prefixLength == 0)
		{
			return;
		}

		for (const auto& builtin : builtins)
		{
			if (_strnicmp(builtin.name, prefix, prefixLength) == 0)
			{
				AddMatch(builtin.name);
			}
		}

		void* command = *reinterpret_cast<void**>(Utils::Hook::Rebase(cmd_functions));

		while (command)
		{
			const char* const name =
				*reinterpret_cast<const char* const*>(static_cast<char*>(command) + cmdName);

			if (name && _strnicmp(name, prefix, prefixLength) == 0 && !FindBuiltinDescription(name))
			{
				AddMatch(name);
			}

			command = *reinterpret_cast<void**>(static_cast<char*>(command) + cmdNext);
		}

		auto* const buckets = reinterpret_cast<void**>(Utils::Hook::Rebase(dvarHashTable));

		for (int bucket = 0; bucket < dvarHashBuckets; ++bucket)
		{
			void* dvar = buckets[bucket];

			while (dvar)
			{
				const char* const name =
					*reinterpret_cast<const char* const*>(static_cast<char*>(dvar) + dvarName);

				if (name && _strnicmp(name, prefix, prefixLength) == 0)
				{
					AddMatch(name);
				}

				dvar = *reinterpret_cast<void**>(static_cast<char*>(dvar) + dvarHashNext);
			}
		}
	}

	const char* Console::NamePrefix()
	{
		const char* text = inputBuffer;

		while (*text == '/' || *text == '\\')
		{
			++text;
		}

		return text;
	}

	bool Console::IsTypingName()
	{
		const char* const text = NamePrefix();

		return *text != '\0' && std::strchr(text, ' ') == nullptr;
	}

	void Console::ExecuteInput()
	{
		if (inputLength == 0)
		{
			return;
		}

		char command[inputCapacity];
		std::snprintf(command, sizeof(command), "%s", NamePrefix());

		char echo[inputCapacity + 16];
		std::snprintf(echo, sizeof(echo), "%s%s", promptText, command);
		Print(echo);

		PushHistory(inputBuffer);
		ResetInput();
		historyBrowse = -1;
		scrollOffset = 0;

		char firstToken[64];
		std::snprintf(firstToken, sizeof(firstToken), "%.*s",
			static_cast<int>(std::strcspn(command, " 	")), command);

		if (*firstToken && !FindCommand(firstToken) && !FindDvar(firstToken))
		{
			Print(Utils::String::VA("unknown command or dvar: %s", firstToken));
			return;
		}

		Game::Cmd_ExecuteSingleCommand(0, 0, command);
	}

	void Console::SetOpen(bool open)
	{
		isOpen = open;

		auto* const catchers = reinterpret_cast<volatile unsigned int*>(Utils::Hook::Rebase(keyCatchers));

		if (isOpen)
		{
			*catchers |= 1u;
		}
		else
		{
			*catchers &= ~1u;
		}

		ResetInput();
		scrollOffset = 0;
		historyBrowse = -1;
	}

	void Console::FollowEngineClose()
	{
		if (!isOpen)
		{
			return;
		}

		const auto* const catchers = reinterpret_cast<const volatile unsigned int*>(Utils::Hook::Rebase(keyCatchers));

		if (*catchers & Game::KEYCATCH_CONSOLE)
		{
			return;
		}

		SetOpen(false);
	}

	void Console::ToggleMode(bool big)
	{
		if (!isOpen)
		{
			isBig = big;
			SetOpen(true);
			return;
		}

		SetOpen(false);
	}

	void Console::DrawRect(float x, float y, float w, float h, const Color& color)
	{
		auto* const material = WhiteMaterial();

		if (!material)
		{
			return;
		}

		const float rgba[4] = { color.r, color.g, color.b, color.a };
		Game::CL_DrawStretchPicPhysical(x, y, w, h, 0.0f, 0.0f, 0.0f, 0.0f, rgba, material);
	}

	void Console::DrawText(const char* text, float x, float y, const Color& color)
	{
		if (!text || !*text)
		{
			return;
		}

		auto* const font = ConsoleFont();

		if (!font)
		{
			return;
		}

		const float rgba[4] = { color.r, color.g, color.b, color.a };
		Game::R_AddCmdDrawText(text, 0x7FFFFFFF, font, x, y, textScale, textScale, 0.0f, rgba, 0);
	}

	void Console::DrawTextWithCursor(const char* text, int maxChars, float x, float y,
		const Color& color, int cursorPos, char cursorChar)
	{
		auto* const font = ConsoleFont();

		if (!font || !text)
		{
			return;
		}

		const float rgba[4] = { color.r, color.g, color.b, color.a };
		Game::R_AddCmdDrawTextWithCursor(text, maxChars, font, x, y, textScale, textScale, 0.0f,
			rgba, 0, cursorPos, cursorChar);
	}

	void Console::DrawBox(float x, float y, float w, float h, const Color& color)
	{
		DrawRect(x, y, w, h, color);

		const Color edge{ color.r * 0.5f, color.g * 0.5f, color.b * 0.5f, color.a };

		DrawRect(x, y, 2.0f, h, edge);
		DrawRect(x + w - 2.0f, y, 2.0f, h, edge);
		DrawRect(x, y, w, 2.0f, edge);
		DrawRect(x, y + h - 2.0f, w, 2.0f, edge);
	}

	void Console::DrawHintBox(float x, float curY, int rows)
	{
		DrawBox(x - pad, curY - pad, maxX - x + pad,
			static_cast<float>(rows) * fontHeight + 2.0f * pad, inputHintBoxColor);
	}

	void Console::DrawHintText(const char* text, float x, float curY, const Color& color)
	{
		DrawText(text, x, curY + fontHeight, color);
	}

	void Console::UpdateConsoleRect()
	{
		const float* const place = Game::ScrPlace_GetActivePlacement(0);

		if (!place)
		{
			minX = 0.0f;
			minY = 0.0f;
			maxX = 640.0f;
			maxY = 480.0f;
			return;
		}

		minX = std::floor(place[0] * 4.0f + place[14]);
		minY = std::floor(place[1] * 4.0f + place[15]);
		maxX = std::floor(place[0] * -4.0f + place[16]);
		maxY = std::floor(place[1] * -4.0f + place[17]);
	}

	void Console::DrawFrame()
	{
		FollowEngineClose();

		if (!isOpen || !IsRenderReady())
		{
			return;
		}

		UpdateConsoleRect();

		const int measured = TextHeight();
		fontHeight = (measured > 0) ? static_cast<float>(measured) : 16.0f;

		const float fontH = fontHeight;
		const float consoleW = maxX - minX;
		const float textX = minX + pad;
		const float inputY = minY + pad;

		DrawBox(minX, minY, consoleW, fontH + 2.0f * pad, inputBoxColor);
		DrawText(promptText, textX, inputY + fontH, outputTextColor);

		const float hintX = textX + static_cast<float>(TextWidth(promptText));
		const float hintY = inputY + 2.0f * fontH;

		AdjustInputScroll(maxX - pad - hintX);

		const char caret = isOverstrike ? '_' : '|';
		DrawTextWithCursor(inputBuffer + inputScroll, inputDrawWidth, hintX, inputY + fontH,
			outputTextColor, inputCursor - inputScroll, caret);

		if (isBig)
		{
			const float outputY = minY + inputBandHeight;
			const float outputH = (maxY - minY) - inputBandHeight;
			DrawBox(minX, outputY, consoleW, outputH, outputWindowColor);

			const float textTop = outputY + pad;
			const float textW = consoleW - 2.0f * pad;
			const float textH = outputH - 2.0f * pad;
			DrawText(versionText, textX, textTop + textH - 16.0f + fontH, versionColor);

			const int visibleLines = static_cast<int>((maxY - minY - 2.0f * fontH - 24.0f) / fontH);
			visibleLineCount = visibleLines;

			if (visibleLines > 0)
			{
				AcquireSRWLockShared(&lineBufferLock);

				const int newest = (writeHead - 1 + maxLines) % maxLines;
				const int maxSkip = std::max(0, lineBufferCount - visibleLines);
				const int skip = std::clamp(scrollOffset, 0, maxSkip);

				const float barX = textX + textW - scrollBarWidth;
				DrawBox(barX, textTop, scrollBarWidth, textH, outputBarColor);

				float sliderY = textTop;
				float sliderH = textH;

				if (maxSkip > 0)
				{
					const float span = 1.0f / static_cast<float>(maxSkip);
					const float travel = static_cast<float>(maxSkip - skip) * span;
					sliderH = std::ceil(static_cast<float>(visibleLines) * span * textH);
					sliderH = std::clamp(sliderH, scrollBarWidth, textH);
					sliderY = textTop + (textH - sliderH) * travel;
				}

				DrawBox(barX, sliderY, scrollBarWidth, sliderH, outputSliderColor);

				for (int row = 0; row < visibleLines; ++row)
				{
					const int back = (visibleLines - 1 - row) + skip;

					if (back >= lineBufferCount)
					{
						continue;
					}

					const int index = (newest - back + maxLines) % maxLines;

					if (!lineBuffer[index][0])
					{
						continue;
					}

					DrawText(lineBuffer[index], textX,
						textTop + static_cast<float>(row + 1) * fontH, outputTextColor);
				}

				ReleaseSRWLockShared(&lineBufferLock);
			}
		}

		DrawInputHints(hintX, hintY, inputY);

		TextRenderer::DrawConsoleAutocomplete(std::string_view(inputBuffer, static_cast<std::size_t>(inputCursor)), ConsoleFont(), hintX, hintY);
	}

	void Console::DrawInputHints(float hintX, float hintY, float inputY)
	{
		const float fontH = fontHeight;
		const char* const namePrefix = NamePrefix();

		if (!*namePrefix || *namePrefix == ' ')
		{
			return;
		}

		char typedName[64];
		std::snprintf(typedName, sizeof(typedName), "%.*s",
			static_cast<int>(std::strcspn(namePrefix, " ")), namePrefix);

		const bool hasSpaceAfter = std::strchr(namePrefix, ' ') != nullptr;

		void* detailDvar = nullptr;

		if (hasSpaceAfter)
		{
			matchCount = 0;
			matchTotal = 0;
			detailDvar = FindDvar(typedName);
		}
		else
		{
			CollectMatches(typedName);

			if (matchTotal == 1)
			{
				detailDvar = FindDvar(matchBuffer[0]);
			}
		}

		if (!hasSpaceAfter && matchTotal == 1)
		{
			const auto typedLength = static_cast<int>(std::strlen(typedName));

			if (_strnicmp(matchBuffer[0], typedName, static_cast<std::size_t>(typedLength)) == 0
				&& matchBuffer[0][typedLength])
			{
				char ghost[80];
				std::snprintf(ghost, sizeof(ghost), "^2%s", matchBuffer[0] + typedLength);

				const float ghostX = hintX
					+ static_cast<float>(MeasureInput(inputScroll, inputLength - inputScroll));

				DrawText(ghost, ghostX, inputY + fontH, outputTextColor);
			}
		}

		if (detailDvar)
		{
			const char* const detailName = hasSpaceAfter ? typedName : matchBuffer[0];

			char value[128];
			DvarValueString(detailDvar, dvarCurrent, value, sizeof(value));
			char resetValue[128];
			DvarValueString(detailDvar, dvarReset, resetValue, sizeof(resetValue));

			const float valueX = hintX + valueColumnOffset;

			char clampedName[nameClamp + 1];
			std::snprintf(clampedName, sizeof(clampedName), "%s", detailName);
			char clampedValue[valueClamp + 1];

			DrawHintBox(hintX, hintY, 2);
			DrawHintText(clampedName, hintX, hintY, dvarNameColor);
			std::snprintf(clampedValue, sizeof(clampedValue), "%s", value);
			DrawHintText(clampedValue, valueX, hintY, dvarValueColor);

			std::snprintf(clampedValue, sizeof(clampedValue), "%s", resetValue);
			DrawHintText("  default", hintX, hintY + fontH, descriptionColor);
			DrawHintText(clampedValue, valueX, hintY + fontH, descriptionColor);
			return;
		}

		if (!hasSpaceAfter && matchTotal == 1 && FindBuiltinDescription(matchBuffer[0]))
		{
			DrawHintBox(hintX, hintY, 2);
			DrawHintText(matchBuffer[0], hintX, hintY, commandNameColor);
			DrawHintText(FindBuiltinDescription(matchBuffer[0]), hintX, hintY + fontH, descriptionColor);
			return;
		}

		if (matchTotal > maxMatches)
		{
			DrawHintBox(hintX, hintY, 1);

			char note[128];
			std::snprintf(note, sizeof(note), "%d matches, keep typing", matchTotal);
			DrawHintText(note, hintX, hintY, dvarNameColor);
			return;
		}

		if (matchCount > 0)
		{
			DrawHintBox(hintX, hintY, matchCount);

			for (int i = 0; i < matchCount; ++i)
			{
				const float rowY = hintY + static_cast<float>(i) * fontH;
				void* const dvar = FindDvar(matchBuffer[i]);

				char name[nameClamp + 1];
				std::snprintf(name, sizeof(name), "%s", matchBuffer[i]);
				DrawHintText(name, hintX, rowY, dvar ? dvarNameColor : commandNameColor);

				if (dvar)
				{
					char value[128];
					DvarValueString(dvar, dvarCurrent, value, sizeof(value));
					char clamped[valueClamp + 1];
					std::snprintf(clamped, sizeof(clamped), "%s", value);
					DrawHintText(clamped, hintX + valueColumnOffset, rowY, dvarValueColor);
				}
			}
		}
	}

	bool Console::HandleKey(int key, int down)
	{
		FollowEngineClose();

		for (const auto& observer : keyObservers)
		{
			if (observer(key, down))
			{
				return true;
			}
		}

		const bool isShiftDown = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		const bool isControlDown = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;

		if (!isOpen)
		{
			if (key == keyConsole && down)
			{
				ToggleMode(isShiftDown);
				return true;
			}

			return false;
		}

		if (!down)
		{
			return true;
		}

		if (TextRenderer::HandleFontIconAutocompleteKey(TextRenderer::FONT_ICON_ACI_CONSOLE, key, inputBuffer, inputCursor))
		{
			inputLength = static_cast<int>(std::strlen(inputBuffer));
			return true;
		}

		switch (key)
		{
		case keyConsole:
			ToggleMode(isShiftDown);
			return true;

		case keyEscape:
			SetOpen(false);
			return true;

		case keyEnter:
			ExecuteInput();
			return true;

		case keyTab:
			if (IsTypingName())
			{
				const char* const prefix = NamePrefix();
				CollectMatches(prefix);

				if (matchCount > 0)
				{
					const auto slashes = static_cast<int>(prefix - inputBuffer);
					char completed[inputCapacity];
					std::snprintf(completed, sizeof(completed), "%.*s%s ", slashes, inputBuffer, matchBuffer[0]);
					LoadInput(completed);
				}
			}
			return true;

		case keyBackspace:
			if (inputCursor > 0)
			{
				std::memmove(inputBuffer + inputCursor - 1, inputBuffer + inputCursor,
					static_cast<std::size_t>(inputLength - inputCursor) + 1);
				--inputCursor;
				--inputLength;
			}
			return true;

		case keyLeftArrow:
			if (inputCursor > 0)
			{
				--inputCursor;
			}
			return true;

		case keyRightArrow:
			if (inputCursor < inputLength)
			{
				++inputCursor;
			}
			return true;

		case keyInsert:
			isOverstrike = !isOverstrike;
			return true;

		case keyUpArrow:
			HistoryUp();
			return true;

		case keyDownArrow:
			HistoryDown();
			return true;

		case keyHome:
			if (isControlDown)
			{
				ScrollBy(lineBufferCount);
			}
			else
			{
				inputCursor = 0;
			}
			return true;

		case keyEnd:
			if (isControlDown)
			{
				ScrollBy(-scrollOffset);
			}
			else
			{
				inputCursor = inputLength;
			}
			return true;

		case keyPageUp:
			ScrollBy(8);
			return true;

		case keyPageDown:
			ScrollBy(-8);
			return true;

		case keyMouseWheelUp:
			ScrollBy(3);
			return true;

		case keyMouseWheelDown:
			ScrollBy(-3);
			return true;

		default:
			return true;
		}
	}

	void Console::HandleChar(int character)
	{
		if (character < ' ' || character > '~')
		{
			return;
		}

		if (character == '`' || character == '~')
		{
			return;
		}

		InsertInputChar(static_cast<char>(character));
	}

	void Console::CL_KeyEvent_Hook(int localClientNum, int key, int down, unsigned int time)
	{
		if (HandleKey(key, down))
		{
			return;
		}

		reinterpret_cast<Game::CL_KeyEvent_t>(keyEventHook.GetOriginal())(localClientNum, key, down, time);
	}

	void Console::CL_CharEvent_Hook(int localClientNum, int character)
	{
		FollowEngineClose();

		if (isOpen)
		{
			HandleChar(character);
			return;
		}

		reinterpret_cast<Game::CL_CharEvent_t>(charEventHook.GetOriginal())(localClientNum, character);
	}

	void* Console::CL_ConsolePrint_AddLine_Hook(int localClientNum, int channel, const char* text,
		int duration, int pixelWidth, unsigned char color, int flags)
	{
		Print(text);

		return reinterpret_cast<Game::CL_ConsolePrint_AddLine_t>(consolePrintHooks[0].GetOriginal())(
			localClientNum, channel, text, duration, pixelWidth, color, flags);
	}

	void Console::RequestShutdown(DWORD watchdogDelayMs)
	{
		if (isShutdownRequested.exchange(true))
		{
			return;
		}

		Command::Execute("quit", false);
		StartShutdownWatchdog(watchdogDelayMs);
	}

	void Console::StartShutdownWatchdog(DWORD watchdogDelayMs)
	{
		if (isWatchdogStarted.exchange(true))
		{
			return;
		}

		std::thread([watchdogDelayMs]
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(watchdogDelayMs));
			TerminateProcess(GetCurrentProcess(), EXIT_SUCCESS);
		}).detach();
	}

	BOOL WINAPI Console::ConsoleCtrlHandler(DWORD ctrlType)
	{
		switch (ctrlType)
		{
		case CTRL_C_EVENT:
		case CTRL_BREAK_EVENT:
		case CTRL_CLOSE_EVENT:
		case CTRL_LOGOFF_EVENT:
		case CTRL_SHUTDOWN_EVENT:
			RequestShutdown(3000);
			return TRUE;
		default:
			return FALSE;
		}
	}

	Console::Console()
	{
		if (isInstalled)
		{
			return;
		}

		SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
		Scheduler::OnShutdown([]
		{
			StartShutdownWatchdog(5000);
		});

		ResetInput();

		Scheduler::Loop(DrawFrame, Scheduler::Pipeline::RENDERER);

		if (Flags::HasFlag("stdout"))
		{
			if (!Utils::Hook::MatchesBytes(Sys_Error, sysErrorEntry, sizeof(sysErrorEntry))
				|| !sysErrorHook.Initialize(Sys_Error, reinterpret_cast<void*>(StdOutError), HOOK_JUMP)->Install()->IsInstalled())
			{
				Logger::Error("console: could not hook Sys_Error, -stdout prints but errors still go to the error box\n");
			}
			else
			{
				sysErrorHook.Quick();
			}
		}
		else if (Dedicated::IsEnabled() && !Flags::HasFlag("console"))
		{
			const bool isExpected = Utils::Hook::BranchesTo(Sys_GetEvent_Sys_ConsoleInputCall, Sys_ConsoleInput, HOOK_CALL)
				&& Utils::Hook::MatchesBytes(Sys_Error, sysErrorEntry, sizeof(sysErrorEntry));

			bool isSeated = false;

			if (isExpected)
			{
				isSeated = consoleInputHook.Initialize(Sys_GetEvent_Sys_ConsoleInputCall, reinterpret_cast<void*>(Input), HOOK_CALL)->Install()->IsInstalled();
				isSeated = sysErrorHook.Initialize(Sys_Error, reinterpret_cast<void*>(Error), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
			}

			if (!isSeated)
			{
				consoleInputHook.Uninstall();
				sysErrorHook.Uninstall();
				Logger::Error("console: Sys_GetEvent or Sys_Error does not read as expected, the dedicated server has no console\n");
			}
			else
			{
				consoleInputHook.Quick();
				sysErrorHook.Quick();

				Events::OnDvarInit(Create);
				Scheduler::Loop(RefreshStatus, Scheduler::Pipeline::MAIN);
			}
		}

		int failed = 0;

		failed += !keyEventHook.Initialize(CL_KeyEventCall, CL_KeyEvent_Hook, HOOK_CALL)->Install()->IsInstalled();
		failed += !charEventHook.Initialize(CL_CharEventCall, CL_CharEvent_Hook, HOOK_CALL)->Install()->IsInstalled();

		for (std::size_t i = 0; i < ARRAYSIZE(CL_ConsolePrint_AddLineCalls); ++i)
		{
			failed += !consolePrintHooks[i]
				.Initialize(CL_ConsolePrint_AddLineCalls[i], CL_ConsolePrint_AddLine_Hook, HOOK_CALL)
				->Install()->IsInstalled();
		}

		if (failed)
		{
			return;
		}

		keyEventHook.Quick();
		charEventHook.Quick();

		for (auto& hook : consolePrintHooks)
		{
			hook.Quick();
		}

		Command::Add("clear", []
		{
			ClearScrollback();
		});

		Command::Add("echo", [](const Command::Params* params)
		{
			Print(params->Join(1).data());
		});

		Command::Add("quit", []
		{
			reinterpret_cast<void(*)()>(Utils::Hook::Rebase(0x1401F5BB0))();
		});

		isInstalled = true;

		char banner[128];
		std::snprintf(banner, sizeof(banner), "%s console, press ~ to open and shift+~ for the log", versionText);
		Print(banner);
		Print("tab completes, up and down recall, pgup and pgdn scroll, quit exits");
	}
}
