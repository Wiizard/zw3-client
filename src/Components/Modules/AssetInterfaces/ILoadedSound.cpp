#include "STDInclude.hpp"

#include "../AssetHandler.hpp"
#include "../FileSystem.hpp"
#include "../Logger.hpp"
#include "ILoadedSound.hpp"

namespace Assets
{
	constexpr std::uint16_t pcmFormat = 1;
	constexpr std::uint16_t imaAdpcmFormat = 17;

	constexpr std::uint32_t riffId = 0x46464952;
	constexpr std::uint32_t waveId = 0x45564157;
	constexpr std::uint32_t formatId = 0x20746D66;
	constexpr std::uint32_t dataId = 0x61746164;

	static std::string GetSoundPath(const std::string& name)
	{
		return std::format("loaded_sound/{}", name);
	}

	template <typename T> static T ReadWave(const std::string& wave, std::size_t position)
	{
		T value{};
		std::memcpy(&value, wave.data() + position, sizeof(T));
		return value;
	}

	static Game::LoadedSound* TryReadSound(const std::string& name, Utils::Memory::Allocator* allocator)
	{
		Components::FileSystem::File file(GetSoundPath(name));

		if (!file.Exists())
		{
			return nullptr;
		}

		const auto& wave = file.GetBuffer();

		if (wave.size() < 12 || ReadWave<std::uint32_t>(wave, 0) != riffId || ReadWave<std::uint32_t>(wave, 8) != waveId)
		{
			Components::Logger::Error("Reading sound '{}' failed, header is invalid!\n", name);
			return nullptr;
		}

		Game::MssSound info{};
		bool hasFormat = false;
		std::size_t position = 12;

		while (!info.data && position + 8 <= wave.size())
		{
			const auto chunkId = ReadWave<std::uint32_t>(wave, position);
			const auto chunkSize = ReadWave<std::uint32_t>(wave, position + 4);
			position += 8;

			if (position + chunkSize > wave.size())
			{
				Components::Logger::Error("Reading sound '{}' failed, a chunk runs past the end of the file!\n", name);
				return nullptr;
			}

			if (chunkId == formatId && chunkSize >= 16)
			{
				info.formatTag = ReadWave<std::uint16_t>(wave, position);

				if (info.formatTag != pcmFormat && info.formatTag != imaAdpcmFormat)
				{
					Components::Logger::Error("Reading sound '{}' failed, invalid format!\n", name);
					return nullptr;
				}

				info.channels = ReadWave<std::uint16_t>(wave, position + 2);
				info.rate = ReadWave<std::uint32_t>(wave, position + 4);
				info.averageBytes = ReadWave<std::uint32_t>(wave, position + 8);
				info.blockAlign = ReadWave<std::uint16_t>(wave, position + 12);
				info.bits = ReadWave<std::uint16_t>(wave, position + 14);
				hasFormat = true;
			}

			if (chunkId == dataId && hasFormat)
			{
				info.dataLength = chunkSize;
				info.data = allocator->AllocateArray<char>(chunkSize);
				std::memcpy(info.data, wave.data() + position, chunkSize);
			}

			position += chunkSize;
		}

		if (!info.data)
		{
			Components::Logger::Error("Reading sound '{}' failed, missing sound ptr !\n", name);
			return nullptr;
		}

		auto* const sound = allocator->Allocate<Game::LoadedSound>();
		sound->name = allocator->DuplicateString(name);
		std::memcpy(&sound->sound, &info, sizeof(info));

		return sound;
	}

	void ILoadedSound::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		header->loadSnd = TryReadSound(name, builder->GetAllocator());
	}

	void ILoadedSound::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.loadSnd;

		Game::MssSound info{};
		std::memcpy(&info, &asset->sound, sizeof(info));

		Game::X86::LoadedSound converted{};

		if (info.data)
		{
			const bool canLoad = (info.formatTag == pcmFormat || info.formatTag == imaAdpcmFormat) && info.bits > 0;

			if (!canLoad)
			{
				Components::Logger::Fatal("Sound '{}' is in format {} with {} bits, which 1.2.211 cannot load. Export failed!", asset->name, info.formatTag, info.bits);
			}

			converted.sound.info.format = info.formatTag;
			converted.sound.info.data_len = info.dataLength;
			converted.sound.info.rate = info.rate;
			converted.sound.info.bits = info.bits;
			converted.sound.info.channels = info.channels;
			converted.sound.info.samples = static_cast<std::uint32_t>(static_cast<std::uint64_t>(info.dataLength) * 8 / info.bits);
			converted.sound.info.block_size = info.blockAlign;
		}

		auto* const dest = buffer->Dest<Game::X86::LoadedSound>();
		buffer->Save(&converted);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		buffer->PushBlock(Game::XFILE_BLOCK_TEMP);

		if (info.data)
		{
			buffer->Save(info.data, info.dataLength);
			Utils::Stream::ClearPointer(&dest->sound.data);
		}

		buffer->PopBlock();
		buffer->PopBlock();
	}

	void ILoadedSound::Dump(Game::XAssetHeader header)
	{
		const auto* const asset = header.loadSnd;

		Game::MssSound info{};
		std::memcpy(&info, &asset->sound, sizeof(info));

		if (!asset->name)
		{
			return;
		}

		if (!info.data)
		{
			Components::Logger::Error("Tried to save sound {} which was never loaded before!\n", asset->name);
			return;
		}

		std::string wave;

		const auto append = [&wave](const auto value)
		{
			wave.append(reinterpret_cast<const char*>(&value), sizeof(value));
		};

		const std::uint32_t formatSize = 16;
		const std::uint32_t byteRate = info.rate * info.channels * info.bits / 8;

		append(riffId);
		append(static_cast<std::uint32_t>(4 + (8 + formatSize) + (8 + info.dataLength)));
		append(waveId);
		append(formatId);
		append(formatSize);
		append(info.formatTag);
		append(info.channels);
		append(info.rate);
		append(byteRate);
		append(info.blockAlign);
		append(info.bits);
		append(dataId);
		append(info.dataLength);
		wave.append(info.data, info.dataLength);

		Utils::IO::WriteFile(std::format("{}/{}", Components::ZoneBuilder::GetDumpingZonePath(), GetSoundPath(asset->name)), wave);
	}
}
