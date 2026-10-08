#include "STDInclude.hpp"

#include "MenuDvarList.hpp"
#include "Menus.hpp"
#include "Logger.hpp"

namespace Components
{
	static constexpr int maxListEntries = 32;

	struct ListScan
	{
		const char* at;
		const char* end;
	};

	struct ListToken
	{
		char text[512];
		bool isQuoted;
	};

	static bool IsWordChar(char character)
	{
		const bool isLetter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
		const bool isDigit = character >= '0' && character <= '9';

		return isLetter || isDigit || character == '_' || character == '.' || character == '-' || character == '+';
	}

	static void SkipLine(ListScan& scan)
	{
		while (scan.at < scan.end && *scan.at != '\n')
		{
			++scan.at;
		}
	}

	static void SkipSpace(ListScan& scan)
	{
		while (scan.at < scan.end)
		{
			const char current = *scan.at;
			const bool hasNext = scan.at + 1 < scan.end;

			if (current == '/' && hasNext && scan.at[1] == '/')
			{
				SkipLine(scan);
				continue;
			}

			if (current == '/' && hasNext && scan.at[1] == '*')
			{
				scan.at += 2;

				while (scan.at + 1 < scan.end && !(scan.at[0] == '*' && scan.at[1] == '/'))
				{
					++scan.at;
				}

				if (scan.at + 1 < scan.end)
				{
					scan.at += 2;
				}
				else
				{
					scan.at = scan.end;
				}

				continue;
			}

			if (current == '#')
			{
				SkipLine(scan);
				continue;
			}

			if (static_cast<unsigned char>(current) > ' ')
			{
				return;
			}

			++scan.at;
		}
	}

	static bool NextToken(ListScan& scan, ListToken& token)
	{
		SkipSpace(scan);

		token.text[0] = '\0';
		token.isQuoted = false;

		if (scan.at >= scan.end)
		{
			return false;
		}

		const int capacity = static_cast<int>(sizeof(token.text));
		int length = 0;

		if (*scan.at == '"')
		{
			token.isQuoted = true;
			++scan.at;

			while (scan.at < scan.end && *scan.at != '"')
			{
				if (length < capacity - 1)
				{
					token.text[length] = *scan.at;
					++length;
				}

				++scan.at;
			}

			if (scan.at < scan.end)
			{
				++scan.at;
			}

			token.text[length] = '\0';
			return true;
		}

		if (!IsWordChar(*scan.at))
		{
			token.text[0] = *scan.at;
			token.text[1] = '\0';
			++scan.at;
			return true;
		}

		while (scan.at < scan.end && IsWordChar(*scan.at))
		{
			if (length < capacity - 1)
			{
				token.text[length] = *scan.at;
				++length;
			}

			++scan.at;
		}

		token.text[length] = '\0';
		return true;
	}

	static bool IsPunctuation(const ListToken& token, char character)
	{
		return !token.isQuoted && token.text[0] == character && token.text[1] == '\0';
	}

	static bool IsSeparator(const ListToken& token)
	{
		return IsPunctuation(token, ';') || IsPunctuation(token, ',');
	}

	static bool TryOpenList(ListScan& scan)
	{
		SkipSpace(scan);

		if (scan.at >= scan.end || *scan.at != '{')
		{
			return false;
		}

		++scan.at;
		return true;
	}

	static void ReadStringList(ListScan& scan, Game::multiDef_s* multi)
	{
		if (!TryOpenList(scan))
		{
			return;
		}

		multi->count = 0;
		multi->strDef = 1;

		bool isValueNext = false;
		bool didOverflow = false;
		ListToken token = {};

		while (NextToken(scan, token))
		{
			if (IsPunctuation(token, '}'))
			{
				break;
			}

			if (IsSeparator(token))
			{
				continue;
			}

			if (multi->count >= maxListEntries)
			{
				didOverflow = true;
				continue;
			}

			if (isValueNext)
			{
				multi->dvarStr[multi->count] = Menus::GetAllocator()->DuplicateString(token.text);
				++multi->count;
			}
			else
			{
				multi->dvarList[multi->count] = Menus::GetAllocator()->DuplicateString(token.text);
			}

			isValueNext = !isValueNext;
		}

		if (didOverflow)
		{
			Logger::Warning("menus: a dvarStrList holds more than {} entries\n", maxListEntries);
		}
	}

	static void ReadFloatList(ListScan& scan, Game::multiDef_s* multi)
	{
		if (!TryOpenList(scan))
		{
			return;
		}

		multi->count = 0;
		multi->strDef = 0;

		bool didOverflow = false;
		ListToken label = {};

		while (NextToken(scan, label))
		{
			if (IsPunctuation(label, '}'))
			{
				break;
			}

			if (IsSeparator(label))
			{
				continue;
			}

			ListToken value = {};

			if (!NextToken(scan, value) || IsPunctuation(value, '}'))
			{
				break;
			}

			if (multi->count >= maxListEntries)
			{
				didOverflow = true;
				continue;
			}

			multi->dvarList[multi->count] = Menus::GetAllocator()->DuplicateString(label.text);
			multi->dvarValue[multi->count] = static_cast<float>(std::atof(value.text));
			++multi->count;
		}

		if (didOverflow)
		{
			Logger::Warning("menus: a dvarFloatList holds more than {} entries\n", maxListEntries);
		}
	}

	void MenuDvarList::Parse(Game::itemDef_s* item, const char* begin, const char* end)
	{
		if (!item || !begin || end <= begin)
		{
			return;
		}

		ListScan scan = { begin, end };

		if (!TryOpenList(scan))
		{
			return;
		}

		const bool isMulti = item->type == Game::ITEM_TYPE_MULTI && item->typeData.multi;
		const bool isDvarEnum = item->type == Game::ITEM_TYPE_DVARENUM;

		int depth = 1;
		ListToken token = {};

		while (depth > 0 && NextToken(scan, token))
		{
			if (IsPunctuation(token, '{'))
			{
				++depth;
				continue;
			}

			if (IsPunctuation(token, '}'))
			{
				--depth;
				continue;
			}

			if (depth != 1 || token.isQuoted)
			{
				continue;
			}

			if (isMulti && _stricmp(token.text, "dvarStrList") == 0)
			{
				ReadStringList(scan, item->typeData.multi);
			}
			else if (isMulti && _stricmp(token.text, "dvarFloatList") == 0)
			{
				ReadFloatList(scan, item->typeData.multi);
			}
			else if (isDvarEnum && _stricmp(token.text, "dvarEnumList") == 0)
			{
				ListToken name = {};

				if (NextToken(scan, name))
				{
					item->typeData.enumDvarName = Menus::GetAllocator()->DuplicateString(name.text);
				}
			}
		}
	}
}
