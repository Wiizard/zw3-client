#include "STDInclude.hpp"

#include "Localization.hpp"
#include "ArenaLength.hpp"
#include "AssetHandler.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"

#include "GSC/Script.hpp"

namespace Components
{
	std::unordered_map<std::string, std::string> Localization::strings;
	std::mutex Localization::stringsMutex;
	Dvar::Var Localization::ui_localize;

	constexpr std::uintptr_t SEH_StringEd_GetString = 0x14024F2D0;

	constexpr std::uintptr_t SEH_SafeTranslateString_LookupCall = 0x14024F1FE;
	constexpr std::uintptr_t DB_FindXAssetHeader = 0x14012D6D0;

	constexpr std::uintptr_t SEH_LocalizeTextMessage = 0x14024EB90;

	constexpr std::uintptr_t loc_translate = 0x146521128;

	constexpr unsigned int assetTypeLocalizeEntry = 0x1B;

	constexpr std::uintptr_t db_hashCritSect = 0x14151D630;

	static const char* FindLoadedLocalizeValue(const char* key)
	{
		auto* const readCount = reinterpret_cast<volatile long*>(Utils::Hook::Rebase(db_hashCritSect));
		const auto* const writeCount = readCount + 1;

		InterlockedIncrement(readCount);

		while (*writeCount)
		{
			Sleep(1);
		}

		const auto* const entry = Game::DB_FindXAssetEntry(assetTypeLocalizeEntry, key);
		const char* value = nullptr;

		if (entry && entry->asset.header.data)
		{
			value = *static_cast<const char* const*>(entry->asset.header.data);
		}

		InterlockedDecrement(readCount);

		return value;
	}

	constexpr std::uintptr_t SEH_UpdateLanguageInfo = 0x14024F320;
	constexpr std::uintptr_t SEH_UpdateLanguageInfoCalls[] = { 0x140276E47, 0x1402780B4, 0x1401F3C73 };

	constexpr std::uintptr_t loc_forceEnglish = 0x146521120;

	static Utils::Hook lookupHook;
	static Utils::Hook safeTranslateHook;
	static Utils::Hook localizeTextMessageHook;
	static Utils::Hook updateLanguageHooks[std::size(SEH_UpdateLanguageInfoCalls)];

	static std::string ReadStringLine(std::string_view& text)
	{
		std::string line;
		const auto end = text.find('\n');

		if (end == std::string_view::npos)
		{
			line = text;
			text = {};
		}
		else
		{
			line = text.substr(0, end);
			text.remove_prefix(end);

			while (!text.empty() && (text.front() == '\r' || text.front() == '\n'))
			{
				text.remove_prefix(1);
			}
		}

		while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
		{
			line.pop_back();
		}

		std::size_t searchFrom = 0;
		std::size_t quotes = 0;

		while (true)
		{
			const auto comment = line.find("//", searchFrom);

			if (comment == std::string::npos)
			{
				break;
			}

			for (auto i = searchFrom; i < comment; ++i)
			{
				if (line[i] == '\"')
				{
					++quotes;
				}
			}

			if (quotes % 2 == 0)
			{
				line.resize(comment);

				while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
				{
					line.pop_back();
				}

				break;
			}

			searchFrom = comment + 1;
		}

		return line;
	}

	static bool TakeKeyword(std::string_view& line, std::string_view keyword)
	{
		if (line.size() < keyword.size() || _strnicmp(line.data(), keyword.data(), keyword.size()) != 0)
		{
			return false;
		}

		line.remove_prefix(keyword.size());

		while (!line.empty() && (line.front() == ' ' || line.front() == '\t'))
		{
			line.remove_prefix(1);
		}

		return true;
	}

	static std::string Unquote(std::string_view value)
	{
		while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
		{
			value.remove_prefix(1);
		}

		if (!value.empty() && value.front() == '\"')
		{
			value.remove_prefix(1);
		}

		std::string result(value);

		while (!result.empty() && (result.back() == ' ' || result.back() == '\t'))
		{
			result.pop_back();
		}

		if (!result.empty() && result.back() == '\"')
		{
			result.pop_back();
		}

		return result;
	}

