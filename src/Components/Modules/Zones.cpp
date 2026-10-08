#include "STDInclude.hpp"

#include "Zones.hpp"
#include "ZonesLayouts.hpp"
#include "FastFiles.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "Maps.hpp"

namespace Components
{
	constexpr std::uintptr_t DB_Thread_LoadXFileCall = 0x14012FC6C;
	constexpr std::uintptr_t DB_LoadXFile = 0x140117D40;
	constexpr std::size_t dbFileName = 8;

	constexpr std::uintptr_t DB_LoadXFile_XFileRead = 0x140117E2B;
	constexpr std::uintptr_t DB_LoadXFile_AssetListRead = 0x140117EB5;
	constexpr std::uintptr_t DB_ReadXFile = 0x1401182D0;

	constexpr std::uintptr_t StringReadCalls[] = { 0x140132312, 0x14013232D, 0x14013239A, 0x1401323BD };

	constexpr std::uintptr_t Load_Stream = 0x1401322A0;
	constexpr std::uintptr_t DB_AllocStreamPos = 0x140131FD0;
	constexpr std::uintptr_t DB_IncStreamPos = 0x140131FF0;
	constexpr std::uintptr_t DB_PushStreamPos = 0x140132140;
	constexpr std::uintptr_t DB_PopStreamPos = 0x140132100;
	constexpr std::uintptr_t DB_InsertPointer = 0x140132080;
	constexpr std::uintptr_t DB_ConvertOffsetToAlias = 0x140132240;
	constexpr std::uintptr_t DB_ConvertOffsetToPointer = 0x140132270;
	constexpr std::uintptr_t DB_SetStreamIndex = 0x140132190;

