#include "STDInclude.hpp"

#include <shared_mutex>

#include "ModelSurfs.hpp"
#include "Dedicated.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"
#include "Renderer.hpp"

namespace Components
{
	constexpr unsigned char cloneZoneHandle = 0xFF;

	constexpr std::uintptr_t DB_GetIndexBufferAndBase = 0x14012DCF0;
	constexpr std::uintptr_t DB_GetVertexBufferAndOffset = 0x14012DEC0;
	constexpr std::uintptr_t DB_GetIndexBuffer = 0x14012DCD0;
	constexpr std::uintptr_t DB_GetBaseIndex = 0x14012DCB0;

	constexpr std::uintptr_t Load_XModelAsset_SurfsFixupCall = 0x140131B74;
	constexpr std::uintptr_t DB_XModelSurfsFixup = 0x140130120;
	constexpr std::uintptr_t DB_ReleaseXAssetHandler = 0x140422180;

	constexpr int modelFileVersion = 1;
	constexpr std::uint32_t modelSurfsSize = 36;
	constexpr std::uint32_t surfaceSize = 84;
	constexpr std::uint32_t vertListSize = 12;
	constexpr std::uint32_t collisionTreeSize = 40;

	constexpr std::uintptr_t indexBufferAndBaseCalls[] = { 0x140075167, 0x140076838, 0x14007ABE5 };
	constexpr std::uintptr_t vertexBufferAndOffsetCalls[] = { 0x140076874, 0x14007AC2E, 0x14007B944, 0x14007BE55, 0x14007C31D };
	constexpr std::uintptr_t indexBufferCalls[] = { 0x14007B912, 0x14007BE19, 0x14007C2E1 };
	constexpr std::uintptr_t baseIndexCalls[] = { 0x14007BB8B, 0x14007C09F, 0x14007C572 };

	constexpr std::size_t hookCount = std::size(indexBufferAndBaseCalls) + std::size(vertexBufferAndOffsetCalls)
		+ std::size(indexBufferCalls) + std::size(baseIndexCalls);

	using DB_GetIndexBufferAndBase_t = void(*)(unsigned char zoneHandle, void* indices, IDirect3DIndexBuffer9** indexBuffer, int* baseIndex);
	using DB_GetVertexBufferAndOffset_t = void(*)(unsigned char zoneHandle, void* verts, IDirect3DVertexBuffer9** vertexBuffer, int* vertexOffset);
	using DB_GetIndexBuffer_t = IDirect3DIndexBuffer9*(*)(unsigned char zoneHandle, void* indices);
	using DB_GetBaseIndex_t = int(*)(unsigned char zoneHandle, void* indices);
	using DB_XModelSurfsFixup_t = void(*)(Game::XModel* model, void* userData);
	using DB_ReleaseXAssetHandler_t = void(*)(Game::XModelSurfs* modelSurfs);

	struct LoadedSurfs
	{
		Utils::Memory::Allocator allocator;
		Game::XModelSurfs* surfs = nullptr;
	};

	struct ModelFile
	{
		std::uint8_t* sections[Game::SECTION_FIXUP];
		std::uint32_t sizes[Game::SECTION_FIXUP];
		std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> pointers;
	};

	static DB_GetIndexBufferAndBase_t getIndexBufferAndBase = nullptr;
	static DB_GetVertexBufferAndOffset_t getVertexBufferAndOffset = nullptr;
	static DB_GetIndexBuffer_t getIndexBuffer = nullptr;
	static DB_GetBaseIndex_t getBaseIndex = nullptr;
	static DB_XModelSurfsFixup_t xmodelSurfsFixup = nullptr;

	static Utils::Hook hooks[hookCount];
	static Utils::Hook surfsFixupHook;
	static bool isInstalled = false;

	static std::shared_mutex bufferMutex;
	static std::unordered_map<const void*, IUnknown*> buffers;

	static std::vector<Game::XModelSurfs*> clones;
	static Utils::Memory::Allocator cloneAllocator;

