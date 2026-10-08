#include "STDInclude.hpp"

#include <Utils/Compression.hpp>

#include "ZoneBuilder.hpp"
#include "AssetHandler.hpp"
#include "Branding.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "FileSystem.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"

#include "AssetInterfaces/ILocalizeEntry.hpp"

namespace Components
{
	std::string ZoneBuilder::traceZone;
	std::vector<ZoneBuilder::NamedAsset> ZoneBuilder::traceAssets;
	std::string ZoneBuilder::dumpingZone;

	Dvar::Var ZoneBuilder::zb_sp_to_mp;

	constexpr std::uintptr_t Load_Texture = 0x140037B00;

	constexpr std::uintptr_t DB_ReleaseXAssetHandler = 0x140422180;
	constexpr std::uintptr_t j_Image_Release = 0x14012C5D0;

	constexpr std::uintptr_t Load_ClipMapAsset_SetInUse = 0x14013025C;

	constexpr std::uintptr_t DB_PostLoadXZone_OverrideTechniqueSetsCall = 0x14012F2ED;
	constexpr std::uintptr_t Material_OverrideTechniqueSets = 0x14003ABE0;

	static const std::uint8_t loadTextureEntry[] = { 0x40, 0x53, 0x57, 0x48, 0x83, 0xEC, 0x68 };
	static const std::uint8_t clipMapSetInUse[] = { 0xC7, 0x40, 0x08, 0x01, 0x00, 0x00, 0x00 };

	constexpr std::size_t imageLoadDefHeaderSize = 16;
	constexpr std::size_t imageLoadDefResourceSize = 12;
	constexpr std::size_t assetEntryZoneIndex = 16;

	constexpr int DB_ZONE_MOD = 0x10;
	constexpr int DB_ZONE_LOAD = 0x20;

	constexpr std::uint8_t XLANG_NONE = 0;

	constexpr std::uint32_t insertPointerMarker = 0xFFFFFFFE;

	static Utils::Hook loadTextureHook;

	AssertSize(Game::X86::XFile, 40);
	AssertSize(Game::X86::XAsset, 8);
	AssertSize(Game::X86::ScriptStringList, 8);
	AssertSize(Game::X86::XAssetList, 16);

	ZoneBuilder::Zone::Zone(const std::string& name, const std::string& sourceName, const std::string& destinationPath) :
		indexStart(0),
		externalSize(0),
		buffer(0xC800000),
		zoneName(name),
		destination(destinationPath),
		dataMap("zone_source/" + sourceName + ".csv"),
		branding{},
		assetDepth(0)
	{
	}

	ZoneBuilder::Zone::Zone(const std::string& name) : Zone(name, name, std::format("zonebuilder_out/{}.ff", name))
	{
	}

	ZoneBuilder::Zone::~Zone()
	{
#ifdef DEBUG
		for (auto& subAsset : this->loadedSubAssets)
		{
			bool isFound = false;
			const std::string name = Game::DB_GetXAssetName(&subAsset);

			for (auto& alias : this->aliasList)
			{
				if (subAsset.type == alias.first.type && name == Game::DB_GetXAssetName(&alias.first))
				{
					isFound = true;
					break;
				}
			}

			if (!isFound)
			{
				Logger::Print("Asset {} of type {} was loaded, but not written!\n", name, Game::DB_GetXAssetTypeName(subAsset.type));
			}
		}

		for (auto& alias : this->aliasList)
		{
			bool isFound = false;
			const std::string name = Game::DB_GetXAssetName(&alias.first);

			for (auto& subAsset : this->loadedSubAssets)
			{
				if (subAsset.type == alias.first.type && name == Game::DB_GetXAssetName(&subAsset))
				{
					isFound = true;
					break;
				}
			}

			if (!isFound)
			{
				Logger::Error("Asset {} of type {} was written, but not loaded!\n", name, Game::DB_GetXAssetTypeName(alias.first.type));
			}
		}
#endif

		Game::XZoneInfo info{};
		info.name = nullptr;
		info.allocFlags = 0;
		info.freeFlags = DB_ZONE_LOAD;

		Game::DB_LoadXAssets(&info, 1, true);

		AssetHandler::ClearTemporaryAssets();
	}

	Utils::Stream* ZoneBuilder::Zone::GetBuffer()
	{
		return &this->buffer;
	}

	Utils::Memory::Allocator* ZoneBuilder::Zone::GetAllocator()
	{
		return &this->memAllocator;
	}

	void ZoneBuilder::Zone::Build()
	{
		if (!this->dataMap.IsValid())
		{
			Logger::Print("Unable to load CSV for '{}'!\n", this->zoneName);
			return;
		}

		this->LoadFastFiles();

		Logger::Print("Linking assets...\n");

		if (!this->LoadAssets())
		{
			return;
		}

		this->AddBranding();

		Logger::Print("Saving...\n");

		if (!this->TrySaveData())
		{
			return;
		}

		if (this->buffer.HasBlock())
		{
			Logger::Fatal("Non-popped blocks left!\n");
		}

		Logger::Print("Compressing...\n");
		this->WriteZone();
	}