	static bool IsStringFormatValid(const std::string& value)
	{
		bool isUsed[9] = {};
		auto at = value.find("&&");

		while (at != std::string::npos)
		{
			char digit = '\0';

			if (at + 2 < value.size())
			{
				digit = value[at + 2];
			}

			if (digit < '1' || digit > '9' || isUsed[digit - '1'])
			{
				return false;
			}

			isUsed[digit - '1'] = true;
			at = value.find("&&", at + 3);
		}

		return true;
	}

	static std::optional<std::string> LoadStringFile(const std::string& path, bool isEnglishForced)
	{
		FileSystem::File file(path);

		if (!file.Exists())
		{
			return std::format("Unable to load \"{}\"!", path);
		}

		std::string_view text = file.GetBuffer();
		std::string reference;
		bool hasEnd = false;

		while (!text.empty())
		{
			const auto line = ReadStringLine(text);

			if (line.empty())
			{
				continue;
			}

			std::string_view rest = line;
			std::string error;

			if (TakeKeyword(rest, "VERSION"))
			{
				const auto version = std::atol(Unquote(rest).data());

				if (version != 1)
				{
					error = std::format("Unexpected version number {}, expecting {}!", version, 1);
				}
			}
			else if (TakeKeyword(rest, "CONFIG") || TakeKeyword(rest, "FILENOTES") || TakeKeyword(rest, "NOTES") || TakeKeyword(rest, "FLAGS"))
			{
				continue;
			}
			else if (TakeKeyword(rest, "REFERENCE"))
			{
				reference = Unquote(rest);
			}
			else if (TakeKeyword(rest, "ENDMARKER"))
			{
				hasEnd = true;
			}
			else if (_strnicmp(line.data(), "LANG_", 5) != 0)
			{
				error = std::format("Unknown keyword at linestart: \"{}\"", line);
			}
			else if (reference.empty())
			{
				error = "Error parsing file: Unexpected \"LANG_\"";
			}
			else
			{
				std::string_view language = std::string_view(line).substr(5);
				const auto languageEnd = language.find_first_of(" \t");
				auto value = Unquote(language.substr(std::min(languageEnd, language.size())));
				language = language.substr(0, std::min(languageEnd, std::size_t{ 1023 }));
				Utils::String::Replace(value, "\\n", "\n");

				if (!IsStringFormatValid(value))
				{
					error = std::format("Illegal string format \"{}\"", value);
				}
				else
				{
					const bool isEnglish = _strnicmp(language.data(), "english", 7) == 0 && language.size() == 7;

					if (isEnglish || !isEnglishForced)
					{
						Localization::Set(reference, value);
					}
				}
			}

			if (!error.empty())
			{
				return std::format("{} in {}", error, path);
			}
		}

		if (!hasEnd)
		{
			return "Truncated file, failed to find \"ENDMARKER\" at file end!";
		}

		return std::nullopt;
	}

	static void ListStringFiles(const std::string& path, std::vector<std::string>& files)
	{
		for (const auto& folder : FileSystem::GetFileList(path, "/"))
		{
			if (!folder.empty() && folder.front() != '.')
			{
				ListStringFiles(path + "/" + folder, files);
			}
		}

		for (const auto& name : FileSystem::GetFileList(path, "str"))
		{
			files.push_back(path + "/" + name);
		}
	}

	static void SEH_UpdateLanguageInfo_Hk()
	{
		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(SEH_UpdateLanguageInfo))();

		const auto* forceEnglish = Utils::Hook::Get<Game::dvar_t*>(loc_forceEnglish);
		const bool isEnglishForced = forceEnglish && forceEnglish->current.enabled;

		std::vector<std::string> files;
		ListStringFiles("localizedstrings", files);