	static std::mutex loadedMutex;
	static std::unordered_map<const Game::XSurface*, std::unique_ptr<LoadedSurfs>> loaded;

	static IUnknown* FindBuffer(const void* data)
	{
		std::shared_lock lock(bufferMutex);

		const auto buffer = buffers.find(data);

		if (buffer == buffers.end())
		{
			return nullptr;
		}

		return buffer->second;
	}

	static void CreateBuffers(const Game::XModelSurfs* surfs)
	{
		for (int i = 0; i < surfs->numsurfs; ++i)
		{
			const Game::XSurface* surface = &surfs->surfs[i];

			if (surface->zoneHandle != cloneZoneHandle)
			{
				continue;
			}

			IDirect3DVertexBuffer9* vertexBuffer = nullptr;
			IDirect3DIndexBuffer9* indexBuffer = nullptr;

			Game::Load_VertexBuffer(&vertexBuffer, surface->verts0, surface->vertCount * static_cast<int>(sizeof(Game::GfxPackedVertex)));
			Game::Load_IndexBuffer(surface->triIndices, &indexBuffer, surface->triCount * 3);

			std::unique_lock lock(bufferMutex);

			if (vertexBuffer)
			{
				buffers[surface->verts0] = vertexBuffer;
			}

			if (indexBuffer)
			{
				buffers[surface->triIndices] = indexBuffer;
			}
		}
	}

	static void ReleaseBuffer(const void* data)
	{
		const auto buffer = buffers.find(data);

		if (buffer == buffers.end())
		{
			return;
		}

		buffer->second->Release();
		buffers.erase(buffer);
	}

	static void ReleaseBuffers(const Game::XModelSurfs* surfs)
	{
		std::unique_lock lock(bufferMutex);

		for (int i = 0; i < surfs->numsurfs; ++i)
		{
			const Game::XSurface* surface = &surfs->surfs[i];

			if (surface->zoneHandle != cloneZoneHandle)
			{
				continue;
			}

			ReleaseBuffer(surface->verts0);
			ReleaseBuffer(surface->triIndices);
		}
	}

	static void BeginRecover()
	{
		std::unique_lock lock(bufferMutex);

		for (const auto& entry : buffers)
		{
			entry.second->Release();
		}

		buffers.clear();
	}

	static void EndRecover()
	{
		if (!*Game::dx_device)
		{
			return;
		}

		for (const auto* clone : clones)
		{
			CreateBuffers(clone);
		}

		std::lock_guard lock(loadedMutex);

		for (const auto& entry : loaded)
		{
			CreateBuffers(entry.second->surfs);
		}
	}

	static void DB_GetIndexBufferAndBase_Hook(unsigned char zoneHandle, void* indices, IDirect3DIndexBuffer9** indexBuffer, int* baseIndex)
	{
		if (zoneHandle != cloneZoneHandle)
		{
			getIndexBufferAndBase(zoneHandle, indices, indexBuffer, baseIndex);
			return;
		}

		*indexBuffer = static_cast<IDirect3DIndexBuffer9*>(FindBuffer(indices));
		*baseIndex = 0;
	}

	static void DB_GetVertexBufferAndOffset_Hook(unsigned char zoneHandle, void* verts, IDirect3DVertexBuffer9** vertexBuffer, int* vertexOffset)
	{
		if (zoneHandle != cloneZoneHandle)
		{
			getVertexBufferAndOffset(zoneHandle, verts, vertexBuffer, vertexOffset);
			return;
		}

		*vertexBuffer = static_cast<IDirect3DVertexBuffer9*>(FindBuffer(verts));
		*vertexOffset = 0;
	}

	static IDirect3DIndexBuffer9* DB_GetIndexBuffer_Hook(unsigned char zoneHandle, void* indices)
	{
		if (zoneHandle != cloneZoneHandle)
		{
			return getIndexBuffer(zoneHandle, indices);
		}

		return static_cast<IDirect3DIndexBuffer9*>(FindBuffer(indices));
	}