	void ZoneBuilder::Zone::LoadFastFiles() const
	{
		Logger::Print("Loading required FastFiles...\n");

		for (std::size_t i = 0; i < this->dataMap.GetRows(); ++i)
		{
			if (this->dataMap.GetElementAt(i, 0) != "require")
			{
				continue;
			}

			const std::string fastfile = this->dataMap.GetElementAt(i, 1);

			if (Game::DB_IsZoneLoaded(fastfile.data()))
			{
				Logger::Print("Zone '{}' already loaded\n", fastfile);
				continue;
			}

			Game::XZoneInfo info{};
			info.name = fastfile.data();
			info.allocFlags = DB_ZONE_LOAD;
			info.freeFlags = 0;

			Game::DB_LoadXAssets(&info, 1, true);
		}
	}

	bool ZoneBuilder::Zone::LoadAssets()
	{
		for (std::size_t i = 0; i < this->dataMap.GetRows(); ++i)
		{
			if (this->dataMap.GetElementAt(i, 0) == "require"s)
			{
				continue;
			}

			if (this->dataMap.GetElementAt(i, 0) == "localize"s)
			{
				const auto filename = this->dataMap.GetElementAt(i, 1);

				if (FileSystem::File file(std::format("localizedstrings/{}.str", filename)); file.Exists())
				{
					Assets::ILocalizeEntry::ParseLocalizedStringsFile(this, filename, file.GetName());
					continue;
				}

				if (FileSystem::File file(std::format("localizedstrings/{}.json", filename)); file.Exists())
				{
					Assets::ILocalizeEntry::ParseLocalizedStringsJSON(this, file);
					continue;
				}
			}

			if (this->dataMap.GetColumns(i) > 2)
			{
				const auto oldName = this->dataMap.GetElementAt(i, 1);
				const auto newName = this->dataMap.GetElementAt(i, 2);
				const auto typeName = this->dataMap.GetElementAt(i, 0);
				const auto type = Game::DB_GetXAssetNameType(typeName.data());

				if (type >= Game::ASSET_TYPE_COUNT || type < 0)
				{
					Logger::Fatal("Unable to rename '{}' to '{}' as the asset type '{}' is invalid!", oldName, newName, typeName);
				}

				this->RenameAsset(type, oldName, newName);
			}

			if (!this->LoadAssetByName(this->dataMap.GetElementAt(i, 0), this->dataMap.GetElementAt(i, 1), false))
			{
				return false;
			}
		}

		return true;
	}

	bool ZoneBuilder::Zone::LoadAsset(Game::XAssetType type, void* data, bool isSubAsset)
	{
		const Game::XAsset asset{ static_cast<unsigned int>(type), data };
		const char* name = Game::DB_GetXAssetName(&asset);

		if (!name)
		{
			return false;
		}

		return this->LoadAssetByName(type, std::string(name), isSubAsset);
	}

	bool ZoneBuilder::Zone::LoadAssetByName(Game::XAssetType type, const std::string& name, bool isSubAsset)
	{
		return this->LoadAssetByName(std::string(Game::DB_GetXAssetTypeName(type)), name, isSubAsset);
	}

	bool ZoneBuilder::Zone::LoadAssetByName(const std::string& typeName, std::string name, bool isSubAsset)
	{
		const Game::XAssetType type = Game::DB_GetXAssetNameType(typeName.data());

		if (name.find(' ', 0) != std::string::npos)
		{
			Logger::Warning("Asset with name '{}' contains spaces. Check your zone source file to ensure this is correct!\n", name);
		}

		if (name[0] == ',')
		{
			name.erase(name.begin());
		}

		std::replace(name.begin(), name.end(), '\\', '/');

		if (this->FindAsset(type, name) != -1 || this->FindSubAsset(type, name).data)
		{
			return true;
		}

		if (type >= Game::ASSET_TYPE_COUNT || type < 0)
		{
			Logger::Fatal("Invalid asset type '{}'\n", typeName);
		}

		const Game::XAssetHeader assetHeader = AssetHandler::FindAssetForZone(type, name, this, isSubAsset);

		if (!assetHeader.data)
		{
			Logger::Fatal("Missing asset '{}' of type '{}'\n", name, Game::DB_GetXAssetTypeName(type));
		}

		const Game::XAsset asset{ static_cast<unsigned int>(type), assetHeader.data };

		AssetHandler::ZoneMark(asset, this);

		if (isSubAsset)
		{
			this->loadedSubAssets.push_back(asset);
		}
		else
		{
			this->loadedAssets.push_back(asset);
		}

		return true;
	}

