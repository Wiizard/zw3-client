#include "STDInclude.hpp"

#include "AssetHandler.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "ModelSurfs.hpp"
#include "Scheduler.hpp"
#include "Zones.hpp"

#include "AssetInterfaces/IWeapon.hpp"
#include "AssetInterfaces/ISndCurve.hpp"
#include "AssetInterfaces/ITracerDef.hpp"
#include "AssetInterfaces/ILoadedSound.hpp"
#include "AssetInterfaces/Isnd_alias_list_t.hpp"
#include "AssetInterfaces/IXModel.hpp"
#include "AssetInterfaces/IPhysPreset.hpp"
#include "AssetInterfaces/IXAnimParts.hpp"
#include "AssetInterfaces/IFxEffectDef.hpp"
#include "AssetInterfaces/IPhysCollmap.hpp"
#include "AssetInterfaces/IXModelSurfs.hpp"
#include "AssetInterfaces/IFxWorld.hpp"
#include "AssetInterfaces/IMapEnts.hpp"
#include "AssetInterfaces/IComWorld.hpp"
#include "AssetInterfaces/IGfxWorld.hpp"
#include "AssetInterfaces/IclipMap_t.hpp"
#include "AssetInterfaces/IGameWorldMp.hpp"
#include "AssetInterfaces/IGameWorldSp.hpp"
#include "AssetInterfaces/IFont_s.hpp"
#include "AssetInterfaces/IRawFile.hpp"
#include "AssetInterfaces/IGfxImage.hpp"
#include "AssetInterfaces/IMaterial.hpp"
#include "AssetInterfaces/IMenuList.hpp"
#include "AssetInterfaces/ImenuDef_t.hpp"
#include "AssetInterfaces/IGfxLightDef.hpp"
#include "AssetInterfaces/IStringTable.hpp"
#include "AssetInterfaces/ILocalizeEntry.hpp"
#include "AssetInterfaces/IMaterialPixelShader.hpp"
#include "AssetInterfaces/IMaterialTechniqueSet.hpp"
#include "AssetInterfaces/IMaterialVertexShader.hpp"
#include "AssetInterfaces/IStructuredDataDefSet.hpp"
#include "AssetInterfaces/IMaterialVertexDeclaration.hpp"

namespace Components
{
	bool AssetHandler::isInstalled = false;
	std::vector<std::function<AssetHandler::LoadCallback>> AssetHandler::loadCallbacks;
	std::unordered_map<unsigned int, std::vector<std::function<AssetHandler::LoadCallback>>> AssetHandler::typeLoadCallbacks;
	std::map<unsigned int, std::vector<std::function<AssetHandler::FindCallback>>> AssetHandler::findCallbacks;

	bool AssetHandler::shouldSearchTempAssets = false;
	std::map<std::string, Game::XAssetHeader> AssetHandler::temporaryAssets[Game::ASSET_TYPE_COUNT];
	std::map<Game::XAssetType, std::unique_ptr<AssetHandler::IAsset>> AssetHandler::assetInterfaces;

	static std::unordered_set<std::string> dumpedAssets;

	constexpr std::uintptr_t DB_AddXAsset = 0x14012E700;

	constexpr std::uintptr_t DB_GetXAssetName = 0x140117610;

	static const std::uintptr_t DB_AddXAssetCalls[] =
	{
		0x140130201,
		0x1401302C6,
		0x140130381,
		0x140130441,
		0x140130501,
		0x1401305F1,
		0x1401306B1,
		0x140130771,
		0x140130831,
		0x140130901,
		0x1401309C1,
		0x140130A81,
		0x140130B41,
		0x140130C01,
		0x140130CC1,
		0x140130D81,
		0x140130E41,
		0x140130F01,
		0x140130FC1,
		0x1401310A1,
		0x140131161,
		0x140131221,
		0x140131321,
		0x1401313E1,
		0x14013149E,
		0x140131561,
		0x140131621,
		0x1401316E1,
		0x1401317A1,
		0x140131861,
		0x140131921,
		0x1401319E1,
		0x140131AA1,
		0x140131B61,
		0x140131C31,
		0x140131D21,
	};

	static Utils::Hook addXAssetHooks[std::size(DB_AddXAssetCalls)];

	constexpr std::uintptr_t DB_LoadXFile_LoadDelayedImagesCall = 0x140117F61;
	constexpr std::uintptr_t DB_LoadDelayedImages = 0x14012EAB0;
	constexpr std::uintptr_t R_DelayLoadImage = 0x140037A70;

	constexpr std::uintptr_t g_copyInfoCount = 0x14151D648;
	constexpr std::uintptr_t g_copyInfo = 0x14151D650;

	static_assert(offsetof(Game::GfxImage, delayLoadPixels) == 0x1E);

	static Utils::Hook loadDelayedImagesHook;

	constexpr std::uintptr_t imageBase = 0x140000000;
	constexpr std::uintptr_t DB_AddXAsset_CopyInfoBound = 0x14012EA56;
	constexpr std::uintptr_t DB_AddXAsset_CopyInfoStore = 0x14012EA81;
	static const std::uint8_t copyInfoBound[] = { 0x81, 0xF9, 0x00, 0x08, 0x00, 0x00 };
	static const std::uint8_t copyInfoStore[] = { 0x4C, 0x89, 0xB4, 0xC2, 0x50, 0xD6, 0x51, 0x01 };

	static const Utils::Hook::LeaSite copyInfoLeas[] =
	{
		{ 0x14012EAE6, Utils::Hook::leaRcx, g_copyInfo },
		{ 0x14012F2AC, { 0x4C, 0x8D, 0x35 }, g_copyInfo },
	};

	constexpr std::uint32_t copyInfoCapacity = 65536;

	static const Game::XAsset* const* copyInfo = nullptr;

	static void RaiseCopyInfo()
	{
		bool isExpected = Utils::Hook::MatchesBytes(DB_AddXAsset_CopyInfoBound, copyInfoBound, sizeof(copyInfoBound))
			&& Utils::Hook::MatchesBytes(DB_AddXAsset_CopyInfoStore, copyInfoStore, sizeof(copyInfoStore));

		for (const auto& lea : copyInfoLeas)
		{
			isExpected = isExpected && Utils::Hook::IsLeaIntact(lea);
		}

		if (!isExpected)
		{
			Logger::Error("assethandler: g_copyInfo's sites do not read as expected, a zone load still overrides at most 2048 assets\n");
			return;
		}

		auto* const block = Utils::Hook::AllocateDataNear(DB_AddXAsset_CopyInfoStore, copyInfoCapacity * sizeof(Game::XAsset*));

		if (!block)
		{
			Logger::Error("assethandler: no room near the image for g_copyInfo, a zone load still overrides at most 2048 assets\n");
			return;
		}

		const auto imageOffset = reinterpret_cast<std::int64_t>(block) - static_cast<std::int64_t>(Utils::Hook::Rebase(imageBase));
		bool canReach = imageOffset >= std::numeric_limits<std::int32_t>::min() && imageOffset <= std::numeric_limits<std::int32_t>::max();

		for (const auto& lea : copyInfoLeas)
		{
			canReach = canReach && Utils::Hook::CanLeaReach(lea, block);
		}

		if (!canReach)
		{
			Logger::Error("assethandler: g_copyInfo's block is out of reach, a zone load still overrides at most 2048 assets\n");
			return;
		}

		Utils::Hook::Set<std::uint32_t>(DB_AddXAsset_CopyInfoBound + 2, copyInfoCapacity);
		Utils::Hook::Set<std::int32_t>(DB_AddXAsset_CopyInfoStore + 4, static_cast<std::int32_t>(imageOffset));

		for (const auto& lea : copyInfoLeas)
		{
			Utils::Hook::PointLeaAt(lea, block);
		}

		copyInfo = static_cast<const Game::XAsset* const*>(block);
	}

	static void ListDelayLoadImage(void* header, void* data)
	{
		auto* const image = static_cast<Game::GfxImage*>(header);

		if (!image->delayLoadPixels)
		{
			return;
		}

		image->delayLoadPixels = false;
		static_cast<std::vector<Game::GfxImage*>*>(data)->push_back(image);
	}

	static void DB_LoadDelayedImages_Hook()
	{
		std::vector<Game::GfxImage*> images;

		Game::DB_EnumXAssets_FastFile(Game::ASSET_TYPE_IMAGE, ListDelayLoadImage, &images, false);

		const auto copyCount = *reinterpret_cast<const unsigned int*>(Utils::Hook::Rebase(g_copyInfoCount));
		const auto* const copies = copyInfo;

		for (unsigned int i = 0; i < copyCount; ++i)
		{
			if (copies[i]->type == Game::ASSET_TYPE_IMAGE)
			{
				ListDelayLoadImage(copies[i]->header.data, &images);
			}
		}

		const auto delayLoadImage = reinterpret_cast<void(*)(Game::GfxImage*)>(Utils::Hook::Rebase(R_DelayLoadImage));

		for (auto* const image : images)
		{
			delayLoadImage(image);
		}
	}

	constexpr std::uintptr_t j_DB_EnumXAssets_FastFile = 0x14027E8C0;