		for (const auto& path : files)
		{
			if (const auto error = LoadStringFile(path, isEnglishForced))
			{
				Logger::Warning("localization: could not load localization strings: {}\n", *error);
				return;
			}
		}
	}

	void Localization::Set(const std::string& reference, const std::string& value)
	{
		std::lock_guard _(stringsMutex);

		strings[reference] = value;
	}

	const char* Localization::Get(const char* key)
	{
		if (!key)
		{
			return key;
		}

		if (ui_localize.IsValid() && !ui_localize.Get<bool>())
		{
			return key;
		}

		{
			std::lock_guard _(stringsMutex);

			const auto entry = strings.find(key);

			if (entry != strings.end())
			{
				return entry->second.data();
			}
		}

		if (!FastFiles::Ready())
		{
			const auto* const value = FindLoadedLocalizeValue(key);

			if (value)
			{
				return value;
			}

			return key;
		}

		const auto* const asset = static_cast<const char* const*>(Game::DB_FindXAssetHeader(assetTypeLocalizeEntry, key));

		if (asset && *asset)
		{
			return *asset;
		}

		return key;
	}

	bool Localization::IsTranslating()
	{
		const auto* const dvar = Utils::Hook::Get<const Game::dvar_t*>(loc_translate);

		return dvar && dvar->current.enabled;
	}

	const char* Localization::SEH_StringEd_GetString_Stub(const char* reference)
	{
		if (!reference || !IsTranslating() || !reference[0] || !reference[1])
		{
			return reference;
		}

		return Get(reference);
	}

	void* Localization::SEH_SafeTranslateString_Lookup([[maybe_unused]] unsigned int type, const char* reference)
	{
		thread_local const char* value = nullptr;
		value = Get(reference);

		return &value;
	}

	void Localization::SEH_GetLocalizedTokenReference(char* token, std::size_t tokenSize)
	{
		if (!IsTranslating() || !token[0] || !token[1])
		{
			return;
		}

		const char* const translation = Get(token);

		if (translation != token)
		{
			Game::I_strcpy(token, tokenSize, translation);
		}
	}

	void Localization::SetCredits()
	{
		static const char* staff[] =
		{
			"Snake",
			"/dev/../",
			"/dev/console",
			"/dev/full",
			"/dev/sdb",
			"/dev/sr0",
			"/dev/tty0",
			"/dev/urandom",
			"Dss0",
			"Evan/Eve",
			"FutureRave",
			"H3X1C",
			"Homura",
			"Laupetin",
			"Louvenarde",
			"lsb_release -a",
			"quaK",
		};

		static const char* contributors[] =
		{
			"a231",
			"AmateurHailbut",
			"Aoki",
			"Chase",
			"civil",
			"Dasfonia",
			"Deity",
			"Dizzy",
			"HardNougat",
			"INeedGames",
			"JTAG",
			"Killera",
			"Lithium",
			"OneFourOne",
			"RaidMax",
			"Revo",
			"RezTech",
			"Shadow the Hedgehog",
			"Slykuiper",
			"st0rm",
			"VVLNT",
			"X3RX35",
		};

		static const char* specials[] =
		{
			"NTAuthority",
			"aerosoul94",
			"ReactIW4",
			"IW4Play",
			"V2",
			"luckyy"
		};

		std::string credits = "^2The IW4x Team:^7\n";

		for (const char* const name : staff)
		{
			credits.append(name);
			credits.append("\n");
		}

		credits.append("\n^3Contributors:^7\n");

		for (const char* const name : contributors)
		{
			credits.append(name);
			credits.append("\n");
		}

		credits.append("\n^5Special thanks to:^7\n");

		for (const char* const name : specials)
		{
			credits.append(name);
			credits.append("\n");
		}

		credits.append("-\n-");

		Set("IW4X_CREDITS", credits);
	}

	const char* Localization::SEH_LocalizeTextMessage_Stub(const char* inputBuffer, const char* messageType, [[maybe_unused]] int errType)
	{
		constexpr int stringCount = 10;
		constexpr int stringSize = 1024;

		char insertBuf[stringSize]{};
		char tokenBuf[stringSize];

		static thread_local int currentString;
		static thread_local char resultStrings[stringCount][stringSize];

		currentString = (currentString + 1) % stringCount;
		std::memset(resultStrings[currentString], 0, sizeof(resultStrings[0]));

		char* const result = resultStrings[currentString];
		int length = 0;
		bool isLocalizing = true;
		bool isInsertEnabled = true;
		int insertLevel = 0;
		int insertIndex = 1;
		bool didSkipInsert = false;
		const char* tokenStart = inputBuffer;
		const char* position = inputBuffer;

		int i = 0;

		while (*tokenStart)
		{
			if (*position && *position != '\x14' && *position != '\x15' && *position != '\x16')
			{
				++position;
				continue;
			}

			if (position > tokenStart)
			{
				int tokenLength = static_cast<int>(position - tokenStart);
				Game::I_strncpyz_s(tokenBuf, sizeof(tokenBuf), tokenStart, position - tokenStart);

				if (isLocalizing)
				{
					SEH_GetLocalizedTokenReference(tokenBuf, sizeof(tokenBuf));
					tokenLength = static_cast<int>(std::strlen(tokenBuf));
				}

				if (tokenLength + length >= stringSize)
				{
					Logger::Print("{} too long when translated\n", messageType);
					return nullptr;
				}

				for (i = 0; i < tokenLength - 2; ++i)
				{
					if (!std::strncmp(&tokenBuf[i], "&&", 2) && std::isdigit(static_cast<unsigned char>(tokenBuf[i + 2])))
					{
						if (isInsertEnabled)
						{
							++insertLevel;
						}
						else
						{
							tokenBuf[i] = '\x16';
							didSkipInsert = true;
						}
					}
				}

				if (insertLevel <= 0 || length <= 0)
				{
					Game::I_strcpy(&result[length], stringSize - length, tokenBuf);
				}
				else
				{
					for (i = 0; i < length - 2; ++i)
					{
						if (!std::strncmp(&result[i], "&&", 2) && std::isdigit(static_cast<unsigned char>(result[i + 2])))
						{
							const int digit = result[i + 2] - '0';

							if (!digit)
							{
								Logger::Print("{} cannot have &&0 as conversion format: \"{}\"\n", messageType, inputBuffer);
							}

							if (digit == insertIndex)
							{
								Game::I_strcpy(insertBuf, sizeof(insertBuf), &result[i + 3]);
								result[i] = 0;
								++insertIndex;
								break;
							}
						}
					}

					Game::I_strcpy(&result[i], stringSize - i, tokenBuf);
					Game::I_strcpy(&result[tokenLength + i], stringSize - (tokenLength + i), insertBuf);

					length -= 3;
					--insertLevel;
				}

				length += tokenLength;
			}

			isInsertEnabled = true;

			if (*position == '\x14')
			{
				isLocalizing = true;
				++position;
			}
			else if (*position == '\x15')
			{
				isLocalizing = false;
				++position;
			}

			if (*position == '\x16')
			{
				isInsertEnabled = false;
				++position;
			}

			tokenStart = position;
		}

		if (didSkipInsert)
		{
			for (i = 0; i < length; ++i)
			{
				if (result[i] == '\x16')
				{
					result[i] = '%';
				}
			}
		}

		return result;
	}

	const char* Localization::LocalizeMapName(const char* mapName)
	{
		if (!ArenaLength::newArenas)
		{
			return mapName;
		}

		for (int i = 0; i < *Game::arenaCount; ++i)
		{
			if (!_stricmp(ArenaLength::newArenas[i].mapName, mapName))
			{
				const char* const uiName = ArenaLength::newArenas[i].uiName;
				const bool isReference = (uiName[0] == 'M' && uiName[1] == 'P') || (uiName[0] == 'P' && uiName[1] == 'A');

				if (isReference)
				{
					return Get(uiName);
				}

				return uiName;
			}
		}

		return mapName;
	}

	const char* Localization::GetMapImageName(const char* mapName)
	{
		if (!mapName || !*mapName || !ArenaLength::newArenas)
		{
			return "";
		}

		for (int i = 0; i < *Game::arenaCount; ++i)
		{
			const Game::newMapArena_t& arena = ArenaLength::newArenas[i];

			if (arena.mapimage[0] && !_stricmp(arena.mapName, mapName))
			{
				return arena.mapimage;
			}
		}

		for (int i = 0; i < *Game::arenaCount; ++i)
		{
			const Game::newMapArena_t& arena = ArenaLength::newArenas[i];

			if (!arena.mapimage[0])
			{
				continue;
			}

			if (!_stricmp(arena.mapimage, mapName))
			{
				return arena.mapimage;
			}

			if (arena.uiName[0] && !_stricmp(arena.uiName, mapName))
			{
				return arena.mapimage;
			}

			const char* const localizedName = LocalizeMapName(arena.mapName);

			if (localizedName[0] && !_stricmp(localizedName, mapName))
			{
				return arena.mapimage;
			}
		}

		return "";
	}

	void Localization::GSCr_LocalizeText()
	{
		if (Game::Scr_GetNumParam() != 1)
		{
			GSC::Script::Scr_Error("GSCr_LocalizeText: missing key!");
			return;
		}

		const char* const text = Game::Scr_GetString(0);

		Game::Scr_AddString(Game::UI_SafeTranslateString(text));
	}

	void Localization::GSCr_LocalizeGametype()
	{
		if (Game::Scr_GetNumParam() != 1)
		{
			GSC::Script::Scr_Error("GSCr_LocalizeGametype: missing gametype!");
			return;
		}

		const char* const gametype = Game::Scr_GetString(0);

		Game::Scr_AddString(Game::UI_GetGameTypeDisplayName(gametype));
	}

	Localization::Localization()
	{
		SetCredits();

		Events::OnDvarInit([]
		{
			ui_localize = Dvar::Register("ui_localize", true, Game::DVAR_NONE, "Use localization strings");
		});

		AssetHandler::OnLoad(assetTypeLocalizeEntry, []([[maybe_unused]] unsigned int type, void* asset, const std::string& name, [[maybe_unused]] bool* restrict)
		{
			if (name != "CLASS_SLOT1")
			{
				return;
			}

			for (int i = 11; i <= NUM_CUSTOM_CLASSES; ++i)
			{
				std::string value = *static_cast<const char* const*>(asset);
				Utils::String::Replace(value, "1", std::to_string(i));

				Set(Utils::String::VA("CLASS_SLOT%i", i), value);
			}
		});

		GSC::Script::AddFunction("LocalizeText", GSCr_LocalizeText);
		GSC::Script::AddFunction("LocalizeGametype", GSCr_LocalizeGametype);

		bool areLanguageCallsIntact = true;

		for (const auto call : SEH_UpdateLanguageInfoCalls)
		{
			areLanguageCallsIntact = areLanguageCallsIntact && Utils::Hook::BranchesTo(call, SEH_UpdateLanguageInfo, HOOK_CALL);
		}

		if (!areLanguageCallsIntact)
		{
			Logger::Error("localization: SEH_UpdateLanguageInfo is not called where expected, a mod's .str strings will not load\n");
		}
		else
		{
			bool areLanguageHooksSeated = true;

			for (std::size_t i = 0; i < std::size(SEH_UpdateLanguageInfoCalls); ++i)
			{
				areLanguageHooksSeated = updateLanguageHooks[i].Initialize(SEH_UpdateLanguageInfoCalls[i], reinterpret_cast<void*>(SEH_UpdateLanguageInfo_Hk), HOOK_CALL)
					->Install()->IsInstalled() && areLanguageHooksSeated;
			}

			for (auto& hook : updateLanguageHooks)
			{
				if (areLanguageHooksSeated)
				{
					hook.Quick();
				}
				else
				{
					hook.Uninstall();
				}
			}

			if (!areLanguageHooksSeated)
			{
				Logger::Error("localization: could not seat the SEH_UpdateLanguageInfo hooks, a mod's .str strings will not load\n");
			}
		}

		if (!Utils::Hook::BranchesTo(SEH_SafeTranslateString_LookupCall, DB_FindXAssetHeader, HOOK_CALL))
		{
			Logger::Error("localization: 0x{:X} no longer calls DB_FindXAssetHeader, IW4x's strings will not resolve\n", SEH_SafeTranslateString_LookupCall);
			return;
		}

		bool isSeated = lookupHook.Initialize(SEH_StringEd_GetString, SEH_StringEd_GetString_Stub, HOOK_JUMP)->Install()->IsInstalled();
		isSeated = safeTranslateHook.Initialize(SEH_SafeTranslateString_LookupCall, SEH_SafeTranslateString_Lookup, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = localizeTextMessageHook.Initialize(SEH_LocalizeTextMessage, SEH_LocalizeTextMessage_Stub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			lookupHook.Uninstall();
			safeTranslateHook.Uninstall();
			localizeTextMessageHook.Uninstall();

			Logger::Error("localization: could not seat every lookup hook, IW4x's strings will not resolve\n");
			return;
		}

		lookupHook.Quick();
		safeTranslateHook.Quick();
		localizeTextMessageHook.Quick();
	}
}