	int ZoneBuilder::Zone::FindAsset(Game::XAssetType type, std::string name)
	{
		if (name[0] == ',')
		{
			name.erase(name.begin());
		}

		for (std::size_t i = 0; i < this->loadedAssets.size(); ++i)
		{
			const Game::XAsset* asset = &this->loadedAssets[i];

			if (asset->type != static_cast<unsigned int>(type))
			{
				continue;
			}

			const char* assetName = Game::DB_GetXAssetName(asset);

			if (!assetName)
			{
				return -1;
			}

			if (assetName[0] == ',' && assetName[1] != '\0')
			{
				++assetName;
			}

			if (this->GetAssetName(type, assetName) == name)
			{
				return static_cast<int>(i);
			}

			if (name == assetName)
			{
				return static_cast<int>(i);
			}
		}

		return -1;
	}

	Game::XAssetHeader ZoneBuilder::Zone::FindSubAsset(Game::XAssetType type, std::string name)
	{
		if (name[0] == ',')
		{
			name.erase(name.begin());
		}

		for (const auto& asset : this->loadedSubAssets)
		{
			if (asset.type != static_cast<unsigned int>(type))
			{
				continue;
			}

			const char* assetName = Game::DB_GetXAssetName(&asset);

			if (assetName[0] == ',')
			{
				++assetName;
			}

			if (name == assetName)
			{
				return { asset.header };
			}
		}

		return { nullptr };
	}

	Game::XAsset* ZoneBuilder::Zone::GetAsset(int index)
	{
		if (static_cast<std::size_t>(index) < this->loadedAssets.size())
		{
			return &this->loadedAssets[index];
		}

		return nullptr;
	}

	std::uint32_t ZoneBuilder::Zone::GetAssetTableOffset(int index)
	{
		Utils::Stream::Offset offset;
		offset.block = Game::XFILE_BLOCK_VIRTUAL;
		offset.offset = static_cast<std::uint32_t>(this->indexStart + index * sizeof(Game::X86::XAsset) + 4);
		return offset.GetPackedOffset();
	}

	bool ZoneBuilder::Zone::HasAlias(Game::XAsset asset)
	{
		return this->GetAlias(asset) != 0;
	}

	std::uint32_t ZoneBuilder::Zone::SaveSubAsset(Game::XAssetType type, void* ptr)
	{
		Game::XAsset asset{ static_cast<unsigned int>(type), ptr };
		const std::string name = Game::DB_GetXAssetName(&asset);

		const int assetIndex = this->FindAsset(type, name);

		if (assetIndex != -1)
		{
			return this->GetAssetTableOffset(assetIndex);
		}

		if (this->HasAlias(asset))
		{
			return this->GetAlias(asset);
		}

		asset.header = this->FindSubAsset(type, name);

		if (!asset.header.data)
		{
			Logger::Fatal("Missing required asset '{}' ({}). Export failed!", name, Game::DB_GetXAssetTypeName(type));
		}

		this->buffer.PushBlock(Game::XFILE_BLOCK_VIRTUAL);
		this->buffer.Align(Utils::Stream::ALIGN_4);
		this->StoreAlias(asset);
		this->buffer.IncreaseBlockSize(4);
		this->buffer.PopBlock();

		this->buffer.PushBlock(Game::XFILE_BLOCK_TEMP);
		this->buffer.Align(Utils::Stream::ALIGN_4);
		AssetHandler::ZoneSave(asset, this);
		this->buffer.PopBlock();

		return insertPointerMarker;
	}

	void ZoneBuilder::Zone::WriteZone()
	{
		FILETIME fileTime;
		GetSystemTimeAsFileTime(&fileTime);

		const bool shouldProtect = FastFiles::ShouldProtectZone(this->zoneName);

		std::uint64_t magic = XFILE_MAGIC_UNSIGNED;

		if (shouldProtect)
		{
			magic = XFILE_HEADER_ZW3 | (static_cast<std::uint64_t>(XFILE_VERSION_ZW3) << 32);
		}

		const std::uint32_t version = XFILE_VERSION;
		const std::uint32_t highDateTime = fileTime.dwHighDateTime;
		const std::uint32_t lowDateTime = fileTime.dwLowDateTime;

		std::string outBuffer;
		outBuffer.append(reinterpret_cast<const char*>(&magic), sizeof(magic));
		outBuffer.append(reinterpret_cast<const char*>(&version), sizeof(version));
		outBuffer.append(reinterpret_cast<const char*>(&XLANG_NONE), sizeof(XLANG_NONE));
		outBuffer.append(reinterpret_cast<const char*>(&highDateTime), sizeof(highDateTime));
		outBuffer.append(reinterpret_cast<const char*>(&lowDateTime), sizeof(lowDateTime));

		std::string zoneBuffer = this->buffer.ToBuffer();

		if (shouldProtect)
		{
			FastFiles::ProtectZoneBuffer(zoneBuffer);
		}

		zoneBuffer = Utils::Compression::ZLib::Compress(zoneBuffer);

		if (zoneBuffer.empty())
		{
			Logger::Error("zonebuilder: zlib could not compress '{}', nothing was written\n", this->destination);
			return;
		}

		outBuffer.append(zoneBuffer);

		const auto directoryName = std::filesystem::path(this->destination).parent_path();
		Utils::IO::CreateDir(directoryName.string());

		if (!Utils::IO::WriteFile(this->destination, outBuffer))
		{
			Logger::Error("zonebuilder: could not write '{}'\n", this->destination);
			return;
		}

		Logger::Print("done writing {}\n", this->destination);
		Logger::Print("Zone '{}' written with {} assets and {} script strings\n", this->destination, this->aliasList.size() + this->loadedAssets.size(), this->scriptStrings.size());
	}

