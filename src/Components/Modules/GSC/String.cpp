#include "STDInclude.hpp"

#include "Script.hpp"
#include "String.hpp"

namespace Components::GSC
{
	void String::AddScriptFunctions()
	{
		Script::AddFunction("ToUpper", []
		{
			const auto scriptValue = Game::Scr_GetConstString(0);
			const auto* string = Game::SL_ConvertToString(scriptValue);

			char out[1024]{};
			bool changed = false;

			std::size_t i = 0;

			while (i < sizeof(out))
			{
				const auto value = *string;
				const auto result = static_cast<char>(std::toupper(static_cast<unsigned char>(value)));
				out[i] = result;

				if (value != result)
				{
					changed = true;
				}

				if (result == '\0')
				{
					break;
				}

				++string;
				++i;
			}

			if (i >= sizeof(out))
			{
				Script::Scr_Error("string too long");
				return;
			}

			if (changed)
			{
				Game::Scr_AddString(out);
			}
			else
			{
				Game::SL_AddRefToString(scriptValue);
				Game::Scr_AddConstString(scriptValue);
				Game::SL_RemoveRefToString(scriptValue);
			}
		});

		Script::AddFunction("GetChar", []
		{
			const auto* str = Game::Scr_GetString(0);
			const auto index = Game::Scr_GetInt(1);

			if (!str)
			{
				Script::Scr_Error("GetChar: Illegal parameter!");
				return;
			}

			if (static_cast<std::size_t>(index) >= std::strlen(str))
			{
				Script::Scr_Error("GetChar: char index is out of bounds");
			}

			Game::Scr_AddInt(str[index]);
		});

		Script::AddFunction("StrICmp", []
		{
			const auto* string1 = Game::SL_ConvertToString(Game::Scr_GetConstString(0));
			const auto* string2 = Game::SL_ConvertToString(Game::Scr_GetConstString(1));

			Game::Scr_AddInt(_stricmp(string1, string2));
		});

		Script::AddFunction("IsEndStr", []
		{
			const auto* str = Game::Scr_GetString(0);
			const auto* suffix = Game::Scr_GetString(1);

			if (!str || !suffix)
			{
				Script::Scr_Error("IsEndStr: Illegal parameters!");
				return;
			}

			const auto strLength = std::strlen(str);
			const auto suffixLength = std::strlen(suffix);

			if (suffixLength > strLength)
			{
				Game::Scr_AddBool(0);
				return;
			}

			Game::Scr_AddBool(std::memcmp(str + strLength - suffixLength, suffix, suffixLength) == 0);
		});

		Script::AddFunction("Float", []
		{
			switch (Game::Scr_GetType(0))
			{
			case Game::VAR_STRING:
				Game::Scr_AddFloat(static_cast<float>(std::atof(Game::Scr_GetString(0))));
				break;
			case Game::VAR_FLOAT:
				Game::Scr_AddFloat(Game::Scr_GetFloat(0));
				break;
			case Game::VAR_INTEGER:
				Game::Scr_AddFloat(static_cast<float>(Game::Scr_GetInt(0)));
				break;
			default:
				Script::Scr_ParamError(0, Utils::String::VA("cannot cast %s to float", Game::Scr_GetTypeName(0)));
				break;
			}
		});

		Script::AddFunction("Strtol", []
		{
			const auto* input = Game::Scr_GetString(0);
			const auto base = Game::Scr_GetInt(1);

			char* end;
			const auto result = std::strtol(input, &end, base);

			if (input == end)
			{
				Script::Scr_ParamError(0, "cannot cast string to int");
			}

			Game::Scr_AddInt(result);
		});

		Script::AddFunction("IString", []
		{
			if (Game::Scr_GetType(0) != Game::VAR_STRING)
			{
				Script::Scr_ParamError(0, Utils::String::VA("cannot cast %s to istring", Game::Scr_GetTypeName(0)));
				return;
			}

			const auto value = Game::Scr_GetConstString(0);

			Game::SL_AddRefToString(value);
			Game::Scr_AddIString(Game::Scr_GetString(0));

			Game::SL_RemoveRefToString(value);
		});
	}

	String::String()
	{
		AddScriptFunctions();
	}
}
