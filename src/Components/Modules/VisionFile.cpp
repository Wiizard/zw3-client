#include "STDInclude.hpp"

#include "VisionFile.hpp"
#include "Logger.hpp"

extern "C"
{
	void LoadVisionSettingsFromBufferStub();

	std::uintptr_t VisionFile_ParseDone = 0;

	void VisionFile_LoadVisionSettingsFromBuffer(const char* buffer, const char* filename, Game::visionSetVars_t* settings)
	{
		Components::VisionFile::LoadVisionSettingsFromBuffer(buffer, filename, settings);
	}
}

namespace Components
{
	constexpr std::uintptr_t LoadVisionFile_Parse = 0x1400BFDA2;
	constexpr std::uintptr_t LoadVisionFile_PathLea = 0x1400BFDAF;
	constexpr std::uintptr_t LoadVisionFile_EndParseCall = 0x1400BFF06;
	constexpr std::uintptr_t LoadVisionFile_ParseDone = 0x1400BFF0B;
	static const std::uint8_t parseStart[] = { 0x33, 0xC0, 0x48, 0x89, 0x5C, 0x24, 0x30 };
	static const std::uint8_t pathLea[] = { 0x48, 0x8D, 0x4D, 0xF7 };
	static const std::uint8_t endParseCall[] = { 0xE8, 0x35, 0xAE, 0x1C, 0x00 };

	constexpr unsigned int visionDefFieldCount = 21;

	static Utils::Hook parseHook;

	std::vector<std::string> VisionFile::dvarExceptions =
	{
		"r_pretess",
	};

	std::unordered_map<std::string, std::string> VisionFile::visionReplacements
	{
		{"511", "r_glow"},
		{"516", "r_glowRadius0"},
		{"512", "r_glowBloomCutoff"},
		{"513", "r_glowBloomDesaturation"},
		{"514", "r_glowBloomIntensity0"},
		{"520", "r_filmEnable"},
		{"522", "r_filmContrast"},
		{"521", "r_filmBrightness"},
		{"523", "r_filmDesaturation"},
		{"524", "r_filmDesaturationDark"},
		{"525", "r_filmInvert"},
		{"526", "r_filmLightTint"},
		{"527", "r_filmMediumTint"},
		{"528", "r_filmDarkTint"},
		{"529", "r_primaryLightUseTweaks"},
		{"530", "r_primaryLightTweakDiffuseStrength"},
		{"531", "r_primaryLightTweakSpecularStrength"},
	};

	bool VisionFile::ApplyExemptDvar(const char* dvarName, const char** buffer, const char* filename)
	{
		for (const auto& exception : dvarExceptions)
		{
			if (!_stricmp(dvarName, exception.data()))
			{
				const auto* dvar = Game::Dvar_FindVar(dvarName);

				if (!dvar)
				{
					return false;
				}

				const auto* parsedValue = Game::Com_ParseOnLine(buffer);

				Game::Dvar_SetFromStringFromSource(dvar, parsedValue, Game::DVAR_SOURCE_INTERNAL);
				Logger::Print("Overriding '{}' from '{}'\n", dvar->name, filename);

				return true;
			}
		}

		return false;
	}

	bool VisionFile::ApplyTokenToField(unsigned int fieldNum, const char* token, Game::visionSetVars_t* settings)
	{
		const Game::visField_t& field = Game::visionDefFields[fieldNum];
		char* const target = reinterpret_cast<char*>(settings) + field.offset;

		if (field.fieldType == 0)
		{
			int value = 0;

			if (sscanf_s(token, "%i", &value) != 1)
			{
				return false;
			}

			*reinterpret_cast<bool*>(target) = value != 0;
			return true;
		}

		if (field.fieldType == 1)
		{
			float value = 0.0f;

			if (sscanf_s(token, "%f", &value) != 1)
			{
				return false;
			}

			*reinterpret_cast<float*>(target) = value;
			return true;
		}

		float values[3]{};

		if (sscanf_s(token, "%f %f %f", &values[0], &values[1], &values[2]) != 3)
		{
			return false;
		}

		std::memcpy(target, values, sizeof(values));
		return true;
	}

	bool VisionFile::LoadVisionSettingsFromBuffer(const char* buffer, const char* filename, Game::visionSetVars_t* settings)
	{
		bool wasRead[visionDefFieldCount]{};
		Game::Com_BeginParseSession(filename);

		while (true)
		{
			const char* token = Game::Com_Parse(&buffer);

			if (!*token)
			{
				break;
			}

			bool found = false;
			unsigned int fieldNum = 0;

			const char* fieldName = token;
			const auto replacement = visionReplacements.find(token);

			if (replacement != visionReplacements.end())
			{
				fieldName = replacement->second.data();
			}

			for (fieldNum = 0; fieldNum < visionDefFieldCount; ++fieldNum)
			{
				if (!wasRead[fieldNum] && !_stricmp(fieldName, Game::visionDefFields[fieldNum].name))
				{
					found = true;
					break;
				}
			}

			if (!found)
			{
				if (!ApplyExemptDvar(token, &buffer, filename))
				{
					Logger::Warning("unknown dvar '{}' in file '{}'\n", token, filename);
					Game::Com_SkipRestOfLine(&buffer);
				}

				continue;
			}

			token = Game::Com_ParseOnLine(&buffer);

			if (ApplyTokenToField(fieldNum, token, settings))
			{
				wasRead[fieldNum] = true;
			}
			else
			{
				Logger::Warning("malformed dvar '{}' in file '{}'\n", token, filename);
				Game::Com_SkipRestOfLine(&buffer);
			}
		}

		Game::Com_EndParseSession();
		return true;
	}

	VisionFile::VisionFile()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(LoadVisionFile_Parse, parseStart, sizeof(parseStart))
			&& Utils::Hook::MatchesBytes(LoadVisionFile_PathLea, pathLea, sizeof(pathLea))
			&& Utils::Hook::MatchesBytes(LoadVisionFile_EndParseCall, endParseCall, sizeof(endParseCall));

		if (!isExpected)
		{
			Logger::Error("visionfile: LoadVisionFile does not read as expected, the engine's parser stays\n");
			return;
		}

		VisionFile_ParseDone = Utils::Hook::Rebase(LoadVisionFile_ParseDone);

		if (!parseHook.Initialize(LoadVisionFile_Parse, LoadVisionSettingsFromBufferStub, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("visionfile: could not seat the vision parse hook, the engine's parser stays\n");
			return;
		}

		Utils::Hook::Nop(LoadVisionFile_Parse + 5, sizeof(parseStart) - 5);
	}
}
