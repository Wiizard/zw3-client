#pragma once

namespace Components
{
	class Flags : public Component
	{
	public:
		static bool HasFlag(const std::string& flag);
		static const std::wstring& GetOriginalCommandLine();
		static constexpr std::wstring_view RestartParentFlag = L"-zw3-restart-parent=";

	private:
		static std::vector<std::string> enabledFlags;
		static bool isParsed;
		static std::wstring originalCommandLine;

		static void ParseFlags();
	};
}
