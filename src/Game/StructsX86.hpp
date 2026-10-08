#pragma once

namespace Game::X86
{
	struct PhysPreset
	{
		std::uint32_t name;
		std::int32_t type;
		float mass;
		float bounce;
		float friction;
		float bulletForceScale;
		float explosiveForceScale;
		std::uint32_t sndAliasPrefix;
		float piecesSpreadFraction;
		float piecesUpwardVelocity;
		bool tempDefaultToCylinder;
		bool perSurfaceSndAlias;
	};

	static_assert(sizeof(PhysPreset) == 0x2C);
	static_assert(offsetof(PhysPreset, name) == 0x0);
	static_assert(offsetof(PhysPreset, type) == 0x4);
	static_assert(offsetof(PhysPreset, mass) == 0x8);
	static_assert(offsetof(PhysPreset, bounce) == 0xC);
	static_assert(offsetof(PhysPreset, friction) == 0x10);
	static_assert(offsetof(PhysPreset, bulletForceScale) == 0x14);
	static_assert(offsetof(PhysPreset, explosiveForceScale) == 0x18);
	static_assert(offsetof(PhysPreset, sndAliasPrefix) == 0x1C);
	static_assert(offsetof(PhysPreset, piecesSpreadFraction) == 0x20);
	static_assert(offsetof(PhysPreset, piecesUpwardVelocity) == 0x24);
	static_assert(offsetof(PhysPreset, tempDefaultToCylinder) == 0x28);
	static_assert(offsetof(PhysPreset, perSurfaceSndAlias) == 0x29);

	inline PhysPreset Convert(const Game::PhysPreset& from)
	{
		PhysPreset to{};
		to.type = static_cast<std::int32_t>(from.type);
		to.mass = static_cast<float>(from.mass);
		to.bounce = static_cast<float>(from.bounce);
		to.friction = static_cast<float>(from.friction);
		to.bulletForceScale = static_cast<float>(from.bulletForceScale);
		to.explosiveForceScale = static_cast<float>(from.explosiveForceScale);
		to.piecesSpreadFraction = static_cast<float>(from.piecesSpreadFraction);
		to.piecesUpwardVelocity = static_cast<float>(from.piecesUpwardVelocity);
		to.tempDefaultToCylinder = static_cast<bool>(from.tempDefaultToCylinder);
		to.perSurfaceSndAlias = static_cast<bool>(from.perSurfaceSndAlias);
		return to;
	}

	inline Game::PhysPreset Convert(const PhysPreset& from)
	{
		Game::PhysPreset to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.mass = static_cast<decltype(to.mass)>(from.mass);
		to.bounce = static_cast<decltype(to.bounce)>(from.bounce);
		to.friction = static_cast<decltype(to.friction)>(from.friction);
		to.bulletForceScale = static_cast<decltype(to.bulletForceScale)>(from.bulletForceScale);
		to.explosiveForceScale = static_cast<decltype(to.explosiveForceScale)>(from.explosiveForceScale);
		to.piecesSpreadFraction = static_cast<decltype(to.piecesSpreadFraction)>(from.piecesSpreadFraction);
		to.piecesUpwardVelocity = static_cast<decltype(to.piecesUpwardVelocity)>(from.piecesUpwardVelocity);
		to.tempDefaultToCylinder = static_cast<decltype(to.tempDefaultToCylinder)>(from.tempDefaultToCylinder);
		to.perSurfaceSndAlias = static_cast<decltype(to.perSurfaceSndAlias)>(from.perSurfaceSndAlias);
		return to;
	}

	struct PhysMass
	{
		float centerOfMass[3];
		float momentsOfInertia[3];
		float productsOfInertia[3];
	};

	static_assert(sizeof(PhysMass) == 0x24);
	static_assert(offsetof(PhysMass, centerOfMass) == 0x0);
	static_assert(offsetof(PhysMass, momentsOfInertia) == 0xC);
	static_assert(offsetof(PhysMass, productsOfInertia) == 0x18);

	inline PhysMass Convert(const Game::PhysMass& from)
	{
		PhysMass to{};
		std::memcpy(to.centerOfMass, from.centerOfMass, sizeof(to.centerOfMass));
		std::memcpy(to.momentsOfInertia, from.momentsOfInertia, sizeof(to.momentsOfInertia));
		std::memcpy(to.productsOfInertia, from.productsOfInertia, sizeof(to.productsOfInertia));
		return to;
	}

	inline Game::PhysMass Convert(const PhysMass& from)
	{
		Game::PhysMass to{};
		std::memcpy(to.centerOfMass, from.centerOfMass, sizeof(from.centerOfMass));
		std::memcpy(to.momentsOfInertia, from.momentsOfInertia, sizeof(from.momentsOfInertia));
		std::memcpy(to.productsOfInertia, from.productsOfInertia, sizeof(from.productsOfInertia));
		return to;
	}

	struct Bounds
	{
		float midPoint[3];
		float halfSize[3];
	};

	static_assert(sizeof(Bounds) == 0x18);
	static_assert(offsetof(Bounds, midPoint) == 0x0);
	static_assert(offsetof(Bounds, halfSize) == 0xC);

	inline Bounds Convert(const Game::Bounds& from)
	{
		Bounds to{};
		std::memcpy(to.midPoint, from.midPoint, sizeof(to.midPoint));
		std::memcpy(to.halfSize, from.halfSize, sizeof(to.halfSize));
		return to;
	}

	inline Game::Bounds Convert(const Bounds& from)
	{
		Game::Bounds to{};
		std::memcpy(to.midPoint, from.midPoint, sizeof(from.midPoint));
		std::memcpy(to.halfSize, from.halfSize, sizeof(from.halfSize));
		return to;
	}

	struct PhysCollmap
	{
		std::uint32_t name;
		std::uint32_t count;
		std::uint32_t geoms;
		PhysMass mass;
		Bounds bounds;
	};

	static_assert(sizeof(PhysCollmap) == 0x48);
	static_assert(offsetof(PhysCollmap, name) == 0x0);
	static_assert(offsetof(PhysCollmap, count) == 0x4);
	static_assert(offsetof(PhysCollmap, geoms) == 0x8);
	static_assert(offsetof(PhysCollmap, mass) == 0xC);
	static_assert(offsetof(PhysCollmap, bounds) == 0x30);

	inline PhysCollmap Convert(const Game::PhysCollmap& from)
	{
		PhysCollmap to{};
		to.count = static_cast<std::uint32_t>(from.count);
		to.mass = Convert(from.mass);
		to.bounds = Convert(from.bounds);
		return to;
	}

	inline Game::PhysCollmap Convert(const PhysCollmap& from)
	{
		Game::PhysCollmap to{};
		to.count = static_cast<decltype(to.count)>(from.count);
		to.mass = Convert(from.mass);
		to.bounds = Convert(from.bounds);
		return to;
	}

	struct PhysGeomInfo
	{
		std::uint32_t brushWrapper;
		std::int32_t type;
		float orientation[3][3];
		Bounds bounds;
	};

	static_assert(sizeof(PhysGeomInfo) == 0x44);
	static_assert(offsetof(PhysGeomInfo, brushWrapper) == 0x0);
	static_assert(offsetof(PhysGeomInfo, type) == 0x4);
	static_assert(offsetof(PhysGeomInfo, orientation) == 0x8);
	static_assert(offsetof(PhysGeomInfo, bounds) == 0x2C);

	inline PhysGeomInfo Convert(const Game::PhysGeomInfo& from)
	{
		PhysGeomInfo to{};
		to.type = static_cast<std::int32_t>(from.type);
		std::memcpy(to.orientation, from.orientation, sizeof(to.orientation));
		to.bounds = Convert(from.bounds);
		return to;
	}

	inline Game::PhysGeomInfo Convert(const PhysGeomInfo& from)
	{
		Game::PhysGeomInfo to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		std::memcpy(to.orientation, from.orientation, sizeof(from.orientation));
		to.bounds = Convert(from.bounds);
		return to;
	}

	struct cbrush_t
	{
		std::uint16_t numsides;
		std::uint16_t glassPieceIndex;
		std::uint32_t sides;
		std::uint32_t baseAdjacentSide;
		std::uint16_t axialMaterialNum[2][3];
		std::uint8_t firstAdjacentSideOffsets[2][3];
		std::uint8_t edgeCount[2][3];
	};

	static_assert(sizeof(cbrush_t) == 0x24);
	static_assert(offsetof(cbrush_t, numsides) == 0x0);
	static_assert(offsetof(cbrush_t, glassPieceIndex) == 0x2);
	static_assert(offsetof(cbrush_t, sides) == 0x4);
	static_assert(offsetof(cbrush_t, baseAdjacentSide) == 0x8);
	static_assert(offsetof(cbrush_t, axialMaterialNum) == 0xC);
	static_assert(offsetof(cbrush_t, firstAdjacentSideOffsets) == 0x18);
	static_assert(offsetof(cbrush_t, edgeCount) == 0x1E);

	inline cbrush_t Convert(const Game::cbrush_t& from)
	{
		cbrush_t to{};
		to.numsides = static_cast<std::uint16_t>(from.numsides);
		to.glassPieceIndex = static_cast<std::uint16_t>(from.glassPieceIndex);
		std::memcpy(to.axialMaterialNum, from.axialMaterialNum, sizeof(to.axialMaterialNum));
		std::memcpy(to.firstAdjacentSideOffsets, from.firstAdjacentSideOffsets, sizeof(to.firstAdjacentSideOffsets));
		std::memcpy(to.edgeCount, from.edgeCount, sizeof(to.edgeCount));
		return to;
	}

	inline Game::cbrush_t Convert(const cbrush_t& from)
	{
		Game::cbrush_t to{};
		to.numsides = static_cast<decltype(to.numsides)>(from.numsides);
		to.glassPieceIndex = static_cast<decltype(to.glassPieceIndex)>(from.glassPieceIndex);
		std::memcpy(to.axialMaterialNum, from.axialMaterialNum, sizeof(from.axialMaterialNum));
		std::memcpy(to.firstAdjacentSideOffsets, from.firstAdjacentSideOffsets, sizeof(from.firstAdjacentSideOffsets));
		std::memcpy(to.edgeCount, from.edgeCount, sizeof(from.edgeCount));
		return to;
	}

	struct BrushWrapper
	{
		Bounds bounds;
		cbrush_t brush;
		std::int32_t totalEdgeCount;
		std::uint32_t planes;
	};

	static_assert(sizeof(BrushWrapper) == 0x44);
	static_assert(offsetof(BrushWrapper, bounds) == 0x0);
	static_assert(offsetof(BrushWrapper, brush) == 0x18);
	static_assert(offsetof(BrushWrapper, totalEdgeCount) == 0x3C);
	static_assert(offsetof(BrushWrapper, planes) == 0x40);

	inline BrushWrapper Convert(const Game::BrushWrapper& from)
	{
		BrushWrapper to{};
		to.bounds = Convert(from.bounds);
		to.brush = Convert(from.brush);
		to.totalEdgeCount = static_cast<std::int32_t>(from.totalEdgeCount);
		return to;
	}

	inline Game::BrushWrapper Convert(const BrushWrapper& from)
	{
		Game::BrushWrapper to{};
		to.bounds = Convert(from.bounds);
		to.brush = Convert(from.brush);
		to.totalEdgeCount = static_cast<decltype(to.totalEdgeCount)>(from.totalEdgeCount);
		return to;
	}

	struct cbrushside_t
	{
		std::uint32_t plane;
		std::uint16_t materialNum;
		std::int8_t firstAdjacentSideOffset;
		std::int8_t edgeCount;
	};

	static_assert(sizeof(cbrushside_t) == 0x8);
	static_assert(offsetof(cbrushside_t, plane) == 0x0);
	static_assert(offsetof(cbrushside_t, materialNum) == 0x4);
	static_assert(offsetof(cbrushside_t, firstAdjacentSideOffset) == 0x6);
	static_assert(offsetof(cbrushside_t, edgeCount) == 0x7);

	inline cbrushside_t Convert(const Game::cbrushside_t& from)
	{
		cbrushside_t to{};
		to.materialNum = static_cast<std::uint16_t>(from.materialNum);
		to.firstAdjacentSideOffset = static_cast<std::int8_t>(from.firstAdjacentSideOffset);
		to.edgeCount = static_cast<std::int8_t>(from.edgeCount);
		return to;
	}

	inline Game::cbrushside_t Convert(const cbrushside_t& from)
	{
		Game::cbrushside_t to{};
		to.materialNum = static_cast<decltype(to.materialNum)>(from.materialNum);
		to.firstAdjacentSideOffset = static_cast<decltype(to.firstAdjacentSideOffset)>(from.firstAdjacentSideOffset);
		to.edgeCount = static_cast<decltype(to.edgeCount)>(from.edgeCount);
		return to;
	}

	struct cplane_s
	{
		float normal[3];
		float dist;
		std::uint8_t type;
		std::uint8_t pad[3];
	};

	static_assert(sizeof(cplane_s) == 0x14);
	static_assert(offsetof(cplane_s, normal) == 0x0);
	static_assert(offsetof(cplane_s, dist) == 0xC);
	static_assert(offsetof(cplane_s, type) == 0x10);
	static_assert(offsetof(cplane_s, pad) == 0x11);

	inline cplane_s Convert(const Game::cplane_s& from)
	{
		cplane_s to{};
		std::memcpy(to.normal, from.normal, sizeof(to.normal));
		to.dist = static_cast<float>(from.dist);
		to.type = static_cast<std::uint8_t>(from.type);
		std::memcpy(to.pad, from.pad, sizeof(to.pad));
		return to;
	}

	inline Game::cplane_s Convert(const cplane_s& from)
	{
		Game::cplane_s to{};
		std::memcpy(to.normal, from.normal, sizeof(from.normal));
		to.dist = static_cast<decltype(to.dist)>(from.dist);
		to.type = static_cast<decltype(to.type)>(from.type);
		std::memcpy(to.pad, from.pad, sizeof(from.pad));
		return to;
	}

	union XAnimIndices
	{
		std::uint32_t _1;
		std::uint32_t _2;
		std::uint32_t data;
	};

	static_assert(sizeof(XAnimIndices) == 0x4);
	static_assert(offsetof(XAnimIndices, _1) == 0x0);
	static_assert(offsetof(XAnimIndices, _2) == 0x0);
	static_assert(offsetof(XAnimIndices, data) == 0x0);

	inline XAnimIndices Convert(const Game::XAnimIndices& from)
	{
		XAnimIndices to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::XAnimIndices Convert(const XAnimIndices& from)
	{
		Game::XAnimIndices to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct XAnimParts
	{
		std::uint32_t name;
		std::uint16_t dataByteCount;
		std::uint16_t dataShortCount;
		std::uint16_t dataIntCount;
		std::uint16_t randomDataByteCount;
		std::uint16_t randomDataIntCount;
		std::uint16_t numframes;
		std::int8_t flags;
		std::uint8_t boneCount[10];
		std::uint8_t notifyCount;
		std::int8_t assetType;
		bool isDefault;
		std::uint32_t randomDataShortCount;
		std::uint32_t indexCount;
		float framerate;
		float frequency;
		std::uint32_t names;
		std::uint32_t dataByte;
		std::uint32_t dataShort;
		std::uint32_t dataInt;
		std::uint32_t randomDataShort;
		std::uint32_t randomDataByte;
		std::uint32_t randomDataInt;
		XAnimIndices indices;
		std::uint32_t notify;
		std::uint32_t deltaPart;
	};

	static_assert(sizeof(XAnimParts) == 0x58);
	static_assert(offsetof(XAnimParts, name) == 0x0);
	static_assert(offsetof(XAnimParts, dataByteCount) == 0x4);
	static_assert(offsetof(XAnimParts, dataShortCount) == 0x6);
	static_assert(offsetof(XAnimParts, dataIntCount) == 0x8);
	static_assert(offsetof(XAnimParts, randomDataByteCount) == 0xA);
	static_assert(offsetof(XAnimParts, randomDataIntCount) == 0xC);
	static_assert(offsetof(XAnimParts, numframes) == 0xE);
	static_assert(offsetof(XAnimParts, flags) == 0x10);
	static_assert(offsetof(XAnimParts, boneCount) == 0x11);
	static_assert(offsetof(XAnimParts, notifyCount) == 0x1B);
	static_assert(offsetof(XAnimParts, assetType) == 0x1C);
	static_assert(offsetof(XAnimParts, isDefault) == 0x1D);
	static_assert(offsetof(XAnimParts, randomDataShortCount) == 0x20);
	static_assert(offsetof(XAnimParts, indexCount) == 0x24);
	static_assert(offsetof(XAnimParts, framerate) == 0x28);
	static_assert(offsetof(XAnimParts, frequency) == 0x2C);
	static_assert(offsetof(XAnimParts, names) == 0x30);
	static_assert(offsetof(XAnimParts, dataByte) == 0x34);
	static_assert(offsetof(XAnimParts, dataShort) == 0x38);
	static_assert(offsetof(XAnimParts, dataInt) == 0x3C);
	static_assert(offsetof(XAnimParts, randomDataShort) == 0x40);
	static_assert(offsetof(XAnimParts, randomDataByte) == 0x44);
	static_assert(offsetof(XAnimParts, randomDataInt) == 0x48);
	static_assert(offsetof(XAnimParts, indices) == 0x4C);
	static_assert(offsetof(XAnimParts, notify) == 0x50);
	static_assert(offsetof(XAnimParts, deltaPart) == 0x54);

	inline XAnimParts Convert(const Game::XAnimParts& from)
	{
		XAnimParts to{};
		to.dataByteCount = static_cast<std::uint16_t>(from.dataByteCount);
		to.dataShortCount = static_cast<std::uint16_t>(from.dataShortCount);
		to.dataIntCount = static_cast<std::uint16_t>(from.dataIntCount);
		to.randomDataByteCount = static_cast<std::uint16_t>(from.randomDataByteCount);
		to.randomDataIntCount = static_cast<std::uint16_t>(from.randomDataIntCount);
		to.numframes = static_cast<std::uint16_t>(from.numframes);
		to.flags = static_cast<std::int8_t>(from.flags);
		std::memcpy(to.boneCount, from.boneCount, sizeof(to.boneCount));
		to.notifyCount = static_cast<std::uint8_t>(from.notifyCount);
		to.assetType = static_cast<std::int8_t>(from.assetType);
		to.isDefault = static_cast<bool>(from.isDefault);
		to.randomDataShortCount = static_cast<std::uint32_t>(from.randomDataShortCount);
		to.indexCount = static_cast<std::uint32_t>(from.indexCount);
		to.framerate = static_cast<float>(from.framerate);
		to.frequency = static_cast<float>(from.frequency);
		to.indices = Convert(from.indices);
		return to;
	}

	inline Game::XAnimParts Convert(const XAnimParts& from)
	{
		Game::XAnimParts to{};
		to.dataByteCount = static_cast<decltype(to.dataByteCount)>(from.dataByteCount);
		to.dataShortCount = static_cast<decltype(to.dataShortCount)>(from.dataShortCount);
		to.dataIntCount = static_cast<decltype(to.dataIntCount)>(from.dataIntCount);
		to.randomDataByteCount = static_cast<decltype(to.randomDataByteCount)>(from.randomDataByteCount);
		to.randomDataIntCount = static_cast<decltype(to.randomDataIntCount)>(from.randomDataIntCount);
		to.numframes = static_cast<decltype(to.numframes)>(from.numframes);
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		std::memcpy(to.boneCount, from.boneCount, sizeof(from.boneCount));
		to.notifyCount = static_cast<decltype(to.notifyCount)>(from.notifyCount);
		to.assetType = static_cast<decltype(to.assetType)>(from.assetType);
		to.isDefault = static_cast<decltype(to.isDefault)>(from.isDefault);
		to.randomDataShortCount = static_cast<decltype(to.randomDataShortCount)>(from.randomDataShortCount);
		to.indexCount = static_cast<decltype(to.indexCount)>(from.indexCount);
		to.framerate = static_cast<decltype(to.framerate)>(from.framerate);
		to.frequency = static_cast<decltype(to.frequency)>(from.frequency);
		to.indices = Convert(from.indices);
		return to;
	}

	struct XAnimNotifyInfo
	{
		std::uint16_t name;
		float time;
	};

	static_assert(sizeof(XAnimNotifyInfo) == 0x8);
	static_assert(offsetof(XAnimNotifyInfo, name) == 0x0);
	static_assert(offsetof(XAnimNotifyInfo, time) == 0x4);

	inline XAnimNotifyInfo Convert(const Game::XAnimNotifyInfo& from)
	{
		XAnimNotifyInfo to{};
		to.name = static_cast<std::uint16_t>(from.name);
		to.time = static_cast<float>(from.time);
		return to;
	}

	inline Game::XAnimNotifyInfo Convert(const XAnimNotifyInfo& from)
	{
		Game::XAnimNotifyInfo to{};
		to.name = static_cast<decltype(to.name)>(from.name);
		to.time = static_cast<decltype(to.time)>(from.time);
		return to;
	}

	struct XAnimDeltaPart
	{
		std::uint32_t trans;
		std::uint32_t quat2;
		std::uint32_t quat;
	};

	static_assert(sizeof(XAnimDeltaPart) == 0xC);
	static_assert(offsetof(XAnimDeltaPart, trans) == 0x0);
	static_assert(offsetof(XAnimDeltaPart, quat2) == 0x4);
	static_assert(offsetof(XAnimDeltaPart, quat) == 0x8);

	inline XAnimDeltaPart Convert(const Game::XAnimDeltaPart&)
	{
		XAnimDeltaPart to{};
		return to;
	}

	inline Game::XAnimDeltaPart Convert(const XAnimDeltaPart&)
	{
		Game::XAnimDeltaPart to{};
		return to;
	}

	union XAnimDynamicFrames
	{
		std::uint32_t _1;
		std::uint32_t _2;
	};

	static_assert(sizeof(XAnimDynamicFrames) == 0x4);
	static_assert(offsetof(XAnimDynamicFrames, _1) == 0x0);
	static_assert(offsetof(XAnimDynamicFrames, _2) == 0x0);

	inline XAnimDynamicFrames Convert(const Game::XAnimDynamicFrames& from)
	{
		XAnimDynamicFrames to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::XAnimDynamicFrames Convert(const XAnimDynamicFrames& from)
	{
		Game::XAnimDynamicFrames to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	union XAnimDynamicIndices
	{
		std::int8_t _1[1];
		std::uint16_t _2[1];
	};

	static_assert(sizeof(XAnimDynamicIndices) == 0x2);
	static_assert(offsetof(XAnimDynamicIndices, _1) == 0x0);
	static_assert(offsetof(XAnimDynamicIndices, _2) == 0x0);

	inline XAnimDynamicIndices Convert(const Game::XAnimDynamicIndices& from)
	{
		XAnimDynamicIndices to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::XAnimDynamicIndices Convert(const XAnimDynamicIndices& from)
	{
		Game::XAnimDynamicIndices to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct XAnimPartTransFrames
	{
		float mins[3];
		float size[3];
		XAnimDynamicFrames frames;
		XAnimDynamicIndices indices;
	};

	static_assert(sizeof(XAnimPartTransFrames) == 0x20);
	static_assert(offsetof(XAnimPartTransFrames, mins) == 0x0);
	static_assert(offsetof(XAnimPartTransFrames, size) == 0xC);
	static_assert(offsetof(XAnimPartTransFrames, frames) == 0x18);
	static_assert(offsetof(XAnimPartTransFrames, indices) == 0x1C);

	inline XAnimPartTransFrames Convert(const Game::XAnimPartTransFrames& from)
	{
		XAnimPartTransFrames to{};
		std::memcpy(to.mins, from.mins, sizeof(to.mins));
		std::memcpy(to.size, from.size, sizeof(to.size));
		to.frames = Convert(from.frames);
		to.indices = Convert(from.indices);
		return to;
	}

	inline Game::XAnimPartTransFrames Convert(const XAnimPartTransFrames& from)
	{
		Game::XAnimPartTransFrames to{};
		std::memcpy(to.mins, from.mins, sizeof(from.mins));
		std::memcpy(to.size, from.size, sizeof(from.size));
		to.frames = Convert(from.frames);
		to.indices = Convert(from.indices);
		return to;
	}

	union XAnimPartTransData
	{
		XAnimPartTransFrames frames;
		float frame0[3];
	};

	static_assert(sizeof(XAnimPartTransData) == 0x20);
	static_assert(offsetof(XAnimPartTransData, frames) == 0x0);
	static_assert(offsetof(XAnimPartTransData, frame0) == 0x0);

	inline XAnimPartTransData Convert(const Game::XAnimPartTransData& from)
	{
		XAnimPartTransData to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::XAnimPartTransData Convert(const XAnimPartTransData& from)
	{
		Game::XAnimPartTransData to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct XAnimPartTrans
	{
		std::uint16_t size;
		std::int8_t smallTrans;
		XAnimPartTransData u;
	};

	static_assert(sizeof(XAnimPartTrans) == 0x24);
	static_assert(offsetof(XAnimPartTrans, size) == 0x0);
	static_assert(offsetof(XAnimPartTrans, smallTrans) == 0x2);
	static_assert(offsetof(XAnimPartTrans, u) == 0x4);

	inline XAnimPartTrans Convert(const Game::XAnimPartTrans& from)
	{
		XAnimPartTrans to{};
		to.size = static_cast<std::uint16_t>(from.size);
		to.smallTrans = static_cast<std::int8_t>(from.smallTrans);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::XAnimPartTrans Convert(const XAnimPartTrans& from)
	{
		Game::XAnimPartTrans to{};
		to.size = static_cast<decltype(to.size)>(from.size);
		to.smallTrans = static_cast<decltype(to.smallTrans)>(from.smallTrans);
		to.u = Convert(from.u);
		return to;
	}

	struct XAnimDeltaPartQuatDataFrames2
	{
		std::uint32_t frames;
		XAnimDynamicIndices indices;
	};

	static_assert(sizeof(XAnimDeltaPartQuatDataFrames2) == 0x8);
	static_assert(offsetof(XAnimDeltaPartQuatDataFrames2, frames) == 0x0);
	static_assert(offsetof(XAnimDeltaPartQuatDataFrames2, indices) == 0x4);

	inline XAnimDeltaPartQuatDataFrames2 Convert(const Game::XAnimDeltaPartQuatDataFrames2& from)
	{
		XAnimDeltaPartQuatDataFrames2 to{};
		to.indices = Convert(from.indices);
		return to;
	}

	inline Game::XAnimDeltaPartQuatDataFrames2 Convert(const XAnimDeltaPartQuatDataFrames2& from)
	{
		Game::XAnimDeltaPartQuatDataFrames2 to{};
		to.indices = Convert(from.indices);
		return to;
	}

	union XAnimDeltaPartQuatData2
	{
		XAnimDeltaPartQuatDataFrames2 frames;
		std::int16_t frame0[2];
	};

	static_assert(sizeof(XAnimDeltaPartQuatData2) == 0x8);
	static_assert(offsetof(XAnimDeltaPartQuatData2, frames) == 0x0);
	static_assert(offsetof(XAnimDeltaPartQuatData2, frame0) == 0x0);

	inline XAnimDeltaPartQuatData2 Convert(const Game::XAnimDeltaPartQuatData2& from)
	{
		XAnimDeltaPartQuatData2 to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::XAnimDeltaPartQuatData2 Convert(const XAnimDeltaPartQuatData2& from)
	{
		Game::XAnimDeltaPartQuatData2 to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct XAnimDeltaPartQuat2
	{
		std::uint16_t size;
		XAnimDeltaPartQuatData2 u;
	};

	static_assert(sizeof(XAnimDeltaPartQuat2) == 0xC);
	static_assert(offsetof(XAnimDeltaPartQuat2, size) == 0x0);
	static_assert(offsetof(XAnimDeltaPartQuat2, u) == 0x4);

	inline XAnimDeltaPartQuat2 Convert(const Game::XAnimDeltaPartQuat2& from)
	{
		XAnimDeltaPartQuat2 to{};
		to.size = static_cast<std::uint16_t>(from.size);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::XAnimDeltaPartQuat2 Convert(const XAnimDeltaPartQuat2& from)
	{
		Game::XAnimDeltaPartQuat2 to{};
		to.size = static_cast<decltype(to.size)>(from.size);
		to.u = Convert(from.u);
		return to;
	}

	struct XAnimDeltaPartQuatDataFrames
	{
		std::uint32_t frames;
		XAnimDynamicIndices indices;
	};

	static_assert(sizeof(XAnimDeltaPartQuatDataFrames) == 0x8);
	static_assert(offsetof(XAnimDeltaPartQuatDataFrames, frames) == 0x0);
	static_assert(offsetof(XAnimDeltaPartQuatDataFrames, indices) == 0x4);

	inline XAnimDeltaPartQuatDataFrames Convert(const Game::XAnimDeltaPartQuatDataFrames& from)
	{
		XAnimDeltaPartQuatDataFrames to{};
		to.indices = Convert(from.indices);
		return to;
	}

	inline Game::XAnimDeltaPartQuatDataFrames Convert(const XAnimDeltaPartQuatDataFrames& from)
	{
		Game::XAnimDeltaPartQuatDataFrames to{};
		to.indices = Convert(from.indices);
		return to;
	}

	union XAnimDeltaPartQuatData
	{
		XAnimDeltaPartQuatDataFrames frames;
		std::int16_t frame0[4];
	};

	static_assert(sizeof(XAnimDeltaPartQuatData) == 0x8);
	static_assert(offsetof(XAnimDeltaPartQuatData, frames) == 0x0);
	static_assert(offsetof(XAnimDeltaPartQuatData, frame0) == 0x0);

	inline XAnimDeltaPartQuatData Convert(const Game::XAnimDeltaPartQuatData& from)
	{
		XAnimDeltaPartQuatData to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::XAnimDeltaPartQuatData Convert(const XAnimDeltaPartQuatData& from)
	{
		Game::XAnimDeltaPartQuatData to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct XAnimDeltaPartQuat
	{
		std::uint16_t size;
		XAnimDeltaPartQuatData u;
	};

	static_assert(sizeof(XAnimDeltaPartQuat) == 0xC);
	static_assert(offsetof(XAnimDeltaPartQuat, size) == 0x0);
	static_assert(offsetof(XAnimDeltaPartQuat, u) == 0x4);

	inline XAnimDeltaPartQuat Convert(const Game::XAnimDeltaPartQuat& from)
	{
		XAnimDeltaPartQuat to{};
		to.size = static_cast<std::uint16_t>(from.size);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::XAnimDeltaPartQuat Convert(const XAnimDeltaPartQuat& from)
	{
		Game::XAnimDeltaPartQuat to{};
		to.size = static_cast<decltype(to.size)>(from.size);
		to.u = Convert(from.u);
		return to;
	}

	struct XModelSurfs
	{
		std::uint32_t name;
		std::uint32_t surfs;
		std::uint16_t numsurfs;
		std::int32_t partBits[6];
	};

	static_assert(sizeof(XModelSurfs) == 0x24);
	static_assert(offsetof(XModelSurfs, name) == 0x0);
	static_assert(offsetof(XModelSurfs, surfs) == 0x4);
	static_assert(offsetof(XModelSurfs, numsurfs) == 0x8);
	static_assert(offsetof(XModelSurfs, partBits) == 0xC);

	inline XModelSurfs Convert(const Game::XModelSurfs& from)
	{
		XModelSurfs to{};
		to.numsurfs = static_cast<std::uint16_t>(from.numsurfs);
		std::memcpy(to.partBits, from.partBits, sizeof(to.partBits));
		return to;
	}

	inline Game::XModelSurfs Convert(const XModelSurfs& from)
	{
		Game::XModelSurfs to{};
		to.numsurfs = static_cast<decltype(to.numsurfs)>(from.numsurfs);
		std::memcpy(to.partBits, from.partBits, sizeof(from.partBits));
		return to;
	}

	struct XSurfaceVertexInfo
	{
		std::uint16_t vertCount[4];
		std::uint32_t vertsBlend;
	};

	static_assert(sizeof(XSurfaceVertexInfo) == 0xC);
	static_assert(offsetof(XSurfaceVertexInfo, vertCount) == 0x0);
	static_assert(offsetof(XSurfaceVertexInfo, vertsBlend) == 0x8);

	inline XSurfaceVertexInfo Convert(const Game::XSurfaceVertexInfo& from)
	{
		XSurfaceVertexInfo to{};
		std::memcpy(to.vertCount, from.vertCount, sizeof(to.vertCount));
		return to;
	}

	inline Game::XSurfaceVertexInfo Convert(const XSurfaceVertexInfo& from)
	{
		Game::XSurfaceVertexInfo to{};
		std::memcpy(to.vertCount, from.vertCount, sizeof(from.vertCount));
		return to;
	}

	struct XSurface
	{
		std::int8_t tileMode;
		bool deformed;
		std::uint16_t vertCount;
		std::uint16_t triCount;
		std::int8_t zoneHandle;
		std::uint16_t baseTriIndex;
		std::uint16_t baseVertIndex;
		std::uint32_t triIndices;
		XSurfaceVertexInfo vertInfo;
		std::uint32_t verts0;
		std::uint32_t vertListCount;
		std::uint32_t vertList;
		std::int32_t partBits[6];
	};

	static_assert(sizeof(XSurface) == 0x40);
	static_assert(offsetof(XSurface, tileMode) == 0x0);
	static_assert(offsetof(XSurface, deformed) == 0x1);
	static_assert(offsetof(XSurface, vertCount) == 0x2);
	static_assert(offsetof(XSurface, triCount) == 0x4);
	static_assert(offsetof(XSurface, zoneHandle) == 0x6);
	static_assert(offsetof(XSurface, baseTriIndex) == 0x8);
	static_assert(offsetof(XSurface, baseVertIndex) == 0xA);
	static_assert(offsetof(XSurface, triIndices) == 0xC);
	static_assert(offsetof(XSurface, vertInfo) == 0x10);
	static_assert(offsetof(XSurface, verts0) == 0x1C);
	static_assert(offsetof(XSurface, vertListCount) == 0x20);
	static_assert(offsetof(XSurface, vertList) == 0x24);
	static_assert(offsetof(XSurface, partBits) == 0x28);

	inline XSurface Convert(const Game::XSurface& from)
	{
		XSurface to{};
		to.tileMode = static_cast<std::int8_t>(from.tileMode);
		to.deformed = static_cast<bool>(from.deformed);
		to.vertCount = static_cast<std::uint16_t>(from.vertCount);
		to.triCount = static_cast<std::uint16_t>(from.triCount);
		to.zoneHandle = static_cast<std::int8_t>(from.zoneHandle);
		to.baseTriIndex = static_cast<std::uint16_t>(from.baseTriIndex);
		to.baseVertIndex = static_cast<std::uint16_t>(from.baseVertIndex);
		to.vertInfo = Convert(from.vertInfo);
		to.vertListCount = static_cast<std::uint32_t>(from.vertListCount);
		std::memcpy(to.partBits, from.partBits, sizeof(to.partBits));
		return to;
	}

	inline Game::XSurface Convert(const XSurface& from)
	{
		Game::XSurface to{};
		to.tileMode = static_cast<decltype(to.tileMode)>(from.tileMode);
		to.deformed = static_cast<decltype(to.deformed)>(from.deformed);
		to.vertCount = static_cast<decltype(to.vertCount)>(from.vertCount);
		to.triCount = static_cast<decltype(to.triCount)>(from.triCount);
		to.zoneHandle = static_cast<decltype(to.zoneHandle)>(from.zoneHandle);
		to.baseTriIndex = static_cast<decltype(to.baseTriIndex)>(from.baseTriIndex);
		to.baseVertIndex = static_cast<decltype(to.baseVertIndex)>(from.baseVertIndex);
		to.vertInfo = Convert(from.vertInfo);
		to.vertListCount = static_cast<decltype(to.vertListCount)>(from.vertListCount);
		std::memcpy(to.partBits, from.partBits, sizeof(from.partBits));
		return to;
	}

	union GfxColor
	{
		std::uint32_t packed;
		std::uint8_t array[4];
	};

	static_assert(sizeof(GfxColor) == 0x4);
	static_assert(offsetof(GfxColor, packed) == 0x0);
	static_assert(offsetof(GfxColor, array) == 0x0);

	inline GfxColor Convert(const Game::GfxColor& from)
	{
		GfxColor to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::GfxColor Convert(const GfxColor& from)
	{
		Game::GfxColor to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	union PackedTexCoords
	{
		std::uint32_t packed;
	};

	static_assert(sizeof(PackedTexCoords) == 0x4);
	static_assert(offsetof(PackedTexCoords, packed) == 0x0);

	inline PackedTexCoords Convert(const Game::PackedTexCoords& from)
	{
		PackedTexCoords to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::PackedTexCoords Convert(const PackedTexCoords& from)
	{
		Game::PackedTexCoords to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	union PackedUnitVec
	{
		std::uint32_t packed;
		std::int8_t array[4];
	};

	static_assert(sizeof(PackedUnitVec) == 0x4);
	static_assert(offsetof(PackedUnitVec, packed) == 0x0);
	static_assert(offsetof(PackedUnitVec, array) == 0x0);

	inline PackedUnitVec Convert(const Game::PackedUnitVec& from)
	{
		PackedUnitVec to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::PackedUnitVec Convert(const PackedUnitVec& from)
	{
		Game::PackedUnitVec to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct GfxPackedVertex
	{
		float xyz[3];
		float binormalSign;
		GfxColor color;
		PackedTexCoords texCoord;
		PackedUnitVec normal;
		PackedUnitVec tangent;
	};

	static_assert(sizeof(GfxPackedVertex) == 0x20);
	static_assert(offsetof(GfxPackedVertex, xyz) == 0x0);
	static_assert(offsetof(GfxPackedVertex, binormalSign) == 0xC);
	static_assert(offsetof(GfxPackedVertex, color) == 0x10);
	static_assert(offsetof(GfxPackedVertex, texCoord) == 0x14);
	static_assert(offsetof(GfxPackedVertex, normal) == 0x18);
	static_assert(offsetof(GfxPackedVertex, tangent) == 0x1C);

	inline GfxPackedVertex Convert(const Game::GfxPackedVertex& from)
	{
		GfxPackedVertex to{};
		std::memcpy(to.xyz, from.xyz, sizeof(to.xyz));
		to.binormalSign = static_cast<float>(from.binormalSign);
		to.color = Convert(from.color);
		to.texCoord = Convert(from.texCoord);
		to.normal = Convert(from.normal);
		to.tangent = Convert(from.tangent);
		return to;
	}

	inline Game::GfxPackedVertex Convert(const GfxPackedVertex& from)
	{
		Game::GfxPackedVertex to{};
		std::memcpy(to.xyz, from.xyz, sizeof(from.xyz));
		to.binormalSign = static_cast<decltype(to.binormalSign)>(from.binormalSign);
		to.color = Convert(from.color);
		to.texCoord = Convert(from.texCoord);
		to.normal = Convert(from.normal);
		to.tangent = Convert(from.tangent);
		return to;
	}

	struct XRigidVertList
	{
		std::uint16_t boneOffset;
		std::uint16_t vertCount;
		std::uint16_t triOffset;
		std::uint16_t triCount;
		std::uint32_t collisionTree;
	};

	static_assert(sizeof(XRigidVertList) == 0xC);
	static_assert(offsetof(XRigidVertList, boneOffset) == 0x0);
	static_assert(offsetof(XRigidVertList, vertCount) == 0x2);
	static_assert(offsetof(XRigidVertList, triOffset) == 0x4);
	static_assert(offsetof(XRigidVertList, triCount) == 0x6);
	static_assert(offsetof(XRigidVertList, collisionTree) == 0x8);

	inline XRigidVertList Convert(const Game::XRigidVertList& from)
	{
		XRigidVertList to{};
		to.boneOffset = static_cast<std::uint16_t>(from.boneOffset);
		to.vertCount = static_cast<std::uint16_t>(from.vertCount);
		to.triOffset = static_cast<std::uint16_t>(from.triOffset);
		to.triCount = static_cast<std::uint16_t>(from.triCount);
		return to;
	}

	inline Game::XRigidVertList Convert(const XRigidVertList& from)
	{
		Game::XRigidVertList to{};
		to.boneOffset = static_cast<decltype(to.boneOffset)>(from.boneOffset);
		to.vertCount = static_cast<decltype(to.vertCount)>(from.vertCount);
		to.triOffset = static_cast<decltype(to.triOffset)>(from.triOffset);
		to.triCount = static_cast<decltype(to.triCount)>(from.triCount);
		return to;
	}

	struct XSurfaceCollisionTree
	{
		float trans[3];
		float scale[3];
		std::uint32_t nodeCount;
		std::uint32_t nodes;
		std::uint32_t leafCount;
		std::uint32_t leafs;
	};

	static_assert(sizeof(XSurfaceCollisionTree) == 0x28);
	static_assert(offsetof(XSurfaceCollisionTree, trans) == 0x0);
	static_assert(offsetof(XSurfaceCollisionTree, scale) == 0xC);
	static_assert(offsetof(XSurfaceCollisionTree, nodeCount) == 0x18);
	static_assert(offsetof(XSurfaceCollisionTree, nodes) == 0x1C);
	static_assert(offsetof(XSurfaceCollisionTree, leafCount) == 0x20);
	static_assert(offsetof(XSurfaceCollisionTree, leafs) == 0x24);

	inline XSurfaceCollisionTree Convert(const Game::XSurfaceCollisionTree& from)
	{
		XSurfaceCollisionTree to{};
		std::memcpy(to.trans, from.trans, sizeof(to.trans));
		std::memcpy(to.scale, from.scale, sizeof(to.scale));
		to.nodeCount = static_cast<std::uint32_t>(from.nodeCount);
		to.leafCount = static_cast<std::uint32_t>(from.leafCount);
		return to;
	}

	inline Game::XSurfaceCollisionTree Convert(const XSurfaceCollisionTree& from)
	{
		Game::XSurfaceCollisionTree to{};
		std::memcpy(to.trans, from.trans, sizeof(from.trans));
		std::memcpy(to.scale, from.scale, sizeof(from.scale));
		to.nodeCount = static_cast<decltype(to.nodeCount)>(from.nodeCount);
		to.leafCount = static_cast<decltype(to.leafCount)>(from.leafCount);
		return to;
	}

	struct XSurfaceCollisionAabb
	{
		std::uint16_t mins[3];
		std::uint16_t maxs[3];
	};

	static_assert(sizeof(XSurfaceCollisionAabb) == 0xC);
	static_assert(offsetof(XSurfaceCollisionAabb, mins) == 0x0);
	static_assert(offsetof(XSurfaceCollisionAabb, maxs) == 0x6);

	inline XSurfaceCollisionAabb Convert(const Game::XSurfaceCollisionAabb& from)
	{
		XSurfaceCollisionAabb to{};
		std::memcpy(to.mins, from.mins, sizeof(to.mins));
		std::memcpy(to.maxs, from.maxs, sizeof(to.maxs));
		return to;
	}

	inline Game::XSurfaceCollisionAabb Convert(const XSurfaceCollisionAabb& from)
	{
		Game::XSurfaceCollisionAabb to{};
		std::memcpy(to.mins, from.mins, sizeof(from.mins));
		std::memcpy(to.maxs, from.maxs, sizeof(from.maxs));
		return to;
	}

	struct XSurfaceCollisionNode
	{
		XSurfaceCollisionAabb aabb;
		std::uint16_t childBeginIndex;
		std::uint16_t childCount;
	};

	static_assert(sizeof(XSurfaceCollisionNode) == 0x10);
	static_assert(offsetof(XSurfaceCollisionNode, aabb) == 0x0);
	static_assert(offsetof(XSurfaceCollisionNode, childBeginIndex) == 0xC);
	static_assert(offsetof(XSurfaceCollisionNode, childCount) == 0xE);

	inline XSurfaceCollisionNode Convert(const Game::XSurfaceCollisionNode& from)
	{
		XSurfaceCollisionNode to{};
		to.aabb = Convert(from.aabb);
		to.childBeginIndex = static_cast<std::uint16_t>(from.childBeginIndex);
		to.childCount = static_cast<std::uint16_t>(from.childCount);
		return to;
	}

	inline Game::XSurfaceCollisionNode Convert(const XSurfaceCollisionNode& from)
	{
		Game::XSurfaceCollisionNode to{};
		to.aabb = Convert(from.aabb);
		to.childBeginIndex = static_cast<decltype(to.childBeginIndex)>(from.childBeginIndex);
		to.childCount = static_cast<decltype(to.childCount)>(from.childCount);
		return to;
	}

	struct XSurfaceCollisionLeaf
	{
		std::uint16_t triangleBeginIndex;
	};

	static_assert(sizeof(XSurfaceCollisionLeaf) == 0x2);
	static_assert(offsetof(XSurfaceCollisionLeaf, triangleBeginIndex) == 0x0);

	inline XSurfaceCollisionLeaf Convert(const Game::XSurfaceCollisionLeaf& from)
	{
		XSurfaceCollisionLeaf to{};
		to.triangleBeginIndex = static_cast<std::uint16_t>(from.triangleBeginIndex);
		return to;
	}

	inline Game::XSurfaceCollisionLeaf Convert(const XSurfaceCollisionLeaf& from)
	{
		Game::XSurfaceCollisionLeaf to{};
		to.triangleBeginIndex = static_cast<decltype(to.triangleBeginIndex)>(from.triangleBeginIndex);
		return to;
	}

	struct XModelLodInfo
	{
		float dist;
		std::uint16_t numsurfs;
		std::uint16_t surfIndex;
		std::uint32_t modelSurfs;
		std::int32_t partBits[6];
		std::uint32_t surfs;
		std::int8_t lod;
		std::int8_t smcBaseIndexPlusOne;
		std::int8_t smcSubIndexMask;
		std::int8_t smcBucket;
	};

	static_assert(sizeof(XModelLodInfo) == 0x2C);
	static_assert(offsetof(XModelLodInfo, dist) == 0x0);
	static_assert(offsetof(XModelLodInfo, numsurfs) == 0x4);
	static_assert(offsetof(XModelLodInfo, surfIndex) == 0x6);
	static_assert(offsetof(XModelLodInfo, modelSurfs) == 0x8);
	static_assert(offsetof(XModelLodInfo, partBits) == 0xC);
	static_assert(offsetof(XModelLodInfo, surfs) == 0x24);
	static_assert(offsetof(XModelLodInfo, lod) == 0x28);
	static_assert(offsetof(XModelLodInfo, smcBaseIndexPlusOne) == 0x29);
	static_assert(offsetof(XModelLodInfo, smcSubIndexMask) == 0x2A);
	static_assert(offsetof(XModelLodInfo, smcBucket) == 0x2B);

	inline XModelLodInfo Convert(const Game::XModelLodInfo& from)
	{
		XModelLodInfo to{};
		to.dist = static_cast<float>(from.dist);
		to.numsurfs = static_cast<std::uint16_t>(from.numsurfs);
		to.surfIndex = static_cast<std::uint16_t>(from.surfIndex);
		std::memcpy(to.partBits, from.partBits, sizeof(to.partBits));
		to.lod = static_cast<std::int8_t>(from.lod);
		to.smcBaseIndexPlusOne = static_cast<std::int8_t>(from.smcBaseIndexPlusOne);
		to.smcSubIndexMask = static_cast<std::int8_t>(from.smcSubIndexMask);
		to.smcBucket = static_cast<std::int8_t>(from.smcBucket);
		return to;
	}

	inline Game::XModelLodInfo Convert(const XModelLodInfo& from)
	{
		Game::XModelLodInfo to{};
		to.dist = static_cast<decltype(to.dist)>(from.dist);
		to.numsurfs = static_cast<decltype(to.numsurfs)>(from.numsurfs);
		to.surfIndex = static_cast<decltype(to.surfIndex)>(from.surfIndex);
		std::memcpy(to.partBits, from.partBits, sizeof(from.partBits));
		to.lod = static_cast<decltype(to.lod)>(from.lod);
		to.smcBaseIndexPlusOne = static_cast<decltype(to.smcBaseIndexPlusOne)>(from.smcBaseIndexPlusOne);
		to.smcSubIndexMask = static_cast<decltype(to.smcSubIndexMask)>(from.smcSubIndexMask);
		to.smcBucket = static_cast<decltype(to.smcBucket)>(from.smcBucket);
		return to;
	}

	struct XModel
	{
		std::uint32_t name;
		std::uint8_t numBones;
		std::uint8_t numRootBones;
		std::uint8_t numsurfs;
		std::uint8_t lodRampType;
		float scale;
		std::uint32_t noScalePartBits[6];
		std::uint32_t boneNames;
		std::uint32_t parentList;
		std::uint32_t quats;
		std::uint32_t trans;
		std::uint32_t partClassification;
		std::uint32_t baseMat;
		std::uint32_t materialHandles;
		XModelLodInfo lodInfo[4];
		std::uint8_t maxLoadedLod;
		std::uint8_t numLods;
		std::uint8_t collLod;
		std::uint8_t flags;
		std::uint32_t collSurfs;
		std::int32_t numCollSurfs;
		std::int32_t contents;
		std::uint32_t boneInfo;
		float radius;
		Bounds bounds;
		std::int32_t memUsage;
		bool bad;
		std::uint32_t physPreset;
		std::uint32_t physCollmap;
	};

	static_assert(sizeof(XModel) == 0x130);
	static_assert(offsetof(XModel, name) == 0x0);
	static_assert(offsetof(XModel, numBones) == 0x4);
	static_assert(offsetof(XModel, numRootBones) == 0x5);
	static_assert(offsetof(XModel, numsurfs) == 0x6);
	static_assert(offsetof(XModel, lodRampType) == 0x7);
	static_assert(offsetof(XModel, scale) == 0x8);
	static_assert(offsetof(XModel, noScalePartBits) == 0xC);
	static_assert(offsetof(XModel, boneNames) == 0x24);
	static_assert(offsetof(XModel, parentList) == 0x28);
	static_assert(offsetof(XModel, quats) == 0x2C);
	static_assert(offsetof(XModel, trans) == 0x30);
	static_assert(offsetof(XModel, partClassification) == 0x34);
	static_assert(offsetof(XModel, baseMat) == 0x38);
	static_assert(offsetof(XModel, materialHandles) == 0x3C);
	static_assert(offsetof(XModel, lodInfo) == 0x40);
	static_assert(offsetof(XModel, maxLoadedLod) == 0xF0);
	static_assert(offsetof(XModel, numLods) == 0xF1);
	static_assert(offsetof(XModel, collLod) == 0xF2);
	static_assert(offsetof(XModel, flags) == 0xF3);
	static_assert(offsetof(XModel, collSurfs) == 0xF4);
	static_assert(offsetof(XModel, numCollSurfs) == 0xF8);
	static_assert(offsetof(XModel, contents) == 0xFC);
	static_assert(offsetof(XModel, boneInfo) == 0x100);
	static_assert(offsetof(XModel, radius) == 0x104);
	static_assert(offsetof(XModel, bounds) == 0x108);
	static_assert(offsetof(XModel, memUsage) == 0x120);
	static_assert(offsetof(XModel, bad) == 0x124);
	static_assert(offsetof(XModel, physPreset) == 0x128);
	static_assert(offsetof(XModel, physCollmap) == 0x12C);

	inline XModel Convert(const Game::XModel& from)
	{
		XModel to{};
		to.numBones = static_cast<std::uint8_t>(from.numBones);
		to.numRootBones = static_cast<std::uint8_t>(from.numRootBones);
		to.numsurfs = static_cast<std::uint8_t>(from.numsurfs);
		to.lodRampType = static_cast<std::uint8_t>(from.lodRampType);
		to.scale = static_cast<float>(from.scale);
		std::memcpy(to.noScalePartBits, from.noScalePartBits, sizeof(to.noScalePartBits));
		for (std::size_t i = 0; i < std::size(to.lodInfo); ++i)
		{
			to.lodInfo[i] = Convert(from.lodInfo[i]);
		}
		to.maxLoadedLod = static_cast<std::uint8_t>(from.maxLoadedLod);
		to.numLods = static_cast<std::uint8_t>(from.numLods);
		to.collLod = static_cast<std::uint8_t>(from.collLod);
		to.flags = static_cast<std::uint8_t>(from.flags);
		to.numCollSurfs = static_cast<std::int32_t>(from.numCollSurfs);
		to.contents = static_cast<std::int32_t>(from.contents);
		to.radius = static_cast<float>(from.radius);
		to.bounds = Convert(from.bounds);
		to.memUsage = static_cast<std::int32_t>(from.memUsage);
		to.bad = static_cast<bool>(from.bad);
		return to;
	}

	inline Game::XModel Convert(const XModel& from)
	{
		Game::XModel to{};
		to.numBones = static_cast<decltype(to.numBones)>(from.numBones);
		to.numRootBones = static_cast<decltype(to.numRootBones)>(from.numRootBones);
		to.numsurfs = static_cast<decltype(to.numsurfs)>(from.numsurfs);
		to.lodRampType = static_cast<decltype(to.lodRampType)>(from.lodRampType);
		to.scale = static_cast<decltype(to.scale)>(from.scale);
		std::memcpy(to.noScalePartBits, from.noScalePartBits, sizeof(from.noScalePartBits));
		for (std::size_t i = 0; i < std::size(to.lodInfo); ++i)
		{
			to.lodInfo[i] = Convert(from.lodInfo[i]);
		}
		to.maxLoadedLod = static_cast<decltype(to.maxLoadedLod)>(from.maxLoadedLod);
		to.numLods = static_cast<decltype(to.numLods)>(from.numLods);
		to.collLod = static_cast<decltype(to.collLod)>(from.collLod);
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.numCollSurfs = static_cast<decltype(to.numCollSurfs)>(from.numCollSurfs);
		to.contents = static_cast<decltype(to.contents)>(from.contents);
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		to.bounds = Convert(from.bounds);
		to.memUsage = static_cast<decltype(to.memUsage)>(from.memUsage);
		to.bad = static_cast<decltype(to.bad)>(from.bad);
		return to;
	}

	struct DObjAnimMat
	{
		float quat[4];
		float trans[3];
		float transWeight;
	};

	static_assert(sizeof(DObjAnimMat) == 0x20);
	static_assert(offsetof(DObjAnimMat, quat) == 0x0);
	static_assert(offsetof(DObjAnimMat, trans) == 0x10);
	static_assert(offsetof(DObjAnimMat, transWeight) == 0x1C);

	inline DObjAnimMat Convert(const Game::DObjAnimMat& from)
	{
		DObjAnimMat to{};
		std::memcpy(to.quat, from.quat, sizeof(to.quat));
		std::memcpy(to.trans, from.trans, sizeof(to.trans));
		to.transWeight = static_cast<float>(from.transWeight);
		return to;
	}

	inline Game::DObjAnimMat Convert(const DObjAnimMat& from)
	{
		Game::DObjAnimMat to{};
		std::memcpy(to.quat, from.quat, sizeof(from.quat));
		std::memcpy(to.trans, from.trans, sizeof(from.trans));
		to.transWeight = static_cast<decltype(to.transWeight)>(from.transWeight);
		return to;
	}

	struct GfxDrawSurfFields
	{
		std::uint64_t objectId : 16;
		std::uint64_t reflectionProbeIndex : 8;
		std::uint64_t hasGfxEntIndex : 1;
		std::uint64_t customIndex : 5;
		std::uint64_t materialSortedIndex : 12;
		std::uint64_t prepass : 2;
		std::uint64_t useHeroLighting : 1;
		std::uint64_t sceneLightIndex : 8;
		std::uint64_t surfType : 4;
		std::uint64_t primarySortKey : 6;
		std::uint64_t unused : 1;
	};

	static_assert(sizeof(GfxDrawSurfFields) == 0x8);

	inline GfxDrawSurfFields Convert(const Game::GfxDrawSurfFields& from)
	{
		GfxDrawSurfFields to{};
		to.objectId = from.objectId;
		to.reflectionProbeIndex = from.reflectionProbeIndex;
		to.hasGfxEntIndex = from.hasGfxEntIndex;
		to.customIndex = from.customIndex;
		to.materialSortedIndex = from.materialSortedIndex;
		to.prepass = from.prepass;
		to.useHeroLighting = from.useHeroLighting;
		to.sceneLightIndex = from.sceneLightIndex;
		to.surfType = from.surfType;
		to.primarySortKey = from.primarySortKey;
		to.unused = from.unused;
		return to;
	}

	inline Game::GfxDrawSurfFields Convert(const GfxDrawSurfFields& from)
	{
		Game::GfxDrawSurfFields to{};
		to.objectId = from.objectId;
		to.reflectionProbeIndex = from.reflectionProbeIndex;
		to.hasGfxEntIndex = from.hasGfxEntIndex;
		to.customIndex = from.customIndex;
		to.materialSortedIndex = from.materialSortedIndex;
		to.prepass = from.prepass;
		to.useHeroLighting = from.useHeroLighting;
		to.sceneLightIndex = from.sceneLightIndex;
		to.surfType = from.surfType;
		to.primarySortKey = from.primarySortKey;
		to.unused = from.unused;
		return to;
	}

	union GfxDrawSurf
	{
		GfxDrawSurfFields fields;
		std::uint64_t packed;
	};

	static_assert(sizeof(GfxDrawSurf) == 0x8);
	static_assert(offsetof(GfxDrawSurf, fields) == 0x0);
	static_assert(offsetof(GfxDrawSurf, packed) == 0x0);

	inline GfxDrawSurf Convert(const Game::GfxDrawSurf& from)
	{
		GfxDrawSurf to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::GfxDrawSurf Convert(const GfxDrawSurf& from)
	{
		Game::GfxDrawSurf to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct MaterialInfo
	{
		std::uint32_t name;
		std::uint8_t gameFlags;
		std::uint8_t sortKey;
		std::uint8_t textureAtlasRowCount;
		std::uint8_t textureAtlasColumnCount;
		GfxDrawSurf drawSurf;
		std::uint32_t surfaceTypeBits;
		std::uint16_t hashIndex;
	};

	static_assert(sizeof(MaterialInfo) == 0x18);
	static_assert(offsetof(MaterialInfo, name) == 0x0);
	static_assert(offsetof(MaterialInfo, gameFlags) == 0x4);
	static_assert(offsetof(MaterialInfo, sortKey) == 0x5);
	static_assert(offsetof(MaterialInfo, textureAtlasRowCount) == 0x6);
	static_assert(offsetof(MaterialInfo, textureAtlasColumnCount) == 0x7);
	static_assert(offsetof(MaterialInfo, drawSurf) == 0x8);
	static_assert(offsetof(MaterialInfo, surfaceTypeBits) == 0x10);
	static_assert(offsetof(MaterialInfo, hashIndex) == 0x14);

	inline MaterialInfo Convert(const Game::MaterialInfo& from)
	{
		MaterialInfo to{};
		to.gameFlags = static_cast<std::uint8_t>(from.gameFlags);
		to.sortKey = static_cast<std::uint8_t>(from.sortKey);
		to.textureAtlasRowCount = static_cast<std::uint8_t>(from.textureAtlasRowCount);
		to.textureAtlasColumnCount = static_cast<std::uint8_t>(from.textureAtlasColumnCount);
		to.drawSurf = Convert(from.drawSurf);
		to.surfaceTypeBits = static_cast<std::uint32_t>(from.surfaceTypeBits);
		to.hashIndex = static_cast<std::uint16_t>(from.hashIndex);
		return to;
	}

	inline Game::MaterialInfo Convert(const MaterialInfo& from)
	{
		Game::MaterialInfo to{};
		to.gameFlags = static_cast<decltype(to.gameFlags)>(from.gameFlags);
		to.sortKey = static_cast<decltype(to.sortKey)>(from.sortKey);
		to.textureAtlasRowCount = static_cast<decltype(to.textureAtlasRowCount)>(from.textureAtlasRowCount);
		to.textureAtlasColumnCount = static_cast<decltype(to.textureAtlasColumnCount)>(from.textureAtlasColumnCount);
		to.drawSurf = Convert(from.drawSurf);
		to.surfaceTypeBits = static_cast<decltype(to.surfaceTypeBits)>(from.surfaceTypeBits);
		to.hashIndex = static_cast<decltype(to.hashIndex)>(from.hashIndex);
		return to;
	}

	struct Material
	{
		MaterialInfo info;
		std::int8_t stateBitsEntry[48];
		std::uint8_t textureCount;
		std::uint8_t constantCount;
		std::uint8_t stateBitsCount;
		std::int8_t stateFlags;
		std::int8_t cameraRegion;
		std::uint32_t techniqueSet;
		std::uint32_t textureTable;
		std::uint32_t constantTable;
		std::uint32_t stateBitsTable;
	};

	static_assert(sizeof(Material) == 0x60);
	static_assert(offsetof(Material, info) == 0x0);
	static_assert(offsetof(Material, stateBitsEntry) == 0x18);
	static_assert(offsetof(Material, textureCount) == 0x48);
	static_assert(offsetof(Material, constantCount) == 0x49);
	static_assert(offsetof(Material, stateBitsCount) == 0x4A);
	static_assert(offsetof(Material, stateFlags) == 0x4B);
	static_assert(offsetof(Material, cameraRegion) == 0x4C);
	static_assert(offsetof(Material, techniqueSet) == 0x50);
	static_assert(offsetof(Material, textureTable) == 0x54);
	static_assert(offsetof(Material, constantTable) == 0x58);
	static_assert(offsetof(Material, stateBitsTable) == 0x5C);

	inline Material Convert(const Game::Material& from)
	{
		Material to{};
		to.info = Convert(from.info);
		std::memcpy(to.stateBitsEntry, from.stateBitsEntry, sizeof(to.stateBitsEntry));
		to.textureCount = static_cast<std::uint8_t>(from.textureCount);
		to.constantCount = static_cast<std::uint8_t>(from.constantCount);
		to.stateBitsCount = static_cast<std::uint8_t>(from.stateBitsCount);
		to.stateFlags = static_cast<std::int8_t>(from.stateFlags);
		to.cameraRegion = static_cast<std::int8_t>(from.cameraRegion);
		return to;
	}

	inline Game::Material Convert(const Material& from)
	{
		Game::Material to{};
		to.info = Convert(from.info);
		std::memcpy(to.stateBitsEntry, from.stateBitsEntry, sizeof(from.stateBitsEntry));
		to.textureCount = static_cast<decltype(to.textureCount)>(from.textureCount);
		to.constantCount = static_cast<decltype(to.constantCount)>(from.constantCount);
		to.stateBitsCount = static_cast<decltype(to.stateBitsCount)>(from.stateBitsCount);
		to.stateFlags = static_cast<decltype(to.stateFlags)>(from.stateFlags);
		to.cameraRegion = static_cast<decltype(to.cameraRegion)>(from.cameraRegion);
		return to;
	}

	struct MaterialTechniqueSet
	{
		std::uint32_t name;
		std::int8_t worldVertFormat;
		bool hasBeenUploaded;
		std::int8_t unused[1];
		std::uint32_t remappedTechniqueSet;
		std::uint32_t techniques[48];
	};

	static_assert(sizeof(MaterialTechniqueSet) == 0xCC);
	static_assert(offsetof(MaterialTechniqueSet, name) == 0x0);
	static_assert(offsetof(MaterialTechniqueSet, worldVertFormat) == 0x4);
	static_assert(offsetof(MaterialTechniqueSet, hasBeenUploaded) == 0x5);
	static_assert(offsetof(MaterialTechniqueSet, unused) == 0x6);
	static_assert(offsetof(MaterialTechniqueSet, remappedTechniqueSet) == 0x8);
	static_assert(offsetof(MaterialTechniqueSet, techniques) == 0xC);

	inline MaterialTechniqueSet Convert(const Game::MaterialTechniqueSet& from)
	{
		MaterialTechniqueSet to{};
		to.worldVertFormat = static_cast<std::int8_t>(from.worldVertFormat);
		to.hasBeenUploaded = static_cast<bool>(from.hasBeenUploaded);
		std::memcpy(to.unused, from.unused, sizeof(to.unused));
		return to;
	}

	inline Game::MaterialTechniqueSet Convert(const MaterialTechniqueSet& from)
	{
		Game::MaterialTechniqueSet to{};
		to.worldVertFormat = static_cast<decltype(to.worldVertFormat)>(from.worldVertFormat);
		to.hasBeenUploaded = static_cast<decltype(to.hasBeenUploaded)>(from.hasBeenUploaded);
		std::memcpy(to.unused, from.unused, sizeof(from.unused));
		return to;
	}

	struct MaterialPass
	{
		std::uint32_t vertexDecl;
		std::uint32_t vertexShader;
		std::uint32_t pixelShader;
		std::int8_t perPrimArgCount;
		std::int8_t perObjArgCount;
		std::int8_t stableArgCount;
		std::int8_t customSamplerFlags;
		std::uint32_t args;
	};

	static_assert(sizeof(MaterialPass) == 0x14);
	static_assert(offsetof(MaterialPass, vertexDecl) == 0x0);
	static_assert(offsetof(MaterialPass, vertexShader) == 0x4);
	static_assert(offsetof(MaterialPass, pixelShader) == 0x8);
	static_assert(offsetof(MaterialPass, perPrimArgCount) == 0xC);
	static_assert(offsetof(MaterialPass, perObjArgCount) == 0xD);
	static_assert(offsetof(MaterialPass, stableArgCount) == 0xE);
	static_assert(offsetof(MaterialPass, customSamplerFlags) == 0xF);
	static_assert(offsetof(MaterialPass, args) == 0x10);

	inline MaterialPass Convert(const Game::MaterialPass& from)
	{
		MaterialPass to{};
		to.perPrimArgCount = static_cast<std::int8_t>(from.perPrimArgCount);
		to.perObjArgCount = static_cast<std::int8_t>(from.perObjArgCount);
		to.stableArgCount = static_cast<std::int8_t>(from.stableArgCount);
		to.customSamplerFlags = static_cast<std::int8_t>(from.customSamplerFlags);
		return to;
	}

	inline Game::MaterialPass Convert(const MaterialPass& from)
	{
		Game::MaterialPass to{};
		to.perPrimArgCount = static_cast<decltype(to.perPrimArgCount)>(from.perPrimArgCount);
		to.perObjArgCount = static_cast<decltype(to.perObjArgCount)>(from.perObjArgCount);
		to.stableArgCount = static_cast<decltype(to.stableArgCount)>(from.stableArgCount);
		to.customSamplerFlags = static_cast<decltype(to.customSamplerFlags)>(from.customSamplerFlags);
		return to;
	}

	struct MaterialTechnique
	{
		std::uint32_t name;
		std::uint16_t flags;
		std::uint16_t passCount;
		MaterialPass passArray[1];
	};

	static_assert(sizeof(MaterialTechnique) == 0x1C);
	static_assert(offsetof(MaterialTechnique, name) == 0x0);
	static_assert(offsetof(MaterialTechnique, flags) == 0x4);
	static_assert(offsetof(MaterialTechnique, passCount) == 0x6);
	static_assert(offsetof(MaterialTechnique, passArray) == 0x8);

	inline MaterialTechnique Convert(const Game::MaterialTechnique& from)
	{
		MaterialTechnique to{};
		to.flags = static_cast<std::uint16_t>(from.flags);
		to.passCount = static_cast<std::uint16_t>(from.passCount);
		for (std::size_t i = 0; i < std::size(to.passArray); ++i)
		{
			to.passArray[i] = Convert(from.passArray[i]);
		}
		return to;
	}

	inline Game::MaterialTechnique Convert(const MaterialTechnique& from)
	{
		Game::MaterialTechnique to{};
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.passCount = static_cast<decltype(to.passCount)>(from.passCount);
		for (std::size_t i = 0; i < std::size(to.passArray); ++i)
		{
			to.passArray[i] = Convert(from.passArray[i]);
		}
		return to;
	}

	struct MaterialStreamRouting
	{
		std::int8_t source;
		std::int8_t dest;
	};

	static_assert(sizeof(MaterialStreamRouting) == 0x2);
	static_assert(offsetof(MaterialStreamRouting, source) == 0x0);
	static_assert(offsetof(MaterialStreamRouting, dest) == 0x1);

	inline MaterialStreamRouting Convert(const Game::MaterialStreamRouting& from)
	{
		MaterialStreamRouting to{};
		to.source = static_cast<std::int8_t>(from.source);
		to.dest = static_cast<std::int8_t>(from.dest);
		return to;
	}

	inline Game::MaterialStreamRouting Convert(const MaterialStreamRouting& from)
	{
		Game::MaterialStreamRouting to{};
		to.source = static_cast<decltype(to.source)>(from.source);
		to.dest = static_cast<decltype(to.dest)>(from.dest);
		return to;
	}

	struct MaterialVertexStreamRouting
	{
		MaterialStreamRouting data[13];
		std::uint32_t decl[16];
	};

	static_assert(sizeof(MaterialVertexStreamRouting) == 0x5C);
	static_assert(offsetof(MaterialVertexStreamRouting, data) == 0x0);
	static_assert(offsetof(MaterialVertexStreamRouting, decl) == 0x1C);

	inline MaterialVertexStreamRouting Convert(const Game::MaterialVertexStreamRouting& from)
	{
		MaterialVertexStreamRouting to{};
		for (std::size_t i = 0; i < std::size(to.data); ++i)
		{
			to.data[i] = Convert(from.data[i]);
		}
		return to;
	}

	inline Game::MaterialVertexStreamRouting Convert(const MaterialVertexStreamRouting& from)
	{
		Game::MaterialVertexStreamRouting to{};
		for (std::size_t i = 0; i < std::size(to.data); ++i)
		{
			to.data[i] = Convert(from.data[i]);
		}
		return to;
	}

	struct MaterialVertexDeclaration
	{
		std::uint32_t name;
		std::int8_t streamCount;
		bool hasOptionalSource;
		MaterialVertexStreamRouting routing;
	};

	static_assert(sizeof(MaterialVertexDeclaration) == 0x64);
	static_assert(offsetof(MaterialVertexDeclaration, name) == 0x0);
	static_assert(offsetof(MaterialVertexDeclaration, streamCount) == 0x4);
	static_assert(offsetof(MaterialVertexDeclaration, hasOptionalSource) == 0x5);
	static_assert(offsetof(MaterialVertexDeclaration, routing) == 0x8);

	inline MaterialVertexDeclaration Convert(const Game::MaterialVertexDeclaration& from)
	{
		MaterialVertexDeclaration to{};
		to.streamCount = static_cast<std::int8_t>(from.streamCount);
		to.hasOptionalSource = static_cast<bool>(from.hasOptionalSource);
		to.routing = Convert(from.routing);
		return to;
	}

	inline Game::MaterialVertexDeclaration Convert(const MaterialVertexDeclaration& from)
	{
		Game::MaterialVertexDeclaration to{};
		to.streamCount = static_cast<decltype(to.streamCount)>(from.streamCount);
		to.hasOptionalSource = static_cast<decltype(to.hasOptionalSource)>(from.hasOptionalSource);
		to.routing = Convert(from.routing);
		return to;
	}

	struct GfxVertexShaderLoadDef
	{
		std::uint32_t program;
		std::uint16_t programSize;
		std::uint16_t loadForRenderer;
	};

	static_assert(sizeof(GfxVertexShaderLoadDef) == 0x8);
	static_assert(offsetof(GfxVertexShaderLoadDef, program) == 0x0);
	static_assert(offsetof(GfxVertexShaderLoadDef, programSize) == 0x4);
	static_assert(offsetof(GfxVertexShaderLoadDef, loadForRenderer) == 0x6);

	inline GfxVertexShaderLoadDef Convert(const Game::GfxVertexShaderLoadDef& from)
	{
		GfxVertexShaderLoadDef to{};
		to.programSize = static_cast<std::uint16_t>(from.programSize);
		to.loadForRenderer = static_cast<std::uint16_t>(from.loadForRenderer);
		return to;
	}

	inline Game::GfxVertexShaderLoadDef Convert(const GfxVertexShaderLoadDef& from)
	{
		Game::GfxVertexShaderLoadDef to{};
		to.programSize = static_cast<decltype(to.programSize)>(from.programSize);
		to.loadForRenderer = static_cast<decltype(to.loadForRenderer)>(from.loadForRenderer);
		return to;
	}

	struct MaterialVertexShaderProgram
	{
		std::uint32_t vs;
		GfxVertexShaderLoadDef loadDef;
	};

	static_assert(sizeof(MaterialVertexShaderProgram) == 0xC);
	static_assert(offsetof(MaterialVertexShaderProgram, vs) == 0x0);
	static_assert(offsetof(MaterialVertexShaderProgram, loadDef) == 0x4);

	inline MaterialVertexShaderProgram Convert(const Game::MaterialVertexShaderProgram& from)
	{
		MaterialVertexShaderProgram to{};
		to.loadDef = Convert(from.loadDef);
		return to;
	}

	inline Game::MaterialVertexShaderProgram Convert(const MaterialVertexShaderProgram& from)
	{
		Game::MaterialVertexShaderProgram to{};
		to.loadDef = Convert(from.loadDef);
		return to;
	}

	struct MaterialVertexShader
	{
		std::uint32_t name;
		MaterialVertexShaderProgram prog;
	};

	static_assert(sizeof(MaterialVertexShader) == 0x10);
	static_assert(offsetof(MaterialVertexShader, name) == 0x0);
	static_assert(offsetof(MaterialVertexShader, prog) == 0x4);

	inline MaterialVertexShader Convert(const Game::MaterialVertexShader& from)
	{
		MaterialVertexShader to{};
		to.prog = Convert(from.prog);
		return to;
	}

	inline Game::MaterialVertexShader Convert(const MaterialVertexShader& from)
	{
		Game::MaterialVertexShader to{};
		to.prog = Convert(from.prog);
		return to;
	}

	struct GfxPixelShaderLoadDef
	{
		std::uint32_t program;
		std::uint16_t programSize;
		std::uint16_t loadForRenderer;
	};

	static_assert(sizeof(GfxPixelShaderLoadDef) == 0x8);
	static_assert(offsetof(GfxPixelShaderLoadDef, program) == 0x0);
	static_assert(offsetof(GfxPixelShaderLoadDef, programSize) == 0x4);
	static_assert(offsetof(GfxPixelShaderLoadDef, loadForRenderer) == 0x6);

	inline GfxPixelShaderLoadDef Convert(const Game::GfxPixelShaderLoadDef& from)
	{
		GfxPixelShaderLoadDef to{};
		to.programSize = static_cast<std::uint16_t>(from.programSize);
		to.loadForRenderer = static_cast<std::uint16_t>(from.loadForRenderer);
		return to;
	}

	inline Game::GfxPixelShaderLoadDef Convert(const GfxPixelShaderLoadDef& from)
	{
		Game::GfxPixelShaderLoadDef to{};
		to.programSize = static_cast<decltype(to.programSize)>(from.programSize);
		to.loadForRenderer = static_cast<decltype(to.loadForRenderer)>(from.loadForRenderer);
		return to;
	}

	struct MaterialPixelShaderProgram
	{
		std::uint32_t ps;
		GfxPixelShaderLoadDef loadDef;
	};

	static_assert(sizeof(MaterialPixelShaderProgram) == 0xC);
	static_assert(offsetof(MaterialPixelShaderProgram, ps) == 0x0);
	static_assert(offsetof(MaterialPixelShaderProgram, loadDef) == 0x4);

	inline MaterialPixelShaderProgram Convert(const Game::MaterialPixelShaderProgram& from)
	{
		MaterialPixelShaderProgram to{};
		to.loadDef = Convert(from.loadDef);
		return to;
	}

	inline Game::MaterialPixelShaderProgram Convert(const MaterialPixelShaderProgram& from)
	{
		Game::MaterialPixelShaderProgram to{};
		to.loadDef = Convert(from.loadDef);
		return to;
	}

	struct MaterialPixelShader
	{
		std::uint32_t name;
		MaterialPixelShaderProgram prog;
	};

	static_assert(sizeof(MaterialPixelShader) == 0x10);
	static_assert(offsetof(MaterialPixelShader, name) == 0x0);
	static_assert(offsetof(MaterialPixelShader, prog) == 0x4);

	inline MaterialPixelShader Convert(const Game::MaterialPixelShader& from)
	{
		MaterialPixelShader to{};
		to.prog = Convert(from.prog);
		return to;
	}

	inline Game::MaterialPixelShader Convert(const MaterialPixelShader& from)
	{
		Game::MaterialPixelShader to{};
		to.prog = Convert(from.prog);
		return to;
	}

	struct MaterialArgumentCodeConst
	{
		std::uint16_t index;
		std::int8_t firstRow;
		std::int8_t rowCount;
	};

	static_assert(sizeof(MaterialArgumentCodeConst) == 0x4);
	static_assert(offsetof(MaterialArgumentCodeConst, index) == 0x0);
	static_assert(offsetof(MaterialArgumentCodeConst, firstRow) == 0x2);
	static_assert(offsetof(MaterialArgumentCodeConst, rowCount) == 0x3);

	inline MaterialArgumentCodeConst Convert(const Game::MaterialArgumentCodeConst& from)
	{
		MaterialArgumentCodeConst to{};
		to.index = static_cast<std::uint16_t>(from.index);
		to.firstRow = static_cast<std::int8_t>(from.firstRow);
		to.rowCount = static_cast<std::int8_t>(from.rowCount);
		return to;
	}

	inline Game::MaterialArgumentCodeConst Convert(const MaterialArgumentCodeConst& from)
	{
		Game::MaterialArgumentCodeConst to{};
		to.index = static_cast<decltype(to.index)>(from.index);
		to.firstRow = static_cast<decltype(to.firstRow)>(from.firstRow);
		to.rowCount = static_cast<decltype(to.rowCount)>(from.rowCount);
		return to;
	}

	union MaterialArgumentDef
	{
		std::uint32_t literalConst;
		MaterialArgumentCodeConst codeConst;
		std::uint32_t codeSampler;
		std::uint32_t nameHash;
	};

	static_assert(sizeof(MaterialArgumentDef) == 0x4);
	static_assert(offsetof(MaterialArgumentDef, literalConst) == 0x0);
	static_assert(offsetof(MaterialArgumentDef, codeConst) == 0x0);
	static_assert(offsetof(MaterialArgumentDef, codeSampler) == 0x0);
	static_assert(offsetof(MaterialArgumentDef, nameHash) == 0x0);

	inline MaterialArgumentDef Convert(const Game::MaterialArgumentDef& from)
	{
		MaterialArgumentDef to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::MaterialArgumentDef Convert(const MaterialArgumentDef& from)
	{
		Game::MaterialArgumentDef to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct MaterialShaderArgument
	{
		std::uint16_t type;
		std::uint16_t dest;
		MaterialArgumentDef u;
	};

	static_assert(sizeof(MaterialShaderArgument) == 0x8);
	static_assert(offsetof(MaterialShaderArgument, type) == 0x0);
	static_assert(offsetof(MaterialShaderArgument, dest) == 0x2);
	static_assert(offsetof(MaterialShaderArgument, u) == 0x4);

	inline MaterialShaderArgument Convert(const Game::MaterialShaderArgument& from)
	{
		MaterialShaderArgument to{};
		to.type = static_cast<std::uint16_t>(from.type);
		to.dest = static_cast<std::uint16_t>(from.dest);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::MaterialShaderArgument Convert(const MaterialShaderArgument& from)
	{
		Game::MaterialShaderArgument to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.dest = static_cast<decltype(to.dest)>(from.dest);
		to.u = Convert(from.u);
		return to;
	}

	union MaterialTextureDefInfo
	{
		std::uint32_t image;
		std::uint32_t water;
	};

	static_assert(sizeof(MaterialTextureDefInfo) == 0x4);
	static_assert(offsetof(MaterialTextureDefInfo, image) == 0x0);
	static_assert(offsetof(MaterialTextureDefInfo, water) == 0x0);

	inline MaterialTextureDefInfo Convert(const Game::MaterialTextureDefInfo& from)
	{
		MaterialTextureDefInfo to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::MaterialTextureDefInfo Convert(const MaterialTextureDefInfo& from)
	{
		Game::MaterialTextureDefInfo to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct MaterialTextureDef
	{
		std::uint32_t nameHash;
		std::int8_t nameStart;
		std::int8_t nameEnd;
		std::int8_t samplerState;
		std::int8_t semantic;
		MaterialTextureDefInfo u;
	};

	static_assert(sizeof(MaterialTextureDef) == 0xC);
	static_assert(offsetof(MaterialTextureDef, nameHash) == 0x0);
	static_assert(offsetof(MaterialTextureDef, nameStart) == 0x4);
	static_assert(offsetof(MaterialTextureDef, nameEnd) == 0x5);
	static_assert(offsetof(MaterialTextureDef, samplerState) == 0x6);
	static_assert(offsetof(MaterialTextureDef, semantic) == 0x7);
	static_assert(offsetof(MaterialTextureDef, u) == 0x8);

	inline MaterialTextureDef Convert(const Game::MaterialTextureDef& from)
	{
		MaterialTextureDef to{};
		to.nameHash = static_cast<std::uint32_t>(from.nameHash);
		to.nameStart = static_cast<std::int8_t>(from.nameStart);
		to.nameEnd = static_cast<std::int8_t>(from.nameEnd);
		to.samplerState = static_cast<std::int8_t>(from.samplerState);
		to.semantic = static_cast<std::int8_t>(from.semantic);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::MaterialTextureDef Convert(const MaterialTextureDef& from)
	{
		Game::MaterialTextureDef to{};
		to.nameHash = static_cast<decltype(to.nameHash)>(from.nameHash);
		to.nameStart = static_cast<decltype(to.nameStart)>(from.nameStart);
		to.nameEnd = static_cast<decltype(to.nameEnd)>(from.nameEnd);
		to.samplerState = static_cast<decltype(to.samplerState)>(from.samplerState);
		to.semantic = static_cast<decltype(to.semantic)>(from.semantic);
		to.u = Convert(from.u);
		return to;
	}

	union GfxTexture
	{
		std::uint32_t basemap;
		std::uint32_t map;
		std::uint32_t volmap;
		std::uint32_t cubemap;
		std::uint32_t loadDef;
	};

	static_assert(sizeof(GfxTexture) == 0x4);
	static_assert(offsetof(GfxTexture, basemap) == 0x0);
	static_assert(offsetof(GfxTexture, map) == 0x0);
	static_assert(offsetof(GfxTexture, volmap) == 0x0);
	static_assert(offsetof(GfxTexture, cubemap) == 0x0);
	static_assert(offsetof(GfxTexture, loadDef) == 0x0);

	inline GfxTexture Convert(const Game::GfxTexture& from)
	{
		GfxTexture to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::GfxTexture Convert(const GfxTexture& from)
	{
		Game::GfxTexture to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct Picmip
	{
		std::int8_t platform[2];
	};

	static_assert(sizeof(Picmip) == 0x2);
	static_assert(offsetof(Picmip, platform) == 0x0);

	inline Picmip Convert(const Game::Picmip& from)
	{
		Picmip to{};
		std::memcpy(to.platform, from.platform, sizeof(to.platform));
		return to;
	}

	inline Game::Picmip Convert(const Picmip& from)
	{
		Game::Picmip to{};
		std::memcpy(to.platform, from.platform, sizeof(from.platform));
		return to;
	}

	struct CardMemory
	{
		std::int32_t platform[2];
	};

	static_assert(sizeof(CardMemory) == 0x8);
	static_assert(offsetof(CardMemory, platform) == 0x0);

	inline CardMemory Convert(const Game::CardMemory& from)
	{
		CardMemory to{};
		std::memcpy(to.platform, from.platform, sizeof(to.platform));
		return to;
	}

	inline Game::CardMemory Convert(const CardMemory& from)
	{
		Game::CardMemory to{};
		std::memcpy(to.platform, from.platform, sizeof(from.platform));
		return to;
	}

	struct GfxImage
	{
		GfxTexture texture;
		std::int8_t mapType;
		std::int8_t semantic;
		std::int8_t category;
		bool useSrgbReads;
		Picmip picmip;
		bool noPicmip;
		std::int8_t track;
		CardMemory cardMemory;
		std::uint16_t width;
		std::uint16_t height;
		std::uint16_t depth;
		bool delayLoadPixels;
		std::uint32_t name;
	};

	static_assert(sizeof(GfxImage) == 0x20);
	static_assert(offsetof(GfxImage, texture) == 0x0);
	static_assert(offsetof(GfxImage, mapType) == 0x4);
	static_assert(offsetof(GfxImage, semantic) == 0x5);
	static_assert(offsetof(GfxImage, category) == 0x6);
	static_assert(offsetof(GfxImage, useSrgbReads) == 0x7);
	static_assert(offsetof(GfxImage, picmip) == 0x8);
	static_assert(offsetof(GfxImage, noPicmip) == 0xA);
	static_assert(offsetof(GfxImage, track) == 0xB);
	static_assert(offsetof(GfxImage, cardMemory) == 0xC);
	static_assert(offsetof(GfxImage, width) == 0x14);
	static_assert(offsetof(GfxImage, height) == 0x16);
	static_assert(offsetof(GfxImage, depth) == 0x18);
	static_assert(offsetof(GfxImage, delayLoadPixels) == 0x1A);
	static_assert(offsetof(GfxImage, name) == 0x1C);

	inline GfxImage Convert(const Game::GfxImage& from)
	{
		GfxImage to{};
		to.texture = Convert(from.texture);
		to.mapType = static_cast<std::int8_t>(from.mapType);
		to.semantic = static_cast<std::int8_t>(from.semantic);
		to.category = static_cast<std::int8_t>(from.category);
		to.useSrgbReads = static_cast<bool>(from.useSrgbReads);
		to.picmip = Convert(from.picmip);
		to.noPicmip = static_cast<bool>(from.noPicmip);
		to.track = static_cast<std::int8_t>(from.track);
		to.cardMemory = Convert(from.cardMemory);
		to.width = static_cast<std::uint16_t>(from.width);
		to.height = static_cast<std::uint16_t>(from.height);
		to.depth = static_cast<std::uint16_t>(from.depth);
		to.delayLoadPixels = static_cast<bool>(from.delayLoadPixels);
		return to;
	}

	inline Game::GfxImage Convert(const GfxImage& from)
	{
		Game::GfxImage to{};
		to.texture = Convert(from.texture);
		to.mapType = static_cast<decltype(to.mapType)>(from.mapType);
		to.semantic = static_cast<decltype(to.semantic)>(from.semantic);
		to.category = static_cast<decltype(to.category)>(from.category);
		to.useSrgbReads = static_cast<decltype(to.useSrgbReads)>(from.useSrgbReads);
		to.picmip = Convert(from.picmip);
		to.noPicmip = static_cast<decltype(to.noPicmip)>(from.noPicmip);
		to.track = static_cast<decltype(to.track)>(from.track);
		to.cardMemory = Convert(from.cardMemory);
		to.width = static_cast<decltype(to.width)>(from.width);
		to.height = static_cast<decltype(to.height)>(from.height);
		to.depth = static_cast<decltype(to.depth)>(from.depth);
		to.delayLoadPixels = static_cast<decltype(to.delayLoadPixels)>(from.delayLoadPixels);
		return to;
	}

	struct GfxImageLoadDef
	{
		std::int8_t levelCount;
		std::int8_t pad[3];
		std::int32_t flags;
		std::int32_t format;
		std::int32_t resourceSize;
		std::uint8_t data[1];
	};

	static_assert(sizeof(GfxImageLoadDef) == 0x14);
	static_assert(offsetof(GfxImageLoadDef, levelCount) == 0x0);
	static_assert(offsetof(GfxImageLoadDef, pad) == 0x1);
	static_assert(offsetof(GfxImageLoadDef, flags) == 0x4);
	static_assert(offsetof(GfxImageLoadDef, format) == 0x8);
	static_assert(offsetof(GfxImageLoadDef, resourceSize) == 0xC);
	static_assert(offsetof(GfxImageLoadDef, data) == 0x10);

	inline GfxImageLoadDef Convert(const Game::GfxImageLoadDef& from)
	{
		GfxImageLoadDef to{};
		to.levelCount = static_cast<std::int8_t>(from.levelCount);
		std::memcpy(to.pad, from.pad, sizeof(to.pad));
		to.flags = static_cast<std::int32_t>(from.flags);
		to.format = static_cast<std::int32_t>(from.format);
		to.resourceSize = static_cast<std::int32_t>(from.resourceSize);
		std::memcpy(to.data, from.data, sizeof(to.data));
		return to;
	}

	inline Game::GfxImageLoadDef Convert(const GfxImageLoadDef& from)
	{
		Game::GfxImageLoadDef to{};
		to.levelCount = static_cast<decltype(to.levelCount)>(from.levelCount);
		std::memcpy(to.pad, from.pad, sizeof(from.pad));
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.format = static_cast<decltype(to.format)>(from.format);
		to.resourceSize = static_cast<decltype(to.resourceSize)>(from.resourceSize);
		std::memcpy(to.data, from.data, sizeof(from.data));
		return to;
	}

	struct WaterWritable
	{
		float floatTime;
	};

	static_assert(sizeof(WaterWritable) == 0x4);
	static_assert(offsetof(WaterWritable, floatTime) == 0x0);

	inline WaterWritable Convert(const Game::WaterWritable& from)
	{
		WaterWritable to{};
		to.floatTime = static_cast<float>(from.floatTime);
		return to;
	}

	inline Game::WaterWritable Convert(const WaterWritable& from)
	{
		Game::WaterWritable to{};
		to.floatTime = static_cast<decltype(to.floatTime)>(from.floatTime);
		return to;
	}

	struct water_t
	{
		WaterWritable writable;
		std::uint32_t H0;
		std::uint32_t wTerm;
		std::int32_t M;
		std::int32_t N;
		float Lx;
		float Lz;
		float gravity;
		float windvel;
		float winddir[2];
		float amplitude;
		float codeConstant[4];
		std::uint32_t image;
	};

	static_assert(sizeof(water_t) == 0x44);
	static_assert(offsetof(water_t, writable) == 0x0);
	static_assert(offsetof(water_t, H0) == 0x4);
	static_assert(offsetof(water_t, wTerm) == 0x8);
	static_assert(offsetof(water_t, M) == 0xC);
	static_assert(offsetof(water_t, N) == 0x10);
	static_assert(offsetof(water_t, Lx) == 0x14);
	static_assert(offsetof(water_t, Lz) == 0x18);
	static_assert(offsetof(water_t, gravity) == 0x1C);
	static_assert(offsetof(water_t, windvel) == 0x20);
	static_assert(offsetof(water_t, winddir) == 0x24);
	static_assert(offsetof(water_t, amplitude) == 0x2C);
	static_assert(offsetof(water_t, codeConstant) == 0x30);
	static_assert(offsetof(water_t, image) == 0x40);

	inline water_t Convert(const Game::water_t& from)
	{
		water_t to{};
		to.writable = Convert(from.writable);
		to.M = static_cast<std::int32_t>(from.M);
		to.N = static_cast<std::int32_t>(from.N);
		to.Lx = static_cast<float>(from.Lx);
		to.Lz = static_cast<float>(from.Lz);
		to.gravity = static_cast<float>(from.gravity);
		to.windvel = static_cast<float>(from.windvel);
		std::memcpy(to.winddir, from.winddir, sizeof(to.winddir));
		to.amplitude = static_cast<float>(from.amplitude);
		std::memcpy(to.codeConstant, from.codeConstant, sizeof(to.codeConstant));
		return to;
	}

	inline Game::water_t Convert(const water_t& from)
	{
		Game::water_t to{};
		to.writable = Convert(from.writable);
		to.M = static_cast<decltype(to.M)>(from.M);
		to.N = static_cast<decltype(to.N)>(from.N);
		to.Lx = static_cast<decltype(to.Lx)>(from.Lx);
		to.Lz = static_cast<decltype(to.Lz)>(from.Lz);
		to.gravity = static_cast<decltype(to.gravity)>(from.gravity);
		to.windvel = static_cast<decltype(to.windvel)>(from.windvel);
		std::memcpy(to.winddir, from.winddir, sizeof(from.winddir));
		to.amplitude = static_cast<decltype(to.amplitude)>(from.amplitude);
		std::memcpy(to.codeConstant, from.codeConstant, sizeof(from.codeConstant));
		return to;
	}

	struct complex_s
	{
		float real;
		float imag;
	};

	static_assert(sizeof(complex_s) == 0x8);
	static_assert(offsetof(complex_s, real) == 0x0);
	static_assert(offsetof(complex_s, imag) == 0x4);

	inline complex_s Convert(const Game::complex_s& from)
	{
		complex_s to{};
		to.real = static_cast<float>(from.real);
		to.imag = static_cast<float>(from.imag);
		return to;
	}

	inline Game::complex_s Convert(const complex_s& from)
	{
		Game::complex_s to{};
		to.real = static_cast<decltype(to.real)>(from.real);
		to.imag = static_cast<decltype(to.imag)>(from.imag);
		return to;
	}

	struct MaterialConstantDef
	{
		std::uint32_t nameHash;
		std::int8_t name[12];
		float literal[4];
	};

	static_assert(sizeof(MaterialConstantDef) == 0x20);
	static_assert(offsetof(MaterialConstantDef, nameHash) == 0x0);
	static_assert(offsetof(MaterialConstantDef, name) == 0x4);
	static_assert(offsetof(MaterialConstantDef, literal) == 0x10);

	inline MaterialConstantDef Convert(const Game::MaterialConstantDef& from)
	{
		MaterialConstantDef to{};
		to.nameHash = static_cast<std::uint32_t>(from.nameHash);
		std::memcpy(to.name, from.name, sizeof(to.name));
		std::memcpy(to.literal, from.literal, sizeof(to.literal));
		return to;
	}

	inline Game::MaterialConstantDef Convert(const MaterialConstantDef& from)
	{
		Game::MaterialConstantDef to{};
		to.nameHash = static_cast<decltype(to.nameHash)>(from.nameHash);
		std::memcpy(to.name, from.name, sizeof(from.name));
		std::memcpy(to.literal, from.literal, sizeof(from.literal));
		return to;
	}

	struct GfxStateBits
	{
		std::uint32_t loadBits[2];
	};

	static_assert(sizeof(GfxStateBits) == 0x8);
	static_assert(offsetof(GfxStateBits, loadBits) == 0x0);

	inline GfxStateBits Convert(const Game::GfxStateBits& from)
	{
		GfxStateBits to{};
		std::memcpy(to.loadBits, from.loadBits, sizeof(to.loadBits));
		return to;
	}

	inline Game::GfxStateBits Convert(const GfxStateBits& from)
	{
		Game::GfxStateBits to{};
		std::memcpy(to.loadBits, from.loadBits, sizeof(from.loadBits));
		return to;
	}

	struct XModelCollSurf_s
	{
		std::uint32_t collTris;
		std::int32_t numCollTris;
		Bounds bounds;
		std::int32_t boneIdx;
		std::int32_t contents;
		std::int32_t surfFlags;
	};

	static_assert(sizeof(XModelCollSurf_s) == 0x2C);
	static_assert(offsetof(XModelCollSurf_s, collTris) == 0x0);
	static_assert(offsetof(XModelCollSurf_s, numCollTris) == 0x4);
	static_assert(offsetof(XModelCollSurf_s, bounds) == 0x8);
	static_assert(offsetof(XModelCollSurf_s, boneIdx) == 0x20);
	static_assert(offsetof(XModelCollSurf_s, contents) == 0x24);
	static_assert(offsetof(XModelCollSurf_s, surfFlags) == 0x28);

	inline XModelCollSurf_s Convert(const Game::XModelCollSurf_s& from)
	{
		XModelCollSurf_s to{};
		to.numCollTris = static_cast<std::int32_t>(from.numCollTris);
		to.bounds = Convert(from.bounds);
		to.boneIdx = static_cast<std::int32_t>(from.boneIdx);
		to.contents = static_cast<std::int32_t>(from.contents);
		to.surfFlags = static_cast<std::int32_t>(from.surfFlags);
		return to;
	}

	inline Game::XModelCollSurf_s Convert(const XModelCollSurf_s& from)
	{
		Game::XModelCollSurf_s to{};
		to.numCollTris = static_cast<decltype(to.numCollTris)>(from.numCollTris);
		to.bounds = Convert(from.bounds);
		to.boneIdx = static_cast<decltype(to.boneIdx)>(from.boneIdx);
		to.contents = static_cast<decltype(to.contents)>(from.contents);
		to.surfFlags = static_cast<decltype(to.surfFlags)>(from.surfFlags);
		return to;
	}

	struct XModelCollTri_s
	{
		float plane[4];
		float svec[4];
		float tvec[4];
	};

	static_assert(sizeof(XModelCollTri_s) == 0x30);
	static_assert(offsetof(XModelCollTri_s, plane) == 0x0);
	static_assert(offsetof(XModelCollTri_s, svec) == 0x10);
	static_assert(offsetof(XModelCollTri_s, tvec) == 0x20);

	inline XModelCollTri_s Convert(const Game::XModelCollTri_s& from)
	{
		XModelCollTri_s to{};
		std::memcpy(to.plane, from.plane, sizeof(to.plane));
		std::memcpy(to.svec, from.svec, sizeof(to.svec));
		std::memcpy(to.tvec, from.tvec, sizeof(to.tvec));
		return to;
	}

	inline Game::XModelCollTri_s Convert(const XModelCollTri_s& from)
	{
		Game::XModelCollTri_s to{};
		std::memcpy(to.plane, from.plane, sizeof(from.plane));
		std::memcpy(to.svec, from.svec, sizeof(from.svec));
		std::memcpy(to.tvec, from.tvec, sizeof(from.tvec));
		return to;
	}

	struct XBoneInfo
	{
		Bounds bounds;
		float radiusSquared;
	};

	static_assert(sizeof(XBoneInfo) == 0x1C);
	static_assert(offsetof(XBoneInfo, bounds) == 0x0);
	static_assert(offsetof(XBoneInfo, radiusSquared) == 0x18);

	inline XBoneInfo Convert(const Game::XBoneInfo& from)
	{
		XBoneInfo to{};
		to.bounds = Convert(from.bounds);
		to.radiusSquared = static_cast<float>(from.radiusSquared);
		return to;
	}

	inline Game::XBoneInfo Convert(const XBoneInfo& from)
	{
		Game::XBoneInfo to{};
		to.bounds = Convert(from.bounds);
		to.radiusSquared = static_cast<decltype(to.radiusSquared)>(from.radiusSquared);
		return to;
	}

	struct snd_alias_list_t
	{
		std::uint32_t aliasName;
		std::uint32_t head;
		std::uint32_t count;
	};

	static_assert(sizeof(snd_alias_list_t) == 0xC);
	static_assert(offsetof(snd_alias_list_t, aliasName) == 0x0);
	static_assert(offsetof(snd_alias_list_t, head) == 0x4);
	static_assert(offsetof(snd_alias_list_t, count) == 0x8);

	inline snd_alias_list_t Convert(const Game::snd_alias_list_t& from)
	{
		snd_alias_list_t to{};
		to.count = static_cast<std::uint32_t>(from.count);
		return to;
	}

	inline Game::snd_alias_list_t Convert(const snd_alias_list_t& from)
	{
		Game::snd_alias_list_t to{};
		to.count = static_cast<decltype(to.count)>(from.count);
		return to;
	}

	union SoundAliasFlags
	{
#pragma warning(push)
#pragma warning(disable: 4201)
		struct
		{
			std::uint32_t looping : 1;
			std::uint32_t isMaster : 1;
			std::uint32_t isSlave : 1;
			std::uint32_t fullDryLevel : 1;
			std::uint32_t noWetLevel : 1;
			std::uint32_t unknown : 1;
			std::uint32_t unk_is3D : 1;
			std::uint32_t type : 2;
			std::uint32_t channel : 6;
		};
#pragma warning(pop)
		std::uint32_t intValue;
	};

	static_assert(sizeof(SoundAliasFlags) == 0x4);
	static_assert(offsetof(SoundAliasFlags, intValue) == 0x0);

	inline SoundAliasFlags Convert(const Game::SoundAliasFlags& from)
	{
		SoundAliasFlags to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::SoundAliasFlags Convert(const SoundAliasFlags& from)
	{
		Game::SoundAliasFlags to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct snd_alias_t
	{
		std::uint32_t aliasName;
		std::uint32_t subtitle;
		std::uint32_t secondaryAliasName;
		std::uint32_t chainAliasName;
		std::uint32_t mixerGroup;
		std::uint32_t soundFile;
		std::int32_t sequence;
		float volMin;
		float volMax;
		float pitchMin;
		float pitchMax;
		float distMin;
		float distMax;
		float velocityMin;
		SoundAliasFlags flags;
		union
		{
			float slavePercentage;
			float masterPercentage;
		} ___u15;
		float probability;
		float lfePercentage;
		float centerPercentage;
		std::int32_t startDelay;
		std::uint32_t volumeFalloffCurve;
		float envelopMin;
		float envelopMax;
		float envelopPercentage;
		std::uint32_t speakerMap;
	};

	static_assert(sizeof(snd_alias_t) == 0x64);
	static_assert(offsetof(snd_alias_t, aliasName) == 0x0);
	static_assert(offsetof(snd_alias_t, subtitle) == 0x4);
	static_assert(offsetof(snd_alias_t, secondaryAliasName) == 0x8);
	static_assert(offsetof(snd_alias_t, chainAliasName) == 0xC);
	static_assert(offsetof(snd_alias_t, mixerGroup) == 0x10);
	static_assert(offsetof(snd_alias_t, soundFile) == 0x14);
	static_assert(offsetof(snd_alias_t, sequence) == 0x18);
	static_assert(offsetof(snd_alias_t, volMin) == 0x1C);
	static_assert(offsetof(snd_alias_t, volMax) == 0x20);
	static_assert(offsetof(snd_alias_t, pitchMin) == 0x24);
	static_assert(offsetof(snd_alias_t, pitchMax) == 0x28);
	static_assert(offsetof(snd_alias_t, distMin) == 0x2C);
	static_assert(offsetof(snd_alias_t, distMax) == 0x30);
	static_assert(offsetof(snd_alias_t, velocityMin) == 0x34);
	static_assert(offsetof(snd_alias_t, flags) == 0x38);
	static_assert(offsetof(snd_alias_t, ___u15) == 0x3C);
	static_assert(offsetof(snd_alias_t, probability) == 0x40);
	static_assert(offsetof(snd_alias_t, lfePercentage) == 0x44);
	static_assert(offsetof(snd_alias_t, centerPercentage) == 0x48);
	static_assert(offsetof(snd_alias_t, startDelay) == 0x4C);
	static_assert(offsetof(snd_alias_t, volumeFalloffCurve) == 0x50);
	static_assert(offsetof(snd_alias_t, envelopMin) == 0x54);
	static_assert(offsetof(snd_alias_t, envelopMax) == 0x58);
	static_assert(offsetof(snd_alias_t, envelopPercentage) == 0x5C);
	static_assert(offsetof(snd_alias_t, speakerMap) == 0x60);

	inline snd_alias_t Convert(const Game::snd_alias_t& from)
	{
		snd_alias_t to{};
		to.sequence = static_cast<std::int32_t>(from.sequence);
		to.volMin = static_cast<float>(from.volMin);
		to.volMax = static_cast<float>(from.volMax);
		to.pitchMin = static_cast<float>(from.pitchMin);
		to.pitchMax = static_cast<float>(from.pitchMax);
		to.distMin = static_cast<float>(from.distMin);
		to.distMax = static_cast<float>(from.distMax);
		to.velocityMin = static_cast<float>(from.velocityMin);
		to.flags = Convert(from.flags);
		std::memcpy(&to.___u15, &from.___u15, sizeof(to.___u15));
		to.probability = static_cast<float>(from.probability);
		to.lfePercentage = static_cast<float>(from.lfePercentage);
		to.centerPercentage = static_cast<float>(from.centerPercentage);
		to.startDelay = static_cast<std::int32_t>(from.startDelay);
		to.envelopMin = static_cast<float>(from.envelopMin);
		to.envelopMax = static_cast<float>(from.envelopMax);
		to.envelopPercentage = static_cast<float>(from.envelopPercentage);
		return to;
	}

	inline Game::snd_alias_t Convert(const snd_alias_t& from)
	{
		Game::snd_alias_t to{};
		to.sequence = static_cast<decltype(to.sequence)>(from.sequence);
		to.volMin = static_cast<decltype(to.volMin)>(from.volMin);
		to.volMax = static_cast<decltype(to.volMax)>(from.volMax);
		to.pitchMin = static_cast<decltype(to.pitchMin)>(from.pitchMin);
		to.pitchMax = static_cast<decltype(to.pitchMax)>(from.pitchMax);
		to.distMin = static_cast<decltype(to.distMin)>(from.distMin);
		to.distMax = static_cast<decltype(to.distMax)>(from.distMax);
		to.velocityMin = static_cast<decltype(to.velocityMin)>(from.velocityMin);
		to.flags = Convert(from.flags);
		std::memcpy(&to.___u15, &from.___u15, sizeof(from.___u15));
		to.probability = static_cast<decltype(to.probability)>(from.probability);
		to.lfePercentage = static_cast<decltype(to.lfePercentage)>(from.lfePercentage);
		to.centerPercentage = static_cast<decltype(to.centerPercentage)>(from.centerPercentage);
		to.startDelay = static_cast<decltype(to.startDelay)>(from.startDelay);
		to.envelopMin = static_cast<decltype(to.envelopMin)>(from.envelopMin);
		to.envelopMax = static_cast<decltype(to.envelopMax)>(from.envelopMax);
		to.envelopPercentage = static_cast<decltype(to.envelopPercentage)>(from.envelopPercentage);
		return to;
	}

	struct StreamFileNameRaw
	{
		std::uint32_t dir;
		std::uint32_t name;
	};

	static_assert(sizeof(StreamFileNameRaw) == 0x8);
	static_assert(offsetof(StreamFileNameRaw, dir) == 0x0);
	static_assert(offsetof(StreamFileNameRaw, name) == 0x4);

	inline StreamFileNameRaw Convert(const Game::StreamFileNameRaw&)
	{
		StreamFileNameRaw to{};
		return to;
	}

	inline Game::StreamFileNameRaw Convert(const StreamFileNameRaw&)
	{
		Game::StreamFileNameRaw to{};
		return to;
	}

	union StreamFileInfo
	{
		StreamFileNameRaw raw;
	};

	static_assert(sizeof(StreamFileInfo) == 0x8);
	static_assert(offsetof(StreamFileInfo, raw) == 0x0);

	inline StreamFileInfo Convert(const Game::StreamFileInfo& from)
	{
		StreamFileInfo to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::StreamFileInfo Convert(const StreamFileInfo& from)
	{
		Game::StreamFileInfo to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct StreamFileName
	{
		StreamFileInfo info;
	};

	static_assert(sizeof(StreamFileName) == 0x8);
	static_assert(offsetof(StreamFileName, info) == 0x0);

	inline StreamFileName Convert(const Game::StreamFileName& from)
	{
		StreamFileName to{};
		to.info = Convert(from.info);
		return to;
	}

	inline Game::StreamFileName Convert(const StreamFileName& from)
	{
		Game::StreamFileName to{};
		to.info = Convert(from.info);
		return to;
	}

	struct StreamedSound
	{
		StreamFileName filename;
	};

	static_assert(sizeof(StreamedSound) == 0x8);
	static_assert(offsetof(StreamedSound, filename) == 0x0);

	inline StreamedSound Convert(const Game::StreamedSound& from)
	{
		StreamedSound to{};
		to.filename = Convert(from.filename);
		return to;
	}

	inline Game::StreamedSound Convert(const StreamedSound& from)
	{
		Game::StreamedSound to{};
		to.filename = Convert(from.filename);
		return to;
	}

	union SoundFileRef
	{
		std::uint32_t loadSnd;
		StreamedSound streamSnd;
	};

	static_assert(sizeof(SoundFileRef) == 0x8);
	static_assert(offsetof(SoundFileRef, loadSnd) == 0x0);
	static_assert(offsetof(SoundFileRef, streamSnd) == 0x0);

	inline SoundFileRef Convert(const Game::SoundFileRef& from)
	{
		SoundFileRef to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::SoundFileRef Convert(const SoundFileRef& from)
	{
		Game::SoundFileRef to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct SoundFile
	{
		std::int8_t type;
		std::int8_t exists;
		SoundFileRef u;
	};

	static_assert(sizeof(SoundFile) == 0xC);
	static_assert(offsetof(SoundFile, type) == 0x0);
	static_assert(offsetof(SoundFile, exists) == 0x1);
	static_assert(offsetof(SoundFile, u) == 0x4);

	inline SoundFile Convert(const Game::SoundFile& from)
	{
		SoundFile to{};
		to.type = static_cast<std::int8_t>(from.type);
		to.exists = static_cast<std::int8_t>(from.exists);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::SoundFile Convert(const SoundFile& from)
	{
		Game::SoundFile to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.exists = static_cast<decltype(to.exists)>(from.exists);
		to.u = Convert(from.u);
		return to;
	}

	struct _AILSOUNDINFO
	{
		std::int32_t format;
		std::uint32_t data_ptr;
		std::uint32_t data_len;
		std::uint32_t rate;
		std::int32_t bits;
		std::int32_t channels;
		std::uint32_t samples;
		std::uint32_t block_size;
		std::uint32_t initial_ptr;
	};

	static_assert(sizeof(_AILSOUNDINFO) == 0x24);
	static_assert(offsetof(_AILSOUNDINFO, format) == 0x0);
	static_assert(offsetof(_AILSOUNDINFO, data_ptr) == 0x4);
	static_assert(offsetof(_AILSOUNDINFO, data_len) == 0x8);
	static_assert(offsetof(_AILSOUNDINFO, rate) == 0xC);
	static_assert(offsetof(_AILSOUNDINFO, bits) == 0x10);
	static_assert(offsetof(_AILSOUNDINFO, channels) == 0x14);
	static_assert(offsetof(_AILSOUNDINFO, samples) == 0x18);
	static_assert(offsetof(_AILSOUNDINFO, block_size) == 0x1C);
	static_assert(offsetof(_AILSOUNDINFO, initial_ptr) == 0x20);

	struct MssSound
	{
		_AILSOUNDINFO info;
		std::uint32_t data;
	};

	static_assert(sizeof(MssSound) == 0x28);
	static_assert(offsetof(MssSound, info) == 0x0);
	static_assert(offsetof(MssSound, data) == 0x24);

	struct LoadedSound
	{
		std::uint32_t name;
		MssSound sound;
	};

	static_assert(sizeof(LoadedSound) == 0x2C);
	static_assert(offsetof(LoadedSound, name) == 0x0);
	static_assert(offsetof(LoadedSound, sound) == 0x4);

	struct SndCurve
	{
		std::uint32_t filename;
		std::uint16_t knotCount;
		float knots[16][2];
	};

	static_assert(sizeof(SndCurve) == 0x88);
	static_assert(offsetof(SndCurve, filename) == 0x0);
	static_assert(offsetof(SndCurve, knotCount) == 0x4);
	static_assert(offsetof(SndCurve, knots) == 0x8);

	inline SndCurve Convert(const Game::SndCurve& from)
	{
		SndCurve to{};
		to.knotCount = static_cast<std::uint16_t>(from.knotCount);
		std::memcpy(to.knots, from.knots, sizeof(to.knots));
		return to;
	}

	inline Game::SndCurve Convert(const SndCurve& from)
	{
		Game::SndCurve to{};
		to.knotCount = static_cast<decltype(to.knotCount)>(from.knotCount);
		std::memcpy(to.knots, from.knots, sizeof(from.knots));
		return to;
	}

	struct MSSSpeakerLevels
	{
		std::int32_t speaker;
		std::int32_t numLevels;
		float levels[2];
	};

	static_assert(sizeof(MSSSpeakerLevels) == 0x10);
	static_assert(offsetof(MSSSpeakerLevels, speaker) == 0x0);
	static_assert(offsetof(MSSSpeakerLevels, numLevels) == 0x4);
	static_assert(offsetof(MSSSpeakerLevels, levels) == 0x8);

	struct MSSChannelMap
	{
		std::uint32_t speakerCount;
		MSSSpeakerLevels speakers[6];
	};

	static_assert(sizeof(MSSChannelMap) == 0x64);
	static_assert(offsetof(MSSChannelMap, speakerCount) == 0x0);
	static_assert(offsetof(MSSChannelMap, speakers) == 0x4);

	struct SpeakerMap
	{
		bool isDefault;
		std::uint32_t name;
		MSSChannelMap channelMaps[2][2];
	};

	static_assert(sizeof(SpeakerMap) == 0x198);
	static_assert(offsetof(SpeakerMap, isDefault) == 0x0);
	static_assert(offsetof(SpeakerMap, name) == 0x4);
	static_assert(offsetof(SpeakerMap, channelMaps) == 0x8);

#pragma warning(push)
#pragma warning(disable: 4324)
	struct alignas(64) clipMap_t
	{
		std::uint32_t name;
		std::int32_t isInUse;
		std::uint32_t planeCount;
		std::uint32_t planes;
		std::uint32_t numStaticModels;
		std::uint32_t staticModelList;
		std::uint32_t numMaterials;
		std::uint32_t materials;
		std::uint32_t numBrushSides;
		std::uint32_t brushsides;
		std::uint32_t numBrushEdges;
		std::uint32_t brushEdges;
		std::uint32_t numNodes;
		std::uint32_t nodes;
		std::uint32_t numLeafs;
		std::uint32_t leafs;
		std::uint32_t leafbrushNodesCount;
		std::uint32_t leafbrushNodes;
		std::uint32_t numLeafBrushes;
		std::uint32_t leafbrushes;
		std::uint32_t numLeafSurfaces;
		std::uint32_t leafsurfaces;
		std::uint32_t vertCount;
		std::uint32_t verts;
		std::uint32_t triCount;
		std::uint32_t triIndices;
		std::uint32_t triEdgeIsWalkable;
		std::uint32_t borderCount;
		std::uint32_t borders;
		std::uint32_t partitionCount;
		std::uint32_t partitions;
		std::uint32_t aabbTreeCount;
		std::uint32_t aabbTrees;
		std::uint32_t numSubModels;
		std::uint32_t cmodels;
		std::uint16_t numBrushes;
		std::uint32_t brushes;
		std::uint32_t brushBounds;
		std::uint32_t brushContents;
		std::uint32_t mapEnts;
		std::uint16_t smodelNodeCount;
		std::uint32_t smodelNodes;
		std::uint16_t dynEntCount[2];
		std::uint32_t dynEntDefList[2];
		std::uint32_t dynEntPoseList[2];
		std::uint32_t dynEntClientList[2];
		std::uint32_t dynEntCollList[2];
		std::uint32_t checksum;
	};
#pragma warning(pop)

	static_assert(sizeof(clipMap_t) == 0x100);
	static_assert(offsetof(clipMap_t, name) == 0x0);
	static_assert(offsetof(clipMap_t, isInUse) == 0x4);
	static_assert(offsetof(clipMap_t, planeCount) == 0x8);
	static_assert(offsetof(clipMap_t, planes) == 0xC);
	static_assert(offsetof(clipMap_t, numStaticModels) == 0x10);
	static_assert(offsetof(clipMap_t, staticModelList) == 0x14);
	static_assert(offsetof(clipMap_t, numMaterials) == 0x18);
	static_assert(offsetof(clipMap_t, materials) == 0x1C);
	static_assert(offsetof(clipMap_t, numBrushSides) == 0x20);
	static_assert(offsetof(clipMap_t, brushsides) == 0x24);
	static_assert(offsetof(clipMap_t, numBrushEdges) == 0x28);
	static_assert(offsetof(clipMap_t, brushEdges) == 0x2C);
	static_assert(offsetof(clipMap_t, numNodes) == 0x30);
	static_assert(offsetof(clipMap_t, nodes) == 0x34);
	static_assert(offsetof(clipMap_t, numLeafs) == 0x38);
	static_assert(offsetof(clipMap_t, leafs) == 0x3C);
	static_assert(offsetof(clipMap_t, leafbrushNodesCount) == 0x40);
	static_assert(offsetof(clipMap_t, leafbrushNodes) == 0x44);
	static_assert(offsetof(clipMap_t, numLeafBrushes) == 0x48);
	static_assert(offsetof(clipMap_t, leafbrushes) == 0x4C);
	static_assert(offsetof(clipMap_t, numLeafSurfaces) == 0x50);
	static_assert(offsetof(clipMap_t, leafsurfaces) == 0x54);
	static_assert(offsetof(clipMap_t, vertCount) == 0x58);
	static_assert(offsetof(clipMap_t, verts) == 0x5C);
	static_assert(offsetof(clipMap_t, triCount) == 0x60);
	static_assert(offsetof(clipMap_t, triIndices) == 0x64);
	static_assert(offsetof(clipMap_t, triEdgeIsWalkable) == 0x68);
	static_assert(offsetof(clipMap_t, borderCount) == 0x6C);
	static_assert(offsetof(clipMap_t, borders) == 0x70);
	static_assert(offsetof(clipMap_t, partitionCount) == 0x74);
	static_assert(offsetof(clipMap_t, partitions) == 0x78);
	static_assert(offsetof(clipMap_t, aabbTreeCount) == 0x7C);
	static_assert(offsetof(clipMap_t, aabbTrees) == 0x80);
	static_assert(offsetof(clipMap_t, numSubModels) == 0x84);
	static_assert(offsetof(clipMap_t, cmodels) == 0x88);
	static_assert(offsetof(clipMap_t, numBrushes) == 0x8C);
	static_assert(offsetof(clipMap_t, brushes) == 0x90);
	static_assert(offsetof(clipMap_t, brushBounds) == 0x94);
	static_assert(offsetof(clipMap_t, brushContents) == 0x98);
	static_assert(offsetof(clipMap_t, mapEnts) == 0x9C);
	static_assert(offsetof(clipMap_t, smodelNodeCount) == 0xA0);
	static_assert(offsetof(clipMap_t, smodelNodes) == 0xA4);
	static_assert(offsetof(clipMap_t, dynEntCount) == 0xA8);
	static_assert(offsetof(clipMap_t, dynEntDefList) == 0xAC);
	static_assert(offsetof(clipMap_t, dynEntPoseList) == 0xB4);
	static_assert(offsetof(clipMap_t, dynEntClientList) == 0xBC);
	static_assert(offsetof(clipMap_t, dynEntCollList) == 0xC4);
	static_assert(offsetof(clipMap_t, checksum) == 0xCC);

	inline clipMap_t Convert(const Game::clipMap_t& from)
	{
		clipMap_t to{};
		to.isInUse = static_cast<std::int32_t>(from.isInUse);
		to.planeCount = static_cast<std::uint32_t>(from.planeCount);
		to.numStaticModels = static_cast<std::uint32_t>(from.numStaticModels);
		to.numMaterials = static_cast<std::uint32_t>(from.numMaterials);
		to.numBrushSides = static_cast<std::uint32_t>(from.numBrushSides);
		to.numBrushEdges = static_cast<std::uint32_t>(from.numBrushEdges);
		to.numNodes = static_cast<std::uint32_t>(from.numNodes);
		to.numLeafs = static_cast<std::uint32_t>(from.numLeafs);
		to.leafbrushNodesCount = static_cast<std::uint32_t>(from.leafbrushNodesCount);
		to.numLeafBrushes = static_cast<std::uint32_t>(from.numLeafBrushes);
		to.numLeafSurfaces = static_cast<std::uint32_t>(from.numLeafSurfaces);
		to.vertCount = static_cast<std::uint32_t>(from.vertCount);
		to.triCount = static_cast<std::uint32_t>(from.triCount);
		to.borderCount = static_cast<std::uint32_t>(from.borderCount);
		to.partitionCount = static_cast<std::uint32_t>(from.partitionCount);
		to.aabbTreeCount = static_cast<std::uint32_t>(from.aabbTreeCount);
		to.numSubModels = static_cast<std::uint32_t>(from.numSubModels);
		to.numBrushes = static_cast<std::uint16_t>(from.numBrushes);
		to.smodelNodeCount = static_cast<std::uint16_t>(from.smodelNodeCount);
		std::memcpy(to.dynEntCount, from.dynEntCount, sizeof(to.dynEntCount));
		to.checksum = static_cast<std::uint32_t>(from.checksum);
		return to;
	}

	inline Game::clipMap_t Convert(const clipMap_t& from)
	{
		Game::clipMap_t to{};
		to.isInUse = static_cast<decltype(to.isInUse)>(from.isInUse);
		to.planeCount = static_cast<decltype(to.planeCount)>(from.planeCount);
		to.numStaticModels = static_cast<decltype(to.numStaticModels)>(from.numStaticModels);
		to.numMaterials = static_cast<decltype(to.numMaterials)>(from.numMaterials);
		to.numBrushSides = static_cast<decltype(to.numBrushSides)>(from.numBrushSides);
		to.numBrushEdges = static_cast<decltype(to.numBrushEdges)>(from.numBrushEdges);
		to.numNodes = static_cast<decltype(to.numNodes)>(from.numNodes);
		to.numLeafs = static_cast<decltype(to.numLeafs)>(from.numLeafs);
		to.leafbrushNodesCount = static_cast<decltype(to.leafbrushNodesCount)>(from.leafbrushNodesCount);
		to.numLeafBrushes = static_cast<decltype(to.numLeafBrushes)>(from.numLeafBrushes);
		to.numLeafSurfaces = static_cast<decltype(to.numLeafSurfaces)>(from.numLeafSurfaces);
		to.vertCount = static_cast<decltype(to.vertCount)>(from.vertCount);
		to.triCount = static_cast<decltype(to.triCount)>(from.triCount);
		to.borderCount = static_cast<decltype(to.borderCount)>(from.borderCount);
		to.partitionCount = static_cast<decltype(to.partitionCount)>(from.partitionCount);
		to.aabbTreeCount = static_cast<decltype(to.aabbTreeCount)>(from.aabbTreeCount);
		to.numSubModels = static_cast<decltype(to.numSubModels)>(from.numSubModels);
		to.numBrushes = static_cast<decltype(to.numBrushes)>(from.numBrushes);
		to.smodelNodeCount = static_cast<decltype(to.smodelNodeCount)>(from.smodelNodeCount);
		std::memcpy(to.dynEntCount, from.dynEntCount, sizeof(from.dynEntCount));
		to.checksum = static_cast<decltype(to.checksum)>(from.checksum);
		return to;
	}

	struct cStaticModel_s
	{
		std::uint32_t xmodel;
		float origin[3];
		float invScaledAxis[3][3];
		Bounds absBounds;
	};

	static_assert(sizeof(cStaticModel_s) == 0x4C);
	static_assert(offsetof(cStaticModel_s, xmodel) == 0x0);
	static_assert(offsetof(cStaticModel_s, origin) == 0x4);
	static_assert(offsetof(cStaticModel_s, invScaledAxis) == 0x10);
	static_assert(offsetof(cStaticModel_s, absBounds) == 0x34);

	inline cStaticModel_s Convert(const Game::cStaticModel_s& from)
	{
		cStaticModel_s to{};
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		std::memcpy(to.invScaledAxis, from.invScaledAxis, sizeof(to.invScaledAxis));
		to.absBounds = Convert(from.absBounds);
		return to;
	}

	inline Game::cStaticModel_s Convert(const cStaticModel_s& from)
	{
		Game::cStaticModel_s to{};
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		std::memcpy(to.invScaledAxis, from.invScaledAxis, sizeof(from.invScaledAxis));
		to.absBounds = Convert(from.absBounds);
		return to;
	}

	struct ClipMaterial
	{
		std::uint32_t name;
		std::int32_t surfaceFlags;
		std::int32_t contents;
	};

	static_assert(sizeof(ClipMaterial) == 0xC);
	static_assert(offsetof(ClipMaterial, name) == 0x0);
	static_assert(offsetof(ClipMaterial, surfaceFlags) == 0x4);
	static_assert(offsetof(ClipMaterial, contents) == 0x8);

	inline ClipMaterial Convert(const Game::ClipMaterial& from)
	{
		ClipMaterial to{};
		to.surfaceFlags = static_cast<std::int32_t>(from.surfaceFlags);
		to.contents = static_cast<std::int32_t>(from.contents);
		return to;
	}

	inline Game::ClipMaterial Convert(const ClipMaterial& from)
	{
		Game::ClipMaterial to{};
		to.surfaceFlags = static_cast<decltype(to.surfaceFlags)>(from.surfaceFlags);
		to.contents = static_cast<decltype(to.contents)>(from.contents);
		return to;
	}

	struct cNode_t
	{
		std::uint32_t plane;
		std::int16_t children[2];
	};

	static_assert(sizeof(cNode_t) == 0x8);
	static_assert(offsetof(cNode_t, plane) == 0x0);
	static_assert(offsetof(cNode_t, children) == 0x4);

	inline cNode_t Convert(const Game::cNode_t& from)
	{
		cNode_t to{};
		std::memcpy(to.children, from.children, sizeof(to.children));
		return to;
	}

	inline Game::cNode_t Convert(const cNode_t& from)
	{
		Game::cNode_t to{};
		std::memcpy(to.children, from.children, sizeof(from.children));
		return to;
	}

	struct cLeaf_t
	{
		std::uint16_t firstCollAabbIndex;
		std::uint16_t collAabbCount;
		std::int32_t brushContents;
		std::int32_t terrainContents;
		Bounds bounds;
		std::int32_t leafBrushNode;
	};

	static_assert(sizeof(cLeaf_t) == 0x28);
	static_assert(offsetof(cLeaf_t, firstCollAabbIndex) == 0x0);
	static_assert(offsetof(cLeaf_t, collAabbCount) == 0x2);
	static_assert(offsetof(cLeaf_t, brushContents) == 0x4);
	static_assert(offsetof(cLeaf_t, terrainContents) == 0x8);
	static_assert(offsetof(cLeaf_t, bounds) == 0xC);
	static_assert(offsetof(cLeaf_t, leafBrushNode) == 0x24);

	inline cLeaf_t Convert(const Game::cLeaf_t& from)
	{
		cLeaf_t to{};
		to.firstCollAabbIndex = static_cast<std::uint16_t>(from.firstCollAabbIndex);
		to.collAabbCount = static_cast<std::uint16_t>(from.collAabbCount);
		to.brushContents = static_cast<std::int32_t>(from.brushContents);
		to.terrainContents = static_cast<std::int32_t>(from.terrainContents);
		to.bounds = Convert(from.bounds);
		to.leafBrushNode = static_cast<std::int32_t>(from.leafBrushNode);
		return to;
	}

	inline Game::cLeaf_t Convert(const cLeaf_t& from)
	{
		Game::cLeaf_t to{};
		to.firstCollAabbIndex = static_cast<decltype(to.firstCollAabbIndex)>(from.firstCollAabbIndex);
		to.collAabbCount = static_cast<decltype(to.collAabbCount)>(from.collAabbCount);
		to.brushContents = static_cast<decltype(to.brushContents)>(from.brushContents);
		to.terrainContents = static_cast<decltype(to.terrainContents)>(from.terrainContents);
		to.bounds = Convert(from.bounds);
		to.leafBrushNode = static_cast<decltype(to.leafBrushNode)>(from.leafBrushNode);
		return to;
	}

	struct cLeafBrushNodeLeaf_t
	{
		std::uint32_t brushes;
	};

	static_assert(sizeof(cLeafBrushNodeLeaf_t) == 0x4);
	static_assert(offsetof(cLeafBrushNodeLeaf_t, brushes) == 0x0);

	inline cLeafBrushNodeLeaf_t Convert(const Game::cLeafBrushNodeLeaf_t&)
	{
		cLeafBrushNodeLeaf_t to{};
		return to;
	}

	inline Game::cLeafBrushNodeLeaf_t Convert(const cLeafBrushNodeLeaf_t&)
	{
		Game::cLeafBrushNodeLeaf_t to{};
		return to;
	}

	struct cLeafBrushNodeChildren_t
	{
		float dist;
		float range;
		std::uint16_t childOffset[2];
	};

	static_assert(sizeof(cLeafBrushNodeChildren_t) == 0xC);
	static_assert(offsetof(cLeafBrushNodeChildren_t, dist) == 0x0);
	static_assert(offsetof(cLeafBrushNodeChildren_t, range) == 0x4);
	static_assert(offsetof(cLeafBrushNodeChildren_t, childOffset) == 0x8);

	inline cLeafBrushNodeChildren_t Convert(const Game::cLeafBrushNodeChildren_t& from)
	{
		cLeafBrushNodeChildren_t to{};
		to.dist = static_cast<float>(from.dist);
		to.range = static_cast<float>(from.range);
		std::memcpy(to.childOffset, from.childOffset, sizeof(to.childOffset));
		return to;
	}

	inline Game::cLeafBrushNodeChildren_t Convert(const cLeafBrushNodeChildren_t& from)
	{
		Game::cLeafBrushNodeChildren_t to{};
		to.dist = static_cast<decltype(to.dist)>(from.dist);
		to.range = static_cast<decltype(to.range)>(from.range);
		std::memcpy(to.childOffset, from.childOffset, sizeof(from.childOffset));
		return to;
	}

	union cLeafBrushNodeData_t
	{
		cLeafBrushNodeLeaf_t leaf;
		cLeafBrushNodeChildren_t children;
	};

	static_assert(sizeof(cLeafBrushNodeData_t) == 0xC);
	static_assert(offsetof(cLeafBrushNodeData_t, leaf) == 0x0);
	static_assert(offsetof(cLeafBrushNodeData_t, children) == 0x0);

	inline cLeafBrushNodeData_t Convert(const Game::cLeafBrushNodeData_t& from)
	{
		cLeafBrushNodeData_t to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::cLeafBrushNodeData_t Convert(const cLeafBrushNodeData_t& from)
	{
		Game::cLeafBrushNodeData_t to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct cLeafBrushNode_s
	{
		std::uint8_t axis;
		std::int16_t leafBrushCount;
		std::int32_t contents;
		cLeafBrushNodeData_t data;
	};

	static_assert(sizeof(cLeafBrushNode_s) == 0x14);
	static_assert(offsetof(cLeafBrushNode_s, axis) == 0x0);
	static_assert(offsetof(cLeafBrushNode_s, leafBrushCount) == 0x2);
	static_assert(offsetof(cLeafBrushNode_s, contents) == 0x4);
	static_assert(offsetof(cLeafBrushNode_s, data) == 0x8);

	inline cLeafBrushNode_s Convert(const Game::cLeafBrushNode_s& from)
	{
		cLeafBrushNode_s to{};
		to.axis = static_cast<std::uint8_t>(from.axis);
		to.leafBrushCount = static_cast<std::int16_t>(from.leafBrushCount);
		to.contents = static_cast<std::int32_t>(from.contents);
		to.data = Convert(from.data);
		return to;
	}

	inline Game::cLeafBrushNode_s Convert(const cLeafBrushNode_s& from)
	{
		Game::cLeafBrushNode_s to{};
		to.axis = static_cast<decltype(to.axis)>(from.axis);
		to.leafBrushCount = static_cast<decltype(to.leafBrushCount)>(from.leafBrushCount);
		to.contents = static_cast<decltype(to.contents)>(from.contents);
		to.data = Convert(from.data);
		return to;
	}

	struct CollisionBorder
	{
		float distEq[3];
		float zBase;
		float zSlope;
		float start;
		float length;
	};

	static_assert(sizeof(CollisionBorder) == 0x1C);
	static_assert(offsetof(CollisionBorder, distEq) == 0x0);
	static_assert(offsetof(CollisionBorder, zBase) == 0xC);
	static_assert(offsetof(CollisionBorder, zSlope) == 0x10);
	static_assert(offsetof(CollisionBorder, start) == 0x14);
	static_assert(offsetof(CollisionBorder, length) == 0x18);

	inline CollisionBorder Convert(const Game::CollisionBorder& from)
	{
		CollisionBorder to{};
		std::memcpy(to.distEq, from.distEq, sizeof(to.distEq));
		to.zBase = static_cast<float>(from.zBase);
		to.zSlope = static_cast<float>(from.zSlope);
		to.start = static_cast<float>(from.start);
		to.length = static_cast<float>(from.length);
		return to;
	}

	inline Game::CollisionBorder Convert(const CollisionBorder& from)
	{
		Game::CollisionBorder to{};
		std::memcpy(to.distEq, from.distEq, sizeof(from.distEq));
		to.zBase = static_cast<decltype(to.zBase)>(from.zBase);
		to.zSlope = static_cast<decltype(to.zSlope)>(from.zSlope);
		to.start = static_cast<decltype(to.start)>(from.start);
		to.length = static_cast<decltype(to.length)>(from.length);
		return to;
	}

	struct CollisionPartition
	{
		std::uint8_t triCount;
		std::uint8_t borderCount;
		std::uint8_t firstVertSegment;
		std::int32_t firstTri;
		std::uint32_t borders;
	};

	static_assert(sizeof(CollisionPartition) == 0xC);
	static_assert(offsetof(CollisionPartition, triCount) == 0x0);
	static_assert(offsetof(CollisionPartition, borderCount) == 0x1);
	static_assert(offsetof(CollisionPartition, firstVertSegment) == 0x2);
	static_assert(offsetof(CollisionPartition, firstTri) == 0x4);
	static_assert(offsetof(CollisionPartition, borders) == 0x8);

	inline CollisionPartition Convert(const Game::CollisionPartition& from)
	{
		CollisionPartition to{};
		to.triCount = static_cast<std::uint8_t>(from.triCount);
		to.borderCount = static_cast<std::uint8_t>(from.borderCount);
		to.firstVertSegment = static_cast<std::uint8_t>(from.firstVertSegment);
		to.firstTri = static_cast<std::int32_t>(from.firstTri);
		return to;
	}

	inline Game::CollisionPartition Convert(const CollisionPartition& from)
	{
		Game::CollisionPartition to{};
		to.triCount = static_cast<decltype(to.triCount)>(from.triCount);
		to.borderCount = static_cast<decltype(to.borderCount)>(from.borderCount);
		to.firstVertSegment = static_cast<decltype(to.firstVertSegment)>(from.firstVertSegment);
		to.firstTri = static_cast<decltype(to.firstTri)>(from.firstTri);
		return to;
	}

	union CollisionAabbTreeIndex
	{
		std::int32_t firstChildIndex;
		std::int32_t partitionIndex;
	};

	static_assert(sizeof(CollisionAabbTreeIndex) == 0x4);
	static_assert(offsetof(CollisionAabbTreeIndex, firstChildIndex) == 0x0);
	static_assert(offsetof(CollisionAabbTreeIndex, partitionIndex) == 0x0);

	inline CollisionAabbTreeIndex Convert(const Game::CollisionAabbTreeIndex& from)
	{
		CollisionAabbTreeIndex to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::CollisionAabbTreeIndex Convert(const CollisionAabbTreeIndex& from)
	{
		Game::CollisionAabbTreeIndex to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct CollisionAabbTree
	{
		float midPoint[3];
		std::uint16_t materialIndex;
		std::uint16_t childCount;
		float halfSize[3];
		CollisionAabbTreeIndex u;
	};

	static_assert(sizeof(CollisionAabbTree) == 0x20);
	static_assert(offsetof(CollisionAabbTree, midPoint) == 0x0);
	static_assert(offsetof(CollisionAabbTree, materialIndex) == 0xC);
	static_assert(offsetof(CollisionAabbTree, childCount) == 0xE);
	static_assert(offsetof(CollisionAabbTree, halfSize) == 0x10);
	static_assert(offsetof(CollisionAabbTree, u) == 0x1C);

	inline CollisionAabbTree Convert(const Game::CollisionAabbTree& from)
	{
		CollisionAabbTree to{};
		std::memcpy(to.midPoint, from.midPoint, sizeof(to.midPoint));
		to.materialIndex = static_cast<std::uint16_t>(from.materialIndex);
		to.childCount = static_cast<std::uint16_t>(from.childCount);
		std::memcpy(to.halfSize, from.halfSize, sizeof(to.halfSize));
		to.u = Convert(from.u);
		return to;
	}

	inline Game::CollisionAabbTree Convert(const CollisionAabbTree& from)
	{
		Game::CollisionAabbTree to{};
		std::memcpy(to.midPoint, from.midPoint, sizeof(from.midPoint));
		to.materialIndex = static_cast<decltype(to.materialIndex)>(from.materialIndex);
		to.childCount = static_cast<decltype(to.childCount)>(from.childCount);
		std::memcpy(to.halfSize, from.halfSize, sizeof(from.halfSize));
		to.u = Convert(from.u);
		return to;
	}

	struct cmodel_t
	{
		Bounds bounds;
		float radius;
		cLeaf_t leaf;
	};

	static_assert(sizeof(cmodel_t) == 0x44);
	static_assert(offsetof(cmodel_t, bounds) == 0x0);
	static_assert(offsetof(cmodel_t, radius) == 0x18);
	static_assert(offsetof(cmodel_t, leaf) == 0x1C);

	inline cmodel_t Convert(const Game::cmodel_t& from)
	{
		cmodel_t to{};
		to.bounds = Convert(from.bounds);
		to.radius = static_cast<float>(from.radius);
		to.leaf = Convert(from.leaf);
		return to;
	}

	inline Game::cmodel_t Convert(const cmodel_t& from)
	{
		Game::cmodel_t to{};
		to.bounds = Convert(from.bounds);
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		to.leaf = Convert(from.leaf);
		return to;
	}

	struct MapTriggers
	{
		std::uint32_t count;
		std::uint32_t models;
		std::uint32_t hullCount;
		std::uint32_t hulls;
		std::uint32_t slabCount;
		std::uint32_t slabs;
	};

	static_assert(sizeof(MapTriggers) == 0x18);
	static_assert(offsetof(MapTriggers, count) == 0x0);
	static_assert(offsetof(MapTriggers, models) == 0x4);
	static_assert(offsetof(MapTriggers, hullCount) == 0x8);
	static_assert(offsetof(MapTriggers, hulls) == 0xC);
	static_assert(offsetof(MapTriggers, slabCount) == 0x10);
	static_assert(offsetof(MapTriggers, slabs) == 0x14);

	inline MapTriggers Convert(const Game::MapTriggers& from)
	{
		MapTriggers to{};
		to.count = static_cast<std::uint32_t>(from.count);
		to.hullCount = static_cast<std::uint32_t>(from.hullCount);
		to.slabCount = static_cast<std::uint32_t>(from.slabCount);
		return to;
	}

	inline Game::MapTriggers Convert(const MapTriggers& from)
	{
		Game::MapTriggers to{};
		to.count = static_cast<decltype(to.count)>(from.count);
		to.hullCount = static_cast<decltype(to.hullCount)>(from.hullCount);
		to.slabCount = static_cast<decltype(to.slabCount)>(from.slabCount);
		return to;
	}

	struct MapEnts
	{
		std::uint32_t name;
		std::uint32_t entityString;
		std::int32_t numEntityChars;
		MapTriggers trigger;
		std::uint32_t stages;
		std::int8_t stageCount;
	};

	static_assert(sizeof(MapEnts) == 0x2C);
	static_assert(offsetof(MapEnts, name) == 0x0);
	static_assert(offsetof(MapEnts, entityString) == 0x4);
	static_assert(offsetof(MapEnts, numEntityChars) == 0x8);
	static_assert(offsetof(MapEnts, trigger) == 0xC);
	static_assert(offsetof(MapEnts, stages) == 0x24);
	static_assert(offsetof(MapEnts, stageCount) == 0x28);

	inline MapEnts Convert(const Game::MapEnts& from)
	{
		MapEnts to{};
		to.numEntityChars = static_cast<std::int32_t>(from.numEntityChars);
		to.trigger = Convert(from.trigger);
		to.stageCount = static_cast<std::int8_t>(from.stageCount);
		return to;
	}

	inline Game::MapEnts Convert(const MapEnts& from)
	{
		Game::MapEnts to{};
		to.numEntityChars = static_cast<decltype(to.numEntityChars)>(from.numEntityChars);
		to.trigger = Convert(from.trigger);
		to.stageCount = static_cast<decltype(to.stageCount)>(from.stageCount);
		return to;
	}

	struct TriggerModel
	{
		std::int32_t contents;
		std::uint16_t hullCount;
		std::uint16_t firstHull;
	};

	static_assert(sizeof(TriggerModel) == 0x8);
	static_assert(offsetof(TriggerModel, contents) == 0x0);
	static_assert(offsetof(TriggerModel, hullCount) == 0x4);
	static_assert(offsetof(TriggerModel, firstHull) == 0x6);

	inline TriggerModel Convert(const Game::TriggerModel& from)
	{
		TriggerModel to{};
		to.contents = static_cast<std::int32_t>(from.contents);
		to.hullCount = static_cast<std::uint16_t>(from.hullCount);
		to.firstHull = static_cast<std::uint16_t>(from.firstHull);
		return to;
	}

	inline Game::TriggerModel Convert(const TriggerModel& from)
	{
		Game::TriggerModel to{};
		to.contents = static_cast<decltype(to.contents)>(from.contents);
		to.hullCount = static_cast<decltype(to.hullCount)>(from.hullCount);
		to.firstHull = static_cast<decltype(to.firstHull)>(from.firstHull);
		return to;
	}

	struct TriggerHull
	{
		Bounds bounds;
		std::int32_t contents;
		std::uint16_t slabCount;
		std::uint16_t firstSlab;
	};

	static_assert(sizeof(TriggerHull) == 0x20);
	static_assert(offsetof(TriggerHull, bounds) == 0x0);
	static_assert(offsetof(TriggerHull, contents) == 0x18);
	static_assert(offsetof(TriggerHull, slabCount) == 0x1C);
	static_assert(offsetof(TriggerHull, firstSlab) == 0x1E);

	inline TriggerHull Convert(const Game::TriggerHull& from)
	{
		TriggerHull to{};
		to.bounds = Convert(from.bounds);
		to.contents = static_cast<std::int32_t>(from.contents);
		to.slabCount = static_cast<std::uint16_t>(from.slabCount);
		to.firstSlab = static_cast<std::uint16_t>(from.firstSlab);
		return to;
	}

	inline Game::TriggerHull Convert(const TriggerHull& from)
	{
		Game::TriggerHull to{};
		to.bounds = Convert(from.bounds);
		to.contents = static_cast<decltype(to.contents)>(from.contents);
		to.slabCount = static_cast<decltype(to.slabCount)>(from.slabCount);
		to.firstSlab = static_cast<decltype(to.firstSlab)>(from.firstSlab);
		return to;
	}

	struct TriggerSlab
	{
		float dir[3];
		float midPoint;
		float halfSize;
	};

	static_assert(sizeof(TriggerSlab) == 0x14);
	static_assert(offsetof(TriggerSlab, dir) == 0x0);
	static_assert(offsetof(TriggerSlab, midPoint) == 0xC);
	static_assert(offsetof(TriggerSlab, halfSize) == 0x10);

	inline TriggerSlab Convert(const Game::TriggerSlab& from)
	{
		TriggerSlab to{};
		std::memcpy(to.dir, from.dir, sizeof(to.dir));
		to.midPoint = static_cast<float>(from.midPoint);
		to.halfSize = static_cast<float>(from.halfSize);
		return to;
	}

	inline Game::TriggerSlab Convert(const TriggerSlab& from)
	{
		Game::TriggerSlab to{};
		std::memcpy(to.dir, from.dir, sizeof(from.dir));
		to.midPoint = static_cast<decltype(to.midPoint)>(from.midPoint);
		to.halfSize = static_cast<decltype(to.halfSize)>(from.halfSize);
		return to;
	}

	struct Stage
	{
		std::uint32_t name;
		float origin[3];
		std::uint16_t triggerIndex;
		std::int8_t sunPrimaryLightIndex;
	};

	static_assert(sizeof(Stage) == 0x14);
	static_assert(offsetof(Stage, name) == 0x0);
	static_assert(offsetof(Stage, origin) == 0x4);
	static_assert(offsetof(Stage, triggerIndex) == 0x10);
	static_assert(offsetof(Stage, sunPrimaryLightIndex) == 0x12);

	inline Stage Convert(const Game::Stage& from)
	{
		Stage to{};
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		to.triggerIndex = static_cast<std::uint16_t>(from.triggerIndex);
		to.sunPrimaryLightIndex = static_cast<std::int8_t>(from.sunPrimaryLightIndex);
		return to;
	}

	inline Game::Stage Convert(const Stage& from)
	{
		Game::Stage to{};
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		to.triggerIndex = static_cast<decltype(to.triggerIndex)>(from.triggerIndex);
		to.sunPrimaryLightIndex = static_cast<decltype(to.sunPrimaryLightIndex)>(from.sunPrimaryLightIndex);
		return to;
	}

	struct SModelAabbNode
	{
		Bounds bounds;
		std::uint16_t firstChild;
		std::uint16_t childCount;
	};

	static_assert(sizeof(SModelAabbNode) == 0x1C);
	static_assert(offsetof(SModelAabbNode, bounds) == 0x0);
	static_assert(offsetof(SModelAabbNode, firstChild) == 0x18);
	static_assert(offsetof(SModelAabbNode, childCount) == 0x1A);

	inline SModelAabbNode Convert(const Game::SModelAabbNode& from)
	{
		SModelAabbNode to{};
		to.bounds = Convert(from.bounds);
		to.firstChild = static_cast<std::uint16_t>(from.firstChild);
		to.childCount = static_cast<std::uint16_t>(from.childCount);
		return to;
	}

	inline Game::SModelAabbNode Convert(const SModelAabbNode& from)
	{
		Game::SModelAabbNode to{};
		to.bounds = Convert(from.bounds);
		to.firstChild = static_cast<decltype(to.firstChild)>(from.firstChild);
		to.childCount = static_cast<decltype(to.childCount)>(from.childCount);
		return to;
	}

	struct GfxPlacement
	{
		float quat[4];
		float origin[3];
	};

	static_assert(sizeof(GfxPlacement) == 0x1C);
	static_assert(offsetof(GfxPlacement, quat) == 0x0);
	static_assert(offsetof(GfxPlacement, origin) == 0x10);

	inline GfxPlacement Convert(const Game::GfxPlacement& from)
	{
		GfxPlacement to{};
		std::memcpy(to.quat, from.quat, sizeof(to.quat));
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		return to;
	}

	inline Game::GfxPlacement Convert(const GfxPlacement& from)
	{
		Game::GfxPlacement to{};
		std::memcpy(to.quat, from.quat, sizeof(from.quat));
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		return to;
	}

	struct DynEntityDef
	{
		std::int32_t type;
		GfxPlacement pose;
		std::uint32_t xModel;
		std::uint16_t brushModel;
		std::uint16_t physicsBrushModel;
		std::uint32_t destroyFx;
		std::uint32_t physPreset;
		std::int32_t health;
		PhysMass mass;
		std::int32_t contents;
	};

	static_assert(sizeof(DynEntityDef) == 0x5C);
	static_assert(offsetof(DynEntityDef, type) == 0x0);
	static_assert(offsetof(DynEntityDef, pose) == 0x4);
	static_assert(offsetof(DynEntityDef, xModel) == 0x20);
	static_assert(offsetof(DynEntityDef, brushModel) == 0x24);
	static_assert(offsetof(DynEntityDef, physicsBrushModel) == 0x26);
	static_assert(offsetof(DynEntityDef, destroyFx) == 0x28);
	static_assert(offsetof(DynEntityDef, physPreset) == 0x2C);
	static_assert(offsetof(DynEntityDef, health) == 0x30);
	static_assert(offsetof(DynEntityDef, mass) == 0x34);
	static_assert(offsetof(DynEntityDef, contents) == 0x58);

	inline DynEntityDef Convert(const Game::DynEntityDef& from)
	{
		DynEntityDef to{};
		to.type = static_cast<std::int32_t>(from.type);
		to.pose = Convert(from.pose);
		to.brushModel = static_cast<std::uint16_t>(from.brushModel);
		to.physicsBrushModel = static_cast<std::uint16_t>(from.physicsBrushModel);
		to.health = static_cast<std::int32_t>(from.health);
		to.mass = Convert(from.mass);
		to.contents = static_cast<std::int32_t>(from.contents);
		return to;
	}

	inline Game::DynEntityDef Convert(const DynEntityDef& from)
	{
		Game::DynEntityDef to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.pose = Convert(from.pose);
		to.brushModel = static_cast<decltype(to.brushModel)>(from.brushModel);
		to.physicsBrushModel = static_cast<decltype(to.physicsBrushModel)>(from.physicsBrushModel);
		to.health = static_cast<decltype(to.health)>(from.health);
		to.mass = Convert(from.mass);
		to.contents = static_cast<decltype(to.contents)>(from.contents);
		return to;
	}

	struct FxEffectDef
	{
		std::uint32_t name;
		std::int32_t flags;
		std::int32_t totalSize;
		std::int32_t msecLoopingLife;
		std::int32_t elemDefCountLooping;
		std::int32_t elemDefCountOneShot;
		std::int32_t elemDefCountEmission;
		std::uint32_t elemDefs;
	};

	static_assert(sizeof(FxEffectDef) == 0x20);
	static_assert(offsetof(FxEffectDef, name) == 0x0);
	static_assert(offsetof(FxEffectDef, flags) == 0x4);
	static_assert(offsetof(FxEffectDef, totalSize) == 0x8);
	static_assert(offsetof(FxEffectDef, msecLoopingLife) == 0xC);
	static_assert(offsetof(FxEffectDef, elemDefCountLooping) == 0x10);
	static_assert(offsetof(FxEffectDef, elemDefCountOneShot) == 0x14);
	static_assert(offsetof(FxEffectDef, elemDefCountEmission) == 0x18);
	static_assert(offsetof(FxEffectDef, elemDefs) == 0x1C);

	inline FxEffectDef Convert(const Game::FxEffectDef& from)
	{
		FxEffectDef to{};
		to.flags = static_cast<std::int32_t>(from.flags);
		to.totalSize = static_cast<std::int32_t>(from.totalSize);
		to.msecLoopingLife = static_cast<std::int32_t>(from.msecLoopingLife);
		to.elemDefCountLooping = static_cast<std::int32_t>(from.elemDefCountLooping);
		to.elemDefCountOneShot = static_cast<std::int32_t>(from.elemDefCountOneShot);
		to.elemDefCountEmission = static_cast<std::int32_t>(from.elemDefCountEmission);
		return to;
	}

	inline Game::FxEffectDef Convert(const FxEffectDef& from)
	{
		Game::FxEffectDef to{};
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.totalSize = static_cast<decltype(to.totalSize)>(from.totalSize);
		to.msecLoopingLife = static_cast<decltype(to.msecLoopingLife)>(from.msecLoopingLife);
		to.elemDefCountLooping = static_cast<decltype(to.elemDefCountLooping)>(from.elemDefCountLooping);
		to.elemDefCountOneShot = static_cast<decltype(to.elemDefCountOneShot)>(from.elemDefCountOneShot);
		to.elemDefCountEmission = static_cast<decltype(to.elemDefCountEmission)>(from.elemDefCountEmission);
		return to;
	}

	struct FxSpawnDefLooping
	{
		std::int32_t intervalMsec;
		std::int32_t count;
	};

	static_assert(sizeof(FxSpawnDefLooping) == 0x8);
	static_assert(offsetof(FxSpawnDefLooping, intervalMsec) == 0x0);
	static_assert(offsetof(FxSpawnDefLooping, count) == 0x4);

	inline FxSpawnDefLooping Convert(const Game::FxSpawnDefLooping& from)
	{
		FxSpawnDefLooping to{};
		to.intervalMsec = static_cast<std::int32_t>(from.intervalMsec);
		to.count = static_cast<std::int32_t>(from.count);
		return to;
	}

	inline Game::FxSpawnDefLooping Convert(const FxSpawnDefLooping& from)
	{
		Game::FxSpawnDefLooping to{};
		to.intervalMsec = static_cast<decltype(to.intervalMsec)>(from.intervalMsec);
		to.count = static_cast<decltype(to.count)>(from.count);
		return to;
	}

	struct FxIntRange
	{
		std::int32_t base;
		std::int32_t amplitude;
	};

	static_assert(sizeof(FxIntRange) == 0x8);
	static_assert(offsetof(FxIntRange, base) == 0x0);
	static_assert(offsetof(FxIntRange, amplitude) == 0x4);

	inline FxIntRange Convert(const Game::FxIntRange& from)
	{
		FxIntRange to{};
		to.base = static_cast<std::int32_t>(from.base);
		to.amplitude = static_cast<std::int32_t>(from.amplitude);
		return to;
	}

	inline Game::FxIntRange Convert(const FxIntRange& from)
	{
		Game::FxIntRange to{};
		to.base = static_cast<decltype(to.base)>(from.base);
		to.amplitude = static_cast<decltype(to.amplitude)>(from.amplitude);
		return to;
	}

	struct FxSpawnDefOneShot
	{
		FxIntRange count;
	};

	static_assert(sizeof(FxSpawnDefOneShot) == 0x8);
	static_assert(offsetof(FxSpawnDefOneShot, count) == 0x0);

	inline FxSpawnDefOneShot Convert(const Game::FxSpawnDefOneShot& from)
	{
		FxSpawnDefOneShot to{};
		to.count = Convert(from.count);
		return to;
	}

	inline Game::FxSpawnDefOneShot Convert(const FxSpawnDefOneShot& from)
	{
		Game::FxSpawnDefOneShot to{};
		to.count = Convert(from.count);
		return to;
	}

	union FxSpawnDef
	{
		FxSpawnDefLooping looping;
		FxSpawnDefOneShot oneShot;
	};

	static_assert(sizeof(FxSpawnDef) == 0x8);
	static_assert(offsetof(FxSpawnDef, looping) == 0x0);
	static_assert(offsetof(FxSpawnDef, oneShot) == 0x0);

	inline FxSpawnDef Convert(const Game::FxSpawnDef& from)
	{
		FxSpawnDef to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::FxSpawnDef Convert(const FxSpawnDef& from)
	{
		Game::FxSpawnDef to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct FxFloatRange
	{
		float base;
		float amplitude;
	};

	static_assert(sizeof(FxFloatRange) == 0x8);
	static_assert(offsetof(FxFloatRange, base) == 0x0);
	static_assert(offsetof(FxFloatRange, amplitude) == 0x4);

	inline FxFloatRange Convert(const Game::FxFloatRange& from)
	{
		FxFloatRange to{};
		to.base = static_cast<float>(from.base);
		to.amplitude = static_cast<float>(from.amplitude);
		return to;
	}

	inline Game::FxFloatRange Convert(const FxFloatRange& from)
	{
		Game::FxFloatRange to{};
		to.base = static_cast<decltype(to.base)>(from.base);
		to.amplitude = static_cast<decltype(to.amplitude)>(from.amplitude);
		return to;
	}

	struct FxElemAtlas
	{
		std::int8_t behavior;
		std::int8_t index;
		std::int8_t fps;
		std::int8_t loopCount;
		std::int8_t colIndexBits;
		std::int8_t rowIndexBits;
		std::int16_t entryCount;
	};

	static_assert(sizeof(FxElemAtlas) == 0x8);
	static_assert(offsetof(FxElemAtlas, behavior) == 0x0);
	static_assert(offsetof(FxElemAtlas, index) == 0x1);
	static_assert(offsetof(FxElemAtlas, fps) == 0x2);
	static_assert(offsetof(FxElemAtlas, loopCount) == 0x3);
	static_assert(offsetof(FxElemAtlas, colIndexBits) == 0x4);
	static_assert(offsetof(FxElemAtlas, rowIndexBits) == 0x5);
	static_assert(offsetof(FxElemAtlas, entryCount) == 0x6);

	inline FxElemAtlas Convert(const Game::FxElemAtlas& from)
	{
		FxElemAtlas to{};
		to.behavior = static_cast<std::int8_t>(from.behavior);
		to.index = static_cast<std::int8_t>(from.index);
		to.fps = static_cast<std::int8_t>(from.fps);
		to.loopCount = static_cast<std::int8_t>(from.loopCount);
		to.colIndexBits = static_cast<std::int8_t>(from.colIndexBits);
		to.rowIndexBits = static_cast<std::int8_t>(from.rowIndexBits);
		to.entryCount = static_cast<std::int16_t>(from.entryCount);
		return to;
	}

	inline Game::FxElemAtlas Convert(const FxElemAtlas& from)
	{
		Game::FxElemAtlas to{};
		to.behavior = static_cast<decltype(to.behavior)>(from.behavior);
		to.index = static_cast<decltype(to.index)>(from.index);
		to.fps = static_cast<decltype(to.fps)>(from.fps);
		to.loopCount = static_cast<decltype(to.loopCount)>(from.loopCount);
		to.colIndexBits = static_cast<decltype(to.colIndexBits)>(from.colIndexBits);
		to.rowIndexBits = static_cast<decltype(to.rowIndexBits)>(from.rowIndexBits);
		to.entryCount = static_cast<decltype(to.entryCount)>(from.entryCount);
		return to;
	}

	union FxEffectDefRef
	{
		std::uint32_t handle;
		std::uint32_t name;
	};

	static_assert(sizeof(FxEffectDefRef) == 0x4);
	static_assert(offsetof(FxEffectDefRef, handle) == 0x0);
	static_assert(offsetof(FxEffectDefRef, name) == 0x0);

	inline FxEffectDefRef Convert(const Game::FxEffectDefRef& from)
	{
		FxEffectDefRef to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::FxEffectDefRef Convert(const FxEffectDefRef& from)
	{
		Game::FxEffectDefRef to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	union FxElemVisuals
	{
		std::uint32_t anonymous;
		std::uint32_t material;
		std::uint32_t model;
		FxEffectDefRef effectDef;
		std::uint32_t soundName;
	};

	static_assert(sizeof(FxElemVisuals) == 0x4);
	static_assert(offsetof(FxElemVisuals, anonymous) == 0x0);
	static_assert(offsetof(FxElemVisuals, material) == 0x0);
	static_assert(offsetof(FxElemVisuals, model) == 0x0);
	static_assert(offsetof(FxElemVisuals, effectDef) == 0x0);
	static_assert(offsetof(FxElemVisuals, soundName) == 0x0);

	inline FxElemVisuals Convert(const Game::FxElemVisuals& from)
	{
		FxElemVisuals to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::FxElemVisuals Convert(const FxElemVisuals& from)
	{
		Game::FxElemVisuals to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	union FxElemDefVisuals
	{
		std::uint32_t markArray;
		std::uint32_t array;
		FxElemVisuals instance;
	};

	static_assert(sizeof(FxElemDefVisuals) == 0x4);
	static_assert(offsetof(FxElemDefVisuals, markArray) == 0x0);
	static_assert(offsetof(FxElemDefVisuals, array) == 0x0);
	static_assert(offsetof(FxElemDefVisuals, instance) == 0x0);

	inline FxElemDefVisuals Convert(const Game::FxElemDefVisuals& from)
	{
		FxElemDefVisuals to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::FxElemDefVisuals Convert(const FxElemDefVisuals& from)
	{
		Game::FxElemDefVisuals to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	union FxElemExtendedDefPtr
	{
		std::uint32_t trailDef;
		std::uint32_t sparkFountainDef;
		std::uint32_t unknownDef;
	};

	static_assert(sizeof(FxElemExtendedDefPtr) == 0x4);
	static_assert(offsetof(FxElemExtendedDefPtr, trailDef) == 0x0);
	static_assert(offsetof(FxElemExtendedDefPtr, sparkFountainDef) == 0x0);
	static_assert(offsetof(FxElemExtendedDefPtr, unknownDef) == 0x0);

	inline FxElemExtendedDefPtr Convert(const Game::FxElemExtendedDefPtr& from)
	{
		FxElemExtendedDefPtr to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::FxElemExtendedDefPtr Convert(const FxElemExtendedDefPtr& from)
	{
		Game::FxElemExtendedDefPtr to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct FxElemDef
	{
		std::int32_t flags;
		FxSpawnDef spawn;
		FxFloatRange spawnRange;
		FxFloatRange fadeInRange;
		FxFloatRange fadeOutRange;
		float spawnFrustumCullRadius;
		FxIntRange spawnDelayMsec;
		FxIntRange lifeSpanMsec;
		FxFloatRange spawnOrigin[3];
		FxFloatRange spawnOffsetRadius;
		FxFloatRange spawnOffsetHeight;
		FxFloatRange spawnAngles[3];
		FxFloatRange angularVelocity[3];
		FxFloatRange initialRotation;
		FxFloatRange gravity;
		FxFloatRange reflectionFactor;
		FxElemAtlas atlas;
		std::int8_t elemType;
		std::int8_t visualCount;
		std::int8_t velIntervalCount;
		std::int8_t visStateIntervalCount;
		std::uint32_t velSamples;
		std::uint32_t visSamples;
		FxElemDefVisuals visuals;
		Bounds collBounds;
		FxEffectDefRef effectOnImpact;
		FxEffectDefRef effectOnDeath;
		FxEffectDefRef effectEmitted;
		FxFloatRange emitDist;
		FxFloatRange emitDistVariance;
		FxElemExtendedDefPtr extended;
		std::int8_t sortOrder;
		std::int8_t lightingFrac;
		std::int8_t useItemClip;
		std::int8_t fadeInfo;
	};

	static_assert(sizeof(FxElemDef) == 0xFC);
	static_assert(offsetof(FxElemDef, flags) == 0x0);
	static_assert(offsetof(FxElemDef, spawn) == 0x4);
	static_assert(offsetof(FxElemDef, spawnRange) == 0xC);
	static_assert(offsetof(FxElemDef, fadeInRange) == 0x14);
	static_assert(offsetof(FxElemDef, fadeOutRange) == 0x1C);
	static_assert(offsetof(FxElemDef, spawnFrustumCullRadius) == 0x24);
	static_assert(offsetof(FxElemDef, spawnDelayMsec) == 0x28);
	static_assert(offsetof(FxElemDef, lifeSpanMsec) == 0x30);
	static_assert(offsetof(FxElemDef, spawnOrigin) == 0x38);
	static_assert(offsetof(FxElemDef, spawnOffsetRadius) == 0x50);
	static_assert(offsetof(FxElemDef, spawnOffsetHeight) == 0x58);
	static_assert(offsetof(FxElemDef, spawnAngles) == 0x60);
	static_assert(offsetof(FxElemDef, angularVelocity) == 0x78);
	static_assert(offsetof(FxElemDef, initialRotation) == 0x90);
	static_assert(offsetof(FxElemDef, gravity) == 0x98);
	static_assert(offsetof(FxElemDef, reflectionFactor) == 0xA0);
	static_assert(offsetof(FxElemDef, atlas) == 0xA8);
	static_assert(offsetof(FxElemDef, elemType) == 0xB0);
	static_assert(offsetof(FxElemDef, visualCount) == 0xB1);
	static_assert(offsetof(FxElemDef, velIntervalCount) == 0xB2);
	static_assert(offsetof(FxElemDef, visStateIntervalCount) == 0xB3);
	static_assert(offsetof(FxElemDef, velSamples) == 0xB4);
	static_assert(offsetof(FxElemDef, visSamples) == 0xB8);
	static_assert(offsetof(FxElemDef, visuals) == 0xBC);
	static_assert(offsetof(FxElemDef, collBounds) == 0xC0);
	static_assert(offsetof(FxElemDef, effectOnImpact) == 0xD8);
	static_assert(offsetof(FxElemDef, effectOnDeath) == 0xDC);
	static_assert(offsetof(FxElemDef, effectEmitted) == 0xE0);
	static_assert(offsetof(FxElemDef, emitDist) == 0xE4);
	static_assert(offsetof(FxElemDef, emitDistVariance) == 0xEC);
	static_assert(offsetof(FxElemDef, extended) == 0xF4);
	static_assert(offsetof(FxElemDef, sortOrder) == 0xF8);
	static_assert(offsetof(FxElemDef, lightingFrac) == 0xF9);
	static_assert(offsetof(FxElemDef, useItemClip) == 0xFA);
	static_assert(offsetof(FxElemDef, fadeInfo) == 0xFB);

	inline FxElemDef Convert(const Game::FxElemDef& from)
	{
		FxElemDef to{};
		to.flags = static_cast<std::int32_t>(from.flags);
		to.spawn = Convert(from.spawn);
		to.spawnRange = Convert(from.spawnRange);
		to.fadeInRange = Convert(from.fadeInRange);
		to.fadeOutRange = Convert(from.fadeOutRange);
		to.spawnFrustumCullRadius = static_cast<float>(from.spawnFrustumCullRadius);
		to.spawnDelayMsec = Convert(from.spawnDelayMsec);
		to.lifeSpanMsec = Convert(from.lifeSpanMsec);
		for (std::size_t i = 0; i < std::size(to.spawnOrigin); ++i)
		{
			to.spawnOrigin[i] = Convert(from.spawnOrigin[i]);
		}
		to.spawnOffsetRadius = Convert(from.spawnOffsetRadius);
		to.spawnOffsetHeight = Convert(from.spawnOffsetHeight);
		for (std::size_t i = 0; i < std::size(to.spawnAngles); ++i)
		{
			to.spawnAngles[i] = Convert(from.spawnAngles[i]);
		}
		for (std::size_t i = 0; i < std::size(to.angularVelocity); ++i)
		{
			to.angularVelocity[i] = Convert(from.angularVelocity[i]);
		}
		to.initialRotation = Convert(from.initialRotation);
		to.gravity = Convert(from.gravity);
		to.reflectionFactor = Convert(from.reflectionFactor);
		to.atlas = Convert(from.atlas);
		to.elemType = static_cast<std::int8_t>(from.elemType);
		to.visualCount = static_cast<std::int8_t>(from.visualCount);
		to.velIntervalCount = static_cast<std::int8_t>(from.velIntervalCount);
		to.visStateIntervalCount = static_cast<std::int8_t>(from.visStateIntervalCount);
		to.visuals = Convert(from.visuals);
		to.collBounds = Convert(from.collBounds);
		to.effectOnImpact = Convert(from.effectOnImpact);
		to.effectOnDeath = Convert(from.effectOnDeath);
		to.effectEmitted = Convert(from.effectEmitted);
		to.emitDist = Convert(from.emitDist);
		to.emitDistVariance = Convert(from.emitDistVariance);
		to.extended = Convert(from.extended);
		to.sortOrder = static_cast<std::int8_t>(from.sortOrder);
		to.lightingFrac = static_cast<std::int8_t>(from.lightingFrac);
		to.useItemClip = static_cast<std::int8_t>(from.useItemClip);
		to.fadeInfo = static_cast<std::int8_t>(from.fadeInfo);
		return to;
	}

	inline Game::FxElemDef Convert(const FxElemDef& from)
	{
		Game::FxElemDef to{};
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.spawn = Convert(from.spawn);
		to.spawnRange = Convert(from.spawnRange);
		to.fadeInRange = Convert(from.fadeInRange);
		to.fadeOutRange = Convert(from.fadeOutRange);
		to.spawnFrustumCullRadius = static_cast<decltype(to.spawnFrustumCullRadius)>(from.spawnFrustumCullRadius);
		to.spawnDelayMsec = Convert(from.spawnDelayMsec);
		to.lifeSpanMsec = Convert(from.lifeSpanMsec);
		for (std::size_t i = 0; i < std::size(to.spawnOrigin); ++i)
		{
			to.spawnOrigin[i] = Convert(from.spawnOrigin[i]);
		}
		to.spawnOffsetRadius = Convert(from.spawnOffsetRadius);
		to.spawnOffsetHeight = Convert(from.spawnOffsetHeight);
		for (std::size_t i = 0; i < std::size(to.spawnAngles); ++i)
		{
			to.spawnAngles[i] = Convert(from.spawnAngles[i]);
		}
		for (std::size_t i = 0; i < std::size(to.angularVelocity); ++i)
		{
			to.angularVelocity[i] = Convert(from.angularVelocity[i]);
		}
		to.initialRotation = Convert(from.initialRotation);
		to.gravity = Convert(from.gravity);
		to.reflectionFactor = Convert(from.reflectionFactor);
		to.atlas = Convert(from.atlas);
		to.elemType = static_cast<decltype(to.elemType)>(from.elemType);
		to.visualCount = static_cast<decltype(to.visualCount)>(from.visualCount);
		to.velIntervalCount = static_cast<decltype(to.velIntervalCount)>(from.velIntervalCount);
		to.visStateIntervalCount = static_cast<decltype(to.visStateIntervalCount)>(from.visStateIntervalCount);
		to.visuals = Convert(from.visuals);
		to.collBounds = Convert(from.collBounds);
		to.effectOnImpact = Convert(from.effectOnImpact);
		to.effectOnDeath = Convert(from.effectOnDeath);
		to.effectEmitted = Convert(from.effectEmitted);
		to.emitDist = Convert(from.emitDist);
		to.emitDistVariance = Convert(from.emitDistVariance);
		to.extended = Convert(from.extended);
		to.sortOrder = static_cast<decltype(to.sortOrder)>(from.sortOrder);
		to.lightingFrac = static_cast<decltype(to.lightingFrac)>(from.lightingFrac);
		to.useItemClip = static_cast<decltype(to.useItemClip)>(from.useItemClip);
		to.fadeInfo = static_cast<decltype(to.fadeInfo)>(from.fadeInfo);
		return to;
	}

	struct FxElemVec3Range
	{
		float base[3];
		float amplitude[3];
	};

	static_assert(sizeof(FxElemVec3Range) == 0x18);
	static_assert(offsetof(FxElemVec3Range, base) == 0x0);
	static_assert(offsetof(FxElemVec3Range, amplitude) == 0xC);

	inline FxElemVec3Range Convert(const Game::FxElemVec3Range& from)
	{
		FxElemVec3Range to{};
		std::memcpy(to.base, from.base, sizeof(to.base));
		std::memcpy(to.amplitude, from.amplitude, sizeof(to.amplitude));
		return to;
	}

	inline Game::FxElemVec3Range Convert(const FxElemVec3Range& from)
	{
		Game::FxElemVec3Range to{};
		std::memcpy(to.base, from.base, sizeof(from.base));
		std::memcpy(to.amplitude, from.amplitude, sizeof(from.amplitude));
		return to;
	}

	struct FxElemVelStateInFrame
	{
		FxElemVec3Range velocity;
		FxElemVec3Range totalDelta;
	};

	static_assert(sizeof(FxElemVelStateInFrame) == 0x30);
	static_assert(offsetof(FxElemVelStateInFrame, velocity) == 0x0);
	static_assert(offsetof(FxElemVelStateInFrame, totalDelta) == 0x18);

	inline FxElemVelStateInFrame Convert(const Game::FxElemVelStateInFrame& from)
	{
		FxElemVelStateInFrame to{};
		to.velocity = Convert(from.velocity);
		to.totalDelta = Convert(from.totalDelta);
		return to;
	}

	inline Game::FxElemVelStateInFrame Convert(const FxElemVelStateInFrame& from)
	{
		Game::FxElemVelStateInFrame to{};
		to.velocity = Convert(from.velocity);
		to.totalDelta = Convert(from.totalDelta);
		return to;
	}

	struct FxElemVelStateSample
	{
		FxElemVelStateInFrame local;
		FxElemVelStateInFrame world;
	};

	static_assert(sizeof(FxElemVelStateSample) == 0x60);
	static_assert(offsetof(FxElemVelStateSample, local) == 0x0);
	static_assert(offsetof(FxElemVelStateSample, world) == 0x30);

	inline FxElemVelStateSample Convert(const Game::FxElemVelStateSample& from)
	{
		FxElemVelStateSample to{};
		to.local = Convert(from.local);
		to.world = Convert(from.world);
		return to;
	}

	inline Game::FxElemVelStateSample Convert(const FxElemVelStateSample& from)
	{
		Game::FxElemVelStateSample to{};
		to.local = Convert(from.local);
		to.world = Convert(from.world);
		return to;
	}

	struct FxElemVisualState
	{
		std::uint8_t color[4];
		float rotationDelta;
		float rotationTotal;
		float size[2];
		float scale;
	};

	static_assert(sizeof(FxElemVisualState) == 0x18);
	static_assert(offsetof(FxElemVisualState, color) == 0x0);
	static_assert(offsetof(FxElemVisualState, rotationDelta) == 0x4);
	static_assert(offsetof(FxElemVisualState, rotationTotal) == 0x8);
	static_assert(offsetof(FxElemVisualState, size) == 0xC);
	static_assert(offsetof(FxElemVisualState, scale) == 0x14);

	inline FxElemVisualState Convert(const Game::FxElemVisualState& from)
	{
		FxElemVisualState to{};
		std::memcpy(to.color, from.color, sizeof(to.color));
		to.rotationDelta = static_cast<float>(from.rotationDelta);
		to.rotationTotal = static_cast<float>(from.rotationTotal);
		std::memcpy(to.size, from.size, sizeof(to.size));
		to.scale = static_cast<float>(from.scale);
		return to;
	}

	inline Game::FxElemVisualState Convert(const FxElemVisualState& from)
	{
		Game::FxElemVisualState to{};
		std::memcpy(to.color, from.color, sizeof(from.color));
		to.rotationDelta = static_cast<decltype(to.rotationDelta)>(from.rotationDelta);
		to.rotationTotal = static_cast<decltype(to.rotationTotal)>(from.rotationTotal);
		std::memcpy(to.size, from.size, sizeof(from.size));
		to.scale = static_cast<decltype(to.scale)>(from.scale);
		return to;
	}

	struct FxElemVisStateSample
	{
		FxElemVisualState base;
		FxElemVisualState amplitude;
	};

	static_assert(sizeof(FxElemVisStateSample) == 0x30);
	static_assert(offsetof(FxElemVisStateSample, base) == 0x0);
	static_assert(offsetof(FxElemVisStateSample, amplitude) == 0x18);

	inline FxElemVisStateSample Convert(const Game::FxElemVisStateSample& from)
	{
		FxElemVisStateSample to{};
		to.base = Convert(from.base);
		to.amplitude = Convert(from.amplitude);
		return to;
	}

	inline Game::FxElemVisStateSample Convert(const FxElemVisStateSample& from)
	{
		Game::FxElemVisStateSample to{};
		to.base = Convert(from.base);
		to.amplitude = Convert(from.amplitude);
		return to;
	}

	struct FxElemMarkVisuals
	{
		std::uint32_t materials[2];
	};

	static_assert(sizeof(FxElemMarkVisuals) == 0x8);
	static_assert(offsetof(FxElemMarkVisuals, materials) == 0x0);

	inline FxElemMarkVisuals Convert(const Game::FxElemMarkVisuals&)
	{
		FxElemMarkVisuals to{};
		return to;
	}

	inline Game::FxElemMarkVisuals Convert(const FxElemMarkVisuals&)
	{
		Game::FxElemMarkVisuals to{};
		return to;
	}

	struct FxTrailDef
	{
		std::int32_t scrollTimeMsec;
		std::int32_t repeatDist;
		float invSplitDist;
		float invSplitArcDist;
		float invSplitTime;
		std::int32_t vertCount;
		std::uint32_t verts;
		std::int32_t indCount;
		std::uint32_t inds;
	};

	static_assert(sizeof(FxTrailDef) == 0x24);
	static_assert(offsetof(FxTrailDef, scrollTimeMsec) == 0x0);
	static_assert(offsetof(FxTrailDef, repeatDist) == 0x4);
	static_assert(offsetof(FxTrailDef, invSplitDist) == 0x8);
	static_assert(offsetof(FxTrailDef, invSplitArcDist) == 0xC);
	static_assert(offsetof(FxTrailDef, invSplitTime) == 0x10);
	static_assert(offsetof(FxTrailDef, vertCount) == 0x14);
	static_assert(offsetof(FxTrailDef, verts) == 0x18);
	static_assert(offsetof(FxTrailDef, indCount) == 0x1C);
	static_assert(offsetof(FxTrailDef, inds) == 0x20);

	inline FxTrailDef Convert(const Game::FxTrailDef& from)
	{
		FxTrailDef to{};
		to.scrollTimeMsec = static_cast<std::int32_t>(from.scrollTimeMsec);
		to.repeatDist = static_cast<std::int32_t>(from.repeatDist);
		to.invSplitDist = static_cast<float>(from.invSplitDist);
		to.invSplitArcDist = static_cast<float>(from.invSplitArcDist);
		to.invSplitTime = static_cast<float>(from.invSplitTime);
		to.vertCount = static_cast<std::int32_t>(from.vertCount);
		to.indCount = static_cast<std::int32_t>(from.indCount);
		return to;
	}

	inline Game::FxTrailDef Convert(const FxTrailDef& from)
	{
		Game::FxTrailDef to{};
		to.scrollTimeMsec = static_cast<decltype(to.scrollTimeMsec)>(from.scrollTimeMsec);
		to.repeatDist = static_cast<decltype(to.repeatDist)>(from.repeatDist);
		to.invSplitDist = static_cast<decltype(to.invSplitDist)>(from.invSplitDist);
		to.invSplitArcDist = static_cast<decltype(to.invSplitArcDist)>(from.invSplitArcDist);
		to.invSplitTime = static_cast<decltype(to.invSplitTime)>(from.invSplitTime);
		to.vertCount = static_cast<decltype(to.vertCount)>(from.vertCount);
		to.indCount = static_cast<decltype(to.indCount)>(from.indCount);
		return to;
	}

	struct FxTrailVertex
	{
		float pos[2];
		float normal[2];
		float texCoord;
	};

	static_assert(sizeof(FxTrailVertex) == 0x14);
	static_assert(offsetof(FxTrailVertex, pos) == 0x0);
	static_assert(offsetof(FxTrailVertex, normal) == 0x8);
	static_assert(offsetof(FxTrailVertex, texCoord) == 0x10);

	inline FxTrailVertex Convert(const Game::FxTrailVertex& from)
	{
		FxTrailVertex to{};
		std::memcpy(to.pos, from.pos, sizeof(to.pos));
		std::memcpy(to.normal, from.normal, sizeof(to.normal));
		to.texCoord = static_cast<float>(from.texCoord);
		return to;
	}

	inline Game::FxTrailVertex Convert(const FxTrailVertex& from)
	{
		Game::FxTrailVertex to{};
		std::memcpy(to.pos, from.pos, sizeof(from.pos));
		std::memcpy(to.normal, from.normal, sizeof(from.normal));
		to.texCoord = static_cast<decltype(to.texCoord)>(from.texCoord);
		return to;
	}

	struct FxSparkFountainDef
	{
		float gravity;
		float bounceFrac;
		float bounceRand;
		float sparkSpacing;
		float sparkLength;
		std::int32_t sparkCount;
		float loopTime;
		float velMin;
		float velMax;
		float velConeFrac;
		float restSpeed;
		float boostTime;
		float boostFactor;
	};

	static_assert(sizeof(FxSparkFountainDef) == 0x34);
	static_assert(offsetof(FxSparkFountainDef, gravity) == 0x0);
	static_assert(offsetof(FxSparkFountainDef, bounceFrac) == 0x4);
	static_assert(offsetof(FxSparkFountainDef, bounceRand) == 0x8);
	static_assert(offsetof(FxSparkFountainDef, sparkSpacing) == 0xC);
	static_assert(offsetof(FxSparkFountainDef, sparkLength) == 0x10);
	static_assert(offsetof(FxSparkFountainDef, sparkCount) == 0x14);
	static_assert(offsetof(FxSparkFountainDef, loopTime) == 0x18);
	static_assert(offsetof(FxSparkFountainDef, velMin) == 0x1C);
	static_assert(offsetof(FxSparkFountainDef, velMax) == 0x20);
	static_assert(offsetof(FxSparkFountainDef, velConeFrac) == 0x24);
	static_assert(offsetof(FxSparkFountainDef, restSpeed) == 0x28);
	static_assert(offsetof(FxSparkFountainDef, boostTime) == 0x2C);
	static_assert(offsetof(FxSparkFountainDef, boostFactor) == 0x30);

	inline FxSparkFountainDef Convert(const Game::FxSparkFountainDef& from)
	{
		FxSparkFountainDef to{};
		to.gravity = static_cast<float>(from.gravity);
		to.bounceFrac = static_cast<float>(from.bounceFrac);
		to.bounceRand = static_cast<float>(from.bounceRand);
		to.sparkSpacing = static_cast<float>(from.sparkSpacing);
		to.sparkLength = static_cast<float>(from.sparkLength);
		to.sparkCount = static_cast<std::int32_t>(from.sparkCount);
		to.loopTime = static_cast<float>(from.loopTime);
		to.velMin = static_cast<float>(from.velMin);
		to.velMax = static_cast<float>(from.velMax);
		to.velConeFrac = static_cast<float>(from.velConeFrac);
		to.restSpeed = static_cast<float>(from.restSpeed);
		to.boostTime = static_cast<float>(from.boostTime);
		to.boostFactor = static_cast<float>(from.boostFactor);
		return to;
	}

	inline Game::FxSparkFountainDef Convert(const FxSparkFountainDef& from)
	{
		Game::FxSparkFountainDef to{};
		to.gravity = static_cast<decltype(to.gravity)>(from.gravity);
		to.bounceFrac = static_cast<decltype(to.bounceFrac)>(from.bounceFrac);
		to.bounceRand = static_cast<decltype(to.bounceRand)>(from.bounceRand);
		to.sparkSpacing = static_cast<decltype(to.sparkSpacing)>(from.sparkSpacing);
		to.sparkLength = static_cast<decltype(to.sparkLength)>(from.sparkLength);
		to.sparkCount = static_cast<decltype(to.sparkCount)>(from.sparkCount);
		to.loopTime = static_cast<decltype(to.loopTime)>(from.loopTime);
		to.velMin = static_cast<decltype(to.velMin)>(from.velMin);
		to.velMax = static_cast<decltype(to.velMax)>(from.velMax);
		to.velConeFrac = static_cast<decltype(to.velConeFrac)>(from.velConeFrac);
		to.restSpeed = static_cast<decltype(to.restSpeed)>(from.restSpeed);
		to.boostTime = static_cast<decltype(to.boostTime)>(from.boostTime);
		to.boostFactor = static_cast<decltype(to.boostFactor)>(from.boostFactor);
		return to;
	}

	struct DynEntityPose
	{
		GfxPlacement pose;
		float radius;
	};

	static_assert(sizeof(DynEntityPose) == 0x20);
	static_assert(offsetof(DynEntityPose, pose) == 0x0);
	static_assert(offsetof(DynEntityPose, radius) == 0x1C);

	inline DynEntityPose Convert(const Game::DynEntityPose& from)
	{
		DynEntityPose to{};
		to.pose = Convert(from.pose);
		to.radius = static_cast<float>(from.radius);
		return to;
	}

	inline Game::DynEntityPose Convert(const DynEntityPose& from)
	{
		Game::DynEntityPose to{};
		to.pose = Convert(from.pose);
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		return to;
	}

	struct DynEntityClient
	{
		std::int32_t physObjId;
		std::uint16_t flags;
		std::uint16_t lightingHandle;
		std::int32_t health;
	};

	static_assert(sizeof(DynEntityClient) == 0xC);
	static_assert(offsetof(DynEntityClient, physObjId) == 0x0);
	static_assert(offsetof(DynEntityClient, flags) == 0x4);
	static_assert(offsetof(DynEntityClient, lightingHandle) == 0x6);
	static_assert(offsetof(DynEntityClient, health) == 0x8);

	inline DynEntityClient Convert(const Game::DynEntityClient& from)
	{
		DynEntityClient to{};
		to.physObjId = static_cast<std::int32_t>(from.physObjId);
		to.flags = static_cast<std::uint16_t>(from.flags);
		to.lightingHandle = static_cast<std::uint16_t>(from.lightingHandle);
		to.health = static_cast<std::int32_t>(from.health);
		return to;
	}

	inline Game::DynEntityClient Convert(const DynEntityClient& from)
	{
		Game::DynEntityClient to{};
		to.physObjId = static_cast<decltype(to.physObjId)>(from.physObjId);
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.lightingHandle = static_cast<decltype(to.lightingHandle)>(from.lightingHandle);
		to.health = static_cast<decltype(to.health)>(from.health);
		return to;
	}

	struct DynEntityColl
	{
		std::uint16_t sector;
		std::uint16_t nextEntInSector;
		float linkMins[2];
		float linkMaxs[2];
	};

	static_assert(sizeof(DynEntityColl) == 0x14);
	static_assert(offsetof(DynEntityColl, sector) == 0x0);
	static_assert(offsetof(DynEntityColl, nextEntInSector) == 0x2);
	static_assert(offsetof(DynEntityColl, linkMins) == 0x4);
	static_assert(offsetof(DynEntityColl, linkMaxs) == 0xC);

	inline DynEntityColl Convert(const Game::DynEntityColl& from)
	{
		DynEntityColl to{};
		to.sector = static_cast<std::uint16_t>(from.sector);
		to.nextEntInSector = static_cast<std::uint16_t>(from.nextEntInSector);
		std::memcpy(to.linkMins, from.linkMins, sizeof(to.linkMins));
		std::memcpy(to.linkMaxs, from.linkMaxs, sizeof(to.linkMaxs));
		return to;
	}

	inline Game::DynEntityColl Convert(const DynEntityColl& from)
	{
		Game::DynEntityColl to{};
		to.sector = static_cast<decltype(to.sector)>(from.sector);
		to.nextEntInSector = static_cast<decltype(to.nextEntInSector)>(from.nextEntInSector);
		std::memcpy(to.linkMins, from.linkMins, sizeof(from.linkMins));
		std::memcpy(to.linkMaxs, from.linkMaxs, sizeof(from.linkMaxs));
		return to;
	}

	struct ComWorld
	{
		std::uint32_t name;
		std::int32_t isInUse;
		std::uint32_t primaryLightCount;
		std::uint32_t primaryLights;
	};

	static_assert(sizeof(ComWorld) == 0x10);
	static_assert(offsetof(ComWorld, name) == 0x0);
	static_assert(offsetof(ComWorld, isInUse) == 0x4);
	static_assert(offsetof(ComWorld, primaryLightCount) == 0x8);
	static_assert(offsetof(ComWorld, primaryLights) == 0xC);

	inline ComWorld Convert(const Game::ComWorld& from)
	{
		ComWorld to{};
		to.isInUse = static_cast<std::int32_t>(from.isInUse);
		to.primaryLightCount = static_cast<std::uint32_t>(from.primaryLightCount);
		return to;
	}

	inline Game::ComWorld Convert(const ComWorld& from)
	{
		Game::ComWorld to{};
		to.isInUse = static_cast<decltype(to.isInUse)>(from.isInUse);
		to.primaryLightCount = static_cast<decltype(to.primaryLightCount)>(from.primaryLightCount);
		return to;
	}

	struct ComPrimaryLight
	{
		std::int8_t type;
		std::int8_t canUseShadowMap;
		std::int8_t exponent;
		std::int8_t unused;
		float color[3];
		float dir[3];
		float origin[3];
		float radius;
		float cosHalfFovOuter;
		float cosHalfFovInner;
		float cosHalfFovExpanded;
		float rotationLimit;
		float translationLimit;
		std::uint32_t defName;
	};

	static_assert(sizeof(ComPrimaryLight) == 0x44);
	static_assert(offsetof(ComPrimaryLight, type) == 0x0);
	static_assert(offsetof(ComPrimaryLight, canUseShadowMap) == 0x1);
	static_assert(offsetof(ComPrimaryLight, exponent) == 0x2);
	static_assert(offsetof(ComPrimaryLight, unused) == 0x3);
	static_assert(offsetof(ComPrimaryLight, color) == 0x4);
	static_assert(offsetof(ComPrimaryLight, dir) == 0x10);
	static_assert(offsetof(ComPrimaryLight, origin) == 0x1C);
	static_assert(offsetof(ComPrimaryLight, radius) == 0x28);
	static_assert(offsetof(ComPrimaryLight, cosHalfFovOuter) == 0x2C);
	static_assert(offsetof(ComPrimaryLight, cosHalfFovInner) == 0x30);
	static_assert(offsetof(ComPrimaryLight, cosHalfFovExpanded) == 0x34);
	static_assert(offsetof(ComPrimaryLight, rotationLimit) == 0x38);
	static_assert(offsetof(ComPrimaryLight, translationLimit) == 0x3C);
	static_assert(offsetof(ComPrimaryLight, defName) == 0x40);

	inline ComPrimaryLight Convert(const Game::ComPrimaryLight& from)
	{
		ComPrimaryLight to{};
		to.type = static_cast<std::int8_t>(from.type);
		to.canUseShadowMap = static_cast<std::int8_t>(from.canUseShadowMap);
		to.exponent = static_cast<std::int8_t>(from.exponent);
		to.unused = static_cast<std::int8_t>(from.unused);
		std::memcpy(to.color, from.color, sizeof(to.color));
		std::memcpy(to.dir, from.dir, sizeof(to.dir));
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		to.radius = static_cast<float>(from.radius);
		to.cosHalfFovOuter = static_cast<float>(from.cosHalfFovOuter);
		to.cosHalfFovInner = static_cast<float>(from.cosHalfFovInner);
		to.cosHalfFovExpanded = static_cast<float>(from.cosHalfFovExpanded);
		to.rotationLimit = static_cast<float>(from.rotationLimit);
		to.translationLimit = static_cast<float>(from.translationLimit);
		return to;
	}

	inline Game::ComPrimaryLight Convert(const ComPrimaryLight& from)
	{
		Game::ComPrimaryLight to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.canUseShadowMap = static_cast<decltype(to.canUseShadowMap)>(from.canUseShadowMap);
		to.exponent = static_cast<decltype(to.exponent)>(from.exponent);
		to.unused = static_cast<decltype(to.unused)>(from.unused);
		std::memcpy(to.color, from.color, sizeof(from.color));
		std::memcpy(to.dir, from.dir, sizeof(from.dir));
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		to.cosHalfFovOuter = static_cast<decltype(to.cosHalfFovOuter)>(from.cosHalfFovOuter);
		to.cosHalfFovInner = static_cast<decltype(to.cosHalfFovInner)>(from.cosHalfFovInner);
		to.cosHalfFovExpanded = static_cast<decltype(to.cosHalfFovExpanded)>(from.cosHalfFovExpanded);
		to.rotationLimit = static_cast<decltype(to.rotationLimit)>(from.rotationLimit);
		to.translationLimit = static_cast<decltype(to.translationLimit)>(from.translationLimit);
		return to;
	}

	struct PathData
	{
		std::uint32_t nodeCount;
		std::uint32_t nodes;
		std::uint32_t basenodes;
		std::uint32_t chainNodeCount;
		std::uint32_t chainNodeForNode;
		std::uint32_t nodeForChainNode;
		std::int32_t visBytes;
		std::uint32_t pathVis;
		std::int32_t nodeTreeCount;
		std::uint32_t nodeTree;
	};

	static_assert(sizeof(PathData) == 0x28);
	static_assert(offsetof(PathData, nodeCount) == 0x0);
	static_assert(offsetof(PathData, nodes) == 0x4);
	static_assert(offsetof(PathData, basenodes) == 0x8);
	static_assert(offsetof(PathData, chainNodeCount) == 0xC);
	static_assert(offsetof(PathData, chainNodeForNode) == 0x10);
	static_assert(offsetof(PathData, nodeForChainNode) == 0x14);
	static_assert(offsetof(PathData, visBytes) == 0x18);
	static_assert(offsetof(PathData, pathVis) == 0x1C);
	static_assert(offsetof(PathData, nodeTreeCount) == 0x20);
	static_assert(offsetof(PathData, nodeTree) == 0x24);

	inline PathData Convert(const Game::PathData& from)
	{
		PathData to{};
		to.nodeCount = static_cast<std::uint32_t>(from.nodeCount);
		to.chainNodeCount = static_cast<std::uint32_t>(from.chainNodeCount);
		to.visBytes = static_cast<std::int32_t>(from.visBytes);
		to.nodeTreeCount = static_cast<std::int32_t>(from.nodeTreeCount);
		return to;
	}

	inline Game::PathData Convert(const PathData& from)
	{
		Game::PathData to{};
		to.nodeCount = static_cast<decltype(to.nodeCount)>(from.nodeCount);
		to.chainNodeCount = static_cast<decltype(to.chainNodeCount)>(from.chainNodeCount);
		to.visBytes = static_cast<decltype(to.visBytes)>(from.visBytes);
		to.nodeTreeCount = static_cast<decltype(to.nodeTreeCount)>(from.nodeTreeCount);
		return to;
	}

	struct VehicleTrack
	{
		std::uint32_t segments;
		std::uint32_t segmentCount;
	};

	static_assert(sizeof(VehicleTrack) == 0x8);
	static_assert(offsetof(VehicleTrack, segments) == 0x0);
	static_assert(offsetof(VehicleTrack, segmentCount) == 0x4);

	inline VehicleTrack Convert(const Game::VehicleTrack& from)
	{
		VehicleTrack to{};
		to.segmentCount = static_cast<std::uint32_t>(from.segmentCount);
		return to;
	}

	inline Game::VehicleTrack Convert(const VehicleTrack& from)
	{
		Game::VehicleTrack to{};
		to.segmentCount = static_cast<decltype(to.segmentCount)>(from.segmentCount);
		return to;
	}

	struct GameWorldSp
	{
		std::uint32_t name;
		PathData path;
		VehicleTrack vehicleTrack;
		std::uint32_t g_glassData;
	};

	static_assert(sizeof(GameWorldSp) == 0x38);
	static_assert(offsetof(GameWorldSp, name) == 0x0);
	static_assert(offsetof(GameWorldSp, path) == 0x4);
	static_assert(offsetof(GameWorldSp, vehicleTrack) == 0x2C);
	static_assert(offsetof(GameWorldSp, g_glassData) == 0x34);

	inline GameWorldSp Convert(const Game::GameWorldSp& from)
	{
		GameWorldSp to{};
		to.path = Convert(from.path);
		to.vehicleTrack = Convert(from.vehicleTrack);
		return to;
	}

	inline Game::GameWorldSp Convert(const GameWorldSp& from)
	{
		Game::GameWorldSp to{};
		to.path = Convert(from.path);
		to.vehicleTrack = Convert(from.vehicleTrack);
		return to;
	}

	union $23305223CFD097B6F79557BDD2047E6C
	{
		float minUseDistSq;
		std::int32_t error;
	};

	static_assert(sizeof($23305223CFD097B6F79557BDD2047E6C) == 0x4);
	static_assert(offsetof($23305223CFD097B6F79557BDD2047E6C, minUseDistSq) == 0x0);
	static_assert(offsetof($23305223CFD097B6F79557BDD2047E6C, error) == 0x0);

	inline $23305223CFD097B6F79557BDD2047E6C Convert(const Game::$23305223CFD097B6F79557BDD2047E6C& from)
	{
		$23305223CFD097B6F79557BDD2047E6C to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::$23305223CFD097B6F79557BDD2047E6C Convert(const $23305223CFD097B6F79557BDD2047E6C& from)
	{
		Game::$23305223CFD097B6F79557BDD2047E6C to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct pathnode_constant_t
	{
		std::int32_t type;
		std::uint16_t spawnflags;
		std::uint16_t targetname;
		std::uint16_t script_linkName;
		std::uint16_t script_noteworthy;
		std::uint16_t target;
		std::uint16_t animscript;
		std::int32_t animscriptfunc;
		float vOrigin[3];
		float fAngle;
		float forward[2];
		float fRadius;
		$23305223CFD097B6F79557BDD2047E6C ___u12;
		std::int16_t wOverlapNode[2];
		std::uint16_t totalLinkCount;
		std::uint32_t Links;
	};

	static_assert(sizeof(pathnode_constant_t) == 0x40);
	static_assert(offsetof(pathnode_constant_t, type) == 0x0);
	static_assert(offsetof(pathnode_constant_t, spawnflags) == 0x4);
	static_assert(offsetof(pathnode_constant_t, targetname) == 0x6);
	static_assert(offsetof(pathnode_constant_t, script_linkName) == 0x8);
	static_assert(offsetof(pathnode_constant_t, script_noteworthy) == 0xA);
	static_assert(offsetof(pathnode_constant_t, target) == 0xC);
	static_assert(offsetof(pathnode_constant_t, animscript) == 0xE);
	static_assert(offsetof(pathnode_constant_t, animscriptfunc) == 0x10);
	static_assert(offsetof(pathnode_constant_t, vOrigin) == 0x14);
	static_assert(offsetof(pathnode_constant_t, fAngle) == 0x20);
	static_assert(offsetof(pathnode_constant_t, forward) == 0x24);
	static_assert(offsetof(pathnode_constant_t, fRadius) == 0x2C);
	static_assert(offsetof(pathnode_constant_t, ___u12) == 0x30);
	static_assert(offsetof(pathnode_constant_t, wOverlapNode) == 0x34);
	static_assert(offsetof(pathnode_constant_t, totalLinkCount) == 0x38);
	static_assert(offsetof(pathnode_constant_t, Links) == 0x3C);

	inline pathnode_constant_t Convert(const Game::pathnode_constant_t& from)
	{
		pathnode_constant_t to{};
		to.type = static_cast<std::int32_t>(from.type);
		to.spawnflags = static_cast<std::uint16_t>(from.spawnflags);
		to.targetname = static_cast<std::uint16_t>(from.targetname);
		to.script_linkName = static_cast<std::uint16_t>(from.script_linkName);
		to.script_noteworthy = static_cast<std::uint16_t>(from.script_noteworthy);
		to.target = static_cast<std::uint16_t>(from.target);
		to.animscript = static_cast<std::uint16_t>(from.animscript);
		to.animscriptfunc = static_cast<std::int32_t>(from.animscriptfunc);
		std::memcpy(to.vOrigin, from.vOrigin, sizeof(to.vOrigin));
		to.fAngle = static_cast<float>(from.fAngle);
		std::memcpy(to.forward, from.forward, sizeof(to.forward));
		to.fRadius = static_cast<float>(from.fRadius);
		to.___u12 = Convert(from.___u12);
		std::memcpy(to.wOverlapNode, from.wOverlapNode, sizeof(to.wOverlapNode));
		to.totalLinkCount = static_cast<std::uint16_t>(from.totalLinkCount);
		return to;
	}

	inline Game::pathnode_constant_t Convert(const pathnode_constant_t& from)
	{
		Game::pathnode_constant_t to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.spawnflags = static_cast<decltype(to.spawnflags)>(from.spawnflags);
		to.targetname = static_cast<decltype(to.targetname)>(from.targetname);
		to.script_linkName = static_cast<decltype(to.script_linkName)>(from.script_linkName);
		to.script_noteworthy = static_cast<decltype(to.script_noteworthy)>(from.script_noteworthy);
		to.target = static_cast<decltype(to.target)>(from.target);
		to.animscript = static_cast<decltype(to.animscript)>(from.animscript);
		to.animscriptfunc = static_cast<decltype(to.animscriptfunc)>(from.animscriptfunc);
		std::memcpy(to.vOrigin, from.vOrigin, sizeof(from.vOrigin));
		to.fAngle = static_cast<decltype(to.fAngle)>(from.fAngle);
		std::memcpy(to.forward, from.forward, sizeof(from.forward));
		to.fRadius = static_cast<decltype(to.fRadius)>(from.fRadius);
		to.___u12 = Convert(from.___u12);
		std::memcpy(to.wOverlapNode, from.wOverlapNode, sizeof(from.wOverlapNode));
		to.totalLinkCount = static_cast<decltype(to.totalLinkCount)>(from.totalLinkCount);
		return to;
	}

	struct pathnode_dynamic_t
	{
		std::uint32_t pOwner;
		std::int32_t iFreeTime;
		std::int32_t iValidTime[3];
		std::int32_t dangerousNodeTime[3];
		std::int32_t inPlayerLOSTime;
		std::int16_t wLinkCount;
		std::int16_t wOverlapCount;
		std::int16_t turretEntNumber;
		std::int8_t userCount;
		bool hasBadPlaceLink;
	};

	static_assert(sizeof(pathnode_dynamic_t) == 0x2C);
	static_assert(offsetof(pathnode_dynamic_t, pOwner) == 0x0);
	static_assert(offsetof(pathnode_dynamic_t, iFreeTime) == 0x4);
	static_assert(offsetof(pathnode_dynamic_t, iValidTime) == 0x8);
	static_assert(offsetof(pathnode_dynamic_t, dangerousNodeTime) == 0x14);
	static_assert(offsetof(pathnode_dynamic_t, inPlayerLOSTime) == 0x20);
	static_assert(offsetof(pathnode_dynamic_t, wLinkCount) == 0x24);
	static_assert(offsetof(pathnode_dynamic_t, wOverlapCount) == 0x26);
	static_assert(offsetof(pathnode_dynamic_t, turretEntNumber) == 0x28);
	static_assert(offsetof(pathnode_dynamic_t, userCount) == 0x2A);
	static_assert(offsetof(pathnode_dynamic_t, hasBadPlaceLink) == 0x2B);

	inline pathnode_dynamic_t Convert(const Game::pathnode_dynamic_t& from)
	{
		pathnode_dynamic_t to{};
		to.iFreeTime = static_cast<std::int32_t>(from.iFreeTime);
		std::memcpy(to.iValidTime, from.iValidTime, sizeof(to.iValidTime));
		std::memcpy(to.dangerousNodeTime, from.dangerousNodeTime, sizeof(to.dangerousNodeTime));
		to.inPlayerLOSTime = static_cast<std::int32_t>(from.inPlayerLOSTime);
		to.wLinkCount = static_cast<std::int16_t>(from.wLinkCount);
		to.wOverlapCount = static_cast<std::int16_t>(from.wOverlapCount);
		to.turretEntNumber = static_cast<std::int16_t>(from.turretEntNumber);
		to.userCount = static_cast<std::int8_t>(from.userCount);
		to.hasBadPlaceLink = static_cast<bool>(from.hasBadPlaceLink);
		return to;
	}

	inline Game::pathnode_dynamic_t Convert(const pathnode_dynamic_t& from)
	{
		Game::pathnode_dynamic_t to{};
		to.iFreeTime = static_cast<decltype(to.iFreeTime)>(from.iFreeTime);
		std::memcpy(to.iValidTime, from.iValidTime, sizeof(from.iValidTime));
		std::memcpy(to.dangerousNodeTime, from.dangerousNodeTime, sizeof(from.dangerousNodeTime));
		to.inPlayerLOSTime = static_cast<decltype(to.inPlayerLOSTime)>(from.inPlayerLOSTime);
		to.wLinkCount = static_cast<decltype(to.wLinkCount)>(from.wLinkCount);
		to.wOverlapCount = static_cast<decltype(to.wOverlapCount)>(from.wOverlapCount);
		to.turretEntNumber = static_cast<decltype(to.turretEntNumber)>(from.turretEntNumber);
		to.userCount = static_cast<decltype(to.userCount)>(from.userCount);
		to.hasBadPlaceLink = static_cast<decltype(to.hasBadPlaceLink)>(from.hasBadPlaceLink);
		return to;
	}

	union $73F238679C0419BE2C31C6559E8604FC
	{
		float nodeCost;
		std::int32_t linkIndex;
	};

	static_assert(sizeof($73F238679C0419BE2C31C6559E8604FC) == 0x4);
	static_assert(offsetof($73F238679C0419BE2C31C6559E8604FC, nodeCost) == 0x0);
	static_assert(offsetof($73F238679C0419BE2C31C6559E8604FC, linkIndex) == 0x0);

	inline $73F238679C0419BE2C31C6559E8604FC Convert(const Game::$73F238679C0419BE2C31C6559E8604FC& from)
	{
		$73F238679C0419BE2C31C6559E8604FC to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::$73F238679C0419BE2C31C6559E8604FC Convert(const $73F238679C0419BE2C31C6559E8604FC& from)
	{
		Game::$73F238679C0419BE2C31C6559E8604FC to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct pathnode_transient_t
	{
		std::int32_t iSearchFrame;
		std::uint32_t pNextOpen;
		std::uint32_t pPrevOpen;
		std::uint32_t pParent;
		float fCost;
		float fHeuristic;
		$73F238679C0419BE2C31C6559E8604FC ___u6;
	};

	static_assert(sizeof(pathnode_transient_t) == 0x1C);
	static_assert(offsetof(pathnode_transient_t, iSearchFrame) == 0x0);
	static_assert(offsetof(pathnode_transient_t, pNextOpen) == 0x4);
	static_assert(offsetof(pathnode_transient_t, pPrevOpen) == 0x8);
	static_assert(offsetof(pathnode_transient_t, pParent) == 0xC);
	static_assert(offsetof(pathnode_transient_t, fCost) == 0x10);
	static_assert(offsetof(pathnode_transient_t, fHeuristic) == 0x14);
	static_assert(offsetof(pathnode_transient_t, ___u6) == 0x18);

	inline pathnode_transient_t Convert(const Game::pathnode_transient_t& from)
	{
		pathnode_transient_t to{};
		to.iSearchFrame = static_cast<std::int32_t>(from.iSearchFrame);
		to.fCost = static_cast<float>(from.fCost);
		to.fHeuristic = static_cast<float>(from.fHeuristic);
		to.___u6 = Convert(from.___u6);
		return to;
	}

	inline Game::pathnode_transient_t Convert(const pathnode_transient_t& from)
	{
		Game::pathnode_transient_t to{};
		to.iSearchFrame = static_cast<decltype(to.iSearchFrame)>(from.iSearchFrame);
		to.fCost = static_cast<decltype(to.fCost)>(from.fCost);
		to.fHeuristic = static_cast<decltype(to.fHeuristic)>(from.fHeuristic);
		to.___u6 = Convert(from.___u6);
		return to;
	}

	struct pathnode_t
	{
		pathnode_constant_t constant;
		pathnode_dynamic_t dynamic;
		pathnode_transient_t transient;
	};

	static_assert(sizeof(pathnode_t) == 0x88);
	static_assert(offsetof(pathnode_t, constant) == 0x0);
	static_assert(offsetof(pathnode_t, dynamic) == 0x40);
	static_assert(offsetof(pathnode_t, transient) == 0x6C);

	inline pathnode_t Convert(const Game::pathnode_t& from)
	{
		pathnode_t to{};
		to.constant = Convert(from.constant);
		to.dynamic = Convert(from.dynamic);
		to.transient = Convert(from.transient);
		return to;
	}

	inline Game::pathnode_t Convert(const pathnode_t& from)
	{
		Game::pathnode_t to{};
		to.constant = Convert(from.constant);
		to.dynamic = Convert(from.dynamic);
		to.transient = Convert(from.transient);
		return to;
	}

	struct pathlink_s
	{
		float fDist;
		std::uint16_t nodeNum;
		std::int8_t disconnectCount;
		std::int8_t negotiationLink;
		std::int8_t flags;
		std::int8_t ubBadPlaceCount[3];
	};

	static_assert(sizeof(pathlink_s) == 0xC);
	static_assert(offsetof(pathlink_s, fDist) == 0x0);
	static_assert(offsetof(pathlink_s, nodeNum) == 0x4);
	static_assert(offsetof(pathlink_s, disconnectCount) == 0x6);
	static_assert(offsetof(pathlink_s, negotiationLink) == 0x7);
	static_assert(offsetof(pathlink_s, flags) == 0x8);
	static_assert(offsetof(pathlink_s, ubBadPlaceCount) == 0x9);

	inline pathlink_s Convert(const Game::pathlink_s& from)
	{
		pathlink_s to{};
		to.fDist = static_cast<float>(from.fDist);
		to.nodeNum = static_cast<std::uint16_t>(from.nodeNum);
		to.disconnectCount = static_cast<std::int8_t>(from.disconnectCount);
		to.negotiationLink = static_cast<std::int8_t>(from.negotiationLink);
		to.flags = static_cast<std::int8_t>(from.flags);
		std::memcpy(to.ubBadPlaceCount, from.ubBadPlaceCount, sizeof(to.ubBadPlaceCount));
		return to;
	}

	inline Game::pathlink_s Convert(const pathlink_s& from)
	{
		Game::pathlink_s to{};
		to.fDist = static_cast<decltype(to.fDist)>(from.fDist);
		to.nodeNum = static_cast<decltype(to.nodeNum)>(from.nodeNum);
		to.disconnectCount = static_cast<decltype(to.disconnectCount)>(from.disconnectCount);
		to.negotiationLink = static_cast<decltype(to.negotiationLink)>(from.negotiationLink);
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		std::memcpy(to.ubBadPlaceCount, from.ubBadPlaceCount, sizeof(from.ubBadPlaceCount));
		return to;
	}

	struct pathbasenode_t
	{
		float vOrigin[3];
		std::uint32_t type;
	};

	static_assert(sizeof(pathbasenode_t) == 0x10);
	static_assert(offsetof(pathbasenode_t, vOrigin) == 0x0);
	static_assert(offsetof(pathbasenode_t, type) == 0xC);

	inline pathbasenode_t Convert(const Game::pathbasenode_t& from)
	{
		pathbasenode_t to{};
		std::memcpy(to.vOrigin, from.vOrigin, sizeof(to.vOrigin));
		to.type = static_cast<std::uint32_t>(from.type);
		return to;
	}

	inline Game::pathbasenode_t Convert(const pathbasenode_t& from)
	{
		Game::pathbasenode_t to{};
		std::memcpy(to.vOrigin, from.vOrigin, sizeof(from.vOrigin));
		to.type = static_cast<decltype(to.type)>(from.type);
		return to;
	}

	struct pathnode_tree_nodes_t
	{
		std::int32_t nodeCount;
		std::uint32_t nodes;
	};

	static_assert(sizeof(pathnode_tree_nodes_t) == 0x8);
	static_assert(offsetof(pathnode_tree_nodes_t, nodeCount) == 0x0);
	static_assert(offsetof(pathnode_tree_nodes_t, nodes) == 0x4);

	inline pathnode_tree_nodes_t Convert(const Game::pathnode_tree_nodes_t& from)
	{
		pathnode_tree_nodes_t to{};
		to.nodeCount = static_cast<std::int32_t>(from.nodeCount);
		return to;
	}

	inline Game::pathnode_tree_nodes_t Convert(const pathnode_tree_nodes_t& from)
	{
		Game::pathnode_tree_nodes_t to{};
		to.nodeCount = static_cast<decltype(to.nodeCount)>(from.nodeCount);
		return to;
	}

	union pathnode_tree_info_t
	{
		std::uint32_t child[2];
		pathnode_tree_nodes_t s;
	};

	static_assert(sizeof(pathnode_tree_info_t) == 0x8);
	static_assert(offsetof(pathnode_tree_info_t, child) == 0x0);
	static_assert(offsetof(pathnode_tree_info_t, s) == 0x0);

	inline pathnode_tree_info_t Convert(const Game::pathnode_tree_info_t& from)
	{
		pathnode_tree_info_t to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::pathnode_tree_info_t Convert(const pathnode_tree_info_t& from)
	{
		Game::pathnode_tree_info_t to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct pathnode_tree_t
	{
		std::int32_t axis;
		float dist;
		pathnode_tree_info_t u;
	};

	static_assert(sizeof(pathnode_tree_t) == 0x10);
	static_assert(offsetof(pathnode_tree_t, axis) == 0x0);
	static_assert(offsetof(pathnode_tree_t, dist) == 0x4);
	static_assert(offsetof(pathnode_tree_t, u) == 0x8);

	inline pathnode_tree_t Convert(const Game::pathnode_tree_t& from)
	{
		pathnode_tree_t to{};
		to.axis = static_cast<std::int32_t>(from.axis);
		to.dist = static_cast<float>(from.dist);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::pathnode_tree_t Convert(const pathnode_tree_t& from)
	{
		Game::pathnode_tree_t to{};
		to.axis = static_cast<decltype(to.axis)>(from.axis);
		to.dist = static_cast<decltype(to.dist)>(from.dist);
		to.u = Convert(from.u);
		return to;
	}

	struct VehicleTrackSegment
	{
		std::uint32_t targetName;
		std::uint32_t sectors;
		std::uint32_t sectorCount;
		std::uint32_t nextBranches;
		std::uint32_t nextBranchesCount;
		std::uint32_t prevBranches;
		std::uint32_t prevBranchesCount;
		float endEdgeDir[2];
		float endEdgeDist;
		float totalLength;
	};

	static_assert(sizeof(VehicleTrackSegment) == 0x2C);
	static_assert(offsetof(VehicleTrackSegment, targetName) == 0x0);
	static_assert(offsetof(VehicleTrackSegment, sectors) == 0x4);
	static_assert(offsetof(VehicleTrackSegment, sectorCount) == 0x8);
	static_assert(offsetof(VehicleTrackSegment, nextBranches) == 0xC);
	static_assert(offsetof(VehicleTrackSegment, nextBranchesCount) == 0x10);
	static_assert(offsetof(VehicleTrackSegment, prevBranches) == 0x14);
	static_assert(offsetof(VehicleTrackSegment, prevBranchesCount) == 0x18);
	static_assert(offsetof(VehicleTrackSegment, endEdgeDir) == 0x1C);
	static_assert(offsetof(VehicleTrackSegment, endEdgeDist) == 0x24);
	static_assert(offsetof(VehicleTrackSegment, totalLength) == 0x28);

	inline VehicleTrackSegment Convert(const Game::VehicleTrackSegment& from)
	{
		VehicleTrackSegment to{};
		to.sectorCount = static_cast<std::uint32_t>(from.sectorCount);
		to.nextBranchesCount = static_cast<std::uint32_t>(from.nextBranchesCount);
		to.prevBranchesCount = static_cast<std::uint32_t>(from.prevBranchesCount);
		std::memcpy(to.endEdgeDir, from.endEdgeDir, sizeof(to.endEdgeDir));
		to.endEdgeDist = static_cast<float>(from.endEdgeDist);
		to.totalLength = static_cast<float>(from.totalLength);
		return to;
	}

	inline Game::VehicleTrackSegment Convert(const VehicleTrackSegment& from)
	{
		Game::VehicleTrackSegment to{};
		to.sectorCount = static_cast<decltype(to.sectorCount)>(from.sectorCount);
		to.nextBranchesCount = static_cast<decltype(to.nextBranchesCount)>(from.nextBranchesCount);
		to.prevBranchesCount = static_cast<decltype(to.prevBranchesCount)>(from.prevBranchesCount);
		std::memcpy(to.endEdgeDir, from.endEdgeDir, sizeof(from.endEdgeDir));
		to.endEdgeDist = static_cast<decltype(to.endEdgeDist)>(from.endEdgeDist);
		to.totalLength = static_cast<decltype(to.totalLength)>(from.totalLength);
		return to;
	}

	struct VehicleTrackSector
	{
		float startEdgeDir[2];
		float startEdgeDist;
		float leftEdgeDir[2];
		float leftEdgeDist;
		float rightEdgeDir[2];
		float rightEdgeDist;
		float sectorLength;
		float sectorWidth;
		float totalPriorLength;
		float totalFollowingLength;
		std::uint32_t obstacles;
		std::uint32_t obstacleCount;
	};

	static_assert(sizeof(VehicleTrackSector) == 0x3C);
	static_assert(offsetof(VehicleTrackSector, startEdgeDir) == 0x0);
	static_assert(offsetof(VehicleTrackSector, startEdgeDist) == 0x8);
	static_assert(offsetof(VehicleTrackSector, leftEdgeDir) == 0xC);
	static_assert(offsetof(VehicleTrackSector, leftEdgeDist) == 0x14);
	static_assert(offsetof(VehicleTrackSector, rightEdgeDir) == 0x18);
	static_assert(offsetof(VehicleTrackSector, rightEdgeDist) == 0x20);
	static_assert(offsetof(VehicleTrackSector, sectorLength) == 0x24);
	static_assert(offsetof(VehicleTrackSector, sectorWidth) == 0x28);
	static_assert(offsetof(VehicleTrackSector, totalPriorLength) == 0x2C);
	static_assert(offsetof(VehicleTrackSector, totalFollowingLength) == 0x30);
	static_assert(offsetof(VehicleTrackSector, obstacles) == 0x34);
	static_assert(offsetof(VehicleTrackSector, obstacleCount) == 0x38);

	inline VehicleTrackSector Convert(const Game::VehicleTrackSector& from)
	{
		VehicleTrackSector to{};
		std::memcpy(to.startEdgeDir, from.startEdgeDir, sizeof(to.startEdgeDir));
		to.startEdgeDist = static_cast<float>(from.startEdgeDist);
		std::memcpy(to.leftEdgeDir, from.leftEdgeDir, sizeof(to.leftEdgeDir));
		to.leftEdgeDist = static_cast<float>(from.leftEdgeDist);
		std::memcpy(to.rightEdgeDir, from.rightEdgeDir, sizeof(to.rightEdgeDir));
		to.rightEdgeDist = static_cast<float>(from.rightEdgeDist);
		to.sectorLength = static_cast<float>(from.sectorLength);
		to.sectorWidth = static_cast<float>(from.sectorWidth);
		to.totalPriorLength = static_cast<float>(from.totalPriorLength);
		to.totalFollowingLength = static_cast<float>(from.totalFollowingLength);
		to.obstacleCount = static_cast<std::uint32_t>(from.obstacleCount);
		return to;
	}

	inline Game::VehicleTrackSector Convert(const VehicleTrackSector& from)
	{
		Game::VehicleTrackSector to{};
		std::memcpy(to.startEdgeDir, from.startEdgeDir, sizeof(from.startEdgeDir));
		to.startEdgeDist = static_cast<decltype(to.startEdgeDist)>(from.startEdgeDist);
		std::memcpy(to.leftEdgeDir, from.leftEdgeDir, sizeof(from.leftEdgeDir));
		to.leftEdgeDist = static_cast<decltype(to.leftEdgeDist)>(from.leftEdgeDist);
		std::memcpy(to.rightEdgeDir, from.rightEdgeDir, sizeof(from.rightEdgeDir));
		to.rightEdgeDist = static_cast<decltype(to.rightEdgeDist)>(from.rightEdgeDist);
		to.sectorLength = static_cast<decltype(to.sectorLength)>(from.sectorLength);
		to.sectorWidth = static_cast<decltype(to.sectorWidth)>(from.sectorWidth);
		to.totalPriorLength = static_cast<decltype(to.totalPriorLength)>(from.totalPriorLength);
		to.totalFollowingLength = static_cast<decltype(to.totalFollowingLength)>(from.totalFollowingLength);
		to.obstacleCount = static_cast<decltype(to.obstacleCount)>(from.obstacleCount);
		return to;
	}

	struct VehicleTrackObstacle
	{
		float origin[2];
		float radius;
	};

	static_assert(sizeof(VehicleTrackObstacle) == 0xC);
	static_assert(offsetof(VehicleTrackObstacle, origin) == 0x0);
	static_assert(offsetof(VehicleTrackObstacle, radius) == 0x8);

	inline VehicleTrackObstacle Convert(const Game::VehicleTrackObstacle& from)
	{
		VehicleTrackObstacle to{};
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		to.radius = static_cast<float>(from.radius);
		return to;
	}

	inline Game::VehicleTrackObstacle Convert(const VehicleTrackObstacle& from)
	{
		Game::VehicleTrackObstacle to{};
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		return to;
	}

	struct G_GlassData
	{
		std::uint32_t glassPieces;
		std::uint32_t pieceCount;
		std::uint16_t damageToWeaken;
		std::uint16_t damageToDestroy;
		std::uint32_t glassNameCount;
		std::uint32_t glassNames;
		std::int8_t pad[108];
	};

	static_assert(sizeof(G_GlassData) == 0x80);
	static_assert(offsetof(G_GlassData, glassPieces) == 0x0);
	static_assert(offsetof(G_GlassData, pieceCount) == 0x4);
	static_assert(offsetof(G_GlassData, damageToWeaken) == 0x8);
	static_assert(offsetof(G_GlassData, damageToDestroy) == 0xA);
	static_assert(offsetof(G_GlassData, glassNameCount) == 0xC);
	static_assert(offsetof(G_GlassData, glassNames) == 0x10);
	static_assert(offsetof(G_GlassData, pad) == 0x14);

	inline G_GlassData Convert(const Game::G_GlassData& from)
	{
		G_GlassData to{};
		to.pieceCount = static_cast<std::uint32_t>(from.pieceCount);
		to.damageToWeaken = static_cast<std::uint16_t>(from.damageToWeaken);
		to.damageToDestroy = static_cast<std::uint16_t>(from.damageToDestroy);
		to.glassNameCount = static_cast<std::uint32_t>(from.glassNameCount);
		std::memcpy(to.pad, from.pad, sizeof(to.pad));
		return to;
	}

	inline Game::G_GlassData Convert(const G_GlassData& from)
	{
		Game::G_GlassData to{};
		to.pieceCount = static_cast<decltype(to.pieceCount)>(from.pieceCount);
		to.damageToWeaken = static_cast<decltype(to.damageToWeaken)>(from.damageToWeaken);
		to.damageToDestroy = static_cast<decltype(to.damageToDestroy)>(from.damageToDestroy);
		to.glassNameCount = static_cast<decltype(to.glassNameCount)>(from.glassNameCount);
		std::memcpy(to.pad, from.pad, sizeof(from.pad));
		return to;
	}

	struct G_GlassPiece
	{
		std::uint16_t damageTaken;
		std::uint16_t collapseTime;
		std::int32_t lastStateChangeTime;
		std::int8_t impactDir;
		std::int8_t impactPos[2];
	};

	static_assert(sizeof(G_GlassPiece) == 0xC);
	static_assert(offsetof(G_GlassPiece, damageTaken) == 0x0);
	static_assert(offsetof(G_GlassPiece, collapseTime) == 0x2);
	static_assert(offsetof(G_GlassPiece, lastStateChangeTime) == 0x4);
	static_assert(offsetof(G_GlassPiece, impactDir) == 0x8);
	static_assert(offsetof(G_GlassPiece, impactPos) == 0x9);

	inline G_GlassPiece Convert(const Game::G_GlassPiece& from)
	{
		G_GlassPiece to{};
		to.damageTaken = static_cast<std::uint16_t>(from.damageTaken);
		to.collapseTime = static_cast<std::uint16_t>(from.collapseTime);
		to.lastStateChangeTime = static_cast<std::int32_t>(from.lastStateChangeTime);
		to.impactDir = static_cast<std::int8_t>(from.impactDir);
		std::memcpy(to.impactPos, from.impactPos, sizeof(to.impactPos));
		return to;
	}

	inline Game::G_GlassPiece Convert(const G_GlassPiece& from)
	{
		Game::G_GlassPiece to{};
		to.damageTaken = static_cast<decltype(to.damageTaken)>(from.damageTaken);
		to.collapseTime = static_cast<decltype(to.collapseTime)>(from.collapseTime);
		to.lastStateChangeTime = static_cast<decltype(to.lastStateChangeTime)>(from.lastStateChangeTime);
		to.impactDir = static_cast<decltype(to.impactDir)>(from.impactDir);
		std::memcpy(to.impactPos, from.impactPos, sizeof(from.impactPos));
		return to;
	}

	struct G_GlassName
	{
		std::uint32_t nameStr;
		std::uint16_t name;
		std::uint16_t pieceCount;
		std::uint32_t pieceIndices;
	};

	static_assert(sizeof(G_GlassName) == 0xC);
	static_assert(offsetof(G_GlassName, nameStr) == 0x0);
	static_assert(offsetof(G_GlassName, name) == 0x4);
	static_assert(offsetof(G_GlassName, pieceCount) == 0x6);
	static_assert(offsetof(G_GlassName, pieceIndices) == 0x8);

	inline G_GlassName Convert(const Game::G_GlassName& from)
	{
		G_GlassName to{};
		to.name = static_cast<std::uint16_t>(from.name);
		to.pieceCount = static_cast<std::uint16_t>(from.pieceCount);
		return to;
	}

	inline Game::G_GlassName Convert(const G_GlassName& from)
	{
		Game::G_GlassName to{};
		to.name = static_cast<decltype(to.name)>(from.name);
		to.pieceCount = static_cast<decltype(to.pieceCount)>(from.pieceCount);
		return to;
	}

	struct GameWorldMp
	{
		std::uint32_t name;
		std::uint32_t g_glassData;
	};

	static_assert(sizeof(GameWorldMp) == 0x8);
	static_assert(offsetof(GameWorldMp, name) == 0x0);
	static_assert(offsetof(GameWorldMp, g_glassData) == 0x4);

	inline GameWorldMp Convert(const Game::GameWorldMp&)
	{
		GameWorldMp to{};
		return to;
	}

	inline Game::GameWorldMp Convert(const GameWorldMp&)
	{
		Game::GameWorldMp to{};
		return to;
	}

	struct FxGlassSystem
	{
		std::int32_t time;
		std::int32_t prevTime;
		std::uint32_t defCount;
		std::uint32_t pieceLimit;
		std::uint32_t pieceWordCount;
		std::uint32_t initPieceCount;
		std::uint32_t cellCount;
		std::uint32_t activePieceCount;
		std::uint32_t firstFreePiece;
		std::uint32_t geoDataLimit;
		std::uint32_t geoDataCount;
		std::uint32_t initGeoDataCount;
		std::uint32_t defs;
		std::uint32_t piecePlaces;
		std::uint32_t pieceStates;
		std::uint32_t pieceDynamics;
		std::uint32_t geoData;
		std::uint32_t isInUse;
		std::uint32_t cellBits;
		std::uint32_t visData;
		std::uint32_t linkOrg;
		std::uint32_t halfThickness;
		std::uint32_t lightingHandles;
		std::uint32_t initPieceStates;
		std::uint32_t initGeoData;
		bool needToCompactData;
		std::int8_t initCount;
		float effectChanceAccum;
		std::int32_t lastPieceDeletionTime;
	};

	static_assert(sizeof(FxGlassSystem) == 0x70);
	static_assert(offsetof(FxGlassSystem, time) == 0x0);
	static_assert(offsetof(FxGlassSystem, prevTime) == 0x4);
	static_assert(offsetof(FxGlassSystem, defCount) == 0x8);
	static_assert(offsetof(FxGlassSystem, pieceLimit) == 0xC);
	static_assert(offsetof(FxGlassSystem, pieceWordCount) == 0x10);
	static_assert(offsetof(FxGlassSystem, initPieceCount) == 0x14);
	static_assert(offsetof(FxGlassSystem, cellCount) == 0x18);
	static_assert(offsetof(FxGlassSystem, activePieceCount) == 0x1C);
	static_assert(offsetof(FxGlassSystem, firstFreePiece) == 0x20);
	static_assert(offsetof(FxGlassSystem, geoDataLimit) == 0x24);
	static_assert(offsetof(FxGlassSystem, geoDataCount) == 0x28);
	static_assert(offsetof(FxGlassSystem, initGeoDataCount) == 0x2C);
	static_assert(offsetof(FxGlassSystem, defs) == 0x30);
	static_assert(offsetof(FxGlassSystem, piecePlaces) == 0x34);
	static_assert(offsetof(FxGlassSystem, pieceStates) == 0x38);
	static_assert(offsetof(FxGlassSystem, pieceDynamics) == 0x3C);
	static_assert(offsetof(FxGlassSystem, geoData) == 0x40);
	static_assert(offsetof(FxGlassSystem, isInUse) == 0x44);
	static_assert(offsetof(FxGlassSystem, cellBits) == 0x48);
	static_assert(offsetof(FxGlassSystem, visData) == 0x4C);
	static_assert(offsetof(FxGlassSystem, linkOrg) == 0x50);
	static_assert(offsetof(FxGlassSystem, halfThickness) == 0x54);
	static_assert(offsetof(FxGlassSystem, lightingHandles) == 0x58);
	static_assert(offsetof(FxGlassSystem, initPieceStates) == 0x5C);
	static_assert(offsetof(FxGlassSystem, initGeoData) == 0x60);
	static_assert(offsetof(FxGlassSystem, needToCompactData) == 0x64);
	static_assert(offsetof(FxGlassSystem, initCount) == 0x65);
	static_assert(offsetof(FxGlassSystem, effectChanceAccum) == 0x68);
	static_assert(offsetof(FxGlassSystem, lastPieceDeletionTime) == 0x6C);

	inline FxGlassSystem Convert(const Game::FxGlassSystem& from)
	{
		FxGlassSystem to{};
		to.time = static_cast<std::int32_t>(from.time);
		to.prevTime = static_cast<std::int32_t>(from.prevTime);
		to.defCount = static_cast<std::uint32_t>(from.defCount);
		to.pieceLimit = static_cast<std::uint32_t>(from.pieceLimit);
		to.pieceWordCount = static_cast<std::uint32_t>(from.pieceWordCount);
		to.initPieceCount = static_cast<std::uint32_t>(from.initPieceCount);
		to.cellCount = static_cast<std::uint32_t>(from.cellCount);
		to.activePieceCount = static_cast<std::uint32_t>(from.activePieceCount);
		to.firstFreePiece = static_cast<std::uint32_t>(from.firstFreePiece);
		to.geoDataLimit = static_cast<std::uint32_t>(from.geoDataLimit);
		to.geoDataCount = static_cast<std::uint32_t>(from.geoDataCount);
		to.initGeoDataCount = static_cast<std::uint32_t>(from.initGeoDataCount);
		to.needToCompactData = static_cast<bool>(from.needToCompactData);
		to.initCount = static_cast<std::int8_t>(from.initCount);
		to.effectChanceAccum = static_cast<float>(from.effectChanceAccum);
		to.lastPieceDeletionTime = static_cast<std::int32_t>(from.lastPieceDeletionTime);
		return to;
	}

	inline Game::FxGlassSystem Convert(const FxGlassSystem& from)
	{
		Game::FxGlassSystem to{};
		to.time = static_cast<decltype(to.time)>(from.time);
		to.prevTime = static_cast<decltype(to.prevTime)>(from.prevTime);
		to.defCount = static_cast<decltype(to.defCount)>(from.defCount);
		to.pieceLimit = static_cast<decltype(to.pieceLimit)>(from.pieceLimit);
		to.pieceWordCount = static_cast<decltype(to.pieceWordCount)>(from.pieceWordCount);
		to.initPieceCount = static_cast<decltype(to.initPieceCount)>(from.initPieceCount);
		to.cellCount = static_cast<decltype(to.cellCount)>(from.cellCount);
		to.activePieceCount = static_cast<decltype(to.activePieceCount)>(from.activePieceCount);
		to.firstFreePiece = static_cast<decltype(to.firstFreePiece)>(from.firstFreePiece);
		to.geoDataLimit = static_cast<decltype(to.geoDataLimit)>(from.geoDataLimit);
		to.geoDataCount = static_cast<decltype(to.geoDataCount)>(from.geoDataCount);
		to.initGeoDataCount = static_cast<decltype(to.initGeoDataCount)>(from.initGeoDataCount);
		to.needToCompactData = static_cast<decltype(to.needToCompactData)>(from.needToCompactData);
		to.initCount = static_cast<decltype(to.initCount)>(from.initCount);
		to.effectChanceAccum = static_cast<decltype(to.effectChanceAccum)>(from.effectChanceAccum);
		to.lastPieceDeletionTime = static_cast<decltype(to.lastPieceDeletionTime)>(from.lastPieceDeletionTime);
		return to;
	}

	struct FxWorld
	{
		std::uint32_t name;
		FxGlassSystem glassSys;
	};

	static_assert(sizeof(FxWorld) == 0x74);
	static_assert(offsetof(FxWorld, name) == 0x0);
	static_assert(offsetof(FxWorld, glassSys) == 0x4);

	inline FxWorld Convert(const Game::FxWorld& from)
	{
		FxWorld to{};
		to.glassSys = Convert(from.glassSys);
		return to;
	}

	inline Game::FxWorld Convert(const FxWorld& from)
	{
		Game::FxWorld to{};
		to.glassSys = Convert(from.glassSys);
		return to;
	}

	struct FxGlassDef
	{
		float halfThickness;
		float texVecs[2][2];
		GfxColor color;
		std::uint32_t material;
		std::uint32_t materialShattered;
		std::uint32_t physPreset;
	};

	static_assert(sizeof(FxGlassDef) == 0x24);
	static_assert(offsetof(FxGlassDef, halfThickness) == 0x0);
	static_assert(offsetof(FxGlassDef, texVecs) == 0x4);
	static_assert(offsetof(FxGlassDef, color) == 0x14);
	static_assert(offsetof(FxGlassDef, material) == 0x18);
	static_assert(offsetof(FxGlassDef, materialShattered) == 0x1C);
	static_assert(offsetof(FxGlassDef, physPreset) == 0x20);

	inline FxGlassDef Convert(const Game::FxGlassDef& from)
	{
		FxGlassDef to{};
		to.halfThickness = static_cast<float>(from.halfThickness);
		std::memcpy(to.texVecs, from.texVecs, sizeof(to.texVecs));
		to.color = Convert(from.color);
		return to;
	}

	inline Game::FxGlassDef Convert(const FxGlassDef& from)
	{
		Game::FxGlassDef to{};
		to.halfThickness = static_cast<decltype(to.halfThickness)>(from.halfThickness);
		std::memcpy(to.texVecs, from.texVecs, sizeof(from.texVecs));
		to.color = Convert(from.color);
		return to;
	}

	struct FxSpatialFrame
	{
		float quat[4];
		float origin[3];
	};

	static_assert(sizeof(FxSpatialFrame) == 0x1C);
	static_assert(offsetof(FxSpatialFrame, quat) == 0x0);
	static_assert(offsetof(FxSpatialFrame, origin) == 0x10);

	inline FxSpatialFrame Convert(const Game::FxSpatialFrame& from)
	{
		FxSpatialFrame to{};
		std::memcpy(to.quat, from.quat, sizeof(to.quat));
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		return to;
	}

	inline Game::FxSpatialFrame Convert(const FxSpatialFrame& from)
	{
		Game::FxSpatialFrame to{};
		std::memcpy(to.quat, from.quat, sizeof(from.quat));
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		return to;
	}

	struct $E43DBA5037697D705289B74D87E76C70
	{
		FxSpatialFrame frame;
		float radius;
	};

	static_assert(sizeof($E43DBA5037697D705289B74D87E76C70) == 0x20);
	static_assert(offsetof($E43DBA5037697D705289B74D87E76C70, frame) == 0x0);
	static_assert(offsetof($E43DBA5037697D705289B74D87E76C70, radius) == 0x1C);

	inline $E43DBA5037697D705289B74D87E76C70 Convert(const Game::$E43DBA5037697D705289B74D87E76C70& from)
	{
		$E43DBA5037697D705289B74D87E76C70 to{};
		to.frame = Convert(from.frame);
		to.radius = static_cast<float>(from.radius);
		return to;
	}

	inline Game::$E43DBA5037697D705289B74D87E76C70 Convert(const $E43DBA5037697D705289B74D87E76C70& from)
	{
		Game::$E43DBA5037697D705289B74D87E76C70 to{};
		to.frame = Convert(from.frame);
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		return to;
	}

	union FxGlassPiecePlace
	{
		$E43DBA5037697D705289B74D87E76C70 __s0;
		std::uint32_t nextFree;
	};

	static_assert(sizeof(FxGlassPiecePlace) == 0x20);
	static_assert(offsetof(FxGlassPiecePlace, __s0) == 0x0);
	static_assert(offsetof(FxGlassPiecePlace, nextFree) == 0x0);

	inline FxGlassPiecePlace Convert(const Game::FxGlassPiecePlace& from)
	{
		FxGlassPiecePlace to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::FxGlassPiecePlace Convert(const FxGlassPiecePlace& from)
	{
		Game::FxGlassPiecePlace to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct FxGlassPieceState
	{
		float texCoordOrigin[2];
		std::uint32_t supportMask;
		std::uint16_t initIndex;
		std::uint16_t geoDataStart;
		std::int8_t defIndex;
		std::int8_t pad[5];
		std::int8_t vertCount;
		std::int8_t holeDataCount;
		std::int8_t crackDataCount;
		std::int8_t fanDataCount;
		std::uint16_t flags;
		float areaX2;
	};

	static_assert(sizeof(FxGlassPieceState) == 0x20);
	static_assert(offsetof(FxGlassPieceState, texCoordOrigin) == 0x0);
	static_assert(offsetof(FxGlassPieceState, supportMask) == 0x8);
	static_assert(offsetof(FxGlassPieceState, initIndex) == 0xC);
	static_assert(offsetof(FxGlassPieceState, geoDataStart) == 0xE);
	static_assert(offsetof(FxGlassPieceState, defIndex) == 0x10);
	static_assert(offsetof(FxGlassPieceState, pad) == 0x11);
	static_assert(offsetof(FxGlassPieceState, vertCount) == 0x16);
	static_assert(offsetof(FxGlassPieceState, holeDataCount) == 0x17);
	static_assert(offsetof(FxGlassPieceState, crackDataCount) == 0x18);
	static_assert(offsetof(FxGlassPieceState, fanDataCount) == 0x19);
	static_assert(offsetof(FxGlassPieceState, flags) == 0x1A);
	static_assert(offsetof(FxGlassPieceState, areaX2) == 0x1C);

	inline FxGlassPieceState Convert(const Game::FxGlassPieceState& from)
	{
		FxGlassPieceState to{};
		std::memcpy(to.texCoordOrigin, from.texCoordOrigin, sizeof(to.texCoordOrigin));
		to.supportMask = static_cast<std::uint32_t>(from.supportMask);
		to.initIndex = static_cast<std::uint16_t>(from.initIndex);
		to.geoDataStart = static_cast<std::uint16_t>(from.geoDataStart);
		to.defIndex = static_cast<std::int8_t>(from.defIndex);
		std::memcpy(to.pad, from.pad, sizeof(to.pad));
		to.vertCount = static_cast<std::int8_t>(from.vertCount);
		to.holeDataCount = static_cast<std::int8_t>(from.holeDataCount);
		to.crackDataCount = static_cast<std::int8_t>(from.crackDataCount);
		to.fanDataCount = static_cast<std::int8_t>(from.fanDataCount);
		to.flags = static_cast<std::uint16_t>(from.flags);
		to.areaX2 = static_cast<float>(from.areaX2);
		return to;
	}

	inline Game::FxGlassPieceState Convert(const FxGlassPieceState& from)
	{
		Game::FxGlassPieceState to{};
		std::memcpy(to.texCoordOrigin, from.texCoordOrigin, sizeof(from.texCoordOrigin));
		to.supportMask = static_cast<decltype(to.supportMask)>(from.supportMask);
		to.initIndex = static_cast<decltype(to.initIndex)>(from.initIndex);
		to.geoDataStart = static_cast<decltype(to.geoDataStart)>(from.geoDataStart);
		to.defIndex = static_cast<decltype(to.defIndex)>(from.defIndex);
		std::memcpy(to.pad, from.pad, sizeof(from.pad));
		to.vertCount = static_cast<decltype(to.vertCount)>(from.vertCount);
		to.holeDataCount = static_cast<decltype(to.holeDataCount)>(from.holeDataCount);
		to.crackDataCount = static_cast<decltype(to.crackDataCount)>(from.crackDataCount);
		to.fanDataCount = static_cast<decltype(to.fanDataCount)>(from.fanDataCount);
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.areaX2 = static_cast<decltype(to.areaX2)>(from.areaX2);
		return to;
	}

	struct FxGlassPieceDynamics
	{
		std::int32_t fallTime;
		std::int32_t physObjId;
		std::int32_t physJointId;
		float vel[3];
		float avel[3];
	};

	static_assert(sizeof(FxGlassPieceDynamics) == 0x24);
	static_assert(offsetof(FxGlassPieceDynamics, fallTime) == 0x0);
	static_assert(offsetof(FxGlassPieceDynamics, physObjId) == 0x4);
	static_assert(offsetof(FxGlassPieceDynamics, physJointId) == 0x8);
	static_assert(offsetof(FxGlassPieceDynamics, vel) == 0xC);
	static_assert(offsetof(FxGlassPieceDynamics, avel) == 0x18);

	inline FxGlassPieceDynamics Convert(const Game::FxGlassPieceDynamics& from)
	{
		FxGlassPieceDynamics to{};
		to.fallTime = static_cast<std::int32_t>(from.fallTime);
		to.physObjId = static_cast<std::int32_t>(from.physObjId);
		to.physJointId = static_cast<std::int32_t>(from.physJointId);
		std::memcpy(to.vel, from.vel, sizeof(to.vel));
		std::memcpy(to.avel, from.avel, sizeof(to.avel));
		return to;
	}

	inline Game::FxGlassPieceDynamics Convert(const FxGlassPieceDynamics& from)
	{
		Game::FxGlassPieceDynamics to{};
		to.fallTime = static_cast<decltype(to.fallTime)>(from.fallTime);
		to.physObjId = static_cast<decltype(to.physObjId)>(from.physObjId);
		to.physJointId = static_cast<decltype(to.physJointId)>(from.physJointId);
		std::memcpy(to.vel, from.vel, sizeof(from.vel));
		std::memcpy(to.avel, from.avel, sizeof(from.avel));
		return to;
	}

	struct FxGlassVertex
	{
		std::int16_t x;
		std::int16_t y;
	};

	static_assert(sizeof(FxGlassVertex) == 0x4);
	static_assert(offsetof(FxGlassVertex, x) == 0x0);
	static_assert(offsetof(FxGlassVertex, y) == 0x2);

	inline FxGlassVertex Convert(const Game::FxGlassVertex& from)
	{
		FxGlassVertex to{};
		to.x = static_cast<std::int16_t>(from.x);
		to.y = static_cast<std::int16_t>(from.y);
		return to;
	}

	inline Game::FxGlassVertex Convert(const FxGlassVertex& from)
	{
		Game::FxGlassVertex to{};
		to.x = static_cast<decltype(to.x)>(from.x);
		to.y = static_cast<decltype(to.y)>(from.y);
		return to;
	}

	struct FxGlassHoleHeader
	{
		std::uint16_t uniqueVertCount;
		std::int8_t touchVert;
		std::int8_t pad[1];
	};

	static_assert(sizeof(FxGlassHoleHeader) == 0x4);
	static_assert(offsetof(FxGlassHoleHeader, uniqueVertCount) == 0x0);
	static_assert(offsetof(FxGlassHoleHeader, touchVert) == 0x2);
	static_assert(offsetof(FxGlassHoleHeader, pad) == 0x3);

	inline FxGlassHoleHeader Convert(const Game::FxGlassHoleHeader& from)
	{
		FxGlassHoleHeader to{};
		to.uniqueVertCount = static_cast<std::uint16_t>(from.uniqueVertCount);
		to.touchVert = static_cast<std::int8_t>(from.touchVert);
		std::memcpy(to.pad, from.pad, sizeof(to.pad));
		return to;
	}

	inline Game::FxGlassHoleHeader Convert(const FxGlassHoleHeader& from)
	{
		Game::FxGlassHoleHeader to{};
		to.uniqueVertCount = static_cast<decltype(to.uniqueVertCount)>(from.uniqueVertCount);
		to.touchVert = static_cast<decltype(to.touchVert)>(from.touchVert);
		std::memcpy(to.pad, from.pad, sizeof(from.pad));
		return to;
	}

	struct FxGlassCrackHeader
	{
		std::uint16_t uniqueVertCount;
		std::int8_t beginVertIndex;
		std::int8_t endVertIndex;
	};

	static_assert(sizeof(FxGlassCrackHeader) == 0x4);
	static_assert(offsetof(FxGlassCrackHeader, uniqueVertCount) == 0x0);
	static_assert(offsetof(FxGlassCrackHeader, beginVertIndex) == 0x2);
	static_assert(offsetof(FxGlassCrackHeader, endVertIndex) == 0x3);

	inline FxGlassCrackHeader Convert(const Game::FxGlassCrackHeader& from)
	{
		FxGlassCrackHeader to{};
		to.uniqueVertCount = static_cast<std::uint16_t>(from.uniqueVertCount);
		to.beginVertIndex = static_cast<std::int8_t>(from.beginVertIndex);
		to.endVertIndex = static_cast<std::int8_t>(from.endVertIndex);
		return to;
	}

	inline Game::FxGlassCrackHeader Convert(const FxGlassCrackHeader& from)
	{
		Game::FxGlassCrackHeader to{};
		to.uniqueVertCount = static_cast<decltype(to.uniqueVertCount)>(from.uniqueVertCount);
		to.beginVertIndex = static_cast<decltype(to.beginVertIndex)>(from.beginVertIndex);
		to.endVertIndex = static_cast<decltype(to.endVertIndex)>(from.endVertIndex);
		return to;
	}

	union FxGlassGeometryData
	{
		FxGlassVertex vert;
		FxGlassHoleHeader hole;
		FxGlassCrackHeader crack;
		std::int8_t asBytes[4];
		std::int16_t anonymous[2];
	};

	static_assert(sizeof(FxGlassGeometryData) == 0x4);
	static_assert(offsetof(FxGlassGeometryData, vert) == 0x0);
	static_assert(offsetof(FxGlassGeometryData, hole) == 0x0);
	static_assert(offsetof(FxGlassGeometryData, crack) == 0x0);
	static_assert(offsetof(FxGlassGeometryData, asBytes) == 0x0);
	static_assert(offsetof(FxGlassGeometryData, anonymous) == 0x0);

	inline FxGlassGeometryData Convert(const Game::FxGlassGeometryData& from)
	{
		FxGlassGeometryData to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::FxGlassGeometryData Convert(const FxGlassGeometryData& from)
	{
		Game::FxGlassGeometryData to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct FxGlassInitPieceState
	{
		FxSpatialFrame frame;
		float radius;
		float texCoordOrigin[2];
		std::uint32_t supportMask;
		float areaX2;
		std::uint8_t defIndex;
		std::uint8_t vertCount;
		std::uint8_t fanDataCount;
		std::int8_t pad[1];
	};

	static_assert(sizeof(FxGlassInitPieceState) == 0x34);
	static_assert(offsetof(FxGlassInitPieceState, frame) == 0x0);
	static_assert(offsetof(FxGlassInitPieceState, radius) == 0x1C);
	static_assert(offsetof(FxGlassInitPieceState, texCoordOrigin) == 0x20);
	static_assert(offsetof(FxGlassInitPieceState, supportMask) == 0x28);
	static_assert(offsetof(FxGlassInitPieceState, areaX2) == 0x2C);
	static_assert(offsetof(FxGlassInitPieceState, defIndex) == 0x30);
	static_assert(offsetof(FxGlassInitPieceState, vertCount) == 0x31);
	static_assert(offsetof(FxGlassInitPieceState, fanDataCount) == 0x32);
	static_assert(offsetof(FxGlassInitPieceState, pad) == 0x33);

	inline FxGlassInitPieceState Convert(const Game::FxGlassInitPieceState& from)
	{
		FxGlassInitPieceState to{};
		to.frame = Convert(from.frame);
		to.radius = static_cast<float>(from.radius);
		std::memcpy(to.texCoordOrigin, from.texCoordOrigin, sizeof(to.texCoordOrigin));
		to.supportMask = static_cast<std::uint32_t>(from.supportMask);
		to.areaX2 = static_cast<float>(from.areaX2);
		to.defIndex = static_cast<std::uint8_t>(from.defIndex);
		to.vertCount = static_cast<std::uint8_t>(from.vertCount);
		to.fanDataCount = static_cast<std::uint8_t>(from.fanDataCount);
		std::memcpy(to.pad, from.pad, sizeof(to.pad));
		return to;
	}

	inline Game::FxGlassInitPieceState Convert(const FxGlassInitPieceState& from)
	{
		Game::FxGlassInitPieceState to{};
		to.frame = Convert(from.frame);
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		std::memcpy(to.texCoordOrigin, from.texCoordOrigin, sizeof(from.texCoordOrigin));
		to.supportMask = static_cast<decltype(to.supportMask)>(from.supportMask);
		to.areaX2 = static_cast<decltype(to.areaX2)>(from.areaX2);
		to.defIndex = static_cast<decltype(to.defIndex)>(from.defIndex);
		to.vertCount = static_cast<decltype(to.vertCount)>(from.vertCount);
		to.fanDataCount = static_cast<decltype(to.fanDataCount)>(from.fanDataCount);
		std::memcpy(to.pad, from.pad, sizeof(from.pad));
		return to;
	}

	struct GfxWorldDpvsPlanes
	{
		std::int32_t cellCount;
		std::uint32_t planes;
		std::uint32_t nodes;
		std::uint32_t sceneEntCellBits;
	};

	static_assert(sizeof(GfxWorldDpvsPlanes) == 0x10);
	static_assert(offsetof(GfxWorldDpvsPlanes, cellCount) == 0x0);
	static_assert(offsetof(GfxWorldDpvsPlanes, planes) == 0x4);
	static_assert(offsetof(GfxWorldDpvsPlanes, nodes) == 0x8);
	static_assert(offsetof(GfxWorldDpvsPlanes, sceneEntCellBits) == 0xC);

	inline GfxWorldDpvsPlanes Convert(const Game::GfxWorldDpvsPlanes& from)
	{
		GfxWorldDpvsPlanes to{};
		to.cellCount = static_cast<std::int32_t>(from.cellCount);
		return to;
	}

	inline Game::GfxWorldDpvsPlanes Convert(const GfxWorldDpvsPlanes& from)
	{
		Game::GfxWorldDpvsPlanes to{};
		to.cellCount = static_cast<decltype(to.cellCount)>(from.cellCount);
		return to;
	}

	struct GfxWorldVertexData
	{
		std::uint32_t vertices;
		std::uint32_t worldVb;
	};

	static_assert(sizeof(GfxWorldVertexData) == 0x8);
	static_assert(offsetof(GfxWorldVertexData, vertices) == 0x0);
	static_assert(offsetof(GfxWorldVertexData, worldVb) == 0x4);

	inline GfxWorldVertexData Convert(const Game::GfxWorldVertexData&)
	{
		GfxWorldVertexData to{};
		return to;
	}

	inline Game::GfxWorldVertexData Convert(const GfxWorldVertexData&)
	{
		Game::GfxWorldVertexData to{};
		return to;
	}

	struct GfxWorldVertexLayerData
	{
		std::uint32_t data;
		std::uint32_t layerVb;
	};

	static_assert(sizeof(GfxWorldVertexLayerData) == 0x8);
	static_assert(offsetof(GfxWorldVertexLayerData, data) == 0x0);
	static_assert(offsetof(GfxWorldVertexLayerData, layerVb) == 0x4);

	inline GfxWorldVertexLayerData Convert(const Game::GfxWorldVertexLayerData&)
	{
		GfxWorldVertexLayerData to{};
		return to;
	}

	inline Game::GfxWorldVertexLayerData Convert(const GfxWorldVertexLayerData&)
	{
		Game::GfxWorldVertexLayerData to{};
		return to;
	}

	struct GfxWorldDraw
	{
		std::uint32_t reflectionProbeCount;
		std::uint32_t reflectionProbes;
		std::uint32_t reflectionProbeOrigins;
		std::uint32_t reflectionProbeTextures;
		std::int32_t lightmapCount;
		std::uint32_t lightmaps;
		std::uint32_t lightmapPrimaryTextures;
		std::uint32_t lightmapSecondaryTextures;
		std::uint32_t lightmapOverridePrimary;
		std::uint32_t lightmapOverrideSecondary;
		std::uint32_t vertexCount;
		GfxWorldVertexData vd;
		std::uint32_t vertexLayerDataSize;
		GfxWorldVertexLayerData vld;
		std::uint32_t indexCount;
		std::uint32_t indices;
	};

	static_assert(sizeof(GfxWorldDraw) == 0x48);
	static_assert(offsetof(GfxWorldDraw, reflectionProbeCount) == 0x0);
	static_assert(offsetof(GfxWorldDraw, reflectionProbes) == 0x4);
	static_assert(offsetof(GfxWorldDraw, reflectionProbeOrigins) == 0x8);
	static_assert(offsetof(GfxWorldDraw, reflectionProbeTextures) == 0xC);
	static_assert(offsetof(GfxWorldDraw, lightmapCount) == 0x10);
	static_assert(offsetof(GfxWorldDraw, lightmaps) == 0x14);
	static_assert(offsetof(GfxWorldDraw, lightmapPrimaryTextures) == 0x18);
	static_assert(offsetof(GfxWorldDraw, lightmapSecondaryTextures) == 0x1C);
	static_assert(offsetof(GfxWorldDraw, lightmapOverridePrimary) == 0x20);
	static_assert(offsetof(GfxWorldDraw, lightmapOverrideSecondary) == 0x24);
	static_assert(offsetof(GfxWorldDraw, vertexCount) == 0x28);
	static_assert(offsetof(GfxWorldDraw, vd) == 0x2C);
	static_assert(offsetof(GfxWorldDraw, vertexLayerDataSize) == 0x34);
	static_assert(offsetof(GfxWorldDraw, vld) == 0x38);
	static_assert(offsetof(GfxWorldDraw, indexCount) == 0x40);
	static_assert(offsetof(GfxWorldDraw, indices) == 0x44);

	inline GfxWorldDraw Convert(const Game::GfxWorldDraw& from)
	{
		GfxWorldDraw to{};
		to.reflectionProbeCount = static_cast<std::uint32_t>(from.reflectionProbeCount);
		to.lightmapCount = static_cast<std::int32_t>(from.lightmapCount);
		to.vertexCount = static_cast<std::uint32_t>(from.vertexCount);
		to.vd = Convert(from.vd);
		to.vertexLayerDataSize = static_cast<std::uint32_t>(from.vertexLayerDataSize);
		to.vld = Convert(from.vld);
		to.indexCount = static_cast<std::uint32_t>(from.indexCount);
		return to;
	}

	inline Game::GfxWorldDraw Convert(const GfxWorldDraw& from)
	{
		Game::GfxWorldDraw to{};
		to.reflectionProbeCount = static_cast<decltype(to.reflectionProbeCount)>(from.reflectionProbeCount);
		to.lightmapCount = static_cast<decltype(to.lightmapCount)>(from.lightmapCount);
		to.vertexCount = static_cast<decltype(to.vertexCount)>(from.vertexCount);
		to.vd = Convert(from.vd);
		to.vertexLayerDataSize = static_cast<decltype(to.vertexLayerDataSize)>(from.vertexLayerDataSize);
		to.vld = Convert(from.vld);
		to.indexCount = static_cast<decltype(to.indexCount)>(from.indexCount);
		return to;
	}

	struct GfxLightGrid
	{
		bool hasLightRegions;
		std::uint32_t lastSunPrimaryLightIndex;
		std::uint16_t mins[3];
		std::uint16_t maxs[3];
		std::uint32_t rowAxis;
		std::uint32_t colAxis;
		std::uint32_t rowDataStart;
		std::uint32_t rawRowDataSize;
		std::uint32_t rawRowData;
		std::uint32_t entryCount;
		std::uint32_t entries;
		std::uint32_t colorCount;
		std::uint32_t colors;
	};

	static_assert(sizeof(GfxLightGrid) == 0x38);
	static_assert(offsetof(GfxLightGrid, hasLightRegions) == 0x0);
	static_assert(offsetof(GfxLightGrid, lastSunPrimaryLightIndex) == 0x4);
	static_assert(offsetof(GfxLightGrid, mins) == 0x8);
	static_assert(offsetof(GfxLightGrid, maxs) == 0xE);
	static_assert(offsetof(GfxLightGrid, rowAxis) == 0x14);
	static_assert(offsetof(GfxLightGrid, colAxis) == 0x18);
	static_assert(offsetof(GfxLightGrid, rowDataStart) == 0x1C);
	static_assert(offsetof(GfxLightGrid, rawRowDataSize) == 0x20);
	static_assert(offsetof(GfxLightGrid, rawRowData) == 0x24);
	static_assert(offsetof(GfxLightGrid, entryCount) == 0x28);
	static_assert(offsetof(GfxLightGrid, entries) == 0x2C);
	static_assert(offsetof(GfxLightGrid, colorCount) == 0x30);
	static_assert(offsetof(GfxLightGrid, colors) == 0x34);

	inline GfxLightGrid Convert(const Game::GfxLightGrid& from)
	{
		GfxLightGrid to{};
		to.hasLightRegions = static_cast<bool>(from.hasLightRegions);
		to.lastSunPrimaryLightIndex = static_cast<std::uint32_t>(from.lastSunPrimaryLightIndex);
		std::memcpy(to.mins, from.mins, sizeof(to.mins));
		std::memcpy(to.maxs, from.maxs, sizeof(to.maxs));
		to.rowAxis = static_cast<std::uint32_t>(from.rowAxis);
		to.colAxis = static_cast<std::uint32_t>(from.colAxis);
		to.rawRowDataSize = static_cast<std::uint32_t>(from.rawRowDataSize);
		to.entryCount = static_cast<std::uint32_t>(from.entryCount);
		to.colorCount = static_cast<std::uint32_t>(from.colorCount);
		return to;
	}

	inline Game::GfxLightGrid Convert(const GfxLightGrid& from)
	{
		Game::GfxLightGrid to{};
		to.hasLightRegions = static_cast<decltype(to.hasLightRegions)>(from.hasLightRegions);
		to.lastSunPrimaryLightIndex = static_cast<decltype(to.lastSunPrimaryLightIndex)>(from.lastSunPrimaryLightIndex);
		std::memcpy(to.mins, from.mins, sizeof(from.mins));
		std::memcpy(to.maxs, from.maxs, sizeof(from.maxs));
		to.rowAxis = static_cast<decltype(to.rowAxis)>(from.rowAxis);
		to.colAxis = static_cast<decltype(to.colAxis)>(from.colAxis);
		to.rawRowDataSize = static_cast<decltype(to.rawRowDataSize)>(from.rawRowDataSize);
		to.entryCount = static_cast<decltype(to.entryCount)>(from.entryCount);
		to.colorCount = static_cast<decltype(to.colorCount)>(from.colorCount);
		return to;
	}

	struct sunflare_t
	{
		bool hasValidData;
		std::uint32_t spriteMaterial;
		std::uint32_t flareMaterial;
		float spriteSize;
		float flareMinSize;
		float flareMinDot;
		float flareMaxSize;
		float flareMaxDot;
		float flareMaxAlpha;
		std::int32_t flareFadeInTime;
		std::int32_t flareFadeOutTime;
		float blindMinDot;
		float blindMaxDot;
		float blindMaxDarken;
		std::int32_t blindFadeInTime;
		std::int32_t blindFadeOutTime;
		float glareMinDot;
		float glareMaxDot;
		float glareMaxLighten;
		std::int32_t glareFadeInTime;
		std::int32_t glareFadeOutTime;
		float sunFxPosition[3];
	};

	static_assert(sizeof(sunflare_t) == 0x60);
	static_assert(offsetof(sunflare_t, hasValidData) == 0x0);
	static_assert(offsetof(sunflare_t, spriteMaterial) == 0x4);
	static_assert(offsetof(sunflare_t, flareMaterial) == 0x8);
	static_assert(offsetof(sunflare_t, spriteSize) == 0xC);
	static_assert(offsetof(sunflare_t, flareMinSize) == 0x10);
	static_assert(offsetof(sunflare_t, flareMinDot) == 0x14);
	static_assert(offsetof(sunflare_t, flareMaxSize) == 0x18);
	static_assert(offsetof(sunflare_t, flareMaxDot) == 0x1C);
	static_assert(offsetof(sunflare_t, flareMaxAlpha) == 0x20);
	static_assert(offsetof(sunflare_t, flareFadeInTime) == 0x24);
	static_assert(offsetof(sunflare_t, flareFadeOutTime) == 0x28);
	static_assert(offsetof(sunflare_t, blindMinDot) == 0x2C);
	static_assert(offsetof(sunflare_t, blindMaxDot) == 0x30);
	static_assert(offsetof(sunflare_t, blindMaxDarken) == 0x34);
	static_assert(offsetof(sunflare_t, blindFadeInTime) == 0x38);
	static_assert(offsetof(sunflare_t, blindFadeOutTime) == 0x3C);
	static_assert(offsetof(sunflare_t, glareMinDot) == 0x40);
	static_assert(offsetof(sunflare_t, glareMaxDot) == 0x44);
	static_assert(offsetof(sunflare_t, glareMaxLighten) == 0x48);
	static_assert(offsetof(sunflare_t, glareFadeInTime) == 0x4C);
	static_assert(offsetof(sunflare_t, glareFadeOutTime) == 0x50);
	static_assert(offsetof(sunflare_t, sunFxPosition) == 0x54);

	inline sunflare_t Convert(const Game::sunflare_t& from)
	{
		sunflare_t to{};
		to.hasValidData = static_cast<bool>(from.hasValidData);
		to.spriteSize = static_cast<float>(from.spriteSize);
		to.flareMinSize = static_cast<float>(from.flareMinSize);
		to.flareMinDot = static_cast<float>(from.flareMinDot);
		to.flareMaxSize = static_cast<float>(from.flareMaxSize);
		to.flareMaxDot = static_cast<float>(from.flareMaxDot);
		to.flareMaxAlpha = static_cast<float>(from.flareMaxAlpha);
		to.flareFadeInTime = static_cast<std::int32_t>(from.flareFadeInTime);
		to.flareFadeOutTime = static_cast<std::int32_t>(from.flareFadeOutTime);
		to.blindMinDot = static_cast<float>(from.blindMinDot);
		to.blindMaxDot = static_cast<float>(from.blindMaxDot);
		to.blindMaxDarken = static_cast<float>(from.blindMaxDarken);
		to.blindFadeInTime = static_cast<std::int32_t>(from.blindFadeInTime);
		to.blindFadeOutTime = static_cast<std::int32_t>(from.blindFadeOutTime);
		to.glareMinDot = static_cast<float>(from.glareMinDot);
		to.glareMaxDot = static_cast<float>(from.glareMaxDot);
		to.glareMaxLighten = static_cast<float>(from.glareMaxLighten);
		to.glareFadeInTime = static_cast<std::int32_t>(from.glareFadeInTime);
		to.glareFadeOutTime = static_cast<std::int32_t>(from.glareFadeOutTime);
		std::memcpy(to.sunFxPosition, from.sunFxPosition, sizeof(to.sunFxPosition));
		return to;
	}

	inline Game::sunflare_t Convert(const sunflare_t& from)
	{
		Game::sunflare_t to{};
		to.hasValidData = static_cast<decltype(to.hasValidData)>(from.hasValidData);
		to.spriteSize = static_cast<decltype(to.spriteSize)>(from.spriteSize);
		to.flareMinSize = static_cast<decltype(to.flareMinSize)>(from.flareMinSize);
		to.flareMinDot = static_cast<decltype(to.flareMinDot)>(from.flareMinDot);
		to.flareMaxSize = static_cast<decltype(to.flareMaxSize)>(from.flareMaxSize);
		to.flareMaxDot = static_cast<decltype(to.flareMaxDot)>(from.flareMaxDot);
		to.flareMaxAlpha = static_cast<decltype(to.flareMaxAlpha)>(from.flareMaxAlpha);
		to.flareFadeInTime = static_cast<decltype(to.flareFadeInTime)>(from.flareFadeInTime);
		to.flareFadeOutTime = static_cast<decltype(to.flareFadeOutTime)>(from.flareFadeOutTime);
		to.blindMinDot = static_cast<decltype(to.blindMinDot)>(from.blindMinDot);
		to.blindMaxDot = static_cast<decltype(to.blindMaxDot)>(from.blindMaxDot);
		to.blindMaxDarken = static_cast<decltype(to.blindMaxDarken)>(from.blindMaxDarken);
		to.blindFadeInTime = static_cast<decltype(to.blindFadeInTime)>(from.blindFadeInTime);
		to.blindFadeOutTime = static_cast<decltype(to.blindFadeOutTime)>(from.blindFadeOutTime);
		to.glareMinDot = static_cast<decltype(to.glareMinDot)>(from.glareMinDot);
		to.glareMaxDot = static_cast<decltype(to.glareMaxDot)>(from.glareMaxDot);
		to.glareMaxLighten = static_cast<decltype(to.glareMaxLighten)>(from.glareMaxLighten);
		to.glareFadeInTime = static_cast<decltype(to.glareFadeInTime)>(from.glareFadeInTime);
		to.glareFadeOutTime = static_cast<decltype(to.glareFadeOutTime)>(from.glareFadeOutTime);
		std::memcpy(to.sunFxPosition, from.sunFxPosition, sizeof(from.sunFxPosition));
		return to;
	}

	struct GfxWorldDpvsStatic
	{
		std::uint32_t smodelCount;
		std::uint32_t staticSurfaceCount;
		std::uint32_t staticSurfaceCountNoDecal;
		std::uint32_t litOpaqueSurfsBegin;
		std::uint32_t litOpaqueSurfsEnd;
		std::uint32_t litTransSurfsBegin;
		std::uint32_t litTransSurfsEnd;
		std::uint32_t shadowCasterSurfsBegin;
		std::uint32_t shadowCasterSurfsEnd;
		std::uint32_t emissiveSurfsBegin;
		std::uint32_t emissiveSurfsEnd;
		std::uint32_t smodelVisDataCount;
		std::uint32_t surfaceVisDataCount;
		std::uint32_t smodelVisData[3];
		std::uint32_t surfaceVisData[3];
		std::uint32_t sortedSurfIndex;
		std::uint32_t smodelInsts;
		std::uint32_t surfaces;
		std::uint32_t surfacesBounds;
		std::uint32_t smodelDrawInsts;
		std::uint32_t surfaceMaterials;
		std::uint32_t surfaceCastsSunShadow;
		std::int32_t usageCount;
	};

	static_assert(sizeof(GfxWorldDpvsStatic) == 0x6C);
	static_assert(offsetof(GfxWorldDpvsStatic, smodelCount) == 0x0);
	static_assert(offsetof(GfxWorldDpvsStatic, staticSurfaceCount) == 0x4);
	static_assert(offsetof(GfxWorldDpvsStatic, staticSurfaceCountNoDecal) == 0x8);
	static_assert(offsetof(GfxWorldDpvsStatic, litOpaqueSurfsBegin) == 0xC);
	static_assert(offsetof(GfxWorldDpvsStatic, litOpaqueSurfsEnd) == 0x10);
	static_assert(offsetof(GfxWorldDpvsStatic, litTransSurfsBegin) == 0x14);
	static_assert(offsetof(GfxWorldDpvsStatic, litTransSurfsEnd) == 0x18);
	static_assert(offsetof(GfxWorldDpvsStatic, shadowCasterSurfsBegin) == 0x1C);
	static_assert(offsetof(GfxWorldDpvsStatic, shadowCasterSurfsEnd) == 0x20);
	static_assert(offsetof(GfxWorldDpvsStatic, emissiveSurfsBegin) == 0x24);
	static_assert(offsetof(GfxWorldDpvsStatic, emissiveSurfsEnd) == 0x28);
	static_assert(offsetof(GfxWorldDpvsStatic, smodelVisDataCount) == 0x2C);
	static_assert(offsetof(GfxWorldDpvsStatic, surfaceVisDataCount) == 0x30);
	static_assert(offsetof(GfxWorldDpvsStatic, smodelVisData) == 0x34);
	static_assert(offsetof(GfxWorldDpvsStatic, surfaceVisData) == 0x40);
	static_assert(offsetof(GfxWorldDpvsStatic, sortedSurfIndex) == 0x4C);
	static_assert(offsetof(GfxWorldDpvsStatic, smodelInsts) == 0x50);
	static_assert(offsetof(GfxWorldDpvsStatic, surfaces) == 0x54);
	static_assert(offsetof(GfxWorldDpvsStatic, surfacesBounds) == 0x58);
	static_assert(offsetof(GfxWorldDpvsStatic, smodelDrawInsts) == 0x5C);
	static_assert(offsetof(GfxWorldDpvsStatic, surfaceMaterials) == 0x60);
	static_assert(offsetof(GfxWorldDpvsStatic, surfaceCastsSunShadow) == 0x64);
	static_assert(offsetof(GfxWorldDpvsStatic, usageCount) == 0x68);

	inline GfxWorldDpvsStatic Convert(const Game::GfxWorldDpvsStatic& from)
	{
		GfxWorldDpvsStatic to{};
		to.smodelCount = static_cast<std::uint32_t>(from.smodelCount);
		to.staticSurfaceCount = static_cast<std::uint32_t>(from.staticSurfaceCount);
		to.staticSurfaceCountNoDecal = static_cast<std::uint32_t>(from.staticSurfaceCountNoDecal);
		to.litOpaqueSurfsBegin = static_cast<std::uint32_t>(from.litOpaqueSurfsBegin);
		to.litOpaqueSurfsEnd = static_cast<std::uint32_t>(from.litOpaqueSurfsEnd);
		to.litTransSurfsBegin = static_cast<std::uint32_t>(from.litTransSurfsBegin);
		to.litTransSurfsEnd = static_cast<std::uint32_t>(from.litTransSurfsEnd);
		to.shadowCasterSurfsBegin = static_cast<std::uint32_t>(from.shadowCasterSurfsBegin);
		to.shadowCasterSurfsEnd = static_cast<std::uint32_t>(from.shadowCasterSurfsEnd);
		to.emissiveSurfsBegin = static_cast<std::uint32_t>(from.emissiveSurfsBegin);
		to.emissiveSurfsEnd = static_cast<std::uint32_t>(from.emissiveSurfsEnd);
		to.smodelVisDataCount = static_cast<std::uint32_t>(from.smodelVisDataCount);
		to.surfaceVisDataCount = static_cast<std::uint32_t>(from.surfaceVisDataCount);
		to.usageCount = static_cast<std::int32_t>(from.usageCount);
		return to;
	}

	inline Game::GfxWorldDpvsStatic Convert(const GfxWorldDpvsStatic& from)
	{
		Game::GfxWorldDpvsStatic to{};
		to.smodelCount = static_cast<decltype(to.smodelCount)>(from.smodelCount);
		to.staticSurfaceCount = static_cast<decltype(to.staticSurfaceCount)>(from.staticSurfaceCount);
		to.staticSurfaceCountNoDecal = static_cast<decltype(to.staticSurfaceCountNoDecal)>(from.staticSurfaceCountNoDecal);
		to.litOpaqueSurfsBegin = static_cast<decltype(to.litOpaqueSurfsBegin)>(from.litOpaqueSurfsBegin);
		to.litOpaqueSurfsEnd = static_cast<decltype(to.litOpaqueSurfsEnd)>(from.litOpaqueSurfsEnd);
		to.litTransSurfsBegin = static_cast<decltype(to.litTransSurfsBegin)>(from.litTransSurfsBegin);
		to.litTransSurfsEnd = static_cast<decltype(to.litTransSurfsEnd)>(from.litTransSurfsEnd);
		to.shadowCasterSurfsBegin = static_cast<decltype(to.shadowCasterSurfsBegin)>(from.shadowCasterSurfsBegin);
		to.shadowCasterSurfsEnd = static_cast<decltype(to.shadowCasterSurfsEnd)>(from.shadowCasterSurfsEnd);
		to.emissiveSurfsBegin = static_cast<decltype(to.emissiveSurfsBegin)>(from.emissiveSurfsBegin);
		to.emissiveSurfsEnd = static_cast<decltype(to.emissiveSurfsEnd)>(from.emissiveSurfsEnd);
		to.smodelVisDataCount = static_cast<decltype(to.smodelVisDataCount)>(from.smodelVisDataCount);
		to.surfaceVisDataCount = static_cast<decltype(to.surfaceVisDataCount)>(from.surfaceVisDataCount);
		to.usageCount = static_cast<decltype(to.usageCount)>(from.usageCount);
		return to;
	}

	struct GfxWorldDpvsDynamic
	{
		std::uint32_t dynEntClientWordCount[2];
		std::uint32_t dynEntClientCount[2];
		std::uint32_t dynEntCellBits[2];
		std::uint32_t dynEntVisData[2][3];
	};

	static_assert(sizeof(GfxWorldDpvsDynamic) == 0x30);
	static_assert(offsetof(GfxWorldDpvsDynamic, dynEntClientWordCount) == 0x0);
	static_assert(offsetof(GfxWorldDpvsDynamic, dynEntClientCount) == 0x8);
	static_assert(offsetof(GfxWorldDpvsDynamic, dynEntCellBits) == 0x10);
	static_assert(offsetof(GfxWorldDpvsDynamic, dynEntVisData) == 0x18);

	inline GfxWorldDpvsDynamic Convert(const Game::GfxWorldDpvsDynamic& from)
	{
		GfxWorldDpvsDynamic to{};
		std::memcpy(to.dynEntClientWordCount, from.dynEntClientWordCount, sizeof(to.dynEntClientWordCount));
		std::memcpy(to.dynEntClientCount, from.dynEntClientCount, sizeof(to.dynEntClientCount));
		return to;
	}

	inline Game::GfxWorldDpvsDynamic Convert(const GfxWorldDpvsDynamic& from)
	{
		Game::GfxWorldDpvsDynamic to{};
		std::memcpy(to.dynEntClientWordCount, from.dynEntClientWordCount, sizeof(from.dynEntClientWordCount));
		std::memcpy(to.dynEntClientCount, from.dynEntClientCount, sizeof(from.dynEntClientCount));
		return to;
	}

	struct GfxWorld
	{
		std::uint32_t name;
		std::uint32_t baseName;
		std::int32_t planeCount;
		std::int32_t nodeCount;
		std::uint32_t surfaceCount;
		std::int32_t skyCount;
		std::uint32_t skies;
		std::uint32_t lastSunPrimaryLightIndex;
		std::uint32_t primaryLightCount;
		std::uint32_t sortKeyLitDecal;
		std::uint32_t sortKeyEffectDecal;
		std::uint32_t sortKeyEffectAuto;
		std::uint32_t sortKeyDistortion;
		GfxWorldDpvsPlanes dpvsPlanes;
		std::uint32_t aabbTreeCounts;
		std::uint32_t aabbTrees;
		std::uint32_t cells;
		GfxWorldDraw draw;
		GfxLightGrid lightGrid;
		std::int32_t modelCount;
		std::uint32_t models;
		Bounds bounds;
		std::uint32_t checksum;
		std::int32_t materialMemoryCount;
		std::uint32_t materialMemory;
		sunflare_t sun;
		float outdoorLookupMatrix[4][4];
		std::uint32_t outdoorImage;
		std::uint32_t cellCasterBits;
		std::uint32_t cellHasSunLitSurfsBits;
		std::uint32_t sceneDynModel;
		std::uint32_t sceneDynBrush;
		std::uint32_t primaryLightEntityShadowVis;
		std::uint32_t primaryLightDynEntShadowVis[2];
		std::uint32_t nonSunPrimaryLightForModelDynEnt;
		std::uint32_t shadowGeom;
		std::uint32_t lightRegion;
		GfxWorldDpvsStatic dpvs;
		GfxWorldDpvsDynamic dpvsDyn;
		std::uint32_t mapVtxChecksum;
		std::uint32_t heroOnlyLightCount;
		std::uint32_t heroOnlyLights;
		std::int8_t fogTypesAllowed;
	};

	static_assert(sizeof(GfxWorld) == 0x274);
	static_assert(offsetof(GfxWorld, name) == 0x0);
	static_assert(offsetof(GfxWorld, baseName) == 0x4);
	static_assert(offsetof(GfxWorld, planeCount) == 0x8);
	static_assert(offsetof(GfxWorld, nodeCount) == 0xC);
	static_assert(offsetof(GfxWorld, surfaceCount) == 0x10);
	static_assert(offsetof(GfxWorld, skyCount) == 0x14);
	static_assert(offsetof(GfxWorld, skies) == 0x18);
	static_assert(offsetof(GfxWorld, lastSunPrimaryLightIndex) == 0x1C);
	static_assert(offsetof(GfxWorld, primaryLightCount) == 0x20);
	static_assert(offsetof(GfxWorld, sortKeyLitDecal) == 0x24);
	static_assert(offsetof(GfxWorld, sortKeyEffectDecal) == 0x28);
	static_assert(offsetof(GfxWorld, sortKeyEffectAuto) == 0x2C);
	static_assert(offsetof(GfxWorld, sortKeyDistortion) == 0x30);
	static_assert(offsetof(GfxWorld, dpvsPlanes) == 0x34);
	static_assert(offsetof(GfxWorld, aabbTreeCounts) == 0x44);
	static_assert(offsetof(GfxWorld, aabbTrees) == 0x48);
	static_assert(offsetof(GfxWorld, cells) == 0x4C);
	static_assert(offsetof(GfxWorld, draw) == 0x50);
	static_assert(offsetof(GfxWorld, lightGrid) == 0x98);
	static_assert(offsetof(GfxWorld, modelCount) == 0xD0);
	static_assert(offsetof(GfxWorld, models) == 0xD4);
	static_assert(offsetof(GfxWorld, bounds) == 0xD8);
	static_assert(offsetof(GfxWorld, checksum) == 0xF0);
	static_assert(offsetof(GfxWorld, materialMemoryCount) == 0xF4);
	static_assert(offsetof(GfxWorld, materialMemory) == 0xF8);
	static_assert(offsetof(GfxWorld, sun) == 0xFC);
	static_assert(offsetof(GfxWorld, outdoorLookupMatrix) == 0x15C);
	static_assert(offsetof(GfxWorld, outdoorImage) == 0x19C);
	static_assert(offsetof(GfxWorld, cellCasterBits) == 0x1A0);
	static_assert(offsetof(GfxWorld, cellHasSunLitSurfsBits) == 0x1A4);
	static_assert(offsetof(GfxWorld, sceneDynModel) == 0x1A8);
	static_assert(offsetof(GfxWorld, sceneDynBrush) == 0x1AC);
	static_assert(offsetof(GfxWorld, primaryLightEntityShadowVis) == 0x1B0);
	static_assert(offsetof(GfxWorld, primaryLightDynEntShadowVis) == 0x1B4);
	static_assert(offsetof(GfxWorld, nonSunPrimaryLightForModelDynEnt) == 0x1BC);
	static_assert(offsetof(GfxWorld, shadowGeom) == 0x1C0);
	static_assert(offsetof(GfxWorld, lightRegion) == 0x1C4);
	static_assert(offsetof(GfxWorld, dpvs) == 0x1C8);
	static_assert(offsetof(GfxWorld, dpvsDyn) == 0x234);
	static_assert(offsetof(GfxWorld, mapVtxChecksum) == 0x264);
	static_assert(offsetof(GfxWorld, heroOnlyLightCount) == 0x268);
	static_assert(offsetof(GfxWorld, heroOnlyLights) == 0x26C);
	static_assert(offsetof(GfxWorld, fogTypesAllowed) == 0x270);

	inline GfxWorld Convert(const Game::GfxWorld& from)
	{
		GfxWorld to{};
		to.planeCount = static_cast<std::int32_t>(from.planeCount);
		to.nodeCount = static_cast<std::int32_t>(from.nodeCount);
		to.surfaceCount = static_cast<std::uint32_t>(from.surfaceCount);
		to.skyCount = static_cast<std::int32_t>(from.skyCount);
		to.lastSunPrimaryLightIndex = static_cast<std::uint32_t>(from.lastSunPrimaryLightIndex);
		to.primaryLightCount = static_cast<std::uint32_t>(from.primaryLightCount);
		to.sortKeyLitDecal = static_cast<std::uint32_t>(from.sortKeyLitDecal);
		to.sortKeyEffectDecal = static_cast<std::uint32_t>(from.sortKeyEffectDecal);
		to.sortKeyEffectAuto = static_cast<std::uint32_t>(from.sortKeyEffectAuto);
		to.sortKeyDistortion = static_cast<std::uint32_t>(from.sortKeyDistortion);
		to.dpvsPlanes = Convert(from.dpvsPlanes);
		to.draw = Convert(from.draw);
		to.lightGrid = Convert(from.lightGrid);
		to.modelCount = static_cast<std::int32_t>(from.modelCount);
		to.bounds = Convert(from.bounds);
		to.checksum = static_cast<std::uint32_t>(from.checksum);
		to.materialMemoryCount = static_cast<std::int32_t>(from.materialMemoryCount);
		to.sun = Convert(from.sun);
		std::memcpy(to.outdoorLookupMatrix, from.outdoorLookupMatrix, sizeof(to.outdoorLookupMatrix));
		to.dpvs = Convert(from.dpvs);
		to.dpvsDyn = Convert(from.dpvsDyn);
		to.mapVtxChecksum = static_cast<std::uint32_t>(from.mapVtxChecksum);
		to.heroOnlyLightCount = static_cast<std::uint32_t>(from.heroOnlyLightCount);
		to.fogTypesAllowed = static_cast<std::int8_t>(from.fogTypesAllowed);
		return to;
	}

	inline Game::GfxWorld Convert(const GfxWorld& from)
	{
		Game::GfxWorld to{};
		to.planeCount = static_cast<decltype(to.planeCount)>(from.planeCount);
		to.nodeCount = static_cast<decltype(to.nodeCount)>(from.nodeCount);
		to.surfaceCount = static_cast<decltype(to.surfaceCount)>(from.surfaceCount);
		to.skyCount = static_cast<decltype(to.skyCount)>(from.skyCount);
		to.lastSunPrimaryLightIndex = static_cast<decltype(to.lastSunPrimaryLightIndex)>(from.lastSunPrimaryLightIndex);
		to.primaryLightCount = static_cast<decltype(to.primaryLightCount)>(from.primaryLightCount);
		to.sortKeyLitDecal = static_cast<decltype(to.sortKeyLitDecal)>(from.sortKeyLitDecal);
		to.sortKeyEffectDecal = static_cast<decltype(to.sortKeyEffectDecal)>(from.sortKeyEffectDecal);
		to.sortKeyEffectAuto = static_cast<decltype(to.sortKeyEffectAuto)>(from.sortKeyEffectAuto);
		to.sortKeyDistortion = static_cast<decltype(to.sortKeyDistortion)>(from.sortKeyDistortion);
		to.dpvsPlanes = Convert(from.dpvsPlanes);
		to.draw = Convert(from.draw);
		to.lightGrid = Convert(from.lightGrid);
		to.modelCount = static_cast<decltype(to.modelCount)>(from.modelCount);
		to.bounds = Convert(from.bounds);
		to.checksum = static_cast<decltype(to.checksum)>(from.checksum);
		to.materialMemoryCount = static_cast<decltype(to.materialMemoryCount)>(from.materialMemoryCount);
		to.sun = Convert(from.sun);
		std::memcpy(to.outdoorLookupMatrix, from.outdoorLookupMatrix, sizeof(from.outdoorLookupMatrix));
		to.dpvs = Convert(from.dpvs);
		to.dpvsDyn = Convert(from.dpvsDyn);
		to.mapVtxChecksum = static_cast<decltype(to.mapVtxChecksum)>(from.mapVtxChecksum);
		to.heroOnlyLightCount = static_cast<decltype(to.heroOnlyLightCount)>(from.heroOnlyLightCount);
		to.fogTypesAllowed = static_cast<decltype(to.fogTypesAllowed)>(from.fogTypesAllowed);
		return to;
	}

	struct GfxSky
	{
		std::int32_t skySurfCount;
		std::uint32_t skyStartSurfs;
		std::uint32_t skyImage;
		std::int8_t skySamplerState;
	};

	static_assert(sizeof(GfxSky) == 0x10);
	static_assert(offsetof(GfxSky, skySurfCount) == 0x0);
	static_assert(offsetof(GfxSky, skyStartSurfs) == 0x4);
	static_assert(offsetof(GfxSky, skyImage) == 0x8);
	static_assert(offsetof(GfxSky, skySamplerState) == 0xC);

	inline GfxSky Convert(const Game::GfxSky& from)
	{
		GfxSky to{};
		to.skySurfCount = static_cast<std::int32_t>(from.skySurfCount);
		to.skySamplerState = static_cast<std::int8_t>(from.skySamplerState);
		return to;
	}

	inline Game::GfxSky Convert(const GfxSky& from)
	{
		Game::GfxSky to{};
		to.skySurfCount = static_cast<decltype(to.skySurfCount)>(from.skySurfCount);
		to.skySamplerState = static_cast<decltype(to.skySamplerState)>(from.skySamplerState);
		return to;
	}

	struct GfxCellTreeCount
	{
		std::int32_t aabbTreeCount;
	};

	static_assert(sizeof(GfxCellTreeCount) == 0x4);
	static_assert(offsetof(GfxCellTreeCount, aabbTreeCount) == 0x0);

	inline GfxCellTreeCount Convert(const Game::GfxCellTreeCount& from)
	{
		GfxCellTreeCount to{};
		to.aabbTreeCount = static_cast<std::int32_t>(from.aabbTreeCount);
		return to;
	}

	inline Game::GfxCellTreeCount Convert(const GfxCellTreeCount& from)
	{
		Game::GfxCellTreeCount to{};
		to.aabbTreeCount = static_cast<decltype(to.aabbTreeCount)>(from.aabbTreeCount);
		return to;
	}

	struct GfxCellTree
	{
		std::uint32_t aabbTree;
	};

	static_assert(sizeof(GfxCellTree) == 0x4);
	static_assert(offsetof(GfxCellTree, aabbTree) == 0x0);

	inline GfxCellTree Convert(const Game::GfxCellTree&)
	{
		GfxCellTree to{};
		return to;
	}

	inline Game::GfxCellTree Convert(const GfxCellTree&)
	{
		Game::GfxCellTree to{};
		return to;
	}

	struct GfxAabbTree
	{
		Bounds bounds;
		std::uint16_t childCount;
		std::uint16_t surfaceCount;
		std::uint16_t startSurfIndex;
		std::uint16_t surfaceCountNoDecal;
		std::uint16_t startSurfIndexNoDecal;
		std::uint16_t smodelIndexCount;
		std::uint32_t smodelIndexes;
		std::int32_t childrenOffset;
	};

	static_assert(sizeof(GfxAabbTree) == 0x2C);
	static_assert(offsetof(GfxAabbTree, bounds) == 0x0);
	static_assert(offsetof(GfxAabbTree, childCount) == 0x18);
	static_assert(offsetof(GfxAabbTree, surfaceCount) == 0x1A);
	static_assert(offsetof(GfxAabbTree, startSurfIndex) == 0x1C);
	static_assert(offsetof(GfxAabbTree, surfaceCountNoDecal) == 0x1E);
	static_assert(offsetof(GfxAabbTree, startSurfIndexNoDecal) == 0x20);
	static_assert(offsetof(GfxAabbTree, smodelIndexCount) == 0x22);
	static_assert(offsetof(GfxAabbTree, smodelIndexes) == 0x24);
	static_assert(offsetof(GfxAabbTree, childrenOffset) == 0x28);

	inline GfxAabbTree Convert(const Game::GfxAabbTree& from)
	{
		GfxAabbTree to{};
		to.bounds = Convert(from.bounds);
		to.childCount = static_cast<std::uint16_t>(from.childCount);
		to.surfaceCount = static_cast<std::uint16_t>(from.surfaceCount);
		to.startSurfIndex = static_cast<std::uint16_t>(from.startSurfIndex);
		to.surfaceCountNoDecal = static_cast<std::uint16_t>(from.surfaceCountNoDecal);
		to.startSurfIndexNoDecal = static_cast<std::uint16_t>(from.startSurfIndexNoDecal);
		to.smodelIndexCount = static_cast<std::uint16_t>(from.smodelIndexCount);
		to.childrenOffset = static_cast<std::int32_t>(from.childrenOffset);
		return to;
	}

	inline Game::GfxAabbTree Convert(const GfxAabbTree& from)
	{
		Game::GfxAabbTree to{};
		to.bounds = Convert(from.bounds);
		to.childCount = static_cast<decltype(to.childCount)>(from.childCount);
		to.surfaceCount = static_cast<decltype(to.surfaceCount)>(from.surfaceCount);
		to.startSurfIndex = static_cast<decltype(to.startSurfIndex)>(from.startSurfIndex);
		to.surfaceCountNoDecal = static_cast<decltype(to.surfaceCountNoDecal)>(from.surfaceCountNoDecal);
		to.startSurfIndexNoDecal = static_cast<decltype(to.startSurfIndexNoDecal)>(from.startSurfIndexNoDecal);
		to.smodelIndexCount = static_cast<decltype(to.smodelIndexCount)>(from.smodelIndexCount);
		to.childrenOffset = static_cast<decltype(to.childrenOffset)>(from.childrenOffset);
		return to;
	}

	struct GfxCell
	{
		Bounds bounds;
		std::int32_t portalCount;
		std::uint32_t portals;
		std::int8_t reflectionProbeCount;
		std::uint32_t reflectionProbes;
	};

	static_assert(sizeof(GfxCell) == 0x28);
	static_assert(offsetof(GfxCell, bounds) == 0x0);
	static_assert(offsetof(GfxCell, portalCount) == 0x18);
	static_assert(offsetof(GfxCell, portals) == 0x1C);
	static_assert(offsetof(GfxCell, reflectionProbeCount) == 0x20);
	static_assert(offsetof(GfxCell, reflectionProbes) == 0x24);

	inline GfxCell Convert(const Game::GfxCell& from)
	{
		GfxCell to{};
		to.bounds = Convert(from.bounds);
		to.portalCount = static_cast<std::int32_t>(from.portalCount);
		to.reflectionProbeCount = static_cast<std::int8_t>(from.reflectionProbeCount);
		return to;
	}

	inline Game::GfxCell Convert(const GfxCell& from)
	{
		Game::GfxCell to{};
		to.bounds = Convert(from.bounds);
		to.portalCount = static_cast<decltype(to.portalCount)>(from.portalCount);
		to.reflectionProbeCount = static_cast<decltype(to.reflectionProbeCount)>(from.reflectionProbeCount);
		return to;
	}

	struct GfxPortalWritable
	{
		bool isQueued;
		bool isAncestor;
		std::int8_t recursionDepth;
		std::int8_t hullPointCount;
		std::uint32_t hullPoints;
		std::uint32_t queuedParent;
	};

	static_assert(sizeof(GfxPortalWritable) == 0xC);
	static_assert(offsetof(GfxPortalWritable, isQueued) == 0x0);
	static_assert(offsetof(GfxPortalWritable, isAncestor) == 0x1);
	static_assert(offsetof(GfxPortalWritable, recursionDepth) == 0x2);
	static_assert(offsetof(GfxPortalWritable, hullPointCount) == 0x3);
	static_assert(offsetof(GfxPortalWritable, hullPoints) == 0x4);
	static_assert(offsetof(GfxPortalWritable, queuedParent) == 0x8);

	inline GfxPortalWritable Convert(const Game::GfxPortalWritable& from)
	{
		GfxPortalWritable to{};
		to.isQueued = static_cast<bool>(from.isQueued);
		to.isAncestor = static_cast<bool>(from.isAncestor);
		to.recursionDepth = static_cast<std::int8_t>(from.recursionDepth);
		to.hullPointCount = static_cast<std::int8_t>(from.hullPointCount);
		return to;
	}

	inline Game::GfxPortalWritable Convert(const GfxPortalWritable& from)
	{
		Game::GfxPortalWritable to{};
		to.isQueued = static_cast<decltype(to.isQueued)>(from.isQueued);
		to.isAncestor = static_cast<decltype(to.isAncestor)>(from.isAncestor);
		to.recursionDepth = static_cast<decltype(to.recursionDepth)>(from.recursionDepth);
		to.hullPointCount = static_cast<decltype(to.hullPointCount)>(from.hullPointCount);
		return to;
	}

	struct DpvsPlane
	{
		float coeffs[4];
	};

	static_assert(sizeof(DpvsPlane) == 0x10);
	static_assert(offsetof(DpvsPlane, coeffs) == 0x0);

	inline DpvsPlane Convert(const Game::DpvsPlane& from)
	{
		DpvsPlane to{};
		std::memcpy(to.coeffs, from.coeffs, sizeof(to.coeffs));
		return to;
	}

	inline Game::DpvsPlane Convert(const DpvsPlane& from)
	{
		Game::DpvsPlane to{};
		std::memcpy(to.coeffs, from.coeffs, sizeof(from.coeffs));
		return to;
	}

	struct GfxPortal
	{
		GfxPortalWritable writable;
		DpvsPlane plane;
		std::uint32_t vertices;
		std::uint16_t cellIndex;
		std::int8_t vertexCount;
		float hullAxis[2][3];
	};

	static_assert(sizeof(GfxPortal) == 0x3C);
	static_assert(offsetof(GfxPortal, writable) == 0x0);
	static_assert(offsetof(GfxPortal, plane) == 0xC);
	static_assert(offsetof(GfxPortal, vertices) == 0x1C);
	static_assert(offsetof(GfxPortal, cellIndex) == 0x20);
	static_assert(offsetof(GfxPortal, vertexCount) == 0x22);
	static_assert(offsetof(GfxPortal, hullAxis) == 0x24);

	inline GfxPortal Convert(const Game::GfxPortal& from)
	{
		GfxPortal to{};
		to.writable = Convert(from.writable);
		to.plane = Convert(from.plane);
		to.cellIndex = static_cast<std::uint16_t>(from.cellIndex);
		to.vertexCount = static_cast<std::int8_t>(from.vertexCount);
		std::memcpy(to.hullAxis, from.hullAxis, sizeof(to.hullAxis));
		return to;
	}

	inline Game::GfxPortal Convert(const GfxPortal& from)
	{
		Game::GfxPortal to{};
		to.writable = Convert(from.writable);
		to.plane = Convert(from.plane);
		to.cellIndex = static_cast<decltype(to.cellIndex)>(from.cellIndex);
		to.vertexCount = static_cast<decltype(to.vertexCount)>(from.vertexCount);
		std::memcpy(to.hullAxis, from.hullAxis, sizeof(from.hullAxis));
		return to;
	}

	struct GfxReflectionProbe
	{
		float origin[3];
	};

	static_assert(sizeof(GfxReflectionProbe) == 0xC);
	static_assert(offsetof(GfxReflectionProbe, origin) == 0x0);

	inline GfxReflectionProbe Convert(const Game::GfxReflectionProbe& from)
	{
		GfxReflectionProbe to{};
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		return to;
	}

	inline Game::GfxReflectionProbe Convert(const GfxReflectionProbe& from)
	{
		Game::GfxReflectionProbe to{};
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		return to;
	}

	struct GfxLightmapArray
	{
		std::uint32_t primary;
		std::uint32_t secondary;
	};

	static_assert(sizeof(GfxLightmapArray) == 0x8);
	static_assert(offsetof(GfxLightmapArray, primary) == 0x0);
	static_assert(offsetof(GfxLightmapArray, secondary) == 0x4);

	inline GfxLightmapArray Convert(const Game::GfxLightmapArray&)
	{
		GfxLightmapArray to{};
		return to;
	}

	inline Game::GfxLightmapArray Convert(const GfxLightmapArray&)
	{
		Game::GfxLightmapArray to{};
		return to;
	}

	struct GfxWorldVertex
	{
		float xyz[3];
		float binormalSign;
		GfxColor color;
		float texCoord[2];
		float lmapCoord[2];
		PackedUnitVec normal;
		PackedUnitVec tangent;
	};

	static_assert(sizeof(GfxWorldVertex) == 0x2C);
	static_assert(offsetof(GfxWorldVertex, xyz) == 0x0);
	static_assert(offsetof(GfxWorldVertex, binormalSign) == 0xC);
	static_assert(offsetof(GfxWorldVertex, color) == 0x10);
	static_assert(offsetof(GfxWorldVertex, texCoord) == 0x14);
	static_assert(offsetof(GfxWorldVertex, lmapCoord) == 0x1C);
	static_assert(offsetof(GfxWorldVertex, normal) == 0x24);
	static_assert(offsetof(GfxWorldVertex, tangent) == 0x28);

	inline GfxWorldVertex Convert(const Game::GfxWorldVertex& from)
	{
		GfxWorldVertex to{};
		std::memcpy(to.xyz, from.xyz, sizeof(to.xyz));
		to.binormalSign = static_cast<float>(from.binormalSign);
		to.color = Convert(from.color);
		std::memcpy(to.texCoord, from.texCoord, sizeof(to.texCoord));
		std::memcpy(to.lmapCoord, from.lmapCoord, sizeof(to.lmapCoord));
		to.normal = Convert(from.normal);
		to.tangent = Convert(from.tangent);
		return to;
	}

	inline Game::GfxWorldVertex Convert(const GfxWorldVertex& from)
	{
		Game::GfxWorldVertex to{};
		std::memcpy(to.xyz, from.xyz, sizeof(from.xyz));
		to.binormalSign = static_cast<decltype(to.binormalSign)>(from.binormalSign);
		to.color = Convert(from.color);
		std::memcpy(to.texCoord, from.texCoord, sizeof(from.texCoord));
		std::memcpy(to.lmapCoord, from.lmapCoord, sizeof(from.lmapCoord));
		to.normal = Convert(from.normal);
		to.tangent = Convert(from.tangent);
		return to;
	}

	struct GfxLightGridEntry
	{
		std::uint16_t colorsIndex;
		std::int8_t primaryLightIndex;
		std::int8_t needsTrace;
	};

	static_assert(sizeof(GfxLightGridEntry) == 0x4);
	static_assert(offsetof(GfxLightGridEntry, colorsIndex) == 0x0);
	static_assert(offsetof(GfxLightGridEntry, primaryLightIndex) == 0x2);
	static_assert(offsetof(GfxLightGridEntry, needsTrace) == 0x3);

	inline GfxLightGridEntry Convert(const Game::GfxLightGridEntry& from)
	{
		GfxLightGridEntry to{};
		to.colorsIndex = static_cast<std::uint16_t>(from.colorsIndex);
		to.primaryLightIndex = static_cast<std::int8_t>(from.primaryLightIndex);
		to.needsTrace = static_cast<std::int8_t>(from.needsTrace);
		return to;
	}

	inline Game::GfxLightGridEntry Convert(const GfxLightGridEntry& from)
	{
		Game::GfxLightGridEntry to{};
		to.colorsIndex = static_cast<decltype(to.colorsIndex)>(from.colorsIndex);
		to.primaryLightIndex = static_cast<decltype(to.primaryLightIndex)>(from.primaryLightIndex);
		to.needsTrace = static_cast<decltype(to.needsTrace)>(from.needsTrace);
		return to;
	}

	struct GfxLightGridColors
	{
		std::uint8_t rgb[56][3];
	};

	static_assert(sizeof(GfxLightGridColors) == 0xA8);
	static_assert(offsetof(GfxLightGridColors, rgb) == 0x0);

	inline GfxLightGridColors Convert(const Game::GfxLightGridColors& from)
	{
		GfxLightGridColors to{};
		std::memcpy(to.rgb, from.rgb, sizeof(to.rgb));
		return to;
	}

	inline Game::GfxLightGridColors Convert(const GfxLightGridColors& from)
	{
		Game::GfxLightGridColors to{};
		std::memcpy(to.rgb, from.rgb, sizeof(from.rgb));
		return to;
	}

	struct GfxBrushModelWritable
	{
		Bounds bounds;
	};

	static_assert(sizeof(GfxBrushModelWritable) == 0x18);
	static_assert(offsetof(GfxBrushModelWritable, bounds) == 0x0);

	inline GfxBrushModelWritable Convert(const Game::GfxBrushModelWritable& from)
	{
		GfxBrushModelWritable to{};
		to.bounds = Convert(from.bounds);
		return to;
	}

	inline Game::GfxBrushModelWritable Convert(const GfxBrushModelWritable& from)
	{
		Game::GfxBrushModelWritable to{};
		to.bounds = Convert(from.bounds);
		return to;
	}

	struct GfxBrushModel
	{
		GfxBrushModelWritable writable;
		Bounds bounds;
		float radius;
		std::uint16_t surfaceCount;
		std::uint16_t startSurfIndex;
		std::uint16_t surfaceCountNoDecal;
	};

	static_assert(sizeof(GfxBrushModel) == 0x3C);
	static_assert(offsetof(GfxBrushModel, writable) == 0x0);
	static_assert(offsetof(GfxBrushModel, bounds) == 0x18);
	static_assert(offsetof(GfxBrushModel, radius) == 0x30);
	static_assert(offsetof(GfxBrushModel, surfaceCount) == 0x34);
	static_assert(offsetof(GfxBrushModel, startSurfIndex) == 0x36);
	static_assert(offsetof(GfxBrushModel, surfaceCountNoDecal) == 0x38);

	inline GfxBrushModel Convert(const Game::GfxBrushModel& from)
	{
		GfxBrushModel to{};
		to.writable = Convert(from.writable);
		to.bounds = Convert(from.bounds);
		to.radius = static_cast<float>(from.radius);
		to.surfaceCount = static_cast<std::uint16_t>(from.surfaceCount);
		to.startSurfIndex = static_cast<std::uint16_t>(from.startSurfIndex);
		to.surfaceCountNoDecal = static_cast<std::uint16_t>(from.surfaceCountNoDecal);
		return to;
	}

	inline Game::GfxBrushModel Convert(const GfxBrushModel& from)
	{
		Game::GfxBrushModel to{};
		to.writable = Convert(from.writable);
		to.bounds = Convert(from.bounds);
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		to.surfaceCount = static_cast<decltype(to.surfaceCount)>(from.surfaceCount);
		to.startSurfIndex = static_cast<decltype(to.startSurfIndex)>(from.startSurfIndex);
		to.surfaceCountNoDecal = static_cast<decltype(to.surfaceCountNoDecal)>(from.surfaceCountNoDecal);
		return to;
	}

	struct MaterialMemory
	{
		std::uint32_t material;
		std::int32_t memory;
	};

	static_assert(sizeof(MaterialMemory) == 0x8);
	static_assert(offsetof(MaterialMemory, material) == 0x0);
	static_assert(offsetof(MaterialMemory, memory) == 0x4);

	inline MaterialMemory Convert(const Game::MaterialMemory& from)
	{
		MaterialMemory to{};
		to.memory = static_cast<std::int32_t>(from.memory);
		return to;
	}

	inline Game::MaterialMemory Convert(const MaterialMemory& from)
	{
		Game::MaterialMemory to{};
		to.memory = static_cast<decltype(to.memory)>(from.memory);
		return to;
	}

	struct XModelDrawInfo
	{
		std::int8_t hasGfxEntIndex;
		std::int8_t lod;
		std::uint16_t surfId;
	};

	static_assert(sizeof(XModelDrawInfo) == 0x4);
	static_assert(offsetof(XModelDrawInfo, hasGfxEntIndex) == 0x0);
	static_assert(offsetof(XModelDrawInfo, lod) == 0x1);
	static_assert(offsetof(XModelDrawInfo, surfId) == 0x2);

	inline XModelDrawInfo Convert(const Game::XModelDrawInfo& from)
	{
		XModelDrawInfo to{};
		to.hasGfxEntIndex = static_cast<std::int8_t>(from.hasGfxEntIndex);
		to.lod = static_cast<std::int8_t>(from.lod);
		to.surfId = static_cast<std::uint16_t>(from.surfId);
		return to;
	}

	inline Game::XModelDrawInfo Convert(const XModelDrawInfo& from)
	{
		Game::XModelDrawInfo to{};
		to.hasGfxEntIndex = static_cast<decltype(to.hasGfxEntIndex)>(from.hasGfxEntIndex);
		to.lod = static_cast<decltype(to.lod)>(from.lod);
		to.surfId = static_cast<decltype(to.surfId)>(from.surfId);
		return to;
	}

	struct GfxSceneDynModel
	{
		XModelDrawInfo info;
		std::uint16_t dynEntId;
	};

	static_assert(sizeof(GfxSceneDynModel) == 0x6);
	static_assert(offsetof(GfxSceneDynModel, info) == 0x0);
	static_assert(offsetof(GfxSceneDynModel, dynEntId) == 0x4);

	inline GfxSceneDynModel Convert(const Game::GfxSceneDynModel& from)
	{
		GfxSceneDynModel to{};
		to.info = Convert(from.info);
		to.dynEntId = static_cast<std::uint16_t>(from.dynEntId);
		return to;
	}

	inline Game::GfxSceneDynModel Convert(const GfxSceneDynModel& from)
	{
		Game::GfxSceneDynModel to{};
		to.info = Convert(from.info);
		to.dynEntId = static_cast<decltype(to.dynEntId)>(from.dynEntId);
		return to;
	}

	struct BModelDrawInfo
	{
		std::uint16_t surfId;
	};

	static_assert(sizeof(BModelDrawInfo) == 0x2);
	static_assert(offsetof(BModelDrawInfo, surfId) == 0x0);

	inline BModelDrawInfo Convert(const Game::BModelDrawInfo& from)
	{
		BModelDrawInfo to{};
		to.surfId = static_cast<std::uint16_t>(from.surfId);
		return to;
	}

	inline Game::BModelDrawInfo Convert(const BModelDrawInfo& from)
	{
		Game::BModelDrawInfo to{};
		to.surfId = static_cast<decltype(to.surfId)>(from.surfId);
		return to;
	}

	struct GfxSceneDynBrush
	{
		BModelDrawInfo info;
		std::uint16_t dynEntId;
	};

	static_assert(sizeof(GfxSceneDynBrush) == 0x4);
	static_assert(offsetof(GfxSceneDynBrush, info) == 0x0);
	static_assert(offsetof(GfxSceneDynBrush, dynEntId) == 0x2);

	inline GfxSceneDynBrush Convert(const Game::GfxSceneDynBrush& from)
	{
		GfxSceneDynBrush to{};
		to.info = Convert(from.info);
		to.dynEntId = static_cast<std::uint16_t>(from.dynEntId);
		return to;
	}

	inline Game::GfxSceneDynBrush Convert(const GfxSceneDynBrush& from)
	{
		Game::GfxSceneDynBrush to{};
		to.info = Convert(from.info);
		to.dynEntId = static_cast<decltype(to.dynEntId)>(from.dynEntId);
		return to;
	}

	struct GfxShadowGeometry
	{
		std::uint16_t surfaceCount;
		std::uint16_t smodelCount;
		std::uint32_t sortedSurfIndex;
		std::uint32_t smodelIndex;
	};

	static_assert(sizeof(GfxShadowGeometry) == 0xC);
	static_assert(offsetof(GfxShadowGeometry, surfaceCount) == 0x0);
	static_assert(offsetof(GfxShadowGeometry, smodelCount) == 0x2);
	static_assert(offsetof(GfxShadowGeometry, sortedSurfIndex) == 0x4);
	static_assert(offsetof(GfxShadowGeometry, smodelIndex) == 0x8);

	inline GfxShadowGeometry Convert(const Game::GfxShadowGeometry& from)
	{
		GfxShadowGeometry to{};
		to.surfaceCount = static_cast<std::uint16_t>(from.surfaceCount);
		to.smodelCount = static_cast<std::uint16_t>(from.smodelCount);
		return to;
	}

	inline Game::GfxShadowGeometry Convert(const GfxShadowGeometry& from)
	{
		Game::GfxShadowGeometry to{};
		to.surfaceCount = static_cast<decltype(to.surfaceCount)>(from.surfaceCount);
		to.smodelCount = static_cast<decltype(to.smodelCount)>(from.smodelCount);
		return to;
	}

	struct GfxLightRegion
	{
		std::uint32_t hullCount;
		std::uint32_t hulls;
	};

	static_assert(sizeof(GfxLightRegion) == 0x8);
	static_assert(offsetof(GfxLightRegion, hullCount) == 0x0);
	static_assert(offsetof(GfxLightRegion, hulls) == 0x4);

	inline GfxLightRegion Convert(const Game::GfxLightRegion& from)
	{
		GfxLightRegion to{};
		to.hullCount = static_cast<std::uint32_t>(from.hullCount);
		return to;
	}

	inline Game::GfxLightRegion Convert(const GfxLightRegion& from)
	{
		Game::GfxLightRegion to{};
		to.hullCount = static_cast<decltype(to.hullCount)>(from.hullCount);
		return to;
	}

	struct GfxLightRegionHull
	{
		float kdopMidPoint[9];
		float kdopHalfSize[9];
		std::uint32_t axisCount;
		std::uint32_t axis;
	};

	static_assert(sizeof(GfxLightRegionHull) == 0x50);
	static_assert(offsetof(GfxLightRegionHull, kdopMidPoint) == 0x0);
	static_assert(offsetof(GfxLightRegionHull, kdopHalfSize) == 0x24);
	static_assert(offsetof(GfxLightRegionHull, axisCount) == 0x48);
	static_assert(offsetof(GfxLightRegionHull, axis) == 0x4C);

	inline GfxLightRegionHull Convert(const Game::GfxLightRegionHull& from)
	{
		GfxLightRegionHull to{};
		std::memcpy(to.kdopMidPoint, from.kdopMidPoint, sizeof(to.kdopMidPoint));
		std::memcpy(to.kdopHalfSize, from.kdopHalfSize, sizeof(to.kdopHalfSize));
		to.axisCount = static_cast<std::uint32_t>(from.axisCount);
		return to;
	}

	inline Game::GfxLightRegionHull Convert(const GfxLightRegionHull& from)
	{
		Game::GfxLightRegionHull to{};
		std::memcpy(to.kdopMidPoint, from.kdopMidPoint, sizeof(from.kdopMidPoint));
		std::memcpy(to.kdopHalfSize, from.kdopHalfSize, sizeof(from.kdopHalfSize));
		to.axisCount = static_cast<decltype(to.axisCount)>(from.axisCount);
		return to;
	}

	struct GfxLightRegionAxis
	{
		float dir[3];
		float midPoint;
		float halfSize;
	};

	static_assert(sizeof(GfxLightRegionAxis) == 0x14);
	static_assert(offsetof(GfxLightRegionAxis, dir) == 0x0);
	static_assert(offsetof(GfxLightRegionAxis, midPoint) == 0xC);
	static_assert(offsetof(GfxLightRegionAxis, halfSize) == 0x10);

	inline GfxLightRegionAxis Convert(const Game::GfxLightRegionAxis& from)
	{
		GfxLightRegionAxis to{};
		std::memcpy(to.dir, from.dir, sizeof(to.dir));
		to.midPoint = static_cast<float>(from.midPoint);
		to.halfSize = static_cast<float>(from.halfSize);
		return to;
	}

	inline Game::GfxLightRegionAxis Convert(const GfxLightRegionAxis& from)
	{
		Game::GfxLightRegionAxis to{};
		std::memcpy(to.dir, from.dir, sizeof(from.dir));
		to.midPoint = static_cast<decltype(to.midPoint)>(from.midPoint);
		to.halfSize = static_cast<decltype(to.halfSize)>(from.halfSize);
		return to;
	}

	struct GfxStaticModelInst
	{
		Bounds bounds;
		float lightingOrigin[3];
	};

	static_assert(sizeof(GfxStaticModelInst) == 0x24);
	static_assert(offsetof(GfxStaticModelInst, bounds) == 0x0);
	static_assert(offsetof(GfxStaticModelInst, lightingOrigin) == 0x18);

	inline GfxStaticModelInst Convert(const Game::GfxStaticModelInst& from)
	{
		GfxStaticModelInst to{};
		to.bounds = Convert(from.bounds);
		std::memcpy(to.lightingOrigin, from.lightingOrigin, sizeof(to.lightingOrigin));
		return to;
	}

	inline Game::GfxStaticModelInst Convert(const GfxStaticModelInst& from)
	{
		Game::GfxStaticModelInst to{};
		to.bounds = Convert(from.bounds);
		std::memcpy(to.lightingOrigin, from.lightingOrigin, sizeof(from.lightingOrigin));
		return to;
	}

	struct srfTriangles_t
	{
		std::uint32_t vertexLayerData;
		std::uint32_t firstVertex;
		std::uint16_t vertexCount;
		std::uint16_t triCount;
		std::uint32_t baseIndex;
	};

	static_assert(sizeof(srfTriangles_t) == 0x10);
	static_assert(offsetof(srfTriangles_t, vertexLayerData) == 0x0);
	static_assert(offsetof(srfTriangles_t, firstVertex) == 0x4);
	static_assert(offsetof(srfTriangles_t, vertexCount) == 0x8);
	static_assert(offsetof(srfTriangles_t, triCount) == 0xA);
	static_assert(offsetof(srfTriangles_t, baseIndex) == 0xC);

	inline srfTriangles_t Convert(const Game::srfTriangles_t& from)
	{
		srfTriangles_t to{};
		to.vertexLayerData = static_cast<std::uint32_t>(from.vertexLayerData);
		to.firstVertex = static_cast<std::uint32_t>(from.firstVertex);
		to.vertexCount = static_cast<std::uint16_t>(from.vertexCount);
		to.triCount = static_cast<std::uint16_t>(from.triCount);
		to.baseIndex = static_cast<std::uint32_t>(from.baseIndex);
		return to;
	}

	inline Game::srfTriangles_t Convert(const srfTriangles_t& from)
	{
		Game::srfTriangles_t to{};
		to.vertexLayerData = static_cast<decltype(to.vertexLayerData)>(from.vertexLayerData);
		to.firstVertex = static_cast<decltype(to.firstVertex)>(from.firstVertex);
		to.vertexCount = static_cast<decltype(to.vertexCount)>(from.vertexCount);
		to.triCount = static_cast<decltype(to.triCount)>(from.triCount);
		to.baseIndex = static_cast<decltype(to.baseIndex)>(from.baseIndex);
		return to;
	}

	struct GfxSurfaceLightingAndFlagsFields
	{
		std::uint8_t lightmapIndex;
		std::uint8_t reflectionProbeIndex;
		std::uint8_t primaryLightIndex;
		std::uint8_t flags;
	};

	static_assert(sizeof(GfxSurfaceLightingAndFlagsFields) == 0x4);
	static_assert(offsetof(GfxSurfaceLightingAndFlagsFields, lightmapIndex) == 0x0);
	static_assert(offsetof(GfxSurfaceLightingAndFlagsFields, reflectionProbeIndex) == 0x1);
	static_assert(offsetof(GfxSurfaceLightingAndFlagsFields, primaryLightIndex) == 0x2);
	static_assert(offsetof(GfxSurfaceLightingAndFlagsFields, flags) == 0x3);

	inline GfxSurfaceLightingAndFlagsFields Convert(const Game::GfxSurfaceLightingAndFlagsFields& from)
	{
		GfxSurfaceLightingAndFlagsFields to{};
		to.lightmapIndex = static_cast<std::uint8_t>(from.lightmapIndex);
		to.reflectionProbeIndex = static_cast<std::uint8_t>(from.reflectionProbeIndex);
		to.primaryLightIndex = static_cast<std::uint8_t>(from.primaryLightIndex);
		to.flags = static_cast<std::uint8_t>(from.flags);
		return to;
	}

	inline Game::GfxSurfaceLightingAndFlagsFields Convert(const GfxSurfaceLightingAndFlagsFields& from)
	{
		Game::GfxSurfaceLightingAndFlagsFields to{};
		to.lightmapIndex = static_cast<decltype(to.lightmapIndex)>(from.lightmapIndex);
		to.reflectionProbeIndex = static_cast<decltype(to.reflectionProbeIndex)>(from.reflectionProbeIndex);
		to.primaryLightIndex = static_cast<decltype(to.primaryLightIndex)>(from.primaryLightIndex);
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		return to;
	}

	union GfxSurfaceLightingAndFlags
	{
		GfxSurfaceLightingAndFlagsFields fields;
		std::uint32_t packed;
	};

	static_assert(sizeof(GfxSurfaceLightingAndFlags) == 0x4);
	static_assert(offsetof(GfxSurfaceLightingAndFlags, fields) == 0x0);
	static_assert(offsetof(GfxSurfaceLightingAndFlags, packed) == 0x0);

	inline GfxSurfaceLightingAndFlags Convert(const Game::GfxSurfaceLightingAndFlags& from)
	{
		GfxSurfaceLightingAndFlags to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::GfxSurfaceLightingAndFlags Convert(const GfxSurfaceLightingAndFlags& from)
	{
		Game::GfxSurfaceLightingAndFlags to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct GfxSurface
	{
		srfTriangles_t tris;
		std::uint32_t material;
		GfxSurfaceLightingAndFlags laf;
	};

	static_assert(sizeof(GfxSurface) == 0x18);
	static_assert(offsetof(GfxSurface, tris) == 0x0);
	static_assert(offsetof(GfxSurface, material) == 0x10);
	static_assert(offsetof(GfxSurface, laf) == 0x14);

	inline GfxSurface Convert(const Game::GfxSurface& from)
	{
		GfxSurface to{};
		to.tris = Convert(from.tris);
		to.laf = Convert(from.laf);
		return to;
	}

	inline Game::GfxSurface Convert(const GfxSurface& from)
	{
		Game::GfxSurface to{};
		to.tris = Convert(from.tris);
		to.laf = Convert(from.laf);
		return to;
	}

	struct GfxSurfaceBounds
	{
		Bounds bounds;
	};

	static_assert(sizeof(GfxSurfaceBounds) == 0x18);
	static_assert(offsetof(GfxSurfaceBounds, bounds) == 0x0);

	inline GfxSurfaceBounds Convert(const Game::GfxSurfaceBounds& from)
	{
		GfxSurfaceBounds to{};
		to.bounds = Convert(from.bounds);
		return to;
	}

	inline Game::GfxSurfaceBounds Convert(const GfxSurfaceBounds& from)
	{
		Game::GfxSurfaceBounds to{};
		to.bounds = Convert(from.bounds);
		return to;
	}

	struct GfxPackedPlacement
	{
		float origin[3];
		float axis[3][3];
		float scale;
	};

	static_assert(sizeof(GfxPackedPlacement) == 0x34);
	static_assert(offsetof(GfxPackedPlacement, origin) == 0x0);
	static_assert(offsetof(GfxPackedPlacement, axis) == 0xC);
	static_assert(offsetof(GfxPackedPlacement, scale) == 0x30);

	inline GfxPackedPlacement Convert(const Game::GfxPackedPlacement& from)
	{
		GfxPackedPlacement to{};
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		std::memcpy(to.axis, from.axis, sizeof(to.axis));
		to.scale = static_cast<float>(from.scale);
		return to;
	}

	inline Game::GfxPackedPlacement Convert(const GfxPackedPlacement& from)
	{
		Game::GfxPackedPlacement to{};
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		std::memcpy(to.axis, from.axis, sizeof(from.axis));
		to.scale = static_cast<decltype(to.scale)>(from.scale);
		return to;
	}

	struct GfxStaticModelDrawInst
	{
		GfxPackedPlacement placement;
		std::uint32_t model;
		std::uint16_t cullDist;
		std::uint16_t lightingHandle;
		std::uint8_t reflectionProbeIndex;
		std::uint8_t primaryLightIndex;
		std::uint8_t flags;
		std::uint8_t firstMtlSkinIndex;
		GfxColor groundLighting;
		std::uint16_t cacheId[4];
	};

	static_assert(sizeof(GfxStaticModelDrawInst) == 0x4C);
	static_assert(offsetof(GfxStaticModelDrawInst, placement) == 0x0);
	static_assert(offsetof(GfxStaticModelDrawInst, model) == 0x34);
	static_assert(offsetof(GfxStaticModelDrawInst, cullDist) == 0x38);
	static_assert(offsetof(GfxStaticModelDrawInst, lightingHandle) == 0x3A);
	static_assert(offsetof(GfxStaticModelDrawInst, reflectionProbeIndex) == 0x3C);
	static_assert(offsetof(GfxStaticModelDrawInst, primaryLightIndex) == 0x3D);
	static_assert(offsetof(GfxStaticModelDrawInst, flags) == 0x3E);
	static_assert(offsetof(GfxStaticModelDrawInst, firstMtlSkinIndex) == 0x3F);
	static_assert(offsetof(GfxStaticModelDrawInst, groundLighting) == 0x40);
	static_assert(offsetof(GfxStaticModelDrawInst, cacheId) == 0x44);

	inline GfxStaticModelDrawInst Convert(const Game::GfxStaticModelDrawInst& from)
	{
		GfxStaticModelDrawInst to{};
		to.placement = Convert(from.placement);
		to.cullDist = static_cast<std::uint16_t>(from.cullDist);
		to.lightingHandle = static_cast<std::uint16_t>(from.lightingHandle);
		to.reflectionProbeIndex = static_cast<std::uint8_t>(from.reflectionProbeIndex);
		to.primaryLightIndex = static_cast<std::uint8_t>(from.primaryLightIndex);
		to.flags = static_cast<std::uint8_t>(from.flags);
		to.firstMtlSkinIndex = static_cast<std::uint8_t>(from.firstMtlSkinIndex);
		to.groundLighting = Convert(from.groundLighting);
		std::memcpy(to.cacheId, from.cacheId, sizeof(to.cacheId));
		return to;
	}

	inline Game::GfxStaticModelDrawInst Convert(const GfxStaticModelDrawInst& from)
	{
		Game::GfxStaticModelDrawInst to{};
		to.placement = Convert(from.placement);
		to.cullDist = static_cast<decltype(to.cullDist)>(from.cullDist);
		to.lightingHandle = static_cast<decltype(to.lightingHandle)>(from.lightingHandle);
		to.reflectionProbeIndex = static_cast<decltype(to.reflectionProbeIndex)>(from.reflectionProbeIndex);
		to.primaryLightIndex = static_cast<decltype(to.primaryLightIndex)>(from.primaryLightIndex);
		to.flags = static_cast<decltype(to.flags)>(from.flags);
		to.firstMtlSkinIndex = static_cast<decltype(to.firstMtlSkinIndex)>(from.firstMtlSkinIndex);
		to.groundLighting = Convert(from.groundLighting);
		std::memcpy(to.cacheId, from.cacheId, sizeof(from.cacheId));
		return to;
	}

	struct GfxHeroOnlyLight
	{
		std::int8_t type;
		std::int8_t unused[3];
		float color[3];
		float dir[3];
		float origin[3];
		float radius;
		float cosHalfFovOuter;
		float cosHalfFovInner;
		std::int32_t exponent;
	};

	static_assert(sizeof(GfxHeroOnlyLight) == 0x38);
	static_assert(offsetof(GfxHeroOnlyLight, type) == 0x0);
	static_assert(offsetof(GfxHeroOnlyLight, unused) == 0x1);
	static_assert(offsetof(GfxHeroOnlyLight, color) == 0x4);
	static_assert(offsetof(GfxHeroOnlyLight, dir) == 0x10);
	static_assert(offsetof(GfxHeroOnlyLight, origin) == 0x1C);
	static_assert(offsetof(GfxHeroOnlyLight, radius) == 0x28);
	static_assert(offsetof(GfxHeroOnlyLight, cosHalfFovOuter) == 0x2C);
	static_assert(offsetof(GfxHeroOnlyLight, cosHalfFovInner) == 0x30);
	static_assert(offsetof(GfxHeroOnlyLight, exponent) == 0x34);

	inline GfxHeroOnlyLight Convert(const Game::GfxHeroOnlyLight& from)
	{
		GfxHeroOnlyLight to{};
		to.type = static_cast<std::int8_t>(from.type);
		std::memcpy(to.unused, from.unused, sizeof(to.unused));
		std::memcpy(to.color, from.color, sizeof(to.color));
		std::memcpy(to.dir, from.dir, sizeof(to.dir));
		std::memcpy(to.origin, from.origin, sizeof(to.origin));
		to.radius = static_cast<float>(from.radius);
		to.cosHalfFovOuter = static_cast<float>(from.cosHalfFovOuter);
		to.cosHalfFovInner = static_cast<float>(from.cosHalfFovInner);
		to.exponent = static_cast<std::int32_t>(from.exponent);
		return to;
	}

	inline Game::GfxHeroOnlyLight Convert(const GfxHeroOnlyLight& from)
	{
		Game::GfxHeroOnlyLight to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		std::memcpy(to.unused, from.unused, sizeof(from.unused));
		std::memcpy(to.color, from.color, sizeof(from.color));
		std::memcpy(to.dir, from.dir, sizeof(from.dir));
		std::memcpy(to.origin, from.origin, sizeof(from.origin));
		to.radius = static_cast<decltype(to.radius)>(from.radius);
		to.cosHalfFovOuter = static_cast<decltype(to.cosHalfFovOuter)>(from.cosHalfFovOuter);
		to.cosHalfFovInner = static_cast<decltype(to.cosHalfFovInner)>(from.cosHalfFovInner);
		to.exponent = static_cast<decltype(to.exponent)>(from.exponent);
		return to;
	}

	struct GfxLightImage
	{
		std::uint32_t image;
		std::int8_t samplerState;
	};

	static_assert(sizeof(GfxLightImage) == 0x8);
	static_assert(offsetof(GfxLightImage, image) == 0x0);
	static_assert(offsetof(GfxLightImage, samplerState) == 0x4);

	inline GfxLightImage Convert(const Game::GfxLightImage& from)
	{
		GfxLightImage to{};
		to.samplerState = static_cast<std::int8_t>(from.samplerState);
		return to;
	}

	inline Game::GfxLightImage Convert(const GfxLightImage& from)
	{
		Game::GfxLightImage to{};
		to.samplerState = static_cast<decltype(to.samplerState)>(from.samplerState);
		return to;
	}

	struct GfxLightDef
	{
		std::uint32_t name;
		GfxLightImage attenuation;
		std::int32_t lmapLookupStart;
	};

	static_assert(sizeof(GfxLightDef) == 0x10);
	static_assert(offsetof(GfxLightDef, name) == 0x0);
	static_assert(offsetof(GfxLightDef, attenuation) == 0x4);
	static_assert(offsetof(GfxLightDef, lmapLookupStart) == 0xC);

	inline GfxLightDef Convert(const Game::GfxLightDef& from)
	{
		GfxLightDef to{};
		to.attenuation = Convert(from.attenuation);
		to.lmapLookupStart = static_cast<std::int32_t>(from.lmapLookupStart);
		return to;
	}

	inline Game::GfxLightDef Convert(const GfxLightDef& from)
	{
		Game::GfxLightDef to{};
		to.attenuation = Convert(from.attenuation);
		to.lmapLookupStart = static_cast<decltype(to.lmapLookupStart)>(from.lmapLookupStart);
		return to;
	}

	struct Font_s
	{
		std::uint32_t fontName;
		std::int32_t pixelHeight;
		std::int32_t glyphCount;
		std::uint32_t material;
		std::uint32_t glowMaterial;
		std::uint32_t glyphs;
	};

	static_assert(sizeof(Font_s) == 0x18);
	static_assert(offsetof(Font_s, fontName) == 0x0);
	static_assert(offsetof(Font_s, pixelHeight) == 0x4);
	static_assert(offsetof(Font_s, glyphCount) == 0x8);
	static_assert(offsetof(Font_s, material) == 0xC);
	static_assert(offsetof(Font_s, glowMaterial) == 0x10);
	static_assert(offsetof(Font_s, glyphs) == 0x14);

	inline Font_s Convert(const Game::Font_s& from)
	{
		Font_s to{};
		to.pixelHeight = static_cast<std::int32_t>(from.pixelHeight);
		to.glyphCount = static_cast<std::int32_t>(from.glyphCount);
		return to;
	}

	inline Game::Font_s Convert(const Font_s& from)
	{
		Game::Font_s to{};
		to.pixelHeight = static_cast<decltype(to.pixelHeight)>(from.pixelHeight);
		to.glyphCount = static_cast<decltype(to.glyphCount)>(from.glyphCount);
		return to;
	}

	struct Glyph
	{
		std::uint16_t letter;
		std::int8_t x0;
		std::int8_t y0;
		std::int8_t dx;
		std::int8_t pixelWidth;
		std::int8_t pixelHeight;
		float s0;
		float t0;
		float s1;
		float t1;
	};

	static_assert(sizeof(Glyph) == 0x18);
	static_assert(offsetof(Glyph, letter) == 0x0);
	static_assert(offsetof(Glyph, x0) == 0x2);
	static_assert(offsetof(Glyph, y0) == 0x3);
	static_assert(offsetof(Glyph, dx) == 0x4);
	static_assert(offsetof(Glyph, pixelWidth) == 0x5);
	static_assert(offsetof(Glyph, pixelHeight) == 0x6);
	static_assert(offsetof(Glyph, s0) == 0x8);
	static_assert(offsetof(Glyph, t0) == 0xC);
	static_assert(offsetof(Glyph, s1) == 0x10);
	static_assert(offsetof(Glyph, t1) == 0x14);

	inline Glyph Convert(const Game::Glyph& from)
	{
		Glyph to{};
		to.letter = static_cast<std::uint16_t>(from.letter);
		to.x0 = static_cast<std::int8_t>(from.x0);
		to.y0 = static_cast<std::int8_t>(from.y0);
		to.dx = static_cast<std::int8_t>(from.dx);
		to.pixelWidth = static_cast<std::int8_t>(from.pixelWidth);
		to.pixelHeight = static_cast<std::int8_t>(from.pixelHeight);
		to.s0 = static_cast<float>(from.s0);
		to.t0 = static_cast<float>(from.t0);
		to.s1 = static_cast<float>(from.s1);
		to.t1 = static_cast<float>(from.t1);
		return to;
	}

	inline Game::Glyph Convert(const Glyph& from)
	{
		Game::Glyph to{};
		to.letter = static_cast<decltype(to.letter)>(from.letter);
		to.x0 = static_cast<decltype(to.x0)>(from.x0);
		to.y0 = static_cast<decltype(to.y0)>(from.y0);
		to.dx = static_cast<decltype(to.dx)>(from.dx);
		to.pixelWidth = static_cast<decltype(to.pixelWidth)>(from.pixelWidth);
		to.pixelHeight = static_cast<decltype(to.pixelHeight)>(from.pixelHeight);
		to.s0 = static_cast<decltype(to.s0)>(from.s0);
		to.t0 = static_cast<decltype(to.t0)>(from.t0);
		to.s1 = static_cast<decltype(to.s1)>(from.s1);
		to.t1 = static_cast<decltype(to.t1)>(from.t1);
		return to;
	}

	struct MenuList
	{
		std::uint32_t name;
		std::int32_t menuCount;
		std::uint32_t menus;
	};

	static_assert(sizeof(MenuList) == 0xC);
	static_assert(offsetof(MenuList, name) == 0x0);
	static_assert(offsetof(MenuList, menuCount) == 0x4);
	static_assert(offsetof(MenuList, menus) == 0x8);

	inline MenuList Convert(const Game::MenuList& from)
	{
		MenuList to{};
		to.menuCount = static_cast<std::int32_t>(from.menuCount);
		return to;
	}

	inline Game::MenuList Convert(const MenuList& from)
	{
		Game::MenuList to{};
		to.menuCount = static_cast<decltype(to.menuCount)>(from.menuCount);
		return to;
	}

	struct rectDef_s
	{
		float x;
		float y;
		float w;
		float h;
		std::int8_t horzAlign;
		std::int8_t vertAlign;
	};

	static_assert(sizeof(rectDef_s) == 0x14);
	static_assert(offsetof(rectDef_s, x) == 0x0);
	static_assert(offsetof(rectDef_s, y) == 0x4);
	static_assert(offsetof(rectDef_s, w) == 0x8);
	static_assert(offsetof(rectDef_s, h) == 0xC);
	static_assert(offsetof(rectDef_s, horzAlign) == 0x10);
	static_assert(offsetof(rectDef_s, vertAlign) == 0x11);

	inline rectDef_s Convert(const Game::rectDef_s& from)
	{
		rectDef_s to{};
		to.x = static_cast<float>(from.x);
		to.y = static_cast<float>(from.y);
		to.w = static_cast<float>(from.w);
		to.h = static_cast<float>(from.h);
		to.horzAlign = static_cast<std::int8_t>(from.horzAlign);
		to.vertAlign = static_cast<std::int8_t>(from.vertAlign);
		return to;
	}

	inline Game::rectDef_s Convert(const rectDef_s& from)
	{
		Game::rectDef_s to{};
		to.x = static_cast<decltype(to.x)>(from.x);
		to.y = static_cast<decltype(to.y)>(from.y);
		to.w = static_cast<decltype(to.w)>(from.w);
		to.h = static_cast<decltype(to.h)>(from.h);
		to.horzAlign = static_cast<decltype(to.horzAlign)>(from.horzAlign);
		to.vertAlign = static_cast<decltype(to.vertAlign)>(from.vertAlign);
		return to;
	}

	struct windowDef_t
	{
		std::uint32_t name;
		rectDef_s rect;
		rectDef_s rectClient;
		std::uint32_t group;
		std::int32_t style;
		std::int32_t border;
		std::int32_t ownerDraw;
		std::int32_t ownerDrawFlags;
		float borderSize;
		std::int32_t staticFlags;
		std::int32_t dynamicFlags[1];
		std::int32_t nextTime;
		float foreColor[4];
		float backColor[4];
		float borderColor[4];
		float outlineColor[4];
		float disableColor[4];
		std::uint32_t background;
	};

	static_assert(sizeof(windowDef_t) == 0xA4);
	static_assert(offsetof(windowDef_t, name) == 0x0);
	static_assert(offsetof(windowDef_t, rect) == 0x4);
	static_assert(offsetof(windowDef_t, rectClient) == 0x18);
	static_assert(offsetof(windowDef_t, group) == 0x2C);
	static_assert(offsetof(windowDef_t, style) == 0x30);
	static_assert(offsetof(windowDef_t, border) == 0x34);
	static_assert(offsetof(windowDef_t, ownerDraw) == 0x38);
	static_assert(offsetof(windowDef_t, ownerDrawFlags) == 0x3C);
	static_assert(offsetof(windowDef_t, borderSize) == 0x40);
	static_assert(offsetof(windowDef_t, staticFlags) == 0x44);
	static_assert(offsetof(windowDef_t, dynamicFlags) == 0x48);
	static_assert(offsetof(windowDef_t, nextTime) == 0x4C);
	static_assert(offsetof(windowDef_t, foreColor) == 0x50);
	static_assert(offsetof(windowDef_t, backColor) == 0x60);
	static_assert(offsetof(windowDef_t, borderColor) == 0x70);
	static_assert(offsetof(windowDef_t, outlineColor) == 0x80);
	static_assert(offsetof(windowDef_t, disableColor) == 0x90);
	static_assert(offsetof(windowDef_t, background) == 0xA0);

	inline windowDef_t Convert(const Game::windowDef_t& from)
	{
		windowDef_t to{};
		to.rect = Convert(from.rect);
		to.rectClient = Convert(from.rectClient);
		to.style = static_cast<std::int32_t>(from.style);
		to.border = static_cast<std::int32_t>(from.border);
		to.ownerDraw = static_cast<std::int32_t>(from.ownerDraw);
		to.ownerDrawFlags = static_cast<std::int32_t>(from.ownerDrawFlags);
		to.borderSize = static_cast<float>(from.borderSize);
		to.staticFlags = static_cast<std::int32_t>(from.staticFlags);
		std::memcpy(to.dynamicFlags, from.dynamicFlags, sizeof(to.dynamicFlags));
		to.nextTime = static_cast<std::int32_t>(from.nextTime);
		std::memcpy(to.foreColor, from.foreColor, sizeof(to.foreColor));
		std::memcpy(to.backColor, from.backColor, sizeof(to.backColor));
		std::memcpy(to.borderColor, from.borderColor, sizeof(to.borderColor));
		std::memcpy(to.outlineColor, from.outlineColor, sizeof(to.outlineColor));
		std::memcpy(to.disableColor, from.disableColor, sizeof(to.disableColor));
		return to;
	}

	inline Game::windowDef_t Convert(const windowDef_t& from)
	{
		Game::windowDef_t to{};
		to.rect = Convert(from.rect);
		to.rectClient = Convert(from.rectClient);
		to.style = static_cast<decltype(to.style)>(from.style);
		to.border = static_cast<decltype(to.border)>(from.border);
		to.ownerDraw = static_cast<decltype(to.ownerDraw)>(from.ownerDraw);
		to.ownerDrawFlags = static_cast<decltype(to.ownerDrawFlags)>(from.ownerDrawFlags);
		to.borderSize = static_cast<decltype(to.borderSize)>(from.borderSize);
		to.staticFlags = static_cast<decltype(to.staticFlags)>(from.staticFlags);
		std::memcpy(to.dynamicFlags, from.dynamicFlags, sizeof(from.dynamicFlags));
		to.nextTime = static_cast<decltype(to.nextTime)>(from.nextTime);
		std::memcpy(to.foreColor, from.foreColor, sizeof(from.foreColor));
		std::memcpy(to.backColor, from.backColor, sizeof(from.backColor));
		std::memcpy(to.borderColor, from.borderColor, sizeof(from.borderColor));
		std::memcpy(to.outlineColor, from.outlineColor, sizeof(from.outlineColor));
		std::memcpy(to.disableColor, from.disableColor, sizeof(from.disableColor));
		return to;
	}

	struct menuTransition
	{
		std::int32_t transitionType;
		std::int32_t targetField;
		std::int32_t startTime;
		float startVal;
		float endVal;
		float time;
		std::int32_t endTriggerType;
	};

	static_assert(sizeof(menuTransition) == 0x1C);
	static_assert(offsetof(menuTransition, transitionType) == 0x0);
	static_assert(offsetof(menuTransition, targetField) == 0x4);
	static_assert(offsetof(menuTransition, startTime) == 0x8);
	static_assert(offsetof(menuTransition, startVal) == 0xC);
	static_assert(offsetof(menuTransition, endVal) == 0x10);
	static_assert(offsetof(menuTransition, time) == 0x14);
	static_assert(offsetof(menuTransition, endTriggerType) == 0x18);

	inline menuTransition Convert(const Game::menuTransition& from)
	{
		menuTransition to{};
		to.transitionType = static_cast<std::int32_t>(from.transitionType);
		to.targetField = static_cast<std::int32_t>(from.targetField);
		to.startTime = static_cast<std::int32_t>(from.startTime);
		to.startVal = static_cast<float>(from.startVal);
		to.endVal = static_cast<float>(from.endVal);
		to.time = static_cast<float>(from.time);
		to.endTriggerType = static_cast<std::int32_t>(from.endTriggerType);
		return to;
	}

	inline Game::menuTransition Convert(const menuTransition& from)
	{
		Game::menuTransition to{};
		to.transitionType = static_cast<decltype(to.transitionType)>(from.transitionType);
		to.targetField = static_cast<decltype(to.targetField)>(from.targetField);
		to.startTime = static_cast<decltype(to.startTime)>(from.startTime);
		to.startVal = static_cast<decltype(to.startVal)>(from.startVal);
		to.endVal = static_cast<decltype(to.endVal)>(from.endVal);
		to.time = static_cast<decltype(to.time)>(from.time);
		to.endTriggerType = static_cast<decltype(to.endTriggerType)>(from.endTriggerType);
		return to;
	}

	struct menuDef_t
	{
		windowDef_t window;
		std::uint32_t font;
		std::int32_t fullScreen;
		std::int32_t itemCount;
		std::int32_t fontIndex;
		std::int32_t cursorItem[1];
		std::int32_t fadeCycle;
		float fadeClamp;
		float fadeAmount;
		float fadeInAmount;
		float blurRadius;
		std::uint32_t onOpen;
		std::uint32_t onCloseRequest;
		std::uint32_t onClose;
		std::uint32_t onESC;
		std::uint32_t onKey;
		std::uint32_t visibleExp;
		std::uint32_t allowedBinding;
		std::uint32_t soundName;
		std::int32_t imageTrack;
		float focusColor[4];
		std::uint32_t rectXExp;
		std::uint32_t rectYExp;
		std::uint32_t rectWExp;
		std::uint32_t rectHExp;
		std::uint32_t openSoundExp;
		std::uint32_t closeSoundExp;
		std::uint32_t items;
		menuTransition scaleTransition[1];
		menuTransition alphaTransition[1];
		menuTransition xTransition[1];
		menuTransition yTransition[1];
		std::uint32_t expressionData;
	};

	static_assert(sizeof(menuDef_t) == 0x190);
	static_assert(offsetof(menuDef_t, window) == 0x0);
	static_assert(offsetof(menuDef_t, font) == 0xA4);
	static_assert(offsetof(menuDef_t, fullScreen) == 0xA8);
	static_assert(offsetof(menuDef_t, itemCount) == 0xAC);
	static_assert(offsetof(menuDef_t, fontIndex) == 0xB0);
	static_assert(offsetof(menuDef_t, cursorItem) == 0xB4);
	static_assert(offsetof(menuDef_t, fadeCycle) == 0xB8);
	static_assert(offsetof(menuDef_t, fadeClamp) == 0xBC);
	static_assert(offsetof(menuDef_t, fadeAmount) == 0xC0);
	static_assert(offsetof(menuDef_t, fadeInAmount) == 0xC4);
	static_assert(offsetof(menuDef_t, blurRadius) == 0xC8);
	static_assert(offsetof(menuDef_t, onOpen) == 0xCC);
	static_assert(offsetof(menuDef_t, onCloseRequest) == 0xD0);
	static_assert(offsetof(menuDef_t, onClose) == 0xD4);
	static_assert(offsetof(menuDef_t, onESC) == 0xD8);
	static_assert(offsetof(menuDef_t, onKey) == 0xDC);
	static_assert(offsetof(menuDef_t, visibleExp) == 0xE0);
	static_assert(offsetof(menuDef_t, allowedBinding) == 0xE4);
	static_assert(offsetof(menuDef_t, soundName) == 0xE8);
	static_assert(offsetof(menuDef_t, imageTrack) == 0xEC);
	static_assert(offsetof(menuDef_t, focusColor) == 0xF0);
	static_assert(offsetof(menuDef_t, rectXExp) == 0x100);
	static_assert(offsetof(menuDef_t, rectYExp) == 0x104);
	static_assert(offsetof(menuDef_t, rectWExp) == 0x108);
	static_assert(offsetof(menuDef_t, rectHExp) == 0x10C);
	static_assert(offsetof(menuDef_t, openSoundExp) == 0x110);
	static_assert(offsetof(menuDef_t, closeSoundExp) == 0x114);
	static_assert(offsetof(menuDef_t, items) == 0x118);
	static_assert(offsetof(menuDef_t, scaleTransition) == 0x11C);
	static_assert(offsetof(menuDef_t, alphaTransition) == 0x138);
	static_assert(offsetof(menuDef_t, xTransition) == 0x154);
	static_assert(offsetof(menuDef_t, yTransition) == 0x170);
	static_assert(offsetof(menuDef_t, expressionData) == 0x18C);

	inline menuDef_t Convert(const Game::menuDef_t& from)
	{
		menuDef_t to{};
		to.window = Convert(from.window);
		to.fullScreen = static_cast<std::int32_t>(from.fullScreen);
		to.itemCount = static_cast<std::int32_t>(from.itemCount);
		to.fontIndex = static_cast<std::int32_t>(from.fontIndex);
		std::memcpy(to.cursorItem, from.cursorItem, sizeof(to.cursorItem));
		to.fadeCycle = static_cast<std::int32_t>(from.fadeCycle);
		to.fadeClamp = static_cast<float>(from.fadeClamp);
		to.fadeAmount = static_cast<float>(from.fadeAmount);
		to.fadeInAmount = static_cast<float>(from.fadeInAmount);
		to.blurRadius = static_cast<float>(from.blurRadius);
		to.imageTrack = static_cast<std::int32_t>(from.imageTrack);
		std::memcpy(to.focusColor, from.focusColor, sizeof(to.focusColor));
		for (std::size_t i = 0; i < std::size(to.scaleTransition); ++i)
		{
			to.scaleTransition[i] = Convert(from.scaleTransition[i]);
		}
		for (std::size_t i = 0; i < std::size(to.alphaTransition); ++i)
		{
			to.alphaTransition[i] = Convert(from.alphaTransition[i]);
		}
		for (std::size_t i = 0; i < std::size(to.xTransition); ++i)
		{
			to.xTransition[i] = Convert(from.xTransition[i]);
		}
		for (std::size_t i = 0; i < std::size(to.yTransition); ++i)
		{
			to.yTransition[i] = Convert(from.yTransition[i]);
		}
		return to;
	}

	inline Game::menuDef_t Convert(const menuDef_t& from)
	{
		Game::menuDef_t to{};
		to.window = Convert(from.window);
		to.fullScreen = static_cast<decltype(to.fullScreen)>(from.fullScreen);
		to.itemCount = static_cast<decltype(to.itemCount)>(from.itemCount);
		to.fontIndex = static_cast<decltype(to.fontIndex)>(from.fontIndex);
		std::memcpy(to.cursorItem, from.cursorItem, sizeof(from.cursorItem));
		to.fadeCycle = static_cast<decltype(to.fadeCycle)>(from.fadeCycle);
		to.fadeClamp = static_cast<decltype(to.fadeClamp)>(from.fadeClamp);
		to.fadeAmount = static_cast<decltype(to.fadeAmount)>(from.fadeAmount);
		to.fadeInAmount = static_cast<decltype(to.fadeInAmount)>(from.fadeInAmount);
		to.blurRadius = static_cast<decltype(to.blurRadius)>(from.blurRadius);
		to.imageTrack = static_cast<decltype(to.imageTrack)>(from.imageTrack);
		std::memcpy(to.focusColor, from.focusColor, sizeof(from.focusColor));
		for (std::size_t i = 0; i < std::size(to.scaleTransition); ++i)
		{
			to.scaleTransition[i] = Convert(from.scaleTransition[i]);
		}
		for (std::size_t i = 0; i < std::size(to.alphaTransition); ++i)
		{
			to.alphaTransition[i] = Convert(from.alphaTransition[i]);
		}
		for (std::size_t i = 0; i < std::size(to.xTransition); ++i)
		{
			to.xTransition[i] = Convert(from.xTransition[i]);
		}
		for (std::size_t i = 0; i < std::size(to.yTransition); ++i)
		{
			to.yTransition[i] = Convert(from.yTransition[i]);
		}
		return to;
	}

	struct MenuEventHandlerSet
	{
		std::int32_t eventHandlerCount;
		std::uint32_t eventHandlers;
	};

	static_assert(sizeof(MenuEventHandlerSet) == 0x8);
	static_assert(offsetof(MenuEventHandlerSet, eventHandlerCount) == 0x0);
	static_assert(offsetof(MenuEventHandlerSet, eventHandlers) == 0x4);

	inline MenuEventHandlerSet Convert(const Game::MenuEventHandlerSet& from)
	{
		MenuEventHandlerSet to{};
		to.eventHandlerCount = static_cast<std::int32_t>(from.eventHandlerCount);
		return to;
	}

	inline Game::MenuEventHandlerSet Convert(const MenuEventHandlerSet& from)
	{
		Game::MenuEventHandlerSet to{};
		to.eventHandlerCount = static_cast<decltype(to.eventHandlerCount)>(from.eventHandlerCount);
		return to;
	}

	union EventData
	{
		std::uint32_t unconditionalScript;
		std::uint32_t conditionalScript;
		std::uint32_t elseScript;
		std::uint32_t setLocalVarData;
	};

	static_assert(sizeof(EventData) == 0x4);
	static_assert(offsetof(EventData, unconditionalScript) == 0x0);
	static_assert(offsetof(EventData, conditionalScript) == 0x0);
	static_assert(offsetof(EventData, elseScript) == 0x0);
	static_assert(offsetof(EventData, setLocalVarData) == 0x0);

	inline EventData Convert(const Game::EventData& from)
	{
		EventData to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::EventData Convert(const EventData& from)
	{
		Game::EventData to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct MenuEventHandler
	{
		EventData eventData;
		std::int8_t eventType;
	};

	static_assert(sizeof(MenuEventHandler) == 0x8);
	static_assert(offsetof(MenuEventHandler, eventData) == 0x0);
	static_assert(offsetof(MenuEventHandler, eventType) == 0x4);

	inline MenuEventHandler Convert(const Game::MenuEventHandler& from)
	{
		MenuEventHandler to{};
		to.eventData = Convert(from.eventData);
		to.eventType = static_cast<std::int8_t>(from.eventType);
		return to;
	}

	inline Game::MenuEventHandler Convert(const MenuEventHandler& from)
	{
		Game::MenuEventHandler to{};
		to.eventData = Convert(from.eventData);
		to.eventType = static_cast<decltype(to.eventType)>(from.eventType);
		return to;
	}

	struct ConditionalScript
	{
		std::uint32_t eventHandlerSet;
		std::uint32_t eventExpression;
	};

	static_assert(sizeof(ConditionalScript) == 0x8);
	static_assert(offsetof(ConditionalScript, eventHandlerSet) == 0x0);
	static_assert(offsetof(ConditionalScript, eventExpression) == 0x4);

	inline ConditionalScript Convert(const Game::ConditionalScript&)
	{
		ConditionalScript to{};
		return to;
	}

	inline Game::ConditionalScript Convert(const ConditionalScript&)
	{
		Game::ConditionalScript to{};
		return to;
	}

	struct ExpressionString
	{
		std::uint32_t string;
	};

	static_assert(sizeof(ExpressionString) == 0x4);
	static_assert(offsetof(ExpressionString, string) == 0x0);

	inline ExpressionString Convert(const Game::ExpressionString&)
	{
		ExpressionString to{};
		return to;
	}

	inline Game::ExpressionString Convert(const ExpressionString&)
	{
		Game::ExpressionString to{};
		return to;
	}

	union operandInternalDataUnion
	{
		std::int32_t intVal;
		float floatVal;
		ExpressionString stringVal;
		std::uint32_t function;
	};

	static_assert(sizeof(operandInternalDataUnion) == 0x4);
	static_assert(offsetof(operandInternalDataUnion, intVal) == 0x0);
	static_assert(offsetof(operandInternalDataUnion, floatVal) == 0x0);
	static_assert(offsetof(operandInternalDataUnion, stringVal) == 0x0);
	static_assert(offsetof(operandInternalDataUnion, function) == 0x0);

	inline operandInternalDataUnion Convert(const Game::operandInternalDataUnion& from)
	{
		operandInternalDataUnion to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::operandInternalDataUnion Convert(const operandInternalDataUnion& from)
	{
		Game::operandInternalDataUnion to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct Operand
	{
		std::int32_t dataType;
		operandInternalDataUnion internals;
	};

	static_assert(sizeof(Operand) == 0x8);
	static_assert(offsetof(Operand, dataType) == 0x0);
	static_assert(offsetof(Operand, internals) == 0x4);

	inline Operand Convert(const Game::Operand& from)
	{
		Operand to{};
		to.dataType = static_cast<std::int32_t>(from.dataType);
		to.internals = Convert(from.internals);
		return to;
	}

	inline Game::Operand Convert(const Operand& from)
	{
		Game::Operand to{};
		to.dataType = static_cast<decltype(to.dataType)>(from.dataType);
		to.internals = Convert(from.internals);
		return to;
	}

	struct Statement_s
	{
		std::int32_t numEntries;
		std::uint32_t entries;
		std::uint32_t supportingData;
		std::int32_t lastExecuteTime;
		Operand lastResult;
	};

	static_assert(sizeof(Statement_s) == 0x18);
	static_assert(offsetof(Statement_s, numEntries) == 0x0);
	static_assert(offsetof(Statement_s, entries) == 0x4);
	static_assert(offsetof(Statement_s, supportingData) == 0x8);
	static_assert(offsetof(Statement_s, lastExecuteTime) == 0xC);
	static_assert(offsetof(Statement_s, lastResult) == 0x10);

	inline Statement_s Convert(const Game::Statement_s& from)
	{
		Statement_s to{};
		to.numEntries = static_cast<std::int32_t>(from.numEntries);
		to.lastExecuteTime = static_cast<std::int32_t>(from.lastExecuteTime);
		to.lastResult = Convert(from.lastResult);
		return to;
	}

	inline Game::Statement_s Convert(const Statement_s& from)
	{
		Game::Statement_s to{};
		to.numEntries = static_cast<decltype(to.numEntries)>(from.numEntries);
		to.lastExecuteTime = static_cast<decltype(to.lastExecuteTime)>(from.lastExecuteTime);
		to.lastResult = Convert(from.lastResult);
		return to;
	}

	union entryInternalData
	{
		std::int32_t op;
		Operand operand;
	};

	static_assert(sizeof(entryInternalData) == 0x8);
	static_assert(offsetof(entryInternalData, op) == 0x0);
	static_assert(offsetof(entryInternalData, operand) == 0x0);

	inline entryInternalData Convert(const Game::entryInternalData& from)
	{
		entryInternalData to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::entryInternalData Convert(const entryInternalData& from)
	{
		Game::entryInternalData to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct expressionEntry
	{
		std::int32_t type;
		entryInternalData data;
	};

	static_assert(sizeof(expressionEntry) == 0xC);
	static_assert(offsetof(expressionEntry, type) == 0x0);
	static_assert(offsetof(expressionEntry, data) == 0x4);

	inline expressionEntry Convert(const Game::expressionEntry& from)
	{
		expressionEntry to{};
		to.type = static_cast<std::int32_t>(from.type);
		to.data = Convert(from.data);
		return to;
	}

	inline Game::expressionEntry Convert(const expressionEntry& from)
	{
		Game::expressionEntry to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.data = Convert(from.data);
		return to;
	}

	struct UIFunctionList
	{
		std::int32_t totalFunctions;
		std::uint32_t functions;
	};

	static_assert(sizeof(UIFunctionList) == 0x8);
	static_assert(offsetof(UIFunctionList, totalFunctions) == 0x0);
	static_assert(offsetof(UIFunctionList, functions) == 0x4);

	inline UIFunctionList Convert(const Game::UIFunctionList& from)
	{
		UIFunctionList to{};
		to.totalFunctions = static_cast<std::int32_t>(from.totalFunctions);
		return to;
	}

	inline Game::UIFunctionList Convert(const UIFunctionList& from)
	{
		Game::UIFunctionList to{};
		to.totalFunctions = static_cast<decltype(to.totalFunctions)>(from.totalFunctions);
		return to;
	}

	struct StaticDvarList
	{
		std::int32_t numStaticDvars;
		std::uint32_t staticDvars;
	};

	static_assert(sizeof(StaticDvarList) == 0x8);
	static_assert(offsetof(StaticDvarList, numStaticDvars) == 0x0);
	static_assert(offsetof(StaticDvarList, staticDvars) == 0x4);

	inline StaticDvarList Convert(const Game::StaticDvarList& from)
	{
		StaticDvarList to{};
		to.numStaticDvars = static_cast<std::int32_t>(from.numStaticDvars);
		return to;
	}

	inline Game::StaticDvarList Convert(const StaticDvarList& from)
	{
		Game::StaticDvarList to{};
		to.numStaticDvars = static_cast<decltype(to.numStaticDvars)>(from.numStaticDvars);
		return to;
	}

	struct StringList
	{
		std::int32_t totalStrings;
		std::uint32_t strings;
	};

	static_assert(sizeof(StringList) == 0x8);
	static_assert(offsetof(StringList, totalStrings) == 0x0);
	static_assert(offsetof(StringList, strings) == 0x4);

	inline StringList Convert(const Game::StringList& from)
	{
		StringList to{};
		to.totalStrings = static_cast<std::int32_t>(from.totalStrings);
		return to;
	}

	inline Game::StringList Convert(const StringList& from)
	{
		Game::StringList to{};
		to.totalStrings = static_cast<decltype(to.totalStrings)>(from.totalStrings);
		return to;
	}

	struct ExpressionSupportingData
	{
		UIFunctionList uifunctions;
		StaticDvarList staticDvarList;
		StringList uiStrings;
	};

	static_assert(sizeof(ExpressionSupportingData) == 0x18);
	static_assert(offsetof(ExpressionSupportingData, uifunctions) == 0x0);
	static_assert(offsetof(ExpressionSupportingData, staticDvarList) == 0x8);
	static_assert(offsetof(ExpressionSupportingData, uiStrings) == 0x10);

	inline ExpressionSupportingData Convert(const Game::ExpressionSupportingData& from)
	{
		ExpressionSupportingData to{};
		to.uifunctions = Convert(from.uifunctions);
		to.staticDvarList = Convert(from.staticDvarList);
		to.uiStrings = Convert(from.uiStrings);
		return to;
	}

	inline Game::ExpressionSupportingData Convert(const ExpressionSupportingData& from)
	{
		Game::ExpressionSupportingData to{};
		to.uifunctions = Convert(from.uifunctions);
		to.staticDvarList = Convert(from.staticDvarList);
		to.uiStrings = Convert(from.uiStrings);
		return to;
	}

	struct StaticDvar
	{
		std::uint32_t dvar;
		std::uint32_t dvarName;
	};

	static_assert(sizeof(StaticDvar) == 0x8);
	static_assert(offsetof(StaticDvar, dvar) == 0x0);
	static_assert(offsetof(StaticDvar, dvarName) == 0x4);

	inline StaticDvar Convert(const Game::StaticDvar&)
	{
		StaticDvar to{};
		return to;
	}

	inline Game::StaticDvar Convert(const StaticDvar&)
	{
		Game::StaticDvar to{};
		return to;
	}

	struct SetLocalVarData
	{
		std::uint32_t localVarName;
		std::uint32_t expression;
	};

	static_assert(sizeof(SetLocalVarData) == 0x8);
	static_assert(offsetof(SetLocalVarData, localVarName) == 0x0);
	static_assert(offsetof(SetLocalVarData, expression) == 0x4);

	inline SetLocalVarData Convert(const Game::SetLocalVarData&)
	{
		SetLocalVarData to{};
		return to;
	}

	inline Game::SetLocalVarData Convert(const SetLocalVarData&)
	{
		Game::SetLocalVarData to{};
		return to;
	}

	struct ItemKeyHandler
	{
		std::int32_t key;
		std::uint32_t action;
		std::uint32_t next;
	};

	static_assert(sizeof(ItemKeyHandler) == 0xC);
	static_assert(offsetof(ItemKeyHandler, key) == 0x0);
	static_assert(offsetof(ItemKeyHandler, action) == 0x4);
	static_assert(offsetof(ItemKeyHandler, next) == 0x8);

	inline ItemKeyHandler Convert(const Game::ItemKeyHandler& from)
	{
		ItemKeyHandler to{};
		to.key = static_cast<std::int32_t>(from.key);
		return to;
	}

	inline Game::ItemKeyHandler Convert(const ItemKeyHandler& from)
	{
		Game::ItemKeyHandler to{};
		to.key = static_cast<decltype(to.key)>(from.key);
		return to;
	}

	union itemDefData_t
	{
		std::uint32_t listBox;
		std::uint32_t editField;
		std::uint32_t multi;
		std::uint32_t enumDvarName;
		std::uint32_t ticker;
		std::uint32_t scroll;
		std::uint32_t data;
	};

	static_assert(sizeof(itemDefData_t) == 0x4);
	static_assert(offsetof(itemDefData_t, listBox) == 0x0);
	static_assert(offsetof(itemDefData_t, editField) == 0x0);
	static_assert(offsetof(itemDefData_t, multi) == 0x0);
	static_assert(offsetof(itemDefData_t, enumDvarName) == 0x0);
	static_assert(offsetof(itemDefData_t, ticker) == 0x0);
	static_assert(offsetof(itemDefData_t, scroll) == 0x0);
	static_assert(offsetof(itemDefData_t, data) == 0x0);

	inline itemDefData_t Convert(const Game::itemDefData_t& from)
	{
		itemDefData_t to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::itemDefData_t Convert(const itemDefData_t& from)
	{
		Game::itemDefData_t to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct itemDef_s
	{
		windowDef_t window;
		rectDef_s textRect[1];
		std::int32_t type;
		std::int32_t dataType;
		std::int32_t alignment;
		std::int32_t fontEnum;
		std::int32_t textAlignMode;
		float textalignx;
		float textaligny;
		float textscale;
		std::int32_t textStyle;
		std::int32_t gameMsgWindowIndex;
		std::int32_t gameMsgWindowMode;
		std::uint32_t text;
		std::int32_t itemFlags;
		std::uint32_t parent;
		std::uint32_t mouseEnterText;
		std::uint32_t mouseExitText;
		std::uint32_t mouseEnter;
		std::uint32_t mouseExit;
		std::uint32_t action;
		std::uint32_t accept;
		std::uint32_t onFocus;
		std::uint32_t leaveFocus;
		std::uint32_t dvar;
		std::uint32_t dvarTest;
		std::uint32_t onKey;
		std::uint32_t enableDvar;
		std::uint32_t localVar;
		std::int32_t dvarFlags;
		std::uint32_t focusSound;
		float special;
		std::int32_t cursorPos[1];
		itemDefData_t typeData;
		std::int32_t imageTrack;
		std::int32_t floatExpressionCount;
		std::uint32_t floatExpressions;
		std::uint32_t visibleExp;
		std::uint32_t disabledExp;
		std::uint32_t textExp;
		std::uint32_t materialExp;
		float glowColor[4];
		bool decayActive;
		std::int32_t fxBirthTime;
		std::int32_t fxLetterTime;
		std::int32_t fxDecayStartTime;
		std::int32_t fxDecayDuration;
		std::int32_t lastSoundPlayedTime;
	};

	static_assert(sizeof(itemDef_s) == 0x17C);
	static_assert(offsetof(itemDef_s, window) == 0x0);
	static_assert(offsetof(itemDef_s, textRect) == 0xA4);
	static_assert(offsetof(itemDef_s, type) == 0xB8);
	static_assert(offsetof(itemDef_s, dataType) == 0xBC);
	static_assert(offsetof(itemDef_s, alignment) == 0xC0);
	static_assert(offsetof(itemDef_s, fontEnum) == 0xC4);
	static_assert(offsetof(itemDef_s, textAlignMode) == 0xC8);
	static_assert(offsetof(itemDef_s, textalignx) == 0xCC);
	static_assert(offsetof(itemDef_s, textaligny) == 0xD0);
	static_assert(offsetof(itemDef_s, textscale) == 0xD4);
	static_assert(offsetof(itemDef_s, textStyle) == 0xD8);
	static_assert(offsetof(itemDef_s, gameMsgWindowIndex) == 0xDC);
	static_assert(offsetof(itemDef_s, gameMsgWindowMode) == 0xE0);
	static_assert(offsetof(itemDef_s, text) == 0xE4);
	static_assert(offsetof(itemDef_s, itemFlags) == 0xE8);
	static_assert(offsetof(itemDef_s, parent) == 0xEC);
	static_assert(offsetof(itemDef_s, mouseEnterText) == 0xF0);
	static_assert(offsetof(itemDef_s, mouseExitText) == 0xF4);
	static_assert(offsetof(itemDef_s, mouseEnter) == 0xF8);
	static_assert(offsetof(itemDef_s, mouseExit) == 0xFC);
	static_assert(offsetof(itemDef_s, action) == 0x100);
	static_assert(offsetof(itemDef_s, accept) == 0x104);
	static_assert(offsetof(itemDef_s, onFocus) == 0x108);
	static_assert(offsetof(itemDef_s, leaveFocus) == 0x10C);
	static_assert(offsetof(itemDef_s, dvar) == 0x110);
	static_assert(offsetof(itemDef_s, dvarTest) == 0x114);
	static_assert(offsetof(itemDef_s, onKey) == 0x118);
	static_assert(offsetof(itemDef_s, enableDvar) == 0x11C);
	static_assert(offsetof(itemDef_s, localVar) == 0x120);
	static_assert(offsetof(itemDef_s, dvarFlags) == 0x124);
	static_assert(offsetof(itemDef_s, focusSound) == 0x128);
	static_assert(offsetof(itemDef_s, special) == 0x12C);
	static_assert(offsetof(itemDef_s, cursorPos) == 0x130);
	static_assert(offsetof(itemDef_s, typeData) == 0x134);
	static_assert(offsetof(itemDef_s, imageTrack) == 0x138);
	static_assert(offsetof(itemDef_s, floatExpressionCount) == 0x13C);
	static_assert(offsetof(itemDef_s, floatExpressions) == 0x140);
	static_assert(offsetof(itemDef_s, visibleExp) == 0x144);
	static_assert(offsetof(itemDef_s, disabledExp) == 0x148);
	static_assert(offsetof(itemDef_s, textExp) == 0x14C);
	static_assert(offsetof(itemDef_s, materialExp) == 0x150);
	static_assert(offsetof(itemDef_s, glowColor) == 0x154);
	static_assert(offsetof(itemDef_s, decayActive) == 0x164);
	static_assert(offsetof(itemDef_s, fxBirthTime) == 0x168);
	static_assert(offsetof(itemDef_s, fxLetterTime) == 0x16C);
	static_assert(offsetof(itemDef_s, fxDecayStartTime) == 0x170);
	static_assert(offsetof(itemDef_s, fxDecayDuration) == 0x174);
	static_assert(offsetof(itemDef_s, lastSoundPlayedTime) == 0x178);

	inline itemDef_s Convert(const Game::itemDef_s& from)
	{
		itemDef_s to{};
		to.window = Convert(from.window);
		for (std::size_t i = 0; i < std::size(to.textRect); ++i)
		{
			to.textRect[i] = Convert(from.textRect[i]);
		}
		to.type = static_cast<std::int32_t>(from.type);
		to.dataType = static_cast<std::int32_t>(from.dataType);
		to.alignment = static_cast<std::int32_t>(from.alignment);
		to.fontEnum = static_cast<std::int32_t>(from.fontEnum);
		to.textAlignMode = static_cast<std::int32_t>(from.textAlignMode);
		to.textalignx = static_cast<float>(from.textalignx);
		to.textaligny = static_cast<float>(from.textaligny);
		to.textscale = static_cast<float>(from.textscale);
		to.textStyle = static_cast<std::int32_t>(from.textStyle);
		to.gameMsgWindowIndex = static_cast<std::int32_t>(from.gameMsgWindowIndex);
		to.gameMsgWindowMode = static_cast<std::int32_t>(from.gameMsgWindowMode);
		to.itemFlags = static_cast<std::int32_t>(from.itemFlags);
		to.dvarFlags = static_cast<std::int32_t>(from.dvarFlags);
		to.special = static_cast<float>(from.special);
		std::memcpy(to.cursorPos, from.cursorPos, sizeof(to.cursorPos));
		to.typeData = Convert(from.typeData);
		to.imageTrack = static_cast<std::int32_t>(from.imageTrack);
		to.floatExpressionCount = static_cast<std::int32_t>(from.floatExpressionCount);
		std::memcpy(to.glowColor, from.glowColor, sizeof(to.glowColor));
		to.decayActive = static_cast<bool>(from.decayActive);
		to.fxBirthTime = static_cast<std::int32_t>(from.fxBirthTime);
		to.fxLetterTime = static_cast<std::int32_t>(from.fxLetterTime);
		to.fxDecayStartTime = static_cast<std::int32_t>(from.fxDecayStartTime);
		to.fxDecayDuration = static_cast<std::int32_t>(from.fxDecayDuration);
		to.lastSoundPlayedTime = static_cast<std::int32_t>(from.lastSoundPlayedTime);
		return to;
	}

	inline Game::itemDef_s Convert(const itemDef_s& from)
	{
		Game::itemDef_s to{};
		to.window = Convert(from.window);
		for (std::size_t i = 0; i < std::size(to.textRect); ++i)
		{
			to.textRect[i] = Convert(from.textRect[i]);
		}
		to.type = static_cast<decltype(to.type)>(from.type);
		to.dataType = static_cast<decltype(to.dataType)>(from.dataType);
		to.alignment = static_cast<decltype(to.alignment)>(from.alignment);
		to.fontEnum = static_cast<decltype(to.fontEnum)>(from.fontEnum);
		to.textAlignMode = static_cast<decltype(to.textAlignMode)>(from.textAlignMode);
		to.textalignx = static_cast<decltype(to.textalignx)>(from.textalignx);
		to.textaligny = static_cast<decltype(to.textaligny)>(from.textaligny);
		to.textscale = static_cast<decltype(to.textscale)>(from.textscale);
		to.textStyle = static_cast<decltype(to.textStyle)>(from.textStyle);
		to.gameMsgWindowIndex = static_cast<decltype(to.gameMsgWindowIndex)>(from.gameMsgWindowIndex);
		to.gameMsgWindowMode = static_cast<decltype(to.gameMsgWindowMode)>(from.gameMsgWindowMode);
		to.itemFlags = static_cast<decltype(to.itemFlags)>(from.itemFlags);
		to.dvarFlags = static_cast<decltype(to.dvarFlags)>(from.dvarFlags);
		to.special = static_cast<decltype(to.special)>(from.special);
		std::memcpy(to.cursorPos, from.cursorPos, sizeof(from.cursorPos));
		to.typeData = Convert(from.typeData);
		to.imageTrack = static_cast<decltype(to.imageTrack)>(from.imageTrack);
		to.floatExpressionCount = static_cast<decltype(to.floatExpressionCount)>(from.floatExpressionCount);
		std::memcpy(to.glowColor, from.glowColor, sizeof(from.glowColor));
		to.decayActive = static_cast<decltype(to.decayActive)>(from.decayActive);
		to.fxBirthTime = static_cast<decltype(to.fxBirthTime)>(from.fxBirthTime);
		to.fxLetterTime = static_cast<decltype(to.fxLetterTime)>(from.fxLetterTime);
		to.fxDecayStartTime = static_cast<decltype(to.fxDecayStartTime)>(from.fxDecayStartTime);
		to.fxDecayDuration = static_cast<decltype(to.fxDecayDuration)>(from.fxDecayDuration);
		to.lastSoundPlayedTime = static_cast<decltype(to.lastSoundPlayedTime)>(from.lastSoundPlayedTime);
		return to;
	}

	struct columnInfo_s
	{
		std::int32_t pos;
		std::int32_t width;
		std::int32_t maxChars;
		std::int32_t alignment;
	};

	static_assert(sizeof(columnInfo_s) == 0x10);
	static_assert(offsetof(columnInfo_s, pos) == 0x0);
	static_assert(offsetof(columnInfo_s, width) == 0x4);
	static_assert(offsetof(columnInfo_s, maxChars) == 0x8);
	static_assert(offsetof(columnInfo_s, alignment) == 0xC);

	inline columnInfo_s Convert(const Game::columnInfo_s& from)
	{
		columnInfo_s to{};
		to.pos = static_cast<std::int32_t>(from.pos);
		to.width = static_cast<std::int32_t>(from.width);
		to.maxChars = static_cast<std::int32_t>(from.maxChars);
		to.alignment = static_cast<std::int32_t>(from.alignment);
		return to;
	}

	inline Game::columnInfo_s Convert(const columnInfo_s& from)
	{
		Game::columnInfo_s to{};
		to.pos = static_cast<decltype(to.pos)>(from.pos);
		to.width = static_cast<decltype(to.width)>(from.width);
		to.maxChars = static_cast<decltype(to.maxChars)>(from.maxChars);
		to.alignment = static_cast<decltype(to.alignment)>(from.alignment);
		return to;
	}

	struct listBoxDef_s
	{
		std::int32_t mousePos;
		std::int32_t startPos[1];
		std::int32_t endPos[1];
		std::int32_t drawPadding;
		float elementWidth;
		float elementHeight;
		std::int32_t elementStyle;
		std::int32_t numColumns;
		columnInfo_s columnInfo[16];
		std::uint32_t onDoubleClick;
		std::int32_t notselectable;
		std::int32_t noScrollBars;
		std::int32_t usePaging;
		float selectBorder[4];
		std::uint32_t selectIcon;
	};

	static_assert(sizeof(listBoxDef_s) == 0x144);
	static_assert(offsetof(listBoxDef_s, mousePos) == 0x0);
	static_assert(offsetof(listBoxDef_s, startPos) == 0x4);
	static_assert(offsetof(listBoxDef_s, endPos) == 0x8);
	static_assert(offsetof(listBoxDef_s, drawPadding) == 0xC);
	static_assert(offsetof(listBoxDef_s, elementWidth) == 0x10);
	static_assert(offsetof(listBoxDef_s, elementHeight) == 0x14);
	static_assert(offsetof(listBoxDef_s, elementStyle) == 0x18);
	static_assert(offsetof(listBoxDef_s, numColumns) == 0x1C);
	static_assert(offsetof(listBoxDef_s, columnInfo) == 0x20);
	static_assert(offsetof(listBoxDef_s, onDoubleClick) == 0x120);
	static_assert(offsetof(listBoxDef_s, notselectable) == 0x124);
	static_assert(offsetof(listBoxDef_s, noScrollBars) == 0x128);
	static_assert(offsetof(listBoxDef_s, usePaging) == 0x12C);
	static_assert(offsetof(listBoxDef_s, selectBorder) == 0x130);
	static_assert(offsetof(listBoxDef_s, selectIcon) == 0x140);

	inline listBoxDef_s Convert(const Game::listBoxDef_s& from)
	{
		listBoxDef_s to{};
		to.mousePos = static_cast<std::int32_t>(from.mousePos);
		std::memcpy(to.startPos, from.startPos, sizeof(to.startPos));
		std::memcpy(to.endPos, from.endPos, sizeof(to.endPos));
		to.drawPadding = static_cast<std::int32_t>(from.drawPadding);
		to.elementWidth = static_cast<float>(from.elementWidth);
		to.elementHeight = static_cast<float>(from.elementHeight);
		to.elementStyle = static_cast<std::int32_t>(from.elementStyle);
		to.numColumns = static_cast<std::int32_t>(from.numColumns);
		for (std::size_t i = 0; i < std::size(to.columnInfo); ++i)
		{
			to.columnInfo[i] = Convert(from.columnInfo[i]);
		}
		to.notselectable = static_cast<std::int32_t>(from.notselectable);
		to.noScrollBars = static_cast<std::int32_t>(from.noScrollBars);
		to.usePaging = static_cast<std::int32_t>(from.usePaging);
		std::memcpy(to.selectBorder, from.selectBorder, sizeof(to.selectBorder));
		return to;
	}

	inline Game::listBoxDef_s Convert(const listBoxDef_s& from)
	{
		Game::listBoxDef_s to{};
		to.mousePos = static_cast<decltype(to.mousePos)>(from.mousePos);
		std::memcpy(to.startPos, from.startPos, sizeof(from.startPos));
		std::memcpy(to.endPos, from.endPos, sizeof(from.endPos));
		to.drawPadding = static_cast<decltype(to.drawPadding)>(from.drawPadding);
		to.elementWidth = static_cast<decltype(to.elementWidth)>(from.elementWidth);
		to.elementHeight = static_cast<decltype(to.elementHeight)>(from.elementHeight);
		to.elementStyle = static_cast<decltype(to.elementStyle)>(from.elementStyle);
		to.numColumns = static_cast<decltype(to.numColumns)>(from.numColumns);
		for (std::size_t i = 0; i < std::size(to.columnInfo); ++i)
		{
			to.columnInfo[i] = Convert(from.columnInfo[i]);
		}
		to.notselectable = static_cast<decltype(to.notselectable)>(from.notselectable);
		to.noScrollBars = static_cast<decltype(to.noScrollBars)>(from.noScrollBars);
		to.usePaging = static_cast<decltype(to.usePaging)>(from.usePaging);
		std::memcpy(to.selectBorder, from.selectBorder, sizeof(from.selectBorder));
		return to;
	}

	struct editFieldDef_s
	{
		float minVal;
		float maxVal;
		float defVal;
		float range;
		std::int32_t maxChars;
		std::int32_t maxCharsGotoNext;
		std::int32_t maxPaintChars;
		std::int32_t paintOffset;
	};

	static_assert(sizeof(editFieldDef_s) == 0x20);
	static_assert(offsetof(editFieldDef_s, minVal) == 0x0);
	static_assert(offsetof(editFieldDef_s, maxVal) == 0x4);
	static_assert(offsetof(editFieldDef_s, defVal) == 0x8);
	static_assert(offsetof(editFieldDef_s, range) == 0xC);
	static_assert(offsetof(editFieldDef_s, maxChars) == 0x10);
	static_assert(offsetof(editFieldDef_s, maxCharsGotoNext) == 0x14);
	static_assert(offsetof(editFieldDef_s, maxPaintChars) == 0x18);
	static_assert(offsetof(editFieldDef_s, paintOffset) == 0x1C);

	inline editFieldDef_s Convert(const Game::editFieldDef_s& from)
	{
		editFieldDef_s to{};
		to.minVal = static_cast<float>(from.minVal);
		to.maxVal = static_cast<float>(from.maxVal);
		to.defVal = static_cast<float>(from.defVal);
		to.range = static_cast<float>(from.range);
		to.maxChars = static_cast<std::int32_t>(from.maxChars);
		to.maxCharsGotoNext = static_cast<std::int32_t>(from.maxCharsGotoNext);
		to.maxPaintChars = static_cast<std::int32_t>(from.maxPaintChars);
		to.paintOffset = static_cast<std::int32_t>(from.paintOffset);
		return to;
	}

	inline Game::editFieldDef_s Convert(const editFieldDef_s& from)
	{
		Game::editFieldDef_s to{};
		to.minVal = static_cast<decltype(to.minVal)>(from.minVal);
		to.maxVal = static_cast<decltype(to.maxVal)>(from.maxVal);
		to.defVal = static_cast<decltype(to.defVal)>(from.defVal);
		to.range = static_cast<decltype(to.range)>(from.range);
		to.maxChars = static_cast<decltype(to.maxChars)>(from.maxChars);
		to.maxCharsGotoNext = static_cast<decltype(to.maxCharsGotoNext)>(from.maxCharsGotoNext);
		to.maxPaintChars = static_cast<decltype(to.maxPaintChars)>(from.maxPaintChars);
		to.paintOffset = static_cast<decltype(to.paintOffset)>(from.paintOffset);
		return to;
	}

	struct multiDef_s
	{
		std::uint32_t dvarList[32];
		std::uint32_t dvarStr[32];
		float dvarValue[32];
		std::int32_t count;
		std::int32_t strDef;
	};

	static_assert(sizeof(multiDef_s) == 0x188);
	static_assert(offsetof(multiDef_s, dvarList) == 0x0);
	static_assert(offsetof(multiDef_s, dvarStr) == 0x80);
	static_assert(offsetof(multiDef_s, dvarValue) == 0x100);
	static_assert(offsetof(multiDef_s, count) == 0x180);
	static_assert(offsetof(multiDef_s, strDef) == 0x184);

	inline multiDef_s Convert(const Game::multiDef_s& from)
	{
		multiDef_s to{};
		std::memcpy(to.dvarValue, from.dvarValue, sizeof(to.dvarValue));
		to.count = static_cast<std::int32_t>(from.count);
		to.strDef = static_cast<std::int32_t>(from.strDef);
		return to;
	}

	inline Game::multiDef_s Convert(const multiDef_s& from)
	{
		Game::multiDef_s to{};
		std::memcpy(to.dvarValue, from.dvarValue, sizeof(from.dvarValue));
		to.count = static_cast<decltype(to.count)>(from.count);
		to.strDef = static_cast<decltype(to.strDef)>(from.strDef);
		return to;
	}

	struct newsTickerDef_s
	{
		std::int32_t feedId;
		std::int32_t speed;
		std::int32_t spacing;
		std::int32_t lastTime;
		std::int32_t start;
		std::int32_t end;
		float x;
	};

	static_assert(sizeof(newsTickerDef_s) == 0x1C);
	static_assert(offsetof(newsTickerDef_s, feedId) == 0x0);
	static_assert(offsetof(newsTickerDef_s, speed) == 0x4);
	static_assert(offsetof(newsTickerDef_s, spacing) == 0x8);
	static_assert(offsetof(newsTickerDef_s, lastTime) == 0xC);
	static_assert(offsetof(newsTickerDef_s, start) == 0x10);
	static_assert(offsetof(newsTickerDef_s, end) == 0x14);
	static_assert(offsetof(newsTickerDef_s, x) == 0x18);

	inline newsTickerDef_s Convert(const Game::newsTickerDef_s& from)
	{
		newsTickerDef_s to{};
		to.feedId = static_cast<std::int32_t>(from.feedId);
		to.speed = static_cast<std::int32_t>(from.speed);
		to.spacing = static_cast<std::int32_t>(from.spacing);
		to.lastTime = static_cast<std::int32_t>(from.lastTime);
		to.start = static_cast<std::int32_t>(from.start);
		to.end = static_cast<std::int32_t>(from.end);
		to.x = static_cast<float>(from.x);
		return to;
	}

	inline Game::newsTickerDef_s Convert(const newsTickerDef_s& from)
	{
		Game::newsTickerDef_s to{};
		to.feedId = static_cast<decltype(to.feedId)>(from.feedId);
		to.speed = static_cast<decltype(to.speed)>(from.speed);
		to.spacing = static_cast<decltype(to.spacing)>(from.spacing);
		to.lastTime = static_cast<decltype(to.lastTime)>(from.lastTime);
		to.start = static_cast<decltype(to.start)>(from.start);
		to.end = static_cast<decltype(to.end)>(from.end);
		to.x = static_cast<decltype(to.x)>(from.x);
		return to;
	}

	struct textScrollDef_s
	{
		std::int32_t startTime;
	};

	static_assert(sizeof(textScrollDef_s) == 0x4);
	static_assert(offsetof(textScrollDef_s, startTime) == 0x0);

	inline textScrollDef_s Convert(const Game::textScrollDef_s& from)
	{
		textScrollDef_s to{};
		to.startTime = static_cast<std::int32_t>(from.startTime);
		return to;
	}

	inline Game::textScrollDef_s Convert(const textScrollDef_s& from)
	{
		Game::textScrollDef_s to{};
		to.startTime = static_cast<decltype(to.startTime)>(from.startTime);
		return to;
	}

	struct ItemFloatExpression
	{
		std::int32_t target;
		std::uint32_t expression;
	};

	static_assert(sizeof(ItemFloatExpression) == 0x8);
	static_assert(offsetof(ItemFloatExpression, target) == 0x0);
	static_assert(offsetof(ItemFloatExpression, expression) == 0x4);

	inline ItemFloatExpression Convert(const Game::ItemFloatExpression& from)
	{
		ItemFloatExpression to{};
		to.target = static_cast<std::int32_t>(from.target);
		return to;
	}

	inline Game::ItemFloatExpression Convert(const ItemFloatExpression& from)
	{
		Game::ItemFloatExpression to{};
		to.target = static_cast<decltype(to.target)>(from.target);
		return to;
	}

	struct LocalizeEntry
	{
		std::uint32_t value;
		std::uint32_t name;
	};

	static_assert(sizeof(LocalizeEntry) == 0x8);
	static_assert(offsetof(LocalizeEntry, value) == 0x0);
	static_assert(offsetof(LocalizeEntry, name) == 0x4);

	inline LocalizeEntry Convert(const Game::LocalizeEntry&)
	{
		LocalizeEntry to{};
		return to;
	}

	inline Game::LocalizeEntry Convert(const LocalizeEntry&)
	{
		Game::LocalizeEntry to{};
		return to;
	}

	struct WeaponCompleteDef
	{
		std::uint32_t szInternalName;
		std::uint32_t weapDef;
		std::uint32_t szDisplayName;
		std::uint32_t hideTags;
		std::uint32_t szXAnims;
		float fAdsZoomFov;
		std::int32_t iAdsTransInTime;
		std::int32_t iAdsTransOutTime;
		std::int32_t iClipSize;
		std::int32_t impactType;
		std::int32_t iFireTime;
		std::int32_t dpadIconRatio;
		float penetrateMultiplier;
		float fAdsViewKickCenterSpeed;
		float fHipViewKickCenterSpeed;
		std::uint32_t szAltWeaponName;
		std::uint32_t altWeaponIndex;
		std::int32_t iAltRaiseTime;
		std::uint32_t killIcon;
		std::uint32_t dpadIcon;
		std::int32_t fireAnimLength;
		std::int32_t iFirstRaiseTime;
		std::int32_t ammoDropStockMax;
		float adsDofStart;
		float adsDofEnd;
		std::uint16_t accuracyGraphKnotCount[2];
		std::uint32_t accuracyGraphKnots[2];
		bool motionTracker;
		bool enhanced;
		bool dpadIconShowsAmmo;
	};

	static_assert(sizeof(WeaponCompleteDef) == 0x74);
	static_assert(offsetof(WeaponCompleteDef, szInternalName) == 0x0);
	static_assert(offsetof(WeaponCompleteDef, weapDef) == 0x4);
	static_assert(offsetof(WeaponCompleteDef, szDisplayName) == 0x8);
	static_assert(offsetof(WeaponCompleteDef, hideTags) == 0xC);
	static_assert(offsetof(WeaponCompleteDef, szXAnims) == 0x10);
	static_assert(offsetof(WeaponCompleteDef, fAdsZoomFov) == 0x14);
	static_assert(offsetof(WeaponCompleteDef, iAdsTransInTime) == 0x18);
	static_assert(offsetof(WeaponCompleteDef, iAdsTransOutTime) == 0x1C);
	static_assert(offsetof(WeaponCompleteDef, iClipSize) == 0x20);
	static_assert(offsetof(WeaponCompleteDef, impactType) == 0x24);
	static_assert(offsetof(WeaponCompleteDef, iFireTime) == 0x28);
	static_assert(offsetof(WeaponCompleteDef, dpadIconRatio) == 0x2C);
	static_assert(offsetof(WeaponCompleteDef, penetrateMultiplier) == 0x30);
	static_assert(offsetof(WeaponCompleteDef, fAdsViewKickCenterSpeed) == 0x34);
	static_assert(offsetof(WeaponCompleteDef, fHipViewKickCenterSpeed) == 0x38);
	static_assert(offsetof(WeaponCompleteDef, szAltWeaponName) == 0x3C);
	static_assert(offsetof(WeaponCompleteDef, altWeaponIndex) == 0x40);
	static_assert(offsetof(WeaponCompleteDef, iAltRaiseTime) == 0x44);
	static_assert(offsetof(WeaponCompleteDef, killIcon) == 0x48);
	static_assert(offsetof(WeaponCompleteDef, dpadIcon) == 0x4C);
	static_assert(offsetof(WeaponCompleteDef, fireAnimLength) == 0x50);
	static_assert(offsetof(WeaponCompleteDef, iFirstRaiseTime) == 0x54);
	static_assert(offsetof(WeaponCompleteDef, ammoDropStockMax) == 0x58);
	static_assert(offsetof(WeaponCompleteDef, adsDofStart) == 0x5C);
	static_assert(offsetof(WeaponCompleteDef, adsDofEnd) == 0x60);
	static_assert(offsetof(WeaponCompleteDef, accuracyGraphKnotCount) == 0x64);
	static_assert(offsetof(WeaponCompleteDef, accuracyGraphKnots) == 0x68);
	static_assert(offsetof(WeaponCompleteDef, motionTracker) == 0x70);
	static_assert(offsetof(WeaponCompleteDef, enhanced) == 0x71);
	static_assert(offsetof(WeaponCompleteDef, dpadIconShowsAmmo) == 0x72);

	inline WeaponCompleteDef Convert(const Game::WeaponCompleteDef& from)
	{
		WeaponCompleteDef to{};
		to.fAdsZoomFov = static_cast<float>(from.fAdsZoomFov);
		to.iAdsTransInTime = static_cast<std::int32_t>(from.iAdsTransInTime);
		to.iAdsTransOutTime = static_cast<std::int32_t>(from.iAdsTransOutTime);
		to.iClipSize = static_cast<std::int32_t>(from.iClipSize);
		to.impactType = static_cast<std::int32_t>(from.impactType);
		to.iFireTime = static_cast<std::int32_t>(from.iFireTime);
		to.dpadIconRatio = static_cast<std::int32_t>(from.dpadIconRatio);
		to.penetrateMultiplier = static_cast<float>(from.penetrateMultiplier);
		to.fAdsViewKickCenterSpeed = static_cast<float>(from.fAdsViewKickCenterSpeed);
		to.fHipViewKickCenterSpeed = static_cast<float>(from.fHipViewKickCenterSpeed);
		to.altWeaponIndex = static_cast<std::uint32_t>(from.altWeaponIndex);
		to.iAltRaiseTime = static_cast<std::int32_t>(from.iAltRaiseTime);
		to.fireAnimLength = static_cast<std::int32_t>(from.fireAnimLength);
		to.iFirstRaiseTime = static_cast<std::int32_t>(from.iFirstRaiseTime);
		to.ammoDropStockMax = static_cast<std::int32_t>(from.ammoDropStockMax);
		to.adsDofStart = static_cast<float>(from.adsDofStart);
		to.adsDofEnd = static_cast<float>(from.adsDofEnd);
		std::memcpy(to.accuracyGraphKnotCount, from.accuracyGraphKnotCount, sizeof(to.accuracyGraphKnotCount));
		to.motionTracker = static_cast<bool>(from.motionTracker);
		to.enhanced = static_cast<bool>(from.enhanced);
		to.dpadIconShowsAmmo = static_cast<bool>(from.dpadIconShowsAmmo);
		return to;
	}

	inline Game::WeaponCompleteDef Convert(const WeaponCompleteDef& from)
	{
		Game::WeaponCompleteDef to{};
		to.fAdsZoomFov = static_cast<decltype(to.fAdsZoomFov)>(from.fAdsZoomFov);
		to.iAdsTransInTime = static_cast<decltype(to.iAdsTransInTime)>(from.iAdsTransInTime);
		to.iAdsTransOutTime = static_cast<decltype(to.iAdsTransOutTime)>(from.iAdsTransOutTime);
		to.iClipSize = static_cast<decltype(to.iClipSize)>(from.iClipSize);
		to.impactType = static_cast<decltype(to.impactType)>(from.impactType);
		to.iFireTime = static_cast<decltype(to.iFireTime)>(from.iFireTime);
		to.dpadIconRatio = static_cast<decltype(to.dpadIconRatio)>(from.dpadIconRatio);
		to.penetrateMultiplier = static_cast<decltype(to.penetrateMultiplier)>(from.penetrateMultiplier);
		to.fAdsViewKickCenterSpeed = static_cast<decltype(to.fAdsViewKickCenterSpeed)>(from.fAdsViewKickCenterSpeed);
		to.fHipViewKickCenterSpeed = static_cast<decltype(to.fHipViewKickCenterSpeed)>(from.fHipViewKickCenterSpeed);
		to.altWeaponIndex = static_cast<decltype(to.altWeaponIndex)>(from.altWeaponIndex);
		to.iAltRaiseTime = static_cast<decltype(to.iAltRaiseTime)>(from.iAltRaiseTime);
		to.fireAnimLength = static_cast<decltype(to.fireAnimLength)>(from.fireAnimLength);
		to.iFirstRaiseTime = static_cast<decltype(to.iFirstRaiseTime)>(from.iFirstRaiseTime);
		to.ammoDropStockMax = static_cast<decltype(to.ammoDropStockMax)>(from.ammoDropStockMax);
		to.adsDofStart = static_cast<decltype(to.adsDofStart)>(from.adsDofStart);
		to.adsDofEnd = static_cast<decltype(to.adsDofEnd)>(from.adsDofEnd);
		std::memcpy(to.accuracyGraphKnotCount, from.accuracyGraphKnotCount, sizeof(from.accuracyGraphKnotCount));
		to.motionTracker = static_cast<decltype(to.motionTracker)>(from.motionTracker);
		to.enhanced = static_cast<decltype(to.enhanced)>(from.enhanced);
		to.dpadIconShowsAmmo = static_cast<decltype(to.dpadIconShowsAmmo)>(from.dpadIconShowsAmmo);
		return to;
	}

	struct WeaponDef
	{
		std::uint32_t szOverlayName;
		std::uint32_t gunXModel;
		std::uint32_t handXModel;
		std::uint32_t szXAnimsRightHanded;
		std::uint32_t szXAnimsLeftHanded;
		std::uint32_t szModeName;
		std::uint32_t notetrackSoundMapKeys;
		std::uint32_t notetrackSoundMapValues;
		std::uint32_t notetrackRumbleMapKeys;
		std::uint32_t notetrackRumbleMapValues;
		std::int32_t playerAnimType;
		std::int32_t weapType;
		std::int32_t weapClass;
		std::int32_t penetrateType;
		std::int32_t inventoryType;
		std::int32_t fireType;
		std::int32_t offhandClass;
		std::int32_t stance;
		std::uint32_t viewFlashEffect;
		std::uint32_t worldFlashEffect;
		std::uint32_t pickupSound;
		std::uint32_t pickupSoundPlayer;
		std::uint32_t ammoPickupSound;
		std::uint32_t ammoPickupSoundPlayer;
		std::uint32_t projectileSound;
		std::uint32_t pullbackSound;
		std::uint32_t pullbackSoundPlayer;
		std::uint32_t fireSound;
		std::uint32_t fireSoundPlayer;
		std::uint32_t fireSoundPlayerAkimbo;
		std::uint32_t fireLoopSound;
		std::uint32_t fireLoopSoundPlayer;
		std::uint32_t fireStopSound;
		std::uint32_t fireStopSoundPlayer;
		std::uint32_t fireLastSound;
		std::uint32_t fireLastSoundPlayer;
		std::uint32_t emptyFireSound;
		std::uint32_t emptyFireSoundPlayer;
		std::uint32_t meleeSwipeSound;
		std::uint32_t meleeSwipeSoundPlayer;
		std::uint32_t meleeHitSound;
		std::uint32_t meleeMissSound;
		std::uint32_t rechamberSound;
		std::uint32_t rechamberSoundPlayer;
		std::uint32_t reloadSound;
		std::uint32_t reloadSoundPlayer;
		std::uint32_t reloadEmptySound;
		std::uint32_t reloadEmptySoundPlayer;
		std::uint32_t reloadStartSound;
		std::uint32_t reloadStartSoundPlayer;
		std::uint32_t reloadEndSound;
		std::uint32_t reloadEndSoundPlayer;
		std::uint32_t detonateSound;
		std::uint32_t detonateSoundPlayer;
		std::uint32_t nightVisionWearSound;
		std::uint32_t nightVisionWearSoundPlayer;
		std::uint32_t nightVisionRemoveSound;
		std::uint32_t nightVisionRemoveSoundPlayer;
		std::uint32_t altSwitchSound;
		std::uint32_t altSwitchSoundPlayer;
		std::uint32_t raiseSound;
		std::uint32_t raiseSoundPlayer;
		std::uint32_t firstRaiseSound;
		std::uint32_t firstRaiseSoundPlayer;
		std::uint32_t putawaySound;
		std::uint32_t putawaySoundPlayer;
		std::uint32_t scanSound;
		std::uint32_t bounceSound;
		std::uint32_t viewShellEjectEffect;
		std::uint32_t worldShellEjectEffect;
		std::uint32_t viewLastShotEjectEffect;
		std::uint32_t worldLastShotEjectEffect;
		std::uint32_t reticleCenter;
		std::uint32_t reticleSide;
		std::int32_t iReticleCenterSize;
		std::int32_t iReticleSideSize;
		std::int32_t iReticleMinOfs;
		std::int32_t activeReticleType;
		float vStandMove[3];
		float vStandRot[3];
		float strafeMove[3];
		float strafeRot[3];
		float vDuckedOfs[3];
		float vDuckedMove[3];
		float vDuckedRot[3];
		float vProneOfs[3];
		float vProneMove[3];
		float vProneRot[3];
		float fPosMoveRate;
		float fPosProneMoveRate;
		float fStandMoveMinSpeed;
		float fDuckedMoveMinSpeed;
		float fProneMoveMinSpeed;
		float fPosRotRate;
		float fPosProneRotRate;
		float fStandRotMinSpeed;
		float fDuckedRotMinSpeed;
		float fProneRotMinSpeed;
		std::uint32_t worldModel;
		std::uint32_t worldClipModel;
		std::uint32_t rocketModel;
		std::uint32_t knifeModel;
		std::uint32_t worldKnifeModel;
		std::uint32_t hudIcon;
		std::int32_t hudIconRatio;
		std::uint32_t pickupIcon;
		std::int32_t pickupIconRatio;
		std::uint32_t ammoCounterIcon;
		std::int32_t ammoCounterIconRatio;
		std::int32_t ammoCounterClip;
		std::int32_t iStartAmmo;
		std::uint32_t szAmmoName;
		std::int32_t iAmmoIndex;
		std::uint32_t szClipName;
		std::int32_t iClipIndex;
		std::int32_t iMaxAmmo;
		std::int32_t shotCount;
		std::uint32_t szSharedAmmoCapName;
		std::int32_t iSharedAmmoCapIndex;
		std::int32_t iSharedAmmoCap;
		std::int32_t damage;
		std::int32_t playerDamage;
		std::int32_t iMeleeDamage;
		std::int32_t iDamageType;
		std::int32_t iFireDelay;
		std::int32_t iMeleeDelay;
		std::int32_t meleeChargeDelay;
		std::int32_t iDetonateDelay;
		std::int32_t iRechamberTime;
		std::int32_t rechamberTimeOneHanded;
		std::int32_t iRechamberBoltTime;
		std::int32_t iHoldFireTime;
		std::int32_t iDetonateTime;
		std::int32_t iMeleeTime;
		std::int32_t meleeChargeTime;
		std::int32_t iReloadTime;
		std::int32_t reloadShowRocketTime;
		std::int32_t iReloadEmptyTime;
		std::int32_t iReloadAddTime;
		std::int32_t iReloadStartTime;
		std::int32_t iReloadStartAddTime;
		std::int32_t iReloadEndTime;
		std::int32_t iDropTime;
		std::int32_t iRaiseTime;
		std::int32_t iAltDropTime;
		std::int32_t quickDropTime;
		std::int32_t quickRaiseTime;
		std::int32_t iBreachRaiseTime;
		std::int32_t iEmptyRaiseTime;
		std::int32_t iEmptyDropTime;
		std::int32_t sprintInTime;
		std::int32_t sprintLoopTime;
		std::int32_t sprintOutTime;
		std::int32_t stunnedTimeBegin;
		std::int32_t stunnedTimeLoop;
		std::int32_t stunnedTimeEnd;
		std::int32_t nightVisionWearTime;
		std::int32_t nightVisionWearTimeFadeOutEnd;
		std::int32_t nightVisionWearTimePowerUp;
		std::int32_t nightVisionRemoveTime;
		std::int32_t nightVisionRemoveTimePowerDown;
		std::int32_t nightVisionRemoveTimeFadeInStart;
		std::int32_t fuseTime;
		std::int32_t aiFuseTime;
		float autoAimRange;
		float aimAssistRange;
		float aimAssistRangeAds;
		float aimPadding;
		float enemyCrosshairRange;
		float moveSpeedScale;
		float adsMoveSpeedScale;
		float sprintDurationScale;
		float fAdsZoomInFrac;
		float fAdsZoomOutFrac;
		std::uint32_t overlayMaterial;
		std::uint32_t overlayMaterialLowRes;
		std::uint32_t overlayMaterialEMP;
		std::uint32_t overlayMaterialEMPLowRes;
		std::int32_t overlayReticle;
		std::int32_t overlayInterface;
		float overlayWidth;
		float overlayHeight;
		float overlayWidthSplitscreen;
		float overlayHeightSplitscreen;
		float fAdsBobFactor;
		float fAdsViewBobMult;
		float fHipSpreadStandMin;
		float fHipSpreadDuckedMin;
		float fHipSpreadProneMin;
		float hipSpreadStandMax;
		float hipSpreadDuckedMax;
		float hipSpreadProneMax;
		float fHipSpreadDecayRate;
		float fHipSpreadFireAdd;
		float fHipSpreadTurnAdd;
		float fHipSpreadMoveAdd;
		float fHipSpreadDuckedDecay;
		float fHipSpreadProneDecay;
		float fHipReticleSidePos;
		float fAdsIdleAmount;
		float fHipIdleAmount;
		float adsIdleSpeed;
		float hipIdleSpeed;
		float fIdleCrouchFactor;
		float fIdleProneFactor;
		float fGunMaxPitch;
		float fGunMaxYaw;
		float swayMaxAngle;
		float swayLerpSpeed;
		float swayPitchScale;
		float swayYawScale;
		float swayHorizScale;
		float swayVertScale;
		float swayShellShockScale;
		float adsSwayMaxAngle;
		float adsSwayLerpSpeed;
		float adsSwayPitchScale;
		float adsSwayYawScale;
		float adsSwayHorizScale;
		float adsSwayVertScale;
		float adsViewErrorMin;
		float adsViewErrorMax;
		std::uint32_t physCollmap;
		float dualWieldViewModelOffset;
		std::int32_t killIconRatio;
		std::int32_t iReloadAmmoAdd;
		std::int32_t iReloadStartAdd;
		std::int32_t ammoDropStockMin;
		std::int32_t ammoDropClipPercentMin;
		std::int32_t ammoDropClipPercentMax;
		std::int32_t iExplosionRadius;
		std::int32_t iExplosionRadiusMin;
		std::int32_t iExplosionInnerDamage;
		std::int32_t iExplosionOuterDamage;
		float damageConeAngle;
		float bulletExplDmgMult;
		float bulletExplRadiusMult;
		std::int32_t iProjectileSpeed;
		std::int32_t iProjectileSpeedUp;
		std::int32_t iProjectileSpeedForward;
		std::int32_t iProjectileActivateDist;
		float projLifetime;
		float timeToAccelerate;
		float projectileCurvature;
		std::uint32_t projectileModel;
		std::int32_t projExplosion;
		std::uint32_t projExplosionEffect;
		std::uint32_t projDudEffect;
		std::uint32_t projExplosionSound;
		std::uint32_t projDudSound;
		std::int32_t stickiness;
		float lowAmmoWarningThreshold;
		float ricochetChance;
		std::uint32_t parallelBounce;
		std::uint32_t perpendicularBounce;
		std::uint32_t projTrailEffect;
		std::uint32_t projBeaconEffect;
		float vProjectileColor[3];
		std::int32_t guidedMissileType;
		float maxSteeringAccel;
		std::int32_t projIgnitionDelay;
		std::uint32_t projIgnitionEffect;
		std::uint32_t projIgnitionSound;
		float fAdsAimPitch;
		float fAdsCrosshairInFrac;
		float fAdsCrosshairOutFrac;
		std::int32_t adsGunKickReducedKickBullets;
		float adsGunKickReducedKickPercent;
		float fAdsGunKickPitchMin;
		float fAdsGunKickPitchMax;
		float fAdsGunKickYawMin;
		float fAdsGunKickYawMax;
		float fAdsGunKickAccel;
		float fAdsGunKickSpeedMax;
		float fAdsGunKickSpeedDecay;
		float fAdsGunKickStaticDecay;
		float fAdsViewKickPitchMin;
		float fAdsViewKickPitchMax;
		float fAdsViewKickYawMin;
		float fAdsViewKickYawMax;
		float fAdsViewScatterMin;
		float fAdsViewScatterMax;
		float fAdsSpread;
		std::int32_t hipGunKickReducedKickBullets;
		float hipGunKickReducedKickPercent;
		float fHipGunKickPitchMin;
		float fHipGunKickPitchMax;
		float fHipGunKickYawMin;
		float fHipGunKickYawMax;
		float fHipGunKickAccel;
		float fHipGunKickSpeedMax;
		float fHipGunKickSpeedDecay;
		float fHipGunKickStaticDecay;
		float fHipViewKickPitchMin;
		float fHipViewKickPitchMax;
		float fHipViewKickYawMin;
		float fHipViewKickYawMax;
		float fHipViewScatterMin;
		float fHipViewScatterMax;
		float fightDist;
		float maxDist;
		std::uint32_t accuracyGraphName[2];
		std::uint32_t originalAccuracyGraphKnots[2];
		std::uint16_t originalAccuracyGraphKnotCount[2];
		std::int32_t iPositionReloadTransTime;
		float leftArc;
		float rightArc;
		float topArc;
		float bottomArc;
		float accuracy;
		float aiSpread;
		float playerSpread;
		float minTurnSpeed[2];
		float maxTurnSpeed[2];
		float pitchConvergenceTime;
		float yawConvergenceTime;
		float suppressTime;
		float maxRange;
		float fAnimHorRotateInc;
		float fPlayerPositionDist;
		std::uint32_t szUseHintString;
		std::uint32_t dropHintString;
		std::int32_t iUseHintStringIndex;
		std::int32_t dropHintStringIndex;
		float horizViewJitter;
		float vertViewJitter;
		float scanSpeed;
		float scanAccel;
		std::int32_t scanPauseTime;
		std::uint32_t szScript;
		float fOOPosAnimLength[2];
		std::int32_t minDamage;
		std::int32_t minPlayerDamage;
		float fMaxDamageRange;
		float fMinDamageRange;
		float destabilizationRateTime;
		float destabilizationCurvatureMax;
		std::int32_t destabilizeDistance;
		std::uint32_t locationDamageMultipliers;
		std::uint32_t fireRumble;
		std::uint32_t meleeImpactRumble;
		std::uint32_t tracerType;
		float turretScopeZoomRate;
		float turretScopeZoomMin;
		float turretScopeZoomMax;
		float turretOverheatUpRate;
		float turretOverheatDownRate;
		float turretOverheatPenalty;
		std::uint32_t turretOverheatSound;
		std::uint32_t turretOverheatEffect;
		std::uint32_t turretBarrelSpinRumble;
		float turretBarrelSpinSpeed;
		float turretBarrelSpinUpTime;
		float turretBarrelSpinDownTime;
		std::uint32_t turretBarrelSpinMaxSnd;
		std::uint32_t turretBarrelSpinUpSnd[4];
		std::uint32_t turretBarrelSpinDownSnd[4];
		std::uint32_t missileConeSoundAlias;
		std::uint32_t missileConeSoundAliasAtBase;
		float missileConeSoundRadiusAtTop;
		float missileConeSoundRadiusAtBase;
		float missileConeSoundHeight;
		float missileConeSoundOriginOffset;
		float missileConeSoundVolumescaleAtCore;
		float missileConeSoundVolumescaleAtEdge;
		float missileConeSoundVolumescaleCoreSize;
		float missileConeSoundPitchAtTop;
		float missileConeSoundPitchAtBottom;
		float missileConeSoundPitchTopSize;
		float missileConeSoundPitchBottomSize;
		float missileConeSoundCrossfadeTopSize;
		float missileConeSoundCrossfadeBottomSize;
		bool sharedAmmo;
		bool lockonSupported;
		bool requireLockonToFire;
		bool bigExplosion;
		bool noAdsWhenMagEmpty;
		bool avoidDropCleanup;
		bool inheritsPerks;
		bool crosshairColorChange;
		bool bRifleBullet;
		bool armorPiercing;
		bool bBoltAction;
		bool aimDownSight;
		bool bRechamberWhileAds;
		bool bBulletExplosiveDamage;
		bool bCookOffHold;
		bool bClipOnly;
		bool noAmmoPickup;
		bool adsFireOnly;
		bool cancelAutoHolsterWhenEmpty;
		bool disableSwitchToWhenEmpty;
		bool suppressAmmoReserveDisplay;
		bool laserSightDuringNightvision;
		bool markableViewmodel;
		bool noDualWield;
		bool flipKillIcon;
		bool bNoPartialReload;
		bool bSegmentedReload;
		bool blocksProne;
		bool silenced;
		bool isRollingGrenade;
		bool projExplosionEffectForceNormalUp;
		bool bProjImpactExplode;
		bool stickToPlayers;
		bool hasDetonator;
		bool disableFiring;
		bool timedDetonation;
		bool rotate;
		bool holdButtonToThrow;
		bool freezeMovementWhenFiring;
		bool thermalScope;
		bool altModeSameWeapon;
		bool turretBarrelSpinEnabled;
		bool missileConeSoundEnabled;
		bool missileConeSoundPitchshiftEnabled;
		bool missileConeSoundCrossfadeEnabled;
		bool offhandHoldIsCancelable;
	};

	static_assert(sizeof(WeaponDef) == 0x684);
	static_assert(offsetof(WeaponDef, szOverlayName) == 0x0);
	static_assert(offsetof(WeaponDef, gunXModel) == 0x4);
	static_assert(offsetof(WeaponDef, handXModel) == 0x8);
	static_assert(offsetof(WeaponDef, szXAnimsRightHanded) == 0xC);
	static_assert(offsetof(WeaponDef, szXAnimsLeftHanded) == 0x10);
	static_assert(offsetof(WeaponDef, szModeName) == 0x14);
	static_assert(offsetof(WeaponDef, notetrackSoundMapKeys) == 0x18);
	static_assert(offsetof(WeaponDef, notetrackSoundMapValues) == 0x1C);
	static_assert(offsetof(WeaponDef, notetrackRumbleMapKeys) == 0x20);
	static_assert(offsetof(WeaponDef, notetrackRumbleMapValues) == 0x24);
	static_assert(offsetof(WeaponDef, playerAnimType) == 0x28);
	static_assert(offsetof(WeaponDef, weapType) == 0x2C);
	static_assert(offsetof(WeaponDef, weapClass) == 0x30);
	static_assert(offsetof(WeaponDef, penetrateType) == 0x34);
	static_assert(offsetof(WeaponDef, inventoryType) == 0x38);
	static_assert(offsetof(WeaponDef, fireType) == 0x3C);
	static_assert(offsetof(WeaponDef, offhandClass) == 0x40);
	static_assert(offsetof(WeaponDef, stance) == 0x44);
	static_assert(offsetof(WeaponDef, viewFlashEffect) == 0x48);
	static_assert(offsetof(WeaponDef, worldFlashEffect) == 0x4C);
	static_assert(offsetof(WeaponDef, pickupSound) == 0x50);
	static_assert(offsetof(WeaponDef, pickupSoundPlayer) == 0x54);
	static_assert(offsetof(WeaponDef, ammoPickupSound) == 0x58);
	static_assert(offsetof(WeaponDef, ammoPickupSoundPlayer) == 0x5C);
	static_assert(offsetof(WeaponDef, projectileSound) == 0x60);
	static_assert(offsetof(WeaponDef, pullbackSound) == 0x64);
	static_assert(offsetof(WeaponDef, pullbackSoundPlayer) == 0x68);
	static_assert(offsetof(WeaponDef, fireSound) == 0x6C);
	static_assert(offsetof(WeaponDef, fireSoundPlayer) == 0x70);
	static_assert(offsetof(WeaponDef, fireSoundPlayerAkimbo) == 0x74);
	static_assert(offsetof(WeaponDef, fireLoopSound) == 0x78);
	static_assert(offsetof(WeaponDef, fireLoopSoundPlayer) == 0x7C);
	static_assert(offsetof(WeaponDef, fireStopSound) == 0x80);
	static_assert(offsetof(WeaponDef, fireStopSoundPlayer) == 0x84);
	static_assert(offsetof(WeaponDef, fireLastSound) == 0x88);
	static_assert(offsetof(WeaponDef, fireLastSoundPlayer) == 0x8C);
	static_assert(offsetof(WeaponDef, emptyFireSound) == 0x90);
	static_assert(offsetof(WeaponDef, emptyFireSoundPlayer) == 0x94);
	static_assert(offsetof(WeaponDef, meleeSwipeSound) == 0x98);
	static_assert(offsetof(WeaponDef, meleeSwipeSoundPlayer) == 0x9C);
	static_assert(offsetof(WeaponDef, meleeHitSound) == 0xA0);
	static_assert(offsetof(WeaponDef, meleeMissSound) == 0xA4);
	static_assert(offsetof(WeaponDef, rechamberSound) == 0xA8);
	static_assert(offsetof(WeaponDef, rechamberSoundPlayer) == 0xAC);
	static_assert(offsetof(WeaponDef, reloadSound) == 0xB0);
	static_assert(offsetof(WeaponDef, reloadSoundPlayer) == 0xB4);
	static_assert(offsetof(WeaponDef, reloadEmptySound) == 0xB8);
	static_assert(offsetof(WeaponDef, reloadEmptySoundPlayer) == 0xBC);
	static_assert(offsetof(WeaponDef, reloadStartSound) == 0xC0);
	static_assert(offsetof(WeaponDef, reloadStartSoundPlayer) == 0xC4);
	static_assert(offsetof(WeaponDef, reloadEndSound) == 0xC8);
	static_assert(offsetof(WeaponDef, reloadEndSoundPlayer) == 0xCC);
	static_assert(offsetof(WeaponDef, detonateSound) == 0xD0);
	static_assert(offsetof(WeaponDef, detonateSoundPlayer) == 0xD4);
	static_assert(offsetof(WeaponDef, nightVisionWearSound) == 0xD8);
	static_assert(offsetof(WeaponDef, nightVisionWearSoundPlayer) == 0xDC);
	static_assert(offsetof(WeaponDef, nightVisionRemoveSound) == 0xE0);
	static_assert(offsetof(WeaponDef, nightVisionRemoveSoundPlayer) == 0xE4);
	static_assert(offsetof(WeaponDef, altSwitchSound) == 0xE8);
	static_assert(offsetof(WeaponDef, altSwitchSoundPlayer) == 0xEC);
	static_assert(offsetof(WeaponDef, raiseSound) == 0xF0);
	static_assert(offsetof(WeaponDef, raiseSoundPlayer) == 0xF4);
	static_assert(offsetof(WeaponDef, firstRaiseSound) == 0xF8);
	static_assert(offsetof(WeaponDef, firstRaiseSoundPlayer) == 0xFC);
	static_assert(offsetof(WeaponDef, putawaySound) == 0x100);
	static_assert(offsetof(WeaponDef, putawaySoundPlayer) == 0x104);
	static_assert(offsetof(WeaponDef, scanSound) == 0x108);
	static_assert(offsetof(WeaponDef, bounceSound) == 0x10C);
	static_assert(offsetof(WeaponDef, viewShellEjectEffect) == 0x110);
	static_assert(offsetof(WeaponDef, worldShellEjectEffect) == 0x114);
	static_assert(offsetof(WeaponDef, viewLastShotEjectEffect) == 0x118);
	static_assert(offsetof(WeaponDef, worldLastShotEjectEffect) == 0x11C);
	static_assert(offsetof(WeaponDef, reticleCenter) == 0x120);
	static_assert(offsetof(WeaponDef, reticleSide) == 0x124);
	static_assert(offsetof(WeaponDef, iReticleCenterSize) == 0x128);
	static_assert(offsetof(WeaponDef, iReticleSideSize) == 0x12C);
	static_assert(offsetof(WeaponDef, iReticleMinOfs) == 0x130);
	static_assert(offsetof(WeaponDef, activeReticleType) == 0x134);
	static_assert(offsetof(WeaponDef, vStandMove) == 0x138);
	static_assert(offsetof(WeaponDef, vStandRot) == 0x144);
	static_assert(offsetof(WeaponDef, strafeMove) == 0x150);
	static_assert(offsetof(WeaponDef, strafeRot) == 0x15C);
	static_assert(offsetof(WeaponDef, vDuckedOfs) == 0x168);
	static_assert(offsetof(WeaponDef, vDuckedMove) == 0x174);
	static_assert(offsetof(WeaponDef, vDuckedRot) == 0x180);
	static_assert(offsetof(WeaponDef, vProneOfs) == 0x18C);
	static_assert(offsetof(WeaponDef, vProneMove) == 0x198);
	static_assert(offsetof(WeaponDef, vProneRot) == 0x1A4);
	static_assert(offsetof(WeaponDef, fPosMoveRate) == 0x1B0);
	static_assert(offsetof(WeaponDef, fPosProneMoveRate) == 0x1B4);
	static_assert(offsetof(WeaponDef, fStandMoveMinSpeed) == 0x1B8);
	static_assert(offsetof(WeaponDef, fDuckedMoveMinSpeed) == 0x1BC);
	static_assert(offsetof(WeaponDef, fProneMoveMinSpeed) == 0x1C0);
	static_assert(offsetof(WeaponDef, fPosRotRate) == 0x1C4);
	static_assert(offsetof(WeaponDef, fPosProneRotRate) == 0x1C8);
	static_assert(offsetof(WeaponDef, fStandRotMinSpeed) == 0x1CC);
	static_assert(offsetof(WeaponDef, fDuckedRotMinSpeed) == 0x1D0);
	static_assert(offsetof(WeaponDef, fProneRotMinSpeed) == 0x1D4);
	static_assert(offsetof(WeaponDef, worldModel) == 0x1D8);
	static_assert(offsetof(WeaponDef, worldClipModel) == 0x1DC);
	static_assert(offsetof(WeaponDef, rocketModel) == 0x1E0);
	static_assert(offsetof(WeaponDef, knifeModel) == 0x1E4);
	static_assert(offsetof(WeaponDef, worldKnifeModel) == 0x1E8);
	static_assert(offsetof(WeaponDef, hudIcon) == 0x1EC);
	static_assert(offsetof(WeaponDef, hudIconRatio) == 0x1F0);
	static_assert(offsetof(WeaponDef, pickupIcon) == 0x1F4);
	static_assert(offsetof(WeaponDef, pickupIconRatio) == 0x1F8);
	static_assert(offsetof(WeaponDef, ammoCounterIcon) == 0x1FC);
	static_assert(offsetof(WeaponDef, ammoCounterIconRatio) == 0x200);
	static_assert(offsetof(WeaponDef, ammoCounterClip) == 0x204);
	static_assert(offsetof(WeaponDef, iStartAmmo) == 0x208);
	static_assert(offsetof(WeaponDef, szAmmoName) == 0x20C);
	static_assert(offsetof(WeaponDef, iAmmoIndex) == 0x210);
	static_assert(offsetof(WeaponDef, szClipName) == 0x214);
	static_assert(offsetof(WeaponDef, iClipIndex) == 0x218);
	static_assert(offsetof(WeaponDef, iMaxAmmo) == 0x21C);
	static_assert(offsetof(WeaponDef, shotCount) == 0x220);
	static_assert(offsetof(WeaponDef, szSharedAmmoCapName) == 0x224);
	static_assert(offsetof(WeaponDef, iSharedAmmoCapIndex) == 0x228);
	static_assert(offsetof(WeaponDef, iSharedAmmoCap) == 0x22C);
	static_assert(offsetof(WeaponDef, damage) == 0x230);
	static_assert(offsetof(WeaponDef, playerDamage) == 0x234);
	static_assert(offsetof(WeaponDef, iMeleeDamage) == 0x238);
	static_assert(offsetof(WeaponDef, iDamageType) == 0x23C);
	static_assert(offsetof(WeaponDef, iFireDelay) == 0x240);
	static_assert(offsetof(WeaponDef, iMeleeDelay) == 0x244);
	static_assert(offsetof(WeaponDef, meleeChargeDelay) == 0x248);
	static_assert(offsetof(WeaponDef, iDetonateDelay) == 0x24C);
	static_assert(offsetof(WeaponDef, iRechamberTime) == 0x250);
	static_assert(offsetof(WeaponDef, rechamberTimeOneHanded) == 0x254);
	static_assert(offsetof(WeaponDef, iRechamberBoltTime) == 0x258);
	static_assert(offsetof(WeaponDef, iHoldFireTime) == 0x25C);
	static_assert(offsetof(WeaponDef, iDetonateTime) == 0x260);
	static_assert(offsetof(WeaponDef, iMeleeTime) == 0x264);
	static_assert(offsetof(WeaponDef, meleeChargeTime) == 0x268);
	static_assert(offsetof(WeaponDef, iReloadTime) == 0x26C);
	static_assert(offsetof(WeaponDef, reloadShowRocketTime) == 0x270);
	static_assert(offsetof(WeaponDef, iReloadEmptyTime) == 0x274);
	static_assert(offsetof(WeaponDef, iReloadAddTime) == 0x278);
	static_assert(offsetof(WeaponDef, iReloadStartTime) == 0x27C);
	static_assert(offsetof(WeaponDef, iReloadStartAddTime) == 0x280);
	static_assert(offsetof(WeaponDef, iReloadEndTime) == 0x284);
	static_assert(offsetof(WeaponDef, iDropTime) == 0x288);
	static_assert(offsetof(WeaponDef, iRaiseTime) == 0x28C);
	static_assert(offsetof(WeaponDef, iAltDropTime) == 0x290);
	static_assert(offsetof(WeaponDef, quickDropTime) == 0x294);
	static_assert(offsetof(WeaponDef, quickRaiseTime) == 0x298);
	static_assert(offsetof(WeaponDef, iBreachRaiseTime) == 0x29C);
	static_assert(offsetof(WeaponDef, iEmptyRaiseTime) == 0x2A0);
	static_assert(offsetof(WeaponDef, iEmptyDropTime) == 0x2A4);
	static_assert(offsetof(WeaponDef, sprintInTime) == 0x2A8);
	static_assert(offsetof(WeaponDef, sprintLoopTime) == 0x2AC);
	static_assert(offsetof(WeaponDef, sprintOutTime) == 0x2B0);
	static_assert(offsetof(WeaponDef, stunnedTimeBegin) == 0x2B4);
	static_assert(offsetof(WeaponDef, stunnedTimeLoop) == 0x2B8);
	static_assert(offsetof(WeaponDef, stunnedTimeEnd) == 0x2BC);
	static_assert(offsetof(WeaponDef, nightVisionWearTime) == 0x2C0);
	static_assert(offsetof(WeaponDef, nightVisionWearTimeFadeOutEnd) == 0x2C4);
	static_assert(offsetof(WeaponDef, nightVisionWearTimePowerUp) == 0x2C8);
	static_assert(offsetof(WeaponDef, nightVisionRemoveTime) == 0x2CC);
	static_assert(offsetof(WeaponDef, nightVisionRemoveTimePowerDown) == 0x2D0);
	static_assert(offsetof(WeaponDef, nightVisionRemoveTimeFadeInStart) == 0x2D4);
	static_assert(offsetof(WeaponDef, fuseTime) == 0x2D8);
	static_assert(offsetof(WeaponDef, aiFuseTime) == 0x2DC);
	static_assert(offsetof(WeaponDef, autoAimRange) == 0x2E0);
	static_assert(offsetof(WeaponDef, aimAssistRange) == 0x2E4);
	static_assert(offsetof(WeaponDef, aimAssistRangeAds) == 0x2E8);
	static_assert(offsetof(WeaponDef, aimPadding) == 0x2EC);
	static_assert(offsetof(WeaponDef, enemyCrosshairRange) == 0x2F0);
	static_assert(offsetof(WeaponDef, moveSpeedScale) == 0x2F4);
	static_assert(offsetof(WeaponDef, adsMoveSpeedScale) == 0x2F8);
	static_assert(offsetof(WeaponDef, sprintDurationScale) == 0x2FC);
	static_assert(offsetof(WeaponDef, fAdsZoomInFrac) == 0x300);
	static_assert(offsetof(WeaponDef, fAdsZoomOutFrac) == 0x304);
	static_assert(offsetof(WeaponDef, overlayMaterial) == 0x308);
	static_assert(offsetof(WeaponDef, overlayMaterialLowRes) == 0x30C);
	static_assert(offsetof(WeaponDef, overlayMaterialEMP) == 0x310);
	static_assert(offsetof(WeaponDef, overlayMaterialEMPLowRes) == 0x314);
	static_assert(offsetof(WeaponDef, overlayReticle) == 0x318);
	static_assert(offsetof(WeaponDef, overlayInterface) == 0x31C);
	static_assert(offsetof(WeaponDef, overlayWidth) == 0x320);
	static_assert(offsetof(WeaponDef, overlayHeight) == 0x324);
	static_assert(offsetof(WeaponDef, overlayWidthSplitscreen) == 0x328);
	static_assert(offsetof(WeaponDef, overlayHeightSplitscreen) == 0x32C);
	static_assert(offsetof(WeaponDef, fAdsBobFactor) == 0x330);
	static_assert(offsetof(WeaponDef, fAdsViewBobMult) == 0x334);
	static_assert(offsetof(WeaponDef, fHipSpreadStandMin) == 0x338);
	static_assert(offsetof(WeaponDef, fHipSpreadDuckedMin) == 0x33C);
	static_assert(offsetof(WeaponDef, fHipSpreadProneMin) == 0x340);
	static_assert(offsetof(WeaponDef, hipSpreadStandMax) == 0x344);
	static_assert(offsetof(WeaponDef, hipSpreadDuckedMax) == 0x348);
	static_assert(offsetof(WeaponDef, hipSpreadProneMax) == 0x34C);
	static_assert(offsetof(WeaponDef, fHipSpreadDecayRate) == 0x350);
	static_assert(offsetof(WeaponDef, fHipSpreadFireAdd) == 0x354);
	static_assert(offsetof(WeaponDef, fHipSpreadTurnAdd) == 0x358);
	static_assert(offsetof(WeaponDef, fHipSpreadMoveAdd) == 0x35C);
	static_assert(offsetof(WeaponDef, fHipSpreadDuckedDecay) == 0x360);
	static_assert(offsetof(WeaponDef, fHipSpreadProneDecay) == 0x364);
	static_assert(offsetof(WeaponDef, fHipReticleSidePos) == 0x368);
	static_assert(offsetof(WeaponDef, fAdsIdleAmount) == 0x36C);
	static_assert(offsetof(WeaponDef, fHipIdleAmount) == 0x370);
	static_assert(offsetof(WeaponDef, adsIdleSpeed) == 0x374);
	static_assert(offsetof(WeaponDef, hipIdleSpeed) == 0x378);
	static_assert(offsetof(WeaponDef, fIdleCrouchFactor) == 0x37C);
	static_assert(offsetof(WeaponDef, fIdleProneFactor) == 0x380);
	static_assert(offsetof(WeaponDef, fGunMaxPitch) == 0x384);
	static_assert(offsetof(WeaponDef, fGunMaxYaw) == 0x388);
	static_assert(offsetof(WeaponDef, swayMaxAngle) == 0x38C);
	static_assert(offsetof(WeaponDef, swayLerpSpeed) == 0x390);
	static_assert(offsetof(WeaponDef, swayPitchScale) == 0x394);
	static_assert(offsetof(WeaponDef, swayYawScale) == 0x398);
	static_assert(offsetof(WeaponDef, swayHorizScale) == 0x39C);
	static_assert(offsetof(WeaponDef, swayVertScale) == 0x3A0);
	static_assert(offsetof(WeaponDef, swayShellShockScale) == 0x3A4);
	static_assert(offsetof(WeaponDef, adsSwayMaxAngle) == 0x3A8);
	static_assert(offsetof(WeaponDef, adsSwayLerpSpeed) == 0x3AC);
	static_assert(offsetof(WeaponDef, adsSwayPitchScale) == 0x3B0);
	static_assert(offsetof(WeaponDef, adsSwayYawScale) == 0x3B4);
	static_assert(offsetof(WeaponDef, adsSwayHorizScale) == 0x3B8);
	static_assert(offsetof(WeaponDef, adsSwayVertScale) == 0x3BC);
	static_assert(offsetof(WeaponDef, adsViewErrorMin) == 0x3C0);
	static_assert(offsetof(WeaponDef, adsViewErrorMax) == 0x3C4);
	static_assert(offsetof(WeaponDef, physCollmap) == 0x3C8);
	static_assert(offsetof(WeaponDef, dualWieldViewModelOffset) == 0x3CC);
	static_assert(offsetof(WeaponDef, killIconRatio) == 0x3D0);
	static_assert(offsetof(WeaponDef, iReloadAmmoAdd) == 0x3D4);
	static_assert(offsetof(WeaponDef, iReloadStartAdd) == 0x3D8);
	static_assert(offsetof(WeaponDef, ammoDropStockMin) == 0x3DC);
	static_assert(offsetof(WeaponDef, ammoDropClipPercentMin) == 0x3E0);
	static_assert(offsetof(WeaponDef, ammoDropClipPercentMax) == 0x3E4);
	static_assert(offsetof(WeaponDef, iExplosionRadius) == 0x3E8);
	static_assert(offsetof(WeaponDef, iExplosionRadiusMin) == 0x3EC);
	static_assert(offsetof(WeaponDef, iExplosionInnerDamage) == 0x3F0);
	static_assert(offsetof(WeaponDef, iExplosionOuterDamage) == 0x3F4);
	static_assert(offsetof(WeaponDef, damageConeAngle) == 0x3F8);
	static_assert(offsetof(WeaponDef, bulletExplDmgMult) == 0x3FC);
	static_assert(offsetof(WeaponDef, bulletExplRadiusMult) == 0x400);
	static_assert(offsetof(WeaponDef, iProjectileSpeed) == 0x404);
	static_assert(offsetof(WeaponDef, iProjectileSpeedUp) == 0x408);
	static_assert(offsetof(WeaponDef, iProjectileSpeedForward) == 0x40C);
	static_assert(offsetof(WeaponDef, iProjectileActivateDist) == 0x410);
	static_assert(offsetof(WeaponDef, projLifetime) == 0x414);
	static_assert(offsetof(WeaponDef, timeToAccelerate) == 0x418);
	static_assert(offsetof(WeaponDef, projectileCurvature) == 0x41C);
	static_assert(offsetof(WeaponDef, projectileModel) == 0x420);
	static_assert(offsetof(WeaponDef, projExplosion) == 0x424);
	static_assert(offsetof(WeaponDef, projExplosionEffect) == 0x428);
	static_assert(offsetof(WeaponDef, projDudEffect) == 0x42C);
	static_assert(offsetof(WeaponDef, projExplosionSound) == 0x430);
	static_assert(offsetof(WeaponDef, projDudSound) == 0x434);
	static_assert(offsetof(WeaponDef, stickiness) == 0x438);
	static_assert(offsetof(WeaponDef, lowAmmoWarningThreshold) == 0x43C);
	static_assert(offsetof(WeaponDef, ricochetChance) == 0x440);
	static_assert(offsetof(WeaponDef, parallelBounce) == 0x444);
	static_assert(offsetof(WeaponDef, perpendicularBounce) == 0x448);
	static_assert(offsetof(WeaponDef, projTrailEffect) == 0x44C);
	static_assert(offsetof(WeaponDef, projBeaconEffect) == 0x450);
	static_assert(offsetof(WeaponDef, vProjectileColor) == 0x454);
	static_assert(offsetof(WeaponDef, guidedMissileType) == 0x460);
	static_assert(offsetof(WeaponDef, maxSteeringAccel) == 0x464);
	static_assert(offsetof(WeaponDef, projIgnitionDelay) == 0x468);
	static_assert(offsetof(WeaponDef, projIgnitionEffect) == 0x46C);
	static_assert(offsetof(WeaponDef, projIgnitionSound) == 0x470);
	static_assert(offsetof(WeaponDef, fAdsAimPitch) == 0x474);
	static_assert(offsetof(WeaponDef, fAdsCrosshairInFrac) == 0x478);
	static_assert(offsetof(WeaponDef, fAdsCrosshairOutFrac) == 0x47C);
	static_assert(offsetof(WeaponDef, adsGunKickReducedKickBullets) == 0x480);
	static_assert(offsetof(WeaponDef, adsGunKickReducedKickPercent) == 0x484);
	static_assert(offsetof(WeaponDef, fAdsGunKickPitchMin) == 0x488);
	static_assert(offsetof(WeaponDef, fAdsGunKickPitchMax) == 0x48C);
	static_assert(offsetof(WeaponDef, fAdsGunKickYawMin) == 0x490);
	static_assert(offsetof(WeaponDef, fAdsGunKickYawMax) == 0x494);
	static_assert(offsetof(WeaponDef, fAdsGunKickAccel) == 0x498);
	static_assert(offsetof(WeaponDef, fAdsGunKickSpeedMax) == 0x49C);
	static_assert(offsetof(WeaponDef, fAdsGunKickSpeedDecay) == 0x4A0);
	static_assert(offsetof(WeaponDef, fAdsGunKickStaticDecay) == 0x4A4);
	static_assert(offsetof(WeaponDef, fAdsViewKickPitchMin) == 0x4A8);
	static_assert(offsetof(WeaponDef, fAdsViewKickPitchMax) == 0x4AC);
	static_assert(offsetof(WeaponDef, fAdsViewKickYawMin) == 0x4B0);
	static_assert(offsetof(WeaponDef, fAdsViewKickYawMax) == 0x4B4);
	static_assert(offsetof(WeaponDef, fAdsViewScatterMin) == 0x4B8);
	static_assert(offsetof(WeaponDef, fAdsViewScatterMax) == 0x4BC);
	static_assert(offsetof(WeaponDef, fAdsSpread) == 0x4C0);
	static_assert(offsetof(WeaponDef, hipGunKickReducedKickBullets) == 0x4C4);
	static_assert(offsetof(WeaponDef, hipGunKickReducedKickPercent) == 0x4C8);
	static_assert(offsetof(WeaponDef, fHipGunKickPitchMin) == 0x4CC);
	static_assert(offsetof(WeaponDef, fHipGunKickPitchMax) == 0x4D0);
	static_assert(offsetof(WeaponDef, fHipGunKickYawMin) == 0x4D4);
	static_assert(offsetof(WeaponDef, fHipGunKickYawMax) == 0x4D8);
	static_assert(offsetof(WeaponDef, fHipGunKickAccel) == 0x4DC);
	static_assert(offsetof(WeaponDef, fHipGunKickSpeedMax) == 0x4E0);
	static_assert(offsetof(WeaponDef, fHipGunKickSpeedDecay) == 0x4E4);
	static_assert(offsetof(WeaponDef, fHipGunKickStaticDecay) == 0x4E8);
	static_assert(offsetof(WeaponDef, fHipViewKickPitchMin) == 0x4EC);
	static_assert(offsetof(WeaponDef, fHipViewKickPitchMax) == 0x4F0);
	static_assert(offsetof(WeaponDef, fHipViewKickYawMin) == 0x4F4);
	static_assert(offsetof(WeaponDef, fHipViewKickYawMax) == 0x4F8);
	static_assert(offsetof(WeaponDef, fHipViewScatterMin) == 0x4FC);
	static_assert(offsetof(WeaponDef, fHipViewScatterMax) == 0x500);
	static_assert(offsetof(WeaponDef, fightDist) == 0x504);
	static_assert(offsetof(WeaponDef, maxDist) == 0x508);
	static_assert(offsetof(WeaponDef, accuracyGraphName) == 0x50C);
	static_assert(offsetof(WeaponDef, originalAccuracyGraphKnots) == 0x514);
	static_assert(offsetof(WeaponDef, originalAccuracyGraphKnotCount) == 0x51C);
	static_assert(offsetof(WeaponDef, iPositionReloadTransTime) == 0x520);
	static_assert(offsetof(WeaponDef, leftArc) == 0x524);
	static_assert(offsetof(WeaponDef, rightArc) == 0x528);
	static_assert(offsetof(WeaponDef, topArc) == 0x52C);
	static_assert(offsetof(WeaponDef, bottomArc) == 0x530);
	static_assert(offsetof(WeaponDef, accuracy) == 0x534);
	static_assert(offsetof(WeaponDef, aiSpread) == 0x538);
	static_assert(offsetof(WeaponDef, playerSpread) == 0x53C);
	static_assert(offsetof(WeaponDef, minTurnSpeed) == 0x540);
	static_assert(offsetof(WeaponDef, maxTurnSpeed) == 0x548);
	static_assert(offsetof(WeaponDef, pitchConvergenceTime) == 0x550);
	static_assert(offsetof(WeaponDef, yawConvergenceTime) == 0x554);
	static_assert(offsetof(WeaponDef, suppressTime) == 0x558);
	static_assert(offsetof(WeaponDef, maxRange) == 0x55C);
	static_assert(offsetof(WeaponDef, fAnimHorRotateInc) == 0x560);
	static_assert(offsetof(WeaponDef, fPlayerPositionDist) == 0x564);
	static_assert(offsetof(WeaponDef, szUseHintString) == 0x568);
	static_assert(offsetof(WeaponDef, dropHintString) == 0x56C);
	static_assert(offsetof(WeaponDef, iUseHintStringIndex) == 0x570);
	static_assert(offsetof(WeaponDef, dropHintStringIndex) == 0x574);
	static_assert(offsetof(WeaponDef, horizViewJitter) == 0x578);
	static_assert(offsetof(WeaponDef, vertViewJitter) == 0x57C);
	static_assert(offsetof(WeaponDef, scanSpeed) == 0x580);
	static_assert(offsetof(WeaponDef, scanAccel) == 0x584);
	static_assert(offsetof(WeaponDef, scanPauseTime) == 0x588);
	static_assert(offsetof(WeaponDef, szScript) == 0x58C);
	static_assert(offsetof(WeaponDef, fOOPosAnimLength) == 0x590);
	static_assert(offsetof(WeaponDef, minDamage) == 0x598);
	static_assert(offsetof(WeaponDef, minPlayerDamage) == 0x59C);
	static_assert(offsetof(WeaponDef, fMaxDamageRange) == 0x5A0);
	static_assert(offsetof(WeaponDef, fMinDamageRange) == 0x5A4);
	static_assert(offsetof(WeaponDef, destabilizationRateTime) == 0x5A8);
	static_assert(offsetof(WeaponDef, destabilizationCurvatureMax) == 0x5AC);
	static_assert(offsetof(WeaponDef, destabilizeDistance) == 0x5B0);
	static_assert(offsetof(WeaponDef, locationDamageMultipliers) == 0x5B4);
	static_assert(offsetof(WeaponDef, fireRumble) == 0x5B8);
	static_assert(offsetof(WeaponDef, meleeImpactRumble) == 0x5BC);
	static_assert(offsetof(WeaponDef, tracerType) == 0x5C0);
	static_assert(offsetof(WeaponDef, turretScopeZoomRate) == 0x5C4);
	static_assert(offsetof(WeaponDef, turretScopeZoomMin) == 0x5C8);
	static_assert(offsetof(WeaponDef, turretScopeZoomMax) == 0x5CC);
	static_assert(offsetof(WeaponDef, turretOverheatUpRate) == 0x5D0);
	static_assert(offsetof(WeaponDef, turretOverheatDownRate) == 0x5D4);
	static_assert(offsetof(WeaponDef, turretOverheatPenalty) == 0x5D8);
	static_assert(offsetof(WeaponDef, turretOverheatSound) == 0x5DC);
	static_assert(offsetof(WeaponDef, turretOverheatEffect) == 0x5E0);
	static_assert(offsetof(WeaponDef, turretBarrelSpinRumble) == 0x5E4);
	static_assert(offsetof(WeaponDef, turretBarrelSpinSpeed) == 0x5E8);
	static_assert(offsetof(WeaponDef, turretBarrelSpinUpTime) == 0x5EC);
	static_assert(offsetof(WeaponDef, turretBarrelSpinDownTime) == 0x5F0);
	static_assert(offsetof(WeaponDef, turretBarrelSpinMaxSnd) == 0x5F4);
	static_assert(offsetof(WeaponDef, turretBarrelSpinUpSnd) == 0x5F8);
	static_assert(offsetof(WeaponDef, turretBarrelSpinDownSnd) == 0x608);
	static_assert(offsetof(WeaponDef, missileConeSoundAlias) == 0x618);
	static_assert(offsetof(WeaponDef, missileConeSoundAliasAtBase) == 0x61C);
	static_assert(offsetof(WeaponDef, missileConeSoundRadiusAtTop) == 0x620);
	static_assert(offsetof(WeaponDef, missileConeSoundRadiusAtBase) == 0x624);
	static_assert(offsetof(WeaponDef, missileConeSoundHeight) == 0x628);
	static_assert(offsetof(WeaponDef, missileConeSoundOriginOffset) == 0x62C);
	static_assert(offsetof(WeaponDef, missileConeSoundVolumescaleAtCore) == 0x630);
	static_assert(offsetof(WeaponDef, missileConeSoundVolumescaleAtEdge) == 0x634);
	static_assert(offsetof(WeaponDef, missileConeSoundVolumescaleCoreSize) == 0x638);
	static_assert(offsetof(WeaponDef, missileConeSoundPitchAtTop) == 0x63C);
	static_assert(offsetof(WeaponDef, missileConeSoundPitchAtBottom) == 0x640);
	static_assert(offsetof(WeaponDef, missileConeSoundPitchTopSize) == 0x644);
	static_assert(offsetof(WeaponDef, missileConeSoundPitchBottomSize) == 0x648);
	static_assert(offsetof(WeaponDef, missileConeSoundCrossfadeTopSize) == 0x64C);
	static_assert(offsetof(WeaponDef, missileConeSoundCrossfadeBottomSize) == 0x650);
	static_assert(offsetof(WeaponDef, sharedAmmo) == 0x654);
	static_assert(offsetof(WeaponDef, lockonSupported) == 0x655);
	static_assert(offsetof(WeaponDef, requireLockonToFire) == 0x656);
	static_assert(offsetof(WeaponDef, bigExplosion) == 0x657);
	static_assert(offsetof(WeaponDef, noAdsWhenMagEmpty) == 0x658);
	static_assert(offsetof(WeaponDef, avoidDropCleanup) == 0x659);
	static_assert(offsetof(WeaponDef, inheritsPerks) == 0x65A);
	static_assert(offsetof(WeaponDef, crosshairColorChange) == 0x65B);
	static_assert(offsetof(WeaponDef, bRifleBullet) == 0x65C);
	static_assert(offsetof(WeaponDef, armorPiercing) == 0x65D);
	static_assert(offsetof(WeaponDef, bBoltAction) == 0x65E);
	static_assert(offsetof(WeaponDef, aimDownSight) == 0x65F);
	static_assert(offsetof(WeaponDef, bRechamberWhileAds) == 0x660);
	static_assert(offsetof(WeaponDef, bBulletExplosiveDamage) == 0x661);
	static_assert(offsetof(WeaponDef, bCookOffHold) == 0x662);
	static_assert(offsetof(WeaponDef, bClipOnly) == 0x663);
	static_assert(offsetof(WeaponDef, noAmmoPickup) == 0x664);
	static_assert(offsetof(WeaponDef, adsFireOnly) == 0x665);
	static_assert(offsetof(WeaponDef, cancelAutoHolsterWhenEmpty) == 0x666);
	static_assert(offsetof(WeaponDef, disableSwitchToWhenEmpty) == 0x667);
	static_assert(offsetof(WeaponDef, suppressAmmoReserveDisplay) == 0x668);
	static_assert(offsetof(WeaponDef, laserSightDuringNightvision) == 0x669);
	static_assert(offsetof(WeaponDef, markableViewmodel) == 0x66A);
	static_assert(offsetof(WeaponDef, noDualWield) == 0x66B);
	static_assert(offsetof(WeaponDef, flipKillIcon) == 0x66C);
	static_assert(offsetof(WeaponDef, bNoPartialReload) == 0x66D);
	static_assert(offsetof(WeaponDef, bSegmentedReload) == 0x66E);
	static_assert(offsetof(WeaponDef, blocksProne) == 0x66F);
	static_assert(offsetof(WeaponDef, silenced) == 0x670);
	static_assert(offsetof(WeaponDef, isRollingGrenade) == 0x671);
	static_assert(offsetof(WeaponDef, projExplosionEffectForceNormalUp) == 0x672);
	static_assert(offsetof(WeaponDef, bProjImpactExplode) == 0x673);
	static_assert(offsetof(WeaponDef, stickToPlayers) == 0x674);
	static_assert(offsetof(WeaponDef, hasDetonator) == 0x675);
	static_assert(offsetof(WeaponDef, disableFiring) == 0x676);
	static_assert(offsetof(WeaponDef, timedDetonation) == 0x677);
	static_assert(offsetof(WeaponDef, rotate) == 0x678);
	static_assert(offsetof(WeaponDef, holdButtonToThrow) == 0x679);
	static_assert(offsetof(WeaponDef, freezeMovementWhenFiring) == 0x67A);
	static_assert(offsetof(WeaponDef, thermalScope) == 0x67B);
	static_assert(offsetof(WeaponDef, altModeSameWeapon) == 0x67C);
	static_assert(offsetof(WeaponDef, turretBarrelSpinEnabled) == 0x67D);
	static_assert(offsetof(WeaponDef, missileConeSoundEnabled) == 0x67E);
	static_assert(offsetof(WeaponDef, missileConeSoundPitchshiftEnabled) == 0x67F);
	static_assert(offsetof(WeaponDef, missileConeSoundCrossfadeEnabled) == 0x680);
	static_assert(offsetof(WeaponDef, offhandHoldIsCancelable) == 0x681);

	inline WeaponDef Convert(const Game::WeaponDef& from)
	{
		WeaponDef to{};
		to.playerAnimType = static_cast<std::int32_t>(from.playerAnimType);
		to.weapType = static_cast<std::int32_t>(from.weapType);
		to.weapClass = static_cast<std::int32_t>(from.weapClass);
		to.penetrateType = static_cast<std::int32_t>(from.penetrateType);
		to.inventoryType = static_cast<std::int32_t>(from.inventoryType);
		to.fireType = static_cast<std::int32_t>(from.fireType);
		to.offhandClass = static_cast<std::int32_t>(from.offhandClass);
		to.stance = static_cast<std::int32_t>(from.stance);
		to.iReticleCenterSize = static_cast<std::int32_t>(from.iReticleCenterSize);
		to.iReticleSideSize = static_cast<std::int32_t>(from.iReticleSideSize);
		to.iReticleMinOfs = static_cast<std::int32_t>(from.iReticleMinOfs);
		to.activeReticleType = static_cast<std::int32_t>(from.activeReticleType);
		std::memcpy(to.vStandMove, from.vStandMove, sizeof(to.vStandMove));
		std::memcpy(to.vStandRot, from.vStandRot, sizeof(to.vStandRot));
		std::memcpy(to.strafeMove, from.strafeMove, sizeof(to.strafeMove));
		std::memcpy(to.strafeRot, from.strafeRot, sizeof(to.strafeRot));
		std::memcpy(to.vDuckedOfs, from.vDuckedOfs, sizeof(to.vDuckedOfs));
		std::memcpy(to.vDuckedMove, from.vDuckedMove, sizeof(to.vDuckedMove));
		std::memcpy(to.vDuckedRot, from.vDuckedRot, sizeof(to.vDuckedRot));
		std::memcpy(to.vProneOfs, from.vProneOfs, sizeof(to.vProneOfs));
		std::memcpy(to.vProneMove, from.vProneMove, sizeof(to.vProneMove));
		std::memcpy(to.vProneRot, from.vProneRot, sizeof(to.vProneRot));
		to.fPosMoveRate = static_cast<float>(from.fPosMoveRate);
		to.fPosProneMoveRate = static_cast<float>(from.fPosProneMoveRate);
		to.fStandMoveMinSpeed = static_cast<float>(from.fStandMoveMinSpeed);
		to.fDuckedMoveMinSpeed = static_cast<float>(from.fDuckedMoveMinSpeed);
		to.fProneMoveMinSpeed = static_cast<float>(from.fProneMoveMinSpeed);
		to.fPosRotRate = static_cast<float>(from.fPosRotRate);
		to.fPosProneRotRate = static_cast<float>(from.fPosProneRotRate);
		to.fStandRotMinSpeed = static_cast<float>(from.fStandRotMinSpeed);
		to.fDuckedRotMinSpeed = static_cast<float>(from.fDuckedRotMinSpeed);
		to.fProneRotMinSpeed = static_cast<float>(from.fProneRotMinSpeed);
		to.hudIconRatio = static_cast<std::int32_t>(from.hudIconRatio);
		to.pickupIconRatio = static_cast<std::int32_t>(from.pickupIconRatio);
		to.ammoCounterIconRatio = static_cast<std::int32_t>(from.ammoCounterIconRatio);
		to.ammoCounterClip = static_cast<std::int32_t>(from.ammoCounterClip);
		to.iStartAmmo = static_cast<std::int32_t>(from.iStartAmmo);
		to.iAmmoIndex = static_cast<std::int32_t>(from.iAmmoIndex);
		to.iClipIndex = static_cast<std::int32_t>(from.iClipIndex);
		to.iMaxAmmo = static_cast<std::int32_t>(from.iMaxAmmo);
		to.shotCount = static_cast<std::int32_t>(from.shotCount);
		to.iSharedAmmoCapIndex = static_cast<std::int32_t>(from.iSharedAmmoCapIndex);
		to.iSharedAmmoCap = static_cast<std::int32_t>(from.iSharedAmmoCap);
		to.damage = static_cast<std::int32_t>(from.damage);
		to.playerDamage = static_cast<std::int32_t>(from.playerDamage);
		to.iMeleeDamage = static_cast<std::int32_t>(from.iMeleeDamage);
		to.iDamageType = static_cast<std::int32_t>(from.iDamageType);
		to.iFireDelay = static_cast<std::int32_t>(from.iFireDelay);
		to.iMeleeDelay = static_cast<std::int32_t>(from.iMeleeDelay);
		to.meleeChargeDelay = static_cast<std::int32_t>(from.meleeChargeDelay);
		to.iDetonateDelay = static_cast<std::int32_t>(from.iDetonateDelay);
		to.iRechamberTime = static_cast<std::int32_t>(from.iRechamberTime);
		to.rechamberTimeOneHanded = static_cast<std::int32_t>(from.rechamberTimeOneHanded);
		to.iRechamberBoltTime = static_cast<std::int32_t>(from.iRechamberBoltTime);
		to.iHoldFireTime = static_cast<std::int32_t>(from.iHoldFireTime);
		to.iDetonateTime = static_cast<std::int32_t>(from.iDetonateTime);
		to.iMeleeTime = static_cast<std::int32_t>(from.iMeleeTime);
		to.meleeChargeTime = static_cast<std::int32_t>(from.meleeChargeTime);
		to.iReloadTime = static_cast<std::int32_t>(from.iReloadTime);
		to.reloadShowRocketTime = static_cast<std::int32_t>(from.reloadShowRocketTime);
		to.iReloadEmptyTime = static_cast<std::int32_t>(from.iReloadEmptyTime);
		to.iReloadAddTime = static_cast<std::int32_t>(from.iReloadAddTime);
		to.iReloadStartTime = static_cast<std::int32_t>(from.iReloadStartTime);
		to.iReloadStartAddTime = static_cast<std::int32_t>(from.iReloadStartAddTime);
		to.iReloadEndTime = static_cast<std::int32_t>(from.iReloadEndTime);
		to.iDropTime = static_cast<std::int32_t>(from.iDropTime);
		to.iRaiseTime = static_cast<std::int32_t>(from.iRaiseTime);
		to.iAltDropTime = static_cast<std::int32_t>(from.iAltDropTime);
		to.quickDropTime = static_cast<std::int32_t>(from.quickDropTime);
		to.quickRaiseTime = static_cast<std::int32_t>(from.quickRaiseTime);
		to.iBreachRaiseTime = static_cast<std::int32_t>(from.iBreachRaiseTime);
		to.iEmptyRaiseTime = static_cast<std::int32_t>(from.iEmptyRaiseTime);
		to.iEmptyDropTime = static_cast<std::int32_t>(from.iEmptyDropTime);
		to.sprintInTime = static_cast<std::int32_t>(from.sprintInTime);
		to.sprintLoopTime = static_cast<std::int32_t>(from.sprintLoopTime);
		to.sprintOutTime = static_cast<std::int32_t>(from.sprintOutTime);
		to.stunnedTimeBegin = static_cast<std::int32_t>(from.stunnedTimeBegin);
		to.stunnedTimeLoop = static_cast<std::int32_t>(from.stunnedTimeLoop);
		to.stunnedTimeEnd = static_cast<std::int32_t>(from.stunnedTimeEnd);
		to.nightVisionWearTime = static_cast<std::int32_t>(from.nightVisionWearTime);
		to.nightVisionWearTimeFadeOutEnd = static_cast<std::int32_t>(from.nightVisionWearTimeFadeOutEnd);
		to.nightVisionWearTimePowerUp = static_cast<std::int32_t>(from.nightVisionWearTimePowerUp);
		to.nightVisionRemoveTime = static_cast<std::int32_t>(from.nightVisionRemoveTime);
		to.nightVisionRemoveTimePowerDown = static_cast<std::int32_t>(from.nightVisionRemoveTimePowerDown);
		to.nightVisionRemoveTimeFadeInStart = static_cast<std::int32_t>(from.nightVisionRemoveTimeFadeInStart);
		to.fuseTime = static_cast<std::int32_t>(from.fuseTime);
		to.aiFuseTime = static_cast<std::int32_t>(from.aiFuseTime);
		to.autoAimRange = static_cast<float>(from.autoAimRange);
		to.aimAssistRange = static_cast<float>(from.aimAssistRange);
		to.aimAssistRangeAds = static_cast<float>(from.aimAssistRangeAds);
		to.aimPadding = static_cast<float>(from.aimPadding);
		to.enemyCrosshairRange = static_cast<float>(from.enemyCrosshairRange);
		to.moveSpeedScale = static_cast<float>(from.moveSpeedScale);
		to.adsMoveSpeedScale = static_cast<float>(from.adsMoveSpeedScale);
		to.sprintDurationScale = static_cast<float>(from.sprintDurationScale);
		to.fAdsZoomInFrac = static_cast<float>(from.fAdsZoomInFrac);
		to.fAdsZoomOutFrac = static_cast<float>(from.fAdsZoomOutFrac);
		to.overlayReticle = static_cast<std::int32_t>(from.overlayReticle);
		to.overlayInterface = static_cast<std::int32_t>(from.overlayInterface);
		to.overlayWidth = static_cast<float>(from.overlayWidth);
		to.overlayHeight = static_cast<float>(from.overlayHeight);
		to.overlayWidthSplitscreen = static_cast<float>(from.overlayWidthSplitscreen);
		to.overlayHeightSplitscreen = static_cast<float>(from.overlayHeightSplitscreen);
		to.fAdsBobFactor = static_cast<float>(from.fAdsBobFactor);
		to.fAdsViewBobMult = static_cast<float>(from.fAdsViewBobMult);
		to.fHipSpreadStandMin = static_cast<float>(from.fHipSpreadStandMin);
		to.fHipSpreadDuckedMin = static_cast<float>(from.fHipSpreadDuckedMin);
		to.fHipSpreadProneMin = static_cast<float>(from.fHipSpreadProneMin);
		to.hipSpreadStandMax = static_cast<float>(from.hipSpreadStandMax);
		to.hipSpreadDuckedMax = static_cast<float>(from.hipSpreadDuckedMax);
		to.hipSpreadProneMax = static_cast<float>(from.hipSpreadProneMax);
		to.fHipSpreadDecayRate = static_cast<float>(from.fHipSpreadDecayRate);
		to.fHipSpreadFireAdd = static_cast<float>(from.fHipSpreadFireAdd);
		to.fHipSpreadTurnAdd = static_cast<float>(from.fHipSpreadTurnAdd);
		to.fHipSpreadMoveAdd = static_cast<float>(from.fHipSpreadMoveAdd);
		to.fHipSpreadDuckedDecay = static_cast<float>(from.fHipSpreadDuckedDecay);
		to.fHipSpreadProneDecay = static_cast<float>(from.fHipSpreadProneDecay);
		to.fHipReticleSidePos = static_cast<float>(from.fHipReticleSidePos);
		to.fAdsIdleAmount = static_cast<float>(from.fAdsIdleAmount);
		to.fHipIdleAmount = static_cast<float>(from.fHipIdleAmount);
		to.adsIdleSpeed = static_cast<float>(from.adsIdleSpeed);
		to.hipIdleSpeed = static_cast<float>(from.hipIdleSpeed);
		to.fIdleCrouchFactor = static_cast<float>(from.fIdleCrouchFactor);
		to.fIdleProneFactor = static_cast<float>(from.fIdleProneFactor);
		to.fGunMaxPitch = static_cast<float>(from.fGunMaxPitch);
		to.fGunMaxYaw = static_cast<float>(from.fGunMaxYaw);
		to.swayMaxAngle = static_cast<float>(from.swayMaxAngle);
		to.swayLerpSpeed = static_cast<float>(from.swayLerpSpeed);
		to.swayPitchScale = static_cast<float>(from.swayPitchScale);
		to.swayYawScale = static_cast<float>(from.swayYawScale);
		to.swayHorizScale = static_cast<float>(from.swayHorizScale);
		to.swayVertScale = static_cast<float>(from.swayVertScale);
		to.swayShellShockScale = static_cast<float>(from.swayShellShockScale);
		to.adsSwayMaxAngle = static_cast<float>(from.adsSwayMaxAngle);
		to.adsSwayLerpSpeed = static_cast<float>(from.adsSwayLerpSpeed);
		to.adsSwayPitchScale = static_cast<float>(from.adsSwayPitchScale);
		to.adsSwayYawScale = static_cast<float>(from.adsSwayYawScale);
		to.adsSwayHorizScale = static_cast<float>(from.adsSwayHorizScale);
		to.adsSwayVertScale = static_cast<float>(from.adsSwayVertScale);
		to.adsViewErrorMin = static_cast<float>(from.adsViewErrorMin);
		to.adsViewErrorMax = static_cast<float>(from.adsViewErrorMax);
		to.dualWieldViewModelOffset = static_cast<float>(from.dualWieldViewModelOffset);
		to.killIconRatio = static_cast<std::int32_t>(from.killIconRatio);
		to.iReloadAmmoAdd = static_cast<std::int32_t>(from.iReloadAmmoAdd);
		to.iReloadStartAdd = static_cast<std::int32_t>(from.iReloadStartAdd);
		to.ammoDropStockMin = static_cast<std::int32_t>(from.ammoDropStockMin);
		to.ammoDropClipPercentMin = static_cast<std::int32_t>(from.ammoDropClipPercentMin);
		to.ammoDropClipPercentMax = static_cast<std::int32_t>(from.ammoDropClipPercentMax);
		to.iExplosionRadius = static_cast<std::int32_t>(from.iExplosionRadius);
		to.iExplosionRadiusMin = static_cast<std::int32_t>(from.iExplosionRadiusMin);
		to.iExplosionInnerDamage = static_cast<std::int32_t>(from.iExplosionInnerDamage);
		to.iExplosionOuterDamage = static_cast<std::int32_t>(from.iExplosionOuterDamage);
		to.damageConeAngle = static_cast<float>(from.damageConeAngle);
		to.bulletExplDmgMult = static_cast<float>(from.bulletExplDmgMult);
		to.bulletExplRadiusMult = static_cast<float>(from.bulletExplRadiusMult);
		to.iProjectileSpeed = static_cast<std::int32_t>(from.iProjectileSpeed);
		to.iProjectileSpeedUp = static_cast<std::int32_t>(from.iProjectileSpeedUp);
		to.iProjectileSpeedForward = static_cast<std::int32_t>(from.iProjectileSpeedForward);
		to.iProjectileActivateDist = static_cast<std::int32_t>(from.iProjectileActivateDist);
		to.projLifetime = static_cast<float>(from.projLifetime);
		to.timeToAccelerate = static_cast<float>(from.timeToAccelerate);
		to.projectileCurvature = static_cast<float>(from.projectileCurvature);
		to.projExplosion = static_cast<std::int32_t>(from.projExplosion);
		to.stickiness = static_cast<std::int32_t>(from.stickiness);
		to.lowAmmoWarningThreshold = static_cast<float>(from.lowAmmoWarningThreshold);
		to.ricochetChance = static_cast<float>(from.ricochetChance);
		std::memcpy(to.vProjectileColor, from.vProjectileColor, sizeof(to.vProjectileColor));
		to.guidedMissileType = static_cast<std::int32_t>(from.guidedMissileType);
		to.maxSteeringAccel = static_cast<float>(from.maxSteeringAccel);
		to.projIgnitionDelay = static_cast<std::int32_t>(from.projIgnitionDelay);
		to.fAdsAimPitch = static_cast<float>(from.fAdsAimPitch);
		to.fAdsCrosshairInFrac = static_cast<float>(from.fAdsCrosshairInFrac);
		to.fAdsCrosshairOutFrac = static_cast<float>(from.fAdsCrosshairOutFrac);
		to.adsGunKickReducedKickBullets = static_cast<std::int32_t>(from.adsGunKickReducedKickBullets);
		to.adsGunKickReducedKickPercent = static_cast<float>(from.adsGunKickReducedKickPercent);
		to.fAdsGunKickPitchMin = static_cast<float>(from.fAdsGunKickPitchMin);
		to.fAdsGunKickPitchMax = static_cast<float>(from.fAdsGunKickPitchMax);
		to.fAdsGunKickYawMin = static_cast<float>(from.fAdsGunKickYawMin);
		to.fAdsGunKickYawMax = static_cast<float>(from.fAdsGunKickYawMax);
		to.fAdsGunKickAccel = static_cast<float>(from.fAdsGunKickAccel);
		to.fAdsGunKickSpeedMax = static_cast<float>(from.fAdsGunKickSpeedMax);
		to.fAdsGunKickSpeedDecay = static_cast<float>(from.fAdsGunKickSpeedDecay);
		to.fAdsGunKickStaticDecay = static_cast<float>(from.fAdsGunKickStaticDecay);
		to.fAdsViewKickPitchMin = static_cast<float>(from.fAdsViewKickPitchMin);
		to.fAdsViewKickPitchMax = static_cast<float>(from.fAdsViewKickPitchMax);
		to.fAdsViewKickYawMin = static_cast<float>(from.fAdsViewKickYawMin);
		to.fAdsViewKickYawMax = static_cast<float>(from.fAdsViewKickYawMax);
		to.fAdsViewScatterMin = static_cast<float>(from.fAdsViewScatterMin);
		to.fAdsViewScatterMax = static_cast<float>(from.fAdsViewScatterMax);
		to.fAdsSpread = static_cast<float>(from.fAdsSpread);
		to.hipGunKickReducedKickBullets = static_cast<std::int32_t>(from.hipGunKickReducedKickBullets);
		to.hipGunKickReducedKickPercent = static_cast<float>(from.hipGunKickReducedKickPercent);
		to.fHipGunKickPitchMin = static_cast<float>(from.fHipGunKickPitchMin);
		to.fHipGunKickPitchMax = static_cast<float>(from.fHipGunKickPitchMax);
		to.fHipGunKickYawMin = static_cast<float>(from.fHipGunKickYawMin);
		to.fHipGunKickYawMax = static_cast<float>(from.fHipGunKickYawMax);
		to.fHipGunKickAccel = static_cast<float>(from.fHipGunKickAccel);
		to.fHipGunKickSpeedMax = static_cast<float>(from.fHipGunKickSpeedMax);
		to.fHipGunKickSpeedDecay = static_cast<float>(from.fHipGunKickSpeedDecay);
		to.fHipGunKickStaticDecay = static_cast<float>(from.fHipGunKickStaticDecay);
		to.fHipViewKickPitchMin = static_cast<float>(from.fHipViewKickPitchMin);
		to.fHipViewKickPitchMax = static_cast<float>(from.fHipViewKickPitchMax);
		to.fHipViewKickYawMin = static_cast<float>(from.fHipViewKickYawMin);
		to.fHipViewKickYawMax = static_cast<float>(from.fHipViewKickYawMax);
		to.fHipViewScatterMin = static_cast<float>(from.fHipViewScatterMin);
		to.fHipViewScatterMax = static_cast<float>(from.fHipViewScatterMax);
		to.fightDist = static_cast<float>(from.fightDist);
		to.maxDist = static_cast<float>(from.maxDist);
		std::memcpy(to.originalAccuracyGraphKnotCount, from.originalAccuracyGraphKnotCount, sizeof(to.originalAccuracyGraphKnotCount));
		to.iPositionReloadTransTime = static_cast<std::int32_t>(from.iPositionReloadTransTime);
		to.leftArc = static_cast<float>(from.leftArc);
		to.rightArc = static_cast<float>(from.rightArc);
		to.topArc = static_cast<float>(from.topArc);
		to.bottomArc = static_cast<float>(from.bottomArc);
		to.accuracy = static_cast<float>(from.accuracy);
		to.aiSpread = static_cast<float>(from.aiSpread);
		to.playerSpread = static_cast<float>(from.playerSpread);
		std::memcpy(to.minTurnSpeed, from.minTurnSpeed, sizeof(to.minTurnSpeed));
		std::memcpy(to.maxTurnSpeed, from.maxTurnSpeed, sizeof(to.maxTurnSpeed));
		to.pitchConvergenceTime = static_cast<float>(from.pitchConvergenceTime);
		to.yawConvergenceTime = static_cast<float>(from.yawConvergenceTime);
		to.suppressTime = static_cast<float>(from.suppressTime);
		to.maxRange = static_cast<float>(from.maxRange);
		to.fAnimHorRotateInc = static_cast<float>(from.fAnimHorRotateInc);
		to.fPlayerPositionDist = static_cast<float>(from.fPlayerPositionDist);
		to.iUseHintStringIndex = static_cast<std::int32_t>(from.iUseHintStringIndex);
		to.dropHintStringIndex = static_cast<std::int32_t>(from.dropHintStringIndex);
		to.horizViewJitter = static_cast<float>(from.horizViewJitter);
		to.vertViewJitter = static_cast<float>(from.vertViewJitter);
		to.scanSpeed = static_cast<float>(from.scanSpeed);
		to.scanAccel = static_cast<float>(from.scanAccel);
		to.scanPauseTime = static_cast<std::int32_t>(from.scanPauseTime);
		std::memcpy(to.fOOPosAnimLength, from.fOOPosAnimLength, sizeof(to.fOOPosAnimLength));
		to.minDamage = static_cast<std::int32_t>(from.minDamage);
		to.minPlayerDamage = static_cast<std::int32_t>(from.minPlayerDamage);
		to.fMaxDamageRange = static_cast<float>(from.fMaxDamageRange);
		to.fMinDamageRange = static_cast<float>(from.fMinDamageRange);
		to.destabilizationRateTime = static_cast<float>(from.destabilizationRateTime);
		to.destabilizationCurvatureMax = static_cast<float>(from.destabilizationCurvatureMax);
		to.destabilizeDistance = static_cast<std::int32_t>(from.destabilizeDistance);
		to.turretScopeZoomRate = static_cast<float>(from.turretScopeZoomRate);
		to.turretScopeZoomMin = static_cast<float>(from.turretScopeZoomMin);
		to.turretScopeZoomMax = static_cast<float>(from.turretScopeZoomMax);
		to.turretOverheatUpRate = static_cast<float>(from.turretOverheatUpRate);
		to.turretOverheatDownRate = static_cast<float>(from.turretOverheatDownRate);
		to.turretOverheatPenalty = static_cast<float>(from.turretOverheatPenalty);
		to.turretBarrelSpinSpeed = static_cast<float>(from.turretBarrelSpinSpeed);
		to.turretBarrelSpinUpTime = static_cast<float>(from.turretBarrelSpinUpTime);
		to.turretBarrelSpinDownTime = static_cast<float>(from.turretBarrelSpinDownTime);
		to.missileConeSoundRadiusAtTop = static_cast<float>(from.missileConeSoundRadiusAtTop);
		to.missileConeSoundRadiusAtBase = static_cast<float>(from.missileConeSoundRadiusAtBase);
		to.missileConeSoundHeight = static_cast<float>(from.missileConeSoundHeight);
		to.missileConeSoundOriginOffset = static_cast<float>(from.missileConeSoundOriginOffset);
		to.missileConeSoundVolumescaleAtCore = static_cast<float>(from.missileConeSoundVolumescaleAtCore);
		to.missileConeSoundVolumescaleAtEdge = static_cast<float>(from.missileConeSoundVolumescaleAtEdge);
		to.missileConeSoundVolumescaleCoreSize = static_cast<float>(from.missileConeSoundVolumescaleCoreSize);
		to.missileConeSoundPitchAtTop = static_cast<float>(from.missileConeSoundPitchAtTop);
		to.missileConeSoundPitchAtBottom = static_cast<float>(from.missileConeSoundPitchAtBottom);
		to.missileConeSoundPitchTopSize = static_cast<float>(from.missileConeSoundPitchTopSize);
		to.missileConeSoundPitchBottomSize = static_cast<float>(from.missileConeSoundPitchBottomSize);
		to.missileConeSoundCrossfadeTopSize = static_cast<float>(from.missileConeSoundCrossfadeTopSize);
		to.missileConeSoundCrossfadeBottomSize = static_cast<float>(from.missileConeSoundCrossfadeBottomSize);
		to.sharedAmmo = static_cast<bool>(from.sharedAmmo);
		to.lockonSupported = static_cast<bool>(from.lockonSupported);
		to.requireLockonToFire = static_cast<bool>(from.requireLockonToFire);
		to.bigExplosion = static_cast<bool>(from.bigExplosion);
		to.noAdsWhenMagEmpty = static_cast<bool>(from.noAdsWhenMagEmpty);
		to.avoidDropCleanup = static_cast<bool>(from.avoidDropCleanup);
		to.inheritsPerks = static_cast<bool>(from.inheritsPerks);
		to.crosshairColorChange = static_cast<bool>(from.crosshairColorChange);
		to.bRifleBullet = static_cast<bool>(from.bRifleBullet);
		to.armorPiercing = static_cast<bool>(from.armorPiercing);
		to.bBoltAction = static_cast<bool>(from.bBoltAction);
		to.aimDownSight = static_cast<bool>(from.aimDownSight);
		to.bRechamberWhileAds = static_cast<bool>(from.bRechamberWhileAds);
		to.bBulletExplosiveDamage = static_cast<bool>(from.bBulletExplosiveDamage);
		to.bCookOffHold = static_cast<bool>(from.bCookOffHold);
		to.bClipOnly = static_cast<bool>(from.bClipOnly);
		to.noAmmoPickup = static_cast<bool>(from.noAmmoPickup);
		to.adsFireOnly = static_cast<bool>(from.adsFireOnly);
		to.cancelAutoHolsterWhenEmpty = static_cast<bool>(from.cancelAutoHolsterWhenEmpty);
		to.disableSwitchToWhenEmpty = static_cast<bool>(from.disableSwitchToWhenEmpty);
		to.suppressAmmoReserveDisplay = static_cast<bool>(from.suppressAmmoReserveDisplay);
		to.laserSightDuringNightvision = static_cast<bool>(from.laserSightDuringNightvision);
		to.markableViewmodel = static_cast<bool>(from.markableViewmodel);
		to.noDualWield = static_cast<bool>(from.noDualWield);
		to.flipKillIcon = static_cast<bool>(from.flipKillIcon);
		to.bNoPartialReload = static_cast<bool>(from.bNoPartialReload);
		to.bSegmentedReload = static_cast<bool>(from.bSegmentedReload);
		to.blocksProne = static_cast<bool>(from.blocksProne);
		to.silenced = static_cast<bool>(from.silenced);
		to.isRollingGrenade = static_cast<bool>(from.isRollingGrenade);
		to.projExplosionEffectForceNormalUp = static_cast<bool>(from.projExplosionEffectForceNormalUp);
		to.bProjImpactExplode = static_cast<bool>(from.bProjImpactExplode);
		to.stickToPlayers = static_cast<bool>(from.stickToPlayers);
		to.hasDetonator = static_cast<bool>(from.hasDetonator);
		to.disableFiring = static_cast<bool>(from.disableFiring);
		to.timedDetonation = static_cast<bool>(from.timedDetonation);
		to.rotate = static_cast<bool>(from.rotate);
		to.holdButtonToThrow = static_cast<bool>(from.holdButtonToThrow);
		to.freezeMovementWhenFiring = static_cast<bool>(from.freezeMovementWhenFiring);
		to.thermalScope = static_cast<bool>(from.thermalScope);
		to.altModeSameWeapon = static_cast<bool>(from.altModeSameWeapon);
		to.turretBarrelSpinEnabled = static_cast<bool>(from.turretBarrelSpinEnabled);
		to.missileConeSoundEnabled = static_cast<bool>(from.missileConeSoundEnabled);
		to.missileConeSoundPitchshiftEnabled = static_cast<bool>(from.missileConeSoundPitchshiftEnabled);
		to.missileConeSoundCrossfadeEnabled = static_cast<bool>(from.missileConeSoundCrossfadeEnabled);
		to.offhandHoldIsCancelable = static_cast<bool>(from.offhandHoldIsCancelable);
		return to;
	}

	inline Game::WeaponDef Convert(const WeaponDef& from)
	{
		Game::WeaponDef to{};
		to.playerAnimType = static_cast<decltype(to.playerAnimType)>(from.playerAnimType);
		to.weapType = static_cast<decltype(to.weapType)>(from.weapType);
		to.weapClass = static_cast<decltype(to.weapClass)>(from.weapClass);
		to.penetrateType = static_cast<decltype(to.penetrateType)>(from.penetrateType);
		to.inventoryType = static_cast<decltype(to.inventoryType)>(from.inventoryType);
		to.fireType = static_cast<decltype(to.fireType)>(from.fireType);
		to.offhandClass = static_cast<decltype(to.offhandClass)>(from.offhandClass);
		to.stance = static_cast<decltype(to.stance)>(from.stance);
		to.iReticleCenterSize = static_cast<decltype(to.iReticleCenterSize)>(from.iReticleCenterSize);
		to.iReticleSideSize = static_cast<decltype(to.iReticleSideSize)>(from.iReticleSideSize);
		to.iReticleMinOfs = static_cast<decltype(to.iReticleMinOfs)>(from.iReticleMinOfs);
		to.activeReticleType = static_cast<decltype(to.activeReticleType)>(from.activeReticleType);
		std::memcpy(to.vStandMove, from.vStandMove, sizeof(from.vStandMove));
		std::memcpy(to.vStandRot, from.vStandRot, sizeof(from.vStandRot));
		std::memcpy(to.strafeMove, from.strafeMove, sizeof(from.strafeMove));
		std::memcpy(to.strafeRot, from.strafeRot, sizeof(from.strafeRot));
		std::memcpy(to.vDuckedOfs, from.vDuckedOfs, sizeof(from.vDuckedOfs));
		std::memcpy(to.vDuckedMove, from.vDuckedMove, sizeof(from.vDuckedMove));
		std::memcpy(to.vDuckedRot, from.vDuckedRot, sizeof(from.vDuckedRot));
		std::memcpy(to.vProneOfs, from.vProneOfs, sizeof(from.vProneOfs));
		std::memcpy(to.vProneMove, from.vProneMove, sizeof(from.vProneMove));
		std::memcpy(to.vProneRot, from.vProneRot, sizeof(from.vProneRot));
		to.fPosMoveRate = static_cast<decltype(to.fPosMoveRate)>(from.fPosMoveRate);
		to.fPosProneMoveRate = static_cast<decltype(to.fPosProneMoveRate)>(from.fPosProneMoveRate);
		to.fStandMoveMinSpeed = static_cast<decltype(to.fStandMoveMinSpeed)>(from.fStandMoveMinSpeed);
		to.fDuckedMoveMinSpeed = static_cast<decltype(to.fDuckedMoveMinSpeed)>(from.fDuckedMoveMinSpeed);
		to.fProneMoveMinSpeed = static_cast<decltype(to.fProneMoveMinSpeed)>(from.fProneMoveMinSpeed);
		to.fPosRotRate = static_cast<decltype(to.fPosRotRate)>(from.fPosRotRate);
		to.fPosProneRotRate = static_cast<decltype(to.fPosProneRotRate)>(from.fPosProneRotRate);
		to.fStandRotMinSpeed = static_cast<decltype(to.fStandRotMinSpeed)>(from.fStandRotMinSpeed);
		to.fDuckedRotMinSpeed = static_cast<decltype(to.fDuckedRotMinSpeed)>(from.fDuckedRotMinSpeed);
		to.fProneRotMinSpeed = static_cast<decltype(to.fProneRotMinSpeed)>(from.fProneRotMinSpeed);
		to.hudIconRatio = static_cast<decltype(to.hudIconRatio)>(from.hudIconRatio);
		to.pickupIconRatio = static_cast<decltype(to.pickupIconRatio)>(from.pickupIconRatio);
		to.ammoCounterIconRatio = static_cast<decltype(to.ammoCounterIconRatio)>(from.ammoCounterIconRatio);
		to.ammoCounterClip = static_cast<decltype(to.ammoCounterClip)>(from.ammoCounterClip);
		to.iStartAmmo = static_cast<decltype(to.iStartAmmo)>(from.iStartAmmo);
		to.iAmmoIndex = static_cast<decltype(to.iAmmoIndex)>(from.iAmmoIndex);
		to.iClipIndex = static_cast<decltype(to.iClipIndex)>(from.iClipIndex);
		to.iMaxAmmo = static_cast<decltype(to.iMaxAmmo)>(from.iMaxAmmo);
		to.shotCount = static_cast<decltype(to.shotCount)>(from.shotCount);
		to.iSharedAmmoCapIndex = static_cast<decltype(to.iSharedAmmoCapIndex)>(from.iSharedAmmoCapIndex);
		to.iSharedAmmoCap = static_cast<decltype(to.iSharedAmmoCap)>(from.iSharedAmmoCap);
		to.damage = static_cast<decltype(to.damage)>(from.damage);
		to.playerDamage = static_cast<decltype(to.playerDamage)>(from.playerDamage);
		to.iMeleeDamage = static_cast<decltype(to.iMeleeDamage)>(from.iMeleeDamage);
		to.iDamageType = static_cast<decltype(to.iDamageType)>(from.iDamageType);
		to.iFireDelay = static_cast<decltype(to.iFireDelay)>(from.iFireDelay);
		to.iMeleeDelay = static_cast<decltype(to.iMeleeDelay)>(from.iMeleeDelay);
		to.meleeChargeDelay = static_cast<decltype(to.meleeChargeDelay)>(from.meleeChargeDelay);
		to.iDetonateDelay = static_cast<decltype(to.iDetonateDelay)>(from.iDetonateDelay);
		to.iRechamberTime = static_cast<decltype(to.iRechamberTime)>(from.iRechamberTime);
		to.rechamberTimeOneHanded = static_cast<decltype(to.rechamberTimeOneHanded)>(from.rechamberTimeOneHanded);
		to.iRechamberBoltTime = static_cast<decltype(to.iRechamberBoltTime)>(from.iRechamberBoltTime);
		to.iHoldFireTime = static_cast<decltype(to.iHoldFireTime)>(from.iHoldFireTime);
		to.iDetonateTime = static_cast<decltype(to.iDetonateTime)>(from.iDetonateTime);
		to.iMeleeTime = static_cast<decltype(to.iMeleeTime)>(from.iMeleeTime);
		to.meleeChargeTime = static_cast<decltype(to.meleeChargeTime)>(from.meleeChargeTime);
		to.iReloadTime = static_cast<decltype(to.iReloadTime)>(from.iReloadTime);
		to.reloadShowRocketTime = static_cast<decltype(to.reloadShowRocketTime)>(from.reloadShowRocketTime);
		to.iReloadEmptyTime = static_cast<decltype(to.iReloadEmptyTime)>(from.iReloadEmptyTime);
		to.iReloadAddTime = static_cast<decltype(to.iReloadAddTime)>(from.iReloadAddTime);
		to.iReloadStartTime = static_cast<decltype(to.iReloadStartTime)>(from.iReloadStartTime);
		to.iReloadStartAddTime = static_cast<decltype(to.iReloadStartAddTime)>(from.iReloadStartAddTime);
		to.iReloadEndTime = static_cast<decltype(to.iReloadEndTime)>(from.iReloadEndTime);
		to.iDropTime = static_cast<decltype(to.iDropTime)>(from.iDropTime);
		to.iRaiseTime = static_cast<decltype(to.iRaiseTime)>(from.iRaiseTime);
		to.iAltDropTime = static_cast<decltype(to.iAltDropTime)>(from.iAltDropTime);
		to.quickDropTime = static_cast<decltype(to.quickDropTime)>(from.quickDropTime);
		to.quickRaiseTime = static_cast<decltype(to.quickRaiseTime)>(from.quickRaiseTime);
		to.iBreachRaiseTime = static_cast<decltype(to.iBreachRaiseTime)>(from.iBreachRaiseTime);
		to.iEmptyRaiseTime = static_cast<decltype(to.iEmptyRaiseTime)>(from.iEmptyRaiseTime);
		to.iEmptyDropTime = static_cast<decltype(to.iEmptyDropTime)>(from.iEmptyDropTime);
		to.sprintInTime = static_cast<decltype(to.sprintInTime)>(from.sprintInTime);
		to.sprintLoopTime = static_cast<decltype(to.sprintLoopTime)>(from.sprintLoopTime);
		to.sprintOutTime = static_cast<decltype(to.sprintOutTime)>(from.sprintOutTime);
		to.stunnedTimeBegin = static_cast<decltype(to.stunnedTimeBegin)>(from.stunnedTimeBegin);
		to.stunnedTimeLoop = static_cast<decltype(to.stunnedTimeLoop)>(from.stunnedTimeLoop);
		to.stunnedTimeEnd = static_cast<decltype(to.stunnedTimeEnd)>(from.stunnedTimeEnd);
		to.nightVisionWearTime = static_cast<decltype(to.nightVisionWearTime)>(from.nightVisionWearTime);
		to.nightVisionWearTimeFadeOutEnd = static_cast<decltype(to.nightVisionWearTimeFadeOutEnd)>(from.nightVisionWearTimeFadeOutEnd);
		to.nightVisionWearTimePowerUp = static_cast<decltype(to.nightVisionWearTimePowerUp)>(from.nightVisionWearTimePowerUp);
		to.nightVisionRemoveTime = static_cast<decltype(to.nightVisionRemoveTime)>(from.nightVisionRemoveTime);
		to.nightVisionRemoveTimePowerDown = static_cast<decltype(to.nightVisionRemoveTimePowerDown)>(from.nightVisionRemoveTimePowerDown);
		to.nightVisionRemoveTimeFadeInStart = static_cast<decltype(to.nightVisionRemoveTimeFadeInStart)>(from.nightVisionRemoveTimeFadeInStart);
		to.fuseTime = static_cast<decltype(to.fuseTime)>(from.fuseTime);
		to.aiFuseTime = static_cast<decltype(to.aiFuseTime)>(from.aiFuseTime);
		to.autoAimRange = static_cast<decltype(to.autoAimRange)>(from.autoAimRange);
		to.aimAssistRange = static_cast<decltype(to.aimAssistRange)>(from.aimAssistRange);
		to.aimAssistRangeAds = static_cast<decltype(to.aimAssistRangeAds)>(from.aimAssistRangeAds);
		to.aimPadding = static_cast<decltype(to.aimPadding)>(from.aimPadding);
		to.enemyCrosshairRange = static_cast<decltype(to.enemyCrosshairRange)>(from.enemyCrosshairRange);
		to.moveSpeedScale = static_cast<decltype(to.moveSpeedScale)>(from.moveSpeedScale);
		to.adsMoveSpeedScale = static_cast<decltype(to.adsMoveSpeedScale)>(from.adsMoveSpeedScale);
		to.sprintDurationScale = static_cast<decltype(to.sprintDurationScale)>(from.sprintDurationScale);
		to.fAdsZoomInFrac = static_cast<decltype(to.fAdsZoomInFrac)>(from.fAdsZoomInFrac);
		to.fAdsZoomOutFrac = static_cast<decltype(to.fAdsZoomOutFrac)>(from.fAdsZoomOutFrac);
		to.overlayReticle = static_cast<decltype(to.overlayReticle)>(from.overlayReticle);
		to.overlayInterface = static_cast<decltype(to.overlayInterface)>(from.overlayInterface);
		to.overlayWidth = static_cast<decltype(to.overlayWidth)>(from.overlayWidth);
		to.overlayHeight = static_cast<decltype(to.overlayHeight)>(from.overlayHeight);
		to.overlayWidthSplitscreen = static_cast<decltype(to.overlayWidthSplitscreen)>(from.overlayWidthSplitscreen);
		to.overlayHeightSplitscreen = static_cast<decltype(to.overlayHeightSplitscreen)>(from.overlayHeightSplitscreen);
		to.fAdsBobFactor = static_cast<decltype(to.fAdsBobFactor)>(from.fAdsBobFactor);
		to.fAdsViewBobMult = static_cast<decltype(to.fAdsViewBobMult)>(from.fAdsViewBobMult);
		to.fHipSpreadStandMin = static_cast<decltype(to.fHipSpreadStandMin)>(from.fHipSpreadStandMin);
		to.fHipSpreadDuckedMin = static_cast<decltype(to.fHipSpreadDuckedMin)>(from.fHipSpreadDuckedMin);
		to.fHipSpreadProneMin = static_cast<decltype(to.fHipSpreadProneMin)>(from.fHipSpreadProneMin);
		to.hipSpreadStandMax = static_cast<decltype(to.hipSpreadStandMax)>(from.hipSpreadStandMax);
		to.hipSpreadDuckedMax = static_cast<decltype(to.hipSpreadDuckedMax)>(from.hipSpreadDuckedMax);
		to.hipSpreadProneMax = static_cast<decltype(to.hipSpreadProneMax)>(from.hipSpreadProneMax);
		to.fHipSpreadDecayRate = static_cast<decltype(to.fHipSpreadDecayRate)>(from.fHipSpreadDecayRate);
		to.fHipSpreadFireAdd = static_cast<decltype(to.fHipSpreadFireAdd)>(from.fHipSpreadFireAdd);
		to.fHipSpreadTurnAdd = static_cast<decltype(to.fHipSpreadTurnAdd)>(from.fHipSpreadTurnAdd);
		to.fHipSpreadMoveAdd = static_cast<decltype(to.fHipSpreadMoveAdd)>(from.fHipSpreadMoveAdd);
		to.fHipSpreadDuckedDecay = static_cast<decltype(to.fHipSpreadDuckedDecay)>(from.fHipSpreadDuckedDecay);
		to.fHipSpreadProneDecay = static_cast<decltype(to.fHipSpreadProneDecay)>(from.fHipSpreadProneDecay);
		to.fHipReticleSidePos = static_cast<decltype(to.fHipReticleSidePos)>(from.fHipReticleSidePos);
		to.fAdsIdleAmount = static_cast<decltype(to.fAdsIdleAmount)>(from.fAdsIdleAmount);
		to.fHipIdleAmount = static_cast<decltype(to.fHipIdleAmount)>(from.fHipIdleAmount);
		to.adsIdleSpeed = static_cast<decltype(to.adsIdleSpeed)>(from.adsIdleSpeed);
		to.hipIdleSpeed = static_cast<decltype(to.hipIdleSpeed)>(from.hipIdleSpeed);
		to.fIdleCrouchFactor = static_cast<decltype(to.fIdleCrouchFactor)>(from.fIdleCrouchFactor);
		to.fIdleProneFactor = static_cast<decltype(to.fIdleProneFactor)>(from.fIdleProneFactor);
		to.fGunMaxPitch = static_cast<decltype(to.fGunMaxPitch)>(from.fGunMaxPitch);
		to.fGunMaxYaw = static_cast<decltype(to.fGunMaxYaw)>(from.fGunMaxYaw);
		to.swayMaxAngle = static_cast<decltype(to.swayMaxAngle)>(from.swayMaxAngle);
		to.swayLerpSpeed = static_cast<decltype(to.swayLerpSpeed)>(from.swayLerpSpeed);
		to.swayPitchScale = static_cast<decltype(to.swayPitchScale)>(from.swayPitchScale);
		to.swayYawScale = static_cast<decltype(to.swayYawScale)>(from.swayYawScale);
		to.swayHorizScale = static_cast<decltype(to.swayHorizScale)>(from.swayHorizScale);
		to.swayVertScale = static_cast<decltype(to.swayVertScale)>(from.swayVertScale);
		to.swayShellShockScale = static_cast<decltype(to.swayShellShockScale)>(from.swayShellShockScale);
		to.adsSwayMaxAngle = static_cast<decltype(to.adsSwayMaxAngle)>(from.adsSwayMaxAngle);
		to.adsSwayLerpSpeed = static_cast<decltype(to.adsSwayLerpSpeed)>(from.adsSwayLerpSpeed);
		to.adsSwayPitchScale = static_cast<decltype(to.adsSwayPitchScale)>(from.adsSwayPitchScale);
		to.adsSwayYawScale = static_cast<decltype(to.adsSwayYawScale)>(from.adsSwayYawScale);
		to.adsSwayHorizScale = static_cast<decltype(to.adsSwayHorizScale)>(from.adsSwayHorizScale);
		to.adsSwayVertScale = static_cast<decltype(to.adsSwayVertScale)>(from.adsSwayVertScale);
		to.adsViewErrorMin = static_cast<decltype(to.adsViewErrorMin)>(from.adsViewErrorMin);
		to.adsViewErrorMax = static_cast<decltype(to.adsViewErrorMax)>(from.adsViewErrorMax);
		to.dualWieldViewModelOffset = static_cast<decltype(to.dualWieldViewModelOffset)>(from.dualWieldViewModelOffset);
		to.killIconRatio = static_cast<decltype(to.killIconRatio)>(from.killIconRatio);
		to.iReloadAmmoAdd = static_cast<decltype(to.iReloadAmmoAdd)>(from.iReloadAmmoAdd);
		to.iReloadStartAdd = static_cast<decltype(to.iReloadStartAdd)>(from.iReloadStartAdd);
		to.ammoDropStockMin = static_cast<decltype(to.ammoDropStockMin)>(from.ammoDropStockMin);
		to.ammoDropClipPercentMin = static_cast<decltype(to.ammoDropClipPercentMin)>(from.ammoDropClipPercentMin);
		to.ammoDropClipPercentMax = static_cast<decltype(to.ammoDropClipPercentMax)>(from.ammoDropClipPercentMax);
		to.iExplosionRadius = static_cast<decltype(to.iExplosionRadius)>(from.iExplosionRadius);
		to.iExplosionRadiusMin = static_cast<decltype(to.iExplosionRadiusMin)>(from.iExplosionRadiusMin);
		to.iExplosionInnerDamage = static_cast<decltype(to.iExplosionInnerDamage)>(from.iExplosionInnerDamage);
		to.iExplosionOuterDamage = static_cast<decltype(to.iExplosionOuterDamage)>(from.iExplosionOuterDamage);
		to.damageConeAngle = static_cast<decltype(to.damageConeAngle)>(from.damageConeAngle);
		to.bulletExplDmgMult = static_cast<decltype(to.bulletExplDmgMult)>(from.bulletExplDmgMult);
		to.bulletExplRadiusMult = static_cast<decltype(to.bulletExplRadiusMult)>(from.bulletExplRadiusMult);
		to.iProjectileSpeed = static_cast<decltype(to.iProjectileSpeed)>(from.iProjectileSpeed);
		to.iProjectileSpeedUp = static_cast<decltype(to.iProjectileSpeedUp)>(from.iProjectileSpeedUp);
		to.iProjectileSpeedForward = static_cast<decltype(to.iProjectileSpeedForward)>(from.iProjectileSpeedForward);
		to.iProjectileActivateDist = static_cast<decltype(to.iProjectileActivateDist)>(from.iProjectileActivateDist);
		to.projLifetime = static_cast<decltype(to.projLifetime)>(from.projLifetime);
		to.timeToAccelerate = static_cast<decltype(to.timeToAccelerate)>(from.timeToAccelerate);
		to.projectileCurvature = static_cast<decltype(to.projectileCurvature)>(from.projectileCurvature);
		to.projExplosion = static_cast<decltype(to.projExplosion)>(from.projExplosion);
		to.stickiness = static_cast<decltype(to.stickiness)>(from.stickiness);
		to.lowAmmoWarningThreshold = static_cast<decltype(to.lowAmmoWarningThreshold)>(from.lowAmmoWarningThreshold);
		to.ricochetChance = static_cast<decltype(to.ricochetChance)>(from.ricochetChance);
		std::memcpy(to.vProjectileColor, from.vProjectileColor, sizeof(from.vProjectileColor));
		to.guidedMissileType = static_cast<decltype(to.guidedMissileType)>(from.guidedMissileType);
		to.maxSteeringAccel = static_cast<decltype(to.maxSteeringAccel)>(from.maxSteeringAccel);
		to.projIgnitionDelay = static_cast<decltype(to.projIgnitionDelay)>(from.projIgnitionDelay);
		to.fAdsAimPitch = static_cast<decltype(to.fAdsAimPitch)>(from.fAdsAimPitch);
		to.fAdsCrosshairInFrac = static_cast<decltype(to.fAdsCrosshairInFrac)>(from.fAdsCrosshairInFrac);
		to.fAdsCrosshairOutFrac = static_cast<decltype(to.fAdsCrosshairOutFrac)>(from.fAdsCrosshairOutFrac);
		to.adsGunKickReducedKickBullets = static_cast<decltype(to.adsGunKickReducedKickBullets)>(from.adsGunKickReducedKickBullets);
		to.adsGunKickReducedKickPercent = static_cast<decltype(to.adsGunKickReducedKickPercent)>(from.adsGunKickReducedKickPercent);
		to.fAdsGunKickPitchMin = static_cast<decltype(to.fAdsGunKickPitchMin)>(from.fAdsGunKickPitchMin);
		to.fAdsGunKickPitchMax = static_cast<decltype(to.fAdsGunKickPitchMax)>(from.fAdsGunKickPitchMax);
		to.fAdsGunKickYawMin = static_cast<decltype(to.fAdsGunKickYawMin)>(from.fAdsGunKickYawMin);
		to.fAdsGunKickYawMax = static_cast<decltype(to.fAdsGunKickYawMax)>(from.fAdsGunKickYawMax);
		to.fAdsGunKickAccel = static_cast<decltype(to.fAdsGunKickAccel)>(from.fAdsGunKickAccel);
		to.fAdsGunKickSpeedMax = static_cast<decltype(to.fAdsGunKickSpeedMax)>(from.fAdsGunKickSpeedMax);
		to.fAdsGunKickSpeedDecay = static_cast<decltype(to.fAdsGunKickSpeedDecay)>(from.fAdsGunKickSpeedDecay);
		to.fAdsGunKickStaticDecay = static_cast<decltype(to.fAdsGunKickStaticDecay)>(from.fAdsGunKickStaticDecay);
		to.fAdsViewKickPitchMin = static_cast<decltype(to.fAdsViewKickPitchMin)>(from.fAdsViewKickPitchMin);
		to.fAdsViewKickPitchMax = static_cast<decltype(to.fAdsViewKickPitchMax)>(from.fAdsViewKickPitchMax);
		to.fAdsViewKickYawMin = static_cast<decltype(to.fAdsViewKickYawMin)>(from.fAdsViewKickYawMin);
		to.fAdsViewKickYawMax = static_cast<decltype(to.fAdsViewKickYawMax)>(from.fAdsViewKickYawMax);
		to.fAdsViewScatterMin = static_cast<decltype(to.fAdsViewScatterMin)>(from.fAdsViewScatterMin);
		to.fAdsViewScatterMax = static_cast<decltype(to.fAdsViewScatterMax)>(from.fAdsViewScatterMax);
		to.fAdsSpread = static_cast<decltype(to.fAdsSpread)>(from.fAdsSpread);
		to.hipGunKickReducedKickBullets = static_cast<decltype(to.hipGunKickReducedKickBullets)>(from.hipGunKickReducedKickBullets);
		to.hipGunKickReducedKickPercent = static_cast<decltype(to.hipGunKickReducedKickPercent)>(from.hipGunKickReducedKickPercent);
		to.fHipGunKickPitchMin = static_cast<decltype(to.fHipGunKickPitchMin)>(from.fHipGunKickPitchMin);
		to.fHipGunKickPitchMax = static_cast<decltype(to.fHipGunKickPitchMax)>(from.fHipGunKickPitchMax);
		to.fHipGunKickYawMin = static_cast<decltype(to.fHipGunKickYawMin)>(from.fHipGunKickYawMin);
		to.fHipGunKickYawMax = static_cast<decltype(to.fHipGunKickYawMax)>(from.fHipGunKickYawMax);
		to.fHipGunKickAccel = static_cast<decltype(to.fHipGunKickAccel)>(from.fHipGunKickAccel);
		to.fHipGunKickSpeedMax = static_cast<decltype(to.fHipGunKickSpeedMax)>(from.fHipGunKickSpeedMax);
		to.fHipGunKickSpeedDecay = static_cast<decltype(to.fHipGunKickSpeedDecay)>(from.fHipGunKickSpeedDecay);
		to.fHipGunKickStaticDecay = static_cast<decltype(to.fHipGunKickStaticDecay)>(from.fHipGunKickStaticDecay);
		to.fHipViewKickPitchMin = static_cast<decltype(to.fHipViewKickPitchMin)>(from.fHipViewKickPitchMin);
		to.fHipViewKickPitchMax = static_cast<decltype(to.fHipViewKickPitchMax)>(from.fHipViewKickPitchMax);
		to.fHipViewKickYawMin = static_cast<decltype(to.fHipViewKickYawMin)>(from.fHipViewKickYawMin);
		to.fHipViewKickYawMax = static_cast<decltype(to.fHipViewKickYawMax)>(from.fHipViewKickYawMax);
		to.fHipViewScatterMin = static_cast<decltype(to.fHipViewScatterMin)>(from.fHipViewScatterMin);
		to.fHipViewScatterMax = static_cast<decltype(to.fHipViewScatterMax)>(from.fHipViewScatterMax);
		to.fightDist = static_cast<decltype(to.fightDist)>(from.fightDist);
		to.maxDist = static_cast<decltype(to.maxDist)>(from.maxDist);
		std::memcpy(to.originalAccuracyGraphKnotCount, from.originalAccuracyGraphKnotCount, sizeof(from.originalAccuracyGraphKnotCount));
		to.iPositionReloadTransTime = static_cast<decltype(to.iPositionReloadTransTime)>(from.iPositionReloadTransTime);
		to.leftArc = static_cast<decltype(to.leftArc)>(from.leftArc);
		to.rightArc = static_cast<decltype(to.rightArc)>(from.rightArc);
		to.topArc = static_cast<decltype(to.topArc)>(from.topArc);
		to.bottomArc = static_cast<decltype(to.bottomArc)>(from.bottomArc);
		to.accuracy = static_cast<decltype(to.accuracy)>(from.accuracy);
		to.aiSpread = static_cast<decltype(to.aiSpread)>(from.aiSpread);
		to.playerSpread = static_cast<decltype(to.playerSpread)>(from.playerSpread);
		std::memcpy(to.minTurnSpeed, from.minTurnSpeed, sizeof(from.minTurnSpeed));
		std::memcpy(to.maxTurnSpeed, from.maxTurnSpeed, sizeof(from.maxTurnSpeed));
		to.pitchConvergenceTime = static_cast<decltype(to.pitchConvergenceTime)>(from.pitchConvergenceTime);
		to.yawConvergenceTime = static_cast<decltype(to.yawConvergenceTime)>(from.yawConvergenceTime);
		to.suppressTime = static_cast<decltype(to.suppressTime)>(from.suppressTime);
		to.maxRange = static_cast<decltype(to.maxRange)>(from.maxRange);
		to.fAnimHorRotateInc = static_cast<decltype(to.fAnimHorRotateInc)>(from.fAnimHorRotateInc);
		to.fPlayerPositionDist = static_cast<decltype(to.fPlayerPositionDist)>(from.fPlayerPositionDist);
		to.iUseHintStringIndex = static_cast<decltype(to.iUseHintStringIndex)>(from.iUseHintStringIndex);
		to.dropHintStringIndex = static_cast<decltype(to.dropHintStringIndex)>(from.dropHintStringIndex);
		to.horizViewJitter = static_cast<decltype(to.horizViewJitter)>(from.horizViewJitter);
		to.vertViewJitter = static_cast<decltype(to.vertViewJitter)>(from.vertViewJitter);
		to.scanSpeed = static_cast<decltype(to.scanSpeed)>(from.scanSpeed);
		to.scanAccel = static_cast<decltype(to.scanAccel)>(from.scanAccel);
		to.scanPauseTime = static_cast<decltype(to.scanPauseTime)>(from.scanPauseTime);
		std::memcpy(to.fOOPosAnimLength, from.fOOPosAnimLength, sizeof(from.fOOPosAnimLength));
		to.minDamage = static_cast<decltype(to.minDamage)>(from.minDamage);
		to.minPlayerDamage = static_cast<decltype(to.minPlayerDamage)>(from.minPlayerDamage);
		to.fMaxDamageRange = static_cast<decltype(to.fMaxDamageRange)>(from.fMaxDamageRange);
		to.fMinDamageRange = static_cast<decltype(to.fMinDamageRange)>(from.fMinDamageRange);
		to.destabilizationRateTime = static_cast<decltype(to.destabilizationRateTime)>(from.destabilizationRateTime);
		to.destabilizationCurvatureMax = static_cast<decltype(to.destabilizationCurvatureMax)>(from.destabilizationCurvatureMax);
		to.destabilizeDistance = static_cast<decltype(to.destabilizeDistance)>(from.destabilizeDistance);
		to.turretScopeZoomRate = static_cast<decltype(to.turretScopeZoomRate)>(from.turretScopeZoomRate);
		to.turretScopeZoomMin = static_cast<decltype(to.turretScopeZoomMin)>(from.turretScopeZoomMin);
		to.turretScopeZoomMax = static_cast<decltype(to.turretScopeZoomMax)>(from.turretScopeZoomMax);
		to.turretOverheatUpRate = static_cast<decltype(to.turretOverheatUpRate)>(from.turretOverheatUpRate);
		to.turretOverheatDownRate = static_cast<decltype(to.turretOverheatDownRate)>(from.turretOverheatDownRate);
		to.turretOverheatPenalty = static_cast<decltype(to.turretOverheatPenalty)>(from.turretOverheatPenalty);
		to.turretBarrelSpinSpeed = static_cast<decltype(to.turretBarrelSpinSpeed)>(from.turretBarrelSpinSpeed);
		to.turretBarrelSpinUpTime = static_cast<decltype(to.turretBarrelSpinUpTime)>(from.turretBarrelSpinUpTime);
		to.turretBarrelSpinDownTime = static_cast<decltype(to.turretBarrelSpinDownTime)>(from.turretBarrelSpinDownTime);
		to.missileConeSoundRadiusAtTop = static_cast<decltype(to.missileConeSoundRadiusAtTop)>(from.missileConeSoundRadiusAtTop);
		to.missileConeSoundRadiusAtBase = static_cast<decltype(to.missileConeSoundRadiusAtBase)>(from.missileConeSoundRadiusAtBase);
		to.missileConeSoundHeight = static_cast<decltype(to.missileConeSoundHeight)>(from.missileConeSoundHeight);
		to.missileConeSoundOriginOffset = static_cast<decltype(to.missileConeSoundOriginOffset)>(from.missileConeSoundOriginOffset);
		to.missileConeSoundVolumescaleAtCore = static_cast<decltype(to.missileConeSoundVolumescaleAtCore)>(from.missileConeSoundVolumescaleAtCore);
		to.missileConeSoundVolumescaleAtEdge = static_cast<decltype(to.missileConeSoundVolumescaleAtEdge)>(from.missileConeSoundVolumescaleAtEdge);
		to.missileConeSoundVolumescaleCoreSize = static_cast<decltype(to.missileConeSoundVolumescaleCoreSize)>(from.missileConeSoundVolumescaleCoreSize);
		to.missileConeSoundPitchAtTop = static_cast<decltype(to.missileConeSoundPitchAtTop)>(from.missileConeSoundPitchAtTop);
		to.missileConeSoundPitchAtBottom = static_cast<decltype(to.missileConeSoundPitchAtBottom)>(from.missileConeSoundPitchAtBottom);
		to.missileConeSoundPitchTopSize = static_cast<decltype(to.missileConeSoundPitchTopSize)>(from.missileConeSoundPitchTopSize);
		to.missileConeSoundPitchBottomSize = static_cast<decltype(to.missileConeSoundPitchBottomSize)>(from.missileConeSoundPitchBottomSize);
		to.missileConeSoundCrossfadeTopSize = static_cast<decltype(to.missileConeSoundCrossfadeTopSize)>(from.missileConeSoundCrossfadeTopSize);
		to.missileConeSoundCrossfadeBottomSize = static_cast<decltype(to.missileConeSoundCrossfadeBottomSize)>(from.missileConeSoundCrossfadeBottomSize);
		to.sharedAmmo = static_cast<decltype(to.sharedAmmo)>(from.sharedAmmo);
		to.lockonSupported = static_cast<decltype(to.lockonSupported)>(from.lockonSupported);
		to.requireLockonToFire = static_cast<decltype(to.requireLockonToFire)>(from.requireLockonToFire);
		to.bigExplosion = static_cast<decltype(to.bigExplosion)>(from.bigExplosion);
		to.noAdsWhenMagEmpty = static_cast<decltype(to.noAdsWhenMagEmpty)>(from.noAdsWhenMagEmpty);
		to.avoidDropCleanup = static_cast<decltype(to.avoidDropCleanup)>(from.avoidDropCleanup);
		to.inheritsPerks = static_cast<decltype(to.inheritsPerks)>(from.inheritsPerks);
		to.crosshairColorChange = static_cast<decltype(to.crosshairColorChange)>(from.crosshairColorChange);
		to.bRifleBullet = static_cast<decltype(to.bRifleBullet)>(from.bRifleBullet);
		to.armorPiercing = static_cast<decltype(to.armorPiercing)>(from.armorPiercing);
		to.bBoltAction = static_cast<decltype(to.bBoltAction)>(from.bBoltAction);
		to.aimDownSight = static_cast<decltype(to.aimDownSight)>(from.aimDownSight);
		to.bRechamberWhileAds = static_cast<decltype(to.bRechamberWhileAds)>(from.bRechamberWhileAds);
		to.bBulletExplosiveDamage = static_cast<decltype(to.bBulletExplosiveDamage)>(from.bBulletExplosiveDamage);
		to.bCookOffHold = static_cast<decltype(to.bCookOffHold)>(from.bCookOffHold);
		to.bClipOnly = static_cast<decltype(to.bClipOnly)>(from.bClipOnly);
		to.noAmmoPickup = static_cast<decltype(to.noAmmoPickup)>(from.noAmmoPickup);
		to.adsFireOnly = static_cast<decltype(to.adsFireOnly)>(from.adsFireOnly);
		to.cancelAutoHolsterWhenEmpty = static_cast<decltype(to.cancelAutoHolsterWhenEmpty)>(from.cancelAutoHolsterWhenEmpty);
		to.disableSwitchToWhenEmpty = static_cast<decltype(to.disableSwitchToWhenEmpty)>(from.disableSwitchToWhenEmpty);
		to.suppressAmmoReserveDisplay = static_cast<decltype(to.suppressAmmoReserveDisplay)>(from.suppressAmmoReserveDisplay);
		to.laserSightDuringNightvision = static_cast<decltype(to.laserSightDuringNightvision)>(from.laserSightDuringNightvision);
		to.markableViewmodel = static_cast<decltype(to.markableViewmodel)>(from.markableViewmodel);
		to.noDualWield = static_cast<decltype(to.noDualWield)>(from.noDualWield);
		to.flipKillIcon = static_cast<decltype(to.flipKillIcon)>(from.flipKillIcon);
		to.bNoPartialReload = static_cast<decltype(to.bNoPartialReload)>(from.bNoPartialReload);
		to.bSegmentedReload = static_cast<decltype(to.bSegmentedReload)>(from.bSegmentedReload);
		to.blocksProne = static_cast<decltype(to.blocksProne)>(from.blocksProne);
		to.silenced = static_cast<decltype(to.silenced)>(from.silenced);
		to.isRollingGrenade = static_cast<decltype(to.isRollingGrenade)>(from.isRollingGrenade);
		to.projExplosionEffectForceNormalUp = static_cast<decltype(to.projExplosionEffectForceNormalUp)>(from.projExplosionEffectForceNormalUp);
		to.bProjImpactExplode = static_cast<decltype(to.bProjImpactExplode)>(from.bProjImpactExplode);
		to.stickToPlayers = static_cast<decltype(to.stickToPlayers)>(from.stickToPlayers);
		to.hasDetonator = static_cast<decltype(to.hasDetonator)>(from.hasDetonator);
		to.disableFiring = static_cast<decltype(to.disableFiring)>(from.disableFiring);
		to.timedDetonation = static_cast<decltype(to.timedDetonation)>(from.timedDetonation);
		to.rotate = static_cast<decltype(to.rotate)>(from.rotate);
		to.holdButtonToThrow = static_cast<decltype(to.holdButtonToThrow)>(from.holdButtonToThrow);
		to.freezeMovementWhenFiring = static_cast<decltype(to.freezeMovementWhenFiring)>(from.freezeMovementWhenFiring);
		to.thermalScope = static_cast<decltype(to.thermalScope)>(from.thermalScope);
		to.altModeSameWeapon = static_cast<decltype(to.altModeSameWeapon)>(from.altModeSameWeapon);
		to.turretBarrelSpinEnabled = static_cast<decltype(to.turretBarrelSpinEnabled)>(from.turretBarrelSpinEnabled);
		to.missileConeSoundEnabled = static_cast<decltype(to.missileConeSoundEnabled)>(from.missileConeSoundEnabled);
		to.missileConeSoundPitchshiftEnabled = static_cast<decltype(to.missileConeSoundPitchshiftEnabled)>(from.missileConeSoundPitchshiftEnabled);
		to.missileConeSoundCrossfadeEnabled = static_cast<decltype(to.missileConeSoundCrossfadeEnabled)>(from.missileConeSoundCrossfadeEnabled);
		to.offhandHoldIsCancelable = static_cast<decltype(to.offhandHoldIsCancelable)>(from.offhandHoldIsCancelable);
		return to;
	}

	struct TracerDef
	{
		std::uint32_t name;
		std::uint32_t material;
		std::uint32_t drawInterval;
		float speed;
		float beamLength;
		float beamWidth;
		float screwRadius;
		float screwDist;
		float colors[5][4];
	};

	static_assert(sizeof(TracerDef) == 0x70);
	static_assert(offsetof(TracerDef, name) == 0x0);
	static_assert(offsetof(TracerDef, material) == 0x4);
	static_assert(offsetof(TracerDef, drawInterval) == 0x8);
	static_assert(offsetof(TracerDef, speed) == 0xC);
	static_assert(offsetof(TracerDef, beamLength) == 0x10);
	static_assert(offsetof(TracerDef, beamWidth) == 0x14);
	static_assert(offsetof(TracerDef, screwRadius) == 0x18);
	static_assert(offsetof(TracerDef, screwDist) == 0x1C);
	static_assert(offsetof(TracerDef, colors) == 0x20);

	inline TracerDef Convert(const Game::TracerDef& from)
	{
		TracerDef to{};
		to.drawInterval = static_cast<std::uint32_t>(from.drawInterval);
		to.speed = static_cast<float>(from.speed);
		to.beamLength = static_cast<float>(from.beamLength);
		to.beamWidth = static_cast<float>(from.beamWidth);
		to.screwRadius = static_cast<float>(from.screwRadius);
		to.screwDist = static_cast<float>(from.screwDist);
		std::memcpy(to.colors, from.colors, sizeof(to.colors));
		return to;
	}

	inline Game::TracerDef Convert(const TracerDef& from)
	{
		Game::TracerDef to{};
		to.drawInterval = static_cast<decltype(to.drawInterval)>(from.drawInterval);
		to.speed = static_cast<decltype(to.speed)>(from.speed);
		to.beamLength = static_cast<decltype(to.beamLength)>(from.beamLength);
		to.beamWidth = static_cast<decltype(to.beamWidth)>(from.beamWidth);
		to.screwRadius = static_cast<decltype(to.screwRadius)>(from.screwRadius);
		to.screwDist = static_cast<decltype(to.screwDist)>(from.screwDist);
		std::memcpy(to.colors, from.colors, sizeof(from.colors));
		return to;
	}

	struct SndDriverGlobals
	{
		std::uint32_t name;
	};

	static_assert(sizeof(SndDriverGlobals) == 0x4);
	static_assert(offsetof(SndDriverGlobals, name) == 0x0);

	inline SndDriverGlobals Convert(const Game::SndDriverGlobals&)
	{
		SndDriverGlobals to{};
		return to;
	}

	inline Game::SndDriverGlobals Convert(const SndDriverGlobals&)
	{
		Game::SndDriverGlobals to{};
		return to;
	}

	struct FxImpactTable
	{
		std::uint32_t name;
		std::uint32_t table;
	};

	static_assert(sizeof(FxImpactTable) == 0x8);
	static_assert(offsetof(FxImpactTable, name) == 0x0);
	static_assert(offsetof(FxImpactTable, table) == 0x4);

	inline FxImpactTable Convert(const Game::FxImpactTable&)
	{
		FxImpactTable to{};
		return to;
	}

	inline Game::FxImpactTable Convert(const FxImpactTable&)
	{
		Game::FxImpactTable to{};
		return to;
	}

	struct FxImpactEntry
	{
		std::uint32_t nonflesh[31];
		std::uint32_t flesh[4];
	};

	static_assert(sizeof(FxImpactEntry) == 0x8C);
	static_assert(offsetof(FxImpactEntry, nonflesh) == 0x0);
	static_assert(offsetof(FxImpactEntry, flesh) == 0x7C);

	inline FxImpactEntry Convert(const Game::FxImpactEntry&)
	{
		FxImpactEntry to{};
		return to;
	}

	inline Game::FxImpactEntry Convert(const FxImpactEntry&)
	{
		Game::FxImpactEntry to{};
		return to;
	}

	struct RawFile
	{
		std::uint32_t name;
		std::int32_t compressedLen;
		std::int32_t len;
		std::uint32_t buffer;
	};

	static_assert(sizeof(RawFile) == 0x10);
	static_assert(offsetof(RawFile, name) == 0x0);
	static_assert(offsetof(RawFile, compressedLen) == 0x4);
	static_assert(offsetof(RawFile, len) == 0x8);
	static_assert(offsetof(RawFile, buffer) == 0xC);

	inline RawFile Convert(const Game::RawFile& from)
	{
		RawFile to{};
		to.compressedLen = static_cast<std::int32_t>(from.compressedLen);
		to.len = static_cast<std::int32_t>(from.len);
		return to;
	}

	inline Game::RawFile Convert(const RawFile& from)
	{
		Game::RawFile to{};
		to.compressedLen = static_cast<decltype(to.compressedLen)>(from.compressedLen);
		to.len = static_cast<decltype(to.len)>(from.len);
		return to;
	}

	struct StringTable
	{
		std::uint32_t name;
		std::int32_t columnCount;
		std::int32_t rowCount;
		std::uint32_t values;
	};

	static_assert(sizeof(StringTable) == 0x10);
	static_assert(offsetof(StringTable, name) == 0x0);
	static_assert(offsetof(StringTable, columnCount) == 0x4);
	static_assert(offsetof(StringTable, rowCount) == 0x8);
	static_assert(offsetof(StringTable, values) == 0xC);

	inline StringTable Convert(const Game::StringTable& from)
	{
		StringTable to{};
		to.columnCount = static_cast<std::int32_t>(from.columnCount);
		to.rowCount = static_cast<std::int32_t>(from.rowCount);
		return to;
	}

	inline Game::StringTable Convert(const StringTable& from)
	{
		Game::StringTable to{};
		to.columnCount = static_cast<decltype(to.columnCount)>(from.columnCount);
		to.rowCount = static_cast<decltype(to.rowCount)>(from.rowCount);
		return to;
	}

	struct StringTableCell
	{
		std::uint32_t string;
		std::int32_t hash;
	};

	static_assert(sizeof(StringTableCell) == 0x8);
	static_assert(offsetof(StringTableCell, string) == 0x0);
	static_assert(offsetof(StringTableCell, hash) == 0x4);

	inline StringTableCell Convert(const Game::StringTableCell& from)
	{
		StringTableCell to{};
		to.hash = static_cast<std::int32_t>(from.hash);
		return to;
	}

	inline Game::StringTableCell Convert(const StringTableCell& from)
	{
		Game::StringTableCell to{};
		to.hash = static_cast<decltype(to.hash)>(from.hash);
		return to;
	}

	struct LeaderboardDef
	{
		std::uint32_t name;
		std::int32_t id;
		std::int32_t columnCount;
		std::int32_t xpColId;
		std::int32_t prestigeColId;
		std::uint32_t columns;
	};

	static_assert(sizeof(LeaderboardDef) == 0x18);
	static_assert(offsetof(LeaderboardDef, name) == 0x0);
	static_assert(offsetof(LeaderboardDef, id) == 0x4);
	static_assert(offsetof(LeaderboardDef, columnCount) == 0x8);
	static_assert(offsetof(LeaderboardDef, xpColId) == 0xC);
	static_assert(offsetof(LeaderboardDef, prestigeColId) == 0x10);
	static_assert(offsetof(LeaderboardDef, columns) == 0x14);

	inline LeaderboardDef Convert(const Game::LeaderboardDef& from)
	{
		LeaderboardDef to{};
		to.id = static_cast<std::int32_t>(from.id);
		to.columnCount = static_cast<std::int32_t>(from.columnCount);
		to.xpColId = static_cast<std::int32_t>(from.xpColId);
		to.prestigeColId = static_cast<std::int32_t>(from.prestigeColId);
		return to;
	}

	inline Game::LeaderboardDef Convert(const LeaderboardDef& from)
	{
		Game::LeaderboardDef to{};
		to.id = static_cast<decltype(to.id)>(from.id);
		to.columnCount = static_cast<decltype(to.columnCount)>(from.columnCount);
		to.xpColId = static_cast<decltype(to.xpColId)>(from.xpColId);
		to.prestigeColId = static_cast<decltype(to.prestigeColId)>(from.prestigeColId);
		return to;
	}

	struct LbColumnDef
	{
		std::uint32_t name;
		std::int32_t id;
		std::int32_t propertyId;
		bool hidden;
		std::uint32_t statName;
		std::int32_t type;
		std::int32_t precision;
		std::int32_t agg;
	};

	static_assert(sizeof(LbColumnDef) == 0x20);
	static_assert(offsetof(LbColumnDef, name) == 0x0);
	static_assert(offsetof(LbColumnDef, id) == 0x4);
	static_assert(offsetof(LbColumnDef, propertyId) == 0x8);
	static_assert(offsetof(LbColumnDef, hidden) == 0xC);
	static_assert(offsetof(LbColumnDef, statName) == 0x10);
	static_assert(offsetof(LbColumnDef, type) == 0x14);
	static_assert(offsetof(LbColumnDef, precision) == 0x18);
	static_assert(offsetof(LbColumnDef, agg) == 0x1C);

	inline LbColumnDef Convert(const Game::LbColumnDef& from)
	{
		LbColumnDef to{};
		to.id = static_cast<std::int32_t>(from.id);
		to.propertyId = static_cast<std::int32_t>(from.propertyId);
		to.hidden = static_cast<bool>(from.hidden);
		to.type = static_cast<std::int32_t>(from.type);
		to.precision = static_cast<std::int32_t>(from.precision);
		to.agg = static_cast<std::int32_t>(from.agg);
		return to;
	}

	inline Game::LbColumnDef Convert(const LbColumnDef& from)
	{
		Game::LbColumnDef to{};
		to.id = static_cast<decltype(to.id)>(from.id);
		to.propertyId = static_cast<decltype(to.propertyId)>(from.propertyId);
		to.hidden = static_cast<decltype(to.hidden)>(from.hidden);
		to.type = static_cast<decltype(to.type)>(from.type);
		to.precision = static_cast<decltype(to.precision)>(from.precision);
		to.agg = static_cast<decltype(to.agg)>(from.agg);
		return to;
	}

	struct StructuredDataDefSet
	{
		std::uint32_t name;
		std::uint32_t defCount;
		std::uint32_t defs;
	};

	static_assert(sizeof(StructuredDataDefSet) == 0xC);
	static_assert(offsetof(StructuredDataDefSet, name) == 0x0);
	static_assert(offsetof(StructuredDataDefSet, defCount) == 0x4);
	static_assert(offsetof(StructuredDataDefSet, defs) == 0x8);

	inline StructuredDataDefSet Convert(const Game::StructuredDataDefSet& from)
	{
		StructuredDataDefSet to{};
		to.defCount = static_cast<std::uint32_t>(from.defCount);
		return to;
	}

	inline Game::StructuredDataDefSet Convert(const StructuredDataDefSet& from)
	{
		Game::StructuredDataDefSet to{};
		to.defCount = static_cast<decltype(to.defCount)>(from.defCount);
		return to;
	}

	union StructuredDataTypeUnion
	{
		std::uint32_t stringDataLength;
		std::int32_t enumIndex;
		std::int32_t structIndex;
		std::int32_t indexedArrayIndex;
		std::int32_t enumedArrayIndex;
	};

	static_assert(sizeof(StructuredDataTypeUnion) == 0x4);
	static_assert(offsetof(StructuredDataTypeUnion, stringDataLength) == 0x0);
	static_assert(offsetof(StructuredDataTypeUnion, enumIndex) == 0x0);
	static_assert(offsetof(StructuredDataTypeUnion, structIndex) == 0x0);
	static_assert(offsetof(StructuredDataTypeUnion, indexedArrayIndex) == 0x0);
	static_assert(offsetof(StructuredDataTypeUnion, enumedArrayIndex) == 0x0);

	inline StructuredDataTypeUnion Convert(const Game::StructuredDataTypeUnion& from)
	{
		StructuredDataTypeUnion to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::StructuredDataTypeUnion Convert(const StructuredDataTypeUnion& from)
	{
		Game::StructuredDataTypeUnion to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct StructuredDataType
	{
		std::int32_t type;
		StructuredDataTypeUnion u;
	};

	static_assert(sizeof(StructuredDataType) == 0x8);
	static_assert(offsetof(StructuredDataType, type) == 0x0);
	static_assert(offsetof(StructuredDataType, u) == 0x4);

	inline StructuredDataType Convert(const Game::StructuredDataType& from)
	{
		StructuredDataType to{};
		to.type = static_cast<std::int32_t>(from.type);
		to.u = Convert(from.u);
		return to;
	}

	inline Game::StructuredDataType Convert(const StructuredDataType& from)
	{
		Game::StructuredDataType to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.u = Convert(from.u);
		return to;
	}

	struct StructuredDataDef
	{
		std::int32_t version;
		std::uint32_t formatChecksum;
		std::int32_t enumCount;
		std::uint32_t enums;
		std::int32_t structCount;
		std::uint32_t structs;
		std::int32_t indexedArrayCount;
		std::uint32_t indexedArrays;
		std::int32_t enumedArrayCount;
		std::uint32_t enumedArrays;
		StructuredDataType rootType;
		std::uint32_t size;
	};

	static_assert(sizeof(StructuredDataDef) == 0x34);
	static_assert(offsetof(StructuredDataDef, version) == 0x0);
	static_assert(offsetof(StructuredDataDef, formatChecksum) == 0x4);
	static_assert(offsetof(StructuredDataDef, enumCount) == 0x8);
	static_assert(offsetof(StructuredDataDef, enums) == 0xC);
	static_assert(offsetof(StructuredDataDef, structCount) == 0x10);
	static_assert(offsetof(StructuredDataDef, structs) == 0x14);
	static_assert(offsetof(StructuredDataDef, indexedArrayCount) == 0x18);
	static_assert(offsetof(StructuredDataDef, indexedArrays) == 0x1C);
	static_assert(offsetof(StructuredDataDef, enumedArrayCount) == 0x20);
	static_assert(offsetof(StructuredDataDef, enumedArrays) == 0x24);
	static_assert(offsetof(StructuredDataDef, rootType) == 0x28);
	static_assert(offsetof(StructuredDataDef, size) == 0x30);

	inline StructuredDataDef Convert(const Game::StructuredDataDef& from)
	{
		StructuredDataDef to{};
		to.version = static_cast<std::int32_t>(from.version);
		to.formatChecksum = static_cast<std::uint32_t>(from.formatChecksum);
		to.enumCount = static_cast<std::int32_t>(from.enumCount);
		to.structCount = static_cast<std::int32_t>(from.structCount);
		to.indexedArrayCount = static_cast<std::int32_t>(from.indexedArrayCount);
		to.enumedArrayCount = static_cast<std::int32_t>(from.enumedArrayCount);
		to.rootType = Convert(from.rootType);
		to.size = static_cast<std::uint32_t>(from.size);
		return to;
	}

	inline Game::StructuredDataDef Convert(const StructuredDataDef& from)
	{
		Game::StructuredDataDef to{};
		to.version = static_cast<decltype(to.version)>(from.version);
		to.formatChecksum = static_cast<decltype(to.formatChecksum)>(from.formatChecksum);
		to.enumCount = static_cast<decltype(to.enumCount)>(from.enumCount);
		to.structCount = static_cast<decltype(to.structCount)>(from.structCount);
		to.indexedArrayCount = static_cast<decltype(to.indexedArrayCount)>(from.indexedArrayCount);
		to.enumedArrayCount = static_cast<decltype(to.enumedArrayCount)>(from.enumedArrayCount);
		to.rootType = Convert(from.rootType);
		to.size = static_cast<decltype(to.size)>(from.size);
		return to;
	}

	struct StructuredDataEnum
	{
		std::int32_t entryCount;
		std::int32_t reservedEntryCount;
		std::uint32_t entries;
	};

	static_assert(sizeof(StructuredDataEnum) == 0xC);
	static_assert(offsetof(StructuredDataEnum, entryCount) == 0x0);
	static_assert(offsetof(StructuredDataEnum, reservedEntryCount) == 0x4);
	static_assert(offsetof(StructuredDataEnum, entries) == 0x8);

	inline StructuredDataEnum Convert(const Game::StructuredDataEnum& from)
	{
		StructuredDataEnum to{};
		to.entryCount = static_cast<std::int32_t>(from.entryCount);
		to.reservedEntryCount = static_cast<std::int32_t>(from.reservedEntryCount);
		return to;
	}

	inline Game::StructuredDataEnum Convert(const StructuredDataEnum& from)
	{
		Game::StructuredDataEnum to{};
		to.entryCount = static_cast<decltype(to.entryCount)>(from.entryCount);
		to.reservedEntryCount = static_cast<decltype(to.reservedEntryCount)>(from.reservedEntryCount);
		return to;
	}

	struct StructuredDataEnumEntry
	{
		std::uint32_t string;
		std::uint16_t index;
	};

	static_assert(sizeof(StructuredDataEnumEntry) == 0x8);
	static_assert(offsetof(StructuredDataEnumEntry, string) == 0x0);
	static_assert(offsetof(StructuredDataEnumEntry, index) == 0x4);

	inline StructuredDataEnumEntry Convert(const Game::StructuredDataEnumEntry& from)
	{
		StructuredDataEnumEntry to{};
		to.index = static_cast<std::uint16_t>(from.index);
		return to;
	}

	inline Game::StructuredDataEnumEntry Convert(const StructuredDataEnumEntry& from)
	{
		Game::StructuredDataEnumEntry to{};
		to.index = static_cast<decltype(to.index)>(from.index);
		return to;
	}

	struct StructuredDataStruct
	{
		std::int32_t propertyCount;
		std::uint32_t properties;
		std::int32_t size;
		std::uint32_t bitOffset;
	};

	static_assert(sizeof(StructuredDataStruct) == 0x10);
	static_assert(offsetof(StructuredDataStruct, propertyCount) == 0x0);
	static_assert(offsetof(StructuredDataStruct, properties) == 0x4);
	static_assert(offsetof(StructuredDataStruct, size) == 0x8);
	static_assert(offsetof(StructuredDataStruct, bitOffset) == 0xC);

	inline StructuredDataStruct Convert(const Game::StructuredDataStruct& from)
	{
		StructuredDataStruct to{};
		to.propertyCount = static_cast<std::int32_t>(from.propertyCount);
		to.size = static_cast<std::int32_t>(from.size);
		to.bitOffset = static_cast<std::uint32_t>(from.bitOffset);
		return to;
	}

	inline Game::StructuredDataStruct Convert(const StructuredDataStruct& from)
	{
		Game::StructuredDataStruct to{};
		to.propertyCount = static_cast<decltype(to.propertyCount)>(from.propertyCount);
		to.size = static_cast<decltype(to.size)>(from.size);
		to.bitOffset = static_cast<decltype(to.bitOffset)>(from.bitOffset);
		return to;
	}

	struct StructuredDataStructProperty
	{
		std::uint32_t name;
		StructuredDataType type;
		std::uint32_t offset;
	};

	static_assert(sizeof(StructuredDataStructProperty) == 0x10);
	static_assert(offsetof(StructuredDataStructProperty, name) == 0x0);
	static_assert(offsetof(StructuredDataStructProperty, type) == 0x4);
	static_assert(offsetof(StructuredDataStructProperty, offset) == 0xC);

	inline StructuredDataStructProperty Convert(const Game::StructuredDataStructProperty& from)
	{
		StructuredDataStructProperty to{};
		to.type = Convert(from.type);
		to.offset = static_cast<std::uint32_t>(from.offset);
		return to;
	}

	inline Game::StructuredDataStructProperty Convert(const StructuredDataStructProperty& from)
	{
		Game::StructuredDataStructProperty to{};
		to.type = Convert(from.type);
		to.offset = static_cast<decltype(to.offset)>(from.offset);
		return to;
	}

	struct StructuredDataIndexedArray
	{
		std::int32_t arraySize;
		StructuredDataType elementType;
		std::uint32_t elementSize;
	};

	static_assert(sizeof(StructuredDataIndexedArray) == 0x10);
	static_assert(offsetof(StructuredDataIndexedArray, arraySize) == 0x0);
	static_assert(offsetof(StructuredDataIndexedArray, elementType) == 0x4);
	static_assert(offsetof(StructuredDataIndexedArray, elementSize) == 0xC);

	inline StructuredDataIndexedArray Convert(const Game::StructuredDataIndexedArray& from)
	{
		StructuredDataIndexedArray to{};
		to.arraySize = static_cast<std::int32_t>(from.arraySize);
		to.elementType = Convert(from.elementType);
		to.elementSize = static_cast<std::uint32_t>(from.elementSize);
		return to;
	}

	inline Game::StructuredDataIndexedArray Convert(const StructuredDataIndexedArray& from)
	{
		Game::StructuredDataIndexedArray to{};
		to.arraySize = static_cast<decltype(to.arraySize)>(from.arraySize);
		to.elementType = Convert(from.elementType);
		to.elementSize = static_cast<decltype(to.elementSize)>(from.elementSize);
		return to;
	}

	struct StructuredDataEnumedArray
	{
		std::int32_t enumIndex;
		StructuredDataType elementType;
		std::uint32_t elementSize;
	};

	static_assert(sizeof(StructuredDataEnumedArray) == 0x10);
	static_assert(offsetof(StructuredDataEnumedArray, enumIndex) == 0x0);
	static_assert(offsetof(StructuredDataEnumedArray, elementType) == 0x4);
	static_assert(offsetof(StructuredDataEnumedArray, elementSize) == 0xC);

	inline StructuredDataEnumedArray Convert(const Game::StructuredDataEnumedArray& from)
	{
		StructuredDataEnumedArray to{};
		to.enumIndex = static_cast<std::int32_t>(from.enumIndex);
		to.elementType = Convert(from.elementType);
		to.elementSize = static_cast<std::uint32_t>(from.elementSize);
		return to;
	}

	inline Game::StructuredDataEnumedArray Convert(const StructuredDataEnumedArray& from)
	{
		Game::StructuredDataEnumedArray to{};
		to.enumIndex = static_cast<decltype(to.enumIndex)>(from.enumIndex);
		to.elementType = Convert(from.elementType);
		to.elementSize = static_cast<decltype(to.elementSize)>(from.elementSize);
		return to;
	}

	struct VehiclePhysDef
	{
		std::int32_t physicsEnabled;
		std::uint32_t physPresetName;
		std::uint32_t physPreset;
		std::uint32_t accelGraphName;
		std::int32_t steeringAxle;
		std::int32_t powerAxle;
		std::int32_t brakingAxle;
		float topSpeed;
		float reverseSpeed;
		float maxVelocity;
		float maxPitch;
		float maxRoll;
		float suspensionTravelFront;
		float suspensionTravelRear;
		float suspensionStrengthFront;
		float suspensionDampingFront;
		float suspensionStrengthRear;
		float suspensionDampingRear;
		float frictionBraking;
		float frictionCoasting;
		float frictionTopSpeed;
		float frictionSide;
		float frictionSideRear;
		float velocityDependentSlip;
		float rollStability;
		float rollResistance;
		float pitchResistance;
		float yawResistance;
		float uprightStrengthPitch;
		float uprightStrengthRoll;
		float targetAirPitch;
		float airYawTorque;
		float airPitchTorque;
		float minimumMomentumForCollision;
		float collisionLaunchForceScale;
		float wreckedMassScale;
		float wreckedBodyFriction;
		float minimumJoltForNotify;
		float slipThresholdFront;
		float slipThresholdRear;
		float slipFricScaleFront;
		float slipFricScaleRear;
		float slipFricRateFront;
		float slipFricRateRear;
		float slipYawTorque;
	};

	static_assert(sizeof(VehiclePhysDef) == 0xB4);
	static_assert(offsetof(VehiclePhysDef, physicsEnabled) == 0x0);
	static_assert(offsetof(VehiclePhysDef, physPresetName) == 0x4);
	static_assert(offsetof(VehiclePhysDef, physPreset) == 0x8);
	static_assert(offsetof(VehiclePhysDef, accelGraphName) == 0xC);
	static_assert(offsetof(VehiclePhysDef, steeringAxle) == 0x10);
	static_assert(offsetof(VehiclePhysDef, powerAxle) == 0x14);
	static_assert(offsetof(VehiclePhysDef, brakingAxle) == 0x18);
	static_assert(offsetof(VehiclePhysDef, topSpeed) == 0x1C);
	static_assert(offsetof(VehiclePhysDef, reverseSpeed) == 0x20);
	static_assert(offsetof(VehiclePhysDef, maxVelocity) == 0x24);
	static_assert(offsetof(VehiclePhysDef, maxPitch) == 0x28);
	static_assert(offsetof(VehiclePhysDef, maxRoll) == 0x2C);
	static_assert(offsetof(VehiclePhysDef, suspensionTravelFront) == 0x30);
	static_assert(offsetof(VehiclePhysDef, suspensionTravelRear) == 0x34);
	static_assert(offsetof(VehiclePhysDef, suspensionStrengthFront) == 0x38);
	static_assert(offsetof(VehiclePhysDef, suspensionDampingFront) == 0x3C);
	static_assert(offsetof(VehiclePhysDef, suspensionStrengthRear) == 0x40);
	static_assert(offsetof(VehiclePhysDef, suspensionDampingRear) == 0x44);
	static_assert(offsetof(VehiclePhysDef, frictionBraking) == 0x48);
	static_assert(offsetof(VehiclePhysDef, frictionCoasting) == 0x4C);
	static_assert(offsetof(VehiclePhysDef, frictionTopSpeed) == 0x50);
	static_assert(offsetof(VehiclePhysDef, frictionSide) == 0x54);
	static_assert(offsetof(VehiclePhysDef, frictionSideRear) == 0x58);
	static_assert(offsetof(VehiclePhysDef, velocityDependentSlip) == 0x5C);
	static_assert(offsetof(VehiclePhysDef, rollStability) == 0x60);
	static_assert(offsetof(VehiclePhysDef, rollResistance) == 0x64);
	static_assert(offsetof(VehiclePhysDef, pitchResistance) == 0x68);
	static_assert(offsetof(VehiclePhysDef, yawResistance) == 0x6C);
	static_assert(offsetof(VehiclePhysDef, uprightStrengthPitch) == 0x70);
	static_assert(offsetof(VehiclePhysDef, uprightStrengthRoll) == 0x74);
	static_assert(offsetof(VehiclePhysDef, targetAirPitch) == 0x78);
	static_assert(offsetof(VehiclePhysDef, airYawTorque) == 0x7C);
	static_assert(offsetof(VehiclePhysDef, airPitchTorque) == 0x80);
	static_assert(offsetof(VehiclePhysDef, minimumMomentumForCollision) == 0x84);
	static_assert(offsetof(VehiclePhysDef, collisionLaunchForceScale) == 0x88);
	static_assert(offsetof(VehiclePhysDef, wreckedMassScale) == 0x8C);
	static_assert(offsetof(VehiclePhysDef, wreckedBodyFriction) == 0x90);
	static_assert(offsetof(VehiclePhysDef, minimumJoltForNotify) == 0x94);
	static_assert(offsetof(VehiclePhysDef, slipThresholdFront) == 0x98);
	static_assert(offsetof(VehiclePhysDef, slipThresholdRear) == 0x9C);
	static_assert(offsetof(VehiclePhysDef, slipFricScaleFront) == 0xA0);
	static_assert(offsetof(VehiclePhysDef, slipFricScaleRear) == 0xA4);
	static_assert(offsetof(VehiclePhysDef, slipFricRateFront) == 0xA8);
	static_assert(offsetof(VehiclePhysDef, slipFricRateRear) == 0xAC);
	static_assert(offsetof(VehiclePhysDef, slipYawTorque) == 0xB0);

	inline VehiclePhysDef Convert(const Game::VehiclePhysDef& from)
	{
		VehiclePhysDef to{};
		to.physicsEnabled = static_cast<std::int32_t>(from.physicsEnabled);
		to.steeringAxle = static_cast<std::int32_t>(from.steeringAxle);
		to.powerAxle = static_cast<std::int32_t>(from.powerAxle);
		to.brakingAxle = static_cast<std::int32_t>(from.brakingAxle);
		to.topSpeed = static_cast<float>(from.topSpeed);
		to.reverseSpeed = static_cast<float>(from.reverseSpeed);
		to.maxVelocity = static_cast<float>(from.maxVelocity);
		to.maxPitch = static_cast<float>(from.maxPitch);
		to.maxRoll = static_cast<float>(from.maxRoll);
		to.suspensionTravelFront = static_cast<float>(from.suspensionTravelFront);
		to.suspensionTravelRear = static_cast<float>(from.suspensionTravelRear);
		to.suspensionStrengthFront = static_cast<float>(from.suspensionStrengthFront);
		to.suspensionDampingFront = static_cast<float>(from.suspensionDampingFront);
		to.suspensionStrengthRear = static_cast<float>(from.suspensionStrengthRear);
		to.suspensionDampingRear = static_cast<float>(from.suspensionDampingRear);
		to.frictionBraking = static_cast<float>(from.frictionBraking);
		to.frictionCoasting = static_cast<float>(from.frictionCoasting);
		to.frictionTopSpeed = static_cast<float>(from.frictionTopSpeed);
		to.frictionSide = static_cast<float>(from.frictionSide);
		to.frictionSideRear = static_cast<float>(from.frictionSideRear);
		to.velocityDependentSlip = static_cast<float>(from.velocityDependentSlip);
		to.rollStability = static_cast<float>(from.rollStability);
		to.rollResistance = static_cast<float>(from.rollResistance);
		to.pitchResistance = static_cast<float>(from.pitchResistance);
		to.yawResistance = static_cast<float>(from.yawResistance);
		to.uprightStrengthPitch = static_cast<float>(from.uprightStrengthPitch);
		to.uprightStrengthRoll = static_cast<float>(from.uprightStrengthRoll);
		to.targetAirPitch = static_cast<float>(from.targetAirPitch);
		to.airYawTorque = static_cast<float>(from.airYawTorque);
		to.airPitchTorque = static_cast<float>(from.airPitchTorque);
		to.minimumMomentumForCollision = static_cast<float>(from.minimumMomentumForCollision);
		to.collisionLaunchForceScale = static_cast<float>(from.collisionLaunchForceScale);
		to.wreckedMassScale = static_cast<float>(from.wreckedMassScale);
		to.wreckedBodyFriction = static_cast<float>(from.wreckedBodyFriction);
		to.minimumJoltForNotify = static_cast<float>(from.minimumJoltForNotify);
		to.slipThresholdFront = static_cast<float>(from.slipThresholdFront);
		to.slipThresholdRear = static_cast<float>(from.slipThresholdRear);
		to.slipFricScaleFront = static_cast<float>(from.slipFricScaleFront);
		to.slipFricScaleRear = static_cast<float>(from.slipFricScaleRear);
		to.slipFricRateFront = static_cast<float>(from.slipFricRateFront);
		to.slipFricRateRear = static_cast<float>(from.slipFricRateRear);
		to.slipYawTorque = static_cast<float>(from.slipYawTorque);
		return to;
	}

	inline Game::VehiclePhysDef Convert(const VehiclePhysDef& from)
	{
		Game::VehiclePhysDef to{};
		to.physicsEnabled = static_cast<decltype(to.physicsEnabled)>(from.physicsEnabled);
		to.steeringAxle = static_cast<decltype(to.steeringAxle)>(from.steeringAxle);
		to.powerAxle = static_cast<decltype(to.powerAxle)>(from.powerAxle);
		to.brakingAxle = static_cast<decltype(to.brakingAxle)>(from.brakingAxle);
		to.topSpeed = static_cast<decltype(to.topSpeed)>(from.topSpeed);
		to.reverseSpeed = static_cast<decltype(to.reverseSpeed)>(from.reverseSpeed);
		to.maxVelocity = static_cast<decltype(to.maxVelocity)>(from.maxVelocity);
		to.maxPitch = static_cast<decltype(to.maxPitch)>(from.maxPitch);
		to.maxRoll = static_cast<decltype(to.maxRoll)>(from.maxRoll);
		to.suspensionTravelFront = static_cast<decltype(to.suspensionTravelFront)>(from.suspensionTravelFront);
		to.suspensionTravelRear = static_cast<decltype(to.suspensionTravelRear)>(from.suspensionTravelRear);
		to.suspensionStrengthFront = static_cast<decltype(to.suspensionStrengthFront)>(from.suspensionStrengthFront);
		to.suspensionDampingFront = static_cast<decltype(to.suspensionDampingFront)>(from.suspensionDampingFront);
		to.suspensionStrengthRear = static_cast<decltype(to.suspensionStrengthRear)>(from.suspensionStrengthRear);
		to.suspensionDampingRear = static_cast<decltype(to.suspensionDampingRear)>(from.suspensionDampingRear);
		to.frictionBraking = static_cast<decltype(to.frictionBraking)>(from.frictionBraking);
		to.frictionCoasting = static_cast<decltype(to.frictionCoasting)>(from.frictionCoasting);
		to.frictionTopSpeed = static_cast<decltype(to.frictionTopSpeed)>(from.frictionTopSpeed);
		to.frictionSide = static_cast<decltype(to.frictionSide)>(from.frictionSide);
		to.frictionSideRear = static_cast<decltype(to.frictionSideRear)>(from.frictionSideRear);
		to.velocityDependentSlip = static_cast<decltype(to.velocityDependentSlip)>(from.velocityDependentSlip);
		to.rollStability = static_cast<decltype(to.rollStability)>(from.rollStability);
		to.rollResistance = static_cast<decltype(to.rollResistance)>(from.rollResistance);
		to.pitchResistance = static_cast<decltype(to.pitchResistance)>(from.pitchResistance);
		to.yawResistance = static_cast<decltype(to.yawResistance)>(from.yawResistance);
		to.uprightStrengthPitch = static_cast<decltype(to.uprightStrengthPitch)>(from.uprightStrengthPitch);
		to.uprightStrengthRoll = static_cast<decltype(to.uprightStrengthRoll)>(from.uprightStrengthRoll);
		to.targetAirPitch = static_cast<decltype(to.targetAirPitch)>(from.targetAirPitch);
		to.airYawTorque = static_cast<decltype(to.airYawTorque)>(from.airYawTorque);
		to.airPitchTorque = static_cast<decltype(to.airPitchTorque)>(from.airPitchTorque);
		to.minimumMomentumForCollision = static_cast<decltype(to.minimumMomentumForCollision)>(from.minimumMomentumForCollision);
		to.collisionLaunchForceScale = static_cast<decltype(to.collisionLaunchForceScale)>(from.collisionLaunchForceScale);
		to.wreckedMassScale = static_cast<decltype(to.wreckedMassScale)>(from.wreckedMassScale);
		to.wreckedBodyFriction = static_cast<decltype(to.wreckedBodyFriction)>(from.wreckedBodyFriction);
		to.minimumJoltForNotify = static_cast<decltype(to.minimumJoltForNotify)>(from.minimumJoltForNotify);
		to.slipThresholdFront = static_cast<decltype(to.slipThresholdFront)>(from.slipThresholdFront);
		to.slipThresholdRear = static_cast<decltype(to.slipThresholdRear)>(from.slipThresholdRear);
		to.slipFricScaleFront = static_cast<decltype(to.slipFricScaleFront)>(from.slipFricScaleFront);
		to.slipFricScaleRear = static_cast<decltype(to.slipFricScaleRear)>(from.slipFricScaleRear);
		to.slipFricRateFront = static_cast<decltype(to.slipFricRateFront)>(from.slipFricRateFront);
		to.slipFricRateRear = static_cast<decltype(to.slipFricRateRear)>(from.slipFricRateRear);
		to.slipYawTorque = static_cast<decltype(to.slipYawTorque)>(from.slipYawTorque);
		return to;
	}

	struct VehicleDef
	{
		std::uint32_t name;
		std::int32_t type;
		std::uint32_t useHintString;
		std::int32_t health;
		std::int32_t quadBarrel;
		float texScrollScale;
		float topSpeed;
		float accel;
		float rotRate;
		float rotAccel;
		float maxBodyPitch;
		float maxBodyRoll;
		float fakeBodyAccelPitch;
		float fakeBodyAccelRoll;
		float fakeBodyVelPitch;
		float fakeBodyVelRoll;
		float fakeBodySideVelPitch;
		float fakeBodyPitchStrength;
		float fakeBodyRollStrength;
		float fakeBodyPitchDampening;
		float fakeBodyRollDampening;
		float fakeBodyBoatRockingAmplitude;
		float fakeBodyBoatRockingPeriod;
		float fakeBodyBoatRockingRotationPeriod;
		float fakeBodyBoatRockingFadeoutSpeed;
		float boatBouncingMinForce;
		float boatBouncingMaxForce;
		float boatBouncingRate;
		float boatBouncingFadeinSpeed;
		float boatBouncingFadeoutSteeringAngle;
		float collisionDamage;
		float collisionSpeed;
		float killcamOffset[3];
		std::int32_t playerProtected;
		std::int32_t bulletDamage;
		std::int32_t armorPiercingDamage;
		std::int32_t grenadeDamage;
		std::int32_t projectileDamage;
		std::int32_t projectileSplashDamage;
		std::int32_t heavyExplosiveDamage;
		VehiclePhysDef vehPhysDef;
		float boostDuration;
		float boostRechargeTime;
		float boostAcceleration;
		float suspensionTravel;
		float maxSteeringAngle;
		float steeringLerp;
		float minSteeringScale;
		float minSteeringSpeed;
		std::int32_t camLookEnabled;
		float camLerp;
		float camPitchInfluence;
		float camRollInfluence;
		float camFovIncrease;
		float camFovOffset;
		float camFovSpeed;
		std::uint32_t turretWeaponName;
		std::uint32_t turretWeapon;
		float turretHorizSpanLeft;
		float turretHorizSpanRight;
		float turretVertSpanUp;
		float turretVertSpanDown;
		float turretRotRate;
		std::uint32_t turretSpinSnd;
		std::uint32_t turretStopSnd;
		std::int32_t trophyEnabled;
		float trophyRadius;
		float trophyInactiveRadius;
		std::int32_t trophyAmmoCount;
		float trophyReloadTime;
		std::uint16_t trophyTags[4];
		std::uint32_t compassFriendlyIcon;
		std::uint32_t compassEnemyIcon;
		std::int32_t compassIconWidth;
		std::int32_t compassIconHeight;
		std::uint32_t idleLowSnd;
		std::uint32_t idleHighSnd;
		std::uint32_t engineLowSnd;
		std::uint32_t engineHighSnd;
		float engineSndSpeed;
		std::uint32_t engineStartUpSnd;
		std::int32_t engineStartUpLength;
		std::uint32_t engineShutdownSnd;
		std::uint32_t engineIdleSnd;
		std::uint32_t engineSustainSnd;
		std::uint32_t engineRampUpSnd;
		std::int32_t engineRampUpLength;
		std::uint32_t engineRampDownSnd;
		std::int32_t engineRampDownLength;
		std::uint32_t suspensionSoftSnd;
		float suspensionSoftCompression;
		std::uint32_t suspensionHardSnd;
		float suspensionHardCompression;
		std::uint32_t collisionSnd;
		float collisionBlendSpeed;
		std::uint32_t speedSnd;
		float speedSndBlendSpeed;
		std::uint32_t surfaceSndPrefix;
		std::uint32_t surfaceSnds[31];
		float surfaceSndBlendSpeed;
		float slideVolume;
		float slideBlendSpeed;
		float inAirPitch;
	};

	static_assert(sizeof(VehicleDef) == 0x2D0);
	static_assert(offsetof(VehicleDef, name) == 0x0);
	static_assert(offsetof(VehicleDef, type) == 0x4);
	static_assert(offsetof(VehicleDef, useHintString) == 0x8);
	static_assert(offsetof(VehicleDef, health) == 0xC);
	static_assert(offsetof(VehicleDef, quadBarrel) == 0x10);
	static_assert(offsetof(VehicleDef, texScrollScale) == 0x14);
	static_assert(offsetof(VehicleDef, topSpeed) == 0x18);
	static_assert(offsetof(VehicleDef, accel) == 0x1C);
	static_assert(offsetof(VehicleDef, rotRate) == 0x20);
	static_assert(offsetof(VehicleDef, rotAccel) == 0x24);
	static_assert(offsetof(VehicleDef, maxBodyPitch) == 0x28);
	static_assert(offsetof(VehicleDef, maxBodyRoll) == 0x2C);
	static_assert(offsetof(VehicleDef, fakeBodyAccelPitch) == 0x30);
	static_assert(offsetof(VehicleDef, fakeBodyAccelRoll) == 0x34);
	static_assert(offsetof(VehicleDef, fakeBodyVelPitch) == 0x38);
	static_assert(offsetof(VehicleDef, fakeBodyVelRoll) == 0x3C);
	static_assert(offsetof(VehicleDef, fakeBodySideVelPitch) == 0x40);
	static_assert(offsetof(VehicleDef, fakeBodyPitchStrength) == 0x44);
	static_assert(offsetof(VehicleDef, fakeBodyRollStrength) == 0x48);
	static_assert(offsetof(VehicleDef, fakeBodyPitchDampening) == 0x4C);
	static_assert(offsetof(VehicleDef, fakeBodyRollDampening) == 0x50);
	static_assert(offsetof(VehicleDef, fakeBodyBoatRockingAmplitude) == 0x54);
	static_assert(offsetof(VehicleDef, fakeBodyBoatRockingPeriod) == 0x58);
	static_assert(offsetof(VehicleDef, fakeBodyBoatRockingRotationPeriod) == 0x5C);
	static_assert(offsetof(VehicleDef, fakeBodyBoatRockingFadeoutSpeed) == 0x60);
	static_assert(offsetof(VehicleDef, boatBouncingMinForce) == 0x64);
	static_assert(offsetof(VehicleDef, boatBouncingMaxForce) == 0x68);
	static_assert(offsetof(VehicleDef, boatBouncingRate) == 0x6C);
	static_assert(offsetof(VehicleDef, boatBouncingFadeinSpeed) == 0x70);
	static_assert(offsetof(VehicleDef, boatBouncingFadeoutSteeringAngle) == 0x74);
	static_assert(offsetof(VehicleDef, collisionDamage) == 0x78);
	static_assert(offsetof(VehicleDef, collisionSpeed) == 0x7C);
	static_assert(offsetof(VehicleDef, killcamOffset) == 0x80);
	static_assert(offsetof(VehicleDef, playerProtected) == 0x8C);
	static_assert(offsetof(VehicleDef, bulletDamage) == 0x90);
	static_assert(offsetof(VehicleDef, armorPiercingDamage) == 0x94);
	static_assert(offsetof(VehicleDef, grenadeDamage) == 0x98);
	static_assert(offsetof(VehicleDef, projectileDamage) == 0x9C);
	static_assert(offsetof(VehicleDef, projectileSplashDamage) == 0xA0);
	static_assert(offsetof(VehicleDef, heavyExplosiveDamage) == 0xA4);
	static_assert(offsetof(VehicleDef, vehPhysDef) == 0xA8);
	static_assert(offsetof(VehicleDef, boostDuration) == 0x15C);
	static_assert(offsetof(VehicleDef, boostRechargeTime) == 0x160);
	static_assert(offsetof(VehicleDef, boostAcceleration) == 0x164);
	static_assert(offsetof(VehicleDef, suspensionTravel) == 0x168);
	static_assert(offsetof(VehicleDef, maxSteeringAngle) == 0x16C);
	static_assert(offsetof(VehicleDef, steeringLerp) == 0x170);
	static_assert(offsetof(VehicleDef, minSteeringScale) == 0x174);
	static_assert(offsetof(VehicleDef, minSteeringSpeed) == 0x178);
	static_assert(offsetof(VehicleDef, camLookEnabled) == 0x17C);
	static_assert(offsetof(VehicleDef, camLerp) == 0x180);
	static_assert(offsetof(VehicleDef, camPitchInfluence) == 0x184);
	static_assert(offsetof(VehicleDef, camRollInfluence) == 0x188);
	static_assert(offsetof(VehicleDef, camFovIncrease) == 0x18C);
	static_assert(offsetof(VehicleDef, camFovOffset) == 0x190);
	static_assert(offsetof(VehicleDef, camFovSpeed) == 0x194);
	static_assert(offsetof(VehicleDef, turretWeaponName) == 0x198);
	static_assert(offsetof(VehicleDef, turretWeapon) == 0x19C);
	static_assert(offsetof(VehicleDef, turretHorizSpanLeft) == 0x1A0);
	static_assert(offsetof(VehicleDef, turretHorizSpanRight) == 0x1A4);
	static_assert(offsetof(VehicleDef, turretVertSpanUp) == 0x1A8);
	static_assert(offsetof(VehicleDef, turretVertSpanDown) == 0x1AC);
	static_assert(offsetof(VehicleDef, turretRotRate) == 0x1B0);
	static_assert(offsetof(VehicleDef, turretSpinSnd) == 0x1B4);
	static_assert(offsetof(VehicleDef, turretStopSnd) == 0x1B8);
	static_assert(offsetof(VehicleDef, trophyEnabled) == 0x1BC);
	static_assert(offsetof(VehicleDef, trophyRadius) == 0x1C0);
	static_assert(offsetof(VehicleDef, trophyInactiveRadius) == 0x1C4);
	static_assert(offsetof(VehicleDef, trophyAmmoCount) == 0x1C8);
	static_assert(offsetof(VehicleDef, trophyReloadTime) == 0x1CC);
	static_assert(offsetof(VehicleDef, trophyTags) == 0x1D0);
	static_assert(offsetof(VehicleDef, compassFriendlyIcon) == 0x1D8);
	static_assert(offsetof(VehicleDef, compassEnemyIcon) == 0x1DC);
	static_assert(offsetof(VehicleDef, compassIconWidth) == 0x1E0);
	static_assert(offsetof(VehicleDef, compassIconHeight) == 0x1E4);
	static_assert(offsetof(VehicleDef, idleLowSnd) == 0x1E8);
	static_assert(offsetof(VehicleDef, idleHighSnd) == 0x1EC);
	static_assert(offsetof(VehicleDef, engineLowSnd) == 0x1F0);
	static_assert(offsetof(VehicleDef, engineHighSnd) == 0x1F4);
	static_assert(offsetof(VehicleDef, engineSndSpeed) == 0x1F8);
	static_assert(offsetof(VehicleDef, engineStartUpSnd) == 0x1FC);
	static_assert(offsetof(VehicleDef, engineStartUpLength) == 0x200);
	static_assert(offsetof(VehicleDef, engineShutdownSnd) == 0x204);
	static_assert(offsetof(VehicleDef, engineIdleSnd) == 0x208);
	static_assert(offsetof(VehicleDef, engineSustainSnd) == 0x20C);
	static_assert(offsetof(VehicleDef, engineRampUpSnd) == 0x210);
	static_assert(offsetof(VehicleDef, engineRampUpLength) == 0x214);
	static_assert(offsetof(VehicleDef, engineRampDownSnd) == 0x218);
	static_assert(offsetof(VehicleDef, engineRampDownLength) == 0x21C);
	static_assert(offsetof(VehicleDef, suspensionSoftSnd) == 0x220);
	static_assert(offsetof(VehicleDef, suspensionSoftCompression) == 0x224);
	static_assert(offsetof(VehicleDef, suspensionHardSnd) == 0x228);
	static_assert(offsetof(VehicleDef, suspensionHardCompression) == 0x22C);
	static_assert(offsetof(VehicleDef, collisionSnd) == 0x230);
	static_assert(offsetof(VehicleDef, collisionBlendSpeed) == 0x234);
	static_assert(offsetof(VehicleDef, speedSnd) == 0x238);
	static_assert(offsetof(VehicleDef, speedSndBlendSpeed) == 0x23C);
	static_assert(offsetof(VehicleDef, surfaceSndPrefix) == 0x240);
	static_assert(offsetof(VehicleDef, surfaceSnds) == 0x244);
	static_assert(offsetof(VehicleDef, surfaceSndBlendSpeed) == 0x2C0);
	static_assert(offsetof(VehicleDef, slideVolume) == 0x2C4);
	static_assert(offsetof(VehicleDef, slideBlendSpeed) == 0x2C8);
	static_assert(offsetof(VehicleDef, inAirPitch) == 0x2CC);

	inline VehicleDef Convert(const Game::VehicleDef& from)
	{
		VehicleDef to{};
		to.type = static_cast<std::int32_t>(from.type);
		to.health = static_cast<std::int32_t>(from.health);
		to.quadBarrel = static_cast<std::int32_t>(from.quadBarrel);
		to.texScrollScale = static_cast<float>(from.texScrollScale);
		to.topSpeed = static_cast<float>(from.topSpeed);
		to.accel = static_cast<float>(from.accel);
		to.rotRate = static_cast<float>(from.rotRate);
		to.rotAccel = static_cast<float>(from.rotAccel);
		to.maxBodyPitch = static_cast<float>(from.maxBodyPitch);
		to.maxBodyRoll = static_cast<float>(from.maxBodyRoll);
		to.fakeBodyAccelPitch = static_cast<float>(from.fakeBodyAccelPitch);
		to.fakeBodyAccelRoll = static_cast<float>(from.fakeBodyAccelRoll);
		to.fakeBodyVelPitch = static_cast<float>(from.fakeBodyVelPitch);
		to.fakeBodyVelRoll = static_cast<float>(from.fakeBodyVelRoll);
		to.fakeBodySideVelPitch = static_cast<float>(from.fakeBodySideVelPitch);
		to.fakeBodyPitchStrength = static_cast<float>(from.fakeBodyPitchStrength);
		to.fakeBodyRollStrength = static_cast<float>(from.fakeBodyRollStrength);
		to.fakeBodyPitchDampening = static_cast<float>(from.fakeBodyPitchDampening);
		to.fakeBodyRollDampening = static_cast<float>(from.fakeBodyRollDampening);
		to.fakeBodyBoatRockingAmplitude = static_cast<float>(from.fakeBodyBoatRockingAmplitude);
		to.fakeBodyBoatRockingPeriod = static_cast<float>(from.fakeBodyBoatRockingPeriod);
		to.fakeBodyBoatRockingRotationPeriod = static_cast<float>(from.fakeBodyBoatRockingRotationPeriod);
		to.fakeBodyBoatRockingFadeoutSpeed = static_cast<float>(from.fakeBodyBoatRockingFadeoutSpeed);
		to.boatBouncingMinForce = static_cast<float>(from.boatBouncingMinForce);
		to.boatBouncingMaxForce = static_cast<float>(from.boatBouncingMaxForce);
		to.boatBouncingRate = static_cast<float>(from.boatBouncingRate);
		to.boatBouncingFadeinSpeed = static_cast<float>(from.boatBouncingFadeinSpeed);
		to.boatBouncingFadeoutSteeringAngle = static_cast<float>(from.boatBouncingFadeoutSteeringAngle);
		to.collisionDamage = static_cast<float>(from.collisionDamage);
		to.collisionSpeed = static_cast<float>(from.collisionSpeed);
		std::memcpy(to.killcamOffset, from.killcamOffset, sizeof(to.killcamOffset));
		to.playerProtected = static_cast<std::int32_t>(from.playerProtected);
		to.bulletDamage = static_cast<std::int32_t>(from.bulletDamage);
		to.armorPiercingDamage = static_cast<std::int32_t>(from.armorPiercingDamage);
		to.grenadeDamage = static_cast<std::int32_t>(from.grenadeDamage);
		to.projectileDamage = static_cast<std::int32_t>(from.projectileDamage);
		to.projectileSplashDamage = static_cast<std::int32_t>(from.projectileSplashDamage);
		to.heavyExplosiveDamage = static_cast<std::int32_t>(from.heavyExplosiveDamage);
		to.vehPhysDef = Convert(from.vehPhysDef);
		to.boostDuration = static_cast<float>(from.boostDuration);
		to.boostRechargeTime = static_cast<float>(from.boostRechargeTime);
		to.boostAcceleration = static_cast<float>(from.boostAcceleration);
		to.suspensionTravel = static_cast<float>(from.suspensionTravel);
		to.maxSteeringAngle = static_cast<float>(from.maxSteeringAngle);
		to.steeringLerp = static_cast<float>(from.steeringLerp);
		to.minSteeringScale = static_cast<float>(from.minSteeringScale);
		to.minSteeringSpeed = static_cast<float>(from.minSteeringSpeed);
		to.camLookEnabled = static_cast<std::int32_t>(from.camLookEnabled);
		to.camLerp = static_cast<float>(from.camLerp);
		to.camPitchInfluence = static_cast<float>(from.camPitchInfluence);
		to.camRollInfluence = static_cast<float>(from.camRollInfluence);
		to.camFovIncrease = static_cast<float>(from.camFovIncrease);
		to.camFovOffset = static_cast<float>(from.camFovOffset);
		to.camFovSpeed = static_cast<float>(from.camFovSpeed);
		to.turretHorizSpanLeft = static_cast<float>(from.turretHorizSpanLeft);
		to.turretHorizSpanRight = static_cast<float>(from.turretHorizSpanRight);
		to.turretVertSpanUp = static_cast<float>(from.turretVertSpanUp);
		to.turretVertSpanDown = static_cast<float>(from.turretVertSpanDown);
		to.turretRotRate = static_cast<float>(from.turretRotRate);
		to.trophyEnabled = static_cast<std::int32_t>(from.trophyEnabled);
		to.trophyRadius = static_cast<float>(from.trophyRadius);
		to.trophyInactiveRadius = static_cast<float>(from.trophyInactiveRadius);
		to.trophyAmmoCount = static_cast<std::int32_t>(from.trophyAmmoCount);
		to.trophyReloadTime = static_cast<float>(from.trophyReloadTime);
		std::memcpy(to.trophyTags, from.trophyTags, sizeof(to.trophyTags));
		to.compassIconWidth = static_cast<std::int32_t>(from.compassIconWidth);
		to.compassIconHeight = static_cast<std::int32_t>(from.compassIconHeight);
		to.engineSndSpeed = static_cast<float>(from.engineSndSpeed);
		to.engineStartUpLength = static_cast<std::int32_t>(from.engineStartUpLength);
		to.engineRampUpLength = static_cast<std::int32_t>(from.engineRampUpLength);
		to.engineRampDownLength = static_cast<std::int32_t>(from.engineRampDownLength);
		to.suspensionSoftCompression = static_cast<float>(from.suspensionSoftCompression);
		to.suspensionHardCompression = static_cast<float>(from.suspensionHardCompression);
		to.collisionBlendSpeed = static_cast<float>(from.collisionBlendSpeed);
		to.speedSndBlendSpeed = static_cast<float>(from.speedSndBlendSpeed);
		to.surfaceSndBlendSpeed = static_cast<float>(from.surfaceSndBlendSpeed);
		to.slideVolume = static_cast<float>(from.slideVolume);
		to.slideBlendSpeed = static_cast<float>(from.slideBlendSpeed);
		to.inAirPitch = static_cast<float>(from.inAirPitch);
		return to;
	}

	inline Game::VehicleDef Convert(const VehicleDef& from)
	{
		Game::VehicleDef to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.health = static_cast<decltype(to.health)>(from.health);
		to.quadBarrel = static_cast<decltype(to.quadBarrel)>(from.quadBarrel);
		to.texScrollScale = static_cast<decltype(to.texScrollScale)>(from.texScrollScale);
		to.topSpeed = static_cast<decltype(to.topSpeed)>(from.topSpeed);
		to.accel = static_cast<decltype(to.accel)>(from.accel);
		to.rotRate = static_cast<decltype(to.rotRate)>(from.rotRate);
		to.rotAccel = static_cast<decltype(to.rotAccel)>(from.rotAccel);
		to.maxBodyPitch = static_cast<decltype(to.maxBodyPitch)>(from.maxBodyPitch);
		to.maxBodyRoll = static_cast<decltype(to.maxBodyRoll)>(from.maxBodyRoll);
		to.fakeBodyAccelPitch = static_cast<decltype(to.fakeBodyAccelPitch)>(from.fakeBodyAccelPitch);
		to.fakeBodyAccelRoll = static_cast<decltype(to.fakeBodyAccelRoll)>(from.fakeBodyAccelRoll);
		to.fakeBodyVelPitch = static_cast<decltype(to.fakeBodyVelPitch)>(from.fakeBodyVelPitch);
		to.fakeBodyVelRoll = static_cast<decltype(to.fakeBodyVelRoll)>(from.fakeBodyVelRoll);
		to.fakeBodySideVelPitch = static_cast<decltype(to.fakeBodySideVelPitch)>(from.fakeBodySideVelPitch);
		to.fakeBodyPitchStrength = static_cast<decltype(to.fakeBodyPitchStrength)>(from.fakeBodyPitchStrength);
		to.fakeBodyRollStrength = static_cast<decltype(to.fakeBodyRollStrength)>(from.fakeBodyRollStrength);
		to.fakeBodyPitchDampening = static_cast<decltype(to.fakeBodyPitchDampening)>(from.fakeBodyPitchDampening);
		to.fakeBodyRollDampening = static_cast<decltype(to.fakeBodyRollDampening)>(from.fakeBodyRollDampening);
		to.fakeBodyBoatRockingAmplitude = static_cast<decltype(to.fakeBodyBoatRockingAmplitude)>(from.fakeBodyBoatRockingAmplitude);
		to.fakeBodyBoatRockingPeriod = static_cast<decltype(to.fakeBodyBoatRockingPeriod)>(from.fakeBodyBoatRockingPeriod);
		to.fakeBodyBoatRockingRotationPeriod = static_cast<decltype(to.fakeBodyBoatRockingRotationPeriod)>(from.fakeBodyBoatRockingRotationPeriod);
		to.fakeBodyBoatRockingFadeoutSpeed = static_cast<decltype(to.fakeBodyBoatRockingFadeoutSpeed)>(from.fakeBodyBoatRockingFadeoutSpeed);
		to.boatBouncingMinForce = static_cast<decltype(to.boatBouncingMinForce)>(from.boatBouncingMinForce);
		to.boatBouncingMaxForce = static_cast<decltype(to.boatBouncingMaxForce)>(from.boatBouncingMaxForce);
		to.boatBouncingRate = static_cast<decltype(to.boatBouncingRate)>(from.boatBouncingRate);
		to.boatBouncingFadeinSpeed = static_cast<decltype(to.boatBouncingFadeinSpeed)>(from.boatBouncingFadeinSpeed);
		to.boatBouncingFadeoutSteeringAngle = static_cast<decltype(to.boatBouncingFadeoutSteeringAngle)>(from.boatBouncingFadeoutSteeringAngle);
		to.collisionDamage = static_cast<decltype(to.collisionDamage)>(from.collisionDamage);
		to.collisionSpeed = static_cast<decltype(to.collisionSpeed)>(from.collisionSpeed);
		std::memcpy(to.killcamOffset, from.killcamOffset, sizeof(from.killcamOffset));
		to.playerProtected = static_cast<decltype(to.playerProtected)>(from.playerProtected);
		to.bulletDamage = static_cast<decltype(to.bulletDamage)>(from.bulletDamage);
		to.armorPiercingDamage = static_cast<decltype(to.armorPiercingDamage)>(from.armorPiercingDamage);
		to.grenadeDamage = static_cast<decltype(to.grenadeDamage)>(from.grenadeDamage);
		to.projectileDamage = static_cast<decltype(to.projectileDamage)>(from.projectileDamage);
		to.projectileSplashDamage = static_cast<decltype(to.projectileSplashDamage)>(from.projectileSplashDamage);
		to.heavyExplosiveDamage = static_cast<decltype(to.heavyExplosiveDamage)>(from.heavyExplosiveDamage);
		to.vehPhysDef = Convert(from.vehPhysDef);
		to.boostDuration = static_cast<decltype(to.boostDuration)>(from.boostDuration);
		to.boostRechargeTime = static_cast<decltype(to.boostRechargeTime)>(from.boostRechargeTime);
		to.boostAcceleration = static_cast<decltype(to.boostAcceleration)>(from.boostAcceleration);
		to.suspensionTravel = static_cast<decltype(to.suspensionTravel)>(from.suspensionTravel);
		to.maxSteeringAngle = static_cast<decltype(to.maxSteeringAngle)>(from.maxSteeringAngle);
		to.steeringLerp = static_cast<decltype(to.steeringLerp)>(from.steeringLerp);
		to.minSteeringScale = static_cast<decltype(to.minSteeringScale)>(from.minSteeringScale);
		to.minSteeringSpeed = static_cast<decltype(to.minSteeringSpeed)>(from.minSteeringSpeed);
		to.camLookEnabled = static_cast<decltype(to.camLookEnabled)>(from.camLookEnabled);
		to.camLerp = static_cast<decltype(to.camLerp)>(from.camLerp);
		to.camPitchInfluence = static_cast<decltype(to.camPitchInfluence)>(from.camPitchInfluence);
		to.camRollInfluence = static_cast<decltype(to.camRollInfluence)>(from.camRollInfluence);
		to.camFovIncrease = static_cast<decltype(to.camFovIncrease)>(from.camFovIncrease);
		to.camFovOffset = static_cast<decltype(to.camFovOffset)>(from.camFovOffset);
		to.camFovSpeed = static_cast<decltype(to.camFovSpeed)>(from.camFovSpeed);
		to.turretHorizSpanLeft = static_cast<decltype(to.turretHorizSpanLeft)>(from.turretHorizSpanLeft);
		to.turretHorizSpanRight = static_cast<decltype(to.turretHorizSpanRight)>(from.turretHorizSpanRight);
		to.turretVertSpanUp = static_cast<decltype(to.turretVertSpanUp)>(from.turretVertSpanUp);
		to.turretVertSpanDown = static_cast<decltype(to.turretVertSpanDown)>(from.turretVertSpanDown);
		to.turretRotRate = static_cast<decltype(to.turretRotRate)>(from.turretRotRate);
		to.trophyEnabled = static_cast<decltype(to.trophyEnabled)>(from.trophyEnabled);
		to.trophyRadius = static_cast<decltype(to.trophyRadius)>(from.trophyRadius);
		to.trophyInactiveRadius = static_cast<decltype(to.trophyInactiveRadius)>(from.trophyInactiveRadius);
		to.trophyAmmoCount = static_cast<decltype(to.trophyAmmoCount)>(from.trophyAmmoCount);
		to.trophyReloadTime = static_cast<decltype(to.trophyReloadTime)>(from.trophyReloadTime);
		std::memcpy(to.trophyTags, from.trophyTags, sizeof(from.trophyTags));
		to.compassIconWidth = static_cast<decltype(to.compassIconWidth)>(from.compassIconWidth);
		to.compassIconHeight = static_cast<decltype(to.compassIconHeight)>(from.compassIconHeight);
		to.engineSndSpeed = static_cast<decltype(to.engineSndSpeed)>(from.engineSndSpeed);
		to.engineStartUpLength = static_cast<decltype(to.engineStartUpLength)>(from.engineStartUpLength);
		to.engineRampUpLength = static_cast<decltype(to.engineRampUpLength)>(from.engineRampUpLength);
		to.engineRampDownLength = static_cast<decltype(to.engineRampDownLength)>(from.engineRampDownLength);
		to.suspensionSoftCompression = static_cast<decltype(to.suspensionSoftCompression)>(from.suspensionSoftCompression);
		to.suspensionHardCompression = static_cast<decltype(to.suspensionHardCompression)>(from.suspensionHardCompression);
		to.collisionBlendSpeed = static_cast<decltype(to.collisionBlendSpeed)>(from.collisionBlendSpeed);
		to.speedSndBlendSpeed = static_cast<decltype(to.speedSndBlendSpeed)>(from.speedSndBlendSpeed);
		to.surfaceSndBlendSpeed = static_cast<decltype(to.surfaceSndBlendSpeed)>(from.surfaceSndBlendSpeed);
		to.slideVolume = static_cast<decltype(to.slideVolume)>(from.slideVolume);
		to.slideBlendSpeed = static_cast<decltype(to.slideBlendSpeed)>(from.slideBlendSpeed);
		to.inAirPitch = static_cast<decltype(to.inAirPitch)>(from.inAirPitch);
		return to;
	}

	struct ScriptStringList
	{
		std::int32_t count;
		std::uint32_t strings;
	};

	static_assert(sizeof(ScriptStringList) == 0x8);
	static_assert(offsetof(ScriptStringList, count) == 0x0);
	static_assert(offsetof(ScriptStringList, strings) == 0x4);

	inline ScriptStringList Convert(const Game::ScriptStringList& from)
	{
		ScriptStringList to{};
		to.count = static_cast<std::int32_t>(from.count);
		return to;
	}

	inline Game::ScriptStringList Convert(const ScriptStringList& from)
	{
		Game::ScriptStringList to{};
		to.count = static_cast<decltype(to.count)>(from.count);
		return to;
	}

	struct XAssetList
	{
		ScriptStringList stringList;
		std::int32_t assetCount;
		std::uint32_t assets;
	};

	static_assert(sizeof(XAssetList) == 0x10);
	static_assert(offsetof(XAssetList, stringList) == 0x0);
	static_assert(offsetof(XAssetList, assetCount) == 0x8);
	static_assert(offsetof(XAssetList, assets) == 0xC);

	inline XAssetList Convert(const Game::XAssetList& from)
	{
		XAssetList to{};
		to.stringList = Convert(from.stringList);
		to.assetCount = static_cast<std::int32_t>(from.assetCount);
		return to;
	}

	inline Game::XAssetList Convert(const XAssetList& from)
	{
		Game::XAssetList to{};
		to.stringList = Convert(from.stringList);
		to.assetCount = static_cast<decltype(to.assetCount)>(from.assetCount);
		return to;
	}

	union XAssetHeader
	{
		std::uint32_t data;
		std::uint32_t physPreset;
		std::uint32_t physCollmap;
		std::uint32_t parts;
		std::uint32_t modelSurfs;
		std::uint32_t model;
		std::uint32_t material;
		std::uint32_t pixelShader;
		std::uint32_t vertexShader;
		std::uint32_t vertexDecl;
		std::uint32_t techniqueSet;
		std::uint32_t image;
		std::uint32_t sound;
		std::uint32_t sndCurve;
		std::uint32_t loadSnd;
		std::uint32_t clipMap;
		std::uint32_t comWorld;
		std::uint32_t gameWorldSp;
		std::uint32_t gameWorldMp;
		std::uint32_t mapEnts;
		std::uint32_t fxWorld;
		std::uint32_t gfxWorld;
		std::uint32_t lightDef;
		std::uint32_t font;
		std::uint32_t menuList;
		std::uint32_t menu;
		std::uint32_t localize;
		std::uint32_t weapon;
		std::uint32_t sndDriverGlobals;
		std::uint32_t fx;
		std::uint32_t impactFx;
		std::uint32_t rawfile;
		std::uint32_t stringTable;
		std::uint32_t leaderboardDef;
		std::uint32_t structuredDataDefSet;
		std::uint32_t tracerDef;
		std::uint32_t vehDef;
		std::uint32_t addonMapEnts;
	};

	static_assert(sizeof(XAssetHeader) == 0x4);
	static_assert(offsetof(XAssetHeader, data) == 0x0);
	static_assert(offsetof(XAssetHeader, physPreset) == 0x0);
	static_assert(offsetof(XAssetHeader, physCollmap) == 0x0);
	static_assert(offsetof(XAssetHeader, parts) == 0x0);
	static_assert(offsetof(XAssetHeader, modelSurfs) == 0x0);
	static_assert(offsetof(XAssetHeader, model) == 0x0);
	static_assert(offsetof(XAssetHeader, material) == 0x0);
	static_assert(offsetof(XAssetHeader, pixelShader) == 0x0);
	static_assert(offsetof(XAssetHeader, vertexShader) == 0x0);
	static_assert(offsetof(XAssetHeader, vertexDecl) == 0x0);
	static_assert(offsetof(XAssetHeader, techniqueSet) == 0x0);
	static_assert(offsetof(XAssetHeader, image) == 0x0);
	static_assert(offsetof(XAssetHeader, sound) == 0x0);
	static_assert(offsetof(XAssetHeader, sndCurve) == 0x0);
	static_assert(offsetof(XAssetHeader, loadSnd) == 0x0);
	static_assert(offsetof(XAssetHeader, clipMap) == 0x0);
	static_assert(offsetof(XAssetHeader, comWorld) == 0x0);
	static_assert(offsetof(XAssetHeader, gameWorldSp) == 0x0);
	static_assert(offsetof(XAssetHeader, gameWorldMp) == 0x0);
	static_assert(offsetof(XAssetHeader, mapEnts) == 0x0);
	static_assert(offsetof(XAssetHeader, fxWorld) == 0x0);
	static_assert(offsetof(XAssetHeader, gfxWorld) == 0x0);
	static_assert(offsetof(XAssetHeader, lightDef) == 0x0);
	static_assert(offsetof(XAssetHeader, font) == 0x0);
	static_assert(offsetof(XAssetHeader, menuList) == 0x0);
	static_assert(offsetof(XAssetHeader, menu) == 0x0);
	static_assert(offsetof(XAssetHeader, localize) == 0x0);
	static_assert(offsetof(XAssetHeader, weapon) == 0x0);
	static_assert(offsetof(XAssetHeader, sndDriverGlobals) == 0x0);
	static_assert(offsetof(XAssetHeader, fx) == 0x0);
	static_assert(offsetof(XAssetHeader, impactFx) == 0x0);
	static_assert(offsetof(XAssetHeader, rawfile) == 0x0);
	static_assert(offsetof(XAssetHeader, stringTable) == 0x0);
	static_assert(offsetof(XAssetHeader, leaderboardDef) == 0x0);
	static_assert(offsetof(XAssetHeader, structuredDataDefSet) == 0x0);
	static_assert(offsetof(XAssetHeader, tracerDef) == 0x0);
	static_assert(offsetof(XAssetHeader, vehDef) == 0x0);
	static_assert(offsetof(XAssetHeader, addonMapEnts) == 0x0);

	inline XAssetHeader Convert(const Game::XAssetHeader& from)
	{
		XAssetHeader to{};
		std::memcpy(&to, &from, sizeof(to));
		return to;
	}

	inline Game::XAssetHeader Convert(const XAssetHeader& from)
	{
		Game::XAssetHeader to{};
		std::memcpy(&to, &from, sizeof(from));
		return to;
	}

	struct XAsset
	{
		std::int32_t type;
		XAssetHeader header;
	};

	static_assert(sizeof(XAsset) == 0x8);
	static_assert(offsetof(XAsset, type) == 0x0);
	static_assert(offsetof(XAsset, header) == 0x4);

	inline XAsset Convert(const Game::XAsset& from)
	{
		XAsset to{};
		to.type = static_cast<std::int32_t>(from.type);
		to.header = Convert(from.header);
		return to;
	}

	inline Game::XAsset Convert(const XAsset& from)
	{
		Game::XAsset to{};
		to.type = static_cast<decltype(to.type)>(from.type);
		to.header = Convert(from.header);
		return to;
	}

	struct AddonMapEnts
	{
		std::uint32_t name;
		std::uint32_t entityString;
		std::int32_t numEntityChars;
		MapTriggers trigger;
	};

	static_assert(sizeof(AddonMapEnts) == 0x24);
	static_assert(offsetof(AddonMapEnts, name) == 0x0);
	static_assert(offsetof(AddonMapEnts, entityString) == 0x4);
	static_assert(offsetof(AddonMapEnts, numEntityChars) == 0x8);
	static_assert(offsetof(AddonMapEnts, trigger) == 0xC);

	inline AddonMapEnts Convert(const Game::AddonMapEnts& from)
	{
		AddonMapEnts to{};
		to.numEntityChars = static_cast<std::int32_t>(from.numEntityChars);
		to.trigger = Convert(from.trigger);
		return to;
	}

	inline Game::AddonMapEnts Convert(const AddonMapEnts& from)
	{
		Game::AddonMapEnts to{};
		to.numEntityChars = static_cast<decltype(to.numEntityChars)>(from.numEntityChars);
		to.trigger = Convert(from.trigger);
		return to;
	}

	struct XFile
	{
		std::uint32_t size;
		std::uint32_t externalSize;
		std::uint32_t blockSize[8];
	};

	static_assert(sizeof(XFile) == 0x28);
	static_assert(offsetof(XFile, size) == 0x0);
	static_assert(offsetof(XFile, externalSize) == 0x4);
	static_assert(offsetof(XFile, blockSize) == 0x8);

	inline XFile Convert(const Game::XFile& from)
	{
		XFile to{};
		to.size = static_cast<std::uint32_t>(from.size);
		to.externalSize = static_cast<std::uint32_t>(from.externalSize);
		std::memcpy(to.blockSize, from.blockSize, sizeof(to.blockSize));
		return to;
	}

	inline Game::XFile Convert(const XFile& from)
	{
		Game::XFile to{};
		to.size = static_cast<decltype(to.size)>(from.size);
		to.externalSize = static_cast<decltype(to.externalSize)>(from.externalSize);
		std::memcpy(to.blockSize, from.blockSize, sizeof(from.blockSize));
		return to;
	}
}
