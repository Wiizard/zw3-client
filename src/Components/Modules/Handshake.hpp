#pragma once

namespace Components
{
	class Handshake : public Component
	{
	public:
		Handshake();

		static bool IsInstalled();

	private:
		static bool isInstalled;
		static Utils::Hook netDataChecksumHook;
		static Utils::Hook defVersionHook;
		static Utils::Hook defFormatChecksumHook;

		static int BG_NetDataChecksum_Hk();
		static int LiveStorage_GetPersistentDataDefVersion_Hk();
		static int LiveStorage_GetPersistentDataDefFormatChecksum_Hk();
	};
}
