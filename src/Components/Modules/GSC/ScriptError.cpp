#include "STDInclude.hpp"

#include "ScriptError.hpp"
#include "../Logger.hpp"
#include "../RawFiles.hpp"

namespace Components::GSC
{
	constexpr std::uintptr_t CompileError = 0x14021EC60;
	constexpr std::uintptr_t Scr_CompileErrorPos = 0x14021EB00;
	constexpr std::uintptr_t compileErrorFormat = 0x14039A208;
	constexpr std::uintptr_t compileErrorIsFatal = 0x141F2990B;
	constexpr std::uintptr_t Sys_Error = 0x1402A4F90;
	constexpr std::uintptr_t Scr_ShutdownAllocNode = 0x14021ED30;
	constexpr std::uintptr_t Scr_CompileErrorPos_Lookup = 0x14021DBF0;
	constexpr std::uintptr_t Scr_CompileErrorPos_NoPos = 0x14227DA08;

	static const std::uint8_t compileErrorEntry[] = { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x4C, 0x89, 0x40, 0x18, 0x4C, 0x89, 0x48, 0x20, 0x53, 0x57 };
	static const std::uint8_t compileErrorPosEntry[] = { 0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x4C, 0x89, 0x40, 0x18, 0x4C, 0x89, 0x48, 0x20, 0x53, 0x56, 0x57 };

	constexpr std::uintptr_t ScriptParse = 0x14022FA10;
	constexpr std::uintptr_t ScriptCompile = 0x14021CEF0;

	static const std::uintptr_t parseCalls[] = { 0x14021DD34, 0x14021DF04 };
	static const std::uintptr_t compileCalls[] = { 0x14021DD9C, 0x14021DF60 };

	constexpr std::uintptr_t Scr_GetEntryNameId = 0x1402249A0;
	constexpr std::uintptr_t Linker_GetEntryNameIdCall = 0x14021AE81;

	constexpr std::size_t fileNameSize = 64;
	constexpr std::size_t maxFileDepth = 64;

	static char fileStack[maxFileDepth][fileNameSize];
	static std::size_t fileDepth = 0;
	static bool isParsing = false;
	static unsigned int lastEntryName = 0;

	static Utils::Hook parseHooks[std::size(parseCalls)];
	static Utils::Hook compileHooks[std::size(compileCalls)];
	static Utils::Hook entryNameHook;
	static Utils::Hook compileErrorHook;
	static Utils::Hook compileErrorPosHook;

	static void ScriptParse_Hk(void** parseData, unsigned char user)
	{
		isParsing = true;
		reinterpret_cast<void(*)(void**, unsigned char)>(Utils::Hook::Rebase(ScriptParse))(parseData, user);
		isParsing = false;
	}

	static void ScriptCompile_Hk(void* root, unsigned int filePosId, unsigned int fileCountId, unsigned int scriptId, void* entries, int entriesCount)
	{
		if (fileDepth < maxFileDepth)
		{
			strncpy_s(fileStack[fileDepth], RawFiles::LastScriptRead(), _TRUNCATE);
		}

		++fileDepth;
		lastEntryName = 0;

		reinterpret_cast<void(*)(void*, unsigned int, unsigned int, unsigned int, void*, int)>(Utils::Hook::Rebase(ScriptCompile))(root, filePosId, fileCountId, scriptId, entries, entriesCount);

		--fileDepth;
		lastEntryName = 0;
	}

	static unsigned int Scr_GetEntryNameId_Hk(unsigned int id, unsigned int entry)
	{
		lastEntryName = reinterpret_cast<unsigned int(*)(unsigned int, unsigned int)>(Utils::Hook::Rebase(Scr_GetEntryNameId))(id, entry);
		return lastEntryName;
	}

	static const char* CurrentFile()
	{
		if (isParsing || fileDepth == 0)
		{
			return RawFiles::LastScriptRead();
		}

		const std::size_t index = std::min(fileDepth, maxFileDepth) - 1;
		return fileStack[index];
	}

	static bool TryReadSource(const char* file, std::string* source)
	{
		void* buffer = nullptr;
		const int length = Game::FS_ReadFile(file, &buffer);

		if (length >= 0 && buffer)
		{
			source->assign(static_cast<const char*>(buffer), static_cast<std::size_t>(length));
			Game::FS_FreeFile(buffer);
			return true;
		}

		const Game::XAssetEntry* const entry = Game::DB_FindXAssetEntry(Game::ASSET_TYPE_RAWFILE, file);

		if (!entry || !entry->asset.header.data)
		{
			return false;
		}

		const int rawLength = Game::DB_GetRawFileLen(entry->asset.header.data);

		if (rawLength <= 0)
		{
			return false;
		}

		source->resize(static_cast<std::size_t>(rawLength));
		Game::DB_GetRawBuffer(entry->asset.header.data, source->data(), rawLength);
		return true;
	}

