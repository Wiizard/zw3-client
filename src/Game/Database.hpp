#pragma once

namespace Game
{
	typedef void(*DB_BeginRecoverLostDevice_t)();
	extern DB_BeginRecoverLostDevice_t DB_BeginRecoverLostDevice;

	typedef void(*DB_EndRecoverLostDevice_t)();
	extern DB_EndRecoverLostDevice_t DB_EndRecoverLostDevice;

	typedef void(*DB_EnumXAssets_FastFile_t)(XAssetType type, void(*func)(void* header, void* data), void* data, bool includeOverride);
	extern DB_EnumXAssets_FastFile_t DB_EnumXAssets_FastFile;

	typedef void*(*DB_FindXAssetHeader_t)(unsigned int type, const char* name);
	extern DB_FindXAssetHeader_t DB_FindXAssetHeader;

	typedef bool(*DB_IsXAssetDefault_t)(unsigned int type, const char* name);
	extern DB_IsXAssetDefault_t DB_IsXAssetDefault;

	typedef void(*DB_GetRawBuffer_t)(const void* rawfile, char* buffer, int size);
	extern DB_GetRawBuffer_t DB_GetRawBuffer;

	typedef int(*DB_GetRawFileLen_t)(const void* rawfile);
	extern DB_GetRawFileLen_t DB_GetRawFileLen;

	typedef void(*DB_LoadXAssets_t)(XZoneInfo* zoneInfo, unsigned int zoneCount, int sync);
	extern DB_LoadXAssets_t DB_LoadXAssets;

	typedef void(*DB_SetXAssetName_t)(XAsset* asset, const char* name);
	extern DB_SetXAssetName_t DB_SetXAssetName;

	typedef void(*RMsg_SendMessages_t)();
	extern RMsg_SendMessages_t RMsg_SendMessages;

	extern void** DB_XAssetPool;
	extern unsigned int* g_poolSize;

	void* ReallocateAssetPool(XAssetType type, unsigned int newSize);

	typedef const char*(*DB_GetXAssetName_t)(const XAsset* asset);
	extern DB_GetXAssetName_t DB_GetXAssetName;

	const char* DB_GetXAssetTypeName(unsigned int type);
	XAssetType DB_GetXAssetNameType(const char* name);
	int DB_GetZoneIndex(const std::string& name);
	bool DB_IsZoneLoaded(const char* zone);

	typedef void*(*DB_FindXAssetDefaultHeaderInternal_t)(unsigned int type);
	extern DB_FindXAssetDefaultHeaderInternal_t DB_FindXAssetDefaultHeaderInternal;

	typedef XAssetEntry*(*DB_FindXAssetEntry_t)(unsigned int type, const char* name);
	extern DB_FindXAssetEntry_t DB_FindXAssetEntry;

	void BindDatabase();
}
