#pragma once

namespace Components::GSC
{
	class IO : public Component
	{
	public:
		IO();

	private:
		static const char* forbiddenStrings[];

		static FILE* openScriptIOFileHandle;

		static bool ValidatePath(const char* function, const char* path);
		static std::filesystem::path BuildPath(const char* path);

		static void GScr_OpenFile();
		static void GScr_ReadStream();
		static void GScr_CloseFile();

		static void AddScriptFunctions();
	};
}