	static const std::uint8_t load_StreamEntry[] = { 0x84, 0xC9, 0x74, 0x48, 0x53, 0x48, 0x83, 0xEC };
	static const std::uint8_t allocStreamPosEntry[] = { 0x48, 0x63, 0xC1, 0xF7, 0xD1, 0x48, 0x03, 0x05 };
	static const std::uint8_t incStreamPosEntry[] = { 0x48, 0x63, 0xC1, 0x48, 0x01, 0x05, 0x46, 0xB0, 0x4C, 0x01, 0xC3 };
	static const std::uint8_t pushStreamPosEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83 };
	static const std::uint8_t popStreamPosEntry[] = { 0x8B, 0x05, 0x42, 0xAF, 0x4C, 0x01, 0x48, 0x8D };
	static const std::uint8_t insertPointerEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74 };
	static const std::uint8_t offsetToAliasEntry[] = { 0x48, 0x8B, 0x05, 0xF1, 0xAD, 0x4C, 0x01, 0x4C };
	static const std::uint8_t offsetToPointerEntry[] = { 0x4C, 0x8B, 0x01, 0x48, 0x8B, 0x05, 0xBE, 0xAD };

	constexpr std::uintptr_t g_streamPosArray = 0x1415FCFF0;
	constexpr std::uintptr_t g_streamPosIndex = 0x1415FD030;
	constexpr std::uintptr_t g_streamZoneMem = 0x1415FD038;
	constexpr std::uintptr_t g_streamPos = 0x1415FD040;
	constexpr std::uintptr_t g_streamPosStackIndex = 0x1415FD048;
	constexpr std::uintptr_t g_streamPosStack = 0x1415FD050;

	constexpr std::uintptr_t Load_MssSound_SetSoundDataCall = 0x140123239;
	constexpr std::uintptr_t Load_SetSoundData = 0x1402C5BE0;
	constexpr std::uintptr_t Z_MallocInternal = 0x14027F800;

	constexpr std::uint64_t inlineMarker = 0xFFFFFFFFFFFFFFFF;
	constexpr std::uint64_t insertMarker = 0xFFFFFFFFFFFFFFFE;
	constexpr std::uint32_t inlineMarker32 = 0xFFFFFFFF;
	constexpr std::uint32_t insertMarker32 = 0xFFFFFFFE;
	constexpr std::uint16_t noField = 0xFFFF;
	constexpr std::uint32_t blockCount = 8;

	constexpr std::uint32_t planSlack = 0x100000;

	constexpr std::uint8_t imgCategoryLoadFromFile = 3;

	constexpr std::uintptr_t Load_GfxTextureLoad_BlockZero = 0x14011AD98;
	constexpr std::uintptr_t Load_GfxTextureLoad_PushCall = 0x14011AD9A;
	static const std::uint8_t xorEcxEcx[] = { 0x33, 0xC9 };

	constexpr std::uintptr_t Load_FxElemDefArray_ExtendedCall = 0x140119435;
	constexpr std::uintptr_t Load_FxElemExtendedDefPtr = 0x1401195D0;
	constexpr std::uintptr_t Load_XModel_SurfsFixupCall = 0x14012358B;
	constexpr std::uintptr_t Load_XModelSurfsFixup = 0x140131C80;
	constexpr std::uintptr_t Load_GameWorldSp_PathDataCall = 0x14011A3ED;
	constexpr std::uintptr_t Load_PathData = 0x14011E110;
	constexpr std::uintptr_t Load_GfxWorld_FlareMaterialCall = 0x14011B2C3;
	constexpr std::uintptr_t Load_MaterialHandle = 0x14011D120;
	constexpr std::uintptr_t varMaterialHandle = 0x140DB8370;

	constexpr std::uintptr_t Load_FxImpactTable_ReadSize = 0x140119CB2;
	constexpr std::uintptr_t Load_FxImpactTable_EntryCount = 0x140119CD8;
	static const std::uint8_t impactReadSizeStock[] = { 0x41, 0xB8, 0x68, 0x10, 0x00, 0x00 };
	static const std::uint8_t impactEntryCountStock[] = { 0xBF, 0x0F, 0x00, 0x00, 0x00 };
	constexpr std::uint32_t impactEntrySize64 = 280;
	constexpr std::uint32_t impactEntriesStock = 15;
	constexpr std::uint32_t impactEntriesIW4x = 16;

	constexpr std::uintptr_t Load_WeaponCompleteDefPtr_Calls[] = { 0x140120BE2, 0x140120C11 };
	constexpr std::uintptr_t Load_WeaponCompleteDef = 0x140120810;
	constexpr std::uintptr_t varWeaponCompleteDef = 0x140DB7730;
	constexpr std::uint32_t weaponCompleteDefSize64 = 0xA0;
	constexpr std::uintptr_t varXString = 0x140DB6E88;
	constexpr std::uintptr_t Load_XString = 0x140123A40;
	constexpr std::uintptr_t Load_XStringArray = 0x140123AB0;
	constexpr std::uintptr_t varXModelPtr = 0x140DB7850;
	constexpr std::uintptr_t Load_XModelPtr = 0x140123700;
	constexpr std::uintptr_t varFxEffectDefHandle = 0x140DB78B0;
	constexpr std::uintptr_t Load_FxEffectDefHandle = 0x140119000;
	constexpr std::uintptr_t varPhysCollmapPtr = 0x140DB72B8;
	constexpr std::uintptr_t Load_PhysCollmapPtr = 0x14011E4C0;
	constexpr std::uintptr_t varPhysPresetPtr = 0x140DB7840;
	constexpr std::uintptr_t Load_PhysPresetPtr = 0x14011E670;
	constexpr std::uintptr_t varTracerDefPtr = 0x140DB7638;
	constexpr std::uintptr_t Load_TracerDefPtr = 0x14011FAE0;
	constexpr std::uintptr_t varsnd_alias_list_name = 0x140DB7B08;
	constexpr std::uintptr_t Load_SndAliasCustom = 0x140280B30;
	constexpr std::uint32_t weaponSoundArrayCount = 31;

	constexpr std::uint32_t iw4xFirstVersion = 316;
	constexpr std::uint32_t iw4xImageBlockVersion = 332;
	constexpr std::uint32_t iw4xMaterialVersion = 359;
	constexpr std::uint32_t iw4xPathDataGoneVersion = 318;
	constexpr std::uint32_t iw4xSurfaceHeaderVersion = 332;
	constexpr std::uint32_t iw4xGameMapSpEndVersion = 423;
	constexpr std::uint32_t iw4xImpactFx16EndVersion = 446;
	constexpr std::uint32_t iw4xGameMapMpType = Game::ASSET_TYPE_GAMEWORLD_MP;
	constexpr std::uint32_t iw4xWeaponNextVersion = 365;
	constexpr std::uint32_t iw4xReadVersions[] = { 316, 319, 332, 359, 360 };

	constexpr std::uint32_t iw4xAssetTypes[] =
	{
		Game::ASSET_TYPE_XANIMPARTS,
		Game::ASSET_TYPE_XMODEL,
		Game::ASSET_TYPE_MATERIAL,
		Game::ASSET_TYPE_PIXELSHADER,
		Game::ASSET_TYPE_VERTEXSHADER,
		Game::ASSET_TYPE_VERTEXDECL,
		Game::ASSET_TYPE_TECHNIQUE_SET,
		Game::ASSET_TYPE_IMAGE,
		Game::ASSET_TYPE_SOUND,
		Game::ASSET_TYPE_CLIPMAP_MP,
		Game::ASSET_TYPE_COMWORLD,
		Game::ASSET_TYPE_GAMEWORLD_SP,
		Game::ASSET_TYPE_FXWORLD,
		Game::ASSET_TYPE_GFXWORLD,
		Game::ASSET_TYPE_LIGHT_DEF,
		Game::ASSET_TYPE_LOCALIZE_ENTRY,
		Game::ASSET_TYPE_WEAPON,
		Game::ASSET_TYPE_FX,
		Game::ASSET_TYPE_IMPACT_FX,
		Game::ASSET_TYPE_RAWFILE,
		Game::ASSET_TYPE_STRINGTABLE,
		Game::ASSET_TYPE_TRACER,
		Game::ASSET_TYPE_VEHICLE,
	};

	struct IW4xSegment
	{
		std::uint16_t from;
		std::uint16_t to;
		std::uint16_t length;
	};

	constexpr std::uint16_t zeroSegment = 0xFFFF;

	struct IW4xRule
	{
		std::uint16_t record;
		std::uint32_t firstVersion;
		std::uint32_t size32;
		std::span<const IW4xSegment> segments;
	};

	constexpr IW4xSegment xasset334[] = { { 0, 0, 4 }, { 8, 4, 4 } };
	constexpr IW4xSegment gfxImage332[] = { { 0, 0, 28 }, { 32, 28, 4 } };
	constexpr IW4xSegment gfxImage359[] = { { 0, 0, 20 }, { 32, 20, 2 }, { 34, 22, 2 }, { 36, 24, 2 }, { 38, 26, 1 }, { 40, 27, 1 }, { 48, 28, 4 } };
	constexpr IW4xSegment material359[] = { { 8, 0, 4 }, { 18, 4, 1 }, { 20, 5, 3 }, { 0, 8, 8 }, { 12, 16, 6 }, { 22, 22, 2 }, { 24, 24, 72 } };
	constexpr IW4xSegment techniqueSet359[] = { { 0, 0, 12 }, { 16, 12, 192 } };
	constexpr IW4xSegment fxElemDef316[] = { { 0, 0, 252 } };
	constexpr IW4xSegment xmodel316[] =
	{
		{ 0, 0, 36 }, { 44, 36, 28 },
		{ 72, 64, 12 }, { 88, 76, 32 },
		{ 128, 108, 12 }, { 144, 120, 32 },
		{ 184, 152, 12 }, { 200, 164, 32 },
		{ 240, 196, 12 }, { 256, 208, 28 },
		{ 292, 236, 60 }, { 356, 296, 8 },
	};
	constexpr IW4xSegment xmodel318[] =
	{
		{ 0, 0, 36 }, { 44, 36, 28 },
		{ 72, 64, 12 }, { 88, 76, 32 },
		{ 128, 108, 12 }, { 144, 120, 32 },
		{ 184, 152, 12 }, { 200, 164, 32 },
		{ 240, 196, 12 }, { 256, 208, 28 },
		{ 292, 236, 60 }, { 352, 296, 8 },
	};
	constexpr IW4xSegment xsurface316[] = { { 0, 0, 12 }, { 16, 12, 20 }, { 40, 32, 8 }, { 52, 40, 24 } };
	constexpr IW4xSegment xmodelSurfs316[] = { { 0, 0, 36 } };
	constexpr IW4xSegment physPreset316[] = { { 0, 0, 44 } };
	constexpr IW4xSegment sndAlias316[] = { { 0, 0, 60 }, { 68, 60, 20 }, { 88, 80, 20 } };
	constexpr IW4xSegment loadedSound316[] = { { 0, 0, 28 }, { 32, 28, 16 } };
	constexpr IW4xSegment gameWorldSp316[] = { { 0, 0, 44 }, { 72, 44, 12 } };
	constexpr IW4xSegment pathnode316[] = { { 0, 0, 136 } };
	constexpr IW4xSegment pathlink316[] = { { 0, 0, 12 } };
	constexpr IW4xSegment structProperty316[] = { { 0, 0, 16 } };
	constexpr IW4xSegment gfxWorld359[] = { { 0, 0, 252 }, { 252, 252, 12 }, { 272, 264, 84 }, { 1316, 348, 280 } };
	constexpr IW4xSegment vehicleDef316[] = { { 0, 0, 400 }, { zeroSegment, 400, 8 }, { 400, 408, 312 } };

	constexpr IW4xRule iw4xRules[] =
	{
		{ zoneRecordXAsset, 334, 16, xasset334 },
		{ zoneRecordGfxImage, 332, 36, gfxImage332 },
		{ zoneRecordGfxImage, 359, 52, gfxImage359 },
		{ zoneRecordMaterial, 359, 96, material359 },
		{ zoneRecordMaterialTechniqueSet, 359, 208, techniqueSet359 },
		{ zoneRecordFxElemDef, 316, 260, fxElemDef316 },
		{ zoneRecordXModel, 316, 364, xmodel316 },
		{ zoneRecordXModel, 318, 360, xmodel318 },
		{ zoneRecordXSurface, 316, 84, xsurface316 },
		{ zoneRecordXModelSurfs, 316, 48, xmodelSurfs316 },
		{ zoneRecordPhysPreset, 316, 68, physPreset316 },
		{ zoneRecordSnd_alias_t, 316, 108, sndAlias316 },
		{ zoneRecordLoadedSound, 316, 48, loadedSound316 },
		{ zoneRecordGameWorldSp, 316, 84, gameWorldSp316 },
		{ zoneRecordPathnode_t, 316, 148, pathnode316 },
		{ zoneRecordPathlink_s, 316, 16, pathlink316 },
		{ zoneRecordStructuredDataStructProperty, 316, 24, structProperty316 },
		{ zoneRecordGfxWorld, 359, 1596, gfxWorld359 },
		{ zoneRecordVehicleDef, 316, 788, vehicleDef316 },
	};

	enum class TaskKind : std::uint8_t
	{
		Data,
		String,
		Passes,
		LoadDefData,
		Raw,
		H0Part,
		SpeakerEntries,
		XAnimTransFrames,
		XAnimQuatFrames,
	};

	struct Task
	{
		TaskKind kind = TaskKind::Data;
		std::uint16_t type = 0;
		std::uint16_t field = noField;
		std::uint64_t context = 0;
		std::uint32_t count = 0;
		std::uint32_t saved = 0;
		std::uintptr_t fieldAddr = 0;
	};

	struct PendingEntry
	{
		std::uintptr_t fieldAddr;
		std::uint64_t written;
		Task task;
		bool isLive;
	};

	struct PendingBatch
	{
		std::vector<PendingEntry> entries;
		std::size_t first = 0;
		bool isClosed = false;
	};

	struct Region
	{
		std::uint32_t size32;
		std::uint16_t type;
		std::uintptr_t dest;
		const IW4xRule* rule;
	};

	struct ShadowEntry
	{
		std::uint32_t pos;
		std::uint32_t index;
	};

	struct Effect
	{
		std::uintptr_t totalSize = 0;
		std::uintptr_t nameField = 0;
		std::uint32_t count = 0;
		std::int64_t size64 = 0;
		bool isSummed = false;
	};

	enum class Deferred : std::uint8_t
	{
		EffectElements,
		Trail,
	};

	struct Reader
	{
		bool isCandidate = false;
		bool isReading = false;
		bool hasPeek = false;
		std::array<std::uint8_t, 16> peek{};
		std::string zoneName;

		std::uint32_t blockPos[blockCount]{};
		std::uint32_t index = 0;
		std::uint32_t pos = 0;
		std::vector<ShadowEntry> stack;
		std::uint32_t consumed = 0;
		bool isAlignPending = false;
		std::uint32_t alignIndex = 0;
		std::uint32_t align = 0;

		std::vector<PendingBatch> batches;
		std::unordered_map<std::uintptr_t, std::pair<std::size_t, std::size_t>> pendingAt;
		std::unordered_map<std::uintptr_t, Task> continuations;

		bool isInString = false;
		std::uintptr_t stringNext = 0;
		std::uint32_t stringStart = 0;
		std::uintptr_t stringBegin = 0;

		std::map<std::uint32_t, Region> regions;
		std::unordered_map<std::uint32_t, std::uintptr_t> slots;
		std::unordered_map<std::uintptr_t, Deferred> deferred;
		std::vector<std::vector<std::uint8_t>> saved;
		std::vector<std::uint8_t> scratch;
		std::unordered_set<std::uintptr_t> adpcmSounds;
		Effect effect;
		bool hasEffect = false;

		std::vector<std::uint8_t> served;
		bool isDumping = false;

		std::deque<std::uint32_t> fxElemStrings;
		std::deque<std::uint32_t> lodStrings;
		std::deque<std::array<std::uint8_t, 28>> pathTails;
		std::deque<std::array<std::uint32_t, 2>> sunMaterials;
		std::deque<std::uint64_t> driveSlots;
		std::vector<std::unique_ptr<std::uint64_t[]>> driveArrays;
	};

	static Reader reader;
	static bool isReady;

	static std::vector<std::int8_t> recordHasPointer;
	static std::vector<std::int8_t> typeIsPlain;

	static Utils::Hook loadXFileHook;
	static Utils::Hook xfileReadHook;
	static Utils::Hook assetListReadHook;
	static Utils::Hook stringReadHooks[std::size(StringReadCalls)];
	static Utils::Hook loadStreamHook;
	static Utils::Hook allocStreamPosHook;
	static Utils::Hook incStreamPosHook;
	static Utils::Hook pushStreamPosHook;
	static Utils::Hook popStreamPosHook;
	static Utils::Hook insertPointerHook;
	static Utils::Hook offsetToAliasHook;
	static Utils::Hook offsetToPointerHook;
	static Utils::Hook setSoundDataHook;
	static Utils::Hook textureLoadPushHook;
	static Utils::Hook fxElemExtendedHook;
	static Utils::Hook surfsFixupHook;
	static Utils::Hook pathDataHook;
	static Utils::Hook flareMaterialHook;
	static Utils::Hook weaponHooks[std::size(Load_WeaponCompleteDefPtr_Calls)];
	static bool isIW4xReady;
	static bool isImpactPatched;

	static std::uint64_t& StreamPos()
	{
		return *reinterpret_cast<std::uint64_t*>(Utils::Hook::Rebase(g_streamPos));
	}

	static std::uint32_t& StreamPosIndex()
	{
		return *reinterpret_cast<std::uint32_t*>(Utils::Hook::Rebase(g_streamPosIndex));
	}

	static std::uint32_t& StreamPosStackIndex()
	{
		return *reinterpret_cast<std::uint32_t*>(Utils::Hook::Rebase(g_streamPosStackIndex));
	}

	static std::uint64_t& StackPos(std::uint32_t depth)
	{
		return *reinterpret_cast<std::uint64_t*>(Utils::Hook::Rebase(g_streamPosStack) + 16 * depth);
	}

	static std::uint32_t& StackIndex(std::uint32_t depth)
	{
		return *reinterpret_cast<std::uint32_t*>(Utils::Hook::Rebase(g_streamPosStack) + 16 * depth + 8);
	}

	static std::uint8_t* BlockData(std::uint32_t block)
	{
		const auto zoneMem = *reinterpret_cast<std::uint8_t**>(Utils::Hook::Rebase(g_streamZoneMem));
		return *reinterpret_cast<std::uint8_t**>(zoneMem + 16 * block);
	}

	static std::uint32_t BlockSize(std::uint32_t block)
	{
		const auto zoneMem = *reinterpret_cast<std::uint8_t**>(Utils::Hook::Rebase(g_streamZoneMem));
		return *reinterpret_cast<std::uint32_t*>(zoneMem + 16 * block + 8);
	}

	static void SetStreamIndex(std::uint32_t block)
	{
		reinterpret_cast<void(*)(std::uint32_t)>(Utils::Hook::Rebase(DB_SetStreamIndex))(block);
	}

	static void ReadXFile(void* dest, int size)
	{
		reinterpret_cast<void(*)(void*, int, int)>(Utils::Hook::Rebase(DB_ReadXFile))(dest, size, 0);
	}

	static std::uint32_t U32(const std::uint8_t* at)
	{
		std::uint32_t value;
		std::memcpy(&value, at, sizeof(value));
		return value;
	}

	static std::uint16_t U16(const std::uint8_t* at)
	{
		std::uint16_t value;
		std::memcpy(&value, at, sizeof(value));
		return value;
	}

	static void PutU64(std::uint8_t* at, std::uint64_t value)
	{
		std::memcpy(at, &value, sizeof(value));
	}

	static std::uint64_t GetU64(std::uintptr_t at)
	{
		std::uint64_t value;
		std::memcpy(&value, reinterpret_cast<const void*>(at), sizeof(value));
		return value;
	}

	static void Disarm();

	[[noreturn]] static void Fail(const std::string& reason)
	{
		const std::string zoneName = reader.zoneName;
		Disarm();
		Game::Com_Error(1, "32 bit zone '%s' could not be read: %s", zoneName.data(), reason.data());
		std::abort();
	}

	static const ZoneRecord& Record(std::uint16_t record)
	{
		return zoneRecords[record];
	}

	static std::uint16_t FieldIndex(std::uint16_t record, const char* name)
	{
		const auto& r = Record(record);

		for (std::uint16_t i = 0; i < r.fieldCount; ++i)
		{
			if (std::strcmp(zoneFields[r.firstField + i].name, name) == 0)
			{
				return static_cast<std::uint16_t>(r.firstField + i);
			}
		}

		Fail(std::format("{} has no field {}", r.name, name));
	}

	static const ZoneField& Field(std::uint16_t record, const char* name)
	{
		return zoneFields[FieldIndex(record, name)];
	}

	static std::string FieldName(std::uint16_t field)
	{
		if (field == noField)
		{
			return "?";
		}

		return zoneFields[field].name;
	}

	static std::uint32_t Stride32(std::uint16_t type)
	{
		return std::max(zoneTypes[type].size32, 1u);
	}

	static std::uint32_t Stride64(std::uint16_t type)
	{
		return std::max(zoneTypes[type].size64, 1u);
	}

	static bool TypeHasPointer(std::uint16_t type);

	static bool RecordHasPointer(std::uint16_t record)
	{
		if (recordHasPointer[record] >= 0)
		{
			return recordHasPointer[record] != 0;
		}

		recordHasPointer[record] = 0;
		const auto& r = Record(record);
		bool hasPointer = false;

		for (std::uint16_t i = 0; i < r.fieldCount && !hasPointer; ++i)
		{
			hasPointer = TypeHasPointer(zoneFields[r.firstField + i].type);
		}

		recordHasPointer[record] = hasPointer;
		return hasPointer;
	}

	static bool TypeHasPointer(std::uint16_t type)
	{
		const auto& t = zoneTypes[type];

		if (t.kind == ZoneKind::Pointer || t.kind == ZoneKind::String || t.kind == ZoneKind::Runtime)
		{
			return true;
		}

		if (t.kind == ZoneKind::Array)
		{
			return TypeHasPointer(t.target);
		}

		if (t.kind == ZoneKind::Record)
		{
			return RecordHasPointer(t.target);
		}

		return false;
	}

	static bool IsPlain(std::uint16_t type)
	{
		if (typeIsPlain[type] >= 0)
		{
			return typeIsPlain[type] != 0;
		}

		const auto& t = zoneTypes[type];
		const bool isPlain = t.size32 == t.size64 && !TypeHasPointer(type);
		typeIsPlain[type] = isPlain;
		return isPlain;
	}

	static void ShadowSetIndex(std::uint32_t block)
	{
		if (block != reader.index)
		{
			reader.blockPos[reader.index] = reader.pos;
			reader.index = block;
			reader.pos = reader.blockPos[block];
		}
	}

	static void ApplyAlign()
	{
		if (!reader.isAlignPending)
		{
			return;
		}

		reader.isAlignPending = false;

		if (reader.alignIndex == reader.index && reader.consumed == 0)
		{
			reader.pos = (reader.pos + reader.align) & ~reader.align;
		}
	}

	static std::uint32_t ShadowAddress()
	{
		return (reader.index << 28) | (reader.pos + reader.consumed);
	}

	static void CheckInBlock(const void* dest, std::size_t size)
	{
		const auto index = StreamPosIndex();
		const auto* data = BlockData(index);
		const auto* at = static_cast<const std::uint8_t*>(dest);

		if (at < data || at + size > data + BlockSize(index))
		{
			Fail(std::format("block {} needs more than the {} bytes planned for it", index, BlockSize(index)));
		}
	}

	static std::uint32_t Consume(std::size_t size, void* into)
	{
		ApplyAlign();
		const auto addr = ShadowAddress();

		if (size)
		{
			ReadXFile(into, static_cast<int>(size));
		}

		reader.consumed += static_cast<std::uint32_t>(size);
		return addr;
	}

	static std::uint32_t Zeroed(std::size_t size)
	{
		ApplyAlign();
		const auto addr = ShadowAddress();
		reader.consumed += static_cast<std::uint32_t>(size);
		return addr;
	}

	static void AddRegion(std::uint32_t addr, std::uint32_t size32, std::uintptr_t dest, std::uint16_t type, const IW4xRule* rule = nullptr)
	{
		const std::uint32_t end = addr + std::max(size32, 1u);
		auto it = reader.regions.lower_bound(addr);

		if (it != reader.regions.begin())
		{
			auto before = std::prev(it);

			if (before->first + std::max(before->second.size32, 1u) > addr)
			{
				it = before;
			}
		}

		while (it != reader.regions.end() && it->first < end)
		{
			it = reader.regions.erase(it);
		}

		reader.regions[addr] = { size32, type, dest, rule };
	}

	static const IW4xRule* RuleFor(std::uint16_t type)
	{
		const auto version = Zones::Version();
		const auto& t = zoneTypes[type];

		if (version < iw4xFirstVersion || t.kind != ZoneKind::Record)
		{
			return nullptr;
		}

		const IW4xRule* rule = nullptr;

		for (const auto& candidate : iw4xRules)
		{
			if (candidate.record == t.target && candidate.firstVersion <= version)
			{
				rule = &candidate;
			}
		}

		return rule;
	}

	static bool TryStockOffset(const IW4xRule& rule, std::uint32_t inner, std::uint32_t& stockInner)
	{
		for (const auto& segment : rule.segments)
		{
			const auto end = static_cast<std::uint32_t>(segment.from + segment.length);

			if (segment.from <= inner && inner < end)
			{
				stockInner = segment.to + inner - segment.from;
				return true;
			}
		}

		return false;
	}

	static std::uint32_t InnerOffset(std::uint16_t type, std::uint32_t off32)
	{
		if (off32 == 0)
		{
			return 0;
		}

		const auto& t = zoneTypes[type];

		if (t.kind == ZoneKind::Record)
		{
			const auto& r = Record(t.target);

			for (std::uint16_t i = 0; i < r.fieldCount; ++i)
			{
				const auto& f = zoneFields[r.firstField + i];

				if (f.offset32 <= off32 && off32 < f.offset32 + f.size32)
				{
					return f.offset64 + InnerOffset(f.type, off32 - f.offset32);
				}
			}
		}

		if (t.kind == ZoneKind::Array)
		{
			const auto s32 = Stride32(t.target);
			return (off32 / s32) * Stride64(t.target) + InnerOffset(t.target, off32 % s32);
		}

		if (t.kind == ZoneKind::Bytes)
		{
			return off32;
		}

		Fail(std::format("an offset lands inside a pointer, +{}", off32));
	}

	static std::uintptr_t MapShadow(std::uint32_t addr)
	{
		auto it = reader.regions.upper_bound(addr);

		if (it == reader.regions.begin())
		{
			return 0;
		}

		--it;
		const auto start = it->first;
		const auto& region = it->second;
		auto s32 = Stride32(region.type);
		const auto s64 = Stride64(region.type);

		if (region.rule)
		{
			s32 = region.rule->size32;
		}

		if (region.size32 && addr == start + region.size32 && region.size32 % s32 == 0)
		{
			return region.dest + (region.size32 / s32) * s64;
		}

		if (addr < start || addr >= start + std::max(region.size32, 1u))
		{
			return 0;
		}

		const auto index = (addr - start) / s32;
		auto inner = (addr - start) % s32;

		if (region.rule && !TryStockOffset(*region.rule, inner, inner))
		{
			return 0;
		}

		return region.dest + index * s64 + InnerOffset(region.type, inner);
	}

	static void AddPending(std::uintptr_t fieldAddr, Task task, std::uint64_t written)
	{
		task.fieldAddr = fieldAddr;

		if (reader.batches.empty() || reader.batches.back().isClosed)
		{
			reader.batches.emplace_back();
		}

		auto& entries = reader.batches.back().entries;
		reader.pendingAt[fieldAddr] = { reader.batches.size() - 1, entries.size() };
		entries.push_back({ fieldAddr, written, task, true });
	}

	static void CloseBatch()
	{
		if (!reader.batches.empty())
		{
			reader.batches.back().isClosed = true;
		}
	}

	static void DropPending(std::uintptr_t fieldAddr)
	{
		const auto it = reader.pendingAt.find(fieldAddr);

		if (it == reader.pendingAt.end())
		{
			return;
		}

		reader.batches[it->second.first].entries[it->second.second].isLive = false;
		reader.pendingAt.erase(it);
	}

	static bool TakePending(std::uintptr_t dest, Task& task)
	{
		for (std::size_t b = reader.batches.size(); b-- > 0;)
		{
			auto& batch = reader.batches[b];

			while (batch.first < batch.entries.size() && !batch.entries[batch.first].isLive)
			{
				++batch.first;
			}

			for (std::size_t i = batch.first; i < batch.entries.size(); ++i)
			{
				auto& entry = batch.entries[i];

				if (!entry.isLive)
				{
					continue;
				}

				const auto value = GetU64(entry.fieldAddr);

				if (value == dest && value != entry.written)
				{
					entry.isLive = false;
					reader.pendingAt.erase(entry.fieldAddr);
					task = entry.task;

					while (!reader.batches.empty() && reader.batches.back().isClosed)
					{
						const auto& last = reader.batches.back();
						bool isSpent = true;

						for (std::size_t j = last.first; j < last.entries.size() && isSpent; ++j)
						{
							isSpent = !last.entries[j].isLive;
						}

						if (!isSpent)
						{
							break;
						}

						reader.batches.pop_back();
					}

					return true;
				}
			}
		}

		return false;
	}

	static bool TakeContinuation(std::uintptr_t dest, Task& task)
	{
		const auto it = reader.continuations.find(dest);

		if (it == reader.continuations.end())
		{
			return false;
		}

		task = it->second;
		reader.continuations.erase(it);
		return true;
	}

	static void ConvertValue(std::uint16_t type, const std::uint8_t* src, std::uint8_t* dst, std::uint16_t field, std::uint64_t context);

	static std::uint64_t PointerValue(std::uint8_t* fieldAt, bool isString, std::uint16_t targetType, std::uint32_t stored, std::uint16_t field, std::uint64_t context)
	{
		if (!stored)
		{
			return 0;
		}

		std::uint64_t value = stored;

		if (stored == inlineMarker32)
		{
			value = inlineMarker;
		}
		else if (stored == insertMarker32)
		{
			value = insertMarker;
		}

		Task task;
		task.field = field;
		task.context = context;

		if (isString)
		{
			task.kind = TaskKind::String;
		}
		else
		{
			task.kind = TaskKind::Data;
			task.type = targetType;
		}

		AddPending(reinterpret_cast<std::uintptr_t>(fieldAt), task, value);
		return value;
	}

	static void PutPointer(std::uint8_t* dst, std::uint32_t offset64, bool isString, std::uint16_t targetType, const std::uint8_t* srcField, std::uint16_t field, std::uint64_t context)
	{
		PutU64(dst + offset64, PointerValue(dst + offset64, isString, targetType, U32(srcField), field, context));
	}

	static void ConvertFields(std::uint16_t record, const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context, std::initializer_list<const char*> skip)
	{
		const auto& r = Record(record);
		std::vector<std::uint32_t> copiedUnits;

		for (std::uint16_t i = 0; i < r.fieldCount; ++i)
		{
			const auto& f = zoneFields[r.firstField + i];
			bool isSkipped = false;

			for (const auto* name : skip)
			{
				isSkipped = isSkipped || std::strcmp(f.name, name) == 0;
			}

			if (isSkipped)
			{
				continue;
			}

			if (f.isBits)
			{
				if (std::find(copiedUnits.begin(), copiedUnits.end(), f.offset32) != copiedUnits.end())
				{
					continue;
				}

				copiedUnits.push_back(f.offset32);
				std::memcpy(dst + f.offset64, src + f.offset32, f.size32);
				continue;
			}

			ConvertValue(f.type, src + f.offset32, dst + f.offset64, static_cast<std::uint16_t>(r.firstField + i), context);
		}
	}

	static void RunDeferred(std::uintptr_t fieldAddr, const std::uint8_t* src, std::uint32_t count);

	static void CloseEffect()
	{
		reader.hasEffect = false;
		reader.effect = {};
	}

	static void HandleXAsset(const std::uint8_t* src, std::uint8_t* dst)
	{
		auto assetType = U32(src);
		const auto version = Zones::Version();

		if (assetType == iw4xGameMapMpType && version >= iw4xFirstVersion && version < iw4xGameMapSpEndVersion)
		{
			assetType = Game::ASSET_TYPE_GAMEWORLD_SP;
			Maps::HandleAsSPMap();
		}

		std::memcpy(dst, &assetType, sizeof(assetType));

		if (version >= iw4xFirstVersion && std::find(std::begin(iw4xAssetTypes), std::end(iw4xAssetTypes), assetType) == std::end(iw4xAssetTypes))
		{
			const char* typeName = "?";

			if (assetType < std::size(zoneAssetTypes))
			{
				typeName = Game::g_assetNames[assetType];
			}

			Fail(std::format("it is an IW4x zone at version {} holding asset type {} ({}), which is not read at that version yet", version, assetType, typeName));
		}

		if (assetType >= std::size(zoneAssetTypes) || zoneAssetTypes[assetType] == zoneNoType)
		{
			Fail(std::format("asset type {} is not read yet", assetType));
		}

		PutPointer(dst, 8, false, zoneAssetTypes[assetType], src + 4, FieldIndex(zoneRecordXAsset, "header"), 0);
	}

	static void HandleGfxImage(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordGfxImage, src, dst, context, { "texture" });
		const auto category = reinterpret_cast<std::uintptr_t>(dst + Field(zoneRecordGfxImage, "category").offset64);
		PutPointer(dst, 0, false, zoneTypeGfxImageLoadDef, src, FieldIndex(zoneRecordGfxImage, "texture"), category);
	}

	static void HandleFxEffectDef(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		CloseEffect();
		ConvertFields(zoneRecordFxEffectDef, src, dst, context, {});

		const auto totalSize = dst + Field(zoneRecordFxEffectDef, "totalSize").offset64;
		const auto count = U32(src + 16) + U32(src + 20) + U32(src + 24);

		if (!U32(src + 28))
		{
			if (count)
			{
				Fail(std::format("an effect with {} elements and no element list", count));
			}

			const auto size64 = static_cast<std::int32_t>(U32(src + 8)) + 8;
			std::memcpy(totalSize, &size64, sizeof(size64));
			return;
		}

		reader.hasEffect = true;
		reader.effect = {};
		reader.effect.totalSize = reinterpret_cast<std::uintptr_t>(totalSize);
		reader.effect.nameField = reinterpret_cast<std::uintptr_t>(dst + Field(zoneRecordFxEffectDef, "name").offset64);
		reader.effect.count = count;

		const auto elemsField = reinterpret_cast<std::uintptr_t>(dst + Field(zoneRecordFxEffectDef, "elemDefs").offset64);
		reader.deferred[elemsField] = Deferred::EffectElements;
	}

	static bool IsInline(const std::uint8_t* at)
	{
		const auto value = U32(at);
		return value == inlineMarker32 || value == insertMarker32;
	}

	static void SumEffectElements(const std::uint8_t* elems)
	{
		if (!reader.hasEffect)
		{
			return;
		}

		const auto namePtr = GetU64(reader.effect.nameField);
		std::int64_t nameLength = 0;

		if (namePtr)
		{
			nameLength = static_cast<std::int64_t>(std::strlen(reinterpret_cast<const char*>(namePtr))) + 1;
		}

		std::int64_t size64 = 40 + nameLength + 288 * static_cast<std::int64_t>(reader.effect.count);

		for (std::uint32_t i = 0; i < reader.effect.count; ++i)
		{
			const auto* elem = elems + 252 * i;
			const auto elemType = elem[176];
			const auto visualCount = elem[177];
			const auto velCount = elem[178];
			const auto visCount = elem[179];

			if (IsInline(elem + 180))
			{
				size64 += 96 * (velCount + 1);
			}

			if (IsInline(elem + 184))
			{
				size64 += 48 * (visCount + 1);
			}

			if (elemType == 11 && IsInline(elem + 188))
			{
				size64 += 16 * visualCount;
			}
			else if (visualCount > 1 && IsInline(elem + 188))
			{
				size64 += 8 * visualCount;
			}

			if (IsInline(elem + 244) && elemType == 6)
			{
				size64 += 52;
			}
		}

		reader.effect.size64 = size64;
		reader.effect.isSummed = true;
		const auto value = static_cast<std::int32_t>(size64);
		std::memcpy(reinterpret_cast<void*>(reader.effect.totalSize), &value, sizeof(value));
	}

	static void AddTrail(const std::uint8_t* trail)
	{
		if (!reader.hasEffect || !reader.effect.isSummed)
		{
			Fail("an effect trail outside an effect's elements");
		}

		std::int64_t parts = 0;

		if (IsInline(trail + 24))
		{
			parts += 20 * static_cast<std::int64_t>(U32(trail + 20));
		}

		if (IsInline(trail + 32))
		{
			parts += 2 * static_cast<std::int64_t>(U32(trail + 28));
		}

		reader.effect.size64 += 48 + parts;
		const auto value = static_cast<std::int32_t>(reader.effect.size64);
		std::memcpy(reinterpret_cast<void*>(reader.effect.totalSize), &value, sizeof(value));
	}

	static void RunDeferred(std::uintptr_t fieldAddr, const std::uint8_t* src, [[maybe_unused]] std::uint32_t count)
	{
		const auto it = reader.deferred.find(fieldAddr);

		if (it == reader.deferred.end())
		{
			return;
		}

		const auto kind = it->second;
		reader.deferred.erase(it);

		if (kind == Deferred::EffectElements)
		{
			SumEffectElements(src);
			return;
		}

		AddTrail(src);
	}

	static void HandleGfxAabbTree(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		const auto& r = Record(zoneRecordGfxAabbTree);

		if (r.size32 != 44 || r.size64 != 56)
		{
			Fail(std::format("GfxAabbTree is {} / {} bytes, expected 44 / 56", r.size32, r.size64));
		}

		ConvertFields(zoneRecordGfxAabbTree, src, dst, context, { "childrenOffset" });
		const auto& field = Field(zoneRecordGfxAabbTree, "childrenOffset");
		const auto childrenOffset = static_cast<std::int32_t>(U32(src + field.offset32));

		if (childrenOffset % static_cast<std::int32_t>(r.size32) != 0)
		{
			Fail(std::format("GfxAabbTree childrenOffset {} is not a whole number of nodes", childrenOffset));
		}

		const std::int32_t scaled = childrenOffset / static_cast<std::int32_t>(r.size32) * static_cast<std::int32_t>(r.size64);
		std::memcpy(dst + field.offset64, &scaled, sizeof(scaled));
	}

	static void HandleMaterial(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordMaterial, src, dst, context, {});

		const auto& info = Field(zoneRecordMaterial, "info");
		const auto infoRecord = zoneTypes[info.type].target;
		const auto& drawSurf = Field(infoRecord, "drawSurf");
		std::memset(dst + info.offset64 + drawSurf.offset64, 0, 8);
	}

	static void HandleSoundFile(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		std::memcpy(dst, src, 2);
		const auto& u = Field(zoneRecordSoundFile, "u");

		if (src[0] == 1)
		{
			PutPointer(dst, u.offset64, false, zoneTypeLoadedSound, src + u.offset32, FieldIndex(zoneRecordSoundFile, "u"), 0);
			return;
		}

		ConvertValue(zoneTypeStreamFileNameRaw, src + u.offset32, dst + u.offset64, FieldIndex(zoneRecordSoundFile, "u"), context);
	}

	static void HandleMssSound(const std::uint8_t* src, std::uint8_t* dst)
	{
		const auto& r = Record(zoneRecordMssSound);

		if (r.size32 != 40 || r.size64 != 56)
		{
			Fail(std::format("MssSound is {} / {} bytes, expected 40 / 56", r.size32, r.size64));
		}

		const auto soundFormat = U32(src);
		const auto dataLength = U32(src + 8);
		const auto rate = U32(src + 12);
		const auto bits = static_cast<std::int32_t>(U32(src + 16));
		const auto channels = static_cast<std::int32_t>(U32(src + 20));
		const auto samples = U32(src + 24);
		const auto blockSize = U32(src + 28);
		const auto dataField = FieldIndex(zoneRecordMssSound, "data");

		std::memset(dst, 0, 56);

		if (std::all_of(src, src + 40, [](const std::uint8_t byte) { return byte == 0; }))
		{
			return;
		}

		if (soundFormat == 17)
		{
			if ((channels != 1 && channels != 2) || blockSize <= 4u * channels)
			{
				Fail(std::format("an IMA ADPCM sound with {} channels in blocks of {}", channels, blockSize));
			}

			const std::uint16_t header[] = { 17, static_cast<std::uint16_t>(channels) };
			std::memcpy(dst, header, sizeof(header));
			std::memcpy(dst + 4, &rate, 4);
			const std::uint16_t blockAlign[] = { static_cast<std::uint16_t>(blockSize), static_cast<std::uint16_t>(bits) };
			std::memcpy(dst + 12, blockAlign, sizeof(blockAlign));
			std::memcpy(dst + 24, &dataLength, 4);
			std::memcpy(dst + 28, &samples, 4);
			PutPointer(dst, 48, false, zoneTypeBytes1, src + 36, dataField, 0);
			reader.adpcmSounds.insert(reinterpret_cast<std::uintptr_t>(dst));
			return;
		}

		if (soundFormat != 1)
		{
			Fail(std::format("a sound in format {}, neither PCM nor IMA ADPCM", soundFormat));
		}

		if (bits <= 0 || channels <= 0 || blockSize != static_cast<std::uint32_t>(channels * bits / 8))
		{
			Fail(std::format("a sound in blocks of {} for {} channels of {} bits", blockSize, channels, bits));
		}

		const std::uint16_t header[] = { 1, static_cast<std::uint16_t>(channels) };
		std::memcpy(dst, header, sizeof(header));
		std::memcpy(dst + 4, &rate, 4);
		const std::uint32_t averageBytes = rate * blockSize;
		std::memcpy(dst + 8, &averageBytes, 4);
		const std::uint16_t blockAlign[] = { static_cast<std::uint16_t>(blockSize), static_cast<std::uint16_t>(bits) };
		std::memcpy(dst + 12, blockAlign, sizeof(blockAlign));
		std::memcpy(dst + 24, &dataLength, 4);
		PutPointer(dst, 48, false, zoneTypeBytes1, src + 36, dataField, 0);
	}

	static void HandleMaterialTextureDef(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordMaterialTextureDef, src, dst, context, { "u" });
		const auto& u = Field(zoneRecordMaterialTextureDef, "u");
		const auto field = FieldIndex(zoneRecordMaterialTextureDef, "u");

		if (src[7] == 11)
		{
			PutPointer(dst, u.offset64, false, zoneTypeWater_t, src + u.offset32, field, 0);
			return;
		}

		PutPointer(dst, u.offset64, false, zoneTypeGfxImage, src + u.offset32, field, 0);
	}

	static std::uint16_t RemapCodeConst(std::uint16_t stored, std::uint32_t version)
	{
		std::int32_t index = stored;

		if (index >= 58 && index <= 135)
		{
			index -= 3;

			if (version >= iw4xMaterialVersion)
			{
				index -= 7;

				if (index <= 53)
				{
					index += 1;
				}
			}
		}
		else if (index >= 11 && index < 58)
		{
			index -= 2;

			if (version >= iw4xMaterialVersion)
			{
				if (index > 15 && index < 30)
				{
					index -= 1;

					if (index == 19)
					{
						index = 21;
					}
				}
				else if (index >= 50)
				{
					index += 6;
				}
			}
		}

		return static_cast<std::uint16_t>(index);
	}

	static void HandleMaterialShaderArgument(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordMaterialShaderArgument, src, dst, context, { "u" });
		const auto& u = Field(zoneRecordMaterialShaderArgument, "u");
		const auto argType = U16(src);

		if (argType == 1 || argType == 7)
		{
			PutPointer(dst, u.offset64, false, zoneTypeBytes4, src + u.offset32, FieldIndex(zoneRecordMaterialShaderArgument, "u"), 0);
			return;
		}

		std::memcpy(dst + u.offset64, src + u.offset32, 4);

		const auto version = Zones::Version();

		if ((argType == 3 || argType == 5) && version >= iw4xFirstVersion)
		{
			const auto index = RemapCodeConst(U16(src + u.offset32), version);
			std::memcpy(dst + u.offset64, &index, sizeof(index));
		}
	}

	static void HandleOperand(const std::uint8_t* src, std::uint8_t* dst)
	{
		std::memcpy(dst, src, 4);
		const auto& internals = Field(zoneRecordOperand, "internals");
		const auto field = FieldIndex(zoneRecordOperand, "internals");
		const auto dataType = U32(src);

		if (dataType == 2)
		{
			PutPointer(dst, internals.offset64, true, 0, src + internals.offset32, field, 0);
			return;
		}

		if (dataType == 3)
		{
			PutPointer(dst, internals.offset64, false, zoneTypeStatement_s, src + internals.offset32, field, 0);
			return;
		}

		std::memcpy(dst + internals.offset64, src + internals.offset32, 4);
	}

	static void HandleExpressionEntry(const std::uint8_t* src, std::uint8_t* dst)
	{
		std::memcpy(dst, src, 4);
		const auto& data = Field(zoneRecordExpressionEntry, "data");

		if (U32(src))
		{
			HandleOperand(src + data.offset32, dst + data.offset64);
			return;
		}

		std::memcpy(dst + data.offset64, src + data.offset32, 4);
	}

	static void HandleStatement(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordStatement_s, src, dst, context, { "lastResult" });
		const auto& lastResult = Field(zoneRecordStatement_s, "lastResult");
		const auto* resultSrc = src + lastResult.offset32;
		auto* resultDst = dst + lastResult.offset64;
		std::memcpy(resultDst, resultSrc, 4);

		if (U32(resultSrc) <= 1)
		{
			std::memcpy(resultDst + 8, resultSrc + 4, 4);
		}
	}

	static void HandleMenuEventHandler(const std::uint8_t* src, std::uint8_t* dst)
	{
		const auto& eventType = Field(zoneRecordMenuEventHandler, "eventType");
		const auto& eventData = Field(zoneRecordMenuEventHandler, "eventData");
		const auto field = FieldIndex(zoneRecordMenuEventHandler, "eventData");
		const auto type = src[eventType.offset32];
		dst[eventType.offset64] = type;

		if (type == 0)
		{
			PutPointer(dst, eventData.offset64, true, 0, src + eventData.offset32, field, 0);
			return;
		}

		if (type == 1)
		{
			PutPointer(dst, eventData.offset64, false, zoneTypeConditionalScript, src + eventData.offset32, field, 0);
			return;
		}

		if (type == 2)
		{
			PutPointer(dst, eventData.offset64, false, zoneTypeMenuEventHandlerSet, src + eventData.offset32, field, 0);
			return;
		}

		if (type >= 3 && type <= 6)
		{
			PutPointer(dst, eventData.offset64, false, zoneTypeSetLocalVarData, src + eventData.offset32, field, 0);
			return;
		}

		std::memcpy(dst + eventData.offset64, src + eventData.offset32, 4);
	}

	static std::uint16_t ItemDataType(std::uint32_t itemType)
	{
		switch (itemType)
		{
		case 0:
		case 4:
		case 9:
		case 10:
		case 11:
		case 14:
		case 16:
		case 17:
		case 18:
		case 22:
		case 23:
			return zoneTypeEditFieldDef_s;
		case 6:
			return zoneTypeListBoxDef_s;
		case 12:
			return zoneTypeMultiDef_s;
		case 20:
			return zoneTypeNewsTickerDef_s;
		case 21:
			return zoneTypeTextScrollDef_s;
		default:
			return zoneNoType;
		}
	}

	static void HandleItemDef(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordItemDef_s, src, dst, context, { "typeData" });
		const auto& typeData = Field(zoneRecordItemDef_s, "typeData");
		const auto field = FieldIndex(zoneRecordItemDef_s, "typeData");
		const auto itemType = U32(src + Field(zoneRecordItemDef_s, "type").offset32);

		if (itemType == 13)
		{
			PutPointer(dst, typeData.offset64, true, 0, src + typeData.offset32, field, 0);
			return;
		}

		const auto dataType = ItemDataType(itemType);

		if (dataType != zoneNoType)
		{
			PutPointer(dst, typeData.offset64, false, dataType, src + typeData.offset32, field, 0);
			return;
		}

		std::memcpy(dst + typeData.offset64, src + typeData.offset32, 4);
	}

	static void FxVisual(std::uint64_t elemType, const std::uint8_t* src, std::uint8_t* dst, std::uint16_t field)
	{
		if (elemType == 7)
		{
			PutPointer(dst, 0, false, zoneTypeXModel, src, field, 0);
			return;
		}

		if (elemType == 8 || elemType == 9)
		{
			PutU64(dst, 0);
			return;
		}

		if (elemType == 10 || elemType == 12)
		{
			PutPointer(dst, 0, true, 0, src, field, 0);
			return;
		}

		PutPointer(dst, 0, false, zoneTypeMaterial, src, field, 0);
	}

	static void HandleFxElemDef(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordFxElemDef, src, dst, context, { "visuals", "extended" });

		const auto elemType = src[Field(zoneRecordFxElemDef, "elemType").offset32];
		const auto visualCount = src[Field(zoneRecordFxElemDef, "visualCount").offset32];
		const auto& visuals = Field(zoneRecordFxElemDef, "visuals");
		const auto visualsField = FieldIndex(zoneRecordFxElemDef, "visuals");

		if (elemType == 11)
		{
			PutPointer(dst, visuals.offset64, false, zoneTypeFxElemMarkVisuals, src + visuals.offset32, visualsField, 0);
		}
		else if (visualCount > 1)
		{
			PutPointer(dst, visuals.offset64, false, zoneTypeFxElemVisuals, src + visuals.offset32, visualsField, elemType);
		}
		else
		{
			FxVisual(elemType, src + visuals.offset32, dst + visuals.offset64, visualsField);
		}

		const auto& extended = Field(zoneRecordFxElemDef, "extended");
		const auto extendedField = FieldIndex(zoneRecordFxElemDef, "extended");
		std::uint16_t target = zoneTypeBytes1;

		if (elemType == 3)
		{
			target = zoneTypeFxTrailDef;

			if (U32(src + extended.offset32))
			{
				reader.deferred[reinterpret_cast<std::uintptr_t>(dst + extended.offset64)] = Deferred::Trail;
			}
		}
		else if (elemType == 6)
		{
			target = zoneTypeFxSparkFountainDef;
		}

		PutPointer(dst, extended.offset64, false, target, src + extended.offset32, extendedField, 0);
	}

	static void HandleXAnimParts(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		const auto& r = Record(zoneRecordXAnimParts);

		if (r.size32 != 88 || r.size64 != 136)
		{
			Fail(std::format("XAnimParts is {} / {} bytes, expected 88 / 136", r.size32, r.size64));
		}

		ConvertFields(zoneRecordXAnimParts, src, dst, context, { "indices", "deltaPart" });
		const auto numframes = U16(src + 14);
		PutPointer(dst, 112, false, zoneTypeBytes1, src + 76, FieldIndex(zoneRecordXAnimParts, "indices"), 0);
		PutPointer(dst, 128, false, zoneTypeXAnimDeltaPart, src + 84, FieldIndex(zoneRecordXAnimParts, "deltaPart"), numframes);
	}

	static void HandleXAnimDeltaPart(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		const std::uint16_t targets[] = { zoneTypeXAnimPartTrans, zoneTypeXAnimDeltaPartQuat2, zoneTypeXAnimDeltaPartQuat };
		const char* names[] = { "trans", "quat2", "quat" };

		for (std::uint32_t i = 0; i < 3; ++i)
		{
			PutPointer(dst, 8 * i, false, targets[i], src + 4 * i, FieldIndex(zoneRecordXAnimDeltaPart, names[i]), context);
		}
	}

	static void HandleCLeafBrushNode(const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		ConvertFields(zoneRecordCLeafBrushNode_s, src, dst, context, { "data" });
		const auto leafBrushCount = static_cast<std::int16_t>(U16(src + 2));
		const auto& data = Field(zoneRecordCLeafBrushNode_s, "data");

		if (leafBrushCount > 0)
		{
			PutPointer(dst, data.offset64, false, zoneTypeBytes2, src + data.offset32, FieldIndex(zoneRecordCLeafBrushNode_s, "data"), 0);
			return;
		}

		std::memcpy(dst + data.offset64, src + data.offset32, 12);
	}

	static void HandlePathnodeTree(const std::uint8_t* src, std::uint8_t* dst)
	{
		const auto axis = static_cast<std::int32_t>(U32(src));
		std::memcpy(dst, src, 8);

		const auto& u = Field(zoneRecordPathnode_tree_t, "u");
		const auto field = FieldIndex(zoneRecordPathnode_tree_t, "u");

		if (axis < 0)
		{
			std::memcpy(dst + u.offset64, src + u.offset32, 4);
			PutPointer(dst, u.offset64 + 8, false, zoneTypeBytes2, src + u.offset32 + 4, field, 0);
			return;
		}

		PutPointer(dst, u.offset64, false, zoneTypePathnode_tree_t, src + u.offset32, field, 0);
		PutPointer(dst, u.offset64 + 8, false, zoneTypePathnode_tree_t, src + u.offset32 + 4, field, 0);
	}

	static void ConvertRecord(std::uint16_t record, const std::uint8_t* src, std::uint8_t* dst, std::uint64_t context)
	{
		if (record == zoneRecordPathnode_tree_t)
		{
			HandlePathnodeTree(src, dst);
			return;
		}

		if (record == zoneRecordXAsset)
		{
			HandleXAsset(src, dst);
			return;
		}

		if (record == zoneRecordGfxImage)
		{
			HandleGfxImage(src, dst, context);
			return;
		}

		if (record == zoneRecordFxEffectDef)
		{
			HandleFxEffectDef(src, dst, context);
			return;
		}

		if (record == zoneRecordGfxAabbTree)
		{
			HandleGfxAabbTree(src, dst, context);
			return;
		}

		if (record == zoneRecordMaterial)
		{
			HandleMaterial(src, dst, context);
			return;
		}

		if (record == zoneRecordSoundFile)
		{
			HandleSoundFile(src, dst, context);
			return;
		}

		if (record == zoneRecordMssSound)
		{
			HandleMssSound(src, dst);
			return;
		}

		if (record == zoneRecordMaterialTextureDef)
		{
			HandleMaterialTextureDef(src, dst, context);
			return;
		}

		if (record == zoneRecordMaterialShaderArgument)
		{
			HandleMaterialShaderArgument(src, dst, context);
			return;
		}

		if (record == zoneRecordFxElemDef)
		{
			HandleFxElemDef(src, dst, context);
			return;
		}

		if (record == zoneRecordFxElemVisuals)
		{
			FxVisual(context, src, dst, noField);
			return;
		}

		if (record == zoneRecordFxEffectDefRef)
		{
			PutPointer(dst, 0, true, 0, src, noField, 0);
			return;
		}

		if (record == zoneRecordXAnimParts)
		{
			HandleXAnimParts(src, dst, context);
			return;
		}

		if (record == zoneRecordXAnimDeltaPart)
		{
			HandleXAnimDeltaPart(src, dst, context);
			return;
		}

		if (record == zoneRecordCLeafBrushNode_s)
		{
			HandleCLeafBrushNode(src, dst, context);
			return;
		}

		if (record == zoneRecordOperand)
		{
			HandleOperand(src, dst);
			return;
		}

		if (record == zoneRecordExpressionEntry)
		{
			HandleExpressionEntry(src, dst);
			return;
		}

		if (record == zoneRecordStatement_s)
		{
			HandleStatement(src, dst, context);
			return;
		}

		if (record == zoneRecordMenuEventHandler)
		{
			HandleMenuEventHandler(src, dst);
			return;
		}

		if (record == zoneRecordItemDef_s)
		{
			HandleItemDef(src, dst, context);
			return;
		}

		const auto& r = Record(record);

		if (r.isUnion)
		{
			if (RecordHasPointer(record))
			{
				Fail(std::format("union {} with pointers has no handler", r.name));
			}

			std::memcpy(dst, src, r.size32);
			return;
		}

		ConvertFields(record, src, dst, context, {});
	}

	static void ConvertValue(std::uint16_t type, const std::uint8_t* src, std::uint8_t* dst, std::uint16_t field, std::uint64_t context)
	{
		const auto& t = zoneTypes[type];

		if (t.kind == ZoneKind::Bytes)
		{
			std::memcpy(dst, src, t.size32);
			return;
		}

		if (t.kind == ZoneKind::Array)
		{
			const auto s32 = Stride32(t.target);
			const auto s64 = Stride64(t.target);

			for (std::uint32_t i = 0; i < t.count; ++i)
			{
				ConvertValue(t.target, src + i * s32, dst + i * s64, field, context);
			}

			return;
		}

		if (t.kind == ZoneKind::Record)
		{
			if (RuleFor(type))
			{
				Fail(std::format("an IW4x {} inside {}", Record(t.target).name, FieldName(field)));
			}

			ConvertRecord(t.target, src, dst, context);
			return;
		}

		if (t.kind == ZoneKind::Runtime)
		{
			PutU64(dst, 0);
			return;
		}

		const bool isString = t.kind == ZoneKind::String;
		PutU64(dst, PointerValue(dst, isString, t.target, U32(src), field, context));
	}

	static std::uint32_t IndexBytes(std::uint64_t numframes, std::uint32_t size)
	{
		if (numframes >= 0x100)
		{
			return 2 * (size + 1);
		}

		return size + 1;
	}

	static void ReadGfxImageLoadDef(const Task& task, std::uint8_t* dest, std::size_t size)
	{
		if (size != 16)
		{
			Fail(std::format("a GfxImageLoadDef read of {}", size));
		}

		const auto addr = Consume(16, dest);
		const auto resourceSize = U32(dest + 12);

		if (resourceSize)
		{
			Task data;
			data.kind = TaskKind::LoadDefData;
			reader.continuations[reinterpret_cast<std::uintptr_t>(dest) + 16] = data;
		}
		else if (task.context)
		{
			auto* category = reinterpret_cast<std::uint8_t*>(task.context);

			if (*category == 0)
			{
				*category = imgCategoryLoadFromFile;
			}
		}

		AddRegion(addr, 16, reinterpret_cast<std::uintptr_t>(dest), zoneTypeGfxImageLoadDef);
	}

	static void ReadXAnimPartTrans(const Task& task, std::uint8_t* dest, std::size_t size)
	{
		if (size != 8)
		{
			Fail(std::format("an XAnimPartTrans header read of {}", size));
		}

		std::uint8_t src[4];
		Consume(4, src);
		std::memset(dest, 0, 8);
		std::memcpy(dest, src, 3);
		const auto frameSize = U16(src);

		Task next;
		next.field = task.field;

		if (frameSize)
		{
			next.kind = TaskKind::XAnimTransFrames;
			next.context = task.context;
			next.count = frameSize;
		}
		else
		{
			next.kind = TaskKind::Raw;
			next.count = 12;
		}

		reader.continuations[reinterpret_cast<std::uintptr_t>(dest) + 8] = next;
	}

	static void ReadXAnimTransFrames(const Task& task, std::uint8_t* dest, std::size_t size)
	{
		if (size != 32)
		{
			Fail(std::format("an XAnimPartTransFrames read of {}", size));
		}

		std::uint8_t src[28];
		Consume(28, src);
		std::memset(dest, 0, 32);
		std::memcpy(dest, src, 24);
		PutPointer(dest, 24, false, zoneTypeBytes1, src + 24, task.field, 0);

		Task next;
		next.kind = TaskKind::Raw;
		next.field = task.field;
		next.count = IndexBytes(task.context, task.count);
		reader.continuations[reinterpret_cast<std::uintptr_t>(dest) + 32] = next;
	}

	static void ReadXAnimQuat(const Task& task, std::uint8_t* dest, std::size_t size, bool isQuat)
	{
		if (size != 8)
		{
			Fail(std::format("an XAnimDeltaPartQuat header read of {}", size));
		}

		std::uint8_t src[4];
		Consume(4, src);
		std::memset(dest, 0, 8);
		std::memcpy(dest, src, 2);
		const auto frameSize = U16(src);

		Task next;
		next.field = task.field;

		if (frameSize)
		{
			next.kind = TaskKind::XAnimQuatFrames;
			next.context = task.context;
			next.count = frameSize;
		}
		else
		{
			next.kind = TaskKind::Raw;
			next.count = 4;

			if (isQuat)
			{
				next.count = 8;
			}
		}

		reader.continuations[reinterpret_cast<std::uintptr_t>(dest) + 8] = next;
	}

	static void ReadXAnimQuatFrames(const Task& task, std::uint8_t* dest, std::size_t size)
	{
		if (size != 8)
		{
			Fail(std::format("an XAnimDeltaPartQuat frames read of {}", size));
		}

		std::uint8_t src[4];
		Consume(4, src);
		PutPointer(dest, 0, false, zoneTypeBytes1, src, task.field, 0);

		Task next;
		next.kind = TaskKind::Raw;
		next.field = task.field;
		next.count = IndexBytes(task.context, task.count);
		reader.continuations[reinterpret_cast<std::uintptr_t>(dest) + 8] = next;
	}

	static void ReadMaterialTechnique(std::uint8_t* dest, std::size_t size)
	{
		if (size != 16)
		{
			Fail(std::format("a MaterialTechnique header read of {}", size));
		}

		std::uint8_t src[8];
		const auto addr = Consume(8, src);
		std::memset(dest, 0, 16);
		PutPointer(dest, 0, true, 0, src, noField, 0);
		std::memcpy(dest + 8, src + 4, 4);

		if (U16(src + 6))
		{
			Task passes;
			passes.kind = TaskKind::Passes;
			reader.continuations[reinterpret_cast<std::uintptr_t>(dest) + 16] = passes;
		}

		AddRegion(addr, 8, reinterpret_cast<std::uintptr_t>(dest), zoneTypeBytes1);
	}

	static void ReadWater(std::uint8_t* dest, std::size_t size)
	{
		if (size != 96)
		{
			Fail(std::format("a water_t read of {}", size));
		}

		std::uint8_t src[68];
		Consume(68, src);
		std::memset(dest, 0, 96);
		std::memcpy(dest, src, 4);
		std::memcpy(dest + 32, src + 12, 52);

		const auto stored = U32(src + 4);

		if (stored)
		{
			if (stored != inlineMarker32)
			{
				Fail("water_t H0 stored as an offset");
			}

			const auto saved = static_cast<std::uint32_t>(reader.saved.size());
			reader.saved.emplace_back();

			for (std::uint32_t part = 0; part < 2; ++part)
			{
				PutU64(dest + 8 + 8 * part, inlineMarker);
				Task task;
				task.kind = TaskKind::H0Part;
				task.context = part;
				task.saved = saved;
				AddPending(reinterpret_cast<std::uintptr_t>(dest) + 8 + 8 * part, task, inlineMarker);
			}
		}

		PutPointer(dest, 24, false, zoneTypeBytes4, src + 8, noField, 0);
		PutPointer(dest, 88, false, zoneTypeGfxImage, src + 64, noField, 0);
	}

	static void ReadH0Part(const Task& task, std::uint8_t* dest, std::size_t size)
	{
		auto& bytes = reader.saved[task.saved];

		if (bytes.empty())
		{
			bytes.resize(2 * size);
			Consume(2 * size, bytes.data());
		}

		for (std::size_t i = 0; i < size / 4; ++i)
		{
			std::memcpy(dest + 4 * i, bytes.data() + 8 * i + 4 * task.context, 4);
		}
	}

	static std::vector<std::uint8_t> SpeakerEntries(const std::uint8_t* speakerMap, std::uint64_t mapIndex)
	{
		const auto* base = speakerMap + 8 + 100 * mapIndex;
		const auto speakerCount = U32(base);

		if (speakerCount > 6)
		{
			Fail(std::format("a SpeakerMap channel map with {} speakers", speakerCount));
		}

		std::vector<std::uint8_t> entries;

		for (std::uint32_t i = 0; i < speakerCount; ++i)
		{
			const auto speaker = static_cast<std::int32_t>(U32(base + 4 + 16 * i));
			const auto levelCount = static_cast<std::int32_t>(U32(base + 4 + 16 * i + 4));

			if (levelCount < 0 || levelCount > 2 || speaker < 0 || speaker > 255)
			{
				Fail(std::format("SpeakerMap speaker {} with {} levels", speaker, levelCount));
			}

			for (std::int32_t level = 0; level < levelCount; ++level)
			{
				entries.push_back(static_cast<std::uint8_t>(level));
				entries.push_back(static_cast<std::uint8_t>(speaker));
				entries.push_back(0);
				entries.push_back(0);
				const auto* value = base + 4 + 16 * i + 8 + 4 * level;
				entries.insert(entries.end(), value, value + 4);
			}
		}

		return entries;
	}

	static void ReadSpeakerMap(std::uint8_t* dest, std::size_t size)
	{
		if (size != 80)
		{
			Fail(std::format("a SpeakerMap read of {}", size));
		}

		const auto saved = static_cast<std::uint32_t>(reader.saved.size());
		reader.saved.emplace_back(408);
		auto* src = reader.saved.back().data();
		const auto addr = Consume(408, src);

		std::memset(dest, 0, 80);
		dest[0] = src[0];
		PutPointer(dest, 8, true, 0, src + 4, noField, 0);

		for (std::uint32_t mapIndex = 0; mapIndex < 4; ++mapIndex)
		{
			const auto count = SpeakerEntries(src, mapIndex).size() / 8;

			if (count > 255)
			{
				Fail(std::format("a SpeakerMap channel map with {} entries", count));
			}

			dest[16 + 16 * mapIndex] = static_cast<std::uint8_t>(count);

			if (count)
			{
				PutU64(dest + 24 + 16 * mapIndex, inlineMarker);
				Task task;
				task.kind = TaskKind::SpeakerEntries;
				task.context = mapIndex;
				task.saved = saved;
				AddPending(reinterpret_cast<std::uintptr_t>(dest) + 24 + 16 * mapIndex, task, inlineMarker);
			}
		}

		AddRegion(addr, 408, reinterpret_cast<std::uintptr_t>(dest), zoneTypeSpeakerMap);
	}

	static void FixIW4xRecord(std::uint16_t record, const std::uint8_t* raw, std::uint8_t* stock)
	{
		const auto version = Zones::Version();

		if (record == zoneRecordGfxImage)
		{
			auto& category = stock[Field(zoneRecordGfxImage, "category").offset32];

			if (category >= 9 && category <= 11)
			{
				category = static_cast<std::uint8_t>(category - 8);
			}

			if (version >= iw4xMaterialVersion && U32(raw + 44))
			{
				Fail("an IW4x image with a stored texture");
			}

			return;
		}

		if (record == zoneRecordMaterial)
		{
			const auto& info = Field(zoneRecordMaterial, "info");
			auto& sortKey = stock[info.offset32 + Field(zoneTypes[info.type].target, "sortKey").offset32];

			if (sortKey == 44)
			{
				sortKey = 43;
			}

			return;
		}

		if (record == zoneRecordMaterialTechniqueSet && U32(raw + 12))
		{
			Fail("an IW4x technique set with a remapped set");
		}

		if (record == zoneRecordFxElemDef)
		{
			auto& elemType = stock[Field(zoneRecordFxElemDef, "elemType").offset32];

			if (elemType == 3)
			{
				elemType = 2;
			}
			else if (elemType >= 5)
			{
				elemType = static_cast<std::uint8_t>(elemType - 2);
			}

			reader.fxElemStrings.push_back(U32(raw + 256));
			return;
		}

		if (record == zoneRecordXModel)
		{
			for (std::uint32_t lod = 0; lod < 4; ++lod)
			{
				reader.lodStrings.push_back(U32(raw + 72 + 56 * lod + 12));
			}

			return;
		}

		if (record == zoneRecordXSurface && version >= iw4xSurfaceHeaderVersion)
		{
			if (!(raw[2] & 0x20))
			{
				Fail("an IW4x surface without flag 0x20");
			}

			stock[6] = raw[3];
			std::memcpy(stock + 2, raw + 4, 2);
			std::memcpy(stock + 4, raw + 6, 2);
			return;
		}

		if (record == zoneRecordGameWorldSp)
		{
			if (version >= iw4xPathDataGoneVersion)
			{
				std::memset(stock + 4, 0, 40);
				return;
			}

			auto& tail = reader.pathTails.emplace_back();
			std::memcpy(tail.data(), raw + 44, tail.size());
			return;
		}

		if (record == zoneRecordGfxWorld)
		{
			const std::uint32_t distortion = 43;
			std::memcpy(stock + Field(zoneRecordGfxWorld, "sortKeyDistortion").offset32, &distortion, sizeof(distortion));
			reader.sunMaterials.push_back({ U32(raw + 264), U32(raw + 268) });
		}
	}

	static void ProduceIW4xRecords(const IW4xRule& rule, const Task& task, std::uint8_t* dest, std::uint32_t count)
	{
		const auto record = zoneTypes[task.type].target;
		const auto stockSize = Record(record).size32;
		const auto s64 = Stride64(task.type);

		reader.scratch.resize(static_cast<std::size_t>(count) * (rule.size32 + stockSize));
		auto* raw = reader.scratch.data();
		auto* stock = raw + static_cast<std::size_t>(count) * rule.size32;
		const auto addr = Consume(static_cast<std::size_t>(count) * rule.size32, raw);
		std::memset(stock, 0, static_cast<std::size_t>(count) * stockSize);
		std::memset(dest, 0, static_cast<std::size_t>(count) * s64);

		for (std::uint32_t i = 0; i < count; ++i)
		{
			const auto* element = raw + i * rule.size32;
			auto* out = stock + i * stockSize;

			for (const auto& segment : rule.segments)
			{
				if (segment.from != zeroSegment)
				{
					std::memcpy(out + segment.to, element + segment.from, segment.length);
				}
			}

			FixIW4xRecord(record, element, out);
		}

		for (std::uint32_t i = 0; i < count; ++i)
		{
			ConvertRecord(record, stock + i * stockSize, dest + i * s64, task.context);
		}

		AddRegion(addr, count * rule.size32, reinterpret_cast<std::uintptr_t>(dest), task.type, &rule);
		RunDeferred(task.fieldAddr, stock, count);
	}

	static void ProduceRecords(const Task& task, std::uint8_t* dest, std::size_t size)
	{
		const auto& t = zoneTypes[task.type];

		if (t.kind == ZoneKind::Record)
		{
			if (t.target == zoneRecordGfxImageLoadDef)
			{
				ReadGfxImageLoadDef(task, dest, size);
				return;
			}

			if (t.target == zoneRecordXAnimPartTrans)
			{
				ReadXAnimPartTrans(task, dest, size);
				return;
			}

			if (t.target == zoneRecordXAnimDeltaPartQuat2 || t.target == zoneRecordXAnimDeltaPartQuat)
			{
				ReadXAnimQuat(task, dest, size, t.target == zoneRecordXAnimDeltaPartQuat);
				return;
			}

			if (t.target == zoneRecordMaterialTechnique)
			{
				ReadMaterialTechnique(dest, size);
				return;
			}

			if (t.target == zoneRecordWater_t)
			{
				ReadWater(dest, size);
				return;
			}

			if (t.target == zoneRecordSpeakerMap)
			{
				ReadSpeakerMap(dest, size);
				return;
			}
		}

		const auto s32 = Stride32(task.type);
		const auto s64 = Stride64(task.type);

		if (size % s64)
		{
			Fail(std::format("a read of {} is not whole elements of {} for {}", size, s64, FieldName(task.field)));
		}

		const auto count = static_cast<std::uint32_t>(size / s64);
		const auto size32 = count * s32;

		if (const auto* rule = RuleFor(task.type))
		{
			ProduceIW4xRecords(*rule, task, dest, count);
			return;
		}

		if (IsPlain(task.type))
		{
			const auto addr = Consume(size32, dest);
			AddRegion(addr, size32, reinterpret_cast<std::uintptr_t>(dest), task.type);
			RunDeferred(task.fieldAddr, dest, count);
			return;
		}

		reader.scratch.resize(size32);
		const auto addr = Consume(size32, reader.scratch.data());
		std::memset(dest, 0, size);

		for (std::uint32_t i = 0; i < count; ++i)
		{
			ConvertValue(task.type, reader.scratch.data() + i * s32, dest + i * s64, task.field, task.context);
		}

		AddRegion(addr, size32, reinterpret_cast<std::uintptr_t>(dest), task.type);
		RunDeferred(task.fieldAddr, reader.scratch.data(), count);
	}

	static void Produce(const Task& task, std::uint8_t* dest, std::size_t size)
	{
		const auto destAddr = reinterpret_cast<std::uintptr_t>(dest);

		if (task.kind == TaskKind::String)
		{
			if (size != 1)
			{
				const auto addr = Consume(size, dest);
				AddRegion(addr, static_cast<std::uint32_t>(size), destAddr, zoneTypeBytes1);
				return;
			}

			const auto addr = Consume(1, dest);

			if (dest[0] == 0)
			{
				AddRegion(addr, 1, destAddr, zoneTypeBytes1);
				return;
			}

			reader.isInString = true;
			reader.stringNext = destAddr + 1;
			reader.stringStart = addr;
			reader.stringBegin = destAddr;
			return;
		}

		if (task.kind == TaskKind::Data)
		{
			ProduceRecords(task, dest, size);
			return;
		}

		if (task.kind == TaskKind::Raw)
		{
			if (size != task.count)
			{
				Fail(std::format("a raw read of {}, expected {}", size, task.count));
			}

			const auto addr = Consume(size, dest);
			AddRegion(addr, static_cast<std::uint32_t>(size), destAddr, zoneTypeBytes1);
			return;
		}

		if (task.kind == TaskKind::LoadDefData)
		{
			Consume(size, dest);
			return;
		}

		if (task.kind == TaskKind::Passes)
		{
			Task passes = task;
			passes.kind = TaskKind::Data;
			passes.type = zoneTypeMaterialPass;
			ProduceRecords(passes, dest, size);
			return;
		}

		if (task.kind == TaskKind::H0Part)
		{
			ReadH0Part(task, dest, size);
			return;
		}

		if (task.kind == TaskKind::SpeakerEntries)
		{
			const auto entries = SpeakerEntries(reader.saved[task.saved].data(), task.context);

			if (entries.size() != size)
			{
				Fail(std::format("a SpeakerMap channel map of {} bytes, x64 asks {}", entries.size(), size));
			}

			std::memcpy(dest, entries.data(), size);
			return;
		}

		if (task.kind == TaskKind::XAnimTransFrames)
		{
			ReadXAnimTransFrames(task, dest, size);
			return;
		}

		ReadXAnimQuatFrames(task, dest, size);
	}

	static void Dump(const void* data, std::size_t size)
	{
		if (!reader.isDumping)
		{
			return;
		}

		const auto* bytes = static_cast<const std::uint8_t*>(data);
		reader.served.insert(reader.served.end(), bytes, bytes + size);
	}

	static void ServeRead(void* destination, int size)
	{
		auto* dest = static_cast<std::uint8_t*>(destination);
		const auto destAddr = reinterpret_cast<std::uintptr_t>(dest);
		CheckInBlock(dest, size);

		if (reader.isInString && destAddr == reader.stringNext && size == 1)
		{
			Consume(1, dest);

			if (dest[0] == 0)
			{
				AddRegion(reader.stringStart, static_cast<std::uint32_t>(destAddr + 1 - reader.stringBegin), reader.stringBegin, zoneTypeBytes1);
				reader.isInString = false;
			}
			else
			{
				reader.stringNext = destAddr + 1;
			}

			Dump(dest, 1);
			return;
		}

		Task task;

		if (!TakeContinuation(destAddr, task) && !TakePending(destAddr, task))
		{
			Fail(std::format("nothing asked for a read of {} at block {}", size, StreamPosIndex()));
		}

		reader.isInString = false;
		Produce(task, dest, size);
		CloseBatch();
		Dump(dest, size);
	}

	static std::int64_t DB_AllocStreamPos_Hk(int align)
	{
		ApplyAlign();
		reader.isAlignPending = true;
		reader.alignIndex = reader.index;
		reader.align = static_cast<std::uint32_t>(align);

		auto& pos = StreamPos();
		const auto mask = static_cast<std::uint64_t>(static_cast<std::int64_t>(~align));
		pos = (pos + static_cast<std::int64_t>(align)) & mask;
		return static_cast<std::int64_t>(pos);
	}

	static std::int64_t DB_IncStreamPos_Hk(int size)
	{
		reader.isAlignPending = false;
		reader.pos += reader.consumed;
		reader.consumed = 0;

		StreamPos() += static_cast<std::int64_t>(size);
		return size;
	}

	static void DB_PushStreamPos_Hk(std::uint32_t block)
	{
		ApplyAlign();
		const auto saved = reader.index;
		ShadowSetIndex(block);
		reader.stack.push_back({ reader.pos, saved });

		const auto depth = StreamPosStackIndex();
		StackIndex(depth) = StreamPosIndex();
		StreamPosStackIndex() = depth + 1;
		SetStreamIndex(block);
		StackPos(depth) = StreamPos();
	}

	static void Load_GfxTextureLoad_PushStreamPos_Hk(std::uint32_t block)
	{
		std::uint32_t pushed = block;

		if (Zones::Version() >= iw4xImageBlockVersion)
		{
			pushed = 3;
		}

		DB_PushStreamPos_Hk(pushed);
	}

	static void DB_PopStreamPos_Hk()
	{
		ApplyAlign();

		if (reader.stack.empty())
		{
			Fail("the stream position stack is empty");
		}

		const auto entry = reader.stack.back();
		reader.stack.pop_back();

		if (reader.index == 0)
		{
			reader.pos = entry.pos;
		}

		ShadowSetIndex(entry.index);

		const auto depth = StreamPosStackIndex() - 1;
		StreamPosStackIndex() = depth;

		if (StreamPosIndex() == 0)
		{
			StreamPos() = StackPos(depth);
		}

		SetStreamIndex(StackIndex(depth));
	}

	static std::uint64_t DB_InsertPointer_Hk()
	{
		ApplyAlign();
		const auto saved = reader.index;
		ShadowSetIndex(3);
		const auto slot32 = (reader.pos + 3) & ~3u;
		reader.pos = slot32 + 4;
		ShadowSetIndex(saved);

		const auto depth = StreamPosStackIndex();
		StackIndex(depth) = StreamPosIndex();
		SetStreamIndex(3);
		const auto pos = StreamPos();
		StackPos(depth) = pos;
		const auto slot = (pos + 7) & ~7ull;
		auto next = slot + 8;

		if (StreamPosIndex() == 0)
		{
			next = pos;
		}

		StreamPos() = next;
		CheckInBlock(reinterpret_cast<void*>(slot), 8);
		SetStreamIndex(StackIndex(depth));

		reader.slots[(3u << 28) | slot32] = slot;
		return slot;
	}

	static std::uint32_t DecodeOffset(const std::uint64_t* field)
	{
		const auto value = *field;

		if (value >> 32)
		{
			Fail(std::format("an offset field holds {:#x}, not a 32 bit offset", value));
		}

		return static_cast<std::uint32_t>(value - 1);
	}

	alignas(16) static std::uint8_t sharedZeros[0x10000];

	static std::uintptr_t MapSharedZeros(std::uintptr_t fieldAddr, std::uint32_t addr)
	{
		const auto pending = reader.pendingAt.find(fieldAddr);

		if (pending == reader.pendingAt.end())
		{
			return 0;
		}

		const auto& entry = reader.batches[pending->second.first].entries[pending->second.second];

		if (!entry.isLive || entry.task.kind != TaskKind::Data)
		{
			return 0;
		}

		auto it = reader.regions.upper_bound(addr);

		if (it == reader.regions.begin())
		{
			return 0;
		}

		--it;
		const auto start = it->first;
		const auto& region = it->second;

		if (region.rule || addr < start || addr >= start + region.size32)
		{
			return 0;
		}

		if (zoneTypes[entry.task.type].kind != ZoneKind::Pointer || zoneTypes[region.type].kind != ZoneKind::Bytes)
		{
			return 0;
		}

		const auto want32 = Stride32(entry.task.type);
		const auto want64 = Stride64(entry.task.type);
		const auto have32 = Stride32(region.type);
		const auto have64 = Stride64(region.type);

		if (have32 != have64 || have32 < 2 || want64 * have32 <= have64 * want32)
		{
			return 0;
		}

		const auto span32 = start + region.size32 - addr;
		const auto need64 = span32 / want32 * want64;

		if (span32 % want32 || (addr - start) % have32 || need64 > sizeof(sharedZeros))
		{
			Fail(std::format("an offset to 32 bit {:#x} shares {} bytes with another type, in a span this reader cannot split", addr, span32));
		}

		const auto* const image = reinterpret_cast<const std::uint8_t*>(MapShadow(addr));
		const auto imageSize = span32 / have32 * have64;

		const bool isZero = std::all_of(image, image + imageSize, [](const std::uint8_t byte)
		{
			return byte == 0;
		});

		if (!isZero)
		{
			Fail(std::format("an offset to 32 bit {:#x} shares {} bytes with another type that are not all zero", addr, span32));
		}

		return reinterpret_cast<std::uintptr_t>(sharedZeros);
	}

	static void DB_ConvertOffsetToPointer_Hk(std::uint64_t* field)
	{
		const auto addr = DecodeOffset(field);
		auto mapped = MapSharedZeros(reinterpret_cast<std::uintptr_t>(field), addr);

		if (!mapped)
		{
			mapped = MapShadow(addr);
		}

		if (!mapped)
		{
			Fail(std::format("an offset to 32 bit {:#x} maps to nothing loaded", addr));
		}

		*field = mapped;
		DropPending(reinterpret_cast<std::uintptr_t>(field));
	}

	static void DB_ConvertOffsetToAlias_Hk(std::uint64_t* field)
	{
		const auto addr = DecodeOffset(field);
		std::uintptr_t mapped = 0;
		const auto slot = reader.slots.find(addr);

		if (slot != reader.slots.end())
		{
			mapped = slot->second;
		}
		else
		{
			mapped = MapShadow(addr);
		}

		if (!mapped)
		{
			Fail(std::format("an alias to 32 bit {:#x} maps to nothing loaded", addr));
		}

		*field = GetU64(mapped);
		DropPending(reinterpret_cast<std::uintptr_t>(field));
	}

	static std::int64_t Load_Stream_Hk(bool atStreamStart, void* ptr, int size)
	{
		if (!atStreamStart)
		{
			return 0;
		}

		const auto dest = reinterpret_cast<std::uintptr_t>(ptr);

		if (!size)
		{
			Task task;
			TakePending(dest, task);
			return 0;
		}

		if (StreamPosIndex() == 2)
		{
			CheckInBlock(ptr, size);
			Task task;

			if (!TakeContinuation(dest, task) && !TakePending(dest, task))
			{
				Fail(std::format("nothing asked for a zeroed read of {}", size));
			}

			auto type = task.type;

			if (task.kind == TaskKind::String)
			{
				type = zoneTypeBytes1;
			}

			const auto size32 = static_cast<std::uint32_t>(size / Stride64(type) * Stride32(type));
			const auto addr = Zeroed(size32);
			AddRegion(addr, size32, dest, type);

			std::memset(ptr, 0, size);
			return DB_IncStreamPos_Hk(size);
		}

		ServeRead(ptr, size);
		return DB_IncStreamPos_Hk(size);
	}

	static void DB_ReadXFile_String_Hk(void* dest, int size, [[maybe_unused]] int flags)
	{
		ServeRead(dest, size);
	}

	static const std::int32_t imaIndexTable[16] = { -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8 };
	static const std::int32_t imaStepTable[89] =
	{
		7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
		107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
		876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
		4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
		22385, 24623, 27086, 29794, 32767,
	};

	static std::int16_t ImaSample(std::int32_t& predictor, std::int32_t& stepIndex, std::uint8_t nibble)
	{
		const auto step = imaStepTable[stepIndex];
		std::int32_t delta = step >> 3;

		if (nibble & 4)
		{
			delta += step;
		}

		if (nibble & 2)
		{
			delta += step >> 1;
		}

		if (nibble & 1)
		{
			delta += step >> 2;
		}

		if (nibble & 8)
		{
			predictor -= delta;
		}
		else
		{
			predictor += delta;
		}

		predictor = std::clamp(predictor, -32768, 32767);
		stepIndex = std::clamp(stepIndex + imaIndexTable[nibble], 0, 88);
		return static_cast<std::int16_t>(predictor);
	}

	static std::vector<std::int16_t> DecodeIma(const std::uint8_t* data, std::uint32_t length, std::uint32_t channels, std::uint32_t blockAlign)
	{
		std::vector<std::int16_t> out;
		const auto framesPerBlock = (blockAlign - 4 * channels) * 2 / channels + 1;

		for (std::uint32_t offset = 0; offset + 4 * channels <= length; offset += blockAlign)
		{
			const auto blockLength = std::min(blockAlign, length - offset);
			const auto* block = data + offset;
			std::int32_t predictor[2]{};
			std::int32_t stepIndex[2]{};
			std::vector<std::int16_t> frames(static_cast<std::size_t>(framesPerBlock) * channels);

			for (std::uint32_t c = 0; c < channels; ++c)
			{
				predictor[c] = static_cast<std::int16_t>(U16(block + 4 * c));
				stepIndex[c] = std::clamp(static_cast<std::int32_t>(block[4 * c + 2]), 0, 88);
				frames[c] = static_cast<std::int16_t>(predictor[c]);
			}

			std::uint32_t decoded = 1;
			std::uint32_t at = 4 * channels;

			while (at + 4 * channels <= blockLength && decoded < framesPerBlock)
			{
				for (std::uint32_t c = 0; c < channels; ++c)
				{
					const auto* run = block + at + 4 * c;

					for (std::uint32_t i = 0; i < 8 && decoded + i < framesPerBlock; ++i)
					{
						const auto nibble = static_cast<std::uint8_t>((run[i / 2] >> (4 * (i % 2))) & 0xF);
						frames[(decoded + i) * channels + c] = ImaSample(predictor[c], stepIndex[c], nibble);
					}
				}

				decoded += 8;
				at += 4 * channels;
			}

			decoded = std::min(decoded, framesPerBlock);
			out.insert(out.end(), frames.begin(), frames.begin() + static_cast<std::size_t>(decoded) * channels);
		}

		return out;
	}

	static void Load_SetSoundData_Hk(const void** dataField, std::uint8_t* sound)
	{
		const auto it = reader.adpcmSounds.find(reinterpret_cast<std::uintptr_t>(sound));

		if (it == reader.adpcmSounds.end())
		{
			reinterpret_cast<void(*)(const void**, std::uint8_t*)>(Utils::Hook::Rebase(Load_SetSoundData))(dataField, sound);
			return;
		}

		reader.adpcmSounds.erase(it);

		const auto channels = U16(sound + 2);
		const auto rate = U32(sound + 4);
		const auto blockAlign = U16(sound + 12);
		const auto length = U32(sound + 24);
		const auto samples = U32(sound + 28);
		auto pcm = DecodeIma(static_cast<const std::uint8_t*>(*dataField), length, channels, blockAlign);
		const std::size_t frames = pcm.size() / channels;

		if (samples && samples < frames)
		{
			pcm.resize(static_cast<std::size_t>(samples) * channels);
		}

		const auto bytes = static_cast<std::uint32_t>(pcm.size() * sizeof(std::int16_t));
		auto* buffer = reinterpret_cast<void*(*)(std::uint32_t)>(Utils::Hook::Rebase(Z_MallocInternal))(bytes);
		std::memcpy(buffer, pcm.data(), bytes);

		const std::uint16_t format = 1;
		const std::uint16_t pcmBlockAlign = static_cast<std::uint16_t>(channels * 2);
		const std::uint16_t bits = 16;
		const std::uint32_t averageBytes = rate * pcmBlockAlign;
		const std::uint32_t zero = 0;
		std::memcpy(sound, &format, 2);
		std::memcpy(sound + 8, &averageBytes, 4);
		std::memcpy(sound + 12, &pcmBlockAlign, 2);
		std::memcpy(sound + 14, &bits, 2);
		std::memcpy(sound + 24, &bytes, 4);
		std::memcpy(sound + 28, &zero, 4);
		PutU64(sound + 48, reinterpret_cast<std::uint64_t>(buffer));
	}

	static std::uint32_t TakeQueued(std::deque<std::uint32_t>& queue, const char* what)
	{
		if (queue.empty())
		{
			Fail(std::format("an IW4x {} has no queued value", what));
		}

		const auto value = queue.front();
		queue.pop_front();
		return value;
	}

	static void SkipString()
	{
		std::uint8_t byte = 0;

		do
		{
			Consume(1, &byte);
		}
		while (byte != 0);

		reader.pos += reader.consumed;
		reader.consumed = 0;
	}

	static void SkipBytes(std::uint32_t size)
	{
		std::vector<std::uint8_t> bytes(size);
		Consume(size, bytes.data());
		reader.pos += reader.consumed;
		reader.consumed = 0;
	}

	static void Load_FxElemExtendedDefPtr_Hk(bool atStreamStart)
	{
		reinterpret_cast<void(*)(bool)>(Utils::Hook::Rebase(Load_FxElemExtendedDefPtr))(atStreamStart);

		if (TakeQueued(reader.fxElemStrings, "effect element string") == inlineMarker32)
		{
			SkipString();
		}
	}

	static void Load_XModelSurfsFixup_Hk(void* surfs, void* lodInfo)
	{
		if (TakeQueued(reader.lodStrings, "model lod string") == inlineMarker32)
		{
			SkipString();
		}

		reinterpret_cast<void(*)(void*, void*)>(Utils::Hook::Rebase(Load_XModelSurfsFixup))(surfs, lodInfo);
	}

	static void Load_PathData_Hk(bool atStreamStart)
	{
		reinterpret_cast<void(*)(bool)>(Utils::Hook::Rebase(Load_PathData))(atStreamStart);

		if (Zones::Version() >= iw4xPathDataGoneVersion)
		{
			return;
		}

		if (reader.pathTails.empty())
		{
			Fail("an IW4x path data tail has no queued value");
		}

		const auto tail = reader.pathTails.front();
		reader.pathTails.pop_front();

		for (const auto& [countAt, pointerAt] : { std::pair{ 0, 4 }, std::pair{ 8, 12 }, std::pair{ 20, 24 } })
		{
			if (U32(tail.data() + pointerAt))
			{
				SkipBytes(U32(tail.data() + countAt));
			}
		}
	}

	static void Load_MaterialHandle_Flare_Hk(bool atStreamStart)
	{
		reinterpret_cast<void(*)(bool)>(Utils::Hook::Rebase(Load_MaterialHandle))(atStreamStart);

		if (Zones::Version() < iw4xMaterialVersion)
		{
			return;
		}

		if (reader.sunMaterials.empty())
		{
			Fail("an IW4x sun has no queued materials");
		}

		const auto materials = reader.sunMaterials.front();
		reader.sunMaterials.pop_front();

		for (const auto stored : materials)
		{
			if (!stored)
			{
				continue;
			}

			auto& slot = reader.driveSlots.emplace_back(0);
			slot = PointerValue(reinterpret_cast<std::uint8_t*>(&slot), false, zoneTypeMaterial, stored, noField, 0);
			*reinterpret_cast<std::uint64_t**>(Utils::Hook::Rebase(varMaterialHandle)) = &slot;
			reinterpret_cast<void(*)(bool)>(Utils::Hook::Rebase(Load_MaterialHandle))(false);
		}
	}

	enum class WeaponOpKind : std::uint8_t
	{
		XString,
		XModel,
		Fx,
		Material,
		PhysCollmap,
		PhysPreset,
		Tracer,
		XStringArray,
		Sound,
		SoundArray,
		Vec2,
	};

	struct WeaponOp
	{
		WeaponOpKind kind;
		std::uint16_t offset;
		std::uint16_t extra;
	};

	static std::vector<WeaponOp> WeaponOps(std::uint32_t version)
	{
		std::vector<WeaponOp> ops;
		std::uint16_t shift = 0;

		const auto at = [&ops, &shift](WeaponOpKind kind, std::uint32_t offset)
		{
			ops.push_back({ kind, static_cast<std::uint16_t>(offset + shift), 0 });
		};

		const auto with = [&ops](WeaponOpKind kind, std::uint32_t offset, std::uint32_t extra)
		{
			ops.push_back({ kind, static_cast<std::uint16_t>(offset), static_cast<std::uint16_t>(extra) });
		};

		for (const std::uint32_t offset : { 0, 4, 8, 12 })
		{
			at(WeaponOpKind::XString, offset);
		}

		at(WeaponOpKind::XModel, 16);

		if (version >= iw4xMaterialVersion)
		{
			for (std::uint32_t offset = 20; offset < 57; offset += 4)
			{
				at(WeaponOpKind::XModel, offset);
			}

			for (const std::uint32_t base : { 124, 332, 540 })
			{
				with(WeaponOpKind::XStringArray, base, 52);
			}

			at(WeaponOpKind::Fx, 908);
			at(WeaponOpKind::Fx, 912);

			for (std::uint32_t i = 0; i < 52; ++i)
			{
				at(WeaponOpKind::Sound, 916 + 4 * i);
			}

			with(WeaponOpKind::SoundArray, 1128, 0);
			with(WeaponOpKind::SoundArray, 1132, 0);

			for (std::uint32_t offset = 1136; offset < 1149; offset += 4)
			{
				at(WeaponOpKind::Fx, offset);
			}

			for (const std::uint32_t offset : { 1152, 1156, 1372, 1376, 1380, 1384, 1388, 1392, 1400, 1408 })
			{
				at(WeaponOpKind::Material, offset);
			}

			for (const std::uint32_t offset : { 1428, 1436, 1452 })
			{
				at(WeaponOpKind::XString, offset);
			}

			for (std::uint32_t offset = 1716; offset < 1729; offset += 4)
			{
				at(WeaponOpKind::Material, offset);
			}

			with(WeaponOpKind::PhysCollmap, 1928, 0);
			with(WeaponOpKind::PhysPreset, 1932, 0);
			at(WeaponOpKind::XModel, 2020);
			at(WeaponOpKind::Fx, 2028);
			at(WeaponOpKind::Fx, 2032);
			at(WeaponOpKind::Sound, 2036);
			at(WeaponOpKind::Sound, 2040);

			for (const std::uint32_t offset : { 2304, 2308, 2336 })
			{
				at(WeaponOpKind::Fx, offset);
			}

			at(WeaponOpKind::Sound, 2340);
			at(WeaponOpKind::XString, 2516);
			with(WeaponOpKind::Vec2, 2524, 3044);
			at(WeaponOpKind::XString, 2520);
			with(WeaponOpKind::Vec2, 2528, 3046);

			for (const std::uint32_t offset : { 2608, 2612, 2644, 2648, 2772, 2776 })
			{
				at(WeaponOpKind::XString, offset);
			}

			with(WeaponOpKind::Tracer, 2780, 0);
			at(WeaponOpKind::Sound, 2808);
			at(WeaponOpKind::Fx, 2812);
			at(WeaponOpKind::XString, 2816);
			at(WeaponOpKind::Sound, 2832);

			for (std::uint32_t offset = 2836; offset < 2868; offset += 4)
			{
				at(WeaponOpKind::Sound, offset);
			}

			at(WeaponOpKind::Sound, 2868);
			at(WeaponOpKind::Sound, 2872);

			for (std::uint32_t i = 0; i < 6; ++i)
			{
				at(WeaponOpKind::Sound, 2940 + 4 * i);
			}

			for (const std::uint32_t offset : { 2988, 3000, 3004 })
			{
				at(WeaponOpKind::XString, offset);
			}

			for (const std::uint32_t offset : { 3012, 3016, 3020 })
			{
				at(WeaponOpKind::Material, offset);
			}

			with(WeaponOpKind::Vec2, 3048, 3044);
			with(WeaponOpKind::Vec2, 3052, 3046);
			return ops;
		}

		for (std::uint32_t i = 0; i < 32; ++i)
		{
			at(WeaponOpKind::XModel, 20 + 4 * i);
		}

		for (std::uint32_t offset = 148; offset < 169; offset += 4)
		{
			at(WeaponOpKind::XModel, offset);
		}

		for (const std::uint32_t base : { 236, 428, 620 })
		{
			with(WeaponOpKind::XStringArray, base, 48);
		}

		at(WeaponOpKind::Fx, 972);
		at(WeaponOpKind::Fx, 976);

		for (std::uint32_t i = 0; i < 50; ++i)
		{
			at(WeaponOpKind::Sound, 980 + 4 * i);
		}

		if (version >= iw4xPathDataGoneVersion)
		{
			at(WeaponOpKind::Sound, 1184);
			at(WeaponOpKind::Sound, 1188);
			shift += 8;
		}

		with(WeaponOpKind::SoundArray, 1184 + shift, 0);
		with(WeaponOpKind::SoundArray, 1188 + shift, 0);

		for (std::uint32_t offset = 1192; offset < 1205; offset += 4)
		{
			at(WeaponOpKind::Fx, offset);
		}

		for (const std::uint32_t offset : { 1208, 1212, 1428, 1432, 1436, 1440, 1444, 1448, 1456, 1464 })
		{
			at(WeaponOpKind::Material, offset);
		}

		for (const std::uint32_t offset : { 1484, 1492, 1508 })
		{
			at(WeaponOpKind::XString, offset);
		}

		for (std::uint32_t offset = 1764; offset < 1777; offset += 4)
		{
			at(WeaponOpKind::Material, offset);
		}

		with(WeaponOpKind::PhysCollmap, 1964 + shift, 0);
		at(WeaponOpKind::XModel, 2052);
		at(WeaponOpKind::Fx, 2060);
		at(WeaponOpKind::Fx, 2064);
		at(WeaponOpKind::Sound, 2068);
		at(WeaponOpKind::Sound, 2072);

		for (const std::uint32_t offset : { 2336, 2340, 2368 })
		{
			at(WeaponOpKind::Fx, offset);
		}

		at(WeaponOpKind::Sound, 2372);
		at(WeaponOpKind::XString, 2548);

		std::uint32_t countA = 3040;
		std::uint32_t countB = 3042;

		if (version >= iw4xPathDataGoneVersion)
		{
			countA = 3076;
			countB = 3078;
		}

		with(WeaponOpKind::Vec2, 2556 + shift, countA + shift);
		at(WeaponOpKind::XString, 2552);
		with(WeaponOpKind::Vec2, 2560 + shift, countB + shift);

		for (const std::uint32_t offset : { 2640, 2644, 2676, 2680, 2804, 2808 })
		{
			at(WeaponOpKind::XString, offset);
		}

		with(WeaponOpKind::Tracer, 2812 + shift, 0);
		at(WeaponOpKind::Sound, 2840);
		at(WeaponOpKind::Fx, 2844);
		at(WeaponOpKind::XString, 2848);
		at(WeaponOpKind::Sound, 2864);

		for (std::uint32_t offset = 2868; offset < 2900; offset += 4)
		{
			at(WeaponOpKind::Sound, offset);
		}

		at(WeaponOpKind::Sound, 2900);
		at(WeaponOpKind::Sound, 2904);

		if (version >= iw4xPathDataGoneVersion)
		{
			for (std::uint32_t i = 0; i < 6; ++i)
			{
				at(WeaponOpKind::Sound, 2972 + 4 * i);
			}

			shift += 36;
		}

		for (const std::uint32_t offset : { 2984, 2996, 3000 })
		{
			at(WeaponOpKind::XString, offset);
		}

		for (const std::uint32_t offset : { 3008, 3012, 3016 })
		{
			at(WeaponOpKind::Material, offset);
		}

		with(WeaponOpKind::Vec2, 3044 + shift, 3040 + shift);
		with(WeaponOpKind::Vec2, 3048 + shift, 3042 + shift);
		return ops;
	}

	static std::uint32_t WeaponSize(std::uint32_t version)
	{
		if (version >= iw4xMaterialVersion)
		{
			return 3120;
		}

		if (version >= iw4xSurfaceHeaderVersion)
		{
			return 3068;
		}

		if (version >= iw4xPathDataGoneVersion)
		{
			return 3156;
		}

		return 3112;
	}

	struct WeaponLoader
	{
		std::uintptr_t var;
		std::uintptr_t loader;
		std::uint16_t targetType;
	};

	static WeaponLoader LoaderFor(WeaponOpKind kind)
	{
		switch (kind)
		{
		case WeaponOpKind::XString:
			return { varXString, Load_XString, 0 };
		case WeaponOpKind::XModel:
			return { varXModelPtr, Load_XModelPtr, zoneTypeXModel };
		case WeaponOpKind::Fx:
			return { varFxEffectDefHandle, Load_FxEffectDefHandle, zoneTypeFxEffectDef };
		case WeaponOpKind::Material:
			return { varMaterialHandle, Load_MaterialHandle, zoneTypeMaterial };
		case WeaponOpKind::PhysCollmap:
			return { varPhysCollmapPtr, Load_PhysCollmapPtr, zoneTypePhysCollmap };
		case WeaponOpKind::PhysPreset:
			return { varPhysPresetPtr, Load_PhysPresetPtr, zoneTypePhysPreset };
		default:
			return { varTracerDefPtr, Load_TracerDefPtr, zoneTypeTracerDef };
		}
	}

	static std::int64_t DB_AllocStreamPos_Hk(int align);
	static std::int64_t DB_IncStreamPos_Hk(int size);
	static void DB_PushStreamPos_Hk(std::uint32_t block);
	static void DB_PopStreamPos_Hk();
	static std::int64_t Load_Stream_Hk(bool atStreamStart, void* ptr, int size);

	static void LoadSndAliasCustom(std::uint64_t* slot)
	{
		*reinterpret_cast<std::uint64_t**>(Utils::Hook::Rebase(varsnd_alias_list_name)) = slot;
		reinterpret_cast<void(*)(std::uint64_t*)>(Utils::Hook::Rebase(Load_SndAliasCustom))(slot);
	}

	static void Load_WeaponCompleteDef_Hk(bool atStreamStart)
	{
		const auto version = Zones::Version();

		if (version < iw4xFirstVersion)
		{
			reinterpret_cast<void(*)(bool)>(Utils::Hook::Rebase(Load_WeaponCompleteDef))(atStreamStart);
			return;
		}

		if (!atStreamStart)
		{
			Fail("an IW4x weapon loaded in place");
		}

		if ((version >= iw4xSurfaceHeaderVersion && version < iw4xMaterialVersion) || version >= iw4xWeaponNextVersion)
		{
			Fail(std::format("an IW4x weapon at version {}, whose layout zw3 does not keep whole", version));
		}

		auto* const dest = *reinterpret_cast<std::uint8_t**>(Utils::Hook::Rebase(varWeaponCompleteDef));
		Task taken;
		TakePending(reinterpret_cast<std::uintptr_t>(dest), taken);

		std::vector<std::uint8_t> weapon(WeaponSize(version));
		const auto addr = Consume(weapon.size(), weapon.data());
		reader.pos += reader.consumed;
		reader.consumed = 0;

		std::memset(dest, 0, weaponCompleteDefSize64);
		DB_IncStreamPos_Hk(weaponCompleteDefSize64);
		DB_PushStreamPos_Hk(3);

		const auto soundType = Field(zoneRecordWeaponDef, "pickupSound").type;
		const auto soundTarget = zoneTypes[soundType].target;

		for (const auto& op : WeaponOps(version))
		{
			const auto stored = U32(weapon.data() + op.offset);

			if (op.kind == WeaponOpKind::XStringArray)
			{
				auto& slots = reader.driveArrays.emplace_back(std::make_unique<std::uint64_t[]>(op.extra));

				for (std::uint32_t i = 0; i < op.extra; ++i)
				{
					auto* const slot = &slots[i];
					*slot = PointerValue(reinterpret_cast<std::uint8_t*>(slot), true, 0, U32(weapon.data() + op.offset + 4 * i), noField, 0);
				}

				*reinterpret_cast<std::uint64_t**>(Utils::Hook::Rebase(varXString)) = slots.get();
				reinterpret_cast<void(*)(bool, int)>(Utils::Hook::Rebase(Load_XStringArray))(false, op.extra);
				continue;
			}

			if (op.kind == WeaponOpKind::Sound)
			{
				auto& slot = reader.driveSlots.emplace_back(0);
				slot = PointerValue(reinterpret_cast<std::uint8_t*>(&slot), false, soundTarget, stored, noField, 0);
				LoadSndAliasCustom(&slot);
				continue;
			}

			if (op.kind == WeaponOpKind::SoundArray || op.kind == WeaponOpKind::Vec2)
			{
				if (stored != inlineMarker32)
				{
					continue;
				}

				const bool isSoundArray = op.kind == WeaponOpKind::SoundArray;
				auto& cell = reader.driveSlots.emplace_back(inlineMarker);

				Task task;
				task.kind = TaskKind::Data;
				task.type = zoneTypeBytes4;

				if (isSoundArray)
				{
					task.type = soundType;
				}

				AddPending(reinterpret_cast<std::uintptr_t>(&cell), task, inlineMarker);

				auto* const data = reinterpret_cast<std::uint8_t*>(DB_AllocStreamPos_Hk(3));
				cell = reinterpret_cast<std::uint64_t>(data);

				int size64 = 8 * static_cast<std::int16_t>(U16(weapon.data() + op.extra));

				if (isSoundArray)
				{
					size64 = static_cast<int>(8 * weaponSoundArrayCount);
				}

				Load_Stream_Hk(true, data, size64);

				if (isSoundArray)
				{
					for (std::uint32_t i = 0; i < weaponSoundArrayCount; ++i)
					{
						LoadSndAliasCustom(reinterpret_cast<std::uint64_t*>(data) + i);
					}
				}

				continue;
			}

			const auto loader = LoaderFor(op.kind);
			auto& slot = reader.driveSlots.emplace_back(0);
			slot = PointerValue(reinterpret_cast<std::uint8_t*>(&slot), op.kind == WeaponOpKind::XString, loader.targetType, stored, noField, 0);
			AddRegion(addr + op.offset, 4, reinterpret_cast<std::uintptr_t>(&slot), zoneTypeBytes1);
			*reinterpret_cast<std::uint64_t**>(Utils::Hook::Rebase(loader.var)) = &slot;
			reinterpret_cast<void(*)(bool)>(Utils::Hook::Rebase(loader.loader))(false);

			if (op.offset == 0)
			{
				PutU64(dest, slot);
			}
		}

		DB_PopStreamPos_Hk();
	}

	static Utils::Hook* const iw4xHooks[] =
	{
		&fxElemExtendedHook, &surfsFixupHook, &pathDataHook, &flareMaterialHook, &weaponHooks[0], &weaponHooks[1],
	};

	static void SetImpactEntries(std::uint32_t count)
	{
		Utils::Hook::Set<std::uint32_t>(Load_FxImpactTable_ReadSize + 2, count * impactEntrySize64);
		Utils::Hook::Set<std::uint32_t>(Load_FxImpactTable_EntryCount + 1, count);
	}

	static Utils::Hook* const streamHooks[] =
	{
		&loadStreamHook, &allocStreamPosHook, &incStreamPosHook, &pushStreamPosHook, &popStreamPosHook,
		&insertPointerHook, &offsetToAliasHook, &offsetToPointerHook, &setSoundDataHook, &textureLoadPushHook,
		&stringReadHooks[0], &stringReadHooks[1], &stringReadHooks[2], &stringReadHooks[3],
	};

	static void Disarm()
	{
		for (auto* hook : streamHooks)
		{
			hook->Uninstall();
		}

		for (auto* hook : iw4xHooks)
		{
			hook->Uninstall();
		}

		if (isImpactPatched)
		{
			SetImpactEntries(impactEntriesStock);
			isImpactPatched = false;
		}

		xfileReadHook.Uninstall();
		assetListReadHook.Uninstall();

		if (reader.isDumping && !reader.served.empty())
		{
			const auto path = std::format("{}\\iw4x\\zones\\{}.served", (*Game::fs_basepath)->current.string, reader.zoneName);
			Utils::IO::WriteFile(path, std::string(reader.served.begin(), reader.served.end()));
		}

		reader = Reader{};
	}

	static std::uint32_t PlanBlock(std::uint32_t block, std::uint32_t size32)
	{
		if (!size32)
		{
			return 0;
		}

		if (block == 0)
		{
			return size32 + planSlack;
		}

		if (block == 3)
		{
			return size32 + size32 / 2 + planSlack;
		}

		if (block == 6 || block == 7)
		{
			return size32 + planSlack;
		}

		return 2 * size32 + planSlack;
	}

	static bool Is32BitAssetList(const std::uint8_t* list)
	{
		const auto strings = U32(list + 4);
		const auto assetCount = U32(list + 8);
		const auto assets = U32(list + 12);
		return assets == inlineMarker32 && assetCount != 0 && assetCount != inlineMarker32 && (strings == 0 || strings == inlineMarker32);
	}

	static bool Arm()
	{
		for (auto* hook : streamHooks)
		{
			if (!hook->Install()->IsInstalled())
			{
				return false;
			}
		}

		const auto version = Zones::Version();

		if (version < iw4xFirstVersion)
		{
			return true;
		}

		for (auto* hook : iw4xHooks)
		{
			if (!hook->Install()->IsInstalled())
			{
				return false;
			}
		}

		if (version < iw4xImpactFx16EndVersion)
		{
			SetImpactEntries(impactEntriesIW4x);
			isImpactPatched = true;
		}

		return true;
	}

	static void DB_ReadXFile_XFile_Hk(void* dest, int size, int flags)
	{
		reinterpret_cast<void(*)(void*, int, int)>(Utils::Hook::Rebase(DB_ReadXFile))(dest, size, flags);
		ReadXFile(reader.peek.data(), static_cast<int>(reader.peek.size()));
		reader.hasPeek = true;

		if (!Is32BitAssetList(reader.peek.data()))
		{
			return;
		}

		reader.isReading = true;
		reader.isDumping = Flags::HasFlag("zonesdump");

		if (!Arm())
		{
			Fail("its hooks could not be seated");
		}

		auto* blockSizes = static_cast<std::uint8_t*>(dest) + 8;

		for (std::uint32_t block = 0; block < blockCount; ++block)
		{
			const auto planned = PlanBlock(block, U32(blockSizes + 4 * block));
			std::memcpy(blockSizes + 4 * block, &planned, sizeof(planned));
		}

		Dump(dest, size);
	}

	static void DB_ReadXFile_AssetList_Hk(void* dest, int size, int flags)
	{
		if (!reader.hasPeek)
		{
			reinterpret_cast<void(*)(void*, int, int)>(Utils::Hook::Rebase(DB_ReadXFile))(dest, size, flags);
			return;
		}

		reader.hasPeek = false;

		if (!reader.isReading)
		{
			std::memcpy(dest, reader.peek.data(), reader.peek.size());
			const auto rest = size - static_cast<int>(reader.peek.size());
			reinterpret_cast<void(*)(void*, int, int)>(Utils::Hook::Rebase(DB_ReadXFile))(static_cast<std::uint8_t*>(dest) + reader.peek.size(), rest, flags);
			return;
		}

		std::memset(dest, 0, size);
		ConvertValue(zoneTypeXAssetList, reader.peek.data(), static_cast<std::uint8_t*>(dest), noField, 0);
		CloseBatch();
		Dump(dest, size);
	}

	static std::int64_t DB_LoadXFile_Hk(void* zoneMem, void* dbFile)
	{
		Disarm();

		reader.isCandidate = true;
		reader.zoneName = static_cast<const char*>(dbFile) + dbFileName;
		Logger::Print("Loading fastfile {}\n", reader.zoneName);
		xfileReadHook.Install();
		assetListReadHook.Install();

		const auto result = reinterpret_cast<std::int64_t(*)(void*, void*)>(Utils::Hook::Rebase(DB_LoadXFile))(zoneMem, dbFile);
		Disarm();
		return result;
	}

	bool Zones::IsReady()
	{
		return isReady;
	}

	static std::uint32_t zoneVersion = XFILE_VERSION;

	std::uint32_t Zones::Version()
	{
		return zoneVersion;
	}

	void Zones::SetVersion(const std::uint32_t version)
	{
		zoneVersion = version;
	}

	bool Zones::CanRead(const std::uint32_t version)
	{
		if (version == XFILE_VERSION)
		{
			return true;
		}

		return isReady && isIW4xReady && std::find(std::begin(iw4xReadVersions), std::end(iw4xReadVersions), version) != std::end(iw4xReadVersions);
	}

	static bool AreIW4xRulesWhole()
	{
		for (const auto& rule : iw4xRules)
		{
			const auto stockSize = Record(rule.record).size32;
			std::uint32_t copied = 0;

			for (const auto& segment : rule.segments)
			{
				const auto fromEnd = static_cast<std::uint32_t>(segment.from + segment.length);
				const auto toEnd = static_cast<std::uint32_t>(segment.to + segment.length);

				if ((segment.from != zeroSegment && fromEnd > rule.size32) || toEnd > stockSize)
				{
					return false;
				}

				copied += segment.length;
			}

			if (copied != stockSize)
			{
				return false;
			}
		}

		return true;
	}

	Zones::Zones()
	{
		recordHasPointer.assign(std::size(zoneRecords), -1);
		typeIsPlain.assign(std::size(zoneTypes), -1);

		using Utils::Hook;

		const bool areEntriesIntact = Hook::MatchesBytes(Load_Stream, load_StreamEntry, sizeof(load_StreamEntry))
			&& Hook::MatchesBytes(DB_AllocStreamPos, allocStreamPosEntry, sizeof(allocStreamPosEntry))
			&& Hook::MatchesBytes(DB_IncStreamPos, incStreamPosEntry, sizeof(incStreamPosEntry))
			&& Hook::MatchesBytes(DB_PushStreamPos, pushStreamPosEntry, sizeof(pushStreamPosEntry))
			&& Hook::MatchesBytes(DB_PopStreamPos, popStreamPosEntry, sizeof(popStreamPosEntry))
			&& Hook::MatchesBytes(DB_InsertPointer, insertPointerEntry, sizeof(insertPointerEntry))
			&& Hook::MatchesBytes(DB_ConvertOffsetToAlias, offsetToAliasEntry, sizeof(offsetToAliasEntry))
			&& Hook::MatchesBytes(DB_ConvertOffsetToPointer, offsetToPointerEntry, sizeof(offsetToPointerEntry));

		const bool areIW4xSitesIntact = Hook::BranchesTo(Load_FxElemDefArray_ExtendedCall, Load_FxElemExtendedDefPtr, HOOK_CALL)
			&& Hook::BranchesTo(Load_XModel_SurfsFixupCall, Load_XModelSurfsFixup, HOOK_CALL)
			&& Hook::BranchesTo(Load_GameWorldSp_PathDataCall, Load_PathData, HOOK_CALL)
			&& Hook::BranchesTo(Load_GfxWorld_FlareMaterialCall, Load_MaterialHandle, HOOK_CALL)
			&& Hook::BranchesTo(Load_WeaponCompleteDefPtr_Calls[0], Load_WeaponCompleteDef, HOOK_CALL)
			&& Hook::BranchesTo(Load_WeaponCompleteDefPtr_Calls[1], Load_WeaponCompleteDef, HOOK_CALL)
			&& Hook::MatchesBytes(Load_FxImpactTable_ReadSize, impactReadSizeStock, sizeof(impactReadSizeStock))
			&& Hook::MatchesBytes(Load_FxImpactTable_EntryCount, impactEntryCountStock, sizeof(impactEntryCountStock));

		bool areCallsIntact = Hook::BranchesTo(DB_Thread_LoadXFileCall, DB_LoadXFile, HOOK_CALL)
			&& Hook::BranchesTo(DB_LoadXFile_XFileRead, DB_ReadXFile, HOOK_CALL)
			&& Hook::BranchesTo(DB_LoadXFile_AssetListRead, DB_ReadXFile, HOOK_CALL)
			&& Hook::BranchesTo(Load_MssSound_SetSoundDataCall, Load_SetSoundData, HOOK_CALL)
			&& Hook::BranchesTo(Load_GfxTextureLoad_PushCall, DB_PushStreamPos, HOOK_CALL)
			&& Hook::MatchesBytes(Load_GfxTextureLoad_BlockZero, xorEcxEcx, sizeof(xorEcxEcx));

		for (const auto call : StringReadCalls)
		{
			areCallsIntact = areCallsIntact && Hook::BranchesTo(call, DB_ReadXFile, HOOK_CALL);
		}

		if (!areEntriesIntact || !areCallsIntact)
		{
			Logger::Error("zones: the zone loader does not read as expected, 32 bit zones will not load\n");
			return;
		}

		xfileReadHook.Initialize(DB_LoadXFile_XFileRead, reinterpret_cast<void*>(DB_ReadXFile_XFile_Hk), HOOK_CALL);
		assetListReadHook.Initialize(DB_LoadXFile_AssetListRead, reinterpret_cast<void*>(DB_ReadXFile_AssetList_Hk), HOOK_CALL);
		loadStreamHook.Initialize(Load_Stream, reinterpret_cast<void*>(Load_Stream_Hk), HOOK_JUMP);
		allocStreamPosHook.Initialize(DB_AllocStreamPos, reinterpret_cast<void*>(DB_AllocStreamPos_Hk), HOOK_JUMP);
		incStreamPosHook.Initialize(DB_IncStreamPos, reinterpret_cast<void*>(DB_IncStreamPos_Hk), HOOK_JUMP);
		pushStreamPosHook.Initialize(DB_PushStreamPos, reinterpret_cast<void*>(DB_PushStreamPos_Hk), HOOK_JUMP);
		popStreamPosHook.Initialize(DB_PopStreamPos, reinterpret_cast<void*>(DB_PopStreamPos_Hk), HOOK_JUMP);
		insertPointerHook.Initialize(DB_InsertPointer, reinterpret_cast<void*>(DB_InsertPointer_Hk), HOOK_JUMP);
		offsetToAliasHook.Initialize(DB_ConvertOffsetToAlias, reinterpret_cast<void*>(DB_ConvertOffsetToAlias_Hk), HOOK_JUMP);
		offsetToPointerHook.Initialize(DB_ConvertOffsetToPointer, reinterpret_cast<void*>(DB_ConvertOffsetToPointer_Hk), HOOK_JUMP);
		setSoundDataHook.Initialize(Load_MssSound_SetSoundDataCall, reinterpret_cast<void*>(Load_SetSoundData_Hk), HOOK_CALL);
		textureLoadPushHook.Initialize(Load_GfxTextureLoad_PushCall, reinterpret_cast<void*>(Load_GfxTextureLoad_PushStreamPos_Hk), HOOK_CALL);
		fxElemExtendedHook.Initialize(Load_FxElemDefArray_ExtendedCall, reinterpret_cast<void*>(Load_FxElemExtendedDefPtr_Hk), HOOK_CALL);
		surfsFixupHook.Initialize(Load_XModel_SurfsFixupCall, reinterpret_cast<void*>(Load_XModelSurfsFixup_Hk), HOOK_CALL);
		pathDataHook.Initialize(Load_GameWorldSp_PathDataCall, reinterpret_cast<void*>(Load_PathData_Hk), HOOK_CALL);
		flareMaterialHook.Initialize(Load_GfxWorld_FlareMaterialCall, reinterpret_cast<void*>(Load_MaterialHandle_Flare_Hk), HOOK_CALL);

		for (std::size_t i = 0; i < std::size(Load_WeaponCompleteDefPtr_Calls); ++i)
		{
			weaponHooks[i].Initialize(Load_WeaponCompleteDefPtr_Calls[i], reinterpret_cast<void*>(Load_WeaponCompleteDef_Hk), HOOK_CALL);
		}

		for (std::size_t i = 0; i < std::size(StringReadCalls); ++i)
		{
			stringReadHooks[i].Initialize(StringReadCalls[i], reinterpret_cast<void*>(DB_ReadXFile_String_Hk), HOOK_CALL);
		}

		if (!loadXFileHook.Initialize(DB_Thread_LoadXFileCall, reinterpret_cast<void*>(DB_LoadXFile_Hk), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("zones: could not hook DB_Thread's DB_LoadXFile call, 32 bit zones will not load\n");
			return;
		}

		isReady = true;
		isIW4xReady = AreIW4xRulesWhole();

		if (!isIW4xReady)
		{
			Logger::Error("zones: the IW4x record rules do not fit the zone layouts, IW4x's signed zones will not load\n");
		}

		if (isIW4xReady && !areIW4xSitesIntact)
		{
			isIW4xReady = false;
			Logger::Error("zones: the loader's IW4x sites do not read as expected, IW4x's signed zones will not load\n");
		}
	}
}