	static int DB_GetBaseIndex_Hook(unsigned char zoneHandle, void* indices)
	{
		if (zoneHandle != cloneZoneHandle)
		{
			return getBaseIndex(zoneHandle, indices);
		}

		return 0;
	}

	template <typename T>
	static T ReadField(const std::uint8_t* data, const std::uint32_t offset)
	{
		T value;
		std::memcpy(&value, data + offset, sizeof(T));
		return value;
	}

	template <typename T>
	static bool TryResolve(const ModelFile& model, const std::uint8_t* field, const std::uint32_t section, const std::uint64_t length, T** pointer)
	{
		*pointer = nullptr;

		const auto at = static_cast<std::uint32_t>(field - model.sections[Game::SECTION_MAIN]);
		const auto fixup = model.pointers.find(at);

		if (fixup == model.pointers.end())
		{
			return true;
		}

		const auto [target, offset] = fixup->second;

		if (target != section || offset + length > model.sizes[section])
		{
			return false;
		}

		*pointer = reinterpret_cast<T*>(model.sections[section] + offset);
		return true;
	}

	static bool TryLoadXModelSurfaces(const std::string& name, LoadedSurfs& record)
	{
		const FileSystem::FileReader reader(std::format("models/{}", name));

		if (!reader.Exists())
		{
			return false;
		}

		const std::string file = reader.GetBuffer();

		if (file.size() < sizeof(Game::CModelHeader))
		{
			return false;
		}

		Game::CModelHeader header;
		std::memcpy(&header, file.data(), sizeof(header));

		if (header.version != modelFileVersion)
		{
			return false;
		}

		for (const auto& section : header.sectionHeader)
		{
			if (section.size < 0 || section.offset < 0 || static_cast<std::uint64_t>(section.offset) + section.size > file.size())
			{
				return false;
			}
		}

		const auto& fixupSection = header.sectionHeader[Game::SECTION_FIXUP];
		const auto* const fixups = reinterpret_cast<const std::uint8_t*>(file.data()) + fixupSection.offset;
		const auto fixupCount = static_cast<std::uint64_t>(fixupSection.size) / sizeof(std::uint32_t);

		auto& allocator = record.allocator;
		ModelFile model{};

		for (int i = Game::SECTION_MAIN; i < Game::SECTION_FIXUP; ++i)
		{
			const auto& section = header.sectionHeader[i];

			if (section.fixupStart < 0 || section.fixupCount < 0 || static_cast<std::uint64_t>(section.fixupStart) + section.fixupCount > fixupCount)
			{
				return false;
			}

			if (i != Game::SECTION_MAIN && section.fixupCount)
			{
				return false;
			}

			model.sizes[i] = static_cast<std::uint32_t>(section.size);
			model.sections[i] = allocator.AllocateArray<std::uint8_t>(section.size);
			std::memcpy(model.sections[i], file.data() + section.offset, section.size);
		}

		const auto& mainSection = header.sectionHeader[Game::SECTION_MAIN];
		const auto* const main = model.sections[Game::SECTION_MAIN];

		for (int i = mainSection.fixupStart; i < mainSection.fixupStart + mainSection.fixupCount; ++i)
		{
			const auto fixup = ReadField<std::uint32_t>(fixups, static_cast<std::uint32_t>(i * sizeof(std::uint32_t)));
			const std::uint32_t at = fixup >> 3;
			const std::uint32_t target = fixup & 3;

			if (target == Game::SECTION_FIXUP || at + sizeof(std::uint32_t) > model.sizes[Game::SECTION_MAIN])
			{
				return false;
			}

			model.pointers[at] = { target, ReadField<std::uint32_t>(main, at) };
		}

		if (model.sizes[Game::SECTION_MAIN] < modelSurfsSize)
		{
			return false;
		}

		auto* const surfs = allocator.Allocate<Game::XModelSurfs>();
		surfs->name = allocator.DuplicateString(name);
		surfs->numsurfs = ReadField<unsigned short>(main, 8);
		std::memcpy(surfs->partBits, main + 12, sizeof(surfs->partBits));

		const std::uint8_t* records = nullptr;

		if (!surfs->numsurfs || !TryResolve(model, main + 4, Game::SECTION_MAIN, static_cast<std::uint64_t>(surfaceSize) * surfs->numsurfs, &records) || !records)
		{
			return false;
		}

		surfs->surfs = allocator.AllocateArray<Game::XSurface>(surfs->numsurfs);

		for (int i = 0; i < surfs->numsurfs; ++i)
		{
			const auto* const source = records + surfaceSize * i;
			auto& surface = surfs->surfs[i];

			surface.tileMode = source[0];
			surface.deformed = source[1] != 0;
			surface.vertCount = ReadField<unsigned short>(source, 2);
			surface.triCount = ReadField<unsigned short>(source, 4);
			surface.zoneHandle = cloneZoneHandle;
			surface.baseTriIndex = ReadField<unsigned short>(source, 8);
			surface.baseVertIndex = ReadField<unsigned short>(source, 12);
			std::memcpy(surface.vertInfo.vertCount, source + 20, sizeof(surface.vertInfo.vertCount));
			surface.vertListCount = ReadField<unsigned int>(source, 40);
			std::memcpy(surface.partBits, source + 52, sizeof(surface.partBits));

			const auto& vertCounts = surface.vertInfo.vertCount;
			const std::int64_t blendCount = vertCounts[0] + 3 * vertCounts[1] + 5 * vertCounts[2] + 7 * vertCounts[3];

			if (!surface.vertCount || !surface.triCount || blendCount < 0)
			{
				return false;
			}

			const bool areArraysValid = TryResolve(model, source + 16, Game::SECTION_INDEX, surface.triCount * 3ull * sizeof(unsigned short), &surface.triIndices)
				&& TryResolve(model, source + 28, Game::SECTION_MAIN, blendCount * sizeof(unsigned short), &surface.vertInfo.vertsBlend)
				&& TryResolve(model, source + 32, Game::SECTION_VERTEX, surface.vertCount * sizeof(Game::GfxPackedVertex), &surface.verts0);

			if (!areArraysValid || !surface.triIndices || !surface.verts0)
			{
				return false;
			}

			const std::uint8_t* lists = nullptr;

			if (!TryResolve(model, source + 44, Game::SECTION_MAIN, static_cast<std::uint64_t>(vertListSize) * surface.vertListCount, &lists))
			{
				return false;
			}

			if (!lists)
			{
				surface.vertListCount = 0;
				continue;
			}

			surface.vertList = allocator.AllocateArray<Game::XRigidVertList>(surface.vertListCount);

			for (unsigned int j = 0; j < surface.vertListCount; ++j)
			{
				const auto* const list = lists + vertListSize * j;
				auto& vertList = surface.vertList[j];

				vertList.boneOffset = ReadField<unsigned short>(list, 0);
				vertList.vertCount = ReadField<unsigned short>(list, 2);
				vertList.triOffset = ReadField<unsigned short>(list, 4);
				vertList.triCount = ReadField<unsigned short>(list, 6);

				const std::uint8_t* tree = nullptr;

				if (!TryResolve(model, list + 8, Game::SECTION_MAIN, collisionTreeSize, &tree))
				{
					return false;
				}

				if (!tree)
				{
					continue;
				}

				auto* const collisionTree = allocator.Allocate<Game::XSurfaceCollisionTree>();
				std::memcpy(collisionTree->trans, tree, sizeof(collisionTree->trans));
				std::memcpy(collisionTree->scale, tree + 12, sizeof(collisionTree->scale));
				collisionTree->nodeCount = ReadField<unsigned int>(tree, 24);
				collisionTree->leafCount = ReadField<unsigned int>(tree, 32);

				const bool isTreeValid = TryResolve(model, tree + 28, Game::SECTION_MAIN, collisionTree->nodeCount * sizeof(Game::XSurfaceCollisionNode), &collisionTree->nodes)
					&& TryResolve(model, tree + 36, Game::SECTION_MAIN, collisionTree->leafCount * sizeof(Game::XSurfaceCollisionLeaf), &collisionTree->leafs);

				if (!isTreeValid)
				{
					return false;
				}

				vertList.collisionTree = collisionTree;
			}
		}

		record.surfs = surfs;
		return true;
	}