	static bool SeatEnumHooks(std::span<const std::uintptr_t> sites, std::span<Utils::Hook> hooks, void* stub)
	{
		for (const auto site : sites)
		{
			if (!Utils::Hook::BranchesTo(site, j_DB_EnumXAssets_FastFile, HOOK_CALL))
			{
				return false;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < sites.size(); ++i)
		{
			isSeated = hooks[i].Initialize(sites[i], stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			return false;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		return true;
	}

	static const std::uintptr_t techsetEnumCalls[] = { 0x14001D2F6, 0x14003AD0E };
	constexpr std::uintptr_t Material_RemapTechniqueSetName = 0x14003A9C0;

	constexpr std::uintptr_t techsetRemapMask = 0x1493DCD84;
	constexpr std::uintptr_t techsetRemapValue = 0x1493DCD88;

	static_assert(offsetof(Game::MaterialTechniqueSet, remappedTechniqueSet) == 0x10);

	static Utils::Hook remapTechsetHooks[std::size(techsetEnumCalls)];

	static void ListTechniqueSet(void* header, void* data)
	{
		static_cast<std::vector<Game::MaterialTechniqueSet*>*>(data)->push_back(static_cast<Game::MaterialTechniqueSet*>(header));
	}

	static void RemapTechniqueSets(Game::XAssetType type, void(*)(void*, void*), void*, bool includeOverride)
	{
		std::vector<Game::MaterialTechniqueSet*> techsets;
		techsets.reserve(Game::g_poolSize[type]);

		Game::DB_EnumXAssets_FastFile(type, ListTechniqueSet, &techsets, includeOverride);

		const auto mask = *reinterpret_cast<const int*>(Utils::Hook::Rebase(techsetRemapMask));
		const auto value = *reinterpret_cast<const int*>(Utils::Hook::Rebase(techsetRemapValue));
		const auto remapName = reinterpret_cast<void(*)(const char*, char*, int, int)>(Utils::Hook::Rebase(Material_RemapTechniqueSetName));

		for (auto it = techsets.rbegin(); it != techsets.rend(); ++it)
		{
			auto* const techset = *it;
			Game::MaterialTechniqueSet* remapped = techset;

			if (!Game::DB_IsXAssetDefault(type, techset->name))
			{
				char remappedName[256];
				remapName(techset->name, remappedName, mask, value);

				if (std::strcmp(techset->name, remappedName) != 0)
				{
					auto* const found = static_cast<Game::MaterialTechniqueSet*>(Game::DB_FindXAssetHeader(type, remappedName));

					if (!Game::DB_IsXAssetDefault(type, remappedName) && found)
					{
						remapped = found;
					}
				}
			}

			techset->remappedTechniqueSet = remapped;
		}
	}

	static const std::uintptr_t R_ImageList_f_EnumCalls[] = { 0x140038327, 0x140038396 };
	constexpr std::uintptr_t R_AddImageToList = 0x140037660;

	constexpr std::uint32_t imageListCapacity = 0x2000;

	static Utils::Hook imageListHooks[std::size(R_ImageList_f_EnumCalls)];

	static void AddImageToListBounded(void* header, void* data)
	{
		if (*static_cast<const std::uint32_t*>(data) >= imageListCapacity)
		{
			return;
		}

		reinterpret_cast<void(*)(void*, void*)>(Utils::Hook::Rebase(R_AddImageToList))(header, data);
	}

	static void EnumImagesBounded(Game::XAssetType type, void(*)(void*, void*), void* data, bool includeOverride)
	{
		Game::DB_EnumXAssets_FastFile(type, AddImageToListBounded, data, includeOverride);
	}

	constexpr std::uintptr_t lostDeviceEnumList = 0x146655B10;

	static const Utils::Hook::LeaSite lostDeviceEnumListLeas[] =
	{
		{ 0x14027E90C, { 0x4C, 0x8D, 0x35 }, lostDeviceEnumList },
		{ 0x14027E946, Utils::Hook::leaR8, lostDeviceEnumList },
	};

	static bool RaiseLostDeviceEnumList(const std::uint32_t capacity)
	{
		for (const auto& lea : lostDeviceEnumListLeas)
		{
			if (!Utils::Hook::IsLeaIntact(lea))
			{
				Logger::Error("assethandler: 0x{:X} does not reach the lost device list, the image and shader pools stay stock\n", lea.address);
				return false;
			}
		}

		auto* const block = Utils::Hook::AllocateDataNear(lostDeviceEnumList, capacity * sizeof(void*));

		if (!block)
		{
			Logger::Error("assethandler: no room near the image for the lost device list, the image and shader pools stay stock\n");
			return false;
		}

		for (const auto& lea : lostDeviceEnumListLeas)
		{
			if (!Utils::Hook::CanLeaReach(lea, block))
			{
				Logger::Error("assethandler: the lost device list's block is out of reach, the image and shader pools stay stock\n");
				return false;
			}
		}

		for (const auto& lea : lostDeviceEnumListLeas)
		{
			Utils::Hook::PointLeaAt(lea, block);
		}

		return true;
	}

	constexpr std::uint32_t materialCapacity = 16384;
	constexpr std::uint32_t materialPoolSize = 4096;

	constexpr std::uintptr_t g_MaterialPool = 0x140FC6E00;

	constexpr std::uintptr_t rgp = 0x148CCA600;
	constexpr std::uintptr_t rgpSortKeyStarts = 0x148CCC600;
	constexpr std::uint32_t rgpListStock = 0x2000;
	constexpr std::uint32_t rgpSortKeyStartsSize = 64 * sizeof(std::uint16_t);

	constexpr std::uintptr_t materialSortKeys = 0x148F0F900;

	constexpr std::uintptr_t g_drawConsts = 0x148F0F800;

	static const Utils::Hook::LeaSite materialPoolLeas[] =
	{
		{ 0x14012DD32, Utils::Hook::leaRcx, g_MaterialPool + sizeof(void*) },
		{ 0x14012DD50, { 0x48, 0x8D, 0x05 }, g_MaterialPool + sizeof(void*) },
	};

	static const Utils::Hook::LeaSite rgpListLeas[] =
	{
		{ 0x14001D441, { 0x4C, 0x8D, 0x3D }, rgp },
		{ 0x14001EA74, Utils::Hook::leaRcx, rgp },
		{ 0x140032A50, Utils::Hook::leaRcx, rgp },
		{ 0x140051CDF, Utils::Hook::leaRdx, rgp },
		{ 0x140077530, Utils::Hook::leaRcx, rgp },
		{ 0x140077626, Utils::Hook::leaRcx, rgp },
		{ 0x140077B38, Utils::Hook::leaRcx, rgp },
	};

	static const Utils::Hook::LeaSite rgpStartsLeas[] =
	{
		{ 0x14001D523, { 0x48, 0x8D, 0x35 }, rgpSortKeyStarts },
	};

	static const Utils::Hook::LeaSite materialSortKeysLeas[] =
	{
		{ 0x14001D471, { 0x48, 0x8D, 0x35 }, materialSortKeys },
		{ 0x1400507DF, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x140050994, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x140050AE8, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x140050C18, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x140050D48, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x140050E7B, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x1400510F7, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x140051227, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x140051377, { 0x48, 0x8D, 0x0D }, materialSortKeys },
		{ 0x1400514C7, { 0x4C, 0x8D, 0x05 }, materialSortKeys },
		{ 0x140051617, { 0x48, 0x8D, 0x15 }, materialSortKeys },
		{ 0x140051777, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x140051796, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x1400517B6, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x1400517D9, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x140051816, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x14005183A, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x14005185A, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x14005187A, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x14005189A, { 0x48, 0x8D, 0x05 }, materialSortKeys },
		{ 0x1400518BA, { 0x48, 0x8D, 0x05 }, materialSortKeys },
	};

	static const Utils::Hook::LeaSite drawConstsLeas[] =
	{
		{ 0x140049D73, { 0x48, 0x8D, 0x2D }, g_drawConsts },
		{ 0x14004A574, { 0x4C, 0x8D, 0x15 }, g_drawConsts },
	};

	struct ImageOffsetSite
	{
		std::uintptr_t address;
		std::uint32_t dispOffset;
		std::uintptr_t target;
	};

	static const ImageOffsetSite rgpImageOffsetSites[] =
	{
		{ 0x140023F07, 5, rgp },
		{ 0x1400235A6, 5, rgpSortKeyStarts },
		{ 0x1400235B2, 5, rgpSortKeyStarts },
		{ 0x140023B3A, 4, rgpSortKeyStarts },
		{ 0x140023B45, 5, rgpSortKeyStarts },
		{ 0x140023C60, 5, rgpSortKeyStarts },
		{ 0x140023C6C, 5, rgpSortKeyStarts },
		{ 0x140023FF1, 5, materialSortKeys },
		{ 0x140024022, 5, materialSortKeys },
	};

	struct ValueSite
	{
		std::uintptr_t address;
		std::uint32_t valueOffset;
		std::uint32_t stock;
		std::uint32_t raised;
	};

	static const ValueSite rgpValueSites[] =
	{
		{ 0x14001D569, 1, 0x1000, materialCapacity },
		{ 0x14001D56E, 5, rgpListStock, materialCapacity * 2 },
		{ 0x14001EA94, 5, rgpListStock, materialCapacity * 2 },
		{ 0x14001EAA1, 4, rgpListStock, materialCapacity * 2 },
		{ 0x140032A57, 2, rgpListStock + rgpSortKeyStartsSize, materialCapacity * 2 + rgpSortKeyStartsSize },
	};

	static std::int64_t ImageOffsetOf(const void* target)
	{
		return reinterpret_cast<std::int64_t>(target) - static_cast<std::int64_t>(Utils::Hook::Rebase(imageBase));
	}

	static bool CanImageOffsetReach(const void* target)
	{
		const auto offset = ImageOffsetOf(target);
		return offset >= std::numeric_limits<std::int32_t>::min() && offset <= std::numeric_limits<std::int32_t>::max();
	}

	static bool AreLeasIntact(std::span<const Utils::Hook::LeaSite> leas)
	{
		for (const auto& lea : leas)
		{
			if (!Utils::Hook::IsLeaIntact(lea))
			{
				return false;
			}
		}

		return true;
	}

	static bool CanLeasReach(std::span<const Utils::Hook::LeaSite> leas, const void* target)
	{
		for (const auto& lea : leas)
		{
			if (!Utils::Hook::CanLeaReach(lea, target))
			{
				return false;
			}
		}

		return true;
	}

	static void PointLeasAt(std::span<const Utils::Hook::LeaSite> leas, const void* target)
	{
		for (const auto& lea : leas)
		{
			Utils::Hook::PointLeaAt(lea, target);
		}
	}

	static bool RaiseMaterialPool()
	{
		bool isExpected = AreLeasIntact(materialPoolLeas) && AreLeasIntact(rgpListLeas) && AreLeasIntact(rgpStartsLeas)
			&& AreLeasIntact(materialSortKeysLeas) && AreLeasIntact(drawConstsLeas);

		for (const auto& site : rgpImageOffsetSites)
		{
			const auto disp = *reinterpret_cast<const std::int32_t*>(Utils::Hook::Rebase(site.address) + site.dispOffset);
			isExpected = isExpected && disp == static_cast<std::int32_t>(site.target - imageBase);
		}

		for (const auto& site : rgpValueSites)
		{
			isExpected = isExpected && *reinterpret_cast<const std::uint32_t*>(Utils::Hook::Rebase(site.address) + site.valueOffset) == site.stock;
		}

		if (!isExpected)
		{
			Logger::Error("assethandler: the material sort's sites do not read as expected, the material pool stays static\n");
			return false;
		}

		const auto entrySize = static_cast<std::size_t>(Game::DB_GetXAssetTypeSize(Game::ASSET_TYPE_MATERIAL));
		auto* const pool = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(g_MaterialPool, sizeof(void*) + materialCapacity * entrySize));
		auto* const list = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(rgp, materialCapacity * sizeof(std::uint16_t) + rgpSortKeyStartsSize));
		auto* const sortKeys = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(materialSortKeys, materialCapacity));

		if (!pool || !list || !sortKeys)
		{
			Logger::Error("assethandler: no room near the image for the material sort, the material pool stays static\n");
			return false;
		}

		auto* const entries = pool + sizeof(void*);
		auto* const starts = list + materialCapacity * sizeof(std::uint16_t);
		auto* const drawConsts = sortKeys - (materialSortKeys - g_drawConsts);

		const bool canReach = CanImageOffsetReach(list) && CanImageOffsetReach(starts) && CanImageOffsetReach(sortKeys)
			&& CanLeasReach(materialPoolLeas, entries) && CanLeasReach(rgpListLeas, list)
			&& CanLeasReach(rgpStartsLeas, starts) && CanLeasReach(materialSortKeysLeas, sortKeys)
			&& CanLeasReach(drawConstsLeas, drawConsts);

		if (!canReach)
		{
			Logger::Error("assethandler: the material sort's blocks are out of reach, the material pool stays static\n");
			return false;
		}

		Game::DB_XAssetPool[Game::ASSET_TYPE_MATERIAL] = pool;
		Game::g_poolSize[Game::ASSET_TYPE_MATERIAL] = materialPoolSize;

		PointLeasAt(materialPoolLeas, entries);
		PointLeasAt(rgpListLeas, list);
		PointLeasAt(rgpStartsLeas, starts);
		PointLeasAt(materialSortKeysLeas, sortKeys);
		PointLeasAt(drawConstsLeas, drawConsts);

		for (const auto& site : rgpImageOffsetSites)
		{
			const void* target = list;

			if (site.target == rgpSortKeyStarts)
			{
				target = starts;
			}
			else if (site.target == materialSortKeys)
			{
				target = sortKeys;
			}

			Utils::Hook::Set<std::int32_t>(site.address + site.dispOffset, static_cast<std::int32_t>(ImageOffsetOf(target)));
		}

		for (const auto& site : rgpValueSites)
		{
			Utils::Hook::Set<std::uint32_t>(site.address + site.valueOffset, site.raised);
		}

		return true;
	}

	extern "C"
	{
		void XModelKeyStub();
		void BModelProbeStub();
		void SkinnedProbeStub();
		void RigidSkinnedProbeStub();
		void RigidProbeStub();
		void WorldSlotStub();
		void DwordSlotStub();
		void SModelSlotStub();
		void SetupWorldIndexStub();
		void SetupSlotStub();
		void SetupSModelIndexStub();
		void SetupDwordSlotStub();
		void SetupWordSlotStub();
	}

	struct KeySite
	{
		std::uintptr_t address;
		std::string_view stock;
		std::string_view widened;
	};

	struct KeyStubSite
	{
		std::uintptr_t address;
		std::string_view stock;
		void(*stub)();
	};