	bool ZoneBuilder::Zone::TrySaveData()
	{
		Game::X86::XFile xFile{};
		Game::X86::XAssetList assetList{};

		assetList.assetCount = static_cast<decltype(assetList.assetCount)>(this->loadedAssets.size());
		Utils::Stream::ClearPointer(&assetList.assets);

		assetList.stringList.count = static_cast<decltype(assetList.stringList.count)>(this->scriptStrings.size() + 1);
		Utils::Stream::ClearPointer(&assetList.stringList.strings);

		this->buffer.Save(&xFile, sizeof(xFile));
		this->buffer.Save(&assetList, sizeof(assetList));
		this->buffer.PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		this->buffer.SaveNull(4);

		for (std::size_t i = 0; i < this->scriptStrings.size(); ++i)
		{
			this->buffer.SaveMax(4);
		}

		for (const auto& scriptString : this->scriptStrings)
		{
			this->buffer.SaveString(scriptString.data());
		}

		this->buffer.Align(Utils::Stream::ALIGN_4);
		this->indexStart = static_cast<int>(this->buffer.GetBlockSize(Game::XFILE_BLOCK_VIRTUAL));

		for (const auto asset : this->loadedAssets)
		{
			Game::X86::XAsset entry{};
			entry.type = static_cast<decltype(entry.type)>(asset.type);
			Utils::Stream::ClearPointer(&entry.header.data);

			this->buffer.Save(&entry);
		}

		for (const auto asset : this->loadedAssets)
		{
			this->buffer.PushBlock(Game::XFILE_BLOCK_TEMP);
			this->buffer.Align(Utils::Stream::ALIGN_4);

			this->Store({ asset.header });
			AssetHandler::ZoneSave(asset, this);

			this->buffer.PopBlock();
		}

		this->buffer.EnterCriticalSection();

		auto* const header = reinterpret_cast<Game::X86::XFile*>(this->buffer.Data());
		header->size = static_cast<decltype(header->size)>(this->buffer.Length() - sizeof(Game::X86::XFile));
		header->externalSize = static_cast<decltype(header->externalSize)>(this->externalSize);

		for (int i = 0; i < Game::MAX_XFILE_COUNT; ++i)
		{
			header->blockSize[i] = static_cast<std::remove_reference_t<decltype(header->blockSize[i])>>(this->buffer.GetBlockSize(static_cast<Game::XFILE_BLOCK_TYPES>(i)));
		}

		this->buffer.LeaveCriticalSection();
		this->buffer.PopBlock();

		return true;
	}

	void ZoneBuilder::Zone::AddBranding()
	{
		const auto now = std::chrono::system_clock::now();

		const auto zoneBranding = std::format("Built using the IW4x ZoneBuilder! {:%d-%m-%Y %H:%M:%OS}", now);
		const auto brandingLen = zoneBranding.size();

		this->branding = { this->zoneName.data(), 0, static_cast<int>(brandingLen), this->GetAllocator()->DuplicateString(zoneBranding) };

		if (this->FindAsset(Game::ASSET_TYPE_RAWFILE, this->branding.name) != -1)
		{
			Logger::Fatal("Unable to add branding. Asset '{}' already exists!", this->branding.name);
		}

		this->loadedAssets.push_back({ Game::ASSET_TYPE_RAWFILE, &this->branding });
	}

	bool ZoneBuilder::Zone::HasPointer(const void* pointer)
	{
		return this->pointerMap.contains(pointer);
	}

	std::uint32_t ZoneBuilder::Zone::SafeGetPointer(const void* pointer)
	{
		const auto stored = this->pointerMap.find(pointer);

		if (stored != this->pointerMap.end())
		{
			return stored->second;
		}

		return 0;
	}

	std::uint32_t ZoneBuilder::Zone::GetPointer(const void* pointer)
	{
		return this->SafeGetPointer(pointer);
	}

	void ZoneBuilder::Zone::StorePointer(const void* pointer)
	{
		this->pointerMap[pointer] = this->buffer.GetPackedOffset();
	}