	static void BuildEmptySurfaces(const std::string& name, LoadedSurfs& record)
	{
		auto& allocator = record.allocator;

		auto* const indices = allocator.AllocateArray<unsigned short>(3);
		indices[1] = 1;
		indices[2] = 2;

		auto* const vertList = allocator.Allocate<Game::XRigidVertList>();
		vertList->vertCount = 3;
		vertList->triCount = 1;

		auto* const surface = allocator.Allocate<Game::XSurface>();
		surface->vertCount = 3;
		surface->triCount = 1;
		surface->zoneHandle = cloneZoneHandle;
		surface->triIndices = indices;
		surface->verts0 = allocator.AllocateArray<Game::GfxPackedVertex>(3);
		surface->vertListCount = 1;
		surface->vertList = vertList;

		auto* const surfs = allocator.Allocate<Game::XModelSurfs>();
		surfs->name = allocator.DuplicateString(name);
		surfs->surfs = surface;
		surfs->numsurfs = 1;

		record.surfs = surfs;
	}

	static const Game::XModelSurfs* RegisterLoaded(std::unique_ptr<LoadedSurfs> record)
	{
		CreateBuffers(record->surfs);

		const auto* const surfs = record->surfs;

		std::lock_guard lock(loadedMutex);
		loaded[surfs->surfs] = std::move(record);

		return surfs;
	}

