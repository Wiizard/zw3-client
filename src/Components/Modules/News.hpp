#pragma once

#include <rapidjson/document.h>

namespace Components
{
	class News : public Component
	{
	public:
		News();

	private:
		static const char* GetNewsText();

		static void FetchInfo();
		static void ApplyInfo(const std::string& info);
		static bool ProcessPopmenus(const rapidjson::Document& document);
		static std::optional<std::pair<std::string, std::string>> ExtractPopmenuItem(const rapidjson::Value& menuItem);
		static bool ShouldShowForRevision(const rapidjson::Value& revisions);
	};
}
