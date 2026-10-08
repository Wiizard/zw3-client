#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Localization : public Component
	{
	public:
		Localization();

		static void Set(const std::string& reference, const std::string& value);
		static const char* Get(const char* key);

		static const char* LocalizeMapName(const char* mapName);
		static const char* GetMapImageName(const char* mapName);

	private:
		static std::unordered_map<std::string, std::string> strings;
		static std::mutex stringsMutex;
		static Dvar::Var ui_localize;

		static bool IsTranslating();

		static const char* SEH_StringEd_GetString_Stub(const char* reference);
		static void* SEH_SafeTranslateString_Lookup(unsigned int type, const char* reference);
		static void SEH_GetLocalizedTokenReference(char* token, std::size_t tokenSize);
		static const char* SEH_LocalizeTextMessage_Stub(const char* inputBuffer, const char* messageType, int errType);

		static void SetCredits();

		static void GSCr_LocalizeText();
		static void GSCr_LocalizeGametype();
	};
}
