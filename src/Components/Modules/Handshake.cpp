#include "STDInclude.hpp"

#include "Handshake.hpp"
#include "Logger.hpp"

namespace Components
{
	bool Handshake::isInstalled = false;
	Utils::Hook Handshake::netDataChecksumHook;
	Utils::Hook Handshake::defVersionHook;
	Utils::Hook Handshake::defFormatChecksumHook;

	constexpr std::uintptr_t BG_NetDataChecksum = 0x14008A9E0;
	constexpr std::uintptr_t LiveStorage_GetPersistentDataDefVersion = 0x1401F7C10;
	constexpr std::uintptr_t LiveStorage_GetPersistentDataDefFormatChecksum = 0x1401F7BE0;

	constexpr int netDataChecksum = -1740007858;

	constexpr int persistentDataDefVersion = 161;

	constexpr int persistentDataDefFormatChecksum = static_cast<int>(0x9085448Bu);

	int Handshake::BG_NetDataChecksum_Hk()
	{
		return netDataChecksum;
	}

	int Handshake::LiveStorage_GetPersistentDataDefVersion_Hk()
	{
		return persistentDataDefVersion;
	}

	int Handshake::LiveStorage_GetPersistentDataDefFormatChecksum_Hk()
	{
		return persistentDataDefFormatChecksum;
	}

	bool Handshake::IsInstalled()
	{
		return isInstalled;
	}

	Handshake::Handshake()
	{
		int failed = 0;

		failed += !netDataChecksumHook.Initialize(BG_NetDataChecksum,
			BG_NetDataChecksum_Hk, HOOK_JUMP)->Install()->IsInstalled();
		failed += !defVersionHook.Initialize(LiveStorage_GetPersistentDataDefVersion,
			LiveStorage_GetPersistentDataDefVersion_Hk, HOOK_JUMP)->Install()->IsInstalled();
		failed += !defFormatChecksumHook.Initialize(LiveStorage_GetPersistentDataDefFormatChecksum,
			LiveStorage_GetPersistentDataDefFormatChecksum_Hk, HOOK_JUMP)->Install()->IsInstalled();

		if (failed)
		{
			netDataChecksumHook.Uninstall();
			defVersionHook.Uninstall();
			defFormatChecksumHook.Uninstall();

			Logger::Error("handshake failed to seat {} of 3 hooks, the connect values are left alone", failed);
			return;
		}

		netDataChecksumHook.Quick();
		defVersionHook.Quick();
		defFormatChecksumHook.Quick();

		isInstalled = true;
	}
}