	static const Game::XModelSurfs* LoadXModelSurfs(const std::string& name)
	{
		auto record = std::make_unique<LoadedSurfs>();

		if (!TryLoadXModelSurfaces(name, *record))
		{
			Logger::Error("modelsurfs: models/{} could not be read, its model is drawn empty\n", name);

			record = std::make_unique<LoadedSurfs>();
			BuildEmptySurfaces(name, *record);
		}

		return RegisterLoaded(std::move(record));
	}

	static void DB_XModelSurfsFixup_Hk(Game::XModel* model, void* userData)
	{
		for (int lod = 0; lod < model->numLods; ++lod)
		{
			auto* const modelSurfs = model->lodInfo[lod].modelSurfs;

			if (!modelSurfs || modelSurfs->surfs)
			{
				continue;
			}

			const auto* const loadedSurfs = LoadXModelSurfs(modelSurfs->name);

			modelSurfs->surfs = loadedSurfs->surfs;
			modelSurfs->numsurfs = loadedSurfs->numsurfs;
			std::memcpy(modelSurfs->partBits, loadedSurfs->partBits, sizeof(modelSurfs->partBits));
		}

		xmodelSurfsFixup(model, userData);
	}

	static void ReleaseModelSurf(Game::XModelSurfs* modelSurfs)
	{
		std::unique_ptr<LoadedSurfs> record;

		{
			std::lock_guard lock(loadedMutex);

			const auto entry = loaded.find(modelSurfs->surfs);

			if (entry == loaded.end())
			{
				return;
			}

			record = std::move(entry->second);
			loaded.erase(entry);
		}

		ReleaseBuffers(record->surfs);
	}

	bool ModelSurfs::IsInstalled()
	{
		return isInstalled;
	}

	bool ModelSurfs::TryLoadMissing(Game::XModelSurfs* modelSurfs, const char* name)
	{
		if (!isInstalled)
		{
			return false;
		}

		auto record = std::make_unique<LoadedSurfs>();

		if (!TryLoadXModelSurfaces(name, *record))
		{
			return false;
		}

		const auto* const loadedSurfs = RegisterLoaded(std::move(record));

		modelSurfs->surfs = loadedSurfs->surfs;
		modelSurfs->numsurfs = loadedSurfs->numsurfs;
		std::memcpy(modelSurfs->partBits, loadedSurfs->partBits, sizeof(modelSurfs->partBits));

		return true;
	}

