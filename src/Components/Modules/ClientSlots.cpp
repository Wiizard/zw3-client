#include "STDInclude.hpp"

#include <bit>

#include "ClientSlots.hpp"
#include "Bots.hpp"
#include "Logger.hpp"
#include "ServerCommands.hpp"

constexpr std::size_t maskHighWords = Components::ClientSlots::CLIENT_MASK_WORDS - 1;

static_assert(maskHighWords == 3);

extern "C"
{
	std::uint8_t ClientSlots_snapshotDue[Components::ClientSlots::CLIENT_LIMIT];
	std::uint32_t ClientSlots_clientMaskHigh[Components::ClientSlots::ENTITY_LIMIT][maskHighWords];

	std::uint32_t ClientSlots_snapshotMaskHigh[Components::ClientSlots::CLIENT_LIMIT * 32 * 256][maskHighWords];
	std::uint32_t ClientSlots_baselineMaskHigh[1024][maskHighWords];
	std::uint8_t* ClientSlots_snapshotRingBegin;
	std::uint8_t* ClientSlots_snapshotRingEnd;
	std::uint8_t* ClientSlots_baselineBegin;
	std::uint8_t* ClientSlots_baselineEnd;
	int* ClientSlots_serverClientCount;
	std::uint8_t ClientSlots_clientIndexBits = 5;

	std::uint8_t ClientSlots_wideClientBits = static_cast<std::uint8_t>(std::bit_width(Components::ClientSlots::CLIENT_LIMIT));
	void* ClientSlots_buildItemClientMaskBody;
	void* ClientSlots_sessionGetXuid;
	void* ClientSlots_sessionIsRegistered;
	void* ClientSlots_partyRemovePlayer;
	void* ClientSlots_partyVoiceBits;
	void* ClientSlots_setConfigstring;
	void* ClientSlots_partyMemberAddr;
	void* ClientSlots_sessionRegister;
	void* ClientSlots_clientInMyParty;
	void* ClientSlots_playerMuted;
	void* ClientSlots_playerTalking;

	std::uint8_t* ClientSlots_cgameClientInfo;
	std::uint32_t ClientSlots_cgameClientCount = Components::ClientSlots::BASEGAME_CLIENT_LIMIT;

	std::uint32_t* ClientSlots_scriptChildCounts;
	std::uint32_t ClientSlots_scriptChild0Begin;
	void* ClientSlots_allocVariableBody;
	void* ClientSlots_initVariablesBody;
	void* ClientSlots_childPoolExhaustedBody;

	void ClientSlots_AllocVariable();
	void ClientSlots_ScrInitVariables();
	void ClientSlots_ScriptChildAddedEbx();
	void ClientSlots_ScriptChildFreedEbx();
	void ClientSlots_ScriptChildFreedEdi();
	void ClientSlots_ScriptPoolExhaustedEax();
	void ClientSlots_ScriptPoolExhaustedEcx();

	void ClientSlots_BuildItemClientMask();
	void ClientSlots_GuardSessionGetXuid();
	void ClientSlots_GuardSessionIsRegistered();
	void ClientSlots_GuardPartyRemovePlayer();
	void ClientSlots_GuardPartyVoiceBits();
	void ClientSlots_GuardPlayerInfo();
	void ClientSlots_GuardPartyMemberAddr();
	void ClientSlots_GuardSessionRegister();
	void ClientSlots_GuardClientInMyParty();
	void ClientSlots_GuardPlayerMuted();
	void ClientSlots_GuardPlayerTalking();
	void ClientSlots_SnapshotDueAddress();
	void ClientSlots_SnapshotDueSet();
	void ClientSlots_SnapshotDueTest();
	void ClientSlots_ServerIndexBitsR8();
	void ClientSlots_ServerIndexBitsStack();
	void ClientSlots_ServerIndexBitsEdx();
	void ClientSlots_ClientIndexBitsEdx();

	void ClientSlots_MaskLoad_1401627BA();
	void ClientSlots_MaskStore_1401627C4();
	void ClientSlots_MaskLoad_14018215E();
	void ClientSlots_MaskStore_140182169();
	void ClientSlots_MaskLoad_140197A13();
	void ClientSlots_MaskStore_140197A1E();
	void ClientSlots_MaskLoad_140197A8B();
	void ClientSlots_MaskStore_140197A95();
	void ClientSlots_MaskLoad_1401A3419();
	void ClientSlots_MaskStore_1401A3424();
	void ClientSlots_MaskLoad_1401A34BE();
	void ClientSlots_MaskStore_1401A34C8();
	void ClientSlots_MaskLoad_1401A3717();
	void ClientSlots_MaskStore_1401A3720();
	void ClientSlots_MaskAnd_1401629BD();
	void ClientSlots_MaskHideAll_1401629B3();
	void ClientSlots_MaskHideAll_140197A69();
	void ClientSlots_MaskHideAll_1401A33C9();
	void ClientSlots_MaskHideAll_1401A34A7();
	void ClientSlots_MaskHideAll_1401A3672();
	void ClientSlots_MaskOnlyFor_140167BD7();
	void ClientSlots_MaskHiddenTest_14023F34E();
	void ClientSlots_MaskForClient_140242864();
}

namespace Components
{
	enum class SlotArray
	{
		GClients,
		SvsClients,
		LevelBgs,
		SortedClients,
		SnapshotEntities,
		SnapshotClients,
		HudElems,
		CachedClients,
		CgameBgs,
		CgameClientMask,
		CompassActors,
		MotionTrackerPos,
		MotionTrackerPrev,
		OverheadFade,
		ParseClients,
		ActiveSnapshot0,
		ActiveSnapshot1,
		SnapshotData,
		ClientSkeletons,
		ServerSkeletons,
		Scores,
		Count,
	};

	enum class SiteBase
	{
		Rip,
		Svs,
		Image,
		Clients,
		Cg,
	};

	struct SlotArrayLayout
	{
		std::uintptr_t address;
		std::size_t stockSize;
		std::size_t newSize;
		std::size_t headerSize;
		std::size_t entrySize;
	};

	struct ImmediatePatch
	{
		std::uintptr_t address;
		std::uint8_t operandOffset;
		std::uint32_t stock;
		std::uint32_t raised;
	};

	struct BytePatch
	{
		std::uintptr_t address;
		std::array<std::uint8_t, 7> stock;
		std::array<std::uint8_t, 7> patched;
		std::size_t length;
	};

	struct SlotSite
	{
		std::uintptr_t address;
		std::uint8_t length;
		std::uint8_t operandOffset;
		SlotArray array;
		SiteBase base;
		std::int32_t offset;
	};

	constexpr std::uintptr_t imageBase = 0x140000000;
	constexpr std::uintptr_t svs = 0x1422CE700;

	constexpr std::uint32_t packetBackup = 32;
	constexpr std::uint32_t snapshotEntityCount = ClientSlots::CLIENT_LIMIT * packetBackup * 256;
	constexpr std::uint32_t snapshotClientCount = ClientSlots::CLIENT_LIMIT * ClientSlots::CLIENT_LIMIT * packetBackup;
	constexpr std::size_t snapshotEntitySize = 0x100;
	constexpr std::size_t snapshotClientSize = 0x7C;
	constexpr std::uintptr_t clientsStruct = 0x1406CEE40;
	constexpr std::uintptr_t cgArray = 0x1404769A0;
	constexpr std::uintptr_t cg_entities = 0x14059EB20;
	constexpr std::size_t centitySize = 0x220;

	constexpr std::size_t stockCompassActorCount = 18 + 8;
	constexpr std::size_t compassActorCount = (ClientSlots::CLIENT_LIMIT + 8 + 1) / 2 * 2;

	static_assert(ClientSlots::CLIENT_LIMIT <= 127 && compassActorCount / 2 <= 127);

	constexpr std::uint32_t stockParseClientCount = 18 * packetBackup;
	constexpr std::uint32_t parseClientCount = ClientSlots::CLIENT_LIMIT * packetBackup * 2;
	constexpr std::size_t snapshotClientsOffset = 0x33130;
	constexpr std::uint32_t stockCommandSequenceOffset = static_cast<std::uint32_t>(snapshotClientsOffset + 18 * snapshotClientSize);
	constexpr std::uint32_t commandSequenceOffset = static_cast<std::uint32_t>(snapshotClientsOffset + ClientSlots::CLIENT_LIMIT * snapshotClientSize);
	constexpr std::size_t stockSnapshotSize = stockCommandSequenceOffset + 4;
	constexpr std::size_t snapshotSize = commandSequenceOffset + 4;

	static_assert(stockSnapshotSize == 0x339EC);

	constexpr std::size_t stockSkeletonMemorySize = 0x40000;
	constexpr std::size_t skeletonMemorySize = 0x400000;

	constexpr std::uint32_t cachedClientFrames = 18;
	constexpr std::uint32_t stockCachedClientCount = 18 * cachedClientFrames;
	constexpr std::uint32_t cachedClientCount = ClientSlots::CLIENT_LIMIT * cachedClientFrames;
	constexpr std::size_t cachedClientSize = 0x319C;

	static constexpr std::uint32_t DivideMagic(std::uint64_t divisor, std::uint32_t shift)
	{
		const std::uint64_t power = 1ull << (32 + shift);
		return static_cast<std::uint32_t>((power + divisor - 1) / divisor);
	}

	static constexpr bool IsDivideExact(std::uint64_t divisor, std::uint32_t shift, std::uint64_t largest)
	{
		const std::uint64_t power = 1ull << (32 + shift);
		const std::uint64_t magic = (power + divisor - 1) / divisor;

		return magic < 0x80000000ull && (magic * divisor - power) * largest < power;
	}

	static constexpr std::uint32_t DivideShift(std::uint64_t divisor, std::uint64_t largest)
	{
		for (std::uint32_t shift = 0; shift < 32; ++shift)
		{
			if (IsDivideExact(divisor, shift, largest))
			{
				return shift;
			}
		}

		return 32;
	}

	static_assert(DivideMagic(stockCachedClientCount, DivideShift(stockCachedClientCount, 1ull << 31)) == 0x1948B0FD && DivideShift(stockCachedClientCount, 1ull << 31) == 5);

	constexpr std::uint32_t cachedClientShift = DivideShift(cachedClientCount, 1ull << 31);
	constexpr std::uint32_t cachedClientMagic = DivideMagic(cachedClientCount, cachedClientShift);

	static_assert(cachedClientShift < 32);

	constexpr std::uint32_t stockHudElemCount = 1024;
	constexpr std::uint32_t hudElemCount = 8192;
	constexpr std::size_t hudElemSize = 0xB4;

	constexpr std::uint32_t bodyQueueSize = 8;
	constexpr std::uint32_t reservedEntities = ClientSlots::CLIENT_LIMIT + bodyQueueSize;
	constexpr std::uint32_t gentityStride = 0x298;
	constexpr std::uint32_t gclientStride = 0x3678;

	static constexpr SlotArrayLayout PerClient(std::uintptr_t address, std::size_t headerSize, std::size_t entrySize)
	{
		return { address, headerSize + entrySize * ClientSlots::BASEGAME_CLIENT_LIMIT, headerSize + entrySize * ClientSlots::CLIENT_LIMIT, headerSize, entrySize };
	}

	static constexpr SlotArrayLayout Fixed(std::uintptr_t address, std::size_t stockSize, std::size_t newSize)
	{
		return { address, stockSize, newSize, 0, 0 };
	}

	static constexpr std::array<std::uint8_t, 7> MovImm32(std::uint8_t opcode, std::uint32_t value, std::size_t length)
	{
		std::array<std::uint8_t, 7> bytes{};
		bytes[0] = opcode;
		bytes[1] = static_cast<std::uint8_t>(value);
		bytes[2] = static_cast<std::uint8_t>(value >> 8);
		bytes[3] = static_cast<std::uint8_t>(value >> 16);
		bytes[4] = static_cast<std::uint8_t>(value >> 24);

		for (std::size_t i = 5; i < length; ++i)
		{
			bytes[i] = 0x90;
		}

		return bytes;
	}

	static const SlotArrayLayout slotArrays[] =
	{
		PerClient(0x1419B5B50, 0x0, 0x3678),
		PerClient(0x14340EC90, 0x0, 0xA67B0),
		PerClient(0x1417CDF40, 0x931C8, 0x548),
		PerClient(0x141867420, 0x0, 0x4),
		Fixed(0x1425BEC80, 57600 * snapshotEntitySize, snapshotEntityCount * snapshotEntitySize),
		Fixed(0x143FC5C8C, 10368 * snapshotClientSize, snapshotClientCount * snapshotClientSize),
		Fixed(0x141781120, stockHudElemCount * hudElemSize, hudElemCount * hudElemSize),
		Fixed(0x1460FFA9C, stockCachedClientCount * cachedClientSize, cachedClientCount * cachedClientSize),
		{ 0x1404ED308, 0x931C8 + 18 * 0x548, 0x931C8 + (ClientSlots::CLIENT_LIMIT + 2) * 0x548, 0x931C8, 0x548 },
		Fixed(0x140587400, 4, ClientSlots::CLIENT_MASK_WORDS * 4),
		{ 0x140469F10, stockCompassActorCount * 0x3C, compassActorCount * 0x3C, 0, 0x3C },
		Fixed(0x140462710, stockCompassActorCount * 20, compassActorCount * 20),
		Fixed(0x140462920, stockCompassActorCount * 20, compassActorCount * 20),
		PerClient(0x14046B2A0, 0x0, 12),
		Fixed(0x140BE9F9C, stockParseClientCount * snapshotClientSize, parseClientCount * snapshotClientSize),
		Fixed(0x140479D40, stockSnapshotSize, snapshotSize),
		Fixed(0x1404AD72C, stockSnapshotSize, snapshotSize),
		PerClient(0x14650DF00, 0x0, 0x44),
		Fixed(0x140C5DACC, stockSkeletonMemorySize, skeletonMemorySize),
		Fixed(0x14228E1D0, stockSkeletonMemorySize, skeletonMemorySize),

		PerClient(0x1404EBE58, 0x0, 0x38),
	};

	static_assert(std::size(slotArrays) == static_cast<std::size_t>(SlotArray::Count));

