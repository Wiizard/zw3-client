#pragma once

namespace Components
{
	class Steam : public Component
	{
	public:
		Steam();

	private:
		static Utils::Hook initHook;

		static char Steam_Init_Stub();
		static bool RedirectImports();
	};
}
