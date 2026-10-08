#include "STDInclude.hpp"

#include <zlib.h>

#include <condition_variable>
#include <deque>
#include <unordered_set>

#include <Utils/InflateReadAhead.hpp>
#include <Utils/IW4xZoneDecoder.hpp>
#include <Utils/AesCtr.hpp>

#include "FastFiles.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Flags.hpp"
#include "LobbyScene.hpp"
#include "Party.hpp"
#include "Logger.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"
#include "ZoneBuilder.hpp"
#include "Zones.hpp"
#include "ZoneConvert.hpp"

namespace Components
{
	std::vector<std::string> FastFiles::zonePaths;

	Dvar::Var FastFiles::g_loadingInitialZones;

	Utils::Hook FastFiles::loadInitialZonesHook;
	Utils::Hook FastFiles::loadDLCUIZonesHook;
	Utils::Hook FastFiles::loadGfxZonesHook;
	Utils::Hook FastFiles::zoneDirHooks[3];

	constexpr std::uintptr_t g_load = 0x140D36790;
	constexpr std::size_t dbFileName = 8;

	constexpr std::uintptr_t R_LoadGraphicsAssets_LoadCall = 0x140030D90;
	constexpr std::uintptr_t DB_LoadXAssets = 0x14012EC40;

	constexpr std::uintptr_t R_LoadGraphicsAssets_GfxLoadCall = 0x140030CA4;

	constexpr std::uintptr_t Com_AssetLoadUI_LoadCall = 0x1401F370E;

	static const char* const dlcUIZones[] = { "dlc1_ui_mp", "dlc2_ui_mp" };

	constexpr std::uintptr_t Sys_GetMapZoneDir = 0x1402A5420;
	constexpr std::uintptr_t Sys_GetMapZoneDirCalls[] = { 0x14012D3D9, 0x14012D464, 0x14012FA8E };

	constexpr std::uintptr_t Sys_DefaultInstallPath = 0x14028E230;

	constexpr std::uintptr_t DB_InflateInit_UnsignedJnz = 0x140118086;
	static const std::uint8_t unsignedJnz[] = { 0x75, 0x1D };

	constexpr std::uintptr_t Load_XModelSurfs_SurfaceArrayCall = 0x140123970;
	constexpr std::uintptr_t Load_XSurfaceArray = 0x140123BB0;
	constexpr std::uintptr_t varXModelSurfs = 0x140DB72F0;
	constexpr std::size_t xmodelSurfsNumSurfs = 0x10;

	static Utils::Hook surfaceArrayHook;

	constexpr std::uintptr_t DB_LoadXAssets_NotifyCall = 0x14012F121;
	constexpr std::uintptr_t Sys_NotifyDatabase = 0x14020A9F0;
	constexpr std::uintptr_t g_zoneInfoCount = 0x1415FCD90;
	constexpr std::uintptr_t DB_LoadXFile_VersionReadCall = 0x140117DA0;
	constexpr std::uintptr_t DB_ReadXFileUncompressed = 0x140118400;
	constexpr std::uintptr_t DB_GetLoadedFraction = 0x140117CE0;
	constexpr std::uintptr_t DB_GetLoadedFractionCalls[] = { 0x1400B29B1, 0x140109A5D, 0x140251045, 0x14026E972 };

	static std::atomic<int> currentZone = 0;

	static std::atomic_bool isInitialLoadPending = false;
	static std::atomic_bool isMainMenuReached = false;
	static std::atomic<int> maxZones = 0;

	static Utils::Hook notifyDatabaseHook;
	static Utils::Hook versionReadHook;
	static Utils::Hook loadedFractionHooks[std::size(DB_GetLoadedFractionCalls)];

	constexpr std::uint32_t iw4xFirstVersion = 316;
	constexpr std::uint32_t iw4xEncryptedVersion = 319;
	constexpr std::uint32_t iw4xPkcs1Version = 359;
	constexpr std::uint32_t iw4xLastVersion = 360;

	constexpr std::size_t authBlockSize = 0x2000;
	constexpr std::size_t iw4xKeyBlobSize = 256;
	constexpr std::size_t authMasterHashCount = 244;
	constexpr std::size_t authHashSize = 32;

	constexpr std::uintptr_t DB_AuthLoad_Inflate_BlockCall = 0x140117A09;
	constexpr std::uintptr_t authLoadBlockStep = 0x140117660;
	constexpr std::uintptr_t authMasterIndex = 0x140D34674;
	constexpr std::uintptr_t authSubIndex = 0x140D34678;
	constexpr std::uintptr_t authMasterHashes = 0x140D307F4;
	constexpr std::uintptr_t authBlockHashes = 0x140D32674;

	constexpr std::uintptr_t authSignatureJz = 0x1401176CB;
	static const std::uint8_t signatureJz[] = { 0x74, 0x1D };

