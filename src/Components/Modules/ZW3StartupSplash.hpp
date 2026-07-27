#pragma once

#include <string_view>

namespace StartupSplash
{
	void Start();
	void Stop();
	void SetStatus(std::string_view status);
	void SetProgress(float progress);
	bool IsActive();
}

namespace Components
{
	class ZW3StartupSplash final
	{
	public:
		static void Start()
		{
			::StartupSplash::Start();
		}

		static void Stop()
		{
			::StartupSplash::Stop();
		}

		static void SetStatus(const std::string_view status)
		{
			::StartupSplash::SetStatus(status);
		}

		static void SetProgress(const float progress)
		{
			::StartupSplash::SetProgress(progress);
		}

		static bool IsActive()
		{
			return ::StartupSplash::IsActive();
		}
	};
}
