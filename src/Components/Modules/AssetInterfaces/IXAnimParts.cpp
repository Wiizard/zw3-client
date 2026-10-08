#include "STDInclude.hpp"

#include "Components/Modules/AssetHandler.hpp"
#include "Components/Modules/FileSystem.hpp"
#include "Components/Modules/Logger.hpp"

#include "IXAnimParts.hpp"

namespace Assets
{
	constexpr std::int32_t animFileVersion = 3;
	constexpr char animFileMagic[] = "IW4xAnim";
	constexpr std::size_t animFileMagicLength = 8;

	constexpr std::size_t transIndicesOffset = offsetof(Game::XAnimPartTrans, u) + offsetof(Game::XAnimPartTransFrames, indices);
	constexpr std::size_t quat2IndicesOffset = offsetof(Game::XAnimDeltaPartQuat2, u) + offsetof(Game::XAnimDeltaPartQuatDataFrames2, indices);
	constexpr std::size_t quatIndicesOffset = offsetof(Game::XAnimDeltaPartQuat, u) + offsetof(Game::XAnimDeltaPartQuatDataFrames, indices);

	static std::unordered_set<std::string> dumpedPaths;

	static std::size_t IndexSize(unsigned short framecount)
	{
		if (framecount > 0xFF)
		{
			return sizeof(unsigned short);
		}

		return sizeof(unsigned char);
	}