	static int DescribeLine(const std::string& source, std::size_t position, std::string* text)
	{
		position = std::min(position, source.size());

		const int line = 1 + static_cast<int>(std::count(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(position), '\n'));

		std::size_t start = 0;

		if (position > 0)
		{
			const std::size_t lineBreak = source.rfind('\n', position - 1);

			if (lineBreak != std::string::npos)
			{
				start = lineBreak + 1;
			}
		}

		std::size_t end = source.find('\n', position);

		if (end == std::string::npos)
		{
			end = source.size();
		}

		std::string found = source.substr(start, end - start);
		const auto first = found.find_first_not_of(" \t\r");
		const auto last = found.find_last_not_of(" \t\r");

		if (first == std::string::npos)
		{
			found.clear();
		}
		else
		{
			found = found.substr(first, last - first + 1);
		}

		std::size_t column = position - start;

		if (first != std::string::npos)
		{
			column -= std::min(column, first);
		}

		if (found.size() > 160 && column > 60)
		{
			const std::size_t from = std::min(column - 60, found.size());
			found = "..." + found.substr(from);
		}

		if (found.size() > 160)
		{
			found = found.substr(0, 160) + "...";
		}

		*text = found;
		return line;
	}

	static bool IsNameChar(const char character)
	{
		return std::isalnum(static_cast<unsigned char>(character)) || character == '_';
	}

	static std::size_t FindFirstCall(const std::string& source, const std::string& name)
	{
		const std::string lowerSource = Utils::String::ToLower(source);
		const std::string lowerName = Utils::String::ToLower(name);
		std::size_t at = lowerSource.find(lowerName);

		while (at != std::string::npos)
		{
			const bool isStart = at == 0 || !IsNameChar(lowerSource[at - 1]);
			std::size_t after = at + lowerName.size();

			while (after < lowerSource.size() && (lowerSource[after] == ' ' || lowerSource[after] == '\t'))
			{
				++after;
			}

			if (isStart && after < lowerSource.size() && lowerSource[after] == '(')
			{
				return at;
			}

			at = lowerSource.find(lowerName, at + 1);
		}

		return std::string::npos;
	}

	static std::string NameAt(const std::string& source, std::size_t position)
	{
		std::size_t at = position;

		while (at < source.size() && !IsNameChar(source[at]))
		{
			++at;
		}

		std::size_t end = at;

		while (end < source.size() && (IsNameChar(source[end]) || source[end] == '\\' || source[end] == ':'))
		{
			++end;
		}

		return source.substr(at, end - at);
	}

	static std::string FileOrUnknown()
	{
		const std::string file = CurrentFile();

		if (file.empty())
		{
			return "an unknown file";
		}

		return file;
	}

	static void DescribeSourcePos(unsigned int sourcePos, const char* message, char* where, std::size_t size)
	{
		const std::string file = FileOrUnknown();
		std::string source;

		if (!TryReadSource(file.data(), &source))
		{
			sprintf_s(where, size, "in %s", file.data());
			return;
		}

		std::string text;
		const int line = DescribeLine(source, sourcePos, &text);

		if (std::strcmp(message, "unknown function") == 0)
		{
			const std::string name = NameAt(source, sourcePos);
			sprintf_s(where, size, "'%s' in %s, line %d: %s", name.data(), file.data(), line, text.data());
			return;
		}

		sprintf_s(where, size, "in %s, line %d: %s", file.data(), line, text.data());
	}

	static void DescribeUnknownFunction(char* where, std::size_t size)
	{
		const std::string file = FileOrUnknown();
		const char* name = nullptr;

		if (lastEntryName)
		{
			name = Game::SL_ConvertToString(lastEntryName);
		}

		if (!name || !*name)
		{
			sprintf_s(where, size, "in %s", file.data());
			return;
		}

		std::string source;

		if (TryReadSource(file.data(), &source))
		{
			const std::size_t call = FindFirstCall(source, name);

			if (call != std::string::npos)
			{
				std::string text;
				const int line = DescribeLine(source, call, &text);
				sprintf_s(where, size, "'%s' in %s, line %d: %s", name, file.data(), line, text.data());
				return;
			}
		}

		sprintf_s(where, size, "'%s' in %s", name, file.data());
	}