	static const std::uint8_t iw4xZoneKey[1191] =
	{
		0x30, 0x82, 0x04, 0xA3, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00, 0xBD, 0x7E, 0xD6, 0xE2,
		0xE9, 0x1C, 0xB1, 0x08, 0x68, 0xA2, 0xEC, 0xC2, 0x47, 0xCB, 0x60, 0x00, 0x0F, 0xF5, 0x6B, 0x75,
		0x32, 0xA3, 0xE4, 0xEB, 0x85, 0x2F, 0x23, 0x97, 0x6E, 0x13, 0x33, 0xBC, 0xA3, 0xBB, 0x61, 0x88,
		0xDE, 0xB8, 0xA8, 0x33, 0xD6, 0xB8, 0xA3, 0xDB, 0x48, 0x09, 0xE8, 0x7D, 0x0A, 0x43, 0xBF, 0xDB,
		0x52, 0x32, 0x5C, 0x8B, 0x0A, 0x46, 0x01, 0x95, 0x81, 0x97, 0x40, 0x46, 0x28, 0x73, 0x96, 0x88,
		0x7C, 0x9A, 0x08, 0x94, 0x62, 0xDC, 0x10, 0xF1, 0xE7, 0x2B, 0xF8, 0xDE, 0x21, 0xA2, 0xD1, 0x39,
		0x45, 0x1A, 0xD2, 0x41, 0x45, 0xC1, 0xEE, 0x01, 0xFF, 0xD6, 0x33, 0x7F, 0xB4, 0x64, 0x41, 0x87,
		0xE6, 0x71, 0xB7, 0x0D, 0xE8, 0xCF, 0xF4, 0xCB, 0x28, 0x3C, 0xA6, 0x92, 0x3A, 0x69, 0x95, 0x93,
		0xBE, 0x72, 0xA4, 0x86, 0x76, 0xF6, 0x99, 0x7E, 0xD8, 0xB0, 0x63, 0x1E, 0xC7, 0x67, 0x8B, 0xF7,
		0xF6, 0x6B, 0x46, 0xF1, 0xA1, 0x7C, 0x26, 0x15, 0x0E, 0xF9, 0xA6, 0x3F, 0x67, 0x54, 0x16, 0x5C,
		0xD6, 0xAE, 0xDE, 0x12, 0xF8, 0x30, 0x9F, 0x5D, 0xF2, 0xF7, 0xAF, 0x20, 0xAE, 0x69, 0x06, 0x28,
		0x80, 0xA9, 0xD5, 0x94, 0x74, 0x3D, 0xDB, 0x5A, 0x77, 0x87, 0xAE, 0x1D, 0x34, 0x20, 0xF9, 0x8F,
		0x23, 0x21, 0xCE, 0x86, 0xB6, 0xBF, 0x8E, 0xF3, 0xEC, 0x4A, 0xDF, 0xBE, 0x12, 0x51, 0x85, 0x4F,
		0x48, 0x52, 0x2E, 0x8A, 0x88, 0xC6, 0x6C, 0xD2, 0xCD, 0x19, 0xCF, 0xDF, 0x0A, 0x9B, 0xB5, 0x0B,
		0x36, 0x6D, 0x28, 0x36, 0x57, 0xC3, 0x84, 0xDD, 0xC2, 0xA1, 0x1B, 0x7A, 0xE8, 0xE9, 0x1F, 0xE4,
		0xC8, 0x8E, 0x8A, 0x5A, 0x49, 0x0F, 0xDB, 0x8D, 0x60, 0x48, 0xAD, 0x81, 0x2C, 0xE8, 0x44, 0x42,
		0x57, 0xFA, 0x7C, 0x18, 0xAC, 0xEF, 0x30, 0xC9, 0xC6, 0xA0, 0x4D, 0xFD, 0x02, 0x03, 0x01, 0x00,
		0x01, 0x02, 0x82, 0x01, 0x00, 0x1C, 0x18, 0x93, 0x59, 0xDF, 0x80, 0x5E, 0x8B, 0x45, 0xA0, 0x6A,
		0x84, 0x3F, 0xCA, 0xDA, 0xB8, 0x07, 0xA5, 0xB6, 0xC2, 0x10, 0xB9, 0x16, 0x37, 0x09, 0x6F, 0x3C,
		0xD2, 0xB6, 0x02, 0x68, 0xD8, 0x5E, 0x5A, 0x69, 0x12, 0xB7, 0x1B, 0x1F, 0xED, 0x57, 0xB7, 0xD6,
		0xAB, 0xAB, 0x99, 0xB4, 0x7B, 0xDD, 0xAA, 0xBF, 0xE6, 0x8F, 0xE0, 0x61, 0xB2, 0x47, 0xDA, 0xAB,
		0x5F, 0x74, 0x70, 0x6D, 0x9A, 0x39, 0x63, 0x31, 0xFD, 0x98, 0xA3, 0xEA, 0x03, 0xBE, 0x48, 0xAC,
		0xC6, 0x81, 0x25, 0x16, 0xE8, 0x30, 0x8A, 0x88, 0x84, 0xFA, 0x47, 0x08, 0xC7, 0x9E, 0xC5, 0x2B,
		0x39, 0xE6, 0xA9, 0xE6, 0xC6, 0xD7, 0x83, 0x49, 0xE8, 0x11, 0x75, 0xE8, 0xD3, 0x4A, 0x22, 0x93,
		0x44, 0x0F, 0xFA, 0x36, 0x24, 0x56, 0x3E, 0xD3, 0x6B, 0xAD, 0x80, 0x27, 0xFE, 0xBB, 0xE2, 0xC2,
		0x4D, 0x79, 0x69, 0x65, 0xB8, 0xA7, 0xFB, 0x42, 0x9B, 0xE9, 0x41, 0x98, 0x60, 0xB9, 0xEB, 0xD8,
		0xB2, 0x25, 0xAD, 0xF1, 0xDC, 0x0B, 0x04, 0x9F, 0x0F, 0xD0, 0x96, 0x9E, 0x1D, 0x7A, 0x73, 0xC5,
		0xE0, 0x7A, 0x5C, 0xF9, 0xF8, 0x0F, 0xCB, 0xA1, 0xFE, 0xE8, 0x94, 0x84, 0x6D, 0xB4, 0x2B, 0xFD,
		0x64, 0xC2, 0xAF, 0xD3, 0x48, 0x35, 0xDF, 0xCB, 0x2D, 0xD8, 0x2E, 0xE8, 0x35, 0x33, 0x6E, 0xBD,
		0xE1, 0xD5, 0x90, 0xFC, 0x67, 0x9B, 0xFF, 0x9F, 0xA0, 0x54, 0x02, 0xDB, 0xFB, 0xCC, 0xA8, 0x9E,
		0x1C, 0x35, 0x88, 0x10, 0x43, 0x2B, 0xA5, 0xCF, 0x78, 0x4F, 0xF0, 0xC6, 0x74, 0x4D, 0x47, 0xA4,
		0x9B, 0x22, 0x89, 0x56, 0xF0, 0x6A, 0xEA, 0xFA, 0x6B, 0xDD, 0xDA, 0xB1, 0x2F, 0x55, 0x6A, 0x5C,
		0xEF, 0xF6, 0x31, 0xB0, 0x87, 0xAC, 0xBE, 0xCB, 0xAC, 0x8D, 0xA6, 0xC2, 0x79, 0x75, 0x20, 0xE0,
		0x61, 0x49, 0x69, 0xA3, 0xA1, 0x02, 0x81, 0x81, 0x00, 0xC9, 0x70, 0xB5, 0x6E, 0xA6, 0x4A, 0x49,
		0xF1, 0xA9, 0xC7, 0xEA, 0x6E, 0xCF, 0x23, 0xB0, 0x29, 0x93, 0x80, 0x2A, 0xA9, 0x4E, 0x29, 0x5C,
		0x3B, 0xE5, 0x9C, 0xA5, 0x84, 0x0C, 0x1B, 0x9D, 0xD4, 0xDC, 0xA4, 0x63, 0xCD, 0xDD, 0x70, 0x9B,
		0xBE, 0x8F, 0x29, 0xB3, 0x1E, 0x06, 0x46, 0xD1, 0xE6, 0xEC, 0xE7, 0xB0, 0x55, 0x44, 0xA5, 0x24,
		0x56, 0x5F, 0x28, 0x58, 0x32, 0xCF, 0xDC, 0x1F, 0x74, 0x9A, 0xB6, 0x78, 0x83, 0x08, 0x95, 0x3D,
		0x15, 0x54, 0x23, 0x8B, 0x15, 0x0D, 0x9B, 0x34, 0x2B, 0x55, 0xFE, 0x4F, 0x26, 0x7E, 0x59, 0x62,
		0xE8, 0x31, 0xC1, 0x8A, 0x6E, 0xCB, 0xB8, 0xFA, 0xBD, 0x39, 0xAF, 0x9A, 0x9E, 0xF8, 0x7B, 0x3F,
		0x0E, 0x0A, 0x22, 0x40, 0xD6, 0x28, 0xA4, 0xF5, 0x4E, 0x82, 0x45, 0xDE, 0x81, 0xC5, 0x7F, 0x90,
		0x44, 0x01, 0x5F, 0x93, 0xCD, 0x95, 0x16, 0x3D, 0xE9, 0x02, 0x81, 0x81, 0x00, 0xF0, 0xD1, 0xE9,
		0x8D, 0xA9, 0xDF, 0xFA, 0x1E, 0xA6, 0xFE, 0x10, 0x3B, 0x68, 0x7C, 0x0E, 0x52, 0xD2, 0x19, 0x0C,
		0xA5, 0xEB, 0xFD, 0xB4, 0x1A, 0xDF, 0x03, 0x5B, 0x3E, 0x82, 0x5D, 0x99, 0x9B, 0x71, 0x23, 0xFB,
		0xFE, 0x69, 0xB3, 0x2D, 0xEB, 0xE7, 0x67, 0xBC, 0x9E, 0xF9, 0x05, 0xE6, 0x4D, 0x2B, 0x1C, 0xE9,
		0x12, 0xFF, 0xF3, 0xD0, 0xC8, 0x42, 0x9F, 0x00, 0xFE, 0x82, 0xC3, 0x7A, 0xB1, 0x4F, 0x94, 0xA1,
		0xAD, 0x90, 0xE0, 0xA4, 0x3F, 0x78, 0x55, 0x5A, 0x54, 0x59, 0x4A, 0xF9, 0xEF, 0x17, 0x40, 0xD1,
		0x09, 0xA5, 0x0D, 0xAC, 0xAB, 0x8C, 0xF0, 0xFD, 0x05, 0x6B, 0x9A, 0xEC, 0x3D, 0x98, 0x95, 0x5A,
		0x33, 0xA7, 0x47, 0x16, 0xA8, 0x9F, 0xBA, 0xD3, 0xBE, 0xCD, 0x28, 0xEE, 0x17, 0x64, 0x40, 0xBF,
		0xEC, 0x88, 0xCD, 0x97, 0x1F, 0xBD, 0x3A, 0xE6, 0x16, 0xFE, 0x0F, 0xDE, 0xF5, 0x02, 0x81, 0x81,
		0x00, 0xBF, 0x7C, 0xD7, 0xCB, 0xAE, 0x61, 0xF2, 0x36, 0xBA, 0xD9, 0x62, 0xBE, 0x21, 0x44, 0x60,
		0xA2, 0xB5, 0x27, 0x61, 0xE6, 0x7D, 0x79, 0x8D, 0xC7, 0x16, 0x77, 0x39, 0x53, 0xF4, 0x1A, 0x90,
		0x87, 0x97, 0x92, 0xE1, 0x99, 0x01, 0xC6, 0x99, 0x16, 0xA5, 0x8A, 0xD3, 0x4D, 0x58, 0x54, 0x1C,
		0x16, 0xB3, 0xDF, 0x6E, 0xDD, 0x2F, 0x9A, 0xF8, 0x96, 0xEE, 0x70, 0x30, 0x9F, 0x64, 0xBE, 0x70,
		0x5C, 0x6C, 0xF1, 0xC6, 0x4F, 0x71, 0x6A, 0x44, 0x9D, 0xB0, 0xD4, 0xF4, 0xD2, 0x77, 0x93, 0xB1,
		0x1C, 0xFC, 0xEA, 0xF9, 0x9C, 0xB3, 0x01, 0x0F, 0xA7, 0x80, 0x1C, 0xE6, 0x16, 0x7A, 0xAC, 0x86,
		0x16, 0x38, 0xEE, 0xF8, 0x41, 0xE4, 0x1D, 0x6C, 0x8C, 0x51, 0x0F, 0xCC, 0xA8, 0x88, 0x0C, 0x7F,
		0x70, 0x39, 0x20, 0x67, 0xEA, 0xDE, 0xAE, 0x6B, 0x9A, 0x69, 0xDF, 0xCC, 0x65, 0xE2, 0x32, 0x39,
		0x79, 0x02, 0x81, 0x80, 0x01, 0xDE, 0xCF, 0x7E, 0x8F, 0x2C, 0x33, 0x28, 0x1B, 0xC9, 0xEB, 0x5C,
		0x5A, 0xC2, 0x63, 0xE6, 0x16, 0xC5, 0xA5, 0x08, 0x80, 0xDD, 0xB6, 0x91, 0x62, 0xDC, 0x06, 0xD0,
		0x64, 0x78, 0xCF, 0xA1, 0x9A, 0x6E, 0x5A, 0x1D, 0xAE, 0xBA, 0x7A, 0x87, 0xD3, 0x83, 0x45, 0xBE,
		0xC2, 0x56, 0x5E, 0x64, 0x89, 0x0A, 0x2F, 0x71, 0x3B, 0x55, 0xAC, 0x70, 0x71, 0xBC, 0x04, 0x68,
		0xF5, 0xA1, 0x09, 0x09, 0xE9, 0x81, 0x51, 0x04, 0x25, 0x14, 0xE9, 0x91, 0xA8, 0xA0, 0x99, 0x14,
		0x00, 0xA1, 0x89, 0x71, 0x66, 0xEF, 0xD4, 0xEF, 0xCB, 0x3D, 0x60, 0xF2, 0xF0, 0x24, 0x4B, 0x02,
		0xC8, 0xC4, 0x2A, 0x43, 0x8C, 0x34, 0xD4, 0xBF, 0x83, 0xF4, 0x14, 0x63, 0xF8, 0xE1, 0x9D, 0x95,
		0x64, 0xC8, 0x85, 0x98, 0xDE, 0xE9, 0x75, 0xD4, 0x23, 0x77, 0xDD, 0x4D, 0x9C, 0xCD, 0xA1, 0x4D,
		0xDA, 0x69, 0x4B, 0x25, 0x02, 0x81, 0x80, 0x56, 0x6F, 0x21, 0x4E, 0xD2, 0x95, 0x50, 0xDD, 0x1E,
		0xFE, 0x08, 0xCD, 0xE0, 0x1E, 0x97, 0x98, 0x3C, 0xED, 0xFB, 0x7F, 0x53, 0x4A, 0xFC, 0x90, 0x4B,
		0x5E, 0x96, 0x09, 0x5D, 0x57, 0x7C, 0x5C, 0x39, 0x21, 0x10, 0xA5, 0xAA, 0x49, 0xB7, 0xC9, 0x41,
		0x2B, 0xF4, 0xAC, 0x95, 0x07, 0xBC, 0x45, 0xCD, 0xAE, 0x42, 0x05, 0x12, 0xA0, 0x1C, 0x8E, 0x0F,
		0xF8, 0x0A, 0xB3, 0x72, 0x50, 0xCD, 0x0D, 0x21, 0x2D, 0x67, 0x81, 0x99, 0x69, 0xFF, 0xF8, 0x5E,
		0x23, 0x19, 0x86, 0x03, 0x3D, 0x42, 0x6F, 0x38, 0xFB, 0x39, 0x1C, 0xBA, 0x96, 0x69, 0xC6, 0xDC,
		0xE7, 0x52, 0x7A, 0x7E, 0xC5, 0x98, 0x03, 0x8A, 0x3A, 0x88, 0x7C, 0x9A, 0xB0, 0x70, 0x26, 0x62,
		0xD6, 0xF4, 0x13, 0x1D, 0x37, 0x7E, 0x33, 0x3F, 0xBB, 0xFF, 0x91, 0x0D, 0xA6, 0xBB, 0x82, 0x30,
		0x34, 0xB1, 0xD3, 0xA4, 0xA7, 0xD6, 0x91,
	};

