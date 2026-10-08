#pragma once

namespace Components
{
	class VisionFile : public Component
	{
	public:
		VisionFile();

		static bool LoadVisionSettingsFromBuffer(const char* buffer, const char* filename, Game::visionSetVars_t* settings);

	private:
		static std::vector<std::string> dvarExceptions;
		static std::unordered_map<std::string, std::string> visionReplacements;

		static bool ApplyExemptDvar(const char* dvarName, const char** buffer, const char* filename);
		static bool ApplyTokenToField(unsigned int fieldNum, const char* token, Game::visionSetVars_t* settings);
	};
}