	void ZoneBuilder::Zone::StoreAlias(Game::XAsset asset)
	{
		if (!this->HasAlias(asset))
		{
			this->aliasList.emplace_back(asset, this->buffer.GetPackedOffset());
		}
	}

	std::uint32_t ZoneBuilder::Zone::GetAlias(Game::XAsset asset)
	{
		const std::string name = Game::DB_GetXAssetName(&asset);

		for (auto& [aliasAsset, offset] : this->aliasList)
		{
			if (asset.type == aliasAsset.type && name == Game::DB_GetXAssetName(&aliasAsset))
			{
				return offset;
			}
		}

		return 0;
	}

	int ZoneBuilder::Zone::AddScriptString(const std::string& str)
	{
		return this->AddScriptString(static_cast<unsigned short>(Game::SL_GetString(str.data(), 0)));
	}

	int ZoneBuilder::Zone::AddScriptString(unsigned short gameIndex)
	{
		if (!gameIndex)
		{
			if (this->scriptStrings.empty())
			{
				this->scriptStrings.push_back("");
			}

			return 0;
		}

		const std::string str = Game::SL_ConvertToString(gameIndex);
		const int prev = this->FindScriptString(str);

		if (prev > 0)
		{
			this->scriptStringMap[gameIndex] = prev;
			return prev;
		}

		this->scriptStrings.push_back(str);
		this->scriptStringMap[gameIndex] = static_cast<unsigned int>(this->scriptStrings.size());
		return static_cast<int>(this->scriptStrings.size());
	}

	int ZoneBuilder::Zone::FindScriptString(const std::string& str)
	{
		for (std::size_t i = 0; i < this->scriptStrings.size(); ++i)
		{
			if (this->scriptStrings[i] == str)
			{
				return static_cast<int>(i + 1);
			}
		}

		return -1;
	}

	void ZoneBuilder::Zone::AddRawAsset(Game::XAssetType type, void* ptr)
	{
		this->loadedAssets.push_back({ static_cast<unsigned int>(type), ptr });
	}

	void ZoneBuilder::Zone::MapScriptString(unsigned short& gameIndex)
	{
		gameIndex = static_cast<unsigned short>(0xFFFF & this->scriptStringMap[gameIndex]);
	}

	void ZoneBuilder::Zone::RenameAsset(Game::XAssetType type, const std::string& asset, const std::string& newName)
	{
		if (type >= Game::ASSET_TYPE_COUNT || type < 0)
		{
			Logger::Fatal("Unable to rename '{}' to '{}' as the asset type is invalid!", asset, newName);
		}

		this->renameMap[type][asset] = newName;
	}

	std::string ZoneBuilder::Zone::GetAssetName(Game::XAssetType type, const std::string& asset)
	{
		if (type >= Game::ASSET_TYPE_COUNT || type < 0)
		{
			Logger::Fatal("Unable to get name for '{}' as the asset type is invalid!", asset);
		}

		const auto renamed = this->renameMap[type].find(asset);

		if (renamed != this->renameMap[type].end())
		{
			return renamed->second;
		}

		return asset;
	}

	void ZoneBuilder::Zone::Store(Game::XAssetHeader header)
	{
		if (!this->HasPointer(header.data))
		{
			this->StorePointer(header.data);
		}
	}

	void ZoneBuilder::Zone::IncrementExternalSize(unsigned int size)
	{
		this->externalSize += size;
	}

	bool ZoneBuilder::IsEnabled()
	{
		static std::optional<bool> flag;

		if (!flag.has_value())
		{
			flag.emplace(Flags::HasFlag("zonebuilder"));
		}

		return flag.value();
	}

	void ZoneBuilder::BeginAssetTrace(const std::string& zone)
	{
		traceZone = zone;
	}

	std::vector<ZoneBuilder::NamedAsset> ZoneBuilder::EndAssetTrace()
	{
		traceZone.clear();

		std::vector<NamedAsset> assetTrace = std::move(traceAssets);
		traceAssets.clear();

		return assetTrace;
	}

	Game::XAssetHeader ZoneBuilder::GetEmptyAssetIfCommon(Game::XAssetType type, const std::string& name, Zone* builder)
	{
		Game::XAssetHeader header{ nullptr };

		if (type < 0 || type >= Game::ASSET_TYPE_COUNT)
		{
			return header;
		}

		std::string commonZone = "common_mp";

		if (FastFiles::HasZW3CommonZone())
		{
			commonZone = "zw3_common";
		}

		const int zoneIndex = Game::DB_GetZoneIndex(commonZone);

		if (zoneIndex <= 0)
		{
			return header;
		}

		const auto* const entry = Game::DB_FindXAssetEntry(type, name.data());

		if (!entry || reinterpret_cast<const std::uint8_t*>(entry)[assetEntryZoneIndex] != zoneIndex)
		{
			return header;
		}

		header.data = builder->GetAllocator()->Allocate(static_cast<std::size_t>(Game::DB_GetXAssetTypeSize(type)));

		Game::XAsset asset{ static_cast<unsigned int>(type), header.data };
		Game::DB_SetXAssetName(&asset, name.data());
		AssetHandler::StoreTemporaryAsset(type, header);

		Game::DB_SetXAssetName(&asset, builder->GetAllocator()->DuplicateString("," + name));

		return header;
	}