	static const SlotSite slotSites[] =
	{
		{ 0x14019D491, 7, 3, SlotArray::GClients, SiteBase::Rip, 0x0 },
		{ 0x1401A0F38, 7, 3, SlotArray::GClients, SiteBase::Rip, 0xF0 },

		{ 0x1401A1432, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1401A1686, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1401A19F5, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1401A1A1D, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x43F17 },
		{ 0x1401A2831, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x43F17 },
		{ 0x1401A2AF9, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402333CD, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140233403, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023342A, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x212E4 },
		{ 0x140233453, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x41AF8 },
		{ 0x14023349A, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x20E88 },
		{ 0x140233585, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x212B8 },
		{ 0x14023365E, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x1402361C9, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023644D, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140236A5A, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140236ACA, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140236D0F, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023711A, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402371C2, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402372A2, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023738B, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x14CF60 },
		{ 0x140237488, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402377A3, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140237936, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140237A05, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140237AB5, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140237BF0, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140237C90, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140238329, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402383B1, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140238450, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402384B5, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140238763, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x41B19 },
		{ 0x140238783, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x43B15 },
		{ 0x1402387E1, 8, 3, SlotArray::SvsClients, SiteBase::Svs, 0x0 },
		{ 0x1402387EB, 6, 2, SlotArray::SvsClients, SiteBase::Svs, 0x41B19 },
		{ 0x140238873, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140238953, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402389DA, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x41B14 },
		{ 0x1402389FA, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x41B0C },
		{ 0x140238A20, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140239055, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402390D1, 8, 3, SlotArray::SvsClients, SiteBase::Svs, 0x41B0C },
		{ 0x1402390DB, 11, 3, SlotArray::SvsClients, SiteBase::Svs, 0x43F2C },
		{ 0x1402390E6, 8, 3, SlotArray::SvsClients, SiteBase::Svs, 0x43F28 },
		{ 0x140239109, 7, 3, SlotArray::SvsClients, SiteBase::Svs, 0x0 },
		{ 0x140239296, 9, 5, SlotArray::SvsClients, SiteBase::Svs, 0x41B19 },
		{ 0x14023940A, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x43F15 },
		{ 0x140239639, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140239A24, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140239E37, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x14CF60 },
		{ 0x140239F60, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023A029, 7, 3, SlotArray::SvsClients, SiteBase::Svs, 0x0 },
		{ 0x14023A08D, 7, 3, SlotArray::SvsClients, SiteBase::Svs, 0x0 },
		{ 0x14023A266, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x670 },
		{ 0x14023ABFD, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023ADC8, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023AE87, 7, 3, SlotArray::SvsClients, SiteBase::Svs, 0x670 },
		{ 0x14023AEA8, 7, 3, SlotArray::SvsClients, SiteBase::Svs, 0x212C0 },
		{ 0x14023AFBC, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023B3AB, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023B7FE, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023B893, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x14023BDA5, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023BE93, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x14023C32B, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x212D4 },
		{ 0x14023C46A, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023C577, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023C748, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x41B0C },
		{ 0x14023C7E8, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x41B0C },
		{ 0x14023C8FA, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x14023C9ED, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023CDEC, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023D223, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023D639, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x212B8 },
		{ 0x14023D653, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x14023DA92, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023DBE9, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023DFB7, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x14023DFBE, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023E31E, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x14023E382, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023E7DD, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023E889, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023EBC7, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x28 },
		{ 0x14023ED77, 9, 4, SlotArray::SvsClients, SiteBase::Image, 0x0 },
		{ 0x14023ED82, 9, 5, SlotArray::SvsClients, SiteBase::Image, 0x28 },
		{ 0x14023ED8B, 8, 4, SlotArray::SvsClients, SiteBase::Image, 0x38 },
		{ 0x14023F064, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023F272, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x10 },
		{ 0x14023F3AB, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14023F936, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140241BD0, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140241CE9, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140241EDC, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140242147, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x1402424B0, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14024267C, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x140242F1E, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14024BE6D, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14024BFFF, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },
		{ 0x14024D64F, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x43F18 },
		{ 0x14028D407, 7, 3, SlotArray::SvsClients, SiteBase::Rip, 0x0 },

		{ 0x14017E6CE, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x14018EEA4, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x14018EFAC, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x93158 },
		{ 0x140192F9B, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1401930BC, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x140193DCA, 9, 5, SlotArray::LevelBgs, SiteBase::Image, 0x935EC },
		{ 0x140193DDA, 9, 5, SlotArray::LevelBgs, SiteBase::Image, 0x935F0 },
		{ 0x14019410F, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931C8 },
		{ 0x14019468D, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x140194724, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x93248 },
		{ 0x1401947D6, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x933C8 },
		{ 0x14019481C, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x93248 },
		{ 0x140195ED2, 7, 3, SlotArray::LevelBgs, SiteBase::Image, 0x931C8 },
		{ 0x1401960B3, 7, 3, SlotArray::LevelBgs, SiteBase::Image, 0x931D4 },
		{ 0x1401960DF, 8, 4, SlotArray::LevelBgs, SiteBase::Image, 0x931E4 },
		{ 0x1401969FC, 7, 3, SlotArray::LevelBgs, SiteBase::Image, 0x931D4 },
		{ 0x140196A2E, 8, 4, SlotArray::LevelBgs, SiteBase::Image, 0x931E4 },
		{ 0x140198161, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931C8 },
		{ 0x14019CBD2, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x936D8 },
		{ 0x14019D17E, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931A8 },
		{ 0x14019D185, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x93198 },
		{ 0x14019D19A, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931C0 },
		{ 0x14019D1A1, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931A0 },
		{ 0x14019D1AF, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931B8 },
		{ 0x14019D1BD, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931B0 },
		{ 0x14019D1CB, 10, 2, SlotArray::LevelBgs, SiteBase::Rip, 0x93194 },
		{ 0x14019D28A, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x14019D37F, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x93168 },
		{ 0x14019D386, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x936D8 },
		{ 0x14019EAC4, 6, 2, SlotArray::LevelBgs, SiteBase::Rip, 0x93188 },
		{ 0x14019EACA, 6, 2, SlotArray::LevelBgs, SiteBase::Rip, 0x9318C },
		{ 0x14019EAD4, 6, 2, SlotArray::LevelBgs, SiteBase::Rip, 0x93190 },
		{ 0x14019EB07, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x14019F47B, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x936D8 },
		{ 0x1401A1439, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1401A150F, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x1401A1548, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x1401A168D, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1401A237B, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x0 },
		{ 0x1401AAC27, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x931C8 },

		{ 0x140198919, 7, 3, SlotArray::SortedClients, SiteBase::Rip, 0x0 },
		{ 0x1401991E0, 8, 4, SlotArray::SortedClients, SiteBase::Image, 0x0 },
		{ 0x14019926A, 8, 4, SlotArray::SortedClients, SiteBase::Image, 0x0 },
		{ 0x140199968, 7, 3, SlotArray::SortedClients, SiteBase::Rip, 0x0 },
		{ 0x14019CA7B, 7, 3, SlotArray::SortedClients, SiteBase::Rip, 0x0 },
		{ 0x1401A10B5, 7, 3, SlotArray::SortedClients, SiteBase::Rip, 0x0 },
		{ 0x1401A1F65, 7, 3, SlotArray::SortedClients, SiteBase::Rip, 0x0 },

		{ 0x14023F32A, 7, 3, SlotArray::SnapshotEntities, SiteBase::Rip, 0x0 },
		{ 0x14023F709, 8, 4, SlotArray::SnapshotEntities, SiteBase::Svs, 0x0 },
		{ 0x140241D82, 7, 3, SlotArray::SnapshotEntities, SiteBase::Rip, 0x0 },
		{ 0x1402426E6, 7, 3, SlotArray::SnapshotEntities, SiteBase::Rip, 0x0 },

		{ 0x14023F3C7, 7, 3, SlotArray::SnapshotClients, SiteBase::Rip, 0x0 },
		{ 0x14023F7FB, 9, 5, SlotArray::SnapshotClients, SiteBase::Svs, 0x0 },
		{ 0x14023F80D, 9, 5, SlotArray::SnapshotClients, SiteBase::Svs, 0x10 },
		{ 0x14023F81F, 9, 5, SlotArray::SnapshotClients, SiteBase::Svs, 0x20 },
		{ 0x14023F831, 9, 5, SlotArray::SnapshotClients, SiteBase::Svs, 0x30 },
		{ 0x14023F843, 9, 5, SlotArray::SnapshotClients, SiteBase::Svs, 0x40 },
		{ 0x14023F855, 9, 5, SlotArray::SnapshotClients, SiteBase::Svs, 0x50 },
		{ 0x14023F867, 9, 5, SlotArray::SnapshotClients, SiteBase::Svs, 0x60 },
		{ 0x14023F87A, 10, 6, SlotArray::SnapshotClients, SiteBase::Svs, 0x70 },
		{ 0x14023F88C, 8, 4, SlotArray::SnapshotClients, SiteBase::Svs, 0x78 },
		{ 0x140241DAA, 7, 3, SlotArray::SnapshotClients, SiteBase::Rip, 0x0 },
		{ 0x14024270E, 7, 3, SlotArray::SnapshotClients, SiteBase::Rip, 0x0 },

		{ 0x14016A434, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016A4DB, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x84 },
		{ 0x14016A562, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016A64B, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x10 },
		{ 0x14016A699, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x10 },
		{ 0x14016A6F6, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016A7A1, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016A846, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016A8F6, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016A9A1, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AA46, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AB31, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016ABB4, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AC7B, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016ACCB, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AD1B, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AD70, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AE10, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AEC6, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016AFE4, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B12D, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B1EB, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B241, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B2F5, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B5E6, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B713, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B8B7, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B90F, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016B93D, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016BB80, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016BD84, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x14016BDD4, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x1401A10C9, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x1401A1F79, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x1401A9710, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },
		{ 0x1401A97A2, 7, 3, SlotArray::HudElems, SiteBase::Rip, 0x0 },

		{ 0x14023F7F2, 9, 5, SlotArray::CachedClients, SiteBase::Svs, 0x3120 },
		{ 0x14023F804, 9, 5, SlotArray::CachedClients, SiteBase::Svs, 0x3130 },
		{ 0x14023F816, 9, 5, SlotArray::CachedClients, SiteBase::Svs, 0x3140 },
		{ 0x14023F828, 9, 5, SlotArray::CachedClients, SiteBase::Svs, 0x3150 },
		{ 0x14023F83A, 9, 5, SlotArray::CachedClients, SiteBase::Svs, 0x3160 },
		{ 0x14023F84C, 9, 5, SlotArray::CachedClients, SiteBase::Svs, 0x3170 },
		{ 0x14023F85E, 9, 5, SlotArray::CachedClients, SiteBase::Svs, 0x3180 },
		{ 0x14023F870, 10, 6, SlotArray::CachedClients, SiteBase::Svs, 0x3190 },
		{ 0x14023F884, 8, 4, SlotArray::CachedClients, SiteBase::Svs, 0x3198 },
		{ 0x14023F9D5, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x14023FE2E, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x14023FFC6, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x1402400D6, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x14024016A, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x1402402B9, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x1402402F9, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x140241DD4, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },
		{ 0x140242738, 7, 3, SlotArray::CachedClients, SiteBase::Rip, 0x0 },

		{ 0x1400819F9, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x140081A24, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400AF0D0, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400B1BB1, 8, 3, SlotArray::CgameBgs, SiteBase::Image, 0x931C8 },
		{ 0x1400B1BBB, 7, 3, SlotArray::CgameBgs, SiteBase::Image, 0x931D4 },
		{ 0x1400B8239, 6, 2, SlotArray::CgameBgs, SiteBase::Rip, 0x93188 },
		{ 0x1400B8277, 6, 2, SlotArray::CgameBgs, SiteBase::Rip, 0x93190 },
		{ 0x1400B82B7, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x0 },
		{ 0x1400C2A19, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931F4 },
		{ 0x1400C38E9, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400C39C2, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400C3DF7, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400C596C, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400C6879, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400CCF0D, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400CCF9E, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931F4 },
		{ 0x1400CD042, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400CD4B1, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400CD610, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400CE8A9, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400CE93F, 8, 3, SlotArray::CgameBgs, SiteBase::Image, 0x931C8 },
		{ 0x1400CE94D, 7, 3, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400CEA1D, 8, 3, SlotArray::CgameBgs, SiteBase::Image, 0x931C8 },
		{ 0x1400CEA27, 7, 3, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400CEA8D, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931F4 },
		{ 0x1400CEBEB, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400CEC1E, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931F4 },
		{ 0x1400CF199, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400CFFEB, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x93C88 },
		{ 0x1400D17A4, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400D1880, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400D498F, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400D65F1, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x93198 },
		{ 0x1400D65FF, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931A0 },
		{ 0x1400D660D, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931A8 },
		{ 0x1400D661B, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931B8 },
		{ 0x1400D6629, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931B0 },
		{ 0x1400D6637, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C0 },
		{ 0x1400D6645, 10, 2, SlotArray::CgameBgs, SiteBase::Rip, 0x93194 },
		{ 0x1400D69A1, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x0 },
		{ 0x1400D6A5F, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x93168 },
		{ 0x1400D6A66, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x936D8 },
		{ 0x1400DA265, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x936D8 },
		{ 0x1400DFBFB, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400DFCF1, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E023E, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E0375, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x93158 },
		{ 0x1400E0CE1, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E0D47, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E3842, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E402D, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931E4 },
		{ 0x1400E4156, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931E4 },
		{ 0x1400E4976, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931E4 },
		{ 0x1400E518B, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E5B51, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E5BC8, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400E863A, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931C8 },
		{ 0x1400E864B, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931EC },
		{ 0x1400E8656, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931F0 },
		{ 0x1400E865E, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931EC },
		{ 0x1400E868A, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931FC },
		{ 0x1400E869D, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400E95B9, 6, 2, SlotArray::CgameBgs, SiteBase::Rip, 0x9318C },
		{ 0x1400E97AC, 6, 2, SlotArray::CgameBgs, SiteBase::Rip, 0x93188 },
		{ 0x1400E981B, 6, 2, SlotArray::CgameBgs, SiteBase::Rip, 0x93188 },
		{ 0x1400EA90D, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400EADC3, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x1400F565A, 9, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931C8 },
		{ 0x1400F56AA, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400F56E3, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x1400F57A7, 12, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931F4 },
		{ 0x1400F57BE, 7, 3, SlotArray::CgameBgs, SiteBase::Image, 0x935D8 },
		{ 0x1400F5A9F, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931E4 },
		{ 0x140109CC9, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931EC },

		{ 0x140109CD7, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 + 18 * 0x548 + 0x24 },
		{ 0x1401E6BC2, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x931C8 },
		{ 0x140254B16, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931C8 },
		{ 0x140254B20, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x140254D02, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931C8 },
		{ 0x140254D0C, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x14029C066, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },
		{ 0x14029C0A1, 8, 4, SlotArray::CgameBgs, SiteBase::Image, 0x931E4 },

		{ 0x1400A57A4, 8, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400A7D2F, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400A7D52, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400AD056, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931F4 },
		{ 0x1400C2A03, 8, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400C2A42, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400CDE16, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400CDE2A, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400CE1E8, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400CE1FC, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400CFD55, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400CFD6D, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D010D, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D01C8, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D0286, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D1FDF, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931EC },
		{ 0x1400D1FE6, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931F0 },
		{ 0x1400D2301, 8, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400D2319, 8, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400D232C, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D23D6, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D240D, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D2678, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D2695, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D26AA, 8, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931F8 },
		{ 0x1400D33E1, 10, 6, SlotArray::CgameBgs, SiteBase::Cg, 0x935D0 },
		{ 0x1400D33EE, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935D8 },
		{ 0x1400D33F9, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935DC },
		{ 0x1400D3404, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935E0 },
		{ 0x1400D341A, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935D4 },
		{ 0x1400D4464, 9, 5, SlotArray::CgameBgs, SiteBase::Cg, 0x935D0 },
		{ 0x1400D4470, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935D8 },
		{ 0x1400D447A, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935DC },
		{ 0x1400D4484, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935E0 },
		{ 0x1400D44CB, 9, 5, SlotArray::CgameBgs, SiteBase::Cg, 0x935D4 },
		{ 0x1400D51E7, 11, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935F4 },
		{ 0x1400D5214, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935F4 },
		{ 0x1400D521C, 11, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935F8 },
		{ 0x1400D7209, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400D722A, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400DD968, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931D4 },
		{ 0x1400DD977, 9, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400DD994, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400DD99F, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400DF6B9, 8, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400DF6CE, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400DF6DD, 8, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400DF6EB, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400DFE90, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400E002A, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400E0044, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400E09E0, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x936D8 },
		{ 0x1400E0A1F, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x93160 },
		{ 0x1400E0A43, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x93162 },
		{ 0x1400E0A6A, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x93164 },
		{ 0x1400E0A91, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x93570 },
		{ 0x1400E0A99, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x93578 },
		{ 0x1400E0AA1, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x93580 },
		{ 0x1400E0AA9, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x93588 },
		{ 0x1400E0AB1, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x93590 },
		{ 0x1400E0AB9, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935DC },
		{ 0x1400E0AC0, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x93560 },
		{ 0x1400E0AC7, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x93564 },
		{ 0x1400E0ACF, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x9356C },
		{ 0x1400E0AD6, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935A8 },
		{ 0x1400E0ADE, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935B0 },
		{ 0x1400E0AE6, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935B8 },
		{ 0x1400E0AEE, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935C0 },
		{ 0x1400E0AF6, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x935C8 },
		{ 0x1400E0AFE, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935DC },
		{ 0x1400E0B0A, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x93598 },
		{ 0x1400E0B11, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935D8 },
		{ 0x1400E0B18, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935A0 },
		{ 0x1400E0B1F, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x9359C },
		{ 0x1400E0B26, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935A4 },
		{ 0x1400E425D, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400E914B, 9, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400E9156, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },
		{ 0x1400E9AC4, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x931C8 },
		{ 0x1400EAD4C, 9, 5, SlotArray::CgameBgs, SiteBase::Cg, 0x935D0 },
		{ 0x1400EAD5B, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935D4 },
		{ 0x1400EAD65, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935D8 },
		{ 0x1400EAD6F, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935DC },
		{ 0x1400EAD79, 7, 3, SlotArray::CgameBgs, SiteBase::Cg, 0x935E0 },
		{ 0x14029BDE0, 8, 4, SlotArray::CgameBgs, SiteBase::Cg, 0x931E4 },

		{ 0x1400E0B58, 8, 4, SlotArray::CgameClientMask, SiteBase::Image, 0x0 },
		{ 0x1400E0B64, 8, 4, SlotArray::CgameClientMask, SiteBase::Image, 0x0 },
		{ 0x1400E0C13, 8, 4, SlotArray::CgameClientMask, SiteBase::Image, 0x0 },
		{ 0x1400E0C1F, 8, 4, SlotArray::CgameClientMask, SiteBase::Image, 0x0 },
		{ 0x1400E0077, 7, 3, SlotArray::CgameClientMask, SiteBase::Cg, 0x0 },
		{ 0x1400E0091, 7, 3, SlotArray::CgameClientMask, SiteBase::Cg, 0x0 },
		{ 0x1400E00E8, 7, 3, SlotArray::CgameClientMask, SiteBase::Cg, 0x0 },

		{ 0x1400B4B94, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x30 },
		{ 0x1400B52ED, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x0 },
		{ 0x1400CCDC6, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x0 },
		{ 0x1400CCF60, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x0 },
		{ 0x1400CD16C, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x28 },
		{ 0x1400CD600, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x0 },
		{ 0x1400D00A1, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x0 },
		{ 0x1400B3EEA, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x0 },
		{ 0x1400CEA39, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x0 },
		{ 0x1400CEA6C, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x30 },
		{ 0x1400CEAA2, 8, 4, SlotArray::CompassActors, SiteBase::Image, 0x34 },
		{ 0x1400CEAAD, 8, 4, SlotArray::CompassActors, SiteBase::Image, 0x38 },
		{ 0x1400CEB12, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x1C },
		{ 0x1400CEB3F, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x2C },
		{ 0x1400CEB53, 8, 4, SlotArray::CompassActors, SiteBase::Image, 0x20 },
		{ 0x1400CEB64, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x20 },
		{ 0x1400CEBCE, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x0 },
		{ 0x1400CEBFF, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x30 },
		{ 0x1400CEC36, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x34 },
		{ 0x1400CEC43, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x38 },
		{ 0x1400CECD4, 9, 5, SlotArray::CompassActors, SiteBase::Image, 0x1C },
		{ 0x1400CECDF, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x20 },
		{ 0x1400CECED, 7, 3, SlotArray::CompassActors, SiteBase::Image, 0x20 },

		{ 0x1400B3EF7, 7, 3, SlotArray::MotionTrackerPos, SiteBase::Image, 0x0 },
		{ 0x1400B42B3, 7, 3, SlotArray::MotionTrackerPos, SiteBase::Rip, 0x0 },
		{ 0x1400B4667, 7, 3, SlotArray::MotionTrackerPos, SiteBase::Image, 0x0 },
		{ 0x1400B4A55, 7, 3, SlotArray::MotionTrackerPos, SiteBase::Image, 0x0 },
		{ 0x1400B4119, 7, 3, SlotArray::MotionTrackerPrev, SiteBase::Image, 0x0 },
		{ 0x1400B42CE, 7, 3, SlotArray::MotionTrackerPrev, SiteBase::Rip, 0x0 },
		{ 0x1400B4676, 7, 3, SlotArray::MotionTrackerPrev, SiteBase::Image, 0x0 },
		{ 0x1400B4A64, 7, 3, SlotArray::MotionTrackerPrev, SiteBase::Image, 0x0 },

		{ 0x1400D0D62, 7, 3, SlotArray::OverheadFade, SiteBase::Rip, 0x0 },
		{ 0x1400D1A6F, 8, 3, SlotArray::OverheadFade, SiteBase::Image, 0x8 },
		{ 0x1400D1A79, 8, 3, SlotArray::OverheadFade, SiteBase::Image, 0x8 },
		{ 0x1400D1A81, 7, 3, SlotArray::OverheadFade, SiteBase::Image, 0x4 },
		{ 0x1400D1A88, 7, 3, SlotArray::OverheadFade, SiteBase::Image, 0x0 },
		{ 0x1400D1AA5, 9, 4, SlotArray::OverheadFade, SiteBase::Image, 0x8 },
		{ 0x1400D1ADE, 8, 4, SlotArray::OverheadFade, SiteBase::Image, 0x0 },
		{ 0x1400D1AF1, 8, 4, SlotArray::OverheadFade, SiteBase::Image, 0x4 },

		{ 0x1400F4AFE, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x1400F4B0B, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x10 },
		{ 0x1400F4B18, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x20 },
		{ 0x1400F4B25, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x30 },
		{ 0x1400F4B32, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x40 },
		{ 0x1400F4B3F, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x50 },
		{ 0x1400F4B4C, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x60 },
		{ 0x1400F4B59, 9, 5, SlotArray::ParseClients, SiteBase::Clients, 0x70 },
		{ 0x1400F4B68, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x78 },
		{ 0x1400FF842, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x1400FF8FA, 6, 2, SlotArray::ParseClients, SiteBase::Clients, 0x6C },
		{ 0x1400FF91A, 6, 2, SlotArray::ParseClients, SiteBase::Clients, 0x6C },
		{ 0x1400FF930, 6, 2, SlotArray::ParseClients, SiteBase::Clients, 0x6C },
		{ 0x14010047F, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x1401005A9, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x1401005B5, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x10 },
		{ 0x1401005C1, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x20 },
		{ 0x1401005CD, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x30 },
		{ 0x1401005D9, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x40 },
		{ 0x1401005E5, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x50 },
		{ 0x1401005F1, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x60 },
		{ 0x1401005FE, 9, 5, SlotArray::ParseClients, SiteBase::Clients, 0x70 },
		{ 0x14010060A, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x78 },
		{ 0x140100636, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x1401006A1, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x140100773, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x14010077F, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x10 },
		{ 0x14010078B, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x20 },
		{ 0x140100797, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x30 },
		{ 0x1401007A3, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x40 },
		{ 0x1401007AF, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x50 },
		{ 0x1401007BB, 8, 4, SlotArray::ParseClients, SiteBase::Clients, 0x60 },
		{ 0x1401007C8, 9, 5, SlotArray::ParseClients, SiteBase::Clients, 0x70 },
		{ 0x1401007D4, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x78 },
		{ 0x1401007F9, 7, 3, SlotArray::ParseClients, SiteBase::Clients, 0x0 },
		{ 0x140101E57, 7, 3, SlotArray::ParseClients, SiteBase::Rip, 0x0 },

		{ 0x1400E95A0, 7, 3, SlotArray::ActiveSnapshot0, SiteBase::Rip, 0x0 },
		{ 0x1400E95AD, 7, 3, SlotArray::ActiveSnapshot1, SiteBase::Rip, 0x0 },

		{ 0x1400FD357, 7, 3, SlotArray::ClientSkeletons, SiteBase::Rip, 0xF },
		{ 0x140232F7C, 7, 3, SlotArray::ServerSkeletons, SiteBase::Rip, 0xF },
		{ 0x140233564, 7, 3, SlotArray::ServerSkeletons, SiteBase::Rip, 0xF },
		{ 0x1402336D1, 7, 3, SlotArray::ServerSkeletons, SiteBase::Rip, 0xF },

		{ 0x1400E40B7, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x0 },
		{ 0x1400E5120, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x0 },
		{ 0x1400E5B4A, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x0 },
		{ 0x1400E5BB0, 7, 3, SlotArray::Scores, SiteBase::Rip, -0x38 },
		{ 0x1400E78D9, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x0 },
		{ 0x1400E8430, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x0 },
		{ 0x1400E8464, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x14 },
		{ 0x1400E866D, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x30 },
		{ 0x1400E90C6, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x0 },
		{ 0x1400E911D, 8, 4, SlotArray::Scores, SiteBase::Cg, 0x4 },
		{ 0x1400E9130, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x10 },
		{ 0x1400E917C, 7, 3, SlotArray::Scores, SiteBase::Cg, -0x28 },
		{ 0x1400E9220, 7, 3, SlotArray::Scores, SiteBase::Rip, 0x10 },

		{ 0x140242F8B, 7, 3, SlotArray::SnapshotData, SiteBase::Rip, 0x0 },
	};

	struct SlotLoopEnd
	{
		std::uintptr_t address;
		std::uint8_t length;
		std::uint8_t operandOffset;
		SlotArray array;
		SiteBase base;
		std::int32_t fieldOffset;
	};

	static const SlotLoopEnd slotLoopEnds[] =
	{
		{ 0x14019D38D, 7, 3, SlotArray::LevelBgs, SiteBase::Rip, 0x510 },

		{ 0x1400D6A6D, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x510 },
		{ 0x1400D0014, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0xAC0 },
		{ 0x1400C2A3B, 7, 3, SlotArray::CgameBgs, SiteBase::Rip, 0x2C },
		{ 0x1400CE9A4, 7, 3, SlotArray::CompassActors, SiteBase::Rip, 0x0 },
	};

	static const ImmediatePatch immediatePatches[] =
	{
		{ 0x14016B731, 1, stockHudElemCount, hudElemCount },
		{ 0x14016B8BE, 1, stockHudElemCount, hudElemCount },
		{ 0x14016B916, 1, stockHudElemCount, hudElemCount },
		{ 0x14016B944, 2, static_cast<std::uint32_t>(stockHudElemCount * hudElemSize), static_cast<std::uint32_t>(hudElemCount * hudElemSize) },
		{ 0x14016BB92, 2, stockHudElemCount, hudElemCount },

		{ 0x14019D4A1, 2, 18 * gclientStride, ClientSlots::CLIENT_LIMIT * gclientStride },
		{ 0x140163789, 2, 18 * gclientStride, ClientSlots::CLIENT_LIMIT * gclientStride },

		{ 0x14019CBD9, 1, 18, ClientSlots::CLIENT_LIMIT },
		{ 0x14019F482, 1, 18, ClientSlots::CLIENT_LIMIT },

		{ 0x14023A38E, 2, 18, ClientSlots::CLIENT_LIMIT },
		{ 0x14023A3C0, 2, 18, ClientSlots::CLIENT_LIMIT },

		{ 0x1402358A0, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x140235A30, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x14023F7B4, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x14023FFE1, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x1402400F2, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x14024012C, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x140240294, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x14024062C, 1, 0x1948B0FD, cachedClientMagic },
		{ 0x14023F9E4, 1, 0xCA4587E7, cachedClientMagic },
		{ 0x1402358BC, 2, stockCachedClientCount, cachedClientCount },
		{ 0x140235A44, 2, stockCachedClientCount, cachedClientCount },
		{ 0x14023F7D1, 2, stockCachedClientCount, cachedClientCount },
		{ 0x14023F9EE, 2, stockCachedClientCount, cachedClientCount },
		{ 0x14023FFF2, 2, stockCachedClientCount, cachedClientCount },
		{ 0x140240105, 2, stockCachedClientCount, cachedClientCount },
		{ 0x14024014F, 2, stockCachedClientCount, cachedClientCount },
		{ 0x1402402A7, 2, stockCachedClientCount, cachedClientCount },
		{ 0x1402402DE, 2, stockCachedClientCount, cachedClientCount },
		{ 0x140240644, 2, stockCachedClientCount, cachedClientCount },

		{ 0x1402359B4, 1, 0u - stockCachedClientCount, 0u - cachedClientCount },
		{ 0x1402410C7, 1, 0u - stockCachedClientCount, 0u - cachedClientCount },

		{ 0x1400FB75C, 6, stockParseClientCount, parseClientCount },
		{ 0x1400F4ABD, 1, 18, ClientSlots::CLIENT_LIMIT },
		{ 0x1400F48F7, 2, stockCommandSequenceOffset, commandSequenceOffset },
		{ 0x1400E9A7C, 3, stockCommandSequenceOffset, commandSequenceOffset },

		{ 0x1400CCDCD, 2, static_cast<std::uint32_t>(stockCompassActorCount * 0x3C), static_cast<std::uint32_t>(compassActorCount * 0x3C) },
		{ 0x1400B42BA, 2, static_cast<std::uint32_t>(stockCompassActorCount * 20), static_cast<std::uint32_t>(compassActorCount * 20) },
		{ 0x1400B42D5, 2, static_cast<std::uint32_t>(stockCompassActorCount * 20), static_cast<std::uint32_t>(compassActorCount * 20) },
		{ 0x1400B465D, 2, static_cast<std::uint32_t>(stockCompassActorCount * 20), static_cast<std::uint32_t>(compassActorCount * 20) },
		{ 0x1400B4682, 2, static_cast<std::uint32_t>(stockCompassActorCount * 20), static_cast<std::uint32_t>(compassActorCount * 20) },
		{ 0x1400B3EE4, 2, static_cast<std::uint32_t>(stockCompassActorCount), static_cast<std::uint32_t>(compassActorCount) },
		{ 0x1400B4B9B, 2, static_cast<std::uint32_t>(stockCompassActorCount), static_cast<std::uint32_t>(compassActorCount) },

		{ 0x1400CD18A, 1, 18, ClientSlots::CLIENT_LIMIT },
		{ 0x1400DA26C, 1, 18, ClientSlots::CLIENT_LIMIT },
		{ 0x1400D0D69, 2, 18 * 12, ClientSlots::CLIENT_LIMIT * 12 },

		{ 0x1400E8358, 1, 18, ClientSlots::CLIENT_LIMIT },
		{ 0x1400E842A, 2, 18 * 0x38, ClientSlots::CLIENT_LIMIT * 0x38 },
		{ 0x1400E78E0, 2, 18 * 0x38, ClientSlots::CLIENT_LIMIT * 0x38 },

		{ 0x1401014C3, 1, static_cast<std::uint32_t>(stockSkeletonMemorySize - 0x10), static_cast<std::uint32_t>(skeletonMemorySize - 0x10) },
		{ 0x14010153D, 1, static_cast<std::uint32_t>(stockSkeletonMemorySize - 0x10), static_cast<std::uint32_t>(skeletonMemorySize - 0x10) },
		{ 0x140232F6F, 1, static_cast<std::uint32_t>(stockSkeletonMemorySize - 0x10), static_cast<std::uint32_t>(skeletonMemorySize - 0x10) },
		{ 0x140232FB2, 2, static_cast<std::uint32_t>(stockSkeletonMemorySize - 0x10), static_cast<std::uint32_t>(skeletonMemorySize - 0x10) },
	};