	Game::XModelSurfs* ModelSurfs::CloneAndScaleSurfaces(const Game::XModelSurfs* source, const std::string& name, const float scale)
	{
		if (!isInstalled || !source || !source->surfs || source->numsurfs <= 0)
		{
			return nullptr;
		}

		auto* clone = cloneAllocator.Allocate<Game::XModelSurfs>();
		std::memcpy(clone, source, sizeof(Game::XModelSurfs));
		clone->name = cloneAllocator.DuplicateString(name);
		clone->surfs = cloneAllocator.AllocateArray<Game::XSurface>(source->numsurfs);
		std::memcpy(clone->surfs, source->surfs, sizeof(Game::XSurface) * source->numsurfs);

		for (int surfaceIndex = 0; surfaceIndex < source->numsurfs; ++surfaceIndex)
		{
			const auto& sourceSurface = source->surfs[surfaceIndex];
			auto& cloneSurface = clone->surfs[surfaceIndex];

			if (sourceSurface.verts0 && sourceSurface.vertCount > 0)
			{
				const auto vertexBufferSize = sourceSurface.vertCount * sizeof(Game::GfxPackedVertex);
				cloneSurface.verts0 = static_cast<Game::GfxPackedVertex*>(Utils::Memory::AllocateAlign(vertexBufferSize, 16));
				cloneAllocator.Reference(cloneSurface.verts0, static_cast<void(*)(void*)>(Utils::Memory::FreeAlign));
			}

			if (sourceSurface.deformed)
			{
				continue;
			}

			cloneSurface.zoneHandle = cloneZoneHandle;

			if (sourceSurface.triIndices && sourceSurface.triCount > 0)
			{
				const auto indexCount = sourceSurface.triCount * 3;
				cloneSurface.triIndices = cloneAllocator.AllocateArray<unsigned short>(indexCount);
				std::memcpy(cloneSurface.triIndices, sourceSurface.triIndices, sizeof(unsigned short) * indexCount);
			}
		}

		clones.push_back(clone);

		UpdateScaledSurfaces(clone, source, scale);
		CreateBuffers(clone);

		return clone;
	}

	void ModelSurfs::UpdateScaledSurfaces(Game::XModelSurfs* target, const Game::XModelSurfs* source, const float scale)
	{
		if (!target || !source || !target->surfs || !source->surfs)
		{
			return;
		}

		const auto surfaceCount = std::min(target->numsurfs, source->numsurfs);

		for (int surfaceIndex = 0; surfaceIndex < surfaceCount; ++surfaceIndex)
		{
			const auto& sourceSurface = source->surfs[surfaceIndex];
			auto& targetSurface = target->surfs[surfaceIndex];

			if (!sourceSurface.verts0 || !targetSurface.verts0 || sourceSurface.vertCount <= 0)
			{
				continue;
			}

			for (int vertexIndex = 0; vertexIndex < sourceSurface.vertCount; ++vertexIndex)
			{
				targetSurface.verts0[vertexIndex] = sourceSurface.verts0[vertexIndex];
				targetSurface.verts0[vertexIndex].xyz[0] *= scale;
				targetSurface.verts0[vertexIndex].xyz[1] *= scale;
				targetSurface.verts0[vertexIndex].xyz[2] *= scale;
			}

			if (sourceSurface.deformed || targetSurface.deformed)
			{
				continue;
			}

			auto* const vertexBuffer = static_cast<IDirect3DVertexBuffer9*>(FindBuffer(targetSurface.verts0));

			if (!vertexBuffer)
			{
				continue;
			}

			void* lockedBuffer = nullptr;
			const auto bufferSize = static_cast<UINT>(sourceSurface.vertCount * sizeof(Game::GfxPackedVertex));

			if (SUCCEEDED(vertexBuffer->Lock(0, bufferSize, &lockedBuffer, 0)) && lockedBuffer)
			{
				std::memcpy(lockedBuffer, targetSurface.verts0, bufferSize);
				vertexBuffer->Unlock();
			}
		}
	}

