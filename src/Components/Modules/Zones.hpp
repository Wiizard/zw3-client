#pragma once

namespace Components
{
	class Zones : public Component
	{
	public:
		Zones();

		static bool IsReady();

		static std::uint32_t Version();
		static void SetVersion(std::uint32_t version);

		static bool CanRead(std::uint32_t version);
	};
}
