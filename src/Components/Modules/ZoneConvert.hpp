#pragma once

namespace Components
{
	class ZoneConvert : public Component
	{
	public:
		ZoneConvert();

		static bool IsEnabled();
		static std::string SearchPath(std::string_view group);

	private:
		static bool isEnabled;
	};
}