	void ModelSurfs::FreeClones()
	{
		for (const auto* clone : clones)
		{
			ReleaseBuffers(clone);
		}

		clones.clear();
		cloneAllocator.Clear();
	}

	ModelSurfs::ModelSurfs()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t callee;
			void* replacement;
		};

		std::vector<HookSite> sites;

		for (const auto site : indexBufferAndBaseCalls)
		{
			sites.push_back({ site, DB_GetIndexBufferAndBase, reinterpret_cast<void*>(DB_GetIndexBufferAndBase_Hook) });
		}

		for (const auto site : vertexBufferAndOffsetCalls)
		{
			sites.push_back({ site, DB_GetVertexBufferAndOffset, reinterpret_cast<void*>(DB_GetVertexBufferAndOffset_Hook) });
		}

		for (const auto site : indexBufferCalls)
		{
			sites.push_back({ site, DB_GetIndexBuffer, reinterpret_cast<void*>(DB_GetIndexBuffer_Hook) });
		}

		for (const auto site : baseIndexCalls)
		{
			sites.push_back({ site, DB_GetBaseIndex, reinterpret_cast<void*>(DB_GetBaseIndex_Hook) });
		}

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.callee, false))
			{
				Logger::Error("modelsurfs: 0x{:X} no longer calls 0x{:X}, models cannot be resized\n", hookSite.site, hookSite.callee);
				return;
			}
		}

		if (!Utils::Hook::BranchesTo(Load_XModelAsset_SurfsFixupCall, DB_XModelSurfsFixup, false))
		{
			Logger::Error("modelsurfs: 0x{:X} no longer calls DB_XModelSurfsFixup, iw4x model surfaces cannot load\n", Load_XModelAsset_SurfsFixupCall);
			return;
		}

		auto* const releaseHandlers = reinterpret_cast<DB_ReleaseXAssetHandler_t*>(Utils::Hook::Rebase(DB_ReleaseXAssetHandler));

		if (releaseHandlers[Game::ASSET_TYPE_XMODEL_SURFS])
		{
			Logger::Error("modelsurfs: the xmodelsurfs release handler is already taken, iw4x model surfaces cannot load\n");
			return;
		}

		getIndexBufferAndBase = reinterpret_cast<DB_GetIndexBufferAndBase_t>(Utils::Hook::Rebase(DB_GetIndexBufferAndBase));
		getVertexBufferAndOffset = reinterpret_cast<DB_GetVertexBufferAndOffset_t>(Utils::Hook::Rebase(DB_GetVertexBufferAndOffset));
		getIndexBuffer = reinterpret_cast<DB_GetIndexBuffer_t>(Utils::Hook::Rebase(DB_GetIndexBuffer));
		getBaseIndex = reinterpret_cast<DB_GetBaseIndex_t>(Utils::Hook::Rebase(DB_GetBaseIndex));
		xmodelSurfsFixup = reinterpret_cast<DB_XModelSurfsFixup_t>(Utils::Hook::Rebase(DB_XModelSurfsFixup));

		bool isSeated = surfsFixupHook.Initialize(Load_XModelAsset_SurfsFixupCall, reinterpret_cast<void*>(DB_XModelSurfsFixup_Hk), HOOK_CALL)->Install()->IsInstalled();

		for (std::size_t i = 0; i < hookCount; ++i)
		{
			isSeated = hooks[i].Initialize(sites[i].site, sites[i].replacement, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			surfsFixupHook.Uninstall();

			Logger::Error("modelsurfs: could not seat every zone buffer hook, models cannot be resized\n");
			return;
		}

		for (auto& hook : hooks)
		{
			hook.Quick();
		}

		surfsFixupHook.Quick();
		releaseHandlers[Game::ASSET_TYPE_XMODEL_SURFS] = ReleaseModelSurf;

		isInstalled = true;

		Renderer::OnDeviceRecoveryBegin(BeginRecover);
		Renderer::OnDeviceRecoveryEnd(EndRecover);
	}
}