	static const KeySite drawSurfKeySites[] =
	{
		{ 0x14001D4D3, "\x48\xC1\xE1\x0C"sv, "\x48\xC1\xE1\x0E"sv },
		{ 0x14001D4DA, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x14001D4F8, "\x48\xC1\xE1\x13"sv, "\x48\xC1\xE1\x11"sv },
		{ 0x14005400D, "\x48\xA9\x00\x00\x00\x3E"sv, "\x48\xA9\x00\x00\x80\x0F"sv },
		{ 0x140052EAF, "\x48\xF7\xC1\x00\x00\x00\x3E"sv, "\x48\xF7\xC1\x00\x00\x80\x0F"sv },
		{ 0x1400530F7, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x1400530FB, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x1400530FF, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x140053104, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140053594, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140053598, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x14005359C, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x1400535A1, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140053601, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140053605, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140053609, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x14005360F, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x140053671, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140053675, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140053679, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x14005367F, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x1400536CC, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x1400536D0, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x1400536D4, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x1400536DA, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x14005372F, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140053733, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140053737, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x14005373D, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x1400538E9, "\x48\xC1\xEA\x1E"sv, "\x48\xC1\xEA\x1C"sv },
		{ 0x1400538F0, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140053901, "\x81\xE3\xFF\x0F\x00\x00"sv, "\x81\xE3\xFF\x3F\x00\x00"sv },
		{ 0x1400539C1, "\x48\xC1\xEB\x1E"sv, "\x48\xC1\xEB\x1C"sv },
		{ 0x1400539C5, "\x81\xE3\xFF\x0F\x00\x00"sv, "\x81\xE3\xFF\x3F\x00\x00"sv },
		{ 0x1400539DD, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x1400539E1, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140053A52, "\x48\xC1\xEB\x1E"sv, "\x48\xC1\xEB\x1C"sv },
		{ 0x140053A56, "\x81\xE3\xFF\x0F\x00\x00"sv, "\x81\xE3\xFF\x3F\x00\x00"sv },
		{ 0x140053A6E, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140053A72, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140053ADB, "\x48\xC1\xEB\x1E"sv, "\x48\xC1\xEB\x1C"sv },
		{ 0x140053ADF, "\x81\xE3\xFF\x0F\x00\x00"sv, "\x81\xE3\xFF\x3F\x00\x00"sv },
		{ 0x140053AF7, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140053AFB, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140053B56, "\x48\xC1\xEB\x1E"sv, "\x48\xC1\xEB\x1C"sv },
		{ 0x140053B5A, "\x81\xE3\xFF\x0F\x00\x00"sv, "\x81\xE3\xFF\x3F\x00\x00"sv },
		{ 0x140053B73, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140053B77, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140053BE5, "\x48\xC1\xEB\x1E"sv, "\x48\xC1\xEB\x1C"sv },
		{ 0x140053BE9, "\x81\xE3\xFF\x0F\x00\x00"sv, "\x81\xE3\xFF\x3F\x00\x00"sv },
		{ 0x140053C02, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140053C06, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140058443, "\xB8\xFF\x0F\x00\x00"sv, "\xB8\xFF\x3F\x00\x00"sv },
		{ 0x140058448, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x1400586EE, "\x49\xB8\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x49\xB8\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },
		{ 0x14005889C, "\x49\xB8\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x49\xB8\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },
		{ 0x140058C7B, "\x48\xBA\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x48\xBA\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },
		{ 0x14005910E, "\x48\xBA\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x48\xBA\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },
		{ 0x140059147, "\x48\xC1\xE9\x19"sv, "\x48\xC1\xE9\x17"sv },
		{ 0x1400592B8, "\x48\xBA\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x48\xBA\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },
		{ 0x1400595FA, "\x48\xB9\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x48\xB9\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },
		{ 0x140059A77, "\x48\xBA\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x48\xBA\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },
		{ 0x140059F3E, "\x48\xBA\x00\x00\xFF\xC0\xFF\xE3\x1F\x00"sv, "\x48\xBA\x00\x00\x3F\xF0\xFF\xE3\x1F\x00"sv },

		{ 0x1400202C6, "\x49\xC1\xE1\x15"sv, "\x49\xC1\xE1\x17"sv },
		{ 0x1400202E3, "\x49\xC1\xE1\x08"sv, "\x49\xC1\xE1\x06"sv },
		{ 0x140020305, "\x48\xBA\x00\x00\x00\xC0\xFF\x1F\x00\x80"sv, "\x48\xBA\x00\x00\x00\xF0\xFF\x1F\x00\x80"sv },
		{ 0x1400206B1, "\x81\xE2\x00\x01\x00\x00"sv, "\x81\xE2\x40\x00\x00\x00"sv },
		{ 0x1400206F8, "\x48\xB9\x00\x00\x00\xC0\xFF\x0F\x00\x80"sv, "\x48\xB9\x00\x00\x00\xF0\xFF\x0F\x00\x80"sv },
		{ 0x1400208EA, "\x48\xBA\x00\x00\xFF\xFE\xFF\xFF\x1F\x80"sv, "\x48\xBA\x00\x00\xBF\xFF\xFF\xFF\x1F\x80"sv },
		{ 0x1400208FA, "\x41\x81\xE0\x00\x00\x00\x01"sv, "\x41\x81\xE0\x00\x00\x40\x00"sv },
		{ 0x1400283AA, "\x49\xBD\x00\x00\x00\xC1\xFF\x1F\xC0\xFE"sv, "\x49\xBD\x00\x00\x40\xF0\xFF\x1F\xC0\xFE"sv },
		{ 0x140028478, "\x48\xC1\xE2\x14"sv, "\x48\xC1\xE2\x16"sv },
		{ 0x140028483, "\x48\xC1\xE2\x09"sv, "\x48\xC1\xE2\x07"sv },
		{ 0x1400285D1, "\x48\xC1\xE2\x14"sv, "\x48\xC1\xE2\x16"sv },
		{ 0x1400285E3, "\x48\xC1\xE2\x09"sv, "\x48\xC1\xE2\x07"sv },
		{ 0x1400285EA, "\x48\xB9\x00\x00\x00\xC1\xFF\x1F\xC0\xFE"sv, "\x48\xB9\x00\x00\x40\xF0\xFF\x1F\xC0\xFE"sv },
		{ 0x14007781F, "\x48\x25\x00\x00\x00\xC1"sv, "\x48\x25\x00\x00\x40\xF0"sv },
		{ 0x140077857, "\x48\xC1\xEE\x19"sv, "\x48\xC1\xEE\x17"sv },
		{ 0x140077AD9, "\x48\x25\x00\x00\x00\xC1"sv, "\x48\x25\x00\x00\x40\xF0"sv },
		{ 0x140075476, "\x48\x25\x00\x00\x00\xFF"sv, "\x48\x25\x00\x00\xC0\xFF"sv },
		{ 0x140075602, "\x48\x25\x00\x00\x00\xFF"sv, "\x48\x25\x00\x00\xC0\xFF"sv },
		{ 0x140075625, "\x48\x0F\xBA\xE6\x18"sv, "\x48\x0F\xBA\xE6\x16"sv },
		{ 0x1400756AE, "\x48\x81\xE6\x00\x00\x00\xC1"sv, "\x48\x81\xE6\x00\x00\x40\xF0"sv },
		{ 0x1400756D6, "\x48\x81\xE1\x00\x00\x00\xC1"sv, "\x48\x81\xE1\x00\x00\x40\xF0"sv },
		{ 0x1400756F4, "\x48\x0F\xBA\xE0\x18"sv, "\x48\x0F\xBA\xE0\x16"sv },
		{ 0x1400758AB, "\x48\x81\xE6\x00\x00\x00\xC1"sv, "\x48\x81\xE6\x00\x00\x40\xF0"sv },
		{ 0x1400758D6, "\x48\x81\xE1\x00\x00\x00\xC1"sv, "\x48\x81\xE1\x00\x00\x40\xF0"sv },
		{ 0x1400758F0, "\x48\x0F\xBA\xE0\x18"sv, "\x48\x0F\xBA\xE0\x16"sv },
		{ 0x1400759BC, "\x48\x0F\xBA\xE0\x18"sv, "\x48\x0F\xBA\xE0\x16"sv },
		{ 0x140075E1D, "\x48\xC7\xC0\x00\x00\x00\xC1"sv, "\x48\xC7\xC0\x00\x00\x40\xF0"sv },
		{ 0x140075E49, "\x48\x81\xE1\x00\x00\x00\xC1"sv, "\x48\x81\xE1\x00\x00\x40\xF0"sv },
		{ 0x1400764FA, "\x48\x25\x00\x00\x00\xC1"sv, "\x48\x25\x00\x00\x40\xF0"sv },
		{ 0x14007B6D2, "\x48\x81\xE1\x00\x00\x00\xFF"sv, "\x48\x81\xE1\x00\x00\xC0\xFF"sv },
		{ 0x14007BBFB, "\x48\x81\xE1\x00\x00\x00\xFF"sv, "\x48\x81\xE1\x00\x00\xC0\xFF"sv },
		{ 0x14007BC1A, "\x48\x0F\xBA\xE0\x18"sv, "\x48\x0F\xBA\xE0\x16"sv },
		{ 0x14007BD74, "\x48\x81\xE1\x00\x00\x00\xC1"sv, "\x48\x81\xE1\x00\x00\x40\xF0"sv },
		{ 0x14007C106, "\x48\x81\xE1\x00\x00\x00\xC1"sv, "\x48\x81\xE1\x00\x00\x40\xF0"sv },
		{ 0x14007C121, "\x48\x0F\xBA\xE0\x18"sv, "\x48\x0F\xBA\xE0\x16"sv },
		{ 0x14007C269, "\x48\x81\xE1\x00\x00\x00\xC1"sv, "\x48\x81\xE1\x00\x00\x40\xF0"sv },
		{ 0x14007C5D6, "\x48\x81\xE1\x00\x00\x00\xC1"sv, "\x48\x81\xE1\x00\x00\x40\xF0"sv },

		{ 0x14001E26E, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x14001E272, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x14001E3D7, "\x41\xB8\xFF\x0F\x00\x00"sv, "\x41\xB8\xFF\x3F\x00\x00"sv },
		{ 0x14001E3EA, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x14001F7BB, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x14001F7C0, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x14001F84B, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x14001F850, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x14003D31A, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x14003D351, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x14003E9DE, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x14003E9EA, "\xBA\xFF\x0F\x00\x00"sv, "\xBA\xFF\x3F\x00\x00"sv },
		{ 0x140043313, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140043317, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x14005F266, "\x49\xC1\xEF\x1E"sv, "\x49\xC1\xEF\x1C"sv },
		{ 0x14005F26E, "\x41\x81\xE7\xFF\x0F\x00\x00"sv, "\x41\x81\xE7\xFF\x3F\x00\x00"sv },
		{ 0x14005F3D9, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x14005F3E2, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x14005F3E8, "\x41\xB8\x00\xF0\x00\x00"sv, "\x41\xB8\x00\xC0\x00\x00"sv },
		{ 0x14005F544, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x14005F68D, "\x41\xBF\xFF\x0F\x00\x00"sv, "\x41\xBF\xFF\x3F\x00\x00"sv },
		{ 0x14005F6F5, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x14005F853, "\xBA\xFF\x0F\x00\x00"sv, "\xBA\xFF\x3F\x00\x00"sv },
		{ 0x14005F868, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x14005FA21, "\xBA\x00\xF0\x00\x00"sv, "\xBA\x00\xC0\x00\x00"sv },
		{ 0x14005FA2D, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x14005FA31, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x14006086A, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x14006086E, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140060D94, "\x41\xBA\xFF\x0F\x00\x00"sv, "\x41\xBA\xFF\x3F\x00\x00"sv },
		{ 0x140060DD5, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x140060E8C, "\xB8\xFF\x0F\x00\x00"sv, "\xB8\xFF\x3F\x00\x00"sv },
		{ 0x140060EF1, "\x49\xC1\xE9\x1E"sv, "\x49\xC1\xE9\x1C"sv },
		{ 0x140061355, "\x41\xBA\xFF\x0F\x00\x00"sv, "\x41\xBA\xFF\x3F\x00\x00"sv },
		{ 0x140061396, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x1400614D9, "\x41\xBB\xFF\x0F\x00\x00"sv, "\x41\xBB\xFF\x3F\x00\x00"sv },
		{ 0x1400614E8, "\x48\xC1\xE9\x1E"sv, "\x48\xC1\xE9\x1C"sv },
		{ 0x140061AA2, "\x41\xBE\xFF\x0F\x00\x00"sv, "\x41\xBE\xFF\x3F\x00\x00"sv },
		{ 0x140061AF1, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x140061B35, "\x41\xBB\xFF\x0F\x00\x00"sv, "\x41\xBB\xFF\x3F\x00\x00"sv },
		{ 0x140061B51, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x14005F00D, "\x41\x83\xF9\x10"sv, "\x41\x83\xF9\x04"sv },
		{ 0x14005F03A, "\xBE\xFF\x0F\x00\x00"sv, "\xBE\xFF\x3F\x00\x00"sv },
		{ 0x14005F042, "\x66\x41\xC1\xE1\x0C"sv, "\x66\x41\xC1\xE1\x0E"sv },
		{ 0x14005F062, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x14005F066, "\xBE\xFF\x0F\x00\x00"sv, "\xBE\xFF\x3F\x00\x00"sv },
		{ 0x14005F104, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x140060620, "\xBB\xFF\x0F\x00\x00"sv, "\xBB\xFF\x3F\x00\x00"sv },
		{ 0x140060655, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x1400606B1, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x140060703, "\x66\xC1\xE0\x0C"sv, "\x66\xC1\xE0\x0E"sv },
		{ 0x140076708, "\xC1\xEB\x0C"sv, "\xC1\xEB\x0E"sv },
		{ 0x1400767C8, "\x41\xC1\xEF\x0C"sv, "\x41\xC1\xEF\x0E"sv },
		{ 0x140076B00, "\xC1\xEB\x0C"sv, "\xC1\xEB\x0E"sv },
		{ 0x14007B127, "\x41\xC1\xEF\x0C"sv, "\x41\xC1\xEF\x0E"sv },
		{ 0x14007B462, "\x41\xC1\xEC\x0C"sv, "\x41\xC1\xEC\x0E"sv },
		{ 0x14007B301, "\xC1\xEF\x0C"sv, "\xC1\xEF\x0E"sv },
		{ 0x1400782D7, "\x41\xBB\xFF\x0F\x00\x00"sv, "\x41\xBB\xFF\x3F\x00\x00"sv },
		{ 0x1400782F7, "\xC1\xEA\x0C"sv, "\xC1\xEA\x0E"sv },
		{ 0x140078348, "\xC1\xEA\x0C"sv, "\xC1\xEA\x0E"sv },
		{ 0x14001EB00, "\x41\xB9\x00\x10\x00\x00"sv, "\x41\xB9\x00\x40\x00\x00"sv },
		{ 0x140023630, "\x41\x81\xF9\x00\x10\x00\x00"sv, "\x41\x81\xF9\x00\x40\x00\x00"sv },
		{ 0x1400236EA, "\x41\xBF\x00\x10\x00\x00"sv, "\x41\xBF\x00\x80\x00\x00"sv },
		{ 0x140023BC1, "\x41\x81\xFA\x00\x10\x00\x00"sv, "\x41\x81\xFA\x00\x40\x00\x00"sv },
		{ 0x140023D10, "\x41\x81\xFB\x00\x10\x00\x00"sv, "\x41\x81\xFB\x00\x40\x00\x00"sv },

		{ 0x1400507B3, "\xBA\xFF\x0F\x00\x00"sv, "\xBA\xFF\x3F\x00\x00"sv },
		{ 0x1400507D1, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140050815, "\xBE\xFF\x0F\x00\x00"sv, "\xBE\xFF\x3F\x00\x00"sv },
		{ 0x14005083C, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140050887, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x1400508CD, "\x48\xC1\xE8\x1E"sv, "\x48\xC1\xE8\x1C"sv },
		{ 0x140050FA7, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x140050FCB, "\x48\xC1\xEA\x1E"sv, "\x48\xC1\xEA\x1C"sv },
		{ 0x140050FFC, "\x48\x81\xE2\x00\x00\x00\xC1"sv, "\x48\x81\xE2\x00\x00\x40\xF0"sv },
		{ 0x140051013, "\x48\x25\x00\x00\x00\xC1"sv, "\x48\x25\x00\x00\x40\xF0"sv },
		{ 0x140050AD4, "\xBA\xFF\x0F\x00\x00"sv, "\xBA\xFF\x3F\x00\x00"sv },
		{ 0x140050C04, "\xBA\xFF\x0F\x00\x00"sv, "\xBA\xFF\x3F\x00\x00"sv },
		{ 0x140050D34, "\xBA\xFF\x0F\x00\x00"sv, "\xBA\xFF\x3F\x00\x00"sv },
		{ 0x140050E67, "\xBA\xFF\x0F\x00\x00"sv, "\xBA\xFF\x3F\x00\x00"sv },
		{ 0x140050B2A, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x140050C5A, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x140050D8A, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x140050EBD, "\xB9\xFF\x0F\x00\x00"sv, "\xB9\xFF\x3F\x00\x00"sv },
		{ 0x1400517A1, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x1400517C1, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x1400517E4, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x140051821, "\x81\xE1\xFF\x0F\x00\x00"sv, "\x81\xE1\xFF\x3F\x00\x00"sv },
		{ 0x14004A614, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x14004A691, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x14004A70E, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x14004A7F8, "\x25\xFF\x0F\x00\x00"sv, "\x25\xFF\x3F\x00\x00"sv },
		{ 0x1400514E7, "\x0F\xB7\xC9"sv, "\xC1\xE9\x02"sv },
		{ 0x14005162C, "\x0F\xB7\xC9"sv, "\xC1\xE9\x02"sv },
	};

	static const KeyStubSite drawSurfKeyStubSites[] =
	{
		{ 0x14002044F, "\x49\x8B\xD5\x83\xE2\x01\x49\xB8\x00\x00\xFF\xFE\xFF\xFF\x1F\xFE\x49\x0B\xD2\x49\x23\xC8\x48\xC1\xE2\x18"sv, XModelKeyStub },
		{ 0x1400778EB, "\x48\xC1\xE8\x10\x41\xB0\x72\x0F\xB6\xC8"sv, BModelProbeStub },
		{ 0x1400755A9, "\x48\xC1\xEE\x10\xBA\x01\x00\x00\x00\x40\x0F\xB6\xCE"sv, SkinnedProbeStub },
		{ 0x14007616C, "\x48\xC1\xEB\x10\x0F\xB6\xD3"sv, RigidSkinnedProbeStub },
		{ 0x14007B87A, "\x48\xC1\xE8\x10\x0F\xB6\xD0"sv, RigidProbeStub },
		{ 0x1400507F5, "\xC1\xE0\x0D\x0B\xC2"sv, WorldSlotStub },
		{ 0x1400509D4, "\x41\x81\xE1\xFF\x0F\x00\x00"sv, DwordSlotStub },
		{ 0x140051137, "\x41\x81\xE1\xFF\x0F\x00\x00"sv, DwordSlotStub },
		{ 0x140051267, "\x41\x81\xE1\xFF\x0F\x00\x00"sv, DwordSlotStub },
		{ 0x1400513B7, "\x41\x81\xE1\xFF\x0F\x00\x00"sv, DwordSlotStub },
		{ 0x140050B00, "\xC1\xE0\x0D\x41\x0B\xC0"sv, SModelSlotStub },
		{ 0x140050C31, "\xC1\xE0\x0D\x41\x0B\xC0"sv, SModelSlotStub },
		{ 0x140050D60, "\xC1\xE0\x0D\x41\x0B\xC0"sv, SModelSlotStub },
		{ 0x140050E94, "\xC1\xE0\x0D\x41\x0B\xC0"sv, SModelSlotStub },
		{ 0x140049DA2, "\x48\xC1\xE8\x1E\x66\x23\xC6\x49\xC1\xE8\x2D\x0F\xB7\xC8"sv, SetupWorldIndexStub },
		{ 0x140049DC8, "\xC1\xE2\x0D\x0B\xD1"sv, SetupSlotStub },
		{ 0x140049EFA, "\xC1\xE2\x0D\x0B\xD1"sv, SetupSlotStub },
		{ 0x140049F81, "\xC1\xE2\x0D\x0B\xD1"sv, SetupSlotStub },
		{ 0x14004A00C, "\xC1\xE2\x0D\x0B\xD1"sv, SetupSlotStub },
		{ 0x14004A099, "\xC1\xE2\x0D\x0B\xD1"sv, SetupSlotStub },
		{ 0x140049ED7, "\x66\x23\xC6\x0F\xB7\xC8"sv, SetupSModelIndexStub },
		{ 0x140049F5C, "\x66\x23\xC6\x0F\xB7\xC8"sv, SetupSModelIndexStub },
		{ 0x140049FE9, "\x66\x23\xC6\x0F\xB7\xC8"sv, SetupSModelIndexStub },
		{ 0x14004A074, "\x66\x23\xC6\x0F\xB7\xC8"sv, SetupSModelIndexStub },
		{ 0x140049E5E, "\xC1\xE2\x0C\x44\x23\xCE"sv, SetupDwordSlotStub },
		{ 0x14004A1E2, "\xC1\xE2\x0C\x44\x23\xCE"sv, SetupDwordSlotStub },
		{ 0x14004A28F, "\xC1\xE2\x0C\x44\x23\xCE"sv, SetupDwordSlotStub },
		{ 0x14004A33C, "\xC1\xE2\x0C\x44\x23\xCE"sv, SetupDwordSlotStub },
		{ 0x14004A3DB, "\x0F\xB7\xC1\x83\xE2\x3F\x23\xC6"sv, SetupWordSlotStub },
		{ 0x14004A45F, "\x0F\xB7\xC1\x83\xE2\x3F\x23\xC6"sv, SetupWordSlotStub },
	};

	static Utils::Hook drawSurfKeyHooks[std::size(drawSurfKeyStubSites)];

	static bool AreKeySitesIntact()
	{
		for (const auto& site : drawSurfKeySites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, reinterpret_cast<const std::uint8_t*>(site.stock.data()), site.stock.size()))
			{
				return false;
			}
		}

		for (const auto& site : drawSurfKeyStubSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, reinterpret_cast<const std::uint8_t*>(site.stock.data()), site.stock.size()))
			{
				return false;
			}
		}

		return true;
	}

	static bool WidenDrawSurfKey()
	{
		if (!AreKeySitesIntact())
		{
			Logger::Error("assethandler: the draw surface key's sites do not read as expected, the key keeps a 12 bit material index\n");
			return false;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(drawSurfKeyStubSites); ++i)
		{
			const auto& site = drawSurfKeyStubSites[i];
			isSeated = drawSurfKeyHooks[i].Initialize(site.address, site.stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		std::uintptr_t first = std::numeric_limits<std::uintptr_t>::max();
		std::uintptr_t last = 0;

		for (const auto& site : drawSurfKeySites)
		{
			first = std::min(first, site.address);
			last = std::max(last, site.address + site.widened.size());
		}

		for (const auto& site : drawSurfKeyStubSites)
		{
			first = std::min(first, site.address);
			last = std::max(last, site.address + site.stock.size());
		}

		auto* const code = reinterpret_cast<std::uint8_t*>(Utils::Hook::Rebase(first));
		const std::size_t length = last - first;
		DWORD oldProtect;

		if (!isSeated || !VirtualProtect(code, length, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			for (auto& hook : drawSurfKeyHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("assethandler: could not seat every draw surface key site, the key keeps a 12 bit material index\n");
			return false;
		}

		for (const auto& site : drawSurfKeySites)
		{
			std::memcpy(code + (site.address - first), site.widened.data(), site.widened.size());
		}

		for (const auto& site : drawSurfKeyStubSites)
		{
			const std::size_t callLength = 5;
			std::memset(code + (site.address - first) + callLength, 0x90, site.stock.size() - callLength);
		}

		VirtualProtect(code, length, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), code, length);

		for (auto& hook : drawSurfKeyHooks)
		{
			hook.Quick();
		}

		return true;
	}

	constexpr unsigned int reflectionProbeLimit = 64;

	static unsigned int HighestReflectionProbe(const Game::GfxWorld* world)
	{
		unsigned int highest = 0;

		if (world->dpvs.surfaces)
		{
			for (unsigned int i = 0; i < world->surfaceCount; ++i)
			{
				highest = std::max<unsigned int>(highest, world->dpvs.surfaces[i].laf.fields.reflectionProbeIndex);
			}
		}

		if (world->dpvs.smodelDrawInsts)
		{
			for (unsigned int i = 0; i < world->dpvs.smodelCount; ++i)
			{
				highest = std::max<unsigned int>(highest, world->dpvs.smodelDrawInsts[i].reflectionProbeIndex);
			}
		}

		if (world->cells)
		{
			for (int i = 0; i < world->dpvsPlanes.cellCount; ++i)
			{
				const auto& cell = world->cells[i];
				const auto probeCount = static_cast<unsigned char>(cell.reflectionProbeCount);

				for (unsigned int j = 0; cell.reflectionProbes && j < probeCount; ++j)
				{
					highest = std::max<unsigned int>(highest, static_cast<unsigned char>(cell.reflectionProbes[j]));
				}
			}
		}

		return highest;
	}

	static void CheckReflectionProbes(unsigned int, void* asset, const std::string& name, bool*)
	{
		const auto* const world = static_cast<const Game::GfxWorld*>(asset);

		if (world->draw.reflectionProbeCount > reflectionProbeLimit)
		{
			Game::Com_Error(Game::ERR_DROP, "%s has %u reflection probes, this client draws at most %u", name.data(), world->draw.reflectionProbeCount, reflectionProbeLimit);
			return;
		}

		const auto highest = HighestReflectionProbe(world);

		if (highest >= reflectionProbeLimit)
		{
			Game::Com_Error(Game::ERR_DROP, "%s uses reflection probe %u, this client draws at most %u", name.data(), highest, reflectionProbeLimit);
		}
	}

	extern "C"
	{
		void SunShadowStreamStub();
	}

	constexpr std::uintptr_t s_backEndData = 0x149145900;
	constexpr std::size_t backEndDataSize = 0x13D700;
	constexpr std::size_t staticModelStreamSize = 4 * 0x1B000;
	constexpr std::uint8_t staticModelStreamLea[] = { 0x48, 0x8D, 0x8A, 0x80, 0x7F, 0x0B, 0x00 };
	constexpr std::size_t leaDispOffset = 3;

	static const std::uintptr_t staticModelStreamLeas[] = { 0x14005FDF1, 0x1400608EE, 0x140060965, 0x140060F4D, 0x14006152F };

	static const KeySite staticModelStreamSites[] =
	{
		{ 0x14005FDDB, "\xB8\x00\xA0\x00\x00"sv, "\xB8\x00\x80\x02\x00"sv },
		{ 0x14005FE23, "\x48\x8D\x81\x00\x20\x00\x00"sv, "\x48\x8D\x81\x00\x80\x00\x00"sv },
		{ 0x14005FE41, "\x48\x8D\x81\x00\x40\x00\x00"sv, "\x48\x8D\x81\x00\x00\x01\x00"sv },
		{ 0x14005FE54, "\x48\x8D\x81\x00\x60\x00\x00"sv, "\x48\x8D\x81\x00\x80\x01\x00"sv },
		{ 0x14005FE7F, "\x48\x81\xC1\x00\x80\x00\x00"sv, "\x48\x81\xC1\x00\x00\x02\x00"sv },
		{ 0x14005FE9C, "\x48\x8D\x81\x00\x04\x00\x00"sv, "\x48\x8D\x81\x00\x10\x00\x00"sv },
		{ 0x14005FEBF, "\x48\x8D\x81\x00\x08\x00\x00"sv, "\x48\x8D\x81\x00\x20\x00\x00"sv },
		{ 0x14005FEDB, "\x48\x8D\x81\x00\x0C\x00\x00"sv, "\x48\x8D\x81\x00\x30\x00\x00"sv },
		{ 0x14005FFFC, "\x48\x81\xC1\x00\x10\x00\x00"sv, "\x48\x81\xC1\x00\x40\x00\x00"sv },
		{ 0x140060065, "\x48\x8D\x81\x00\x04\x00\x00"sv, "\x48\x8D\x81\x00\x10\x00\x00"sv },
		{ 0x14006008F, "\x48\x8D\x81\x00\x08\x00\x00"sv, "\x48\x8D\x81\x00\x20\x00\x00"sv },
		{ 0x14006027A, "\x48\x8D\x81\x00\x0C\x00\x00"sv, "\x48\x8D\x81\x00\x30\x00\x00"sv },
		{ 0x1400602A0, "\x48\x8D\x81\x00\x10\x00\x00"sv, "\x48\x8D\x81\x00\x40\x00\x00"sv },
		{ 0x1400608DB, "\xB8\x00\x10\x00\x00"sv, "\xB8\x00\x40\x00\x00"sv },
		{ 0x1400608F8, "\x48\x8D\x81\x00\x04\x00\x00"sv, "\x48\x8D\x81\x00\x10\x00\x00"sv },
		{ 0x14006090B, "\x48\x8D\x81\x00\x08\x00\x00"sv, "\x48\x8D\x81\x00\x20\x00\x00"sv },
		{ 0x14006091E, "\x48\x8D\x81\x00\x0C\x00\x00"sv, "\x48\x8D\x81\x00\x30\x00\x00"sv },
		{ 0x140060937, "\x48\x8D\x81\x00\x10\x00\x00"sv, "\x48\x8D\x81\x00\x40\x00\x00"sv },
		{ 0x14006096F, "\x48\x8D\x81\x00\x08\x00\x00"sv, "\x48\x8D\x81\x00\x20\x00\x00"sv },
		{ 0x140060982, "\x48\x8D\x81\x00\x10\x00\x00"sv, "\x48\x8D\x81\x00\x40\x00\x00"sv },
		{ 0x140060995, "\x48\x8D\x81\x00\x18\x00\x00"sv, "\x48\x8D\x81\x00\x60\x00\x00"sv },
		{ 0x1400609AE, "\x48\x8D\x81\x00\x20\x00\x00"sv, "\x48\x8D\x81\x00\x80\x00\x00"sv },
		{ 0x140060F3E, "\xB8\x00\x10\x00\x00"sv, "\xB8\x00\x40\x00\x00"sv },
		{ 0x140060F77, "\x48\x8D\x81\x00\x04\x00\x00"sv, "\x48\x8D\x81\x00\x10\x00\x00"sv },
		{ 0x140060F9B, "\x48\x8D\x81\x00\x08\x00\x00"sv, "\x48\x8D\x81\x00\x20\x00\x00"sv },
		{ 0x140060FB9, "\x48\x8D\x81\x00\x0C\x00\x00"sv, "\x48\x8D\x81\x00\x30\x00\x00"sv },
		{ 0x140060FD2, "\x48\x8D\x81\x00\x10\x00\x00"sv, "\x48\x8D\x81\x00\x40\x00\x00"sv },
		{ 0x140061511, "\xB9\x00\x40\x00\x00"sv, "\xB9\x00\x00\x01\x00"sv },
		{ 0x14006151D, "\xB8\x00\x20\x00\x00"sv, "\xB8\x00\x80\x00\x00"sv },
		{ 0x14006155A, "\x48\x8D\x81\x00\x08\x00\x00"sv, "\x48\x8D\x81\x00\x20\x00\x00"sv },
		{ 0x140061578, "\x48\x8D\x81\x00\x10\x00\x00"sv, "\x48\x8D\x81\x00\x40\x00\x00"sv },
		{ 0x14006158B, "\x48\x8D\x81\x00\x18\x00\x00"sv, "\x48\x8D\x81\x00\x60\x00\x00"sv },
		{ 0x140061592, "\x48\x81\xC1\x00\x20\x00\x00"sv, "\x48\x81\xC1\x00\x80\x00\x00"sv },
		{ 0x14006165C, "\x48\x8D\x81\x00\x08\x00\x00"sv, "\x48\x8D\x81\x00\x20\x00\x00"sv },
		{ 0x140061689, "\x48\x8D\x81\x00\x10\x00\x00"sv, "\x48\x8D\x81\x00\x40\x00\x00"sv },
		{ 0x1400616A5, "\x48\x8D\x81\x00\x18\x00\x00"sv, "\x48\x8D\x81\x00\x60\x00\x00"sv },
		{ 0x1400616C1, "\x48\x8D\x81\x00\x20\x00\x00"sv, "\x48\x8D\x81\x00\x80\x00\x00"sv },
	};

	static const KeyStubSite staticModelStreamStubSites[] =
	{
		{ 0x140060959, "\xF0\x44\x0F\xC1\x82\x64\x19\x04\x00\x41\x8B\xC0"sv, SunShadowStreamStub },
	};

	static Utils::Hook staticModelStreamHooks[std::size(staticModelStreamStubSites)];

	static bool AreStreamSitesIntact()
	{
		for (const auto& site : staticModelStreamSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, reinterpret_cast<const std::uint8_t*>(site.stock.data()), site.stock.size()))
			{
				return false;
			}
		}

		for (const auto& site : staticModelStreamStubSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, reinterpret_cast<const std::uint8_t*>(site.stock.data()), site.stock.size()))
			{
				return false;
			}
		}

		for (const auto lea : staticModelStreamLeas)
		{
			if (!Utils::Hook::MatchesBytes(lea, staticModelStreamLea, sizeof(staticModelStreamLea)))
			{
				return false;
			}
		}

		return true;
	}

	static bool RaiseStaticModelStream()
	{
		if (!AreStreamSitesIntact())
		{
			Logger::Error("assethandler: the static model stream's sites do not read as expected, it stays 0x1B000 bytes\n");
			return false;
		}

		auto* const streams = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(s_backEndData, backEndDataSize + staticModelStreamSize));

		if (!streams)
		{
			Logger::Error("assethandler: no room near the image for the static model stream, it stays 0x1B000 bytes\n");
			return false;
		}

		const auto displacement = reinterpret_cast<std::int64_t>(streams) - static_cast<std::int64_t>(Utils::Hook::Rebase(s_backEndData));

		if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
		{
			Logger::Error("assethandler: the static model stream's block is out of reach, it stays 0x1B000 bytes\n");
			return false;
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(staticModelStreamStubSites); ++i)
		{
			const auto& site = staticModelStreamStubSites[i];
			isSeated = staticModelStreamHooks[i].Initialize(site.address, site.stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		std::uintptr_t first = std::numeric_limits<std::uintptr_t>::max();
		std::uintptr_t last = 0;

		for (const auto& site : staticModelStreamSites)
		{
			first = std::min(first, site.address);
			last = std::max(last, site.address + site.widened.size());
		}

		for (const auto& site : staticModelStreamStubSites)
		{
			first = std::min(first, site.address);
			last = std::max(last, site.address + site.stock.size());
		}

		for (const auto lea : staticModelStreamLeas)
		{
			first = std::min(first, lea);
			last = std::max(last, lea + sizeof(staticModelStreamLea));
		}

		auto* const code = reinterpret_cast<std::uint8_t*>(Utils::Hook::Rebase(first));
		const std::size_t length = last - first;
		DWORD oldProtect;

		if (!isSeated || !VirtualProtect(code, length, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			for (auto& hook : staticModelStreamHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("assethandler: could not seat every static model stream site, it stays 0x1B000 bytes\n");
			return false;
		}

		for (const auto& site : staticModelStreamSites)
		{
			std::memcpy(code + (site.address - first), site.widened.data(), site.widened.size());
		}

		for (const auto lea : staticModelStreamLeas)
		{
			const auto disp = static_cast<std::int32_t>(displacement);
			std::memcpy(code + (lea - first) + leaDispOffset, &disp, sizeof(disp));
		}

		for (const auto& site : staticModelStreamStubSites)
		{
			const std::size_t callLength = 5;
			std::memset(code + (site.address - first) + callLength, 0x90, site.stock.size() - callLength);
		}

		VirtualProtect(code, length, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), code, length);

		for (auto& hook : staticModelStreamHooks)
		{
			hook.Quick();
		}

		return true;
	}

	constexpr std::uintptr_t DB_FindXAssetHeader = 0x14012D6D0;

	struct FindSite
	{
		unsigned int type;
		std::uintptr_t site;
		bool isJump;
	};

	static const FindSite findSites[] =
	{
		{ 0x25, 0x140280D61, false },
		{ 0x05, 0x14001A635, true },
		{ 0x1A, 0x14026C701, false },
		{ 0x04, 0x1402BA708, true },
		{ 0x1E, 0x14014D4C8, true },
		{ 0x28, 0x14024FDE8, true },
	};

	static Utils::Hook findHooks[std::size(findSites)];
	static bool isFindInstalled = false;

	bool AssetHandler::IsInstalled()
	{
		return isInstalled;
	}

	void AssetHandler::OnLoad(const std::function<LoadCallback>& callback)
	{
		loadCallbacks.push_back(callback);
	}

	void AssetHandler::OnLoad(const unsigned int type, const std::function<LoadCallback>& callback)
	{
		typeLoadCallbacks[type].push_back(callback);
	}

	bool AssetHandler::OnFind(const unsigned int type, const std::function<FindCallback>& callback)
	{
		const bool hasSite = std::ranges::any_of(findSites, [type](const FindSite& findSite)
		{
			return findSite.type == type;
		});

		if (!hasSite || !isFindInstalled)
		{
			Logger::Error("assethandler: no find hook answers asset type 0x{:X}\n", type);
			return false;
		}

		findCallbacks[type].push_back(callback);
		return true;
	}

	void* AssetHandler::DB_FindXAssetHeader_Hook(unsigned int type, const char* name)
	{
		const auto callbacks = findCallbacks.find(type);

		if (callbacks != findCallbacks.end() && name)
		{
			for (const auto& callback : callbacks->second)
			{
				void* const header = callback(type, name);

				if (header)
				{
					return header;
				}
			}
		}

		if (shouldSearchTempAssets && name)
		{
			const auto temporary = FindTemporaryAsset(static_cast<Game::XAssetType>(type), name);

			if (temporary.data)
			{
				return temporary.data;
			}
		}

		return reinterpret_cast<void*(*)(unsigned int, const char*)>(Utils::Hook::Rebase(DB_FindXAssetHeader))(type, name);
	}

	void AssetHandler::RegisterInterface(IAsset* asset)
	{
		if (!asset)
		{
			return;
		}

		std::unique_ptr<IAsset> owned(asset);
		const auto type = owned->GetType();

		if (type >= Game::ASSET_TYPE_COUNT)
		{
			return;
		}

		if (assetInterfaces.contains(type))
		{
			Logger::Print("Duplicate asset interface: {}\n", Game::DB_GetXAssetTypeName(type));
		}
		else
		{
			Logger::Print("Asset interface registered: {}\n", Game::DB_GetXAssetTypeName(type));
		}

		assetInterfaces[type] = std::move(owned);
	}

	void AssetHandler::ClearTemporaryAssets()
	{
		for (auto& pool : temporaryAssets)
		{
			pool.clear();
		}
	}

	void AssetHandler::StoreTemporaryAsset(Game::XAssetType type, Game::XAssetHeader asset)
	{
		const Game::XAsset entry{ static_cast<unsigned int>(type), asset.data };
		temporaryAssets[type][Game::DB_GetXAssetName(&entry)] = asset;
	}

	void AssetHandler::RemoveTemporaryAsset(Game::XAssetType type, const char* name)
	{
		if (type < 0 || type >= Game::ASSET_TYPE_COUNT || !name)
		{
			return;
		}

		temporaryAssets[type].erase(name);
	}

	Game::XAssetHeader AssetHandler::FindTemporaryAsset(Game::XAssetType type, const char* filename)
	{
		Game::XAssetHeader header{ nullptr };

		if (type < 0 || type >= Game::ASSET_TYPE_COUNT || !filename)
		{
			return header;
		}

		const auto& pool = temporaryAssets[type];
		const auto entry = pool.find(filename);

		if (entry != pool.end())
		{
			header = entry->second;
		}

		return header;
	}

	void AssetHandler::ExposeTemporaryAssets(bool expose)
	{
		shouldSearchTempAssets = expose;
	}

	void AssetHandler::ZoneSave(Game::XAsset asset, ZoneBuilder::Zone* builder)
	{
		const auto assetInterface = assetInterfaces.find(static_cast<Game::XAssetType>(asset.type));

		if (assetInterface == assetInterfaces.end())
		{
			Logger::Fatal("No interface for type '{}'!", Game::DB_GetXAssetTypeName(asset.type));
		}

		assetInterface->second->Save({ asset.header }, builder);
	}

	void AssetHandler::ZoneMark(Game::XAsset asset, ZoneBuilder::Zone* builder)
	{
		const auto assetInterface = assetInterfaces.find(static_cast<Game::XAssetType>(asset.type));

		if (assetInterface == assetInterfaces.end())
		{
			Logger::Fatal("No interface for type '{}'!", Game::DB_GetXAssetTypeName(asset.type));
		}

		assetInterface->second->Mark({ asset.header }, builder);
	}

	void AssetHandler::DumpAsset(Game::XAsset asset)
	{
		const auto assetInterface = assetInterfaces.find(static_cast<Game::XAssetType>(asset.type));

		if (assetInterface == assetInterfaces.end())
		{
			Logger::Error("assethandler: no interface dumps type '{}'\n", Game::DB_GetXAssetTypeName(asset.type));
			return;
		}

		if (ZoneBuilder::IsDumpingZone())
		{
			const std::string key = std::format("{}/{}", asset.type, Game::DB_GetXAssetName(&asset));

			if (!dumpedAssets.insert(key).second)
			{
				return;
			}
		}

		assetInterface->second->Dump({ asset.header });
	}

	void AssetHandler::ForgetDumpedAssets()
	{
		dumpedAssets.clear();
	}

	bool AssetHandler::CanDump(Game::XAssetType type)
	{
		const auto assetInterface = assetInterfaces.find(type);

		if (assetInterface == assetInterfaces.end())
		{
			return false;
		}

		return assetInterface->second->HasDump();
	}

	Game::XAssetHeader AssetHandler::FindAssetForZone(Game::XAssetType type, const std::string& filename, ZoneBuilder::Zone* builder, bool isSubAsset)
	{
		const ZoneBuilder::Zone::AssetRecursionMarker marker(builder);

		Game::XAssetHeader header{ nullptr };

		if (type >= Game::ASSET_TYPE_COUNT)
		{
			return header;
		}

		header = FindTemporaryAsset(type, filename.data());

		if (header.data)
		{
			return header;
		}

		const auto assetInterface = assetInterfaces.find(type);

		if (assetInterface != assetInterfaces.end())
		{
			assetInterface->second->Load(&header, filename, builder);

			if (header.data)
			{
				StoreTemporaryAsset(type, header);
			}
		}

		if (!header.data && isSubAsset)
		{
			header = ZoneBuilder::GetEmptyAssetIfCommon(type, filename, builder);
		}

		if (!header.data)
		{
			header = FindLoadedAsset(type, filename.data());

			if (header.data)
			{
				StoreTemporaryAsset(type, header);
			}
		}

		return header;
	}

	Game::XAssetHeader AssetHandler::FindOriginalAsset(Game::XAssetType type, const char* filename)
	{
		return { Game::DB_FindXAssetHeader(type, filename) };
	}

	Game::XAssetHeader AssetHandler::FindLoadedAsset(Game::XAssetType type, const char* name)
	{
		if (!name || !Game::DB_FindXAssetEntry(type, name))
		{
			return { nullptr };
		}

		if (Game::DB_IsXAssetDefault(type, name))
		{
			return { nullptr };
		}

		return FindOriginalAsset(type, name);
	}

	constexpr std::uintptr_t DB_AddXAsset_CreateDefaultEntryCall = 0x14012E7FA;
	constexpr std::uintptr_t DB_CreateDefaultEntry = 0x14012D000;

	using EmptyAssetList = std::vector<std::pair<unsigned int, std::string>>;

	static Utils::Concurrency::Container<EmptyAssetList> emptyAssets;
	static Utils::Hook createDefaultEntryHook;

	static void* DB_CreateDefaultEntry_Hook(const unsigned int type, const char* name)
	{
		auto* const entry = static_cast<Game::XAsset*>(reinterpret_cast<void*(*)(unsigned int, const char*)>(Utils::Hook::Rebase(DB_CreateDefaultEntry))(type, name));

		if (type == Game::ASSET_TYPE_XMODEL_SURFS && ModelSurfs::TryLoadMissing(static_cast<Game::XModelSurfs*>(entry->header.data), name))
		{
			return entry;
		}

		emptyAssets.Access([type, name](EmptyAssetList& assets)
		{
			assets.emplace_back(type, name);
		});

		return entry;
	}

	static void ReportEmptyAssets()
	{
		if (!FastFiles::Ready())
		{
			return;
		}

		emptyAssets.Access([](EmptyAssetList& assets)
		{
			for (const auto& [type, name] : assets)
			{
				Logger::Warning("Could not load {} \"{}\".\n", Game::g_assetNames[type], name);
			}

			assets.clear();
		});
	}

	constexpr std::size_t materialSortKey = 0x9;
	constexpr std::size_t imageCategory = 0xA;
	constexpr std::size_t gfxWorldSortKeyEffectDecal = 52;
	constexpr std::size_t gfxWorldSortKeyEffectAuto = 56;
	constexpr std::size_t gfxWorldSortKeyDistortion = 60;
	constexpr std::size_t vehicleTurretWeapon = 456;
	constexpr std::uint32_t iw4xFirstVersion = 316;

	void AssetHandler::ModifyAsset(const unsigned int type, void* asset, const std::string& name)
	{
		auto* const bytes = static_cast<std::uint8_t*>(asset);

		if (*Game::com_developer && (*Game::com_developer)->current.enabled)
		{
			if (type == Game::ASSET_TYPE_IMAGE && name[0] != ',' && bytes[imageCategory] == Game::IMG_CATEGORY_UNKNOWN)
			{
				Logger::Warning("Image {} has wrong category IMG_CATEGORY_UNKNOWN, this is an IMPORTANT ISSUE that should be fixed!\n", name);
			}
		}

		if (type == Game::ASSET_TYPE_GFXWORLD && Zones::Version() >= iw4xFirstVersion)
		{
			const std::uint32_t effectDecal = 39;
			const std::uint32_t effectAuto = 48;
			const std::uint32_t distortion = 43;
			std::memcpy(bytes + gfxWorldSortKeyEffectDecal, &effectDecal, sizeof(effectDecal));
			std::memcpy(bytes + gfxWorldSortKeyEffectAuto, &effectAuto, sizeof(effectAuto));
			std::memcpy(bytes + gfxWorldSortKeyDistortion, &distortion, sizeof(distortion));
			return;
		}

		if (type == Game::ASSET_TYPE_VEHICLE && Zones::Version() >= iw4xFirstVersion)
		{
			std::memset(bytes + vehicleTurretWeapon, 0, sizeof(void*));
			return;
		}

		if (type != Game::ASSET_TYPE_MATERIAL)
		{
			return;
		}

		if ((name == "gfx_distortion_knife_trail" || name == "gfx_distortion_heat_far" || name == "gfx_distortion_ring_light" || name == "gfx_distortion_heat") && bytes[materialSortKey] >= 43)
		{
			bytes[materialSortKey] = 43;
		}

		if (name == "wc/codo_ui_viewer_black_decal3" || name == "wc/codo_ui_viewer_black_decal2" || name == "wc/hint_arrows01" || name == "wc/hint_arrows02")
		{
			bytes[materialSortKey] = 0xE;
		}
	}

	void* AssetHandler::DB_AddXAsset_Hook(unsigned int type, void** header)
	{
		const Game::XAsset asset{ type, *header };
		const char* const assetName = reinterpret_cast<const char*(*)(const Game::XAsset*)>(
			Utils::Hook::Rebase(DB_GetXAssetName))(&asset);

		bool restrict = true;

		if (assetName)
		{
			const std::string name = assetName;
			restrict = false;

			emptyAssets.Access([type, &name](EmptyAssetList& assets)
			{
				std::erase_if(assets, [type, &name](const auto& empty)
				{
					return empty.first == type && empty.second == name;
				});
			});

			static const bool shouldPrintEntries = Flags::HasFlag("entries");

			if (shouldPrintEntries)
			{
				OutputDebugStringA(Utils::String::VA("%s: %d: %s\n", FastFiles::Current().data(), type, name.data()));
			}

			for (const auto& callback : loadCallbacks)
			{
				callback(type, *header, name, &restrict);
			}

			if (const auto typeCallbacks = typeLoadCallbacks.find(type); typeCallbacks != typeLoadCallbacks.end())
			{
				for (const auto& callback : typeCallbacks->second)
				{
					callback(type, *header, name, &restrict);
				}
			}

			if (!restrict)
			{
				ModifyAsset(type, *header, name);
			}
		}

		if (restrict)
		{
			thread_local Game::XAsset restricted;
			restricted = { type, *header };
			return &restricted;
		}

		return reinterpret_cast<void*(*)(unsigned int, void**)>(Utils::Hook::Rebase(DB_AddXAsset))(type, header);
	}

	void AssetHandler::InstallFindHooks()
	{
		for (const FindSite& findSite : findSites)
		{
			if (!Utils::Hook::BranchesTo(findSite.site, DB_FindXAssetHeader, findSite.isJump))
			{
				Logger::Error("assethandler: 0x{:X} does not branch to DB_FindXAssetHeader, no lookup can be answered\n", findSite.site);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(findSites); ++i)
		{
			isSeated = findHooks[i].Initialize(findSites[i].site, reinterpret_cast<void*>(DB_FindXAssetHeader_Hook), findSites[i].isJump)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : findHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("assethandler: could not seat every DB_FindXAssetHeader hook, no lookup can be answered\n");
			return;
		}

		for (auto& hook : findHooks)
		{
			hook.Quick();
		}

		isFindInstalled = true;
	}

	struct EntryPoolSite
	{
		std::uintptr_t instruction;
		std::uint8_t dispOffset;
		std::uint8_t length;
		std::uint32_t offset;
		bool isRipRelative;
	};

	constexpr std::uintptr_t g_assetEntryPool = 0x141521660;
	constexpr std::size_t assetEntrySize = 0x18;
	constexpr std::size_t engineAssetEntryCount = 37000;
	constexpr std::size_t narrowAssetEntryCount = 0x10000;
	constexpr std::size_t wideAssetEntryCount = 4 * engineAssetEntryCount;
	constexpr std::uintptr_t DB_InitThread_LinkCount = 0x14012E05B;
	constexpr std::uint32_t lastEntryNext = (engineAssetEntryCount - 1) * assetEntrySize;

	static const std::uint8_t linkCountMov[] = { 0xBA, 0x86, 0x90, 0x00, 0x00 };

	static const EntryPoolSite entryPoolSites[] =
	{
		{ 0x14012D123, 3, 7, 0x0, true },
		{ 0x14012D30E, 3, 7, 0x0, true },
		{ 0x14012D354, 3, 7, 0x0, true },
		{ 0x14012D385, 3, 7, 0x0, true },
		{ 0x14012D574, 3, 7, 0x0, false },
		{ 0x14012D5D3, 3, 7, 0x0, false },
		{ 0x14012D676, 3, 7, 0x0, false },
		{ 0x14012DA74, 5, 9, 0x11, false },
		{ 0x14012DA7F, 4, 8, 0x11, false },
		{ 0x14012DA87, 5, 9, 0x12, false },
		{ 0x14012DAB4, 3, 7, 0x0, false },
		{ 0x14012DB13, 3, 7, 0x0, false },
		{ 0x14012DC34, 3, 7, 0x0, true },
		{ 0x14012DF84, 3, 7, 0x0, false },
		{ 0x14012E054, 3, 7, 0x18, true },
		{ 0x14012E067, 3, 7, 0x30, true },
		{ 0x14012E088, 3, 7, lastEntryNext, true },
		{ 0x14012E224, 3, 7, 0x0, false },
		{ 0x14012E309, 3, 7, 0x0, true },
		{ 0x14012E47A, 3, 7, 0x0, true },
		{ 0x14012E51C, 3, 7, 0x0, true },
		{ 0x14012E78B, 3, 7, 0x0, true },
		{ 0x14012E7D5, 3, 7, 0x0, true },
		{ 0x14012E914, 3, 7, 0x0, true },
		{ 0x14012EA25, 3, 7, 0x0, true },
		{ 0x14012EE94, 4, 8, 0x0, false },
		{ 0x14012EF32, 4, 8, 0x8, false },
		{ 0x14012EF9D, 5, 9, 0x14, false },
		{ 0x14012F620, 3, 7, 0x0, true },
		{ 0x14012F86A, 3, 7, 0x0, false },
		{ 0x14012FDF8, 4, 8, 0x0, false },
		{ 0x14012FF3C, 4, 8, 0x0, false },
		{ 0x140130037, 4, 8, 0x0, false },
	};

	static bool IsEntryPoolSite(const EntryPoolSite& site)
	{
		const auto disp = static_cast<std::int64_t>(Utils::Hook::Get<std::int32_t>(site.instruction + site.dispOffset));
		const auto target = static_cast<std::int64_t>(g_assetEntryPool + site.offset);

		if (site.isRipRelative)
		{
			return static_cast<std::int64_t>(site.instruction + site.length) + disp == target;
		}

		return disp == target - static_cast<std::int64_t>(imageBase);
	}

	constexpr std::uintptr_t db_hashTable = 0x14150B520;
	constexpr std::size_t hashBucketCount = 37000;

	static std::uint8_t db_hashTableHigh[hashBucketCount * sizeof(std::uint16_t)];

	extern "C"
	{
		std::intptr_t db_hashTableHighDelta;
		std::uintptr_t db_hashTableRebased;
		std::uintptr_t g_assetEntryPoolMoved;

		void BucketRdiEcxStub();
		void HashRbxEcxStub();
		void BucketR14EcxStub();
		void OverrideRbxEaxStub();
		void OverrideWalkRbxRcxStub();
		void BucketR8EcxStub();
		void HashRdxEcxStub();
		void CreateDefaultLinkStub();
		void CreateDefaultHeadStub();
		void BucketRaxEsiStub();
		void HashRbxEsiStub();
		void AddHashLinkStub();
		void AddHeadStub();
		void AddOverrideCopyStub();
		void AddOverrideHeadStub();
		void AllocClearLinksStub();
		void BucketR13RaxEcxStub();
		void HashR12EcxStub();
		void LinkOverrideCopyStub();
		void LinkOverrideHeadStub();
		void BucketR13EaxStub();
		void OverrideR11RsiEaxStub();
		void HashRbxEaxStub();
		void UnloadBucketStub();
		void OverrideRdiEaxStub();
		void UnloadUnlinkHeadStub();
		void UnloadPromoteStub();
		void UnloadOverridesStub();
		void UnloadUnlinkOverrideStub();
		void OverrideSlotRsiStub();
		void UnloadHashSlotStub();
		void BucketRdiEaxStub();
		void LoadOverridePoolStub();
		void BucketRdxEaxStub();
		void ReleaseHashStub();
		void BucketRsiEdiStub();
		void ShutdownHashStub();
		void ShutdownClearBucketStub();
		void FreeHashPoolStub();
		void FreeBucketStub();
		void FreeUnlinkStub();
		void FreeHashSlotStub();
		void BucketR12EaxStub();
		void OverrideRbxR15EaxStub();
		void HashRdiEaxStub();
		void BucketR8EaxStub();
		void HashRcxEaxStub();
	}

	static const KeySite entryLinkSites[] =
	{
		{ 0x14012D5D0, "\x0F\xB7\xC0"sv, "\x89\xC0\x90"sv },
		{ 0x14012FDF1, "\x0F\xB7\xC0"sv, "\x89\xC0\x90"sv },
		{ 0x140130030, "\x0F\xB7\xC0"sv, "\x89\xC0\x90"sv },
		{ 0x14012DB10, "\x0F\xB7\xC0"sv, "\x89\xC0\x90"sv },
	};

	static const KeyStubSite entryLinkStubSites[] =
	{
		{ 0x14012D666, "\x0F\xB7\x8C\x47\x20\xB5\x50\x01\x85\xC9"sv, BucketRdiEcxStub },
		{ 0x14012D69D, "\x0F\xB7\x4B\x12\x85\xC9"sv, HashRbxEcxStub },
		{ 0x14012D560, "\x41\x0F\xB7\x8C\x46\x20\xB5\x50\x01\x85\xC9"sv, BucketR14EcxStub },
		{ 0x14012D59B, "\x0F\xB7\x4B\x12\x85\xC9"sv, HashRbxEcxStub },
		{ 0x14012D5C0, "\x0F\xB7\x43\x14\x66\x85\xC0"sv, OverrideRbxEaxStub },
		{ 0x14012D5DE, "\x0F\xB7\x44\xCB\x14\x48\x8D\x1C\xCB\x66\x85\xC0"sv, OverrideWalkRbxRcxStub },
		{ 0x14012E20E, "\x0F\xB7\x8C\x47\x20\xB5\x50\x01\x85\xC9"sv, BucketRdiEcxStub },
		{ 0x14012E24B, "\x0F\xB7\x4B\x12\x85\xC9"sv, HashRbxEcxStub },
		{ 0x14012DF6C, "\x41\x0F\xB7\x8C\x40\x20\xB5\x50\x01\x85\xC9"sv, BucketR8EcxStub },
		{ 0x14012DF9A, "\x0F\xB7\x4A\x12\x85\xC9"sv, HashRdxEcxStub },
		{ 0x14012D11A, "\x43\x0F\xB7\x04\x30\x66\x89\x45\x12"sv, CreateDefaultLinkStub },
		{ 0x14012D14B, "\x66\x43\x89\x14\x30"sv, CreateDefaultHeadStub },
		{ 0x14012E7AA, "\x0F\xB7\x30\x85\xF6"sv, BucketRaxEsiStub },
		{ 0x14012E7DC, "\x0F\xB7\x73\x12\x85\xF6"sv, HashRbxEsiStub },
		{ 0x14012E90B, "\x41\x0F\xB7\x00\x66\x41\x89\x46\x12"sv, AddHashLinkStub },
		{ 0x14012E93E, "\x66\x41\x89\x10\x49\x8B\x04\x24"sv, AddHeadStub },
		{ 0x14012EA1C, "\x0F\xB7\x43\x14\x66\x41\x89\x46\x14"sv, AddOverrideCopyStub },
		{ 0x14012EA47, "\x48\x03\xD0\x66\x89\x53\x14"sv, AddOverrideHeadStub },
		{ 0x14012CEB9, "\x89\x47\x12\x48\x8B\xC7"sv, AllocClearLinksStub },
		{ 0x14012E310, "\x41\x0F\xB7\x4C\x45\x00\x85\xC9"sv, BucketR13RaxEcxStub },
		{ 0x14012E345, "\x41\x0F\xB7\x4C\x24\x12\x85\xC9"sv, HashR12EcxStub },
		{ 0x14012E481, "\x41\x0F\xB7\x44\x24\x14\x66\x89\x43\x14"sv, LinkOverrideCopyStub },
		{ 0x14012E4A9, "\x66\x41\x89\x54\x24\x14"sv, LinkOverrideHeadStub },
		{ 0x14012E540, "\x41\x0F\xB7\x45\x00\x85\xC0"sv, BucketR13EaxStub },
		{ 0x14012E5CC, "\x0F\xB7\x43\x14\x85\xC0"sv, OverrideRbxEaxStub },
		{ 0x14012E65D, "\x41\x0F\xB7\x44\x33\x14\x85\xC0"sv, OverrideR11RsiEaxStub },
		{ 0x14012E66B, "\x0F\xB7\x43\x12\x85\xC0"sv, HashRbxEaxStub },
		{ 0x14012FDE0, "\x41\x0F\xB7\x04\x24\x49\x8B\xDC\x66\x85\xC0"sv, UnloadBucketStub },
		{ 0x14012FE10, "\x0F\xB7\x47\x14\x66\x85\xC0"sv, OverrideRdiEaxStub },
		{ 0x14012FE38, "\x0F\xB7\x47\x12\x66\x89\x03"sv, UnloadUnlinkHeadStub },
		{ 0x14012FF4E, "\x0F\xB7\x43\x14\x66\x89\x47\x14"sv, UnloadPromoteStub },
		{ 0x14013001D, "\x0F\xB7\x47\x14\x48\x8D\x77\x14\x66\x85\xC0"sv, UnloadOverridesStub },
		{ 0x140130067, "\x0F\xB7\x43\x14\x66\x89\x06"sv, UnloadUnlinkOverrideStub },
		{ 0x140130096, "\x0F\xB7\x06\x66\x85\xC0"sv, OverrideSlotRsiStub },
		{ 0x1401300A7, "\x0F\xB7\x03\x66\x85\xC0"sv, UnloadHashSlotStub },
		{ 0x14012EE70, "\x0F\xB7\x07\x85\xC0"sv, BucketRdiEaxStub },
		{ 0x14012EF0D, "\x0F\xB7\x43\x14\x85\xC0"sv, OverrideRbxEaxStub },
		{ 0x14012EF9D, "\x43\x0F\xB7\x84\x23\x74\x16\x52\x01\x85\xC0"sv, LoadOverridePoolStub },
		{ 0x14012EFAE, "\x0F\xB7\x43\x12\x85\xC0"sv, HashRbxEaxStub },
		{ 0x14012F630, "\x0F\xB7\x02\x85\xC0"sv, BucketRdxEaxStub },
		{ 0x14012F64A, "\x41\x0F\xB7\x44\xC1\x12\x85\xC0"sv, ReleaseHashStub },
		{ 0x14012F855, "\x0F\xB7\x3E\x85\xFF"sv, BucketRsiEdiStub },
		{ 0x14012F879, "\x48\x8B\x53\x08\x0F\xB7\x7B\x12"sv, ShutdownHashStub },
		{ 0x14012F8A6, "\x66\x44\x89\x3E\x48\x83\xC6\x02"sv, ShutdownClearBucketStub },
		{ 0x14012DA60, "\x0F\xB7\x02\x85\xC0"sv, BucketRdxEaxStub },
		{ 0x14012DA87, "\x41\x0F\xB7\x84\xCF\x72\x16\x52\x01\x85\xC0"sv, FreeHashPoolStub },
		{ 0x14012DAA4, "\x0F\xB7\x07\x85\xC0"sv, BucketRdiEaxStub },
		{ 0x14012DAE3, "\x0F\xB7\x43\x12\x85\xC0"sv, HashRbxEaxStub },
		{ 0x14012DB00, "\x41\x0F\xB7\x06\x49\x8B\xFE\x66\x85\xC0"sv, FreeBucketStub },
		{ 0x14012DB6E, "\x66\x89\x07\x48\x63\x03"sv, FreeUnlinkStub },
		{ 0x14012DB99, "\x0F\xB7\x07\x66\x85\xC0"sv, FreeHashSlotStub },
		{ 0x14012D321, "\x41\x0F\xB7\x04\x24\x85\xC0"sv, BucketR12EaxStub },
		{ 0x14012D34C, "\x0F\xB7\x47\x14\x85\xC0"sv, OverrideRdiEaxStub },
		{ 0x14012D376, "\x42\x0F\xB7\x44\x3B\x14\x85\xC0"sv, OverrideRbxR15EaxStub },
		{ 0x14012D38C, "\x0F\xB7\x47\x12\x85\xC0"sv, HashRdiEaxStub },
		{ 0x14012DC40, "\x41\x0F\xB7\x00\x85\xC0"sv, BucketR8EaxStub },
		{ 0x14012DC70, "\x0F\xB7\x41\x12\x85\xC0"sv, HashRcxEaxStub },
	};

	static Utils::Hook entryLinkHooks[std::size(entryLinkStubSites)];

	static bool AreSitesIntact(std::span<const KeySite> sites, std::span<const KeyStubSite> stubSites)
	{
		for (const auto& site : sites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, reinterpret_cast<const std::uint8_t*>(site.stock.data()), site.stock.size()))
			{
				return false;
			}
		}

		for (const auto& site : stubSites)
		{
			if (!Utils::Hook::MatchesBytes(site.address, reinterpret_cast<const std::uint8_t*>(site.stock.data()), site.stock.size()))
			{
				return false;
			}
		}

		return true;
	}

	static bool TrySeatSites(std::span<const KeySite> sites, std::span<const KeyStubSite> stubSites, std::span<Utils::Hook> hooks)
	{
		bool isSeated = true;

		for (std::size_t i = 0; i < stubSites.size(); ++i)
		{
			isSeated = hooks[i].Initialize(stubSites[i].address, stubSites[i].stub, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		std::uintptr_t first = std::numeric_limits<std::uintptr_t>::max();
		std::uintptr_t last = 0;

		for (const auto& site : sites)
		{
			first = std::min(first, site.address);
			last = std::max(last, site.address + site.widened.size());
		}

		for (const auto& site : stubSites)
		{
			first = std::min(first, site.address);
			last = std::max(last, site.address + site.stock.size());
		}

		auto* const code = reinterpret_cast<std::uint8_t*>(Utils::Hook::Rebase(first));
		const std::size_t length = last - first;
		DWORD oldProtect;

		if (!isSeated || !VirtualProtect(code, length, PAGE_EXECUTE_READWRITE, &oldProtect))
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			return false;
		}

		for (const auto& site : sites)
		{
			std::memcpy(code + (site.address - first), site.widened.data(), site.widened.size());
		}

		for (const auto& site : stubSites)
		{
			const std::size_t callLength = 5;
			std::memset(code + (site.address - first) + callLength, 0x90, site.stock.size() - callLength);
		}

		VirtualProtect(code, length, oldProtect, &oldProtect);
		FlushInstructionCache(GetCurrentProcess(), code, length);

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		return true;
	}

	static bool IsInsideLinkStub(const std::uintptr_t instruction)
	{
		for (const auto& site : entryLinkStubSites)
		{
			if (instruction >= site.address && instruction < site.address + site.stock.size())
			{
				return true;
			}
		}

		return false;
	}

	static void ReallocateEntryPool()
	{
		for (const auto& site : entryPoolSites)
		{
			if (!IsEntryPoolSite(site))
			{
				Logger::Error("assethandler: 0x{:X} does not reach the asset entry pool, it keeps its 37000 entries\n", site.instruction);
				return;
			}
		}

		if (!Utils::Hook::MatchesBytes(DB_InitThread_LinkCount, linkCountMov, sizeof(linkCountMov)))
		{
			Logger::Error("assethandler: DB_InitThread's entry count does not read as expected, the pool keeps its 37000 entries\n");
			return;
		}

		const bool canWidenLinks = AreSitesIntact(entryLinkSites, entryLinkStubSites);
		std::size_t entryCount = narrowAssetEntryCount;

		if (canWidenLinks)
		{
			entryCount = wideAssetEntryCount;
		}
		else
		{
			Logger::Error("assethandler: the asset entry links do not read as expected, they stay 16 bits and the pool holds 65536 entries\n");
		}

		auto* const pool = static_cast<std::uint8_t*>(Utils::Hook::AllocateDataNear(DB_InitThread_LinkCount, entryCount * assetEntrySize));

		if (!pool)
		{
			Logger::Error("assethandler: no room beside the image for the asset entry pool, it keeps its 37000 entries\n");
			return;
		}

		std::vector<std::int32_t> displacements;

		for (const auto& site : entryPoolSites)
		{
			std::uint32_t offset = site.offset;

			if (offset == lastEntryNext)
			{
				offset = static_cast<std::uint32_t>((entryCount - 1) * assetEntrySize);
			}

			const auto target = reinterpret_cast<std::int64_t>(pool + offset);
			std::int64_t from = static_cast<std::int64_t>(Utils::Hook::Rebase(imageBase));

			if (site.isRipRelative)
			{
				from = static_cast<std::int64_t>(Utils::Hook::Rebase(site.instruction) + site.length);
			}

			const auto distance = target - from;

			if (distance < INT32_MIN || distance > INT32_MAX)
			{
				Logger::Error("assethandler: the new asset entry pool is out of reach of 0x{:X}, it keeps its 37000 entries\n", site.instruction);
				return;
			}

			displacements.push_back(static_cast<std::int32_t>(distance));
		}

		if (canWidenLinks)
		{
			db_hashTableRebased = Utils::Hook::Rebase(db_hashTable);
			db_hashTableHighDelta = reinterpret_cast<std::intptr_t>(db_hashTableHigh) - static_cast<std::intptr_t>(db_hashTableRebased);
			g_assetEntryPoolMoved = reinterpret_cast<std::uintptr_t>(pool);

			if (!TrySeatSites(entryLinkSites, entryLinkStubSites, entryLinkHooks))
			{
				Logger::Error("assethandler: could not seat every asset entry link site, the pool keeps its 37000 entries\n");
				return;
			}
		}

		for (std::size_t i = 0; i < std::size(entryPoolSites); ++i)
		{
			const auto& site = entryPoolSites[i];

			if (canWidenLinks && IsInsideLinkStub(site.instruction))
			{
				continue;
			}

			Utils::Hook::Set<std::int32_t>(site.instruction + site.dispOffset, displacements[i]);
		}

		Utils::Hook::Set<std::uint32_t>(DB_InitThread_LinkCount + 1, static_cast<std::uint32_t>(entryCount - 2));
	}

	constexpr std::size_t xmodelNumLods = 0x141;

	static Dvar::Var r_noVoid;

	AssetHandler::AssetHandler()
	{
		ReallocateEntryPool();

		Events::OnDvarInit([]
		{
			r_noVoid = Dvar::Register("r_noVoid", false, Game::DVAR_ARCHIVE, "Disable void model (red fx)");
		});

		OnLoad([](const unsigned int type, void* asset, const std::string& name, bool*)
		{
			if (r_noVoid.IsValid() && r_noVoid.Get<bool>() && type == Game::ASSET_TYPE_XMODEL && name == "void")
			{
				static_cast<std::uint8_t*>(asset)[xmodelNumLods] = 0;
			}
		});

		if (!Utils::Hook::BranchesTo(DB_AddXAsset_CreateDefaultEntryCall, DB_CreateDefaultEntry, HOOK_CALL)
			|| !createDefaultEntryHook.Initialize(DB_AddXAsset_CreateDefaultEntryCall, reinterpret_cast<void*>(DB_CreateDefaultEntry_Hook), HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("assethandler: could not hook DB_AddXAsset's DB_CreateDefaultEntry, missing assets are not reported\n");
		}
		else
		{
			createDefaultEntryHook.Quick();
			Scheduler::Loop(ReportEmptyAssets, Scheduler::Pipeline::MAIN);
		}

		copyInfo = reinterpret_cast<const Game::XAsset* const*>(Utils::Hook::Rebase(g_copyInfo));
		RaiseCopyInfo();

		const bool isDelayedImageListSafe = Utils::Hook::BranchesTo(DB_LoadXFile_LoadDelayedImagesCall, DB_LoadDelayedImages, HOOK_CALL)
			&& loadDelayedImagesHook.Initialize(DB_LoadXFile_LoadDelayedImagesCall, reinterpret_cast<void*>(DB_LoadDelayedImages_Hook), HOOK_CALL)
				->Install()->IsInstalled();

		if (isDelayedImageListSafe)
		{
			loadDelayedImagesHook.Quick();
		}
		else
		{
			loadDelayedImagesHook.Uninstall();
			Logger::Error("assethandler: could not move DB_LoadDelayedImages' list to the heap, the image pool stays 3584\n");
		}

		constexpr std::uint32_t pixelShaderPoolSize = 32384;
		const bool isLostDeviceListRaised = RaiseLostDeviceEnumList(pixelShaderPoolSize);

		if (isLostDeviceListRaised)
		{
			Game::ReallocateAssetPool(Game::ASSET_TYPE_PIXELSHADER, pixelShaderPoolSize);
			Game::ReallocateAssetPool(Game::ASSET_TYPE_VERTEXSHADER, 8192);
		}

		const bool isImageListBounded = SeatEnumHooks(R_ImageList_f_EnumCalls, imageListHooks, reinterpret_cast<void*>(EnumImagesBounded));

		if (!isImageListBounded)
		{
			Logger::Error("assethandler: could not bound imagelist's list, the image pool stays 3584\n");
		}

		if (isDelayedImageListSafe && isLostDeviceListRaised && isImageListBounded)
		{
			Game::ReallocateAssetPool(Game::ASSET_TYPE_IMAGE, 14336);
		}

		if (!SeatEnumHooks(techsetEnumCalls, remapTechsetHooks, reinterpret_cast<void*>(RemapTechniqueSets)))
		{
			Logger::Error("assethandler: could not move the techset remap lists to the heap, the techset pool stays 768\n");
		}
		else if (ZoneBuilder::IsEnabled())
		{
			Game::ReallocateAssetPool(Game::ASSET_TYPE_TECHNIQUE_SET, 0x2000);
		}
		else
		{
			Game::ReallocateAssetPool(Game::ASSET_TYPE_TECHNIQUE_SET, 3072);
		}

		const bool isMaterialSortRaised = RaiseMaterialPool();
		const bool isDrawSurfKeyWide = WidenDrawSurfKey();

		if (isDrawSurfKeyWide)
		{
			OnLoad(Game::ASSET_TYPE_GFXWORLD, CheckReflectionProbes);
		}

		if (isMaterialSortRaised && isDrawSurfKeyWide)
		{
			Game::g_poolSize[Game::ASSET_TYPE_MATERIAL] = materialCapacity;
		}

		RaiseStaticModelStream();

		Game::ReallocateAssetPool(Game::ASSET_TYPE_LOADED_SOUND, 8192);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_SOUND, 64000);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_SOUND_CURVE, 256);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_FX, 4096);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_LOCALIZE_ENTRY, 32800);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_XANIMPARTS, 32768);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_XMODEL, 16384);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_XMODEL_SURFS, 16384);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_PHYSPRESET, 100);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_PHYSCOLLMAP, 4096);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_RAWFILE, 4096);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_STRINGTABLE, 2048);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_IMPACT_FX, 16);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_LIGHT_DEF, 128);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_FONT, 64);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_MENULIST, 512);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_MENU, 2960);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_STRUCTURED_DATA_DEF, 96);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_TRACER, 128);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_VEHICLE, 512);
		Game::ReallocateAssetPool(Game::ASSET_TYPE_GAMEWORLD_SP, 1);

		if (ZoneBuilder::IsEnabled())
		{
			Game::ReallocateAssetPool(Game::ASSET_TYPE_VERTEXDECL, 0x400);
			Game::ReallocateAssetPool(Game::ASSET_TYPE_MAP_ENTS, 10);
			Game::ReallocateAssetPool(Game::ASSET_TYPE_LEADERBOARD, 500);

			RegisterInterface(new Assets::IWeapon());
			RegisterInterface(new Assets::ISndCurve());
			RegisterInterface(new Assets::ITracerDef());
			RegisterInterface(new Assets::ILoadedSound());
			RegisterInterface(new Assets::Isnd_alias_list_t());
			RegisterInterface(new Assets::IXModel());
			RegisterInterface(new Assets::IPhysPreset());
			RegisterInterface(new Assets::IXAnimParts());
			RegisterInterface(new Assets::IFxEffectDef());
			RegisterInterface(new Assets::IPhysCollmap());
			RegisterInterface(new Assets::IXModelSurfs());
			RegisterInterface(new Assets::IFxWorld());
			RegisterInterface(new Assets::IMapEnts());
			RegisterInterface(new Assets::IComWorld());
			RegisterInterface(new Assets::IGfxWorld());
			RegisterInterface(new Assets::IclipMap_t());
			RegisterInterface(new Assets::IGameWorldMp());
			RegisterInterface(new Assets::IGameWorldSp());
			RegisterInterface(new Assets::IFont_s());
			RegisterInterface(new Assets::IRawFile());
			RegisterInterface(new Assets::IGfxImage());
			RegisterInterface(new Assets::IMaterial());
			RegisterInterface(new Assets::IMenuList());
			RegisterInterface(new Assets::ImenuDef_t());
			RegisterInterface(new Assets::IGfxLightDef());
			RegisterInterface(new Assets::IStringTable());
			RegisterInterface(new Assets::ILocalizeEntry());
			RegisterInterface(new Assets::IMaterialPixelShader());
			RegisterInterface(new Assets::IMaterialTechniqueSet());
			RegisterInterface(new Assets::IMaterialVertexShader());
			RegisterInterface(new Assets::IStructuredDataDefSet());
			RegisterInterface(new Assets::IMaterialVertexDeclaration());
		}
		else
		{
			Game::ReallocateAssetPool(Game::ASSET_TYPE_VERTEXDECL, 256);
			Game::ReallocateAssetPool(Game::ASSET_TYPE_LEADERBOARD, 400);
		}

		InstallFindHooks();

		for (const std::uintptr_t site : DB_AddXAssetCalls)
		{
			if (!Utils::Hook::BranchesTo(site, DB_AddXAsset, HOOK_CALL))
			{
				Logger::Error("assethandler: 0x{:X} is not a call to DB_AddXAsset, no asset load is watched\n", site);
				return;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(DB_AddXAssetCalls); ++i)
		{
			isSeated = addXAssetHooks[i].Initialize(DB_AddXAssetCalls[i], reinterpret_cast<void*>(DB_AddXAsset_Hook), HOOK_CALL)
				->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : addXAssetHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("assethandler: could not seat every DB_AddXAsset hook, no asset load is watched\n");
			return;
		}

		for (auto& hook : addXAssetHooks)
		{
			hook.Quick();
		}

		isInstalled = true;
	}
}