	static std::uint32_t iw4xVersion = 0;
	static bool isIW4xFormatReady = false;
	static Utils::AesCtr iw4xCtr;
	static std::array<std::uint8_t, 16> iw4xBaseIv{};
	static Utils::Hook blockStepHook;

	static void ResetIW4xCtr()
	{
		iw4xCtr.Reset();
	}

	static bool TryStartIW4xCtr(const std::uint8_t* blob)
	{
		static rsa_key zoneKey{};
		static const bool isKeyImported = []
		{
			ltc_mp = ltm_desc;
			register_hash(&sha256_desc);
			register_cipher(&aes_desc);

			return rsa_import(iw4xZoneKey, sizeof(iw4xZoneKey), &zoneKey) == CRYPT_OK;
		}();

		if (!isKeyImported)
		{
			return false;
		}

		ltc_rsa_op_parameters parameters{};
		parameters.params.hash_idx = find_hash("sha256");
		parameters.params.mgf1_hash_idx = parameters.params.hash_idx;
		parameters.padding = LTC_PKCS_1_OAEP;

		if (iw4xVersion >= iw4xPkcs1Version)
		{
			parameters.padding = LTC_PKCS_1_V1_5;
		}

		std::array<std::uint8_t, 40> keyAndIv{};
		unsigned long length = static_cast<unsigned long>(keyAndIv.size());
		int isValid = 0;

		if (rsa_decrypt_key_v2(blob, iw4xKeyBlobSize, keyAndIv.data(), &length, &parameters, &isValid, &zoneKey) != CRYPT_OK
			|| !isValid || length != keyAndIv.size())
		{
			return false;
		}

		std::memcpy(iw4xBaseIv.data(), keyAndIv.data() + 24, iw4xBaseIv.size());

		return iw4xCtr.Start(keyAndIv.data(), 24, iw4xBaseIv.data());
	}

	static bool AuthLoadBlockStep_Hk(std::uint8_t* block, bool* isHashBlock)
	{
		if (iw4xCtr.IsReady())
		{
			const auto master = Utils::Hook::Get<std::uint32_t>(authMasterIndex);
			const auto sub = Utils::Hook::Get<std::uint32_t>(authSubIndex);
			const std::uint8_t* iv = iw4xBaseIv.data();

			if (master > authMasterHashCount)
			{
				Game::Com_Error(Game::ERR_DROP, "IW4x zone %s has more blocks than its header holds hashes for", FastFiles::Current().data());
			}

			if (master != 0 && sub == 0)
			{
				iv = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(authMasterHashes)) + authHashSize * (master - 1);
			}
			else if (master != 0)
			{
				iv = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(authBlockHashes)) + authHashSize * (sub - 1);
			}

