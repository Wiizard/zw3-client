#pragma once

namespace Components
{
	class ConfigStrings : public Component
	{
	public:
		static constexpr int DEV_DVAR_CONFIGSTRINGS_CAPACITY = 512;
		static constexpr int SCRIPT_MODEL_ANIM_LIMIT = 1024;
		static constexpr int MAX_CONFIGSTRINGS = 13083 + 1 + DEV_DVAR_CONFIGSTRINGS_CAPACITY * 2 + SCRIPT_MODEL_ANIM_LIMIT;

		ConfigStrings();

		static bool HasRaisedTables();

		static bool HasServerModelStrings();

		static const char* CL_GetRumbleConfigString(int index);
		static unsigned int SV_GetRumbleConfigStringConst(int index);
		static void SV_SetRumbleConfigString(int index, const char* data);

	private:
		static void PatchConfigStrings();
		static bool PatchGameStateIndexBits();
		static void PatchModelConfigStrings();
		static void PatchServerConfigStrings();
		static void PatchServerModelConfigStrings();
		static void* SV_ClearServer_Memset_Hook(void* dest, int value, std::size_t size);

		static unsigned int SV_GetCachedModelConfigStringConst(int index);
		static void SV_SetCachedModelConfigString(int index, const char* data);

		static const char* CL_GetCachedModelConfigString(int index);
		static void CG_ConfigStringModified_Hook(int localClientNum);

		static void PatchDevDvarConfigStrings();
		static void SV_SetConfig_Hook(int start, int max, int bit);

		static void PatchScriptModelAnims();
		static int G_FindScriptModelAnimIndex(const char* name, int start, int max, int create, const char* errorName);
	};
}
