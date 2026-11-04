#pragma once

namespace Components
{
	class Singleton : public Component
	{
	public:
		Singleton();

		static bool IsFirstInstance();

	private:
		static HANDLE mutex;
		static bool isFirstInstance;
	};
}