			if (!iw4xCtr.SetIv(iv) || !iw4xCtr.Crypt(block, authBlockSize))
			{
				Game::Com_Error(Game::ERR_DROP, "Unable to decrypt IW4x zone %s", FastFiles::Current().data());
			}
		}

		return reinterpret_cast<bool(*)(std::uint8_t*, bool*)>(Utils::Hook::Rebase(authLoadBlockStep))(block, isHashBlock);
	}

	static void SeatIW4xFormat()
	{
		if (!Utils::Hook::BranchesTo(DB_AuthLoad_Inflate_BlockCall, authLoadBlockStep, HOOK_CALL)
			|| !Utils::Hook::MatchesBytes(authSignatureJz, signatureJz, sizeof(signatureJz)))
		{
			Logger::Error("fastfiles: DB_AuthLoad_Inflate's block step does not read as expected, IW4x's signed zones will not load\n");
			return;
		}

		if (!blockStepHook.Initialize(DB_AuthLoad_Inflate_BlockCall, reinterpret_cast<void*>(AuthLoadBlockStep_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			blockStepHook.Uninstall();
			Logger::Error("fastfiles: could not hook DB_AuthLoad_Inflate's block step, IW4x's signed zones will not load\n");
			return;
		}

		blockStepHook.Quick();
		Utils::Hook::Nop(authSignatureJz, sizeof(signatureJz));
		isIW4xFormatReady = true;
	}

	static void Sys_NotifyDatabase_Hk()
	{
		currentZone = 0;
		maxZones = Utils::Hook::Get<int>(g_zoneInfoCount);

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Sys_NotifyDatabase))();
	}

	static void DB_ReadXFileUncompressed_Hk(void* buffer, const int size)
	{
		++currentZone;

		reinterpret_cast<void(*)(void*, int)>(Utils::Hook::Rebase(DB_ReadXFileUncompressed))(buffer, size);

		auto* const version = static_cast<std::uint32_t*>(buffer);
		Zones::SetVersion(*version);

		if (*version < iw4xFirstVersion || *version > iw4xLastVersion || !isIW4xFormatReady || !Zones::CanRead(*version))
		{
			return;
		}

		iw4xVersion = *version;
		*version = XFILE_VERSION;
	}

	constexpr std::uintptr_t DB_LoadXFile_HeaderReadCall = 0x140117D66;

	constexpr std::uintptr_t DB_LoadXFile_InflateInitCall = 0x140117E1B;
	constexpr std::uintptr_t DB_InflateInit = 0x140118070;
	constexpr std::uintptr_t DB_ReadXFile = 0x1401182D0;

	constexpr std::uintptr_t DB_ReadXFile_InflateCall = 0x1401182ED;
	constexpr std::uintptr_t DB_AuthLoad_Inflate = 0x140117960;

	static const std::array<unsigned char, 32> zw3ZoneKey =
	{
		0x1F, 0x76, 0x3E, 0xCD, 0xD0, 0x4D, 0x4C, 0xD1,
		0x9E, 0xD7, 0x6A, 0xFE, 0xA7, 0x7B, 0x2B, 0x18,
		0xBA, 0x33, 0x58, 0xCE, 0x2A, 0xAA, 0xD0, 0x3F,
		0xCD, 0x39, 0x39, 0x17, 0xFB, 0x52, 0xD6, 0x46,
	};

	constexpr std::size_t zw3NonceSize = 16;

	static bool isIW4xZone = false;
	static std::uint8_t lastByteRead = 0;

	static bool isZW3Zone = false;
	static Utils::AesCtr zw3Ctr;

	static bool IsPrivateZW3Module()
	{
		HMODULE module = nullptr;

		if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCSTR>(&IsPrivateZW3Module), &module))
		{
			return false;
		}

		char modulePath[MAX_PATH]{};

		if (!GetModuleFileNameA(module, modulePath, sizeof(modulePath)))
		{
			return false;
		}

		return Utils::String::ToLower(std::filesystem::path(modulePath).filename().string()) == "zw3.dll";
	}

	static void ResetZW3Ctr()
	{
		zw3Ctr.Reset();
	}

	static void InitZW3Ctr(const unsigned char* nonce)
	{
		if (!zw3Ctr.Start(zw3ZoneKey.data(), zw3ZoneKey.size(), nonce))
		{
			Game::Com_Error(Game::ERR_FATAL, "Unable to initialize ZW3 zone decryption");
		}
	}

	bool FastFiles::IsZombieZoneName(const std::string_view zoneName)
	{
		return Utils::String::ToLower(std::string(zoneName)).find("zombie") != std::string::npos;
	}

	bool FastFiles::ShouldProtectZone(const std::string_view zoneName)
	{
		return IsPrivateZW3Module() && IsZombieZoneName(zoneName);
	}

	void FastFiles::ProtectZoneBuffer(std::string& buffer)
	{
		std::array<unsigned char, zw3NonceSize> nonce{};

		for (std::size_t offset = 0; offset < nonce.size(); offset += sizeof(std::uint32_t))
		{
			const auto value = Utils::Cryptography::Rand::GenerateInt();
			std::memcpy(nonce.data() + offset, &value, sizeof(value));
		}

		Utils::AesCtr ctr;

		if (!ctr.Start(zw3ZoneKey.data(), zw3ZoneKey.size(), nonce.data()))
		{
			Game::Com_Error(Game::ERR_FATAL, "Unable to initialize ZW3 zone encryption");
		}

		const bool isEncrypted = ctr.Crypt(reinterpret_cast<std::uint8_t*>(buffer.data()), buffer.size());
		ctr.Reset();

		if (!isEncrypted)
		{
			Game::Com_Error(Game::ERR_FATAL, "Unable to encrypt ZW3 zone data");
		}

		buffer.insert(0, reinterpret_cast<const char*>(nonce.data()), nonce.size());
	}

	static Utils::Hook headerReadHook;
	static Utils::Hook inflateInitHook;
	static Utils::Hook inflateHook;

	static void ReadHeaderStub(std::uint32_t* header, const int size)
	{
		isIW4xZone = false;
		isZW3Zone = false;
		ResetZW3Ctr();
		lastByteRead = 0;

		iw4xVersion = 0;
		ResetIW4xCtr();

		reinterpret_cast<void(*)(void*, int)>(Utils::Hook::Rebase(DB_ReadXFileUncompressed))(header, size);

		if (header[0] == XFILE_HEADER_ZW3)
		{
			if (!IsPrivateZW3Module())
			{
				Game::Com_Error(Game::ERR_FATAL, "Protected zombie fastfiles require zw3.dll");
			}

			isZW3Zone = true;

			if (header[1] != XFILE_VERSION_ZW3)
			{
				Game::Com_Error(Game::ERR_FATAL, "Unsupported ZW3 fastfile version %u (expected %u)", header[1], XFILE_VERSION_ZW3);
			}

			*reinterpret_cast<std::uint64_t*>(header) = XFILE_MAGIC_UNSIGNED;
			return;
		}

		if (header[0] != XFILE_HEADER_IW4X)
		{
			return;
		}

		isIW4xZone = true;

		if (header[1] < XFILE_VERSION_IW4X)
		{
			Game::Com_Error(Game::ERR_DROP, "The fastfile you are trying to load is outdated (%u, expected %u)", header[1], XFILE_VERSION_IW4X);
		}
		else if (header[1] > XFILE_VERSION_IW4X)
		{
			Game::Com_Error(Game::ERR_DROP, "You are loading a fastfile that is too new (%u, expected %u), update your game or rebuild the fastfile", header[1], XFILE_VERSION_IW4X);
		}

		*reinterpret_cast<std::uint64_t*>(header) = XFILE_MAGIC_UNSIGNED;
	}

	static void InflateInitStub(const int isSecure)
	{
		if (iw4xVersion >= iw4xEncryptedVersion)
		{
			std::array<std::uint8_t, iw4xKeyBlobSize> blob{};
			reinterpret_cast<void(*)(void*, int)>(Utils::Hook::Rebase(DB_ReadXFileUncompressed))(blob.data(), static_cast<int>(blob.size()));

			if (!TryStartIW4xCtr(blob.data()))
			{
				Game::Com_Error(Game::ERR_DROP, "Unable to read the key of IW4x zone %s", FastFiles::Current().data());
			}
		}

		reinterpret_cast<void(*)(int)>(Utils::Hook::Rebase(DB_InflateInit))(isSecure);

		if (isZW3Zone)
		{
			std::array<unsigned char, zw3NonceSize> nonce{};
			reinterpret_cast<void(*)(void*, int, int)>(Utils::Hook::Rebase(DB_ReadXFile))(nonce.data(), static_cast<int>(nonce.size()), 0);
			InitZW3Ctr(nonce.data());
			return;
		}

		if (!isIW4xZone)
		{
			return;
		}

		std::uint8_t pad;
		reinterpret_cast<void(*)(void*, int, int)>(Utils::Hook::Rebase(DB_ReadXFile))(&pad, 1, 0);
	}

	static int InflateStub(z_stream* stream, const int flush)
	{
		auto* const start = stream->next_out;
		const auto result = reinterpret_cast<int(*)(z_stream*, int)>(Utils::Hook::Rebase(DB_AuthLoad_Inflate))(stream, flush);

		if (isZW3Zone && zw3Ctr.IsReady())
		{
			const auto length = static_cast<std::size_t>(stream->next_out - start);

			if (length && !zw3Ctr.Crypt(start, length))
			{
				Game::Com_Error(Game::ERR_FATAL, "Unable to decrypt ZW3 zone data");
			}

			return result;
		}

		if (!isIW4xZone)
		{
			return result;
		}

		Utils::DecodeIW4xZone(start, static_cast<std::size_t>(stream->next_out - start), lastByteRead);

		return result;
	}

	constexpr std::uintptr_t g_totalSize = 0x140D36710;
	constexpr std::uintptr_t g_loadedSize = 0x140D36780;

	static Dvar::Var ui_zoneDebug;

	static void DrawZoneDebug()
	{
		if (!ui_zoneDebug.IsValid() || !ui_zoneDebug.Get<bool>())
		{
			return;
		}

		const auto zone = FastFiles::Current();

		if (zone.empty())
		{
			return;
		}

		auto* const font = Game::R_RegisterFont("fonts/consoleFont", 0);

		float alpha = 1.0f;

		if (Game::CL_IsCgameInitialized(0))
		{
			alpha = 0.3f;
		}

		const float color[4] = { 1.0f, 1.0f, 1.0f, alpha };

		const auto totalSize = Utils::Hook::Get<std::uint32_t>(g_totalSize);
		const auto loadedSize = Utils::Hook::Get<std::uint32_t>(g_loadedSize);

		float progress = (static_cast<float>(loadedSize) / static_cast<float>(totalSize)) * 100.0f;

		if (std::isinf(progress))
		{
			progress = 100.0f;
		}
		else if (std::isnan(progress))
		{
			progress = 0.0f;
		}

		Game::R_AddCmdDrawText(Utils::String::VA("Loading FastFile: %s [%0.1f%%]", zone.data(), progress), std::numeric_limits<int>::max(),
			font, 5.0f, static_cast<float>(Renderer::Height() - 5), 1.0f, 1.0f, 0.0f, color, Game::ITEM_TEXTSTYLE_NORMAL);
	}

	float FastFiles::GetFullLoadedFraction()
	{
		const auto fraction = std::clamp(reinterpret_cast<float(*)()>(Utils::Hook::Rebase(DB_GetLoadedFraction))(), 0.0f, 1.0f);
		const int zoneCount = maxZones;

		if (zoneCount <= 0)
		{
			return fraction;
		}

		const float singleProgress = 1.0f / static_cast<float>(zoneCount);
		const float partialProgress = singleProgress * static_cast<float>(std::max(currentZone - 1, 0));

		return std::min(partialProgress + fraction * singleProgress, 1.0f);
	}

	static void Load_XSurfaceArray_Hk(const bool atStreamStart, [[maybe_unused]] const int count)
	{
		const auto* const surfs = *reinterpret_cast<const std::uint8_t* const*>(Utils::Hook::Rebase(varXModelSurfs));
		const auto numsurfs = *reinterpret_cast<const std::uint16_t*>(surfs + xmodelSurfsNumSurfs);

		reinterpret_cast<void(*)(bool, int)>(Utils::Hook::Rebase(Load_XSurfaceArray))(atStreamStart, numsurfs);
	}

	constexpr std::uintptr_t Image_LoadFromFileWithReader_SizeTest = 0x14006A2D0;
	constexpr std::uintptr_t Image_LoadFromFileWithReader_SizeJz = 0x14006A2D3;
	static const std::uint8_t sizeTest[] = { 0x39, 0x75, 0xF7, 0x74, 0x0E };

	constexpr std::uintptr_t Sys_WaitFileRead_TimeoutCall = 0x14020B205;
	static const std::uint8_t waitFileReadTimeout[] = { 0xE8, 0x16, 0xD4, 0x09, 0x00 };
	static const std::uint8_t waitFileReadEpilogue[] = { 0x48, 0x83, 0xC4, 0x28, 0xC3 };
	constexpr std::uintptr_t Sys_WaitFileRead_Epilogue = 0x14020B23B;

	constexpr std::uintptr_t Sys_Sleep = 0x14020AB90;
	constexpr std::uintptr_t recoveryPollSleeps[] =
	{
		0x14012FC00, 0x140130230, 0x1401302F5, 0x1401303B0, 0x140130470, 0x140130530, 0x140130620, 0x1401306E0,
		0x1401307A0, 0x140130860, 0x140130930, 0x1401309F0, 0x140130AB0, 0x140130B70, 0x140130C30, 0x140130CF0,
		0x140130DB0, 0x140130E70, 0x140130F30, 0x140131010, 0x1401310D0, 0x140131190, 0x140131290, 0x140131350,
		0x140131410, 0x1401314D0, 0x140131590, 0x140131650, 0x140131710, 0x1401317D0, 0x140131890, 0x140131950,
		0x140131A10, 0x140131AD0, 0x140131BA0, 0x140131C60, 0x140131D50,
	};
	static const std::uint8_t sleep25[] = { 0xB9, 0x19, 0x00, 0x00, 0x00 };

	constexpr std::uintptr_t Sys_SpawnDatabaseThread_Priority = 0x14020ACA1;
	constexpr std::uintptr_t databaseThread = 0x141DF31C0;
	static const std::uint8_t belowNormalPriority[] = { 0xBA, 0xFF, 0xFF, 0xFF, 0xFF };

	constexpr std::uintptr_t zoneOpenFlags[] = { 0x14028DEE7, 0x14028DF30 };
	static const std::uint8_t unbufferedFlags[] = { 0xC7, 0x44, 0x24, 0x28, 0x00, 0x00, 0x00, 0x60 };
	constexpr std::uint32_t sequentialFlags = 0x48000000;

	static void ApplyLoadSpeedPatches()
	{
		if (Utils::Hook::MatchesBytes(Sys_WaitFileRead_TimeoutCall, waitFileReadTimeout, sizeof(waitFileReadTimeout))
			&& Utils::Hook::MatchesBytes(Sys_WaitFileRead_Epilogue, waitFileReadEpilogue, sizeof(waitFileReadEpilogue)))
		{
			for (std::size_t i = 0; i < sizeof(waitFileReadEpilogue); ++i)
			{
				Utils::Hook::Set<std::uint8_t>(Sys_WaitFileRead_TimeoutCall + i, waitFileReadEpilogue[i]);
			}
		}
		else
		{
			Logger::Error("fastfiles: Sys_WaitFileRead does not read as expected, zone reads keep their pacing\n");
		}

		bool arePollsExpected = true;

		for (const std::uintptr_t site : recoveryPollSleeps)
		{
			arePollsExpected = arePollsExpected && Utils::Hook::MatchesBytes(site, sleep25, sizeof(sleep25))
				&& Utils::Hook::BranchesTo(site + sizeof(sleep25), Sys_Sleep, HOOK_CALL);
		}

		if (arePollsExpected)
		{
			for (const std::uintptr_t site : recoveryPollSleeps)
			{
				Utils::Hook::Set<std::uint32_t>(site + 1, 1);
			}
		}
		else
		{
			Logger::Error("fastfiles: the lost device polls do not read as expected, they stay at 25 ms\n");
		}

		if (Utils::Hook::MatchesBytes(Sys_SpawnDatabaseThread_Priority, belowNormalPriority, sizeof(belowNormalPriority)))
		{
			Utils::Hook::Set<std::int32_t>(Sys_SpawnDatabaseThread_Priority + 1, THREAD_PRIORITY_HIGHEST);
		}
		else
		{
			Logger::Error("fastfiles: Sys_SpawnDatabaseThread does not read as expected, the database thread starts below normal\n");
		}

		for (const std::uintptr_t site : zoneOpenFlags)
		{
			if (Utils::Hook::MatchesBytes(site, unbufferedFlags, sizeof(unbufferedFlags)))
			{
				Utils::Hook::Set<std::uint32_t>(site + 4, sequentialFlags);
			}
			else
			{
				Logger::Error("fastfiles: the zone open at 0x{:X} does not read as expected, it stays unbuffered\n", site);
			}
		}
	}

	constexpr std::uintptr_t inflateInit2_Entry = 0x1402C9090;
	constexpr std::uintptr_t inflateEntry = 0x1402C8BE0;
	constexpr std::uintptr_t inflateEndEntry = 0x1402C9030;
	constexpr std::uintptr_t memFileStream = 0x146705B00;

	static const std::uint8_t inflateInit2_Bytes[] = { 0x48, 0x89, 0x5C, 0x24, 0x18, 0x57, 0x48, 0x83, 0xEC, 0x20 };
	static const std::uint8_t inflateBytes[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x6C, 0x24, 0x10 };
	static const std::uint8_t inflateEndBytes[] = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B, 0xD9 };

	static Utils::Hook inflateInitRedirect;
	static Utils::Hook inflateRedirect;
	static Utils::Hook inflateEndRedirect;

	static Utils::InflateReadAhead memFileReadAhead;

	static bool IsMemFileStream(const z_streamp stream)
	{
		return reinterpret_cast<std::uintptr_t>(stream) == Utils::Hook::Rebase(memFileStream);
	}

	static int ModernInflateInit2(z_streamp stream, int windowBits, [[maybe_unused]] const char* version, [[maybe_unused]] int streamSize)
	{
		if (IsMemFileStream(stream))
		{
			memFileReadAhead.Reset();
		}

		return ::inflateInit2_(stream, windowBits, ZLIB_VERSION, static_cast<int>(sizeof(z_stream)));
	}

	static int ModernInflate(z_streamp stream, int flush)
	{
		if (IsMemFileStream(stream))
		{
			return memFileReadAhead.Read(stream, flush);
		}

		return ::inflate(stream, flush);
	}

	static int ModernInflateEnd(z_streamp stream)
	{
		if (IsMemFileStream(stream))
		{
			memFileReadAhead.Reset();
		}

		return ::inflateEnd(stream);
	}

	static void RedirectInflate()
	{
		const bool isExpected = Utils::Hook::MatchesBytes(inflateInit2_Entry, inflateInit2_Bytes, sizeof(inflateInit2_Bytes))
			&& Utils::Hook::MatchesBytes(inflateEntry, inflateBytes, sizeof(inflateBytes))
			&& Utils::Hook::MatchesBytes(inflateEndEntry, inflateEndBytes, sizeof(inflateEndBytes));

		if (!isExpected)
		{
			Logger::Error("fastfiles: the engine's zlib does not read as expected, it keeps 1.1.4\n");
			return;
		}

		bool isSeated = inflateInitRedirect.Initialize(inflateInit2_Entry, reinterpret_cast<void*>(ModernInflateInit2), HOOK_JUMP)->Install()->IsInstalled();
		isSeated = inflateRedirect.Initialize(inflateEntry, reinterpret_cast<void*>(ModernInflate), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = inflateEndRedirect.Initialize(inflateEndEntry, reinterpret_cast<void*>(ModernInflateEnd), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			inflateInitRedirect.Uninstall();
			inflateRedirect.Uninstall();
			inflateEndRedirect.Uninstall();

			Logger::Error("fastfiles: could not redirect the engine's zlib, it keeps 1.1.4\n");
			return;
		}

		inflateInitRedirect.Quick();
		inflateRedirect.Quick();
		inflateEndRedirect.Quick();
	}

	static void RaiseDatabasePriority()
	{
		const auto thread = Utils::Hook::Get<HANDLE>(databaseThread);

		if (thread && thread != INVALID_HANDLE_VALUE)
		{
			SetThreadPriority(thread, THREAD_PRIORITY_HIGHEST);
		}
	}

	static std::string InstallPath()
	{
		return reinterpret_cast<const char*(*)()>(Utils::Hook::Rebase(Sys_DefaultInstallPath))();
	}

	constexpr DWORD prefetchBufferSize = 4u * 1024u * 1024u;

	static std::mutex prefetchMutex;
	static std::condition_variable_any prefetchCondition;
	static std::deque<std::filesystem::path> prefetchQueue;
	static std::unordered_set<std::string> scheduledPrefetches;
	static std::jthread prefetchThread;

	static void RunPrefetch(const std::stop_token stopToken)
	{
		SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

		std::vector<std::byte> buffer;

		while (!stopToken.stop_requested())
		{
			std::filesystem::path path;

			{
				std::unique_lock lock(prefetchMutex);

				if (prefetchQueue.empty())
				{
					std::vector<std::byte>().swap(buffer);
				}

				const bool hasWork = prefetchCondition.wait(lock, stopToken, []
				{
					return !prefetchQueue.empty();
				});

				if (!hasWork)
				{
					break;
				}

				path = std::move(prefetchQueue.front());
				prefetchQueue.pop_front();
			}

			const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				continue;
			}

			if (buffer.empty())
			{
				buffer.resize(prefetchBufferSize);
			}

			DWORD bytesRead = 0;

			while (!stopToken.stop_requested()
				&& ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr) && bytesRead > 0)
			{
			}

			CloseHandle(file);
		}
	}

	static void EnqueuePrefetch(const std::filesystem::path& path)
	{
		std::error_code error;

		if (!std::filesystem::is_regular_file(path, error) || error)
		{
			return;
		}

		const auto identity = Utils::String::ToLower(path.string());

		{
			std::lock_guard lock(prefetchMutex);

			if (!scheduledPrefetches.emplace(identity).second)
			{
				return;
			}

			prefetchQueue.push_back(path);

			if (!prefetchThread.joinable())
			{
				prefetchThread = std::jthread(RunPrefetch);
			}
		}

		prefetchCondition.notify_one();
	}

	static void StopPrefetch()
	{
		if (prefetchThread.joinable())
		{
			prefetchThread.request_stop();
			prefetchCondition.notify_all();
			prefetchThread.join();
		}

		std::lock_guard lock(prefetchMutex);
		prefetchQueue.clear();
		scheduledPrefetches.clear();
	}

	bool FastFiles::HasZW3CommonZone()
	{
		const auto path = std::filesystem::path(InstallPath()) / "zw3" / "zw3_common.ff";
		std::error_code error;

		if (!std::filesystem::is_regular_file(path, error) || error)
		{
			return false;
		}

		const auto size = std::filesystem::file_size(path, error);

		return !error && size > zw3NonceSize;
	}

	[[noreturn]] static void ExitWithZW3Error(const std::string& message)
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled())
		{
			Logger::Error("{}\n", message);
			TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
		}
		else
		{
			MessageBoxA(nullptr, message.data(), "Error", MB_OK | MB_ICONERROR);
		}

		std::exit(EXIT_FAILURE);
	}

	void FastFiles::AddZonePath(const std::string& path)
	{
		zonePaths.push_back(path);
	}

	static int NestedZonePathRank(const std::string& path)
	{
		int rank = 0;

		if (path.find("\\zw3\\") == std::string::npos)
		{
			rank += 2;
		}

		if (path.find("\\x64\\") != std::string::npos)
		{
			rank += 1;
		}

		return rank;
	}

	static std::vector<std::string> FindNestedZonePaths(const std::string& installPath, const std::vector<std::string>& listed)
	{
		std::vector<std::string> found;
		std::error_code error;
		const std::filesystem::path zoneRoot = std::filesystem::path(installPath) / "zone";

		for (std::filesystem::recursive_directory_iterator entry(zoneRoot, error), end; !error && entry != end; entry.increment(error))
		{
			if (entry.depth() < 1 || !entry->is_directory(error) || entry->path().filename() == "zonebuilder")
			{
				continue;
			}

			bool hasZone = false;

			for (std::filesystem::directory_iterator file(entry->path(), error), last; !error && file != last; file.increment(error))
			{
				if (file->path().extension() == ".ff")
				{
					hasZone = true;
					break;
				}
			}

			error.clear();

			if (!hasZone)
			{
				continue;
			}

			const std::string relative = std::filesystem::relative(entry->path(), installPath, error).string() + "\\";

			if (error || std::ranges::find(listed, relative) != listed.end())
			{
				error.clear();
				continue;
			}

			found.push_back(relative);
		}

		std::ranges::sort(found, [](const std::string& a, const std::string& b)
		{
			return std::make_tuple(NestedZonePathRank(a), a) < std::make_tuple(NestedZonePathRank(b), b);
		});

		return found;
	}

	const char* FastFiles::GetZoneLocation(const char* file)
	{
		const std::string installPath = InstallPath();
		const std::string_view zoneName = file;

		if (zoneName == "mod" || zoneName == "mod.ff")
		{
			const std::string fsGame = (*Game::fs_gameDirVar)->current.string;

			if (!fsGame.empty() && Utils::IO::FileExists(std::format("{}\\{}\\mod.ff", installPath, fsGame)))
			{
				return Utils::String::Format("{}\\", fsGame);
			}
		}

		if (file && *file)
		{
			std::string zone = file;

			if (Utils::String::EndsWith(zone, ".ff"))
			{
				Utils::String::Replace(zone, ".ff", "");
			}

			const std::string filename = zone;

			if (Utils::String::EndsWith(zone, "_load"))
			{
				Utils::String::Replace(zone, "_load", "");
			}

			if (Utils::IO::FileExists(std::format("{}\\usermaps\\{}\\{}.ff", installPath, zone, filename)))
			{
				return Utils::String::Format("usermaps\\{}\\", zone);
			}
		}

		static const std::vector<std::string> nestedZonePaths = FindNestedZonePaths(installPath, zonePaths);
		const std::vector<std::string>* const pathLists[] = { &zonePaths, &nestedZonePaths };

		for (const auto* paths : pathLists)
		{
			for (const std::string& path : *paths)
			{
				std::string absoluteFile = std::format("{}\\{}{}", installPath, path, file);

				if (!absoluteFile.ends_with(".ff"))
				{
					absoluteFile.append(".ff");
				}

				if (Utils::IO::FileExists(absoluteFile))
				{
					return path.data();
				}
			}
		}

		return nullptr;
	}

	const char* FastFiles::Sys_GetMapZoneDir_Stub(const char* zoneName)
	{
		const char* const location = GetZoneLocation(zoneName);

		if (location)
		{
			return location;
		}

		return reinterpret_cast<const char*(*)(const char*)>(Utils::Hook::Rebase(Sys_GetMapZoneDir))(zoneName);
	}

	bool FastFiles::Exists(const std::string& file)
	{
		std::string zoneName = file;

		if (zoneName.ends_with(".ff"))
		{
			zoneName.resize(zoneName.size() - 3);
		}

		const char* const directory = Sys_GetMapZoneDir_Stub(zoneName.data());

		return Utils::IO::FileExists(std::format("{}\\{}{}.ff", InstallPath(), directory, zoneName));
	}

	void FastFiles::LoadInitialZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync)
	{
		g_loadingInitialZones.Set(true);
		isMainMenuReached.store(false, std::memory_order_release);
		isInitialLoadPending.store(true, std::memory_order_release);

		std::vector<Game::XZoneInfo> zones(zoneInfo, zoneInfo + zoneCount);

		const bool isDev = Flags::HasFlag("dev");

		if (HasZW3CommonZone())
		{
			for (auto& zone : zones)
			{
				if (zone.name && !std::strcmp(zone.name, "common_mp"))
				{
					zone.name = "zw3_common";
				}
			}
		}
		else if (isDev)
		{
			Logger::Print("zw3_common.ff is unavailable; using common_mp.ff\n");
		}
		else
		{
			ExitWithZW3Error(std::format("Missing 'zw3_common.ff':\n{}\\zw3\\zw3_common.ff\n\nPlease run the Zombie Warfare 3 Launcher to verify game files or place it inside the zw3 folder.", InstallPath()));
		}

		const bool isServerOrBuilder = Dedicated::IsEnabled() || ZoneBuilder::IsEnabled();
		const auto lobbyPath = std::format("{}\\zw3\\zw3_lobby.ff", InstallPath());
		const bool hasLobby = Utils::IO::FileExists(lobbyPath);

		if (!isServerOrBuilder && !hasLobby && !isDev)
		{
			ExitWithZW3Error(std::format("Missing 'zw3_lobby.ff':\n{}\n\nPlease run the Zombie Warfare 3 Launcher to verify game files or place it inside the zw3 folder.", lobbyPath));
		}

		if (!isServerOrBuilder && hasLobby)
		{
			const auto common = std::ranges::find_if(zones, [](const Game::XZoneInfo& zone)
			{
				return zone.name && (!std::strcmp(zone.name, "zw3_common") || !std::strcmp(zone.name, "common_mp"));
			});

			const Game::XZoneInfo lobby = { "zw3_lobby", 1, 0 };

			if (common != zones.end())
			{
				zones.insert(std::next(common), lobby);
			}
			else
			{
				zones.push_back(lobby);
			}
		}

		if (Exists("iw4x_patch_mp"))
		{
			zones.push_back({ "iw4x_patch_mp", 1, 0 });
		}

		const bool hasMod = Zones::IsReady() && Exists("mod");
		const bool shouldLoadModSeparately = isDev && hasMod;

		if (hasMod && !shouldLoadModSeparately)
		{
			zones.push_back({ "mod", 1, 0 });
		}

		if (isDev)
		{
			Logger::Print("Skipping zw3/zw3.ff because -dev is enabled.\n");
		}
		else
		{
			const std::string zw3Zone = std::format("{}\\zw3\\zw3.ff", InstallPath());

			if (!Utils::IO::FileExists(zw3Zone))
			{
				ExitWithZW3Error(std::format("Missing 'zw3.ff':\n{}\n\nPlease run the Zombie Warfare 3 Launcher to verify game files or place it inside the zw3 folder.", zw3Zone));
			}

			if (!Zones::IsReady())
			{
				ExitWithZW3Error(std::format("'zw3.ff' cannot be read:\n{}\n\nIt is a 32 bit zone and this build's 32 bit zone reader did not start. See iw4x\\iw4x.log.", zw3Zone));
			}

			zones.push_back({ "zw3", 1, 0 });
		}

		LoadDLCUIZones(zones.data(), static_cast<unsigned int>(zones.size()), sync);

		if (shouldLoadModSeparately)
		{
			Game::XZoneInfo modZone{ "mod", 1, 0 };
			Game::DB_LoadXAssets(&modZone, 1, true);
		}
	}

	void FastFiles::LoadDLCUIZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync)
	{
		std::vector<Game::XZoneInfo> zones;

		if (Exists("iw4x_ui_mp"))
		{
			for (unsigned int i = 0; i < zoneCount; ++i)
			{
				const char* const name = zoneInfo[i].name;
				bool isDlcUIZone = false;

				for (const char* const dlcZone : dlcUIZones)
				{
					if (name && !std::strcmp(name, dlcZone))
					{
						isDlcUIZone = true;
					}
				}

				if (!isDlcUIZone)
				{
					zones.push_back(zoneInfo[i]);
				}
			}

			if (!Game::DB_IsZoneLoaded("iw4x_ui_mp"))
			{
				zones.push_back({ "iw4x_ui_mp", 2, 0 });
			}
		}
		else
		{
			zones.assign(zoneInfo, zoneInfo + zoneCount);
		}

		LoadLocalizeZones(zones.data(), static_cast<unsigned int>(zones.size()), sync);
	}

	void FastFiles::LoadGfxZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync)
	{
		std::vector<Game::XZoneInfo> zones(zoneInfo, zoneInfo + zoneCount);

		if (Exists("iw4x_code_post_gfx_mp"))
		{
			zones.push_back({ "iw4x_code_post_gfx_mp", zoneInfo->allocFlags, zoneInfo->freeFlags });
		}

		for (const auto& zone : zones)
		{
			if (zone.name)
			{
				PrefetchZone(zone.name);
			}
		}

		RaiseDatabasePriority();

		Game::DB_LoadXAssets(zones.data(), static_cast<unsigned int>(zones.size()), sync);
	}

	void FastFiles::LoadLocalizeZones(Game::XZoneInfo* zoneInfo, unsigned int zoneCount, int sync)
	{
		std::vector<Game::XZoneInfo> data(zoneInfo, zoneInfo + zoneCount);

		Game::XZoneInfo info = { nullptr, 4, 0 };

		const std::string langZone = std::format("iw4x_localized_{}", Game::Win_GetLanguage());

		if (Exists(langZone))
		{
			info.name = langZone.data();
		}
		else if (Exists("iw4x_localized_english"))
		{
			info.name = "iw4x_localized_english";
		}

		if (info.name && !Game::DB_IsZoneLoaded(info.name))
		{
			data.push_back(info);
		}

		if (g_loadingInitialZones.Get<bool>())
		{
			std::stable_partition(data.begin(), data.end(), [](const Game::XZoneInfo& zone)
			{
				if (!zone.name)
				{
					return false;
				}

				const std::string_view name = zone.name;

				return name != "zw3_common" && name != "common_mp" && name != "zw3_lobby";
			});
		}

		for (const auto& zone : data)
		{
			if (zone.name)
			{
				PrefetchZone(zone.name);
			}
		}

		RaiseDatabasePriority();

		Game::DB_LoadXAssets(data.data(), static_cast<unsigned int>(data.size()), sync);

		if (!Dedicated::IsEnabled() && !ZoneBuilder::IsEnabled())
		{
			LobbyScene::PrepareStartup();
		}

		Scheduler::OnGameInitialized([]
		{
			g_loadingInitialZones.Set(false);
			LobbyScene::PrepareStartup();
		}, Scheduler::Pipeline::MAIN);
	}

	void FastFiles::PrefetchPath(const std::filesystem::path& path)
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		EnqueuePrefetch(path);
	}

	void FastFiles::PrefetchZone(const std::string& zoneName)
	{
		if (zoneName.empty() || Dedicated::IsEnabled())
		{
			return;
		}

		std::string name = zoneName;

		if (name.ends_with(".ff"))
		{
			name.resize(name.size() - 3);
		}

		std::string directory;

		if (const char* const location = GetZoneLocation(name.data()))
		{
			directory = location;
		}
		else if (const char* const language = Game::Win_GetLanguage())
		{
			directory = std::format("zone\\{}\\", language);
		}
		else
		{
			return;
		}

		EnqueuePrefetch(std::filesystem::path(InstallPath()) / directory / (name + ".ff"));
	}

	void FastFiles::MarkMainMenuReady()
	{
		if (!isInitialLoadPending.load(std::memory_order_acquire) || !Ready())
		{
			return;
		}

		if (!isInitialLoadPending.exchange(false, std::memory_order_acq_rel))
		{
			return;
		}

		isMainMenuReached.store(true, std::memory_order_release);
		Renderer::FinishLoading();
	}

	bool FastFiles::MainMenuReady()
	{
		return isMainMenuReached.load(std::memory_order_acquire);
	}

	bool FastFiles::Ready()
	{
		return Game::Sys_IsDatabaseReady() && Game::Sys_IsDatabaseReady2();
	}

	std::string FastFiles::Current()
	{
		const auto file = Utils::Hook::Get<std::uintptr_t>(g_load);

		if (!file)
		{
			return "";
		}

		return reinterpret_cast<const char*>(file + dbFileName);
	}

	struct ValidateHashCall
	{
		std::uintptr_t address;
		std::array<std::uint8_t, 9> bytes;
	};

	static const ValidateHashCall validateHashCalls[] =
	{
		{ 0x140117757, { 0xE8, 0x94, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x74, 0x50 } },
		{ 0x1401177A7, { 0xE8, 0x44, 0x00, 0x00, 0x00, 0x84, 0xC0, 0x75, 0x08 } },
	};

	static const std::uint8_t hashPassed[] = { 0xB0, 0x01, 0x90, 0x90, 0x90 };

	static void SkipBlockHashes()
	{
		const bool areCallsIntact = std::ranges::all_of(validateHashCalls, [](const ValidateHashCall& call)
		{
			return Utils::Hook::MatchesBytes(call.address, call.bytes.data(), call.bytes.size());
		});

		if (!areCallsIntact)
		{
			Logger::Error("fastfiles: DB_AuthLoad_Inflate's hash calls do not read as expected, signed zones are still hashed\n");
			return;
		}

		for (const auto& call : validateHashCalls)
		{
			for (std::size_t i = 0; i < sizeof(hashPassed); ++i)
			{
				Utils::Hook::Set<std::uint8_t>(call.address + i, hashPassed[i]);
			}
		}
	}

	constexpr std::uintptr_t g_zoneInfo = 0x1415FCB70;
	constexpr std::size_t zoneInfoSize = 68;
	constexpr std::size_t zoneInfoFlags = 0x40;
	constexpr std::size_t zoneInfoCapacity = 32;

	static const Utils::Hook::LeaSite zoneInfoLeas[] =
	{
		{ 0x14012F0C1, { 0x4C, 0x8D, 0x3D }, g_zoneInfo },
		{ 0x14012FA02, { 0x4C, 0x8D, 0x35 }, g_zoneInfo + zoneInfoFlags },
		{ 0x14012FA09, { 0x4C, 0x8D, 0x3D }, g_zoneInfo },
	};

	static void RaiseZoneRequests()
	{
		const bool isExpected = std::ranges::all_of(zoneInfoLeas, [](const Utils::Hook::LeaSite& lea)
		{
			return Utils::Hook::IsLeaIntact(lea);
		});

		if (!isExpected)
		{
			Logger::Error("fastfiles: g_zoneInfo's leas do not read as expected, a zone batch still holds at most 8\n");
			return;
		}

		auto* const block = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(zoneInfoLeas[0].address, zoneInfoCapacity * zoneInfoSize));

		if (!block)
		{
			Logger::Error("fastfiles: no room near the image for g_zoneInfo, a zone batch still holds at most 8\n");
			return;
		}

		const bool canReach = std::ranges::all_of(zoneInfoLeas, [block](const Utils::Hook::LeaSite& lea)
		{
			return Utils::Hook::CanLeaReach(lea, block + (lea.target - g_zoneInfo));
		});

		if (!canReach)
		{
			Logger::Error("fastfiles: g_zoneInfo's block is out of reach, a zone batch still holds at most 8\n");
			return;
		}

		for (const auto& lea : zoneInfoLeas)
		{
			Utils::Hook::PointLeaAt(lea, block + (lea.target - g_zoneInfo));
		}
	}

	FastFiles::FastFiles()
	{
		ApplyLoadSpeedPatches();
		SkipBlockHashes();
		RedirectInflate();
		SeatIW4xFormat();
		RaiseZoneRequests();

		if (ZoneConvert::IsEnabled())
		{
			AddZonePath(ZoneConvert::SearchPath("patch"));
			AddZonePath(ZoneConvert::SearchPath("dlc"));
		}
		else
		{
			AddZonePath("zone\\patch\\");
			AddZonePath("zone\\dlc\\");
		}

		AddZonePath("zw3\\");

		AddZonePath("zone\\x86\\patch\\");
		AddZonePath("zone\\x86\\dlc\\");

		Events::OnDvarInit([]
		{
			ui_zoneDebug = Dvar::Register("ui_zoneDebug", false, Game::DVAR_ARCHIVE, "Display current loaded zone.");
			g_loadingInitialZones = Dvar::Register("g_loadingInitialZones", true, Game::DVAR_NONE, "Is loading initial zones");
		});

		if (!Dedicated::IsEnabled())
		{
			Scheduler::Loop(DrawZoneDebug, Scheduler::Pipeline::RENDERER);

			Events::OnDvarInit([]
			{
				if (!ZoneBuilder::IsEnabled())
				{
					PrefetchZone("zw3_lobby");
				}

				PrefetchZone("zw3_common");

				if (!Flags::HasFlag("dev"))
				{
					PrefetchZone("zw3");
				}

				PrefetchZone("iw4x_patch_mp");
				PrefetchZone("iw4x_ui_mp");
				PrefetchZone("iw4x_localized_english");
				PrefetchZone("iw4x_code_post_gfx_mp");
			});

			Scheduler::OnShutdown(StopPrefetch);
		}

		Command::Add("loadzone", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const char* const zoneName = params->Get(1);

			if (!Exists(zoneName))
			{
				Logger::Error("fastfiles: zone '{}' does not exist\n", zoneName);
				return;
			}

			Game::XZoneInfo info{};
			info.name = zoneName;
			info.allocFlags = 1;
			info.freeFlags = 0;

			PrefetchZone(info.name);
			Game::DB_LoadXAssets(&info, 1, true);
		});

		Command::Add("listassetpool", [](const Command::Params* params)
		{
			unsigned int first = 0;
			unsigned int last = Game::ASSET_TYPE_COUNT - 1;

			if (params->Size() >= 2)
			{
				const Game::XAssetType type = Game::DB_GetXAssetNameType(params->Get(1));

				if (type >= Game::ASSET_TYPE_COUNT)
				{
					Logger::Error("listassetpool: invalid asset type '{}'\n", params->Get(1));
					return;
				}

				first = type;
				last = type;
			}

			for (unsigned int type = first; type <= last; ++type)
			{
				unsigned int count = 0;

				Game::DB_EnumXAssets_FastFile(static_cast<Game::XAssetType>(type), [](void*, void* data)
				{
					++*static_cast<unsigned int*>(data);
				}, &count, false);

				Logger::Print("{}: {} / {}\n", Game::DB_GetXAssetTypeName(type), count, Game::g_poolSize[type]);
			}
		});

		Command::Add("awaitDatabase", []
		{
			Logger::Print("Waiting for database...\n");

			while (!Game::Sys_IsDatabaseReady())
			{
				std::this_thread::yield();

				if (Game::Sys_IsDatabaseReady())
				{
					break;
				}

				std::this_thread::sleep_for(1ms);
			}
		});

		if (!Utils::Hook::BranchesTo(Load_XModelSurfs_SurfaceArrayCall, Load_XSurfaceArray, HOOK_CALL)
			|| !surfaceArrayHook.Initialize(Load_XModelSurfs_SurfaceArrayCall, reinterpret_cast<void*>(Load_XSurfaceArray_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("fastfiles: could not hook Load_XModelSurfs' surface array call, a standalone xmodelsurfs loads with a stale count\n");
		}
		else
		{
			surfaceArrayHook.Quick();
		}

		bool isProgressExpected = Utils::Hook::BranchesTo(DB_LoadXAssets_NotifyCall, Sys_NotifyDatabase, HOOK_CALL)
			&& Utils::Hook::BranchesTo(DB_LoadXFile_VersionReadCall, DB_ReadXFileUncompressed, HOOK_CALL);

		for (const auto call : DB_GetLoadedFractionCalls)
		{
			isProgressExpected = isProgressExpected && Utils::Hook::BranchesTo(call, DB_GetLoadedFraction, HOOK_CALL);
		}

		bool isProgressSeated = isProgressExpected;

		if (isProgressExpected)
		{
			isProgressSeated = notifyDatabaseHook.Initialize(DB_LoadXAssets_NotifyCall, reinterpret_cast<void*>(Sys_NotifyDatabase_Hk), HOOK_CALL)->Install()->IsInstalled();
			isProgressSeated = versionReadHook.Initialize(DB_LoadXFile_VersionReadCall, reinterpret_cast<void*>(DB_ReadXFileUncompressed_Hk), HOOK_CALL)->Install()->IsInstalled() && isProgressSeated;

			for (std::size_t i = 0; i < std::size(DB_GetLoadedFractionCalls); ++i)
			{
				isProgressSeated = loadedFractionHooks[i].Initialize(DB_GetLoadedFractionCalls[i], reinterpret_cast<void*>(GetFullLoadedFraction), HOOK_CALL)->Install()->IsInstalled() && isProgressSeated;
			}
		}

		if (!isProgressSeated)
		{
			notifyDatabaseHook.Uninstall();
			versionReadHook.Uninstall();

			for (auto& hook : loadedFractionHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("fastfiles: the load progress calls do not read as expected, the loading bar runs once per zone\n");
		}
		else
		{
			notifyDatabaseHook.Quick();
			versionReadHook.Quick();

			for (auto& hook : loadedFractionHooks)
			{
				hook.Quick();
			}
		}

		const bool isFormatExpected = Utils::Hook::BranchesTo(DB_LoadXFile_HeaderReadCall, DB_ReadXFileUncompressed, HOOK_CALL)
			&& Utils::Hook::BranchesTo(DB_LoadXFile_InflateInitCall, DB_InflateInit, HOOK_CALL)
			&& Utils::Hook::BranchesTo(DB_ReadXFile_InflateCall, DB_AuthLoad_Inflate, HOOK_CALL);

		bool isFormatSeated = isFormatExpected;

		if (isFormatExpected)
		{
			isFormatSeated = headerReadHook.Initialize(DB_LoadXFile_HeaderReadCall, reinterpret_cast<void*>(ReadHeaderStub), HOOK_CALL)->Install()->IsInstalled();
			isFormatSeated = inflateInitHook.Initialize(DB_LoadXFile_InflateInitCall, reinterpret_cast<void*>(InflateInitStub), HOOK_CALL)->Install()->IsInstalled() && isFormatSeated;
			isFormatSeated = inflateHook.Initialize(DB_ReadXFile_InflateCall, reinterpret_cast<void*>(InflateStub), HOOK_CALL)->Install()->IsInstalled() && isFormatSeated;
		}

		if (!isFormatSeated)
		{
			headerReadHook.Uninstall();
			inflateInitHook.Uninstall();
			inflateHook.Uninstall();

			Logger::Error("fastfiles: the zone header and inflate calls do not read as expected, IW4x format zones will not load\n");
		}
		else
		{
			headerReadHook.Quick();
			inflateInitHook.Quick();
			inflateHook.Quick();
		}

		for (const std::uintptr_t site : { R_LoadGraphicsAssets_LoadCall, R_LoadGraphicsAssets_GfxLoadCall, Com_AssetLoadUI_LoadCall })
		{
			if (!Utils::Hook::BranchesTo(site, DB_LoadXAssets, HOOK_CALL))
			{
				Logger::Error("fastfiles: 0x{:X} no longer calls DB_LoadXAssets, IW4x's zones will not load\n", site);
				return;
			}
		}

		for (const std::uintptr_t site : Sys_GetMapZoneDirCalls)
		{
			if (!Utils::Hook::BranchesTo(site, Sys_GetMapZoneDir, HOOK_CALL))
			{
				Logger::Error("fastfiles: 0x{:X} no longer calls Sys_GetMapZoneDir, IW4x's zones will not load\n", site);
				return;
			}
		}

		if (!Utils::Hook::MatchesBytes(DB_InflateInit_UnsignedJnz, unsignedJnz, sizeof(unsignedJnz)))
		{
			Logger::Error("fastfiles: DB_InflateInit does not read as expected, IW4x's zones will not load\n");
			return;
		}

		if (!Utils::Hook::MatchesBytes(Image_LoadFromFileWithReader_SizeTest, sizeTest, sizeof(sizeTest)))
		{
			Logger::Error("fastfiles: Image_LoadFromFileWithReader does not read as expected, IW4x's zones will not load\n");
			return;
		}

		bool isSeated = loadInitialZonesHook.Initialize(R_LoadGraphicsAssets_LoadCall, reinterpret_cast<void*>(LoadInitialZones), HOOK_CALL)
			->Install()->IsInstalled();
		isSeated = loadDLCUIZonesHook.Initialize(Com_AssetLoadUI_LoadCall, reinterpret_cast<void*>(LoadDLCUIZones), HOOK_CALL)
			->Install()->IsInstalled() && isSeated;
		isSeated = loadGfxZonesHook.Initialize(R_LoadGraphicsAssets_GfxLoadCall, reinterpret_cast<void*>(LoadGfxZones), HOOK_CALL)
			->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(Sys_GetMapZoneDirCalls); ++i)
		{
			isSeated = zoneDirHooks[i].Initialize(Sys_GetMapZoneDirCalls[i], reinterpret_cast<void*>(Sys_GetMapZoneDir_Stub), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			loadInitialZonesHook.Uninstall();
			loadDLCUIZonesHook.Uninstall();
			loadGfxZonesHook.Uninstall();

			for (auto& hook : zoneDirHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("fastfiles: could not seat every hook, IW4x's zones will not load\n");
			return;
		}

		loadInitialZonesHook.Quick();
		loadDLCUIZonesHook.Quick();
		loadGfxZonesHook.Quick();

		for (auto& hook : zoneDirHooks)
		{
			hook.Quick();
		}

		Utils::Hook::Nop(DB_InflateInit_UnsignedJnz, sizeof(unsignedJnz));
		Utils::Hook::Set<std::uint8_t>(Image_LoadFromFileWithReader_SizeJz, 0xEB);

		Scheduler::Loop([]
		{
			if ((!Party::IsInLobby() && !LobbyScene::IsSceneReady()) || !Game::Sys_IsDatabaseReady()
				|| Game::CL_IsCgameInitialized(0) || Game::CL_GetLocalClientConnectionState(0) >= Game::CA_CONNECTING) return;
			static std::string selectedMap;
			static std::string prefetchedMap;
			static std::chrono::steady_clock::time_point selectedAt;
			const auto map = Dvar::Var("ui_mapname").Get<std::string>();
			const auto now = std::chrono::steady_clock::now();
			if (map != selectedMap)
			{
				selectedMap = map;
				selectedAt = now;
			}
			if (map.empty() || map == prefetchedMap || now - selectedAt < 250ms) return;
			PrefetchZone(map + "_load");
			PrefetchZone(map);
			PrefetchZone("patch_" + map);
			PrefetchZone("localized_" + map);
			prefetchedMap = map;
		}, Scheduler::Pipeline::MAIN, 250ms);
	}
}