	static const BytePatch bytePatches[] =
	{
		{ 0x14023A0FF, { 0x69, 0xC8, 0x90, 0x01, 0x00, 0x00 }, MovImm32(0xB9, snapshotEntityCount, 6), 6 },
		{ 0x14023A105, { 0x41, 0x69, 0xC0, 0x44, 0x01, 0x00, 0x00 }, MovImm32(0xB8, snapshotClientCount, 7), 7 },
		{ 0x14023B638, { 0x69, 0xC8, 0x90, 0x01, 0x00, 0x00 }, MovImm32(0xB9, snapshotEntityCount, 6), 6 },
		{ 0x14023B63E, { 0x41, 0x69, 0xC5, 0x44, 0x01, 0x00, 0x00 }, MovImm32(0xB8, snapshotClientCount, 7), 7 },
		{ 0x140241B8D, { 0x44, 0x8D, 0x47, 0x12 }, { 0x44, 0x8D, 0x47, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },

		{ 0x14023A429, { 0x44, 0x8B, 0x48, 0x10 }, { 0x6A, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT), 0x41, 0x59 }, 4 },
		{ 0x140239D51, { 0x44, 0x8B, 0x48, 0x10 }, { 0x6A, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT), 0x41, 0x59 }, 4 },
		{ 0x14019DCDA, { 0x44, 0x8B, 0x48, 0x10 }, { 0x6A, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT), 0x41, 0x59 }, 4 },

		{ 0x140100137, { 0x83, 0xF8, 0x11 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
		{ 0x140100549, { 0x83, 0xF8, 0x11 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },

		{ 0x1402358B2, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x140235A3A, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x14023F7C7, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x14023FFE8, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x1402400FB, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x140240145, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x1402402D4, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x14024029D, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x14024063A, { 0xC1, 0xFA, 0x05 }, { 0xC1, 0xFA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },
		{ 0x14023F9EB, { 0xC1, 0xEA, 0x08 }, { 0xC1, 0xEA, static_cast<std::uint8_t>(cachedClientShift) }, 3 },

		{ 0x14023FD65, { 0x8D, 0x0C, 0xC0 }, { 0x6B, 0xC8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x14023FD6E, { 0x8D, 0x0C, 0x48 }, { 0x8D, 0x0C, 0x08 }, 3 },

		{ 0x1400B52FB, { 0x44, 0x8D, 0x57, 0x0D }, { 0x44, 0x8D, 0x57, static_cast<std::uint8_t>(compassActorCount / 2) }, 4 },
		{ 0x1400B4A72, { 0x8D, 0x7B, 0x0D }, { 0x8D, 0x7B, static_cast<std::uint8_t>(compassActorCount / 2) }, 3 },

		{ 0x1400CE9E1, { 0x44, 0x8D, 0x59, 0x12 }, { 0x44, 0x8D, 0x59, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400CEA80, { 0x41, 0x83, 0xFB, 0x12 }, { 0x41, 0x83, 0xFB, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400CDB77, { 0x41, 0x83, 0xFF, 0x12 }, { 0x41, 0x83, 0xFF, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
	};

	constexpr std::uint32_t nonPvsShift = 2;

	static_assert(DivideMagic(18, nonPvsShift) == 0x38E38E39);
	static_assert(IsDivideExact(ClientSlots::CLIENT_LIMIT, nonPvsShift, 2048 + ClientSlots::CLIENT_LIMIT));

	constexpr std::uint8_t compassClientMask = static_cast<std::uint8_t>((1u << std::bit_width(ClientSlots::CLIENT_LIMIT)) - 1);

	static_assert(compassClientMask == 0x7F);

	static const ImmediatePatch layoutImmediates[] =
	{
		{ 0x14019D4ED, 6, 18 + bodyQueueSize, reservedEntities },
		{ 0x14019D501, 1, 18 + bodyQueueSize, reservedEntities },
		{ 0x1401AB79A, 2, (18 + bodyQueueSize) * gentityStride, reservedEntities * gentityStride },
		{ 0x1401AC6DA, 2, (18 + bodyQueueSize) * gentityStride, reservedEntities * gentityStride },

		{ 0x1401ABB2E, 3, 18 * gentityStride, ClientSlots::CLIENT_LIMIT * gentityStride },

		{ 0x140196B84, 1, DivideMagic(18, nonPvsShift), DivideMagic(ClientSlots::CLIENT_LIMIT, nonPvsShift) },
	};

	static const BytePatch layoutBytes[] =
	{
		{ 0x1401AC570, { 0x8D, 0x42, 0x12 }, { 0x8D, 0x42, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },

		{ 0x14016FC8D, { 0x66, 0x83, 0xF8, 0x12 }, { 0x66, 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1401700C0, { 0x66, 0x83, 0xF8, 0x12 }, { 0x66, 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x14017034A, { 0x66, 0x83, 0xF8, 0x12 }, { 0x66, 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x14023F207, { 0x83, 0x3A, 0x12 }, { 0x83, 0x3A, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x140240B66, { 0x83, 0xF8, 0x12 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x140240BF6, { 0x83, 0xF8, 0x12 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },

		{ 0x140196B95, { 0x8D, 0x04, 0xD2, 0x03, 0xC0 }, { 0x6B, 0xC2, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT), 0x90, 0x90 }, 5 },
		{ 0x140196BE9, { 0x83, 0xFF, 0x12 }, { 0x83, 0xFF, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },

		{ 0x140196D0B, { 0x41, 0x83, 0xE1, 0x3F }, { 0x41, 0x83, 0xE1, compassClientMask }, 4 },
		{ 0x140196D3D, { 0x25, 0xC0, 0x7F, 0x00, 0x00 }, { 0x25, static_cast<std::uint8_t>(0xC0 & ~compassClientMask), 0x7F, 0x00, 0x00 }, 5 },
		{ 0x140192F2D, { 0x83, 0xE0, 0x3F }, { 0x83, 0xE0, compassClientMask }, 3 },

		{ 0x1401A5751, { 0x83, 0xFB, 0x12 }, { 0x83, 0xFB, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x140198C8D, { 0x83, 0xF8, 0x11 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
		{ 0x1401A4B7D, { 0x83, 0xF8, 0x11 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
		{ 0x1401A36F3, { 0x83, 0x38, 0x12 }, { 0x83, 0x38, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x1401639E9, { 0x83, 0xFA, 0x12 }, { 0x83, 0xFA, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
	};

	static constexpr std::array<std::uint8_t, 7> LeaRaxRip(std::uintptr_t site, std::uintptr_t target)
	{
		const auto displacement = static_cast<std::uint32_t>(target - (site + 7));

		return { 0x48, 0x8D, 0x05, static_cast<std::uint8_t>(displacement), static_cast<std::uint8_t>(displacement >> 8), static_cast<std::uint8_t>(displacement >> 16), static_cast<std::uint8_t>(displacement >> 24) };
	}

	static const BytePatch cgameLayoutBytes[] =
	{
		{ 0x1400D3438, { 0x48, 0x83, 0xE8, 0x12 }, { 0x48, 0x83, 0xE8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400EA5C5, { 0x48, 0x83, 0xE8, 0x12 }, { 0x48, 0x83, 0xE8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400EADD4, { 0x48, 0x83, 0xE8, 0x12 }, { 0x48, 0x83, 0xE8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400DF9C4, { 0x48, 0x8D, 0x41, 0xEE }, { 0x48, 0x8D, 0x41, static_cast<std::uint8_t>(0 - ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400B910A, LeaRaxRip(0x1400B910A, cg_entities + 18 * centitySize), LeaRaxRip(0x1400B910A, cg_entities + ClientSlots::CLIENT_LIMIT * centitySize), 7 },
		{ 0x1400B9EA5, LeaRaxRip(0x1400B9EA5, cg_entities + 18 * centitySize), LeaRaxRip(0x1400B9EA5, cg_entities + ClientSlots::CLIENT_LIMIT * centitySize), 7 },

		{ 0x1400D32F9, { 0x83, 0xBE, 0xE8, 0x00, 0x00, 0x00, 0x12 }, { 0x83, 0xBE, 0xE8, 0x00, 0x00, 0x00, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 7 },
		{ 0x1400C21D3, { 0x83, 0xFE, 0x12 }, { 0x83, 0xFE, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x1400C22DE, { 0x83, 0xFD, 0x12 }, { 0x83, 0xFD, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x1400E0215, { 0x41, 0x83, 0xF8, 0x12 }, { 0x41, 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400C1B28, { 0x41, 0x83, 0xFC, 0x12 }, { 0x41, 0x83, 0xFC, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400D25F2, { 0x83, 0xFB, 0x12 }, { 0x83, 0xFB, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 3 },
		{ 0x1400D22DD, { 0x41, 0x83, 0xF8, 0x12 }, { 0x41, 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },
		{ 0x1400CE932, { 0x41, 0x83, 0xFB, 0x12 }, { 0x41, 0x83, 0xFB, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT) }, 4 },

		{ 0x1400CEFFD, { 0x83, 0xFE, 0x11 }, { 0x83, 0xFE, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
		{ 0x1400AD01C, { 0x83, 0xF8, 0x11 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
		{ 0x1400AF0B4, { 0x41, 0x83, 0xFE, 0x11 }, { 0x41, 0x83, 0xFE, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 4 },
		{ 0x1400AF124, { 0x83, 0xFD, 0x11 }, { 0x83, 0xFD, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
		{ 0x1400D518D, { 0x83, 0xFE, 0x11 }, { 0x83, 0xFE, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
		{ 0x1400B1B9F, { 0x83, 0xF8, 0x11 }, { 0x83, 0xF8, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },

		{ 0x1400E84A2, { 0x41, 0x83, 0xFF, 0x11 }, { 0x41, 0x83, 0xFF, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 4 },

		{ 0x1400CEBCA, { 0x41, 0x83, 0xE0, 0x3F }, { 0x41, 0x83, 0xE0, compassClientMask }, 4 },

		{ 0x1401E6B7C, { 0x83, 0xFA, 0x11 }, { 0x83, 0xFA, static_cast<std::uint8_t>(ClientSlots::CLIENT_LIMIT - 1) }, 3 },
	};

	static void WriteBytePatch(const BytePatch& patch, bool isRaised)
	{
		for (std::size_t i = 0; i < patch.length; ++i)
		{
			if (isRaised)
			{
				Utils::Hook::Set<std::uint8_t>(patch.address + i, patch.patched[i]);
			}
			else
			{
				Utils::Hook::Set<std::uint8_t>(patch.address + i, patch.stock[i]);
			}
		}
	}

	static void SetWideCgameLayout(bool isWide)
	{
		for (const BytePatch& patch : cgameLayoutBytes)
		{
			WriteBytePatch(patch, isWide);
		}
	}

	static void SetWideEntityLayout(bool isWide)
	{
		for (const ImmediatePatch& patch : layoutImmediates)
		{
			if (isWide)
			{
				Utils::Hook::Set<std::uint32_t>(patch.address + patch.operandOffset, patch.raised);
			}
			else
			{
				Utils::Hook::Set<std::uint32_t>(patch.address + patch.operandOffset, patch.stock);
			}
		}

		for (const BytePatch& patch : layoutBytes)
		{
			WriteBytePatch(patch, isWide);
		}
	}

	struct StubSite
	{
		std::uintptr_t address;
		std::array<std::uint8_t, 19> stock;
		std::size_t length;
		void(*stub)();
		bool restoresRcxFromRax;
	};

	static const StubSite stubSites[] =
	{
		{ 0x140241BAA, { 0x48, 0x8D, 0x4C, 0x24, 0x28 }, 5, ClientSlots_SnapshotDueAddress, false },
		{ 0x140241CD5, { 0xC6, 0x44, 0x34, 0x28, 0x01 }, 5, ClientSlots_SnapshotDueSet, false },
		{ 0x140241EF8, { 0x80, 0x7C, 0x04, 0x28, 0x00 }, 5, ClientSlots_SnapshotDueTest, false },

		{ 0x140207A7C, { 0x41, 0xB8, 0x05, 0x00, 0x00, 0x00 }, 6, ClientSlots_ServerIndexBitsR8, false },
		{ 0x140207AD3, { 0xC7, 0x44, 0x24, 0x38, 0x05, 0x00, 0x00, 0x00 }, 8, ClientSlots_ServerIndexBitsStack, false },
		{ 0x1402400A4, { 0xBA, 0x05, 0x00, 0x00, 0x00 }, 5, ClientSlots_ServerIndexBitsEdx, false },
		{ 0x1402405FE, { 0xBA, 0x05, 0x00, 0x00, 0x00 }, 5, ClientSlots_ServerIndexBitsEdx, false },
		{ 0x140100539, { 0xBA, 0x05, 0x00, 0x00, 0x00 }, 5, ClientSlots_ClientIndexBitsEdx, false },

		{ 0x1401627BA, { 0x8B, 0x84, 0x96, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskLoad_1401627BA, false },
		{ 0x1401627C4, { 0x89, 0x84, 0x96, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskStore_1401627C4, false },
		{ 0x14018215E, { 0x41, 0x8B, 0x84, 0x97, 0xFC, 0x00, 0x00, 0x00 }, 8, ClientSlots_MaskLoad_14018215E, false },
		{ 0x140182169, { 0x41, 0x89, 0x84, 0x97, 0xFC, 0x00, 0x00, 0x00 }, 8, ClientSlots_MaskStore_140182169, false },
		{ 0x140197A13, { 0x41, 0x8B, 0x84, 0x8D, 0xFC, 0x00, 0x00, 0x00 }, 8, ClientSlots_MaskLoad_140197A13, false },
		{ 0x140197A1E, { 0x41, 0x89, 0x84, 0x8D, 0xFC, 0x00, 0x00, 0x00 }, 8, ClientSlots_MaskStore_140197A1E, false },
		{ 0x140197A8B, { 0x8B, 0x84, 0x88, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskLoad_140197A8B, false },
		{ 0x140197A95, { 0x41, 0x89, 0x84, 0x88, 0xFC, 0x00, 0x00, 0x00 }, 8, ClientSlots_MaskStore_140197A95, false },
		{ 0x1401A3419, { 0x8B, 0x84, 0x2A, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskLoad_1401A3419, false },
		{ 0x1401A3424, { 0x89, 0x84, 0x2A, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskStore_1401A3424, false },
		{ 0x1401A34BE, { 0x8B, 0x84, 0x8B, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskLoad_1401A34BE, false },
		{ 0x1401A34C8, { 0x89, 0x84, 0x8B, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskStore_1401A34C8, false },
		{ 0x1401A3717, { 0x8B, 0x81, 0xFC, 0x00, 0x00, 0x00 }, 6, ClientSlots_MaskLoad_1401A3717, false },
		{ 0x1401A3720, { 0x89, 0x81, 0xFC, 0x00, 0x00, 0x00 }, 6, ClientSlots_MaskStore_1401A3720, false },
		{ 0x1401629BD, { 0x21, 0xB4, 0x83, 0xFC, 0x00, 0x00, 0x00 }, 7, ClientSlots_MaskAnd_1401629BD, false },
		{ 0x1401629B3, { 0xC7, 0x83, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }, 10, ClientSlots_MaskHideAll_1401629B3, false },
		{ 0x140197A69, { 0xC7, 0x80, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }, 10, ClientSlots_MaskHideAll_140197A69, false },
		{ 0x1401A33C9, { 0xC7, 0x85, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }, 10, ClientSlots_MaskHideAll_1401A33C9, false },
		{ 0x1401A34A7, { 0xC7, 0x83, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }, 10, ClientSlots_MaskHideAll_1401A34A7, false },
		{ 0x1401A3672, { 0xC7, 0x83, 0xFC, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF }, 10, ClientSlots_MaskHideAll_1401A3672, false },
		{ 0x140167BD7, { 0x83, 0xE3, 0x1F, 0x0F, 0xB6, 0xCB, 0xD3, 0xE6, 0x48, 0x8B, 0xC8, 0xF7, 0xD6, 0x89, 0xB0, 0xFC, 0x00, 0x00, 0x00 }, 19, ClientSlots_MaskOnlyFor_140167BD7, true },
		{ 0x14023F34E, { 0x41, 0x83, 0xB9, 0xFC, 0x00, 0x00, 0x00, 0xFF }, 8, ClientSlots_MaskHiddenTest_14023F34E, false },
		{ 0x140242864, { 0x8B, 0x81, 0xFC, 0x00, 0x00, 0x00 }, 6, ClientSlots_MaskForClient_140242864, false },
	};

	constexpr std::uintptr_t memsetAddress = 0x140327FA0;
	constexpr std::uintptr_t G_FreeEntity_Memset = 0x1401AB77F;
	constexpr std::uintptr_t G_InitGame_Memset = 0x14019D485;
	constexpr std::uintptr_t BuildItemClientMask = 0x14016E510;
	constexpr std::uintptr_t g_clients_ptr = 0x141867020;
	constexpr std::size_t gentitySize = 0x298;
	constexpr std::size_t gclientSize = 0x3678;

	static Utils::Hook freeEntityHook;
	static Utils::Hook initGameHook;
	static Utils::Hook itemMaskHook;

	static void* EngineMemset(void* place, int value, std::size_t size)
	{
		return reinterpret_cast<void*(*)(void*, int, std::size_t)>(Utils::Hook::Rebase(memsetAddress))(place, value, size);
	}

	static void* G_FreeEntity_Memset_Hk(void* entity, int value, std::size_t size)
	{
		void* const result = EngineMemset(entity, value, size);
		const auto number = (reinterpret_cast<std::uintptr_t>(entity) - reinterpret_cast<std::uintptr_t>(Game::g_entities)) / gentitySize;

		if (number < std::size(ClientSlots_clientMaskHigh))
		{
			std::memset(ClientSlots_clientMaskHigh[number], 0, sizeof(ClientSlots_clientMaskHigh[number]));
		}

		return result;
	}

	static void* G_InitGame_Memset_Hk(void* entities, int value, std::size_t size)
	{
		std::memset(ClientSlots_clientMaskHigh, 0, sizeof(ClientSlots_clientMaskHigh));
		return EngineMemset(entities, value, size);
	}

	static int BuildItemClientMask_Hk(std::uint8_t* item)
	{
		const auto* const clients = *reinterpret_cast<std::uint8_t* const*>(Utils::Hook::Rebase(g_clients_ptr));
		const std::size_t clientCount = ClientSlots::IsServerWide() ? ClientSlots::CLIENT_LIMIT : ClientSlots::BASEGAME_CLIENT_LIMIT;
		std::uint32_t words[ClientSlots::CLIENT_MASK_WORDS]{};

		for (std::size_t i = 0; i < clientCount; ++i)
		{
			const auto flags = *reinterpret_cast<const std::uint32_t*>(clients + i * gclientSize + 0x428);

			if ((flags & 0x400000) != 0)
			{
				continue;
			}

			words[i / 32] |= 1u << (i % 32);
		}

		*reinterpret_cast<std::uint32_t*>(item + 0xFC) = words[0];

		const auto number = *reinterpret_cast<const std::int32_t*>(item);

		if (number >= 0 && static_cast<std::size_t>(number) < std::size(ClientSlots_clientMaskHigh))
		{
			std::memcpy(ClientSlots_clientMaskHigh[number], &words[1], sizeof(ClientSlots_clientMaskHigh[number]));
		}

		return static_cast<int>(words[0]);
	}

	constexpr std::uintptr_t SV_BuildClientSnapshot_FrameCopyCall = 0x14023F382;
	constexpr std::uintptr_t SV_BuildClientSnapshot_BaselineCopyCall = 0x14023F238;
	constexpr std::uintptr_t SV_BuildClientSnapshot_CachedFrameCall = 0x14023F18E;
	constexpr std::uintptr_t memcpyAddress = 0x1402806A0;
	constexpr std::uintptr_t cachedFrameCopy = 0x14023F5E0;
	constexpr std::uintptr_t snapshotEntityNext = 0x143FC3700;
	constexpr std::uintptr_t snapshotEntityRingCount = 0x143FC36F0;
	constexpr std::uintptr_t baselineEntities = 0x1433CEC80;
	constexpr std::size_t baselineEntityCount = 1024;
	constexpr std::size_t entityStateSize = 0x100;

	static_assert(std::size(ClientSlots_snapshotMaskHigh) == snapshotEntityCount && std::size(ClientSlots_baselineMaskHigh) == baselineEntityCount);

	static Utils::Hook frameCopyHook;
	static Utils::Hook baselineCopyHook;
	static Utils::Hook cachedFrameHook;

	static void CopyLiveMaskHigh(std::int32_t number, std::uint32_t (&words)[maskHighWords])
	{
		if (number < 0 || static_cast<std::size_t>(number) >= std::size(ClientSlots_clientMaskHigh))
		{
			std::memset(words, 0, sizeof(words));
			return;
		}

		std::memcpy(words, ClientSlots_clientMaskHigh[number], sizeof(words));
	}

	static void* EngineMemcpy(void* copy, const void* source, std::size_t size)
	{
		return reinterpret_cast<void*(*)(void*, const void*, std::size_t)>(Utils::Hook::Rebase(memcpyAddress))(copy, source, size);
	}

	static void* SV_BuildClientSnapshot_FrameCopy_Hk(void* copy, const void* entity, std::size_t size)
	{
		void* const result = EngineMemcpy(copy, entity, size);
		const auto* const slot = static_cast<std::uint8_t*>(copy);

		if (slot >= ClientSlots_snapshotRingBegin && slot < ClientSlots_snapshotRingEnd)
		{
			CopyLiveMaskHigh(*static_cast<const std::int32_t*>(entity), ClientSlots_snapshotMaskHigh[(slot - ClientSlots_snapshotRingBegin) / entityStateSize]);
		}

		return result;
	}

	static void* SV_BuildClientSnapshot_BaselineCopy_Hk(void* copy, const void* entity, std::size_t size)
	{
		void* const result = EngineMemcpy(copy, entity, size);
		const auto* const slot = static_cast<std::uint8_t*>(copy);

		if (slot >= ClientSlots_baselineBegin && slot < ClientSlots_baselineEnd)
		{
			CopyLiveMaskHigh(*static_cast<const std::int32_t*>(entity), ClientSlots_baselineMaskHigh[(slot - ClientSlots_baselineBegin) / entityStateSize]);
		}

		return result;
	}

	static bool SV_BuildClientSnapshot_CachedFrame_Hk(int time)
	{
		const int first = Utils::Hook::Get<std::int32_t>(snapshotEntityNext);
		const bool isCached = reinterpret_cast<bool(*)(int)>(Utils::Hook::Rebase(cachedFrameCopy))(time);
		const int next = Utils::Hook::Get<std::int32_t>(snapshotEntityNext);
		const int ringCount = Utils::Hook::Get<std::int32_t>(snapshotEntityRingCount);

		if (!ClientSlots_snapshotRingBegin || ringCount <= 0 || static_cast<std::size_t>(ringCount) > std::size(ClientSlots_snapshotMaskHigh))
		{
			return isCached;
		}

		for (int i = first; i != next; ++i)
		{
			const int index = i % ringCount;
			const auto number = *reinterpret_cast<const std::int32_t*>(ClientSlots_snapshotRingBegin + static_cast<std::size_t>(index) * entityStateSize);
			CopyLiveMaskHigh(number, ClientSlots_snapshotMaskHigh[index]);
		}

		return isCached;
	}

	constexpr std::uintptr_t G_FreeEntity_RefsCall = 0x1401AB6CD;
	constexpr std::uintptr_t G_FreeEntityRefs = 0x1401AB820;
	constexpr std::uintptr_t SV_MigrationStart = 0x14023E820;
	constexpr std::uintptr_t SV_MigrationStartJumps[] = { 0x140236190, 0x140236627 };
	constexpr std::uintptr_t SV_MigrationAbort = 0x14023C9C0;
	constexpr std::uintptr_t SV_PacketEvent_MigrationCall = 0x14023CD9C;
	constexpr std::uintptr_t SV_MigrationPacket = 0x14023E730;
	constexpr std::uintptr_t migrationCommands = 0x14039EE40;
	constexpr std::uintptr_t PartyHost_StartMatch_SetMaxClients = 0x140111F51;
	constexpr std::uintptr_t Dvar_SetInt = 0x140287670;

	constexpr std::uintptr_t Dvar_RegisterInt = 0x140286180;
	constexpr std::uintptr_t uiMaxClientsRegisterCalls[] = { 0x14023A40B, 0x140239D33, 0x14019DCBC };

	static Utils::Hook maxClientsRegisterHooks[std::size(uiMaxClientsRegisterCalls)];

	static Game::dvar_t* Dvar_RegisterInt_MaxClients_Hk(const char* name, int value, int min, [[maybe_unused]] int max, unsigned int flags, const char* description)
	{
		return reinterpret_cast<Game::dvar_t*(*)(const char*, int, int, int, unsigned int, const char*)>(Utils::Hook::Rebase(Dvar_RegisterInt))(
			name, value, min, static_cast<int>(ClientSlots::CLIENT_LIMIT), flags, description);
	}

	constexpr std::uintptr_t narrowClientFields[] =
	{
		0x1403908B0,
		0x140390478,
		0x1403936A0,
	};

	constexpr std::uintptr_t clientNumFields[] =
	{
		0x140391C50,
		0x140392988,
		0x140392CA0,
		0x140393850,
		0x140395168,
		0x140394C30,
		0x140393F10,
		0x1403914C8,
		0x140394510,
		0x1403910F0,
		0x1403902E0,
		0x140390AF0,
		0x140393370,
		0x140392178,
		0x140395600,
	};

	constexpr std::uintptr_t FS_NeedRestart = 0x140277AB0;
	constexpr std::uintptr_t CL_ParseGamestate_NeedRestartCall = 0x140100316;
	constexpr std::uintptr_t SV_SpawnServer_PrivateClientsCall = 0x14023B685;

	static Utils::Hook gamestateHook;
	static Utils::Hook spawnServerHook;

	static bool AreNetFieldsExpected()
	{
		const bool areNarrowExpected = std::ranges::all_of(narrowClientFields, [](const std::uintptr_t field)
		{
			return Utils::Hook::Get<std::uint32_t>(field + 16) == 5;
		});

		const bool areClientNumsExpected = std::ranges::all_of(clientNumFields, [](const std::uintptr_t field)
		{
			return Utils::Hook::Get<std::uint32_t>(field + 16) == 6;
		});

		return areNarrowExpected && areClientNumsExpected;
	}

	static void SetWideNetFields(bool isWide)
	{
		for (const std::uintptr_t field : narrowClientFields)
		{
			if (isWide)
			{
				Utils::Hook::Set<std::uint32_t>(field + 16, ClientSlots_wideClientBits);
			}
			else
			{
				Utils::Hook::Set<std::uint32_t>(field + 16, 5);
			}
		}

		for (const std::uintptr_t field : clientNumFields)
		{
			if (isWide)
			{
				Utils::Hook::Set<std::uint32_t>(field + 16, ClientSlots_wideClientBits);
			}
			else
			{
				Utils::Hook::Set<std::uint32_t>(field + 16, 6);
			}
		}
	}

	static int CL_ParseGamestate_NeedRestart_Hk(int checksumFeed)
	{
		const auto* info = Game::CL_GetConfigString(0);
		const bool isWide = std::atoi(Game::Info_ValueForKey(info, "sv_maxclients")) > static_cast<int>(ClientSlots::BASEGAME_CLIENT_LIMIT);

		ClientSlots_clientIndexBits = 5;

		if (isWide)
		{
			ClientSlots_clientIndexBits = ClientSlots_wideClientBits;
		}

		SetWideNetFields(isWide);
		SetWideCgameLayout(isWide);

		return reinterpret_cast<int(*)(int)>(Utils::Hook::Rebase(FS_NeedRestart))(checksumFeed);
	}

	static void SV_SpawnServer_PrivateClients_Hk(Game::dvar_t* dvar, int value)
	{
		SetWideNetFields(ClientSlots::IsServerWide());
		SetWideEntityLayout(ClientSlots::IsServerWide());
		reinterpret_cast<void(*)(Game::dvar_t*, int)>(Utils::Hook::Rebase(Dvar_SetInt))(dvar, value);
	}

	static Utils::Hook freeEntityRefsHook;
	static Utils::Hook migrationStartHooks[std::size(SV_MigrationStartJumps)];
	static Utils::Hook migrationPacketHook;
	static Utils::Hook startMatchHook;

	static void G_FreeEntityRefs_Hk(std::int32_t* entity)
	{
		const auto flags = *reinterpret_cast<const std::uint32_t*>(reinterpret_cast<std::uint8_t*>(entity) + 0x194);
		const auto number = *entity;

		reinterpret_cast<void(*)(std::int32_t*)>(Utils::Hook::Rebase(G_FreeEntityRefs))(entity);

		if ((flags & 0x200000) == 0 || !ClientSlots::IsServerWide())
		{
			return;
		}

		const auto* const entities = reinterpret_cast<std::uint8_t*>(Game::g_entities);

		for (std::size_t i = ClientSlots::BASEGAME_CLIENT_LIMIT; i < ClientSlots::CLIENT_LIMIT; ++i)
		{
			const auto* const player = entities + i * gentityStride;
			auto* const client = *reinterpret_cast<std::uint8_t* const*>(player + 0x158);

			if (player[0x103] && client && *reinterpret_cast<std::int32_t*>(client + 412) == number)
			{
				*reinterpret_cast<std::int32_t*>(client + 412) = 2047;
			}
		}
	}

	static const char* SV_MigrationStart_Hk(const char* reason)
	{
		if (ClientSlots::IsServerWide())
		{
			return reinterpret_cast<const char*(*)()>(Utils::Hook::Rebase(SV_MigrationAbort))();
		}

		return reinterpret_cast<const char*(*)(const char*)>(Utils::Hook::Rebase(SV_MigrationStart))(reason);
	}

	static bool SV_MigrationPacket_Hk(const char* command, void* address, void* message)
	{
		if (!ClientSlots::IsServerWide())
		{
			return reinterpret_cast<bool(*)(const char*, void*, void*)>(Utils::Hook::Rebase(SV_MigrationPacket))(command, address, message);
		}

		const auto* const names = reinterpret_cast<const char* const*>(Utils::Hook::Rebase(migrationCommands));

		for (std::size_t i = 0; names[i * 2] && names[i * 2][0]; ++i)
		{
			if (_stricmp(command, names[i * 2]) == 0)
			{
				return true;
			}
		}

		return false;
	}

	static void PartyHost_StartMatch_SetMaxClients_Hk(Game::dvar_t* dvar, [[maybe_unused]] int value)
	{
		reinterpret_cast<void(*)(Game::dvar_t*, int)>(Utils::Hook::Rebase(Dvar_SetInt))(dvar, static_cast<int>(ClientSlots::CLIENT_LIMIT));
	}

	constexpr std::uintptr_t PartyClient_SessionSync_SetMaxClients = 0x140107715;

	static Utils::Hook partySyncHook;

	static void PartyClient_SessionSync_SetMaxClients_Hk(Game::dvar_t* dvar, int value)
	{
		const int current = dvar->current.integer;

		if (current > static_cast<int>(ClientSlots::BASEGAME_CLIENT_LIMIT) && value < current)
		{
			return;
		}

		reinterpret_cast<void(*)(Game::dvar_t*, int)>(Utils::Hook::Rebase(Dvar_SetInt))(dvar, value);
	}

	constexpr std::uintptr_t Dvar_InfoString = 0x1401FC3C0;
	constexpr std::uintptr_t serverInfoCalls[] = { 0x14023A99A, 0x14023B9DC, 0x14023CFDC };

	static Utils::Hook serverInfoHooks[std::size(serverInfoCalls)];

	static const char* SV_ServerInfo_Hk(int localClientNum, int bit)
	{
		static std::string info;
		info = reinterpret_cast<const char*(*)(int, int)>(Utils::Hook::Rebase(Dvar_InfoString))(localClientNum, bit);

		const std::string_view key = "\\sv_maxclients\\";
		const auto keyAt = info.find(key);

		if (keyAt == std::string::npos)
		{
			return info.c_str();
		}

		const auto valueAt = keyAt + key.size();
		auto valueEnd = info.find('\\', valueAt);

		if (valueEnd == std::string::npos)
		{
			valueEnd = info.size();
		}

		info.replace(valueAt, valueEnd - valueAt, std::to_string(*Game::svs_clientCount));
		return info.c_str();
	}

	constexpr std::uintptr_t SV_DirectConnect = 0x1402374E0;
	constexpr std::uintptr_t SV_AddTestClient_DirectConnectCall = 0x140236E07;
	constexpr std::uintptr_t sv_privateClients_dvar = 0x14650D918;

	static Utils::Hook botConnectHook;
	static int newBotFirstSlot = -1;

	static void SV_AddTestClient_DirectConnect_Hk(void* address)
	{
		auto* const privateClients = *reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(sv_privateClients_dvar));
		const int stock = privateClients->current.integer;
		const int clientCount = *Game::svs_clientCount;
		const auto* const clients = reinterpret_cast<const std::uint8_t*>(Game::svs_clients);
		bool hasBotSlot = false;

		for (int i = static_cast<int>(ClientSlots::BASEGAME_CLIENT_LIMIT); i < clientCount; ++i)
		{
			if (*reinterpret_cast<const std::int32_t*>(clients + static_cast<std::size_t>(i) * 0xA67B0) == 0)
			{
				hasBotSlot = true;
				break;
			}
		}

		if (hasBotSlot && stock < static_cast<int>(ClientSlots::BASEGAME_CLIENT_LIMIT))
		{
			newBotFirstSlot = static_cast<int>(ClientSlots::BASEGAME_CLIENT_LIMIT);
			privateClients->current.integer = newBotFirstSlot;
		}

		reinterpret_cast<void(*)(void*)>(Utils::Hook::Rebase(SV_DirectConnect))(address);
		privateClients->current.integer = stock;
		newBotFirstSlot = -1;
	}

	constexpr std::uintptr_t Hunk_AllocateTempMemory = 0x14027F4C0;
	constexpr std::uintptr_t Hunk_FreeTempMemory = 0x14027F770;
	constexpr std::uintptr_t SV_ChangeMaxClients_AllocCall = 0x140239FD9;
	constexpr std::uintptr_t SV_ChangeMaxClients_FreeCall = 0x14023A0D4;

	static Utils::Hook changeMaxClientsAllocHook;
	static Utils::Hook changeMaxClientsFreeHook;
	static void* changeMaxClientsCopy = nullptr;

	static void* SV_ChangeMaxClients_Alloc_Hk(std::size_t size)
	{
		changeMaxClientsCopy = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

		if (!changeMaxClientsCopy)
		{
			return reinterpret_cast<void*(*)(std::size_t)>(Utils::Hook::Rebase(Hunk_AllocateTempMemory))(size);
		}

		return changeMaxClientsCopy;
	}

	static void SV_ChangeMaxClients_Free_Hk(void* copy)
	{
		if (!copy || copy != changeMaxClientsCopy)
		{
			reinterpret_cast<void(*)(void*)>(Utils::Hook::Rebase(Hunk_FreeTempMemory))(copy);
			return;
		}

		VirtualFree(copy, 0, MEM_RELEASE);
		changeMaxClientsCopy = nullptr;
	}

	constexpr std::uintptr_t Session_GetXuidEvenIfInactive = 0x140243AE0;
	constexpr std::uintptr_t Session_IsUserRegistered = 0x140243DF0;
	constexpr std::uintptr_t PartyHost_RemovePlayer = 0x140110F90;
	constexpr std::uintptr_t PartyHost_UpdateVoiceConnectivityBits = 0x140112B20;
	constexpr std::uintptr_t SV_SetConfigstring = 0x14023ACB0;
	constexpr std::uintptr_t Session_RegisterRemotePlayer = 0x140244240;
	constexpr std::uintptr_t Party_GetClientXNAddr = 0x140109DE0;

	struct MemberGuardSite
	{
		std::uintptr_t address;
		std::uintptr_t target;
		void (*guard)();
		bool isJump;
	};

	constexpr std::uintptr_t CL_ClientIsInMyParty = 0x1400E1190;
	constexpr std::uintptr_t CL_IsPlayerMuted = 0x140102730;
	constexpr std::uintptr_t CL_IsPlayerTalking = 0x140102790;

	static const MemberGuardSite memberGuardSites[] =
	{
		{ 0x14023C76E, Session_GetXuidEvenIfInactive, ClientSlots_GuardSessionGetXuid, false },
		{ 0x14023C7AD, Session_GetXuidEvenIfInactive, ClientSlots_GuardSessionGetXuid, false },
		{ 0x14023D581, Session_GetXuidEvenIfInactive, ClientSlots_GuardSessionGetXuid, false },
		{ 0x1401A3144, Session_GetXuidEvenIfInactive, ClientSlots_GuardSessionGetXuid, false },
		{ 0x14023C3CC, Session_IsUserRegistered, ClientSlots_GuardSessionIsRegistered, false },
		{ 0x14023DAC7, Session_IsUserRegistered, ClientSlots_GuardSessionIsRegistered, false },
		{ 0x14023C405, PartyHost_RemovePlayer, ClientSlots_GuardPartyRemovePlayer, false },
		{ 0x14023DAF2, PartyHost_RemovePlayer, ClientSlots_GuardPartyRemovePlayer, false },
		{ 0x1402389B6, PartyHost_UpdateVoiceConnectivityBits, ClientSlots_GuardPartyVoiceBits, false },
		{ 0x14023DB0E, SV_SetConfigstring, ClientSlots_GuardPlayerInfo, true },

		{ 0x140239AA3, Session_GetXuidEvenIfInactive, ClientSlots_GuardSessionGetXuid, false },
		{ 0x140239ABC, Session_GetXuidEvenIfInactive, ClientSlots_GuardSessionGetXuid, false },
		{ 0x140239ADA, PartyHost_RemovePlayer, ClientSlots_GuardPartyRemovePlayer, false },
		{ 0x140239B0B, Party_GetClientXNAddr, ClientSlots_GuardPartyMemberAddr, false },
		{ 0x140239BCF, Session_RegisterRemotePlayer, ClientSlots_GuardSessionRegister, false },
		{ 0x140239C27, SV_SetConfigstring, ClientSlots_GuardPlayerInfo, false },
		{ 0x140239C37, SV_SetConfigstring, ClientSlots_GuardPlayerInfo, false },
		{ 0x140239C50, PartyHost_RemovePlayer, ClientSlots_GuardPartyRemovePlayer, false },

		{ 0x1400A7D65, CL_ClientIsInMyParty, ClientSlots_GuardClientInMyParty, false },
		{ 0x1400CD7DB, CL_ClientIsInMyParty, ClientSlots_GuardClientInMyParty, false },

		{ 0x1400E3CCA, CL_IsPlayerMuted, ClientSlots_GuardPlayerMuted, false },
		{ 0x1400E3CE9, CL_IsPlayerTalking, ClientSlots_GuardPlayerTalking, false },
	};

	static Utils::Hook memberGuardHooks[std::size(memberGuardSites)];

	constexpr std::uintptr_t G_AntiLagRewindClientPos = 0x140187F30;
	constexpr std::uintptr_t G_AntiLag_RestoreClientPos = 0x140188190;
	constexpr std::uintptr_t Weapon_Melee = 0x140189180;
	constexpr std::uintptr_t Weapon_Melee_outlined = 0x140189440;
	constexpr std::uintptr_t SV_GetCachedSnapshot = 0x14023FB60;
	constexpr std::uintptr_t SV_GetCachedSnapshotInternal = 0x14023FC10;
	constexpr std::uintptr_t BG_EvaluateTrajectory = 0x140089D50;
	constexpr std::uintptr_t SV_LinkEntity = 0x140233F90;
	constexpr std::uintptr_t level_time = 0x1418673E8;
	constexpr std::uintptr_t level_maxclients = 0x1418673E0;
	constexpr std::uintptr_t archivedFrameCount = 0x143FC3708;
	constexpr std::uintptr_t cachedSnapshotEntities = svs + 0x3800;
	constexpr std::uint32_t cachedSnapshotEntityCount = 0x2A30;
	constexpr std::size_t archivedEntitySize = 0x11C;
	constexpr int archivedFrameMsec = 50;
	constexpr int archivedFrameLimit = 1200;
	constexpr int antilagLimitMsec = 400;
	constexpr std::size_t antilagStoreCount = 8;

	struct CachedSnapshot
	{
		int archivedFrame;
		int time;
		int entityCount;
		int firstEntity;
		int clientCount;
		int firstClient;
		int usesDelta;
	};

	struct AntilagStore
	{
		float origins[ClientSlots::CLIENT_LIMIT][3];
		float angles[ClientSlots::CLIENT_LIMIT][3];
		bool isRewound[ClientSlots::CLIENT_LIMIT];
	};

	struct AntilagStoreSlot
	{
		const void* owner;
		int levelTime;
		AntilagStore store;
	};

	static AntilagStoreSlot antilagStores[antilagStoreCount]{};

	static Utils::Hook antilagRewindHook;
	static Utils::Hook antilagRestoreHook;
	static Utils::Hook weaponMeleeHook;

	static void GetClientPositionsFromCachedSnap(const CachedSnapshot& snapshot, float origins[][3], float angles[][3], bool* isValid)
	{
		const auto* const entities = reinterpret_cast<const std::uint8_t*>(Utils::Hook::Rebase(cachedSnapshotEntities));
		const auto evaluateTrajectory = reinterpret_cast<void(*)(const void*, int, float*)>(Utils::Hook::Rebase(BG_EvaluateTrajectory));

		for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(snapshot.entityCount); ++i)
		{
			const auto* const entity = entities + ((i + static_cast<std::uint32_t>(snapshot.firstEntity)) % cachedSnapshotEntityCount) * archivedEntitySize;
			const auto number = *reinterpret_cast<const std::int32_t*>(entity);

			if (number < 0 || number >= static_cast<std::int32_t>(ClientSlots::CLIENT_LIMIT))
			{
				break;
			}

			isValid[number] = true;
			evaluateTrajectory(entity + 0xC, snapshot.time, origins[number]);
			angles[number][0] = 0.0f;
			angles[number][1] = *reinterpret_cast<const float*>(entity + 0x40);
			angles[number][2] = 0.0f;
		}
	}

	static bool GetClientPositionsAtTime(int time, float origins[][3], float angles[][3], bool* isValid)
	{
		const auto getSnapshot = reinterpret_cast<const CachedSnapshot*(*)(int*)>(Utils::Hook::Rebase(SV_GetCachedSnapshot));
		const auto getSnapshotAt = reinterpret_cast<const CachedSnapshot*(*)(int, int, bool)>(Utils::Hook::Rebase(SV_GetCachedSnapshotInternal));
		const auto& frameCount = *reinterpret_cast<const int*>(Utils::Hook::Rebase(archivedFrameCount));

		const int framesBack = (*Game::svs_time - time) / archivedFrameMsec;
		int fromMsec = archivedFrameMsec * framesBack + 2 * archivedFrameMsec;
		int toMsec = archivedFrameMsec * framesBack + archivedFrameMsec;
		const CachedSnapshot* from = getSnapshot(&fromMsec);
		const CachedSnapshot* to = getSnapshot(&toMsec);

		if (!from && !to)
		{
			return false;
		}

		for (int step = 0; to && to->time < time; --step)
		{
			from = to;
			to = nullptr;
			toMsec = archivedFrameMsec * (framesBack + step);

			if (toMsec <= 0)
			{
				break;
			}

			int frame = frameCount - toMsec / archivedFrameMsec;

			if (frame < frameCount - archivedFrameLimit)
			{
				frame = frameCount - archivedFrameLimit;
				toMsec = archivedFrameMsec * (frameCount - frame);
			}

			if (frame < 0)
			{
				frame = 0;
				toMsec = archivedFrameMsec * frameCount;
			}

			while (frame < frameCount)
			{
				to = getSnapshotAt(frame, 0, true);

				if (to)
				{
					break;
				}

				++frame;
				toMsec = archivedFrameMsec * (frameCount - frame);
			}

			if (!to)
			{
				toMsec = 0;
				break;
			}
		}

		for (int step = 3; from && from->time > time; ++step)
		{
			fromMsec = archivedFrameMsec * (framesBack + step);
			from = nullptr;

			if (fromMsec <= 0)
			{
				break;
			}

			int frame = frameCount - fromMsec / archivedFrameMsec;

			if (frame < frameCount - archivedFrameLimit)
			{
				frame = frameCount - archivedFrameLimit;
				fromMsec = archivedFrameMsec * (frameCount - frame);
			}

			if (frame < 0)
			{
				frame = 0;
				fromMsec = archivedFrameMsec * frameCount;
			}

			while (frame < frameCount)
			{
				from = getSnapshotAt(frame, 0, true);

				if (from)
				{
					break;
				}

				++frame;
				fromMsec = archivedFrameMsec * (frameCount - frame);
			}

			if (!from)
			{
				fromMsec = 0;
				break;
			}
		}

		float fromOrigins[ClientSlots::CLIENT_LIMIT][3];
		float fromAngles[ClientSlots::CLIENT_LIMIT][3];
		float toOrigins[ClientSlots::CLIENT_LIMIT][3];
		float toAngles[ClientSlots::CLIENT_LIMIT][3];
		bool isFromValid[ClientSlots::CLIENT_LIMIT]{};
		bool isToValid[ClientSlots::CLIENT_LIMIT]{};

		if (from)
		{
			GetClientPositionsFromCachedSnap(*from, fromOrigins, fromAngles, isFromValid);
		}

		if (to)
		{
			GetClientPositionsFromCachedSnap(*to, toOrigins, toAngles, isToValid);
		}

		float lerp = 0.0f;

		if (from && to && fromMsec != toMsec)
		{
			lerp = static_cast<float>(time - from->time) / static_cast<float>(to->time - from->time);
		}

		for (std::size_t i = 0; i < ClientSlots::CLIENT_LIMIT; ++i)
		{
			if (isFromValid[i])
			{
				isValid[i] = true;

				for (std::size_t axis = 0; axis < 3; ++axis)
				{
					if (isToValid[i])
					{
						origins[i][axis] = (toOrigins[i][axis] - fromOrigins[i][axis]) * lerp + fromOrigins[i][axis];
						angles[i][axis] = (toAngles[i][axis] - fromAngles[i][axis]) * lerp + fromAngles[i][axis];
					}
					else
					{
						origins[i][axis] = fromOrigins[i][axis];
						angles[i][axis] = fromAngles[i][axis];
					}
				}
			}
			else if (isToValid[i])
			{
				isValid[i] = true;

				for (std::size_t axis = 0; axis < 3; ++axis)
				{
					origins[i][axis] = toOrigins[i][axis];
					angles[i][axis] = toAngles[i][axis];
				}
			}
		}

		return true;
	}

	static std::size_t AntilagClientCount()
	{
		const auto maxClients = *reinterpret_cast<const int*>(Utils::Hook::Rebase(level_maxclients));
		return std::clamp<std::size_t>(static_cast<std::size_t>(std::max(maxClients, 0)), 0, ClientSlots::CLIENT_LIMIT);
	}

	static void RewindClients(int gameTime, AntilagStore& store)
	{
		store = {};

		const int levelTime = *reinterpret_cast<const int*>(Utils::Hook::Rebase(level_time));

		if (levelTime - gameTime > antilagLimitMsec)
		{
			gameTime = levelTime - antilagLimitMsec;
		}

		float origins[ClientSlots::CLIENT_LIMIT][3];
		float angles[ClientSlots::CLIENT_LIMIT][3];
		bool isValid[ClientSlots::CLIENT_LIMIT]{};

		if (!GetClientPositionsAtTime(gameTime, origins, angles, isValid))
		{
			return;
		}

		const auto linkEntity = reinterpret_cast<void(*)(void*)>(Utils::Hook::Rebase(SV_LinkEntity));
		const auto* const clients = *reinterpret_cast<std::uint8_t* const*>(Utils::Hook::Rebase(g_clients_ptr));
		auto* const entities = reinterpret_cast<std::uint8_t*>(Game::g_entities);
		const auto clientCount = AntilagClientCount();

		for (std::size_t i = 0; i < clientCount; ++i)
		{
			const auto* const client = clients + i * gclientSize;
			const bool isPlaying = *reinterpret_cast<const std::int32_t*>(client + 0x3148) == 2 && *reinterpret_cast<const std::int32_t*>(client + 0x311C) == 0;

			if (!isValid[i] || !isPlaying)
			{
				continue;
			}

			auto* const entity = entities + i * gentityStride;
			auto* const origin = reinterpret_cast<float*>(entity + 0x138);
			auto* const angle = reinterpret_cast<float*>(entity + 0x144);

			std::memcpy(store.origins[i], origin, sizeof(store.origins[i]));
			std::memcpy(store.angles[i], angle, sizeof(store.angles[i]));
			std::memcpy(origin, origins[i], sizeof(origins[i]));
			std::memcpy(angle, angles[i], sizeof(angles[i]));
			linkEntity(entity);
			store.isRewound[i] = true;
		}
	}

	static void RestoreClients(AntilagStore& store)
	{
		const auto linkEntity = reinterpret_cast<void(*)(void*)>(Utils::Hook::Rebase(SV_LinkEntity));
		auto* const entities = reinterpret_cast<std::uint8_t*>(Game::g_entities);
		const auto clientCount = AntilagClientCount();

		for (std::size_t i = 0; i < clientCount; ++i)
		{
			if (!store.isRewound[i])
			{
				continue;
			}

			auto* const entity = entities + i * gentityStride;
			std::memcpy(entity + 0x138, store.origins[i], sizeof(store.origins[i]));
			std::memcpy(entity + 0x144, store.angles[i], sizeof(store.angles[i]));
			linkEntity(entity);
			store.isRewound[i] = false;
		}
	}

	static void G_AntiLagRewindClientPos_Hk(int gameTime, void* callerStore)
	{
		const int levelTime = *reinterpret_cast<const int*>(Utils::Hook::Rebase(level_time));
		AntilagStoreSlot* chosen = nullptr;

		for (auto& slot : antilagStores)
		{
			if (slot.owner == callerStore)
			{
				chosen = &slot;
				break;
			}
		}

		if (!chosen)
		{
			for (auto& slot : antilagStores)
			{
				if (!slot.owner || slot.levelTime != levelTime)
				{
					chosen = &slot;
					break;
				}
			}
		}

		if (!chosen)
		{
			return;
		}

		chosen->owner = callerStore;
		chosen->levelTime = levelTime;
		RewindClients(gameTime, chosen->store);
	}

	static void G_AntiLag_RestoreClientPos_Hk(void* callerStore)
	{
		for (auto& slot : antilagStores)
		{
			if (slot.owner == callerStore)
			{
				RestoreClients(slot.store);
				slot.owner = nullptr;
				return;
			}
		}
	}

	static std::int64_t Weapon_Melee_Hk(void* entity, void* weapon, float range, float width, float height, int gameTime)
	{
		AntilagStore store;
		RewindClients(gameTime, store);

		const auto result = reinterpret_cast<std::int64_t(*)(void*, void*, float, float, float)>(Utils::Hook::Rebase(Weapon_Melee_outlined))(entity, weapon, range, width, height);

		RestoreClients(store);
		return result;
	}

	constexpr std::uint32_t stockParentSize = 0x9000;
	constexpr std::uint32_t stockChildIds = 0xC800;
	constexpr std::uint32_t stockChild0Begin = stockParentSize + 2;
	constexpr std::uint32_t stockChildSpan = stockChildIds + 1;
	constexpr std::uint32_t stockChild1Begin = stockChild0Begin + stockChildSpan;
	constexpr std::uint32_t stockVariableCount = stockChild1Begin + stockChildSpan;
	constexpr std::uint32_t stockChildHash = stockChildIds - 1;
	constexpr std::uint32_t stockChildHashMagic = 0x51EBEDFB;
	constexpr std::uint32_t stockObjectNameEnd = 0x10000 + stockParentSize;

	constexpr std::uint32_t stockChildPools = 2;
	constexpr std::uint32_t childPools = 8;

	constexpr std::uint32_t parentSize = 0xFFFE;
	constexpr std::uint32_t childIds = 0xFFFE;
	constexpr std::uint32_t child0Begin = parentSize + 2;
	constexpr std::uint32_t childSpan = childIds + 1;
	constexpr std::uint32_t child1Begin = child0Begin + childSpan;
	constexpr std::uint32_t variableCount = child0Begin + childPools * childSpan;
	constexpr std::uint32_t childHash = childIds - 1;
	constexpr std::uint32_t childHashMagic = 0x80018005;
	constexpr std::uint8_t childHashShift = 0x0F;
	constexpr std::uint32_t objectNameEnd = 0x10000 + parentSize;

	static_assert(stockChild0Begin == 0x9002 && stockChild1Begin == 0x15803 && stockVariableCount == 0x22004 && stockChildHash == 0xC7FF);
	static_assert(child0Begin == 0x10000 && child1Begin == 0x1FFFF && variableCount == 0x8FFF8 && childHash == 0xFFFD);

	static_assert(childSpan == 0xFFFF && childPools <= 0xFFFF);
	static_assert(objectNameEnd < 0x800000);

	constexpr std::uintptr_t stockIdMap = 0x141F29968;
	constexpr std::uintptr_t stockIdMapRev = 0x141F3B968;
	constexpr std::uintptr_t stockVariableList = 0x141F4D980;
	constexpr std::size_t variableSize = 24;
	constexpr std::size_t idMapSize = 0x20000;
	constexpr std::size_t variableListOffset = 2 * idMapSize;

	struct ScriptVarReference
	{
		std::uintptr_t address;
		std::uint8_t length;
		std::uint8_t operandOffset;
		SiteBase base;
		std::uintptr_t target;
	};

	static const ScriptVarReference scriptVarReferences[] =
	{
		{ 0x14021F2EF, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F327, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x14021F343, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F376, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x14021F3A3, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x14021F3AC, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F3B4, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14021F3D9, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x14021F406, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x14021F450, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F463, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x14021F490, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x14021F499, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021F4A7, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x14021F4D4, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x14021F7E4, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F80A, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021F825, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021F83B, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F85E, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021F874, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F890, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021F8A6, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F8D8, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021F8F6, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F910, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021F9AB, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x14021F9C3, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14021F9CD, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14021FB81, 7, 3, SiteBase::Rip, 0x141F3B968 },
		{ 0x14021FBDB, 7, 3, SiteBase::Rip, 0x141F29968 },
		{ 0x14021FC03, 7, 3, SiteBase::Rip, 0x141F3B968 },
		{ 0x14021FC4D, 7, 3, SiteBase::Rip, 0x141F29968 },
		{ 0x14021FF87, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x14021FF9E, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x14021FFB3, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14021FFDE, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x140220001, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x14022001F, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x140220039, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140220056, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x140220075, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x1402200A5, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x1402200BC, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x1402200C4, 8, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x1402200D0, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x1402202BE, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x1402202C9, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140220397, 7, 3, SiteBase::Rip, 0x141F3B968 },
		{ 0x14022045A, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402205E9, 7, 3, SiteBase::Rip, 0x141F3B968 },
		{ 0x140220684, 8, 4, SiteBase::Image, 0x141F3B968 },
		{ 0x1402207E5, 8, 4, SiteBase::Image, 0x141F3B968 },
		{ 0x140220810, 8, 4, SiteBase::Image, 0x141F3B968 },
		{ 0x140220869, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140220870, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14022099C, 7, 3, SiteBase::Rip, 0x141F29968 },
		{ 0x1402209B0, 7, 3, SiteBase::Rip, 0x141F3B968 },
		{ 0x1402209EC, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x1402209FC, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140220AFD, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x140220B2A, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x140220BB8, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140220BCA, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x140220BF7, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x140220C16, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x140220C43, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x140220D25, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x140220D52, 9, 5, SiteBase::Image, 0x141F3B968 },
		{ 0x140220E1C, 8, 4, SiteBase::Image, 0x141F3B968 },
		{ 0x140220EC7, 7, 3, SiteBase::Rip, 0x141F29968 },
		{ 0x1402226E7, 7, 3, SiteBase::Rip, 0x141F4D988 },
		{ 0x140222712, 7, 3, SiteBase::Rip, 0x141F4D988 },
		{ 0x14022276F, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x14022282F, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402228FF, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140222966, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402229A9, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402229DD, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140222A32, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140222A7E, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x140222A8E, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x140222ABF, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140222AF2, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x140222B60, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140222CA9, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140222D00, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x140222D0C, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x140222D2A, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140222D5C, 9, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140222DEC, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x140222E22, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140222E34, 9, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140222ED9, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140222F79, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402230EE, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x140223121, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140223199, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x1402231B1, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x1402231C6, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x1402231FD, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223292, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223421, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402235B3, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223641, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x140223668, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x140223687, 7, 3, SiteBase::Rip, 0x141F4D996 },
		{ 0x1402236A6, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223727, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223761, 7, 3, SiteBase::Rip, 0x141F4D988 },
		{ 0x1402237BC, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402237E7, 7, 3, SiteBase::Rip, 0x141F4D982 },
		{ 0x140223848, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223878, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223961, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140223969, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x1402239B8, 9, 5, SiteBase::Image, 0x141F4D994 },
		{ 0x1402239C3, 9, 5, SiteBase::Image, 0x141F4D996 },
		{ 0x1402239EC, 8, 4, SiteBase::Image, 0x141F4D982 },
		{ 0x140223A06, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140223A15, 9, 5, SiteBase::Image, 0x141F4D996 },
		{ 0x140223A2F, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140223A38, 9, 5, SiteBase::Image, 0x141F4D996 },
		{ 0x140223A4E, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x140223A68, 9, 5, SiteBase::Image, 0x141F4D994 },
		{ 0x140223A75, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140223A7E, 9, 5, SiteBase::Image, 0x141F4D99A },
		{ 0x140223A87, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140223A93, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140223AA0, 9, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x140223AC8, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x140223B07, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140223B0E, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140223B5D, 9, 5, SiteBase::Image, 0x141F4D994 },
		{ 0x140223B69, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140223B86, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140223B99, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140223BAE, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x140223BC1, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x140223BED, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223C67, 7, 3, SiteBase::Rip, 0x141F4D98A },
		{ 0x140223CE7, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223DDD, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223E2A, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223E86, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223EC4, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223F22, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140223F66, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x14022406C, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x140224078, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x1402240DE, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x14022410A, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140224119, 9, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x14022412A, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x14022417D, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x140224193, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x1402241A2, 9, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x1402241B2, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x1402241D7, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x1402241E6, 9, 5, SiteBase::Image, 0x141F4D996 },
		{ 0x1402241F9, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x14022421E, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x140224246, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140224255, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x14022426F, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x14022429B, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x1402242C3, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x1402242D2, 9, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x1402242E2, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x1402242F1, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x140224324, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x140224339, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x14022435B, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x140224372, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140224381, 9, 5, SiteBase::Image, 0x141F4D996 },
		{ 0x140224398, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x1402243B1, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x1402243C1, 9, 5, SiteBase::Image, 0x141F4D994 },
		{ 0x1402243D9, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x1402243E9, 9, 5, SiteBase::Image, 0x141F4D994 },
		{ 0x140224400, 9, 5, SiteBase::Image, 0x141F4D994 },
		{ 0x14022448D, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x1402244A0, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x1402244F6, 9, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x14022458A, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x14022466D, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402246F1, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140224757, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x1402247D1, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402247F7, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140224819, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140224847, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140224887, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x1402248FD, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140224987, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x1402249B1, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x1402249E1, 7, 3, SiteBase::Rip, 0x141F4D988 },
		{ 0x140224A07, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140224A48, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140224CE3, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140224DB2, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140224DF4, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140224E90, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140224EA1, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140224EAF, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140224EC3, 9, 4, SiteBase::Image, 0x141F4D996 },
		{ 0x140224ED2, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x140224EEE, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140224F13, 9, 4, SiteBase::Image, 0x141F4D996 },
		{ 0x140224F23, 9, 5, SiteBase::Image, 0x141F4D994 },
		{ 0x140224F2C, 11, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140224F46, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140224F57, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140224F63, 9, 5, SiteBase::Image, 0x141F4D982 },
		{ 0x140224F70, 9, 5, SiteBase::Image, 0x141F4D99A },
		{ 0x1402250AE, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140225109, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140225192, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402251E9, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402253D7, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x1402253EF, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x1402253FA, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140225405, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140225452, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140225479, 9, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140225482, 9, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14022548B, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14022580C, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140225996, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140225A3D, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140225A86, 7, 3, SiteBase::Rip, 0x1420259B8 },
		{ 0x140225AA8, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140225B12, 8, 4, SiteBase::Rip, 0x1420259B8 },
		{ 0x1402260C7, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140226162, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140226184, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14022619B, 10, 5, SiteBase::Image, 0x141F4D996 },
		{ 0x140226200, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x14022620F, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14022621C, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140226243, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14022624B, 9, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x140226254, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x1402263A8, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140226572, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x1402265DF, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x1402265EF, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x14022660E, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140226644, 9, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14022671C, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x140226735, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x14022675D, 7, 3, SiteBase::Image, 0x141F4D980 },
		{ 0x140226787, 11, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140226792, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x1402267C6, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x1402267D8, 9, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140227187, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14022719E, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140227290, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140227B13, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140227B21, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x140227CA5, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140227CFE, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140227D0B, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x140227D29, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140227D53, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140227D62, 10, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x140227DD2, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140227EC1, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140227F37, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228080, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x14022810C, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228207, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x140228220, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14022823F, 8, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140228247, 8, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x14022824F, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x140228257, 8, 4, SiteBase::Image, 0x141F4D98A },
		{ 0x1402282E4, 7, 3, SiteBase::Rip, 0x141F2996A },
		{ 0x1402282EB, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140228458, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140228467, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140228474, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14022849B, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x1402284A3, 9, 5, SiteBase::Image, 0x141F4D988 },
		{ 0x1402284AC, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x1402284EF, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228591, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x1402285A8, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x1402285E7, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228615, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140228736, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228889, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x1402288CC, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x14022899C, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228A85, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228B2F, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228B6F, 9, 4, SiteBase::Image, 0x141F29968 },
		{ 0x140228B83, 8, 4, SiteBase::Image, 0x141F29968 },
		{ 0x140228B92, 8, 4, SiteBase::Image, 0x141F4D996 },
		{ 0x140228BCF, 7, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140228BDC, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x140228BE8, 8, 4, SiteBase::Image, 0x141F4D996 },
		{ 0x140228BFA, 8, 4, SiteBase::Image, 0x141F4D980 },
		{ 0x140228CA9, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228D14, 11, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140228D1F, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x140228D42, 11, 3, SiteBase::Image, 0x141F4D990 },
		{ 0x140228D4D, 7, 3, SiteBase::Image, 0x141F4D988 },
		{ 0x140228D6D, 7, 3, SiteBase::Rip, 0x141F4D990 },
		{ 0x140228D8A, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228DC9, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228EEF, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x140228F68, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x140228FD3, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140228FE0, 9, 5, SiteBase::Image, 0x141F4D98A },
		{ 0x140228FFC, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x14022904F, 9, 5, SiteBase::Image, 0x141F4D980 },
		{ 0x140229061, 8, 4, SiteBase::Image, 0x141F4D990 },
		{ 0x14022906C, 8, 4, SiteBase::Image, 0x141F4D988 },
		{ 0x1402290D9, 7, 3, SiteBase::Rip, 0x141F4D980 },
		{ 0x14022ADE6, 7, 3, SiteBase::Rip, 0x141F29968 },
	};

	static const ImmediatePatch scriptVarImmediates[] =
	{
		{ 0x140182D80, 3, stockChildSpan, childSpan },
		{ 0x140182D92, 4, stockChild0Begin, child0Begin },
		{ 0x140211DC8, 3, stockChildSpan, childSpan },
		{ 0x140211DD4, 3, stockChild0Begin, child0Begin },
		{ 0x1402121BD, 2, stockChildSpan, childSpan },
		{ 0x1402121E2, 2, stockChild0Begin, child0Begin },
		{ 0x14021220A, 2, stockChild0Begin, child0Begin },
		{ 0x1402124AC, 2, stockChildSpan, childSpan },
		{ 0x1402124B2, 1, stockChild0Begin, child0Begin },
		{ 0x14021AE1C, 2, stockChildSpan, childSpan },
		{ 0x14021AE25, 1, stockChild0Begin, child0Begin },
		{ 0x14021AEE8, 2, stockChildSpan, childSpan },
		{ 0x14021AEF3, 1, stockChild0Begin, child0Begin },
		{ 0x14021AFD5, 2, stockChildSpan, childSpan },
		{ 0x14021AFDE, 2, stockChild0Begin, child0Begin },
		{ 0x14021D2DB, 2, stockChildSpan, childSpan },
		{ 0x14021D2E4, 1, stockChild0Begin, child0Begin },
		{ 0x14021D457, 2, stockChildSpan, childSpan },
		{ 0x14021D462, 1, stockChild0Begin, child0Begin },
		{ 0x14021F2F7, 2, stockChildSpan, childSpan },
		{ 0x14021F300, 2, stockChild0Begin, child0Begin },
		{ 0x14021F94C, 3, stockChildSpan, childSpan },
		{ 0x14021F953, 3, stockChild0Begin, child0Begin },
		{ 0x1402200EB, 3, stockChildSpan, childSpan },
		{ 0x1402200F6, 3, stockChild0Begin, child0Begin },
		{ 0x1402203AA, 2, stockParentSize * 2, parentSize * 2 },
		{ 0x140220438, 1, stockChild0Begin, child0Begin },
		{ 0x140220461, 1, stockChild0Begin, child0Begin },
		{ 0x140220860, 1, stockChild0Begin, child0Begin },
		{ 0x1402209A3, 2, stockParentSize * 2, parentSize * 2 },
		{ 0x1402209B7, 2, stockParentSize * 2, parentSize * 2 },
		{ 0x1402209DB, 1, stockChild0Begin, child0Begin },
		{ 0x140220A7C, 3, stockChildSpan, childSpan },
		{ 0x140220AB4, 2, stockChildSpan, childSpan },
		{ 0x140220ABC, 2, stockChild0Begin, child0Begin },
		{ 0x140220B44, 3, stockChild0Begin, child0Begin },
		{ 0x140220B65, 3, stockChildSpan, childSpan },
		{ 0x140220B94, 2, stockChildSpan, childSpan },
		{ 0x140220B9C, 2, stockChild0Begin, child0Begin },
		{ 0x140220C61, 3, stockChild0Begin, child0Begin },
		{ 0x140220C82, 2, stockChildSpan, childSpan },
		{ 0x140220C88, 1, stockChild0Begin, child0Begin },
		{ 0x140220CB7, 3, stockChildSpan, childSpan },
		{ 0x140220CC0, 3, stockChild0Begin, child0Begin },
		{ 0x140220CF2, 2, stockChildSpan, childSpan },
		{ 0x140220CFA, 2, stockChild0Begin, child0Begin },
		{ 0x1402227B0, 2, stockChildSpan, childSpan },
		{ 0x1402227B6, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402227C1, 2, stockChild0Begin, child0Begin },
		{ 0x1402227CA, 2, stockChildHash, childHash },
		{ 0x1402227DE, 2, stockChild0Begin, child0Begin },
		{ 0x1402227F6, 1, stockChild0Begin, child0Begin },
		{ 0x14022280C, 3, stockChildSpan, childSpan },
		{ 0x140222820, 3, stockChild0Begin, child0Begin },
		{ 0x14022285A, 1, stockChildHashMagic, childHashMagic },
		{ 0x140222872, 2, stockChildHash, childHash },
		{ 0x140222888, 2, stockChild0Begin, child0Begin },
		{ 0x1402228AB, 2, stockChild0Begin, child0Begin },
		{ 0x140222A4B, 1, stockChild0Begin - 1, child0Begin - 1 },
		{ 0x140222CC9, 2, stockChildSpan, childSpan },
		{ 0x140222CCF, 1, stockChildHashMagic, childHashMagic },
		{ 0x140222CDA, 2, stockChildHash, childHash },
		{ 0x140222CE0, 2, stockChild0Begin, child0Begin },
		{ 0x140222CF4, 2, stockChild0Begin, child0Begin },
		{ 0x140222D1A, 1, stockChild0Begin, child0Begin },
		{ 0x140222FA2, 2, stockChildSpan, childSpan },
		{ 0x140222FA8, 2, stockChild0Begin, child0Begin },
		{ 0x140222FB5, 1, stockChildHashMagic, childHashMagic },
		{ 0x140222FD2, 2, stockChildHash, childHash },
		{ 0x140223090, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402230A3, 2, stockChildHash, childHash },
		{ 0x1402230B3, 2, stockChildSpan, childSpan },
		{ 0x1402230BD, 2, stockChild0Begin, child0Begin },
		{ 0x1402230CE, 2, stockChild0Begin, child0Begin },
		{ 0x1402230D9, 2, stockChild0Begin, child0Begin },
		{ 0x140223150, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022316D, 2, stockChildHash, childHash },
		{ 0x140223176, 2, stockChildSpan, childSpan },
		{ 0x140223182, 2, stockChild0Begin, child0Begin },
		{ 0x14022318D, 2, stockChild0Begin, child0Begin },
		{ 0x1402231A5, 1, stockChild0Begin, child0Begin },
		{ 0x1402232CE, 3, stockChildSpan, childSpan },
		{ 0x1402232D5, 2, stockChildSpan, childSpan },
		{ 0x1402232DB, 3, stockChild0Begin, child0Begin },
		{ 0x1402232E2, 2, stockChild0Begin, child0Begin },
		{ 0x14022342E, 2, stockChildSpan, childSpan },
		{ 0x140223439, 3, stockChildSpan, childSpan },
		{ 0x140223447, 2, stockChild0Begin, child0Begin },
		{ 0x140223453, 3, stockChild0Begin, child0Begin },
		{ 0x140223483, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022349F, 2, stockChildHash, childHash },
		{ 0x140223579, 2, stockChildSpan, childSpan },
		{ 0x14022357F, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022358E, 2, stockChild0Begin, child0Begin },
		{ 0x140223597, 2, stockChildHash, childHash },
		{ 0x1402235AB, 2, stockChild0Begin, child0Begin },
		{ 0x140223607, 2, stockChildSpan, childSpan },
		{ 0x140223610, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022361B, 2, stockChild0Begin, child0Begin },
		{ 0x140223621, 2, stockChildHash, childHash },
		{ 0x140223635, 2, stockChild0Begin, child0Begin },
		{ 0x140223658, 1, stockChild0Begin, child0Begin },
		{ 0x1402236CE, 3, stockChildSpan, childSpan },
		{ 0x1402236D5, 3, stockChild0Begin, child0Begin },
		{ 0x1402236DE, 3, stockChild0Begin, child0Begin },
		{ 0x1402236EE, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223704, 2, stockChildHash, childHash },
		{ 0x140223753, 2, stockChildSpan, childSpan },
		{ 0x140223759, 2, stockChild0Begin, child0Begin },
		{ 0x140223786, 2, stockChildSpan, childSpan },
		{ 0x14022378C, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223797, 2, stockChild0Begin, child0Begin },
		{ 0x1402237A0, 2, stockChildHash, childHash },
		{ 0x1402237B4, 2, stockChild0Begin, child0Begin },
		{ 0x140223812, 2, stockChildSpan, childSpan },
		{ 0x140223818, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223823, 2, stockChild0Begin, child0Begin },
		{ 0x14022382C, 2, stockChildHash, childHash },
		{ 0x140223840, 2, stockChild0Begin, child0Begin },
		{ 0x140223946, 2, stockChildSpan, childSpan },
		{ 0x14022394C, 2, stockChild0Begin, child0Begin },
		{ 0x140223BF4, 2, stockChildSpan, childSpan },
		{ 0x140223BFA, 2, stockChild0Begin, child0Begin },
		{ 0x140223CA0, 2, stockChildSpan, childSpan },
		{ 0x140223CA8, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223CB1, 2, stockChild0Begin, child0Begin },
		{ 0x140223CC2, 2, stockChildHash, childHash },
		{ 0x140223D28, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223D3A, 2, stockChildHash, childHash },
		{ 0x140223D4D, 2, stockChildSpan, childSpan },
		{ 0x140223D53, 2, stockChild0Begin, child0Begin },
		{ 0x140223D9D, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223DB8, 2, stockChildHash, childHash },
		{ 0x140223DCF, 2, stockChildSpan, childSpan },
		{ 0x140223DD5, 2, stockChild0Begin, child0Begin },
		{ 0x140223E1C, 2, stockChildSpan, childSpan },
		{ 0x140223E22, 2, stockChild0Begin, child0Begin },
		{ 0x140223E4D, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223E61, 2, stockChildHash, childHash },
		{ 0x140223E78, 2, stockChildSpan, childSpan },
		{ 0x140223E7E, 2, stockChild0Begin, child0Begin },
		{ 0x140223EB6, 2, stockChildSpan, childSpan },
		{ 0x140223EBC, 2, stockChild0Begin, child0Begin },
		{ 0x140223EED, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223EFD, 2, stockChildHash, childHash },
		{ 0x140223F14, 2, stockChildSpan, childSpan },
		{ 0x140223F1A, 2, stockChild0Begin, child0Begin },
		{ 0x140223F8A, 2, stockChildSpan, childSpan },
		{ 0x140223F90, 2, stockChild0Begin, child0Begin },
		{ 0x140223FBB, 1, stockChildHashMagic, childHashMagic },
		{ 0x140223FD0, 2, stockChildHash, childHash },
		{ 0x140224066, 2, stockChildSpan, childSpan },
		{ 0x14022408D, 2, stockChild0Begin, child0Begin },
		{ 0x1402244BD, 3, stockObjectNameEnd, objectNameEnd },
		{ 0x140224563, 1, stockChildHashMagic, childHashMagic },
		{ 0x140224574, 2, stockChildHash, childHash },
		{ 0x1402245AA, 2, stockChildSpan, childSpan },
		{ 0x1402245B0, 2, stockChild0Begin, child0Begin },
		{ 0x1402245E1, 1, stockChildHashMagic, childHashMagic },
		{ 0x140224608, 2, stockChildHash, childHash },
		{ 0x140224674, 2, stockChildSpan, childSpan },
		{ 0x14022467A, 2, stockChild0Begin, child0Begin },
		{ 0x14022478E, 2, stockChildSpan, childSpan },
		{ 0x140224796, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402247A1, 2, stockChild0Begin, child0Begin },
		{ 0x1402247AC, 2, stockChildHash, childHash },
		{ 0x1402248BC, 2, stockChildSpan, childSpan },
		{ 0x1402248C2, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402248CD, 2, stockChild0Begin, child0Begin },
		{ 0x1402248D8, 2, stockChildHash, childHash },
		{ 0x140224932, 1, stockChildHashMagic, childHashMagic },
		{ 0x140224940, 2, stockChildHash, childHash },
		{ 0x140224953, 2, stockChildSpan, childSpan },
		{ 0x140224959, 2, stockChild0Begin, child0Begin },
		{ 0x1402249A3, 2, stockChildSpan, childSpan },
		{ 0x1402249A9, 2, stockChild0Begin, child0Begin },
		{ 0x1402249D3, 2, stockChildSpan, childSpan },
		{ 0x1402249D9, 2, stockChild0Begin, child0Begin },
		{ 0x140224A5B, 2, stockChildSpan, childSpan },
		{ 0x140224A69, 2, stockChild0Begin, child0Begin },
		{ 0x140224AC6, 2, stockObjectNameEnd, objectNameEnd },
		{ 0x140224C75, 2, stockChild0Begin - 1, child0Begin - 1 },
		{ 0x140224CF5, 2, stockChildSpan, childSpan },
		{ 0x140224CFB, 2, stockChild0Begin, child0Begin },
		{ 0x140224D01, 2, stockChild0Begin, child0Begin },
		{ 0x140224D12, 1, stockChildHashMagic, childHashMagic },
		{ 0x140224D28, 2, stockChildHash, childHash },
		{ 0x140224D7A, 2, stockChildSpan, childSpan },
		{ 0x140224D82, 1, stockChildHashMagic, childHashMagic },
		{ 0x140224D8D, 2, stockChild0Begin, child0Begin },
		{ 0x140224D96, 2, stockChildHash, childHash },
		{ 0x140224DAA, 2, stockChild0Begin, child0Begin },
		{ 0x140225078, 2, stockChildSpan, childSpan },
		{ 0x14022507E, 1, stockChildHashMagic, childHashMagic },
		{ 0x140225089, 2, stockChild0Begin, child0Begin },
		{ 0x140225092, 2, stockChildHash, childHash },
		{ 0x1402250A6, 2, stockChild0Begin, child0Begin },
		{ 0x1402250EC, 3, stockChild0Begin, child0Begin },
		{ 0x1402250F9, 2, stockChildSpan, childSpan },
		{ 0x140225158, 2, stockChildSpan, childSpan },
		{ 0x14022515E, 1, stockChildHashMagic, childHashMagic },
		{ 0x140225169, 2, stockChild0Begin, child0Begin },
		{ 0x140225172, 2, stockChildHash, childHash },
		{ 0x14022518A, 2, stockChild0Begin, child0Begin },
		{ 0x14022520B, 2, stockChildSpan, childSpan },
		{ 0x140225213, 2, stockChild0Begin, child0Begin },
		{ 0x14022522B, 3, stockChildSpan, childSpan },
		{ 0x140225271, 1, stockObjectNameEnd, objectNameEnd },
		{ 0x140225292, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402252B4, 2, stockChildHash, childHash },
		{ 0x1402252C9, 3, stockChild0Begin, child0Begin },
		{ 0x1402252DB, 3, stockChild0Begin, child0Begin },
		{ 0x140225388, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402253A0, 2, stockChildHash, childHash },
		{ 0x1402253BE, 2, stockChildSpan, childSpan },
		{ 0x1402253C4, 2, stockChild0Begin, child0Begin },
		{ 0x1402253CC, 3, stockChild0Begin, child0Begin },
		{ 0x140225421, 1, stockChildHashMagic, childHashMagic },
		{ 0x140225432, 3, stockChildHash, childHash },
		{ 0x140225446, 2, stockChild0Begin, child0Begin },
		{ 0x140225838, 3, stockChildSpan, childSpan },
		{ 0x14022583F, 3, stockChild0Begin, child0Begin },
		{ 0x140225848, 3, stockChild0Begin, child0Begin },
		{ 0x140225857, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022586C, 2, stockChildHash, childHash },
		{ 0x1402258B0, 1, stockObjectNameEnd, objectNameEnd },
		{ 0x14022593F, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022594D, 2, stockChildHash, childHash },
		{ 0x140225961, 2, stockChildSpan, childSpan },
		{ 0x140225967, 2, stockChild0Begin, child0Begin },
		{ 0x140225990, 2, stockChildSpan, childSpan },
		{ 0x14022599D, 2, stockChild0Begin, child0Begin },
		{ 0x1402259E2, 1, stockObjectNameEnd, objectNameEnd },
		{ 0x140225AAF, 3, stockChild0Begin, child0Begin },
		{ 0x140225AC3, 3, stockChild0Begin, child0Begin },
		{ 0x140225AEB, 5, stockChild0Begin * 24, child0Begin * 24 },
		{ 0x140225B1C, 4, stockChild0Begin * 24 + 2, child0Begin * 24 + 2 },
		{ 0x140225B68, 1, stockChild0Begin, child0Begin },
		{ 0x140226123, 3, stockChildSpan, childSpan },
		{ 0x14022612A, 1, stockChildHashMagic, childHashMagic },
		{ 0x140226138, 3, stockChild0Begin, child0Begin },
		{ 0x14022613F, 2, stockChildHash, childHash },
		{ 0x140226155, 2, stockChild0Begin, child0Begin },
		{ 0x140226173, 1, stockChild0Begin, child0Begin },
		{ 0x1402261B0, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402261D2, 2, stockChildHash, childHash },
		{ 0x1402261F5, 3, stockChild0Begin, child0Begin },
		{ 0x1402263FB, 2, stockChildSpan, childSpan },
		{ 0x140226401, 1, stockChildHashMagic, childHashMagic },
		{ 0x140226410, 2, stockChild0Begin, child0Begin },
		{ 0x140226419, 2, stockChildHash, childHash },
		{ 0x14022642D, 2, stockChild0Begin, child0Begin },
		{ 0x14022644E, 2, stockChildSpan, childSpan },
		{ 0x140226454, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022645F, 2, stockChild0Begin, child0Begin },
		{ 0x140226468, 2, stockChildHash, childHash },
		{ 0x14022647B, 2, stockChild0Begin, child0Begin },
		{ 0x1402264A7, 2, stockChildSpan, childSpan },
		{ 0x1402264AD, 2, stockChild0Begin, child0Begin },
		{ 0x1402265A5, 3, stockChildSpan, childSpan },
		{ 0x1402265AC, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402265B7, 2, stockChildHash, childHash },
		{ 0x1402265BD, 3, stockChild0Begin, child0Begin },
		{ 0x1402265D2, 2, stockChild0Begin, child0Begin },
		{ 0x1402265FD, 1, stockChild0Begin, child0Begin },
		{ 0x1402266F1, 1, stockChildHashMagic, childHashMagic },
		{ 0x140226706, 2, stockChildHash, childHash },
		{ 0x140226723, 2, stockChildSpan, childSpan },
		{ 0x140226729, 2, stockChild0Begin, child0Begin },
		{ 0x14022673D, 2, stockChild0Begin, child0Begin },
		{ 0x140226743, 2, stockChildSpan, childSpan },
		{ 0x1402272B3, 1, stockChild0Begin, child0Begin },
		{ 0x140227C3C, 2, stockChildSpan, childSpan },
		{ 0x140227C42, 2, stockChild0Begin, child0Begin },
		{ 0x140227CC7, 2, stockChildSpan, childSpan },
		{ 0x140227CCD, 1, stockChildHashMagic, childHashMagic },
		{ 0x140227CD8, 2, stockChildHash, childHash },
		{ 0x140227CDE, 2, stockChild0Begin, child0Begin },
		{ 0x140227CF2, 2, stockChild0Begin, child0Begin },
		{ 0x140227D19, 1, stockChild0Begin, child0Begin },
		{ 0x140227F3E, 2, stockChildSpan, childSpan },
		{ 0x140227F44, 2, stockChild0Begin, child0Begin },
		{ 0x140228046, 2, stockChildSpan, childSpan },
		{ 0x14022804E, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022805C, 2, stockChild0Begin, child0Begin },
		{ 0x140228065, 2, stockChildHash, childHash },
		{ 0x140228078, 2, stockChild0Begin, child0Begin },
		{ 0x140228095, 2, stockChild0Begin, child0Begin },
		{ 0x1402281CE, 2, stockChildSpan, childSpan },
		{ 0x1402281D4, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402281DF, 2, stockChild0Begin, child0Begin },
		{ 0x1402281E8, 2, stockChildHash, childHash },
		{ 0x1402281FB, 2, stockChild0Begin, child0Begin },
		{ 0x140228213, 1, stockChild0Begin, child0Begin },
		{ 0x1402282AE, 1, stockChild0Begin, child0Begin },
		{ 0x14022831C, 2, stockParentSize, parentSize },
		{ 0x140228358, 2, stockObjectNameEnd, objectNameEnd },
		{ 0x14022840D, 2, stockChildSpan, childSpan },
		{ 0x140228416, 1, stockChildHashMagic, childHashMagic },
		{ 0x140228421, 3, stockChild0Begin, child0Begin },
		{ 0x140228428, 2, stockChildHash, childHash },
		{ 0x14022844E, 2, stockChild0Begin, child0Begin },
		{ 0x140228552, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022855C, 2, stockChildSpan, childSpan },
		{ 0x14022856B, 2, stockChildHash, childHash },
		{ 0x140228577, 2, stockChild0Begin, child0Begin },
		{ 0x140228585, 2, stockChild0Begin, child0Begin },
		{ 0x14022859D, 1, stockChild0Begin, child0Begin },
		{ 0x140228627, 1, stockChildHashMagic, childHashMagic },
		{ 0x140228639, 2, stockChildHash, childHash },
		{ 0x14022864C, 2, stockChildSpan, childSpan },
		{ 0x140228652, 2, stockChild0Begin, child0Begin },
		{ 0x140228679, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022868B, 2, stockChildHash, childHash },
		{ 0x14022869E, 2, stockChildSpan, childSpan },
		{ 0x1402286A4, 2, stockChild0Begin, child0Begin },
		{ 0x14022890A, 2, stockChildSpan, childSpan },
		{ 0x140228910, 1, stockChildHashMagic, childHashMagic },
		{ 0x14022891B, 2, stockChild0Begin, child0Begin },
		{ 0x140228924, 2, stockChildHash, childHash },
		{ 0x140228938, 2, stockChild0Begin, child0Begin },
		{ 0x140228950, 1, stockChild0Begin, child0Begin },
		{ 0x140228975, 3, stockChildSpan, childSpan },
		{ 0x140228995, 3, stockChild0Begin, child0Begin },
		{ 0x1402289C1, 1, stockChildHashMagic, childHashMagic },
		{ 0x1402289D7, 2, stockChildHash, childHash },
		{ 0x1402289F0, 2, stockChild0Begin, child0Begin },
		{ 0x140228A02, 2, stockChild0Begin, child0Begin },
		{ 0x140228A4E, 2, stockChild0Begin, child0Begin },
		{ 0x140228A96, 1, stockChildHashMagic, childHashMagic },
		{ 0x140228AA4, 2, stockChildSpan, childSpan },
		{ 0x140228AB0, 2, stockChild0Begin, child0Begin },
		{ 0x140228AB9, 2, stockChildHash, childHash },
		{ 0x140228ACD, 2, stockChild0Begin, child0Begin },
		{ 0x140228B2A, 1, stockChild0Begin, child0Begin },
		{ 0x140228BAB, 2, stockChildSpan, childSpan },
		{ 0x140228BB1, 2, stockChild0Begin, child0Begin },
		{ 0x140228EE3, 2, stockChildSpan, childSpan },
		{ 0x140228EE9, 2, stockChild0Begin, child0Begin },
		{ 0x140228F9C, 2, stockChildSpan, childSpan },
		{ 0x140228FA2, 1, stockChildHashMagic, childHashMagic },
		{ 0x140228FAD, 2, stockChildHash, childHash },
		{ 0x140228FB3, 2, stockChild0Begin, child0Begin },
		{ 0x140228FC7, 2, stockChild0Begin, child0Begin },
		{ 0x140228FE9, 1, stockChild0Begin, child0Begin },
		{ 0x140229011, 1, stockChildHashMagic, childHashMagic },
		{ 0x140229021, 2, stockChildHash, childHash },
		{ 0x140229035, 1, stockChild0Begin, child0Begin },
		{ 0x14022903D, 2, stockChildSpan, childSpan },
		{ 0x140229045, 2, stockChild0Begin, child0Begin },
		{ 0x1402290A3, 2, stockChildSpan, childSpan },
		{ 0x1402290A9, 2, stockChild0Begin, child0Begin },
		{ 0x140229661, 2, stockChildSpan, childSpan },
		{ 0x140229667, 2, stockChild0Begin, child0Begin },
		{ 0x140229674, 2, stockChild0Begin, child0Begin },
		{ 0x140229E3D, 2, stockChildSpan, childSpan },
		{ 0x140229E4C, 2, stockChild0Begin, child0Begin },
		{ 0x14022A299, 2, stockChildSpan, childSpan },
		{ 0x14022A2A6, 2, stockChild0Begin, child0Begin },
		{ 0x14022A727, 2, stockChildSpan, childSpan },
		{ 0x14022A742, 2, stockChild0Begin, child0Begin },
		{ 0x14022A753, 2, stockChild0Begin, child0Begin },
		{ 0x14022A7C8, 1, stockChild0Begin, child0Begin },
		{ 0x14022A89B, 1, stockChild0Begin, child0Begin },
		{ 0x14022ABE5, 2, stockChildSpan, childSpan },
		{ 0x14022AC02, 2, stockChild0Begin, child0Begin },
		{ 0x14022AC13, 2, stockChild0Begin, child0Begin },
		{ 0x14022AC88, 1, stockChild0Begin, child0Begin },
		{ 0x14022ACE2, 2, stockChildSpan, childSpan },
		{ 0x14022ACE8, 2, stockChild0Begin, child0Begin },
		{ 0x14022ACFA, 2, stockChild0Begin, child0Begin },
		{ 0x14022AD52, 2, stockChildSpan, childSpan },
		{ 0x14022AD58, 2, stockChild0Begin, child0Begin },
		{ 0x14022AD6A, 2, stockChild0Begin, child0Begin },
		{ 0x14022ADBE, 1, stockChild0Begin, child0Begin },
		{ 0x14022ADED, 2, stockParentSize * 2, parentSize * 2 },
		{ 0x14022AF41, 2, stockChildSpan, childSpan },
		{ 0x14022AF47, 2, stockChild0Begin, child0Begin },
		{ 0x14022AF54, 2, stockChild0Begin, child0Begin },
		{ 0x14022B0DA, 1, stockChild0Begin, child0Begin },
		{ 0x14022B914, 1, stockChild0Begin, child0Begin },
		{ 0x14022BF21, 1, stockChild0Begin, child0Begin },
		{ 0x14022BF8B, 2, stockChild0Begin, child0Begin },
		{ 0x14022C26A, 3, stockChildSpan, childSpan },
		{ 0x14022C271, 3, stockChild0Begin, child0Begin },
		{ 0x14022C2A6, 3, stockChildSpan, childSpan },
		{ 0x14022C2B8, 3, stockChild0Begin, child0Begin },
		{ 0x14022C377, 3, stockChildSpan, childSpan },
		{ 0x14022C37E, 3, stockChild0Begin, child0Begin },
		{ 0x14022C554, 3, stockChildSpan, childSpan },
		{ 0x14022C55B, 3, stockChild0Begin, child0Begin },
		{ 0x14022C69E, 2, stockChildSpan, childSpan },
		{ 0x14022C6A6, 2, stockChild0Begin, child0Begin },
		{ 0x14022C74D, 3, stockChildSpan, childSpan },
		{ 0x14022C754, 3, stockChild0Begin, child0Begin },
		{ 0x14022C792, 3, stockChildSpan, childSpan },
		{ 0x14022C799, 3, stockChild0Begin, child0Begin },
		{ 0x14022C82A, 3, stockChildSpan, childSpan },
		{ 0x14022C831, 3, stockChild0Begin, child0Begin },
		{ 0x14022DFAD, 1, stockChild0Begin, child0Begin },
		{ 0x14022DFC5, 4, stockChild0Begin, child0Begin },
		{ 0x14022DFD8, 1, stockChild0Begin, child0Begin },
		{ 0x14022DFF7, 4, stockChild0Begin, child0Begin },
		{ 0x14022E00A, 1, stockChild0Begin, child0Begin },
		{ 0x14022E01A, 1, stockChild0Begin, child0Begin },
		{ 0x14022E03A, 1, stockChild0Begin, child0Begin },
		{ 0x14022E04A, 1, stockChild0Begin, child0Begin },
		{ 0x14022EAA4, 3, stockChildSpan, childSpan },
		{ 0x14022EAAB, 3, stockChild0Begin, child0Begin },
		{ 0x14022EFB1, 2, stockChildSpan, childSpan },
		{ 0x14022EFB7, 2, stockChild0Begin, child0Begin },
		{ 0x14022F3DC, 2, stockChildSpan, childSpan },
		{ 0x14022F3F8, 2, stockChild0Begin, child0Begin },
		{ 0x14022F412, 2, stockChild0Begin, child0Begin },
		{ 0x14022F43F, 2, stockChildSpan, childSpan },
		{ 0x14022F468, 2, stockChild0Begin, child0Begin },
		{ 0x14022F483, 2, stockChild0Begin, child0Begin },
	};

	static const BytePatch scriptVarShifts[] =
	{
		{ 0x1402227C7, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022286F, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140222CD7, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140222FCF, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402230A0, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022316A, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022349C, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223594, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223618, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223701, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022379D, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223829, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223CBF, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223D37, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223DB5, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223E5E, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223EFA, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140223FCD, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140224571, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140224605, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402247A7, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402248D3, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022493D, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140224D25, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140224D93, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022508F, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022516F, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402252B1, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022539D, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022542F, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140225869, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022594A, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140226135, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402261CF, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140226416, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140226465, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402265B4, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140226703, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140227CD5, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140228062, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402281E5, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022841E, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140228568, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140228636, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140228688, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140228921, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x1402289D4, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140228AB6, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x140228FAA, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
		{ 0x14022901E, { 0xC1, 0xEA, 0x0E }, { 0xC1, 0xEA, childHashShift }, 3 },
	};

	constexpr std::uintptr_t childPoolParities[] =
	{
		0x140182D78, 0x140211DC2, 0x1402121B8, 0x1402124A9, 0x14021AE17, 0x14021AEE5,
		0x14021AFD0, 0x14021D2D4, 0x14021D454, 0x14021F2E5, 0x14021F947, 0x1402200E8,
		0x140220A79, 0x140220AB1, 0x140220B62, 0x140220B91, 0x140220C7F, 0x140220CB4,
		0x140220CEF, 0x1402227A9, 0x140222809, 0x140222CC6, 0x140222F9A, 0x1402230B0,
		0x140223167, 0x1402232AD, 0x1402232BC, 0x140223428, 0x140223436, 0x14022356F,
		0x140223604, 0x1402236CB, 0x140223750, 0x14022377F, 0x14022380F, 0x140223941,
		0x140223BEA, 0x140223C97, 0x140223D47, 0x140223DCC, 0x140223E19, 0x140223E75,
		0x140223EB3, 0x140223F11, 0x140223F83, 0x14022405E, 0x1402245A7, 0x14022466A,
		0x140224787, 0x1402248B9, 0x14022494D, 0x1402249A0, 0x1402249D0, 0x140224A54,
		0x140224CF2, 0x140224D73, 0x140225075, 0x1402250F3, 0x140225155, 0x140225202,
		0x140225228, 0x1402253BB, 0x140225835, 0x14022595B, 0x140225976, 0x14022598D, 0x140226120,
		0x1402263F1, 0x14022644B, 0x1402264A4, 0x1402265A2, 0x1402266FA, 0x140226719,
		0x140227C36, 0x140227CC4, 0x140227F34, 0x14022803F, 0x1402281CB, 0x14022840A,
		0x140228559, 0x140228646, 0x140228698, 0x140228902, 0x140228972, 0x140228AA1,
		0x140228B9E, 0x140228EE0, 0x140228F96, 0x14022903A, 0x1402290A0, 0x14022965E,
		0x140229E37, 0x14022A293, 0x14022A71D, 0x14022ABDB, 0x14022ACDD, 0x14022AD4D,
		0x14022AF3E, 0x14022C263, 0x14022C29F, 0x14022C374, 0x14022C551, 0x14022C698,
		0x14022C746, 0x14022C78B, 0x14022C827, 0x14022EAA1, 0x14022EFAE, 0x14022F3D9,
		0x14022F43C,
	};

	constexpr std::uintptr_t parentListParities[] = { 0x140222F18, 0x140224E27, 0x140224F3A };

	static_assert(std::size(childPoolParities) == 110);

	struct ScriptStubSite
	{
		std::uintptr_t address;
		std::array<std::uint8_t, 8> stock;
		std::size_t length;
		void(*stub)();
	};

	static const ScriptStubSite scriptStubSites[] =
	{
		{ 0x140222A52, { 0x3D, 0x01, 0xC8, 0x00, 0x00, 0x75, 0x1E }, 7, ClientSlots_ScriptPoolExhaustedEax },
		{ 0x140224C7D, { 0x81, 0xF9, 0x01, 0xC8, 0x00, 0x00, 0x75, 0x24 }, 8, ClientSlots_ScriptPoolExhaustedEcx },
		{ 0x140222AFB, { 0x81, 0xFB, 0x02, 0x90, 0x00, 0x00 }, 6, ClientSlots_ScriptChildAddedEbx },
		{ 0x14022445F, { 0x81, 0xFB, 0x02, 0x90, 0x00, 0x00 }, 6, ClientSlots_ScriptChildAddedEbx },
		{ 0x1402239CC, { 0x81, 0xFB, 0x02, 0x90, 0x00, 0x00 }, 6, ClientSlots_ScriptChildFreedEbx },
		{ 0x140223B71, { 0x81, 0xFF, 0x02, 0x90, 0x00, 0x00 }, 6, ClientSlots_ScriptChildFreedEdi },
	};

	constexpr std::uintptr_t scriptChild0CountInc = 0x140225B08;
	constexpr std::uintptr_t stockScriptChildCounts = 0x141F29934;

	constexpr std::uintptr_t AllocVariable = 0x140222B40;
	constexpr std::uintptr_t Scr_InitVariables = 0x140228800;
	constexpr std::uintptr_t Sys_Error = 0x1402A4F90;
	static const std::uint8_t allocVariableEntry[] = { 0x48, 0x89, 0x5C, 0x24, 0x08 };
	static const std::uint8_t initVariablesEntry[] = { 0x48, 0x83, 0xEC, 0x28, 0x33 };

	static Utils::Hook scriptStubHooks[std::size(scriptStubSites)];
	static Utils::Hook allocVariableHook;
	static Utils::Hook initVariablesHook;

	static std::uint8_t* scriptVariables;
	static std::uint32_t scriptChildPools = stockChildPools;

	static std::uint16_t& VariableWord(std::uint32_t index, std::size_t offset)
	{
		return *reinterpret_cast<std::uint16_t*>(scriptVariables + index * variableSize + offset);
	}

	static std::uint32_t& VariableStatus(std::uint32_t index)
	{
		return *reinterpret_cast<std::uint32_t*>(scriptVariables + index * variableSize + 0x10);
	}

	static void InitVariableRange(std::uint32_t begin, std::uint32_t end, std::uint32_t first, std::uint32_t step)
	{
		const std::uint32_t head = begin + first;
		std::uint32_t index = head + step;

		for (; index < end; index += step)
		{
			VariableStatus(index) = 0;
			VariableWord(index, 0x0) = static_cast<std::uint16_t>(index - begin);
			VariableWord(index, 0x14) = static_cast<std::uint16_t>(index - begin);
			VariableWord(index, 0x8) = static_cast<std::uint16_t>(index + step - begin);
			VariableWord(index, 0x2) = static_cast<std::uint16_t>(index - step - begin);
		}

		const std::uint32_t last = index - step;
		VariableWord(head, 0x8) = static_cast<std::uint16_t>(first + step);
		VariableWord(head, 0x0) = static_cast<std::uint16_t>(first);
		VariableWord(head, 0x14) = static_cast<std::uint16_t>(first);
		VariableStatus(head) = 0;
		VariableWord(head + step, 0x2) = static_cast<std::uint16_t>(first);
		VariableWord(head, 0x2) = static_cast<std::uint16_t>(last - begin);
		VariableWord(last, 0x8) = static_cast<std::uint16_t>(first);
	}

	static void Scr_InitVariables_Hk()
	{
		std::fill_n(ClientSlots_scriptChildCounts, childPools, 0u);

		for (std::uint32_t list = 0; list < childPools; ++list)
		{
			InitVariableRange(1, child0Begin - 1, list, childPools);
		}

		for (std::uint32_t pool = 0; pool < childPools; ++pool)
		{
			const std::uint32_t first = child0Begin + pool * childSpan;
			InitVariableRange(first, first + childSpan - 1, 0, 1);
		}
	}

	static std::uint32_t AllocVariable_Hk()
	{
		std::uint32_t list = childPools;

		for (std::uint32_t candidate = 0; candidate < childPools; ++candidate)
		{
			const bool isEmpty = VariableWord(candidate + 1, 0x8) == candidate;

			if (isEmpty)
			{
				continue;
			}

			if (list == childPools || ClientSlots_scriptChildCounts[candidate] < ClientSlots_scriptChildCounts[list])
			{
				list = candidate;
			}
		}

		if (list == childPools)
		{
			reinterpret_cast<void(*)(const char*, ...)>(Utils::Hook::Rebase(Sys_Error))("exceeded maximum number of parent script variables");
			std::terminate();
		}

		const std::uint32_t id = VariableWord(list + 1, 0x8);
		const std::uint32_t taken = id + 1;
		const std::uint32_t hashed = VariableWord(taken, 0x0);
		const std::uint32_t next = VariableWord(hashed + 1, 0x8);
		std::uint32_t moved = hashed + 1;

		if (taken != moved && (VariableStatus(taken) & 0x60) == 0)
		{
			const std::uint32_t back = VariableWord(taken, 0x14);
			VariableWord(back + 1, 0x0) = static_cast<std::uint16_t>(hashed);
			VariableWord(taken, 0x0) = static_cast<std::uint16_t>(id);
			VariableWord(moved, 0x14) = static_cast<std::uint16_t>(back);
			VariableWord(moved, 0x8) = VariableWord(taken, 0x8);
			moved = taken;
		}

		VariableWord(list + 1, 0x8) = static_cast<std::uint16_t>(next);
		VariableWord(next + 1, 0x2) = static_cast<std::uint16_t>(list);
		VariableWord(moved, 0x14) = static_cast<std::uint16_t>(id);
		VariableWord(moved, 0x16) = 0;

		const std::uint32_t result = VariableWord(taken, 0x0);
		VariableWord(taken, 0x2) = 0;
		return result;
	}

	[[noreturn]] static void ChildPoolExhausted(std::uint32_t offset)
	{
		const std::uint32_t pool = (offset + childSpan) >> 16;
		reinterpret_cast<void(*)(const char*, ...)>(Utils::Hook::Rebase(Sys_Error))("exceeded maximum number of child%u script variables", pool);
		std::terminate();
	}

	static std::uintptr_t MovedScriptVarTarget(std::uintptr_t target, std::uintptr_t block)
	{
		if (target < stockIdMapRev)
		{
			return block + (target - stockIdMap);
		}

		if (target < stockVariableList)
		{
			return block + idMapSize + (target - stockIdMapRev);
		}

		const auto offset = target - stockVariableList;
		auto index = offset / variableSize;

		if (index >= stockChild1Begin)
		{
			index = index - stockChild1Begin + child1Begin;
		}
		else if (index >= stockChild0Begin)
		{
			index = index - stockChild0Begin + child0Begin;
		}

		return block + variableListOffset + index * variableSize + offset % variableSize;
	}

	static std::int64_t EncodeScriptVarReference(const ScriptVarReference& site, std::uintptr_t target, std::uintptr_t siteAddress, std::uintptr_t image)
	{
		if (site.base == SiteBase::Rip)
		{
			return static_cast<std::int64_t>(target) - static_cast<std::int64_t>(siteAddress + site.length);
		}

		return static_cast<std::int64_t>(target) - static_cast<std::int64_t>(image);
	}

	static std::uint32_t scriptChildIds = stockChildIds;

	static bool MoveScriptVariables()
	{
		for (const ScriptVarReference& site : scriptVarReferences)
		{
			const std::int64_t expected = EncodeScriptVarReference(site, site.target, site.address, imageBase);

			if (Utils::Hook::Get<std::int32_t>(site.address + site.operandOffset) != expected)
			{
				Logger::Error("clientslots: the script variable reference at 0x{:X} does not read as expected, the script VM keeps its limits\n", site.address);
				return false;
			}
		}

		for (const ImmediatePatch& patch : scriptVarImmediates)
		{
			if (Utils::Hook::Get<std::uint32_t>(patch.address + patch.operandOffset) != patch.stock)
			{
				Logger::Error("clientslots: the script variable bound at 0x{:X} does not read as expected, the script VM keeps its limits\n", patch.address);
				return false;
			}
		}

		for (const BytePatch& patch : scriptVarShifts)
		{
			if (!Utils::Hook::MatchesBytes(patch.address, patch.stock.data(), patch.length))
			{
				Logger::Error("clientslots: the child hash at 0x{:X} does not read as expected, the script VM keeps its limits\n", patch.address);
				return false;
			}
		}

		for (const std::uintptr_t site : childPoolParities)
		{
			const std::uint8_t register32 = Utils::Hook::Get<std::uint8_t>(site + 1);

			if (Utils::Hook::Get<std::uint8_t>(site) != 0x83 || (register32 & 0xF8) != 0xE0 || Utils::Hook::Get<std::uint8_t>(site + 2) != 0x01)
			{
				Logger::Error("clientslots: the child pool pick at 0x{:X} does not read as expected, the script VM keeps its limits\n", site);
				return false;
			}
		}

		static const std::uint8_t andR9d1[] = { 0x41, 0x83, 0xE1, 0x01 };

		for (const std::uintptr_t site : parentListParities)
		{
			if (!Utils::Hook::MatchesBytes(site, andR9d1, sizeof(andR9d1)))
			{
				Logger::Error("clientslots: the parent list pick at 0x{:X} does not read as expected, the script VM keeps its limits\n", site);
				return false;
			}
		}

		for (const ScriptStubSite& site : scriptStubSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, site.stock.data(), site.length))
			{
				Logger::Error("clientslots: the child count at 0x{:X} does not read as expected, the script VM keeps its limits\n", site.address);
				return false;
			}
		}

		const bool isChild0CountExpected = Utils::Hook::Get<std::uint16_t>(scriptChild0CountInc) == 0x05FF
			&& Utils::Hook::Get<std::int32_t>(scriptChild0CountInc + 2) == static_cast<std::int32_t>(stockScriptChildCounts - (scriptChild0CountInc + 6));

		if (!isChild0CountExpected
			|| !Utils::Hook::MatchesBytes(AllocVariable, allocVariableEntry, sizeof(allocVariableEntry))
			|| !Utils::Hook::MatchesBytes(Scr_InitVariables, initVariablesEntry, sizeof(initVariablesEntry)))
		{
			Logger::Error("clientslots: AllocVariable, Scr_InitVariables or the pool 0 count does not read as expected, the script VM keeps its limits\n");
			return false;
		}

		const std::size_t countsOffset = variableListOffset + variableCount * variableSize;
		const std::size_t blockSize = countsOffset + childPools * sizeof(std::uint32_t);
		auto* const block = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(stockVariableList, blockSize));

		if (!block)
		{
			Logger::Error("clientslots: no memory free within reach of the image for the script variables, the script VM keeps its limits\n");
			return false;
		}

		const std::uintptr_t liveImage = Utils::Hook::Rebase(imageBase);
		std::int32_t operands[std::size(scriptVarReferences)]{};

		for (std::size_t i = 0; i < std::size(scriptVarReferences); ++i)
		{
			const ScriptVarReference& site = scriptVarReferences[i];
			const std::uintptr_t target = MovedScriptVarTarget(site.target, reinterpret_cast<std::uintptr_t>(block));
			const std::int64_t encoded = EncodeScriptVarReference(site, target, Utils::Hook::Rebase(site.address), liveImage);

			if (encoded < INT32_MIN || encoded > INT32_MAX)
			{
				VirtualFree(block, 0, MEM_RELEASE);
				Logger::Error("clientslots: the script variables are out of reach of 0x{:X}, the script VM keeps its limits\n", site.address);
				return false;
			}

			operands[i] = static_cast<std::int32_t>(encoded);
		}

		auto* const counts = reinterpret_cast<std::uint32_t*>(block + countsOffset);
		const std::int64_t child0CountOperand = static_cast<std::int64_t>(reinterpret_cast<std::uintptr_t>(counts)) - static_cast<std::int64_t>(Utils::Hook::Rebase(scriptChild0CountInc) + 6);

		if (child0CountOperand < INT32_MIN || child0CountOperand > INT32_MAX)
		{
			VirtualFree(block, 0, MEM_RELEASE);
			Logger::Error("clientslots: the script child counts are out of reach of 0x{:X}, the script VM keeps its limits\n", scriptChild0CountInc);
			return false;
		}

		ClientSlots_allocVariableBody = reinterpret_cast<void*>(AllocVariable_Hk);
		ClientSlots_initVariablesBody = reinterpret_cast<void*>(Scr_InitVariables_Hk);
		ClientSlots_childPoolExhaustedBody = reinterpret_cast<void*>(ChildPoolExhausted);
		ClientSlots_scriptChild0Begin = child0Begin;

		bool isSeated = allocVariableHook.Initialize(AllocVariable, ClientSlots_AllocVariable, HOOK_JUMP)->Install()->IsInstalled();
		isSeated = initVariablesHook.Initialize(Scr_InitVariables, ClientSlots_ScrInitVariables, HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(scriptStubSites); ++i)
		{
			isSeated = scriptStubHooks[i].Initialize(scriptStubSites[i].address, scriptStubSites[i].stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			allocVariableHook.Uninstall();
			initVariablesHook.Uninstall();

			for (Utils::Hook& hook : scriptStubHooks)
			{
				hook.Uninstall();
			}

			VirtualFree(block, 0, MEM_RELEASE);
			Logger::Error("clientslots: could not hook the script VM's child pools, the script VM keeps its limits\n");
			return false;
		}

		for (std::size_t i = 0; i < std::size(scriptVarReferences); ++i)
		{
			Utils::Hook::Set<std::int32_t>(scriptVarReferences[i].address + scriptVarReferences[i].operandOffset, operands[i]);
		}

		for (const std::uintptr_t site : childPoolParities)
		{
			Utils::Hook::Set<std::uint8_t>(site + 2, static_cast<std::uint8_t>(childPools - 1));
		}

		for (const std::uintptr_t site : parentListParities)
		{
			Utils::Hook::Set<std::uint8_t>(site + 3, static_cast<std::uint8_t>(childPools - 1));
		}

		for (std::size_t i = 0; i < std::size(scriptStubSites); ++i)
		{
			for (std::size_t next = 5; next < scriptStubSites[i].length; ++next)
			{
				Utils::Hook::Set<std::uint8_t>(scriptStubSites[i].address + next, 0x90);
			}

			scriptStubHooks[i].Quick();
		}

		Utils::Hook::Set<std::int32_t>(scriptChild0CountInc + 2, static_cast<std::int32_t>(child0CountOperand));
		allocVariableHook.Quick();
		initVariablesHook.Quick();

		scriptVariables = block + variableListOffset;
		ClientSlots_scriptChildCounts = counts;
		scriptChildPools = childPools;

		for (const ImmediatePatch& patch : scriptVarImmediates)
		{
			Utils::Hook::Set<std::uint32_t>(patch.address + patch.operandOffset, patch.raised);
		}

		for (const BytePatch& patch : scriptVarShifts)
		{
			for (std::size_t i = 0; i < patch.length; ++i)
			{
				Utils::Hook::Set<std::uint8_t>(patch.address + i, patch.patched[i]);
			}
		}

		scriptChildIds = childIds;
		return true;
	}

	static Utils::Hook stubHooks[std::size(stubSites)];

	static std::uint8_t* movedSlotArrays[std::size(slotArrays)]{};

	constexpr std::uintptr_t CG_Init_ClearCall = 0x1400D64DA;
	constexpr std::uintptr_t CG_Shutdown_ClearCall = 0x1400DA2DC;
	constexpr std::uintptr_t SV_Shutdown_ClearSvsCall = 0x14023B1CC;
	constexpr std::uintptr_t UI_BuildPlayerList_MaxClientsCall = 0x14026BE47;
	constexpr std::uintptr_t atoiAddress = 0x1403373EC;

	static Utils::Hook cgInitClearHook;
	static Utils::Hook cgShutdownClearHook;
	static Utils::Hook svShutdownClearHook;
	static Utils::Hook playerListHook;

	static std::uintptr_t cgInitClearNext;
	static std::uintptr_t cgShutdownClearNext;

	static std::uintptr_t CallTarget(std::uintptr_t call)
	{
		return Utils::Hook::Rebase(call) + 5 + Utils::Hook::Get<std::int32_t>(call + 1);
	}

	static void ClearMovedCg()
	{
		for (const SlotArray array : { SlotArray::CgameBgs, SlotArray::CgameClientMask, SlotArray::ActiveSnapshot0, SlotArray::ActiveSnapshot1, SlotArray::Scores })
		{
			const auto index = static_cast<std::size_t>(array);
			std::memset(movedSlotArrays[index], 0, slotArrays[index].newSize);
		}
	}

	static void* CG_Init_ClearCg_Hk(void* cg, int value, std::size_t size)
	{
		void* const result = reinterpret_cast<void*(*)(void*, int, std::size_t)>(cgInitClearNext)(cg, value, size);
		ClearMovedCg();
		return result;
	}

	static void* CG_Shutdown_ClearCg_Hk(void* cg, int value, std::size_t size)
	{
		void* const result = reinterpret_cast<void*(*)(void*, int, std::size_t)>(cgShutdownClearNext)(cg, value, size);
		ClearMovedCg();
		return result;
	}

	static void* SV_Shutdown_ClearSvs_Hk(void* serverStatic, int value, std::size_t size)
	{
		void* const result = EngineMemset(serverStatic, value, size);

		const auto index = static_cast<std::size_t>(SlotArray::SvsClients);
		std::memset(movedSlotArrays[index], 0, slotArrays[index].newSize);

		return result;
	}

	static int UI_BuildPlayerList_MaxClients_Hk(const char* text)
	{
		const int maxClients = reinterpret_cast<int(*)(const char*)>(Utils::Hook::Rebase(atoiAddress))(text);
		return std::min(maxClients, static_cast<int>(ClientSlots::BASEGAME_CLIENT_LIMIT));
	}

	constexpr std::uintptr_t SV_GameSendServerCommand = 0x1402333E0;
	constexpr std::uintptr_t scoreboardSendCalls[] = { 0x140199B26, 0x14019936F };
	constexpr std::uintptr_t SV_GetClientSkill = 0x1402387A0;
	constexpr std::uintptr_t level_numConnectedClients = 0x14186741C;
	constexpr std::uintptr_t level_teamScores = 0x1418673FC;
	constexpr std::uintptr_t scr_scorelimit_dvar = 0x141869B18;
	constexpr std::uintptr_t scr_roundlimit_dvar = 0x141869B20;
	constexpr char scoreboardMore = 'Y';
	constexpr std::size_t scoreboardCommandSize = 1000;
	constexpr std::size_t scoreboardHeaderRoom = 64;

	static Utils::Hook scoreboardSendHooks[std::size(scoreboardSendCalls)];

	static void SendScoreboard_Hk(int clientNum, int type, const char* text)
	{
		const auto send = reinterpret_cast<void(*)(int, int, const char*)>(Utils::Hook::Rebase(SV_GameSendServerCommand));

		if (!ClientSlots::IsServerWide())
		{
			send(clientNum, type, text);
			return;
		}

		const auto* const clients = *reinterpret_cast<const std::uint8_t* const*>(Utils::Hook::Rebase(g_clients_ptr));
		const auto* const sortedClients = reinterpret_cast<const std::int32_t*>(movedSlotArrays[static_cast<std::size_t>(SlotArray::SortedClients)]);
		const auto* const teamScores = reinterpret_cast<const std::int32_t*>(Utils::Hook::Rebase(level_teamScores));
		const int connected = std::min(*reinterpret_cast<const std::int32_t*>(Utils::Hook::Rebase(level_numConnectedClients)), static_cast<int>(ClientSlots::CLIENT_LIMIT));
		const auto getSkill = reinterpret_cast<int(*)(int)>(Utils::Hook::Rebase(SV_GetClientSkill));

		int limit = (*reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(scr_scorelimit_dvar)))->current.integer;

		if (!limit)
		{
			limit = (*reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(scr_roundlimit_dvar)))->current.integer;
		}

		std::vector<std::string> rows;
		rows.reserve(static_cast<std::size_t>(std::max(connected, 0)));

		for (int i = 0; i < connected; ++i)
		{
			const int client = sortedClients[i];
			const auto* const gclient = clients + static_cast<std::size_t>(client) * gclientSize;
			const auto field = [gclient](std::size_t offset)
			{
				return *reinterpret_cast<const std::int32_t*>(gclient + offset);
			};

			int ping = -1;

			if (field(0x3148) != 1)
			{
				ping = Bots::ScoreboardPing(client);
			}

			rows.push_back(std::format(" {} {} {} {} {} {} {} {}", client, field(0x3134), ping, field(0x3138), field(0x312C), field(0x313C), field(0x3140), getSkill(client)));
		}

		std::size_t next = 0;
		char letter = 'b';

		do
		{
			std::string body;
			std::size_t count = 0;

			while (next + count < rows.size() && scoreboardHeaderRoom + body.size() + rows[next + count].size() <= scoreboardCommandSize)
			{
				body += rows[next + count];
				++count;
			}

			send(clientNum, type, std::format("{:c} {} {} {} {}{}", letter, count, teamScores[0], teamScores[1], limit, body).c_str());
			next += count;
			letter = scoreboardMore;
		}
		while (next < rows.size());
	}

	constexpr std::uintptr_t scoresParse = 0x1400E8300;
	constexpr std::uintptr_t numScores = 0x1404EBE20;
	constexpr std::uintptr_t teamPing = 0x1404EBE34;
	constexpr std::uintptr_t teamPlayers = 0x1404EBE44;
	constexpr std::size_t scoreSize = 0x38;
	constexpr std::size_t teamCount = 4;

	static bool ParseMoreScores()
	{
		auto* const count = reinterpret_cast<std::int32_t*>(Utils::Hook::Rebase(numScores));
		auto* const scores = movedSlotArrays[static_cast<std::size_t>(SlotArray::Scores)];
		const int kept = std::clamp(*count, 0, static_cast<int>(ClientSlots::CLIENT_LIMIT));

		std::array<std::uint8_t, ClientSlots::CLIENT_LIMIT * scoreSize> keptRows{};
		std::memcpy(keptRows.data(), scores, static_cast<std::size_t>(kept) * scoreSize);

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(scoresParse))();

		const int added = std::clamp(*count, 0, static_cast<int>(ClientSlots::CLIENT_LIMIT) - kept);
		std::memmove(scores + static_cast<std::size_t>(kept) * scoreSize, scores, static_cast<std::size_t>(added) * scoreSize);
		std::memcpy(scores, keptRows.data(), static_cast<std::size_t>(kept) * scoreSize);
		*count = kept + added;

		auto* const ping = reinterpret_cast<std::int32_t*>(Utils::Hook::Rebase(teamPing));
		auto* const players = reinterpret_cast<std::int32_t*>(Utils::Hook::Rebase(teamPlayers));
		std::fill_n(ping, teamCount, 0);
		std::fill_n(players, teamCount, 0);

		for (int i = 0; i < *count; ++i)
		{
			const auto* const row = scores + static_cast<std::size_t>(i) * scoreSize;
			const auto team = *reinterpret_cast<const std::int32_t*>(row + 0x10);

			if (team < 0 || static_cast<std::size_t>(team) >= teamCount)
			{
				continue;
			}

			++players[team];
			ping[team] += *reinterpret_cast<const std::int32_t*>(row + 0x8);
		}

		for (std::size_t team = 0; team < teamCount; ++team)
		{
			if (players[team] < 1 || ping[team] < 1)
			{
				ping[team] = 0;
			}
			else
			{
				ping[team] /= players[team];
			}
		}

		return true;
	}

	static SlotSite LoopEndSite(const SlotLoopEnd& end, std::size_t count)
	{
		const SlotArrayLayout& layout = slotArrays[static_cast<std::size_t>(end.array)];
		const auto offset = static_cast<std::int32_t>(layout.headerSize + layout.entrySize * count) + end.fieldOffset;

		return { end.address, end.length, end.operandOffset, end.array, end.base, offset };
	}

	struct SiteBases
	{
		std::uintptr_t image;
		std::uintptr_t svs;
		std::uintptr_t clients;
		std::uintptr_t cg;
	};

	static std::int64_t EncodeSlotSite(const SlotSite& site, std::uintptr_t array, std::uintptr_t siteAddress, const SiteBases& bases)
	{
		const auto target = static_cast<std::int64_t>(array) + site.offset;

		switch (site.base)
		{
		case SiteBase::Rip:
			return target - static_cast<std::int64_t>(siteAddress + site.length);
		case SiteBase::Svs:
			return target - static_cast<std::int64_t>(bases.svs);
		case SiteBase::Image:
			return target - static_cast<std::int64_t>(bases.image);
		case SiteBase::Clients:
			return target - static_cast<std::int64_t>(bases.clients);
		case SiteBase::Cg:
			return target - static_cast<std::int64_t>(bases.cg);
		}

		return 0;
	}

	static void FreeBlocks(std::uint8_t* const* blocks, std::size_t count)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			if (blocks[i])
			{
				VirtualFree(blocks[i], 0, MEM_RELEASE);
			}
		}
	}

	bool ClientSlots::MoveSlotArrays()
	{
		ClientSlots_serverClientCount = Game::svs_clientCount;

		const SiteBases stockBases{ imageBase, svs, clientsStruct, cgArray };
		const SiteBases liveBases{ Utils::Hook::Rebase(imageBase), Utils::Hook::Rebase(svs), Utils::Hook::Rebase(clientsStruct), Utils::Hook::Rebase(cgArray) };

		SlotSite sites[std::size(slotSites) + std::size(slotLoopEnds)]{};
		std::copy(std::begin(slotSites), std::end(slotSites), sites);

		for (std::size_t i = 0; i < std::size(slotLoopEnds); ++i)
		{
			sites[std::size(slotSites) + i] = LoopEndSite(slotLoopEnds[i], BASEGAME_CLIENT_LIMIT);
		}

		for (const SlotSite& site : sites)
		{
			const SlotArrayLayout& layout = slotArrays[static_cast<std::size_t>(site.array)];
			const std::int64_t expected = EncodeSlotSite(site, layout.address, site.address, stockBases);
			const auto current = Utils::Hook::Get<std::int32_t>(site.address + site.operandOffset);

			if (current != expected)
			{
				Logger::Error("clientslots: 0x{:X} does not read as expected, the slot arrays were not moved\n", site.address);
				return false;
			}
		}

		for (const ImmediatePatch& patch : immediatePatches)
		{
			if (Utils::Hook::Get<std::uint32_t>(patch.address + patch.operandOffset) != patch.stock)
			{
				Logger::Error("clientslots: the count at 0x{:X} does not read as expected, the slot arrays were not moved\n", patch.address);
				return false;
			}
		}

		for (const BytePatch& patch : bytePatches)
		{
			if (!Utils::Hook::MatchesBytes(patch.address, patch.stock.data(), patch.length))
			{
				Logger::Error("clientslots: 0x{:X} does not read as expected, the slot arrays were not moved\n", patch.address);
				return false;
			}
		}

		for (const BytePatch& patch : cgameLayoutBytes)
		{
			if (!Utils::Hook::MatchesBytes(patch.address, patch.stock.data(), patch.length))
			{
				Logger::Error("clientslots: 0x{:X} does not read as expected, the slot arrays were not moved\n", patch.address);
				return false;
			}
		}

		for (const ImmediatePatch& patch : layoutImmediates)
		{
			if (Utils::Hook::Get<std::uint32_t>(patch.address + patch.operandOffset) != patch.stock)
			{
				Logger::Error("clientslots: the count at 0x{:X} does not read as expected, the slot arrays were not moved\n", patch.address);
				return false;
			}
		}

		for (const BytePatch& patch : layoutBytes)
		{
			if (!Utils::Hook::MatchesBytes(patch.address, patch.stock.data(), patch.length))
			{
				Logger::Error("clientslots: 0x{:X} does not read as expected, the slot arrays were not moved\n", patch.address);
				return false;
			}
		}

		for (const StubSite& site : stubSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, site.stock.data(), site.length))
			{
				Logger::Error("clientslots: 0x{:X} does not read as expected, the slot arrays were not moved\n", site.address);
				return false;
			}
		}

		const bool areCallsExpected = Utils::Hook::BranchesTo(G_FreeEntity_Memset, memsetAddress, HOOK_CALL)
			&& Utils::Hook::BranchesTo(G_InitGame_Memset, memsetAddress, HOOK_CALL)
			&& Utils::Hook::Get<std::uint8_t>(CG_Init_ClearCall) == 0xE8
			&& Utils::Hook::Get<std::uint8_t>(CG_Shutdown_ClearCall) == 0xE8
			&& Utils::Hook::BranchesTo(SV_Shutdown_ClearSvsCall, memsetAddress, HOOK_CALL)
			&& Utils::Hook::BranchesTo(UI_BuildPlayerList_MaxClientsCall, atoiAddress, HOOK_CALL);
		static constexpr std::uint8_t itemMaskStart[] = { 0x33, 0xC0, 0x89, 0x81, 0xFC, 0x00, 0x00, 0x00 };

		const bool areRaiseSitesExpected = Utils::Hook::BranchesTo(G_FreeEntity_RefsCall, G_FreeEntityRefs, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_BuildClientSnapshot_FrameCopyCall, memcpyAddress, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_BuildClientSnapshot_BaselineCopyCall, memcpyAddress, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_BuildClientSnapshot_CachedFrameCall, cachedFrameCopy, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_MigrationStartJumps[0], SV_MigrationStart, HOOK_JUMP)
			&& Utils::Hook::BranchesTo(SV_MigrationStartJumps[1], SV_MigrationStart, HOOK_JUMP)
			&& Utils::Hook::BranchesTo(SV_PacketEvent_MigrationCall, SV_MigrationPacket, HOOK_CALL)
			&& Utils::Hook::BranchesTo(PartyHost_StartMatch_SetMaxClients, Dvar_SetInt, HOOK_CALL)
			&& Utils::Hook::BranchesTo(PartyClient_SessionSync_SetMaxClients, Dvar_SetInt, HOOK_CALL)
			&& std::ranges::all_of(scoreboardSendCalls, [](const std::uintptr_t call)
			{
				return Utils::Hook::BranchesTo(call, SV_GameSendServerCommand, HOOK_CALL);
			})
			&& std::ranges::all_of(serverInfoCalls, [](const std::uintptr_t call)
			{
				return Utils::Hook::BranchesTo(call, Dvar_InfoString, HOOK_CALL);
			})
			&& std::ranges::all_of(uiMaxClientsRegisterCalls, [](const std::uintptr_t call)
			{
				return Utils::Hook::BranchesTo(call, Dvar_RegisterInt, HOOK_CALL);
			})
			&& Utils::Hook::BranchesTo(CL_ParseGamestate_NeedRestartCall, FS_NeedRestart, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_SpawnServer_PrivateClientsCall, Dvar_SetInt, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_AddTestClient_DirectConnectCall, SV_DirectConnect, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_ChangeMaxClients_AllocCall, Hunk_AllocateTempMemory, HOOK_CALL)
			&& Utils::Hook::BranchesTo(SV_ChangeMaxClients_FreeCall, Hunk_FreeTempMemory, HOOK_CALL)
			&& std::ranges::all_of(memberGuardSites, [](const MemberGuardSite& site)
			{
				return Utils::Hook::BranchesTo(site.address, site.target, site.isJump);
			})
			&& AreNetFieldsExpected();

		if (!areCallsExpected || !areRaiseSitesExpected || !Utils::Hook::MatchesBytes(BuildItemClientMask, itemMaskStart, sizeof(itemMaskStart)))
		{
			Logger::Error("clientslots: the entity wipes or 0x{:X} do not read as expected, the slot arrays were not moved\n", BuildItemClientMask);
			return false;
		}

		std::uint8_t* blocks[std::size(slotArrays)]{};

		for (std::size_t i = 0; i < std::size(slotArrays); ++i)
		{
			blocks[i] = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(slotArrays[i].address, slotArrays[i].newSize));

			if (!blocks[i])
			{
				FreeBlocks(blocks, std::size(blocks));
				Logger::Error("clientslots: no memory free within reach of the image for 0x{:X}, the slot arrays were not moved\n", slotArrays[i].address);
				return false;
			}
		}

		for (std::size_t i = 0; i < std::size(slotLoopEnds); ++i)
		{
			sites[std::size(slotSites) + i] = LoopEndSite(slotLoopEnds[i], CLIENT_LIMIT);
		}

		std::int32_t operands[std::size(sites)]{};

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			const SlotSite& site = sites[i];
			const auto array = reinterpret_cast<std::uintptr_t>(blocks[static_cast<std::size_t>(site.array)]);
			const std::int64_t encoded = EncodeSlotSite(site, array, Utils::Hook::Rebase(site.address), liveBases);

			if (encoded < INT32_MIN || encoded > INT32_MAX)
			{
				FreeBlocks(blocks, std::size(blocks));
				Logger::Error("clientslots: the moved arrays are out of reach of 0x{:X}, not moved\n", site.address);
				return false;
			}

			operands[i] = static_cast<std::int32_t>(encoded);
		}

		bool isSeated = true;
		Utils::Hook* const singleHooks[] =
		{
			&freeEntityHook, &initGameHook, &itemMaskHook, &freeEntityRefsHook, &migrationPacketHook, &startMatchHook,
			&botConnectHook, &changeMaxClientsAllocHook, &changeMaxClientsFreeHook, &antilagRewindHook, &antilagRestoreHook, &weaponMeleeHook,
			&gamestateHook, &spawnServerHook, &cgInitClearHook, &cgShutdownClearHook, &svShutdownClearHook, &playerListHook, &partySyncHook,
			&serverInfoHooks[0], &serverInfoHooks[1], &serverInfoHooks[2], &scoreboardSendHooks[0], &scoreboardSendHooks[1],
			&frameCopyHook, &baselineCopyHook, &cachedFrameHook,
		};

		for (std::size_t i = 0; i < std::size(stubSites); ++i)
		{
			isSeated = stubHooks[i].Initialize(stubSites[i].address, stubSites[i].stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = freeEntityHook.Initialize(G_FreeEntity_Memset, reinterpret_cast<void*>(G_FreeEntity_Memset_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = initGameHook.Initialize(G_InitGame_Memset, reinterpret_cast<void*>(G_InitGame_Memset_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		ClientSlots_buildItemClientMaskBody = reinterpret_cast<void*>(BuildItemClientMask_Hk);
		isSeated = itemMaskHook.Initialize(BuildItemClientMask, reinterpret_cast<void*>(ClientSlots_BuildItemClientMask), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = freeEntityRefsHook.Initialize(G_FreeEntity_RefsCall, reinterpret_cast<void*>(G_FreeEntityRefs_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = frameCopyHook.Initialize(SV_BuildClientSnapshot_FrameCopyCall, reinterpret_cast<void*>(SV_BuildClientSnapshot_FrameCopy_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = baselineCopyHook.Initialize(SV_BuildClientSnapshot_BaselineCopyCall, reinterpret_cast<void*>(SV_BuildClientSnapshot_BaselineCopy_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = cachedFrameHook.Initialize(SV_BuildClientSnapshot_CachedFrameCall, reinterpret_cast<void*>(SV_BuildClientSnapshot_CachedFrame_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = migrationPacketHook.Initialize(SV_PacketEvent_MigrationCall, reinterpret_cast<void*>(SV_MigrationPacket_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = startMatchHook.Initialize(PartyHost_StartMatch_SetMaxClients, reinterpret_cast<void*>(PartyHost_StartMatch_SetMaxClients_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		ClientSlots_sessionGetXuid = reinterpret_cast<void*>(Utils::Hook::Rebase(Session_GetXuidEvenIfInactive));
		ClientSlots_sessionIsRegistered = reinterpret_cast<void*>(Utils::Hook::Rebase(Session_IsUserRegistered));
		ClientSlots_partyRemovePlayer = reinterpret_cast<void*>(Utils::Hook::Rebase(PartyHost_RemovePlayer));
		ClientSlots_partyVoiceBits = reinterpret_cast<void*>(Utils::Hook::Rebase(PartyHost_UpdateVoiceConnectivityBits));
		ClientSlots_setConfigstring = reinterpret_cast<void*>(Utils::Hook::Rebase(SV_SetConfigstring));
		ClientSlots_partyMemberAddr = reinterpret_cast<void*>(Utils::Hook::Rebase(Party_GetClientXNAddr));
		ClientSlots_sessionRegister = reinterpret_cast<void*>(Utils::Hook::Rebase(Session_RegisterRemotePlayer));
		ClientSlots_clientInMyParty = reinterpret_cast<void*>(Utils::Hook::Rebase(CL_ClientIsInMyParty));
		ClientSlots_playerMuted = reinterpret_cast<void*>(Utils::Hook::Rebase(CL_IsPlayerMuted));
		ClientSlots_playerTalking = reinterpret_cast<void*>(Utils::Hook::Rebase(CL_IsPlayerTalking));

		for (std::size_t i = 0; i < std::size(memberGuardSites); ++i)
		{
			isSeated = memberGuardHooks[i].Initialize(memberGuardSites[i].address, memberGuardSites[i].guard, memberGuardSites[i].isJump)->Install()->IsInstalled() && isSeated;
		}

		isSeated = botConnectHook.Initialize(SV_AddTestClient_DirectConnectCall, reinterpret_cast<void*>(SV_AddTestClient_DirectConnect_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = changeMaxClientsAllocHook.Initialize(SV_ChangeMaxClients_AllocCall, reinterpret_cast<void*>(SV_ChangeMaxClients_Alloc_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = changeMaxClientsFreeHook.Initialize(SV_ChangeMaxClients_FreeCall, reinterpret_cast<void*>(SV_ChangeMaxClients_Free_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = antilagRewindHook.Initialize(G_AntiLagRewindClientPos, reinterpret_cast<void*>(G_AntiLagRewindClientPos_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = antilagRestoreHook.Initialize(G_AntiLag_RestoreClientPos, reinterpret_cast<void*>(G_AntiLag_RestoreClientPos_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		isSeated = weaponMeleeHook.Initialize(Weapon_Melee, reinterpret_cast<void*>(Weapon_Melee_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(SV_MigrationStartJumps); ++i)
		{
			isSeated = migrationStartHooks[i].Initialize(SV_MigrationStartJumps[i], reinterpret_cast<void*>(SV_MigrationStart_Hk), HOOK_JUMP)->Install()->IsInstalled() && isSeated;
		}

		for (std::size_t i = 0; i < std::size(uiMaxClientsRegisterCalls); ++i)
		{
			isSeated = maxClientsRegisterHooks[i].Initialize(uiMaxClientsRegisterCalls[i], reinterpret_cast<void*>(Dvar_RegisterInt_MaxClients_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		isSeated = gamestateHook.Initialize(CL_ParseGamestate_NeedRestartCall, reinterpret_cast<void*>(CL_ParseGamestate_NeedRestart_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = spawnServerHook.Initialize(SV_SpawnServer_PrivateClientsCall, reinterpret_cast<void*>(SV_SpawnServer_PrivateClients_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		cgInitClearNext = CallTarget(CG_Init_ClearCall);
		cgShutdownClearNext = CallTarget(CG_Shutdown_ClearCall);
		isSeated = cgInitClearHook.Initialize(CG_Init_ClearCall, reinterpret_cast<void*>(CG_Init_ClearCg_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = cgShutdownClearHook.Initialize(CG_Shutdown_ClearCall, reinterpret_cast<void*>(CG_Shutdown_ClearCg_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = svShutdownClearHook.Initialize(SV_Shutdown_ClearSvsCall, reinterpret_cast<void*>(SV_Shutdown_ClearSvs_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = playerListHook.Initialize(UI_BuildPlayerList_MaxClientsCall, reinterpret_cast<void*>(UI_BuildPlayerList_MaxClients_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		isSeated = partySyncHook.Initialize(PartyClient_SessionSync_SetMaxClients, reinterpret_cast<void*>(PartyClient_SessionSync_SetMaxClients_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;

		for (std::size_t i = 0; i < std::size(serverInfoCalls); ++i)
		{
			isSeated = serverInfoHooks[i].Initialize(serverInfoCalls[i], reinterpret_cast<void*>(SV_ServerInfo_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		for (std::size_t i = 0; i < std::size(scoreboardSendCalls); ++i)
		{
			isSeated = scoreboardSendHooks[i].Initialize(scoreboardSendCalls[i], reinterpret_cast<void*>(SendScoreboard_Hk), HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (Utils::Hook& hook : stubHooks)
			{
				hook.Uninstall();
			}

			for (Utils::Hook& hook : migrationStartHooks)
			{
				hook.Uninstall();
			}

			for (Utils::Hook& hook : maxClientsRegisterHooks)
			{
				hook.Uninstall();
			}

			for (Utils::Hook& hook : memberGuardHooks)
			{
				hook.Uninstall();
			}

			for (Utils::Hook* hook : singleHooks)
			{
				hook->Uninstall();
			}

			FreeBlocks(blocks, std::size(blocks));
			Logger::Error("clientslots: could not hook SV_SendClientMessages' due flags, the slot arrays were not moved\n");
			return false;
		}

		for (std::size_t i = 0; i < std::size(slotArrays); ++i)
		{
			movedSlotArrays[i] = blocks[i];
			std::memcpy(movedSlotArrays[i], reinterpret_cast<const void*>(Utils::Hook::Rebase(slotArrays[i].address)), slotArrays[i].stockSize);
		}

		for (std::size_t i = 0; i < std::size(sites); ++i)
		{
			Utils::Hook::Set<std::int32_t>(sites[i].address + sites[i].operandOffset, operands[i]);
		}

		for (const ImmediatePatch& patch : immediatePatches)
		{
			Utils::Hook::Set<std::uint32_t>(patch.address + patch.operandOffset, patch.raised);
		}

		for (const BytePatch& patch : bytePatches)
		{
			WriteBytePatch(patch, true);
		}

		for (std::size_t i = 0; i < std::size(stubSites); ++i)
		{
			const StubSite& site = stubSites[i];
			std::size_t next = 5;

			if (site.restoresRcxFromRax)
			{
				static constexpr std::uint8_t movRcxRax[] = { 0x48, 0x8B, 0xC8 };

				for (const std::uint8_t byte : movRcxRax)
				{
					Utils::Hook::Set<std::uint8_t>(site.address + next++, byte);
				}
			}

			for (; next < site.length; ++next)
			{
				Utils::Hook::Set<std::uint8_t>(site.address + next, 0x90);
			}

			stubHooks[i].Quick();
		}

		for (Utils::Hook& hook : migrationStartHooks)
		{
			hook.Quick();
		}

		for (Utils::Hook& hook : maxClientsRegisterHooks)
		{
			hook.Quick();
		}

		for (Utils::Hook& hook : memberGuardHooks)
		{
			hook.Quick();
		}

		for (Utils::Hook* hook : singleHooks)
		{
			hook->Quick();
		}

		Game::svs_clients = reinterpret_cast<Game::client_s*>(movedSlotArrays[static_cast<std::size_t>(SlotArray::SvsClients)]);
		ClientSlots_snapshotRingBegin = movedSlotArrays[static_cast<std::size_t>(SlotArray::SnapshotEntities)];
		ClientSlots_snapshotRingEnd = ClientSlots_snapshotRingBegin + static_cast<std::size_t>(snapshotEntityCount) * entityStateSize;
		ClientSlots_baselineBegin = reinterpret_cast<std::uint8_t*>(Utils::Hook::Rebase(baselineEntities));
		ClientSlots_baselineEnd = ClientSlots_baselineBegin + baselineEntityCount * entityStateSize;
		ClientSlots_cgameClientInfo = movedSlotArrays[static_cast<std::size_t>(SlotArray::CgameBgs)] + 0x931C8;
		ClientSlots_cgameClientCount = CLIENT_LIMIT;

		return true;
	}

	std::size_t ClientSlots::SentClientCount()
	{
		return std::max(BASEGAME_CLIENT_LIMIT, static_cast<std::size_t>(*Game::svs_clientCount));
	}

	bool ClientSlots::IsServerWide()
	{
		return static_cast<std::size_t>(*Game::svs_clientCount) > BASEGAME_CLIENT_LIMIT;
	}

	Game::clientInfo_t* ClientSlots::CgameClientInfo(std::size_t index)
	{
		return &reinterpret_cast<Game::clientInfo_t*>(ClientSlots_cgameClientInfo)[index];
	}

	std::size_t ClientSlots::CgameClientCount()
	{
		return ClientSlots_cgameClientCount;
	}

	std::size_t ClientSlots::ScriptChildCapacity()
	{
		return scriptChildIds;
	}

	std::size_t ClientSlots::ScriptChildPools()
	{
		return scriptChildPools;
	}

	std::size_t ClientSlots::ScriptChildHighest()
	{
		std::uint32_t highest = 0;

		for (std::uint32_t pool = 0; pool < scriptChildPools; ++pool)
		{
			highest = std::max(highest, ClientSlots_scriptChildCounts[pool]);
		}

		return highest;
	}

	std::size_t ClientSlots::ScriptParentCapacity()
	{
		if (!scriptVariables)
		{
			return 0;
		}

		return parentSize - childPools;
	}

	std::size_t ClientSlots::ScriptParentsUsed()
	{
		if (!scriptVariables)
		{
			return 0;
		}

		std::size_t freeCount = 0;

		for (std::uint32_t list = 0; list < childPools; ++list)
		{
			std::uint32_t id = VariableWord(list + 1, 0x8);

			while (id != list && freeCount < parentSize)
			{
				++freeCount;
				id = VariableWord(VariableWord(id + 1, 0x0) + 1u, 0x8);
			}
		}

		return ScriptParentCapacity() - std::min(freeCount, ScriptParentCapacity());
	}

	constexpr std::uintptr_t scriptMemoryFreeBits = 0x141EEF600;
	constexpr std::size_t scriptMemoryFreeWords = 0x800;

	std::size_t ClientSlots::ScriptMemoryFreeBlocks()
	{
		const auto* const words = reinterpret_cast<const std::uint32_t*>(Utils::Hook::Rebase(scriptMemoryFreeBits));
		std::size_t blocks = 0;

		for (std::size_t i = 0; i < scriptMemoryFreeWords; ++i)
		{
			if ((words[i] & 0xFFFF) == 0xFFFF)
			{
				++blocks;
			}

			if ((words[i] >> 16) == 0xFFFF)
			{
				++blocks;
			}
		}

		return blocks;
	}

	int ClientSlots::FirstSlotForNewBot()
	{
		return newBotFirstSlot;
	}


	ClientSlots::ClientSlots()
	{
		static_assert(CLIENT_LIMIT == Game::MAX_CLIENTS);

		ClientSlots_cgameClientInfo = reinterpret_cast<std::uint8_t*>(Utils::Hook::Rebase(0x1405804D0));
		ClientSlots_scriptChildCounts = reinterpret_cast<std::uint32_t*>(Utils::Hook::Rebase(stockScriptChildCounts));

		if (!MoveScriptVariables())
		{
			return;
		}

		if (!MoveSlotArrays())
		{
			return;
		}

		ServerCommands::OnCommand(scoreboardMore, []([[maybe_unused]] const Command::Params* params)
		{
			return ParseMoreScores();
		});
	}
}