	std::string ZoneBuilder::GetDumpingZonePath()
	{
		if (dumpingZone.empty())
		{
			return "userraw/dump/stray";
		}

		return std::format("userraw/dump/{}", dumpingZone);
	}

	void ZoneBuilder::StoreTexture(Game::GfxImageLoadDef** loadDef, Game::GfxImage* image)
	{
		const auto* const source = reinterpret_cast<const std::uint8_t*>(*loadDef);

		std::int32_t resourceSize = 0;
		std::memcpy(&resourceSize, source + imageLoadDefResourceSize, sizeof(resourceSize));

		const auto size = imageLoadDefHeaderSize + static_cast<std::size_t>(resourceSize);
		void* const data = Utils::Memory::GetAllocator()->Allocate(size);
		std::memcpy(data, source, size);

		std::memcpy(&image->texture, &data, sizeof(data));
	}

	void ZoneBuilder::ReleaseTexture(Game::XAssetHeader header)
	{
		if (!header.image)
		{
			return;
		}

		void* loadDef = nullptr;
		std::memcpy(&loadDef, &header.image->texture, sizeof(loadDef));

		if (loadDef)
		{
			Utils::Memory::GetAllocator()->Free(loadDef);
		}
	}

	void ZoneBuilder::DumpZone(const std::string& zone)
	{
		if (FastFiles::IsZombieZoneName(zone))
		{
			Logger::Error("Dumping zombie zones is disabled.\n");
			return;
		}

		dumpingZone = zone;
		AssetHandler::ForgetDumpedAssets();

		std::vector<NamedAsset> assets;
		const auto unload = LoadZoneWithTrace(zone, assets);

		Logger::Print("Dumping zone '{}'...\n", zone);

		Utils::IO::CreateDir(GetDumpingZonePath());

		constexpr Game::XAssetType typeOrder[] =
		{
			Game::ASSET_TYPE_GAMEWORLD_MP,
			Game::ASSET_TYPE_GAMEWORLD_SP,
			Game::ASSET_TYPE_GFXWORLD,
			Game::ASSET_TYPE_COMWORLD,
			Game::ASSET_TYPE_FXWORLD,
			Game::ASSET_TYPE_CLIPMAP_MP,
			Game::ASSET_TYPE_CLIPMAP_SP,
			Game::ASSET_TYPE_RAWFILE,
			Game::ASSET_TYPE_VEHICLE,
			Game::ASSET_TYPE_WEAPON,
			Game::ASSET_TYPE_FX,
			Game::ASSET_TYPE_TRACER,
			Game::ASSET_TYPE_XMODEL,
			Game::ASSET_TYPE_MATERIAL,
			Game::ASSET_TYPE_TECHNIQUE_SET,
			Game::ASSET_TYPE_PIXELSHADER,
			Game::ASSET_TYPE_VERTEXSHADER,
			Game::ASSET_TYPE_VERTEXDECL,
			Game::ASSET_TYPE_LIGHT_DEF,
			Game::ASSET_TYPE_IMAGE,
			Game::ASSET_TYPE_SOUND,
			Game::ASSET_TYPE_LOADED_SOUND,
			Game::ASSET_TYPE_SOUND_CURVE,
			Game::ASSET_TYPE_PHYSPRESET,
			Game::ASSET_TYPE_LOCALIZE_ENTRY,
		};

		std::unordered_map<Game::XAssetType, int> typePriority;

		for (std::size_t i = 0; i < std::size(typeOrder); ++i)
		{
			typePriority.emplace(typeOrder[i], static_cast<int>(1 + std::size(typeOrder) - i));
		}

		enum AssetCategory
		{
			EXPLICIT,
			IMPLICIT,
			NOT_SUPPORTED,
			DISAPPEARED,

			COUNT
		};

		std::vector<std::string> assetsToList[AssetCategory::COUNT]{};

		std::sort(assets.begin(), assets.end(), [&](const NamedAsset& a, const NamedAsset& b)
		{
			if (a.type == b.type)
			{
				return a.name.compare(b.name) < 0;
			}

			const auto priorityA = typePriority[a.type];
			const auto priorityB = typePriority[b.type];

			if (priorityA == priorityB)
			{
				return a.name.compare(b.name) < 0;
			}

			return priorityB < priorityA;
		});

		Logger::Print("Filtering asset list for '{}.csv'...\n", zone);

		std::vector<Game::XAsset> explicitAssetsToFilter;

		for (const auto& asset : assets)
		{
			const auto type = asset.type;
			const auto& name = asset.name;

			if (!AssetHandler::CanDump(type) || name[0] == ',')
			{
				assetsToList[AssetCategory::NOT_SUPPORTED].push_back(std::format("{},{}", Game::DB_GetXAssetTypeName(type), name));
				continue;
			}

			void* assetHeader = nullptr;

			if (Game::DB_FindXAssetEntry(type, name.data()))
			{
				assetHeader = Game::DB_FindXAssetHeader(type, name.data());
			}

			if (!assetHeader)
			{
				Logger::Warning("Asset {} has disappeared while dumping!\n", name);
				assetsToList[AssetCategory::DISAPPEARED].push_back(std::format("{},{}", Game::DB_GetXAssetTypeName(type), name));
				continue;
			}

			explicitAssetsToFilter.push_back({ static_cast<unsigned int>(type), assetHeader });
		}

		Logger::Print("Dumping remaining assets...\n");

		for (auto& asset : explicitAssetsToFilter)
		{
			const auto* const name = Game::DB_GetXAssetName(&asset);

			AssetHandler::DumpAsset(asset);
			Logger::Print(".");
			assetsToList[AssetCategory::EXPLICIT].push_back(std::format("{},{}", Game::DB_GetXAssetTypeName(asset.type), name));
		}

		Logger::Print("\n");

		Logger::Print("Writing zone source...\n");

		std::ofstream csv(std::filesystem::path(GetDumpingZonePath()) / std::format("{}.csv", zone));
		csv << std::format("### Zone '{}' dumped with Zonebuilder {}", zone, Branding::GetVersionString()) << "\n\n";

		for (std::size_t i = 0; i < static_cast<std::size_t>(AssetCategory::COUNT); ++i)
		{
			if (assetsToList[i].empty())
			{
				continue;
			}

			const auto category = static_cast<AssetCategory>(i);

			switch (category)
			{
			case AssetCategory::EXPLICIT:
				break;
			case AssetCategory::IMPLICIT:
				csv << "\n### The following assets are implicitly built with previous assets:\n";
				break;
			case AssetCategory::NOT_SUPPORTED:
				csv << "\n### The following assets are not supported for dumping:\n";
				break;
			case AssetCategory::DISAPPEARED:
				csv << "\n### The following assets disappeared while dumping but are mentioned in the zone:\n";
				break;
			default:
				break;
			}

			for (const auto& asset : assetsToList[i])
			{
				if (category != AssetCategory::EXPLICIT)
				{
					csv << "#";
				}

				csv << asset << "\n";
			}
		}

		csv << std::format("\n### {} assets", assets.size()) << "\n";
		csv.close();

		unload();

		Logger::Print("Zone '{}' dumped", dumpingZone);
		dumpingZone.clear();
	}