	static std::uint32_t StoredPointer(const void* pointer)
	{
		if (!pointer)
		{
			return 0;
		}

		const auto low = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));

		if (!low)
		{
			return 0xFFFFFFFF;
		}

		return low;
	}

	template <typename T>
	static T* AllocateWithIndices(Utils::Memory::Allocator& allocator, std::size_t indicesOffset, std::size_t indicesBytes)
	{
		const auto bytes = std::max(sizeof(T), indicesOffset + indicesBytes);
		return static_cast<T*>(allocator.Allocate(bytes));
	}

	static Game::XAnimPartTrans* ReadTrans(Utils::Stream::Reader& reader, Utils::Memory::Allocator& allocator, unsigned short framecount)
	{
		const auto record = reader.Read<Game::X86::XAnimPartTrans>();

		if (!record.size)
		{
			auto* const trans = allocator.Allocate<Game::XAnimPartTrans>();
			trans->smallTrans = record.smallTrans;

			const auto* const frame0 = reader.Read(sizeof(trans->u.frame0), 1);
			std::memcpy(trans->u.frame0, frame0, sizeof(trans->u.frame0));
			return trans;
		}

		const std::size_t count = record.size + 1u;
		const auto indexSize = IndexSize(framecount);
		auto* const trans = AllocateWithIndices<Game::XAnimPartTrans>(allocator, transIndicesOffset, indexSize * count);
		trans->size = record.size;
		trans->smallTrans = record.smallTrans;

		const auto frames = reader.Read<Game::X86::XAnimPartTransFrames>();
		std::memcpy(trans->u.frames.mins, frames.mins, sizeof(trans->u.frames.mins));
		std::memcpy(trans->u.frames.size, frames.size, sizeof(trans->u.frames.size));

		const auto* const indices = reader.Read(indexSize, count);
		std::memcpy(trans->u.frames.indices._1, indices, indexSize * count);

		if (frames.frames._1)
		{
			if (trans->smallTrans)
			{
				trans->u.frames.frames._1 = static_cast<std::uint8_t(*)[3]>(reader.Read(3, count));
			}
			else
			{
				trans->u.frames.frames._2 = static_cast<unsigned short(*)[3]>(reader.Read(6, count));
			}
		}

		return trans;
	}

	static Game::XAnimDeltaPartQuat2* ReadQuat2(Utils::Stream::Reader& reader, Utils::Memory::Allocator& allocator, unsigned short framecount)
	{
		const auto record = reader.Read<Game::X86::XAnimDeltaPartQuat2>();

		if (!record.size)
		{
			auto* const quat2 = allocator.Allocate<Game::XAnimDeltaPartQuat2>();

			const auto* const frame0 = reader.Read(sizeof(quat2->u.frame0), 1);
			std::memcpy(quat2->u.frame0, frame0, sizeof(quat2->u.frame0));
			return quat2;
		}

		const std::size_t count = record.size + 1u;
		const auto indexSize = IndexSize(framecount);
		auto* const quat2 = AllocateWithIndices<Game::XAnimDeltaPartQuat2>(allocator, quat2IndicesOffset, indexSize * count);
		quat2->size = record.size;

		const auto frames = reader.Read<Game::X86::XAnimDeltaPartQuatDataFrames2>();

		const auto* const indices = reader.Read(indexSize, count);
		std::memcpy(quat2->u.frames.indices._1, indices, indexSize * count);

		if (frames.frames)
		{
			quat2->u.frames.frames = static_cast<short(*)[2]>(reader.Read(4, count));
		}

		return quat2;
	}

	static Game::XAnimDeltaPartQuat* ReadQuat(Utils::Stream::Reader& reader, Utils::Memory::Allocator& allocator, unsigned short framecount)
	{
		const auto record = reader.Read<Game::X86::XAnimDeltaPartQuat>();

		if (!record.size)
		{
			auto* const quat = allocator.Allocate<Game::XAnimDeltaPartQuat>();

			const auto* const frame0 = reader.Read(sizeof(quat->u.frame0), 1);
			std::memcpy(quat->u.frame0, frame0, sizeof(quat->u.frame0));
			return quat;
		}

		const std::size_t count = record.size + 1u;
		const auto indexSize = IndexSize(framecount);
		auto* const quat = AllocateWithIndices<Game::XAnimDeltaPartQuat>(allocator, quatIndicesOffset, indexSize * count);
		quat->size = record.size;

		const auto frames = reader.Read<Game::X86::XAnimDeltaPartQuatDataFrames>();

		const auto* const indices = reader.Read(indexSize, count);
		std::memcpy(quat->u.frames.indices._1, indices, indexSize * count);

		if (frames.frames)
		{
			quat->u.frames.frames = static_cast<short(*)[4]>(reader.Read(8, count));
		}

		return quat;
	}

	static Game::XAnimParts* ReadAnim(Utils::Stream::Reader& reader, Utils::Memory::Allocator& allocator)
	{
		const auto magic = reader.Read<std::uint64_t>();

		if (std::memcmp(&magic, animFileMagic, animFileMagicLength))
		{
			throw std::runtime_error("header is invalid");
		}

		const auto version = reader.Read<std::int32_t>();

		if (version > animFileVersion)
		{
			throw std::runtime_error(std::format("expected version is {}, but it was {}", animFileVersion, version));
		}

		const auto record = reader.Read<Game::X86::XAnimParts>();
		auto* const parts = allocator.Allocate<Game::XAnimParts>();
		*parts = Game::X86::Convert(record);
		parts->indices.data = nullptr;

		if (record.name)
		{
			parts->name = reader.ReadCString();
		}

		if (record.names)
		{
			const auto boneCount = parts->boneCount[Game::PART_TYPE_ALL];
			parts->names = allocator.AllocateArray<unsigned short>(boneCount);

			for (unsigned char i = 0; i < boneCount; ++i)
			{
				parts->names[i] = static_cast<unsigned short>(Game::SL_GetString(reader.ReadString().data(), 0));
			}
		}

		if (record.notify)
		{
			parts->notify = reader.ReadArray<Game::XAnimNotifyInfo>(parts->notifyCount);

			for (unsigned char i = 0; i < parts->notifyCount; ++i)
			{
				parts->notify[i].name = static_cast<unsigned short>(Game::SL_GetString(reader.ReadString().data(), 0));
			}
		}

		if (record.dataByte)
		{
			parts->dataByte = reader.ReadArray<char>(parts->dataByteCount);
		}

		if (record.dataShort)
		{
			parts->dataShort = reader.ReadArray<short>(parts->dataShortCount);
		}

		if (record.dataInt)
		{
			parts->dataInt = reader.ReadArray<int>(parts->dataIntCount);
		}

		if (record.randomDataByte)
		{
			parts->randomDataByte = reader.ReadArray<char>(parts->randomDataByteCount);
		}

		if (record.randomDataShort)
		{
			parts->randomDataShort = reader.ReadArray<short>(parts->randomDataShortCount);
		}

		if (record.randomDataInt)
		{
			parts->randomDataInt = reader.ReadArray<int>(parts->randomDataIntCount);
		}

		if (record.indices.data)
		{
			if (parts->numframes < 256)
			{
				parts->indices._1 = reader.ReadArray<char>(parts->indexCount);
			}
			else
			{
				parts->indices._2 = reader.ReadArray<unsigned short>(parts->indexCount);
			}
		}

		if (version > 1 && record.deltaPart)
		{
			const auto deltaRecord = reader.Read<Game::X86::XAnimDeltaPart>();
			auto* const delta = allocator.Allocate<Game::XAnimDeltaPart>();
			parts->deltaPart = delta;

			if (deltaRecord.trans)
			{
				delta->trans = ReadTrans(reader, allocator, parts->numframes);
			}

			if (version > 2 && deltaRecord.quat2)
			{
				delta->quat2 = ReadQuat2(reader, allocator, parts->numframes);
			}

			if (version > 2 && deltaRecord.quat)
			{
				delta->quat = ReadQuat(reader, allocator, parts->numframes);
			}
		}

		if (!reader.End())
		{
			throw std::runtime_error("remaining raw data found");
		}

		return parts;
	}

	static void WriteTrans(Utils::Stream& buffer, const Game::XAnimPartTrans* trans, unsigned short framecount)
	{
		Game::X86::XAnimPartTrans record{};
		record.size = trans->size;
		record.smallTrans = trans->smallTrans;

		if (!trans->size)
		{
			std::memcpy(record.u.frame0, trans->u.frame0, sizeof(record.u.frame0));
			buffer.SaveObject(record);
			buffer.SaveArray(trans->u.frame0, 3);
			return;
		}

		const std::size_t count = trans->size + 1u;
		const auto indexSize = IndexSize(framecount);

		Game::X86::XAnimPartTransFrames frames{};
		std::memcpy(frames.mins, trans->u.frames.mins, sizeof(frames.mins));
		std::memcpy(frames.size, trans->u.frames.size, sizeof(frames.size));
		frames.frames._1 = StoredPointer(trans->u.frames.frames._1);
		std::memcpy(&frames.indices, &trans->u.frames.indices, sizeof(frames.indices));

		record.u.frames = frames;
		buffer.SaveObject(record);
		buffer.SaveObject(frames);
		buffer.Save(trans->u.frames.indices._1, indexSize, count);

		if (!trans->u.frames.frames._1)
		{
			return;
		}

		if (trans->smallTrans)
		{
			buffer.Save(trans->u.frames.frames._1, 3, count);
		}
		else
		{
			buffer.Save(trans->u.frames.frames._2, 6, count);
		}
	}

	static void WriteQuat2(Utils::Stream& buffer, const Game::XAnimDeltaPartQuat2* quat2, unsigned short framecount)
	{
		Game::X86::XAnimDeltaPartQuat2 record{};
		record.size = quat2->size;

		if (!quat2->size)
		{
			std::memcpy(record.u.frame0, quat2->u.frame0, sizeof(record.u.frame0));
			buffer.SaveObject(record);
			buffer.SaveArray(quat2->u.frame0, 2);
			return;
		}

		const std::size_t count = quat2->size + 1u;

		Game::X86::XAnimDeltaPartQuatDataFrames2 frames{};
		frames.frames = StoredPointer(quat2->u.frames.frames);
		std::memcpy(&frames.indices, &quat2->u.frames.indices, sizeof(frames.indices));

		record.u.frames = frames;
		buffer.SaveObject(record);
		buffer.SaveObject(frames);
		buffer.Save(quat2->u.frames.indices._1, IndexSize(framecount), count);

		if (quat2->u.frames.frames)
		{
			buffer.Save(quat2->u.frames.frames, 4, count);
		}
	}

	static void WriteQuat(Utils::Stream& buffer, const Game::XAnimDeltaPartQuat* quat, unsigned short framecount)
	{
		Game::X86::XAnimDeltaPartQuat record{};
		record.size = quat->size;

		if (!quat->size)
		{
			std::memcpy(record.u.frame0, quat->u.frame0, sizeof(record.u.frame0));
			buffer.SaveObject(record);
			buffer.SaveArray(quat->u.frame0, 4);
			return;
		}

		const std::size_t count = quat->size + 1u;

		Game::X86::XAnimDeltaPartQuatDataFrames frames{};
		frames.frames = StoredPointer(quat->u.frames.frames);
		std::memcpy(&frames.indices, &quat->u.frames.indices, sizeof(frames.indices));

		record.u.frames = frames;
		buffer.SaveObject(record);
		buffer.SaveObject(frames);
		buffer.Save(quat->u.frames.indices._1, IndexSize(framecount), count);

		if (quat->u.frames.frames)
		{
			buffer.Save(quat->u.frames.frames, 8, count);
		}
	}

	void IXAnimParts::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		Components::FileSystem::File animFile(std::format("xanim/{}.iw4xAnim", name));

		if (!animFile.Exists())
		{
			return;
		}

		auto* const allocator = builder->GetAllocator();
		Utils::Stream::Reader reader(allocator, animFile.GetBuffer());

		try
		{
			header->parts = ReadAnim(reader, *allocator);
		}
		catch (const std::runtime_error& error)
		{
			Components::Logger::Error("Reading animation '{}' failed, {}\n", name, error.what());
		}
	}

	void IXAnimParts::Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		const auto* const asset = header.parts;

		if (asset->names)
		{
			for (unsigned char i = 0; i < asset->boneCount[Game::PART_TYPE_ALL]; ++i)
			{
				builder->AddScriptString(asset->names[i]);
			}
		}

		if (asset->notify)
		{
			for (unsigned char i = 0; i < asset->notifyCount; ++i)
			{
				builder->AddScriptString(asset->notify[i].name);
			}
		}
	}

	void IXAnimParts::SaveXAnimDeltaPart(const Game::XAnimDeltaPart* delta, unsigned short framecount, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::XAnimDeltaPart, 12);

		auto* const buffer = builder->GetBuffer();
		auto* const destDelta = buffer->Dest<Game::X86::XAnimDeltaPart>();
		const Game::X86::XAnimDeltaPart deltaRecord{};
		buffer->Save(&deltaRecord);

		const auto indexSize = IndexSize(framecount);

		if (delta->trans)
		{
			const auto* const trans = delta->trans;

			buffer->Align(Utils::Stream::ALIGN_4);

			Game::X86::XAnimPartTrans transRecord{};
			transRecord.size = trans->size;
			transRecord.smallTrans = trans->smallTrans;
			buffer->Save(&transRecord, 4);

			if (trans->size)
			{
				const std::size_t count = trans->size + 1u;

				Game::X86::XAnimPartTransFrames frames{};
				std::memcpy(frames.mins, trans->u.frames.mins, sizeof(frames.mins));
				std::memcpy(frames.size, trans->u.frames.size, sizeof(frames.size));

				if (trans->u.frames.frames._1)
				{
					Utils::Stream::ClearPointer(&frames.frames._1);
				}

				buffer->Save(&frames, 28);
				buffer->Save(trans->u.frames.indices._1, indexSize, count);

				if (trans->u.frames.frames._1)
				{
					if (trans->smallTrans)
					{
						buffer->Save(trans->u.frames.frames._1, 3, count);
					}
					else
					{
						buffer->Align(Utils::Stream::ALIGN_4);
						buffer->Save(trans->u.frames.frames._2, 6, count);
					}
				}
			}
			else
			{
				buffer->Save(trans->u.frame0, 12);
			}

			Utils::Stream::ClearPointer(&destDelta->trans);
		}

		if (delta->quat2)
		{
			const auto* const quat2 = delta->quat2;

			buffer->Align(Utils::Stream::ALIGN_4);

			Game::X86::XAnimDeltaPartQuat2 quat2Record{};
			quat2Record.size = quat2->size;
			buffer->Save(&quat2Record, 4);

			if (quat2->size)
			{
				const std::size_t count = quat2->size + 1u;

				std::uint32_t framesPointer = 0;

				if (quat2->u.frames.frames)
				{
					Utils::Stream::ClearPointer(&framesPointer);
				}

				buffer->SaveObject(framesPointer);
				buffer->Save(quat2->u.frames.indices._1, indexSize, count);

				if (quat2->u.frames.frames)
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					buffer->Save(quat2->u.frames.frames, 4, count);
				}
			}
			else
			{
				buffer->Save(quat2->u.frame0, 4);
			}

			Utils::Stream::ClearPointer(&destDelta->quat2);
		}

		if (delta->quat)
		{
			const auto* const quat = delta->quat;

			buffer->Align(Utils::Stream::ALIGN_4);

			Game::X86::XAnimDeltaPartQuat quatRecord{};
			quatRecord.size = quat->size;
			buffer->Save(&quatRecord, 4);

			if (quat->size)
			{
				const std::size_t count = quat->size + 1u;

				std::uint32_t framesPointer = 0;

				if (quat->u.frames.frames)
				{
					Utils::Stream::ClearPointer(&framesPointer);
				}

				buffer->SaveObject(framesPointer);
				buffer->Save(quat->u.frames.indices._1, indexSize, count);

				if (quat->u.frames.frames)
				{
					buffer->Align(Utils::Stream::ALIGN_4);
					buffer->Save(quat->u.frames.frames, 4, 2 * count);
				}
			}
			else
			{
				buffer->Save(quat->u.frame0, sizeof(short), 4);
			}

			Utils::Stream::ClearPointer(&destDelta->quat);
		}
	}

	void IXAnimParts::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		AssertSize(Game::X86::XAnimParts, 88);

		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.parts;
		auto* const dest = buffer->Dest<Game::X86::XAnimParts>();
		auto record = Game::X86::Convert(*asset);
		record.indices.data = 0;
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		if (asset->names)
		{
			buffer->Align(Utils::Stream::ALIGN_2);

			auto* const destTagnames = buffer->Dest<unsigned short>();
			buffer->SaveArray(asset->names, asset->boneCount[Game::PART_TYPE_ALL]);

			for (unsigned char i = 0; i < asset->boneCount[Game::PART_TYPE_ALL]; ++i)
			{
				builder->MapScriptString(destTagnames[i]);
			}

			Utils::Stream::ClearPointer(&dest->names);
		}

		if (asset->notify)
		{
			AssertSize(Game::XAnimNotifyInfo, 8);
			AssertSize(Game::X86::XAnimNotifyInfo, 8);

			buffer->Align(Utils::Stream::ALIGN_4);

			auto* const destNotetracks = buffer->Dest<Game::X86::XAnimNotifyInfo>();

			for (unsigned char i = 0; i < asset->notifyCount; ++i)
			{
				const auto notifyRecord = Game::X86::Convert(asset->notify[i]);
				buffer->Save(&notifyRecord);
			}

			for (unsigned char i = 0; i < asset->notifyCount; ++i)
			{
				builder->MapScriptString(destNotetracks[i].name);
			}

			Utils::Stream::ClearPointer(&dest->notify);
		}

		if (asset->deltaPart)
		{
			buffer->Align(Utils::Stream::ALIGN_4);

			this->SaveXAnimDeltaPart(asset->deltaPart, asset->numframes, builder);

			Utils::Stream::ClearPointer(&dest->deltaPart);
		}

		if (asset->dataByte)
		{
			buffer->SaveArray(asset->dataByte, asset->dataByteCount);
			Utils::Stream::ClearPointer(&dest->dataByte);
		}

		if (asset->dataShort)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(asset->dataShort, asset->dataShortCount);
			Utils::Stream::ClearPointer(&dest->dataShort);
		}

		if (asset->dataInt)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->dataInt, asset->dataIntCount);
			Utils::Stream::ClearPointer(&dest->dataInt);
		}

		if (asset->randomDataShort)
		{
			buffer->Align(Utils::Stream::ALIGN_2);
			buffer->SaveArray(asset->randomDataShort, asset->randomDataShortCount);
			Utils::Stream::ClearPointer(&dest->randomDataShort);
		}

		if (asset->randomDataByte)
		{
			buffer->SaveArray(asset->randomDataByte, asset->randomDataByteCount);
			Utils::Stream::ClearPointer(&dest->randomDataByte);
		}

		if (asset->randomDataInt)
		{
			buffer->Align(Utils::Stream::ALIGN_4);
			buffer->SaveArray(asset->randomDataInt, asset->randomDataIntCount);
			Utils::Stream::ClearPointer(&dest->randomDataInt);
		}

		if (asset->indices.data)
		{
			if (asset->numframes > 0xFF)
			{
				buffer->Align(Utils::Stream::ALIGN_2);
				buffer->SaveArray(asset->indices._2, asset->indexCount);
			}
			else
			{
				buffer->SaveArray(asset->indices._1, asset->indexCount);
			}

			Utils::Stream::ClearPointer(&dest->indices.data);
		}

		buffer->PopBlock();
	}

	void IXAnimParts::Dump(Game::XAssetHeader header)
	{
		const auto* const parts = header.parts;
		const auto path = std::format("{}/xanim/{}.iw4xAnim", Components::ZoneBuilder::GetDumpingZonePath(), parts->name);

		if (!dumpedPaths.insert(path).second)
		{
			return;
		}

		Utils::Stream buffer;

		buffer.Save(animFileMagic, animFileMagicLength);
		buffer.SaveObject(animFileVersion);

		auto record = Game::X86::Convert(*parts);
		record.name = StoredPointer(parts->name);
		record.names = StoredPointer(parts->names);
		record.dataByte = StoredPointer(parts->dataByte);
		record.dataShort = StoredPointer(parts->dataShort);
		record.dataInt = StoredPointer(parts->dataInt);
		record.randomDataShort = StoredPointer(parts->randomDataShort);
		record.randomDataByte = StoredPointer(parts->randomDataByte);
		record.randomDataInt = StoredPointer(parts->randomDataInt);
		record.indices.data = StoredPointer(parts->indices.data);
		record.notify = StoredPointer(parts->notify);
		record.deltaPart = StoredPointer(parts->deltaPart);
		buffer.SaveObject(record);

		if (parts->name)
		{
			buffer.SaveString(parts->name);
		}

		if (parts->names)
		{
			for (unsigned char i = 0; i < parts->boneCount[Game::PART_TYPE_ALL]; ++i)
			{
				buffer.SaveString(Game::SL_ConvertToString(parts->names[i]));
			}
		}

		if (parts->notify)
		{
			for (unsigned char i = 0; i < parts->notifyCount; ++i)
			{
				const auto notifyRecord = Game::X86::Convert(parts->notify[i]);
				buffer.SaveObject(notifyRecord);
			}

			for (unsigned char i = 0; i < parts->notifyCount; ++i)
			{
				buffer.SaveString(Game::SL_ConvertToString(parts->notify[i].name));
			}
		}

		if (parts->dataByte)
		{
			buffer.SaveArray(parts->dataByte, parts->dataByteCount);
		}

		if (parts->dataShort)
		{
			buffer.SaveArray(parts->dataShort, parts->dataShortCount);
		}

		if (parts->dataInt)
		{
			buffer.SaveArray(parts->dataInt, parts->dataIntCount);
		}

		if (parts->randomDataByte)
		{
			buffer.SaveArray(parts->randomDataByte, parts->randomDataByteCount);
		}

		if (parts->randomDataShort)
		{
			buffer.SaveArray(parts->randomDataShort, parts->randomDataShortCount);
		}

		if (parts->randomDataInt)
		{
			buffer.SaveArray(parts->randomDataInt, parts->randomDataIntCount);
		}

		if (parts->indices.data)
		{
			if (parts->numframes < 256)
			{
				buffer.SaveArray(parts->indices._1, parts->indexCount);
			}
			else
			{
				buffer.SaveArray(parts->indices._2, parts->indexCount);
			}
		}

		if (parts->deltaPart)
		{
			const auto* const delta = parts->deltaPart;

			Game::X86::XAnimDeltaPart deltaRecord{};
			deltaRecord.trans = StoredPointer(delta->trans);
			deltaRecord.quat2 = StoredPointer(delta->quat2);
			deltaRecord.quat = StoredPointer(delta->quat);
			buffer.SaveObject(deltaRecord);

			if (delta->trans)
			{
				WriteTrans(buffer, delta->trans, parts->numframes);
			}

			if (delta->quat2)
			{
				WriteQuat2(buffer, delta->quat2, parts->numframes);
			}

			if (delta->quat)
			{
				WriteQuat(buffer, delta->quat, parts->numframes);
			}
		}

		if (!Utils::IO::WriteFile(path, buffer.ToBuffer()))
		{
			Components::Logger::Error("xanim: could not write {}\n", path);
		}
	}
}