	static void ResetCompileState()
	{
		fileDepth = 0;
		isParsing = false;
		lastEntryName = 0;
	}

	static void CompileError_Hk(unsigned int sourcePos, const char* format, ...)
	{
		char message[1024];

		va_list arguments;
		va_start(arguments, format);
		vsnprintf_s(message, _TRUNCATE, format, arguments);
		va_end(arguments);

		if (Utils::Hook::Get<std::uint8_t>(compileErrorIsFatal))
		{
			reinterpret_cast<void(*)(const char*)>(Utils::Hook::Rebase(Sys_Error))(message);
			return;
		}

		char where[1024];
		DescribeSourcePos(sourcePos, message, where, sizeof(where));
		ResetCompileState();

		Logger::Print("script compile error: {}\n{}\n", message, where);

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Scr_ShutdownAllocNode))();
		Game::Com_Error(5, reinterpret_cast<const char*>(Utils::Hook::Rebase(compileErrorFormat)), message, where);
	}

	static void Scr_CompileErrorPos_Hk(const char* codePos, const char* format, ...)
	{
		char message[1024];

		va_list arguments;
		va_start(arguments, format);
		vsnprintf_s(message, _TRUNCATE, format, arguments);
		va_end(arguments);

		const auto* const noPos = reinterpret_cast<const char*>(Utils::Hook::Rebase(Scr_CompileErrorPos_NoPos));

		if (codePos && codePos != noPos)
		{
			const int position = static_cast<int>(reinterpret_cast<std::uintptr_t>(codePos));
			reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(Scr_CompileErrorPos_Lookup))(position - 1);
		}

		char where[1024];

		if (std::strcmp(message, "unknown function") == 0)
		{
			DescribeUnknownFunction(where, sizeof(where));
		}
		else
		{
			sprintf_s(where, sizeof(where), "in %s", FileOrUnknown().data());
		}

		ResetCompileState();

		Logger::Print("script compile error: {}\n{}\n", message, where);

		Game::Com_Error(5, reinterpret_cast<const char*>(Utils::Hook::Rebase(compileErrorFormat)), message, where);
	}

	ScriptError::ScriptError()
	{
		bool isExpected = Utils::Hook::MatchesBytes(CompileError, compileErrorEntry, sizeof(compileErrorEntry))
			&& Utils::Hook::MatchesBytes(Scr_CompileErrorPos, compileErrorPosEntry, sizeof(compileErrorPosEntry))
			&& Utils::Hook::BranchesTo(Linker_GetEntryNameIdCall, Scr_GetEntryNameId, HOOK_CALL);

		for (const std::uintptr_t site : parseCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(site, ScriptParse, HOOK_CALL);
		}

		for (const std::uintptr_t site : compileCalls)
		{
			isExpected = isExpected && Utils::Hook::BranchesTo(site, ScriptCompile, HOOK_CALL);
		}

		if (!isExpected)
		{
			Logger::Error("scripterror: the compiler does not read as expected, compile errors stay without what and where\n");
			return;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(parseCalls); ++i)
		{
			isSeated = parseHooks[i].Initialize(parseCalls[i], reinterpret_cast<void*>(ScriptParse_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		for (std::size_t i = 0; i < std::size(compileCalls); ++i)
		{
			isSeated = compileHooks[i].Initialize(compileCalls[i], reinterpret_cast<void*>(ScriptCompile_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = entryNameHook.Initialize(Linker_GetEntryNameIdCall, reinterpret_cast<void*>(Scr_GetEntryNameId_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = compileErrorHook.Initialize(CompileError, reinterpret_cast<void*>(CompileError_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = compileErrorPosHook.Initialize(Scr_CompileErrorPos, reinterpret_cast<void*>(Scr_CompileErrorPos_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		Utils::Hook* const hooks[] =
		{
			&parseHooks[0], &parseHooks[1], &compileHooks[0], &compileHooks[1],
			&entryNameHook, &compileErrorHook, &compileErrorPosHook,
		};

		if (!isSeated)
		{
			for (Utils::Hook* const hook : hooks)
			{
				hook->Uninstall();
			}

			Logger::Error("scripterror: could not seat every hook, compile errors stay without what and where\n");
			return;
		}

		for (Utils::Hook* const hook : hooks)
		{
			hook->Quick();
		}
	}
}