	std::function<void()> ZoneBuilder::LoadZoneWithTrace(const std::string& zone, std::vector<NamedAsset>& assets)
	{
		BeginAssetTrace(zone);

		Game::XZoneInfo info{};
		info.name = zone.data();
		info.allocFlags = DB_ZONE_MOD;
		info.freeFlags = 0;

		Logger::Print("Loading zone '{}'...\n", zone);

		Game::DB_LoadXAssets(&info, 1, true);

		assets = EndAssetTrace();

		return [zone]
		{
			Logger::Print("Unloading zone '{}'...\n", zone);

			Game::XZoneInfo unloadInfo{};
			unloadInfo.freeFlags = DB_ZONE_MOD;
			unloadInfo.allocFlags = 0;
			unloadInfo.name = nullptr;

			Game::DB_LoadXAssets(&unloadInfo, 1, true);
		};
	}

	bool ZoneBuilder::ApplyEnginePatches()
	{
		const auto releaseImageSlot = DB_ReleaseXAssetHandler + Game::ASSET_TYPE_IMAGE * sizeof(void*);

		const bool isExpected = Utils::Hook::MatchesBytes(Load_Texture, loadTextureEntry, sizeof(loadTextureEntry))
			&& Utils::Hook::Get<std::uintptr_t>(releaseImageSlot) == Utils::Hook::Rebase(j_Image_Release)
			&& Utils::Hook::MatchesBytes(Load_ClipMapAsset_SetInUse, clipMapSetInUse, sizeof(clipMapSetInUse))
			&& Utils::Hook::BranchesTo(DB_PostLoadXZone_OverrideTechniqueSetsCall, Material_OverrideTechniqueSets, HOOK_CALL);

		if (!isExpected)
		{
			Logger::Error("zonebuilder: the engine's texture, release, clip map or techset sites do not read as expected\n");
			return false;
		}

		if (!loadTextureHook.Initialize(Load_Texture, reinterpret_cast<void*>(StoreTexture), HOOK_JUMP)->Install()->IsInstalled())
		{
			loadTextureHook.Uninstall();
			Logger::Error("zonebuilder: could not seat the Load_Texture hook\n");
			return false;
		}

		loadTextureHook.Quick();

		Utils::Hook::Set<void(*)(Game::XAssetHeader)>(releaseImageSlot, ReleaseTexture);
		Utils::Hook::Nop(Load_ClipMapAsset_SetInUse, sizeof(clipMapSetInUse));
		Utils::Hook::Nop(DB_PostLoadXZone_OverrideTechniqueSetsCall, 5);

		return true;
	}

