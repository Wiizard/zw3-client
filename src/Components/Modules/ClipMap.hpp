#pragma once

namespace Components::ClipMap
{
	constexpr std::uintptr_t cm_numMaterials = 0x141BD31A8;
	constexpr std::uintptr_t cm_materials = 0x141BD31B0;
	constexpr std::uintptr_t cm_leafbrushNodesCount = 0x141BD31F8;
	constexpr std::uintptr_t cm_leafbrushNodes = 0x141BD3200;
	constexpr std::uintptr_t cm_numSubModels = 0x141BD3280;
	constexpr std::uintptr_t cm_cmodels = 0x141BD3288;
	constexpr std::uintptr_t cm_numBrushes = 0x141BD3290;
	constexpr std::uintptr_t cm_brushes = 0x141BD3298;
	constexpr std::uintptr_t cm_brushBounds = 0x141BD32A0;
	constexpr std::uintptr_t cm_brushContents = 0x141BD32A8;

	constexpr std::uintptr_t cm_numStaticModels = 0x141BD3198;
	constexpr std::uintptr_t cm_staticModelList = 0x141BD31A0;

	constexpr int contentsSolid = 1;

	struct cStaticModel_s
	{
		Game::XModel* xmodel;
		float origin[3];
		float invScaledAxis[3][3];
		Game::Bounds absBounds;
	};

	AssertSize(cStaticModel_s, 0x50);

#pragma pack(push, 1)
	struct cLeaf_t
	{
		std::uint16_t firstCollAabbIndex;
		std::uint16_t collAabbCount;
		int brushContents;
		int terrainContents;
		Game::Bounds bounds;
		int leafBrushNode;
	};

	struct cmodel_t
	{
		Game::Bounds bounds;
		float radius;
		cLeaf_t leaf;
	};
#pragma pack(pop)

	struct cLeafBrushNode_s
	{
		std::uint8_t axis;
		std::int16_t leafBrushCount;
		int contents;
		std::uint16_t* brushes;
		char pad[0x8];
	};

	struct ClipMaterial
	{
		const char* name;
		int surfaceFlags;
		int contents;
	};

	AssertSize(cLeaf_t, 0x28);
	AssertSize(cmodel_t, 0x44);
	AssertOffset(cmodel_t, leaf, 0x1C);
	AssertSize(cLeafBrushNode_s, 0x18);
	AssertOffset(cLeafBrushNode_s, brushes, 0x8);
	AssertSize(ClipMaterial, 0x10);

	constexpr std::uintptr_t cg_entities = 0x14059EB20;

	template <typename T>
	const T* EntityAtOrigin(const T* entities, const std::size_t originOffset, const float* origin)
	{
		const auto offset = reinterpret_cast<std::uintptr_t>(origin) - reinterpret_cast<std::uintptr_t>(entities) - originOffset;

		if (offset % sizeof(T) || offset / sizeof(T) >= Game::MAX_GENTITIES)
		{
			return nullptr;
		}

		return entities + offset / sizeof(T);
	}

	inline const Game::gentity_s* ServerEntityAt(const float* origin)
	{
		return EntityAtOrigin(Game::g_entities, offsetof(Game::gentity_s, r) + offsetof(Game::entityShared_t, currentOrigin), origin);
	}

	inline const Game::centity_s* ClientEntityAt(const float* origin)
	{
		return EntityAtOrigin(reinterpret_cast<const Game::centity_s*>(Utils::Hook::Rebase(cg_entities)), offsetof(Game::centity_s, pose) + offsetof(Game::cpose_t, origin), origin);
	}

	inline int EncodeSolid(const Game::Bounds& box)
	{
		const int width = std::clamp(static_cast<int>(std::ceil(box.halfSize[0])) + 1, 1, 255);
		const int down = std::clamp(2 - static_cast<int>(std::floor(box.midPoint[2] - box.halfSize[2])), 1, 255);
		const int up = std::clamp(static_cast<int>(std::ceil(box.midPoint[2] + box.halfSize[2])) + 33, 1, 255);

		return width | (down << 8) | (up << 16);
	}

	inline std::uint16_t FindSolidMaterial()
	{
		const auto count = Utils::Hook::Get<unsigned int>(cm_numMaterials);
		const auto* const materials = Utils::Hook::Get<const ClipMaterial*>(cm_materials);

		for (unsigned int i = 0; i < count; ++i)
		{
			if (materials[i].contents & contentsSolid)
			{
				return static_cast<std::uint16_t>(i);
			}
		}

		return 0;
	}
}
