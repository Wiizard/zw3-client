#include "STDInclude.hpp"

namespace Game
{
	DB_BeginRecoverLostDevice_t DB_BeginRecoverLostDevice = nullptr;
	DB_EndRecoverLostDevice_t DB_EndRecoverLostDevice = nullptr;
	DB_EnumXAssets_FastFile_t DB_EnumXAssets_FastFile = nullptr;
	DB_FindXAssetHeader_t DB_FindXAssetHeader = nullptr;
	DB_IsXAssetDefault_t DB_IsXAssetDefault = nullptr;
	DB_GetRawBuffer_t DB_GetRawBuffer = nullptr;
	DB_GetRawFileLen_t DB_GetRawFileLen = nullptr;
	DB_LoadXAssets_t DB_LoadXAssets = nullptr;
	DB_SetXAssetName_t DB_SetXAssetName = nullptr;
	RMsg_SendMessages_t RMsg_SendMessages = nullptr;
	DB_GetXAssetName_t DB_GetXAssetName = nullptr;
	DB_FindXAssetDefaultHeaderInternal_t DB_FindXAssetDefaultHeaderInternal = nullptr;
	DB_FindXAssetEntry_t DB_FindXAssetEntry = nullptr;

	void** DB_XAssetPool = nullptr;
	unsigned int* g_poolSize = nullptr;

	void* ReallocateAssetPool(XAssetType type, unsigned int newSize)
	{
		const auto entrySize = static_cast<std::size_t>(DB_GetXAssetTypeSize(type));
		void* const pool = Utils::Memory::GetAllocator()->Allocate(sizeof(void*) + newSize * entrySize);

		DB_XAssetPool[type] = pool;
		g_poolSize[type] = newSize;

		return pool;
	}

	const char* DB_GetXAssetTypeName(unsigned int type)
	{
		return g_assetNames[type];
	}

	XAssetType DB_GetXAssetNameType(const char* name)
	{
		for (unsigned int i = 0; i < ASSET_TYPE_COUNT; ++i)
		{
			if (_stricmp(DB_GetXAssetTypeName(i), name))
			{
				continue;
			}

			if (i == ASSET_TYPE_CLIPMAP_SP)
			{
				return ASSET_TYPE_CLIPMAP_MP;
			}

			return static_cast<XAssetType>(i);
		}

		return ASSET_TYPE_COUNT;
	}

	int DB_GetZoneIndex(const std::string& name)
	{
		constexpr std::size_t zoneSize = 0xF8;
		constexpr std::size_t zoneName = 8;

		const int zoneCount = Utils::Hook::Get<int>(0x1415FC338);
		const auto* const zoneHandles = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(0x1415FC318));
		const auto* const zones = reinterpret_cast<const char*>(Utils::Hook::Rebase(0x1415FA320));

		for (int i = 0; i < zoneCount; ++i)
		{
			if (name == zones + zoneSize * zoneHandles[i] + zoneName)
			{
				return zoneHandles[i];
			}
		}

		return -1;
	}

	bool DB_IsZoneLoaded(const char* zone)
	{
		constexpr std::size_t zoneSize = 0xF8;
		constexpr std::size_t zoneName = 8;

		const int zoneCount = Utils::Hook::Get<int>(0x1415FC338);
		const auto* const zoneHandles = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(0x1415FC318));
		const auto* const zones = reinterpret_cast<const char*>(Utils::Hook::Rebase(0x1415FA320));

		for (int i = 0; i < zoneCount; ++i)
		{
			if (!std::strcmp(zones + zoneSize * zoneHandles[i] + zoneName, zone))
			{
				return true;
			}
		}

		return false;
	}

	void BindDatabase()
	{
		DB_BeginRecoverLostDevice = BindFunction<DB_BeginRecoverLostDevice_t>(0x14012CF30);
		DB_EndRecoverLostDevice = BindFunction<DB_EndRecoverLostDevice_t>(0x14012D1B0);
		DB_EnumXAssets_FastFile = BindFunction<DB_EnumXAssets_FastFile_t>(0x14012D260);
		DB_FindXAssetHeader = BindFunction<DB_FindXAssetHeader_t>(0x14012D6D0);
		DB_IsXAssetDefault = BindFunction<DB_IsXAssetDefault_t>(0x14012E180);
		DB_GetRawBuffer = BindFunction<DB_GetRawBuffer_t>(0x14012DD80);
		DB_GetRawFileLen = BindFunction<DB_GetRawFileLen_t>(0x14012DEB0);
		DB_LoadXAssets = BindFunction<DB_LoadXAssets_t>(0x14012EC40);
		DB_SetXAssetName = BindFunction<DB_SetXAssetName_t>(0x140117640);
		RMsg_SendMessages = BindFunction<RMsg_SendMessages_t>(0x14028DA10);
		DB_GetXAssetName = BindFunction<DB_GetXAssetName_t>(0x140117610);
		DB_FindXAssetDefaultHeaderInternal = BindFunction<DB_FindXAssetDefaultHeaderInternal_t>(0x14012D4F0);
		DB_FindXAssetEntry = BindFunction<DB_FindXAssetEntry_t>(0x14012D600);

		DB_XAssetPool = reinterpret_cast<void**>(Utils::Hook::Rebase(0x140421D60));
		g_poolSize = reinterpret_cast<unsigned int*>(Utils::Hook::Rebase(0x140421890));
	}
}