	ZoneBuilder::ZoneBuilder()
	{
		static_assert(Game::MAX_XFILE_COUNT == 8, "XFile block enum is invalid!");

		if (!IsEnabled())
		{
			return;
		}

		if (!ApplyEnginePatches())
		{
			MessageBoxA(nullptr, "The zonebuilder's engine patches could not be applied, see iw4x\\iw4x.log.", "Zombie Warfare 3", MB_ICONERROR);
			TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
		}

		Events::OnDvarInit([]
		{
			zb_sp_to_mp = Dvar::Register("zb_sp_to_mp", false, Game::DVAR_ARCHIVE, "Attempt to convert singleplayer assets to multiplayer format whenever possible");
		});

		AssetHandler::OnLoad([](const unsigned int type, void*, const std::string& name, bool*)
		{
			if (!traceZone.empty() && traceZone == FastFiles::Current())
			{
				traceAssets.push_back({ static_cast<Game::XAssetType>(type), name });
			}
		});

		Command::Add("dumpzone", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			DumpZone(params->Get(1));
		});

		Command::Add("verifyzone", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const std::string zone = params->Get(1);
			std::vector<NamedAsset> assets;
			const auto unload = LoadZoneWithTrace(zone, assets);

			int count = 0;

			for (const auto& asset : assets)
			{
				Logger::Print(" {}: {}: {}\n", count, Game::DB_GetXAssetTypeName(asset.type), asset.name);
				++count;
			}

			Logger::Print("\n");
			unload();
		});

		Command::Add("buildzone", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const std::string zoneName = params->Get(1);
			Logger::Print("Building zone '{}'...\n", zoneName);

			Zone(zoneName).Build();
		});

		Command::Add("buildmod", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const Dvar::Var fs_game("fs_game");

			const std::string modName = params->Get(1);
			Logger::Print("Building zone for mod '{}'...\n", modName);

			const std::string previousFsGame = fs_game.Get<std::string>();
			const std::string dir = "mods/" + modName;
			Utils::IO::CreateDir(dir);

			fs_game.Set(dir);
			Game::FS_Restart(0, 0);

			Zone("mod", modName, dir + "/mod.ff").Build();

			fs_game.Set(previousFsGame);
			Game::FS_Restart(0, 0);
		});

		Command::Add("buildall", []
		{
			const auto path = std::format("{}\\zone_source", (*Game::fs_basepath)->current.string);
			const auto zoneSources = FileSystem::GetSysFileList(path, "csv", false);

			for (auto source : zoneSources)
			{
				if (Utils::String::EndsWith(source, ".csv"))
				{
					source = source.substr(0, source.find(".csv"));
				}

				Command::Execute(std::format("buildzone {}", source), true);
			}
		});

		Command::Add("listassets", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			Game::XAssetType type = Game::DB_GetXAssetNameType(params->Get(1));

			if (type >= Game::ASSET_TYPE_COUNT)
			{
				return;
			}

			Game::DB_EnumXAssets_FastFile(type, [](void* header, void* data)
			{
				const Game::XAsset asset{ *static_cast<unsigned int*>(data), header };
				Logger::Print("{}\n", Game::DB_GetXAssetName(&asset));
			}, &type, false);
		});

		Scheduler::OnGameInitialized([]
		{
			Logger::Print(" --------------------------------------------------------------------------------\n");
			Logger::Print(" IW4x ZoneBuilder - {}\n", Branding::GetVersionString());
			Logger::Print(" Commands:\n");
			Logger::Print("\t- buildmod [mod name]: Build a mod.ff from the source located in zone_source/mod_name.csv\n");
			Logger::Print("\t- buildzone [zone]: Builds a zone from a csv located in zone_source\n");
			Logger::Print("\t- dumpzone [zone]: Loads and dump the specified zone in userraw/dump\n");
			Logger::Print("\t- verifyzone [zone]: Loads and verifies the specified zone\n");
			Logger::Print("\t- listassets [assettype]: Lists all loaded assets of the specified type\n");
			Logger::Print("\t- quit: Quits the program\n");
			Logger::Print(" --------------------------------------------------------------------------------\n");
		}, Scheduler::Pipeline::MAIN);
	}
}
