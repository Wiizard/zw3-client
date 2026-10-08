#include "STDInclude.hpp"

#include <mmsystem.h>

#include "Sound.hpp"
#include "Dedicated.hpp"
#include "LobbyScene.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "ZoneBuilder.hpp"

extern "C"
{
	void DirectSoundCheckStub();
	void VoiceLoopStub();
	void SND_ParseStreamHeaderStub();
	void SoundMasterVolumeStub();

	bool Sound_ParseStreamHeader(void* header, const std::uint8_t* data, std::uint32_t size, const std::uint8_t* stream);

	std::uintptr_t Sound_DirectSound = 0;
	std::uintptr_t Sound_CreateBufferNext = 0;
	std::uintptr_t Sound_CreateBufferFail = 0;
	std::uintptr_t Sound_VoiceLoopNext = 0;
	std::uintptr_t Sound_VoiceInitReturn = 0;

	float Sound_FrontendGain = 1.0f;
	std::uintptr_t Sound_MasterVolume = 0;
	std::uintptr_t Sound_MasterVolumeNext = 0;
}

namespace Components
{
	constexpr std::uintptr_t snd_volume = 0x146516510;
	constexpr std::uintptr_t g_sndVolume = 0x14651A554;

	constexpr std::uintptr_t SND_Update_SND_UpdatePauseCall = 0x14024A871;
	constexpr std::uintptr_t SND_UpdatePause = 0x14024B090;
	constexpr std::uintptr_t SND_Update_VolumeStore = 0x14024A9A4;
	constexpr std::uintptr_t SND_Update_VolumeStoreNext = 0x14024A9AC;
	static const std::uint8_t volumeStore[] = { 0xF3, 0x0F, 0x11, 0x05, 0xA8, 0xFB, 0x2C, 0x06 };

	constexpr DWORD frontendFadeMs = 350;

	static Utils::Hook updatePauseHook;
	static Utils::Hook volumeStoreHook;

	struct LobbyVoice
	{
		HWAVEOUT device = nullptr;
		WAVEHDR header{};
		std::vector<short> samples;
	};

	static std::mutex lobbyAudioMutex;
	static WAVEFORMATEX lobbyAudioFormat{};
	static std::vector<short> lobbyAudioSamples;
	static std::unique_ptr<LobbyVoice> lobbyVoice;
	static bool isLobbyAudioStopping = false;

	static void ReleaseLobbyVoice(bool shouldStop)
	{
		if (!lobbyVoice)
		{
			return;
		}

		if (shouldStop)
		{
			waveOutReset(lobbyVoice->device);
		}

		const bool isDone = (lobbyVoice->header.dwFlags & WHDR_DONE) != 0;

		if (!shouldStop && !isDone)
		{
			return;
		}

		if (waveOutUnprepareHeader(lobbyVoice->device, &lobbyVoice->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
		{
			return;
		}

		if (waveOutClose(lobbyVoice->device) != MMSYSERR_NOERROR)
		{
			return;
		}

		lobbyVoice.reset();
	}

	static void SND_UpdatePause_Hk()
	{
		static bool wasHeld = false;
		static DWORD fadeStart = 0;

		if (LobbyScene::IsCinematicActive())
		{
			if (wasHeld || fadeStart != 0)
			{
				auto* const volume = Utils::Hook::Get<Game::dvar_t*>(snd_volume);

				if (volume)
				{
					volume->modified = true;
				}
			}

			wasHeld = false;
			fadeStart = 0;

			reinterpret_cast<void(*)()>(Utils::Hook::Rebase(SND_UpdatePause))();
			Sound_FrontendGain = 1.0f;
			return;
		}

		const bool isHeld = LobbyScene::IsStartupLoading();

		if (isHeld)
		{
			wasHeld = true;
			fadeStart = 0;
		}
		else if (wasHeld)
		{
			wasHeld = false;
			fadeStart = timeGetTime();
		}

		const bool isAdjusting = isHeld || fadeStart != 0;

		if (isAdjusting)
		{
			auto* const volume = Utils::Hook::Get<Game::dvar_t*>(snd_volume);

			if (volume)
			{
				volume->modified = true;
			}
		}

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(SND_UpdatePause))();

		if (!isAdjusting)
		{
			Sound_FrontendGain = 1.0f;
			return;
		}

		if (isHeld)
		{
			Sound_FrontendGain = 0.0f;
			return;
		}

		const DWORD elapsedMs = timeGetTime() - fadeStart;

		Sound_FrontendGain = std::min(static_cast<float>(elapsedMs) / static_cast<float>(frontendFadeMs), 1.0f);

		if (elapsedMs >= frontendFadeMs)
		{
			fadeStart = 0;
		}
	}

	void Sound::PrepareLobbyRoundStart(const std::string& wav)
	{
		const bool isWave = wav.size() >= 44 && wav.size() <= 4 * 1024 * 1024
			&& std::memcmp(wav.data(), "RIFF", 4) == 0
			&& std::memcmp(wav.data() + 8, "WAVE", 4) == 0;

		if (!isWave)
		{
			return;
		}

		WAVEFORMATEX format{};
		std::vector<short> samples;

		for (std::size_t offset = 12; offset + 8 <= wav.size();)
		{
			std::uint32_t length = 0;
			std::memcpy(&length, wav.data() + offset + 4, sizeof(length));

			if (length > wav.size() - offset - 8)
			{
				return;
			}

			if (std::memcmp(wav.data() + offset, "fmt ", 4) == 0 && length >= 16)
			{
				std::memcpy(&format, wav.data() + offset + 8, 16);
			}
			else if (std::memcmp(wav.data() + offset, "data", 4) == 0)
			{
				if (length % sizeof(short))
				{
					return;
				}

				samples.resize(length / sizeof(short));
				std::memcpy(samples.data(), wav.data() + offset + 8, length);
			}

			offset += 8 + static_cast<std::size_t>(length) + (length & 1u);
		}

		const bool isPlayable = format.wFormatTag == WAVE_FORMAT_PCM
			&& format.wBitsPerSample == 16
			&& (format.nChannels == 1 || format.nChannels == 2)
			&& format.nSamplesPerSec >= 8000
			&& format.nSamplesPerSec <= 48000
			&& format.nBlockAlign == format.nChannels * 2
			&& format.nAvgBytesPerSec == format.nSamplesPerSec * format.nBlockAlign
			&& !samples.empty()
			&& samples.size() % format.nChannels == 0;

		if (!isPlayable)
		{
			return;
		}

		std::lock_guard lock(lobbyAudioMutex);

		if (isLobbyAudioStopping)
		{
			return;
		}

		lobbyAudioFormat = format;
		lobbyAudioSamples = std::move(samples);
	}

	void Sound::PlayLobbyRoundStart()
	{
		const auto* const volume = Utils::Hook::Get<Game::dvar_t*>(snd_volume);
		float gain = 0.0f;

		if (volume)
		{
			gain = std::clamp(volume->current.value, 0.0f, 1.0f);
		}

		if (gain <= 0.0f)
		{
			return;
		}

		Scheduler::Once([gain]
		{
			std::lock_guard lock(lobbyAudioMutex);

			ReleaseLobbyVoice(false);

			if (isLobbyAudioStopping || lobbyVoice || lobbyAudioSamples.empty())
			{
				return;
			}

			auto voice = std::make_unique<LobbyVoice>();
			voice->samples = lobbyAudioSamples;

			for (auto& sample : voice->samples)
			{
				sample = static_cast<short>(sample * gain);
			}

			if (waveOutOpen(&voice->device, WAVE_MAPPER, &lobbyAudioFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
			{
				return;
			}

			voice->header.lpData = reinterpret_cast<LPSTR>(voice->samples.data());
			voice->header.dwBufferLength = static_cast<DWORD>(voice->samples.size() * sizeof(short));

			if (waveOutPrepareHeader(voice->device, &voice->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			{
				waveOutClose(voice->device);
				return;
			}

			if (waveOutWrite(voice->device, &voice->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
			{
				waveOutUnprepareHeader(voice->device, &voice->header, sizeof(WAVEHDR));
				waveOutClose(voice->device);
				return;
			}

			lobbyVoice = std::move(voice);
		}, Scheduler::Pipeline::ASYNC);
	}

	static void InstallFrontendVolume()
	{
		const bool isExpected = Utils::Hook::BranchesTo(SND_Update_SND_UpdatePauseCall, SND_UpdatePause, HOOK_CALL)
			&& Utils::Hook::MatchesBytes(SND_Update_VolumeStore, volumeStore, sizeof(volumeStore));

		if (!isExpected)
		{
			Logger::Error("sound: SND_Update's volume code does not read as expected, the menu is not muted while the lobby scene loads\n");
			return;
		}

		Sound_MasterVolume = Utils::Hook::Rebase(g_sndVolume);
		Sound_MasterVolumeNext = Utils::Hook::Rebase(SND_Update_VolumeStoreNext);

		bool isSeated = updatePauseHook.Initialize(SND_Update_SND_UpdatePauseCall, reinterpret_cast<void*>(SND_UpdatePause_Hk), HOOK_CALL)->Install()->IsInstalled();
		isSeated = volumeStoreHook.Initialize(SND_Update_VolumeStore, SoundMasterVolumeStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			updatePauseHook.Uninstall();
			volumeStoreHook.Uninstall();

			Logger::Error("sound: could not seat the menu volume hooks, the menu is not muted while the lobby scene loads\n");
			return;
		}

		Utils::Hook::Nop(SND_Update_VolumeStore + 5, sizeof(volumeStore) - 5);
	}

	constexpr std::uintptr_t CreateBuffer_DirectSoundLoad = 0x1401AD07D;
	constexpr std::uintptr_t CreateBuffer_Next = 0x1401AD084;
	constexpr std::uintptr_t CreateBuffer_Fail = 0x1401AD0DA;
	constexpr std::uintptr_t directSound = 0x1419F45A8;
	static const std::uint8_t directSoundLoad[] = { 0x48, 0x8B, 0x0D, 0x24, 0x75, 0x84, 0x01 };

	constexpr std::uintptr_t Voice_Init_BufferStore = 0x1402A9E6C;
	constexpr std::uintptr_t Voice_Init_LoopNext = 0x1402A9E73;
	constexpr std::uintptr_t Voice_Init_Return = 0x1402A9E7A;
	static const std::uint8_t bufferStore[] = { 0x48, 0x89, 0x03, 0x48, 0x83, 0xC3, 0x08 };

	static Utils::Hook hooks[2];

	constexpr std::uintptr_t SND_OpenStream_ParseCall = 0x1402C4689;
	constexpr std::uintptr_t SND_ParseStreamHeader = 0x1402BC840;
	constexpr std::uintptr_t streamSampleRateRows[] = { 0x1403A9FC0, 0x1403E4E00, 0x1403A9FD0, 0x1403A9FE0 };
	constexpr std::size_t streamFileLength = 0x84;

	struct StreamHeader
	{
		std::uint32_t dataOffset;
		std::uint8_t pad04[12];
		const std::uint8_t* data;
		std::uint64_t size;
		std::uint64_t readPosition;
		std::int32_t rate;
		std::int32_t dataSize;
		std::int32_t samples;
		std::int32_t averageBytes;
		std::uint16_t channels;
		std::uint16_t bits;
		std::uint16_t blockAlign;
		std::int8_t format;
		std::uint8_t pad3F[41];
		std::uint32_t seekCount;
		std::uint8_t pad6C[4];
		const std::uint8_t* seekTable;
	};

	static_assert(sizeof(StreamHeader) == 0x78);
	static_assert(offsetof(StreamHeader, rate) == 40);
	static_assert(offsetof(StreamHeader, channels) == 56);
	static_assert(offsetof(StreamHeader, format) == 62);
	static_assert(offsetof(StreamHeader, seekCount) == 104);
	static_assert(offsetof(StreamHeader, seekTable) == 112);

	constexpr std::int8_t streamFormatMp3 = 5;

	static Utils::Hook streamHeaderHook;

	struct Mp3Frame
	{
		std::uint32_t offset;
		int rate;
		int channels;
		int samplesPerFrame;
		int bitrate;
		int length;
		bool isMpeg1;
	};

	static std::uint32_t ReadBigEndian32(const std::uint8_t* data)
	{
		return (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16) | (static_cast<std::uint32_t>(data[2]) << 8) | data[3];
	}

	static std::uint32_t ReadBigEndian16(const std::uint8_t* data)
	{
		return (static_cast<std::uint32_t>(data[0]) << 8) | data[1];
	}

	static bool TryReadFrame(const std::uint8_t* data, const std::uint32_t size, const std::uint32_t offset, Mp3Frame& frame)
	{
		static const int bitrates[2][3][15] =
		{
			{
				{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256 },
				{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
				{ 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 },
			},
			{
				{ 0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448 },
				{ 0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384 },
				{ 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 },
			},
		};

		static const int rates[4][3] =
		{
			{ 11025, 12000, 8000 },
			{ 0, 0, 0 },
			{ 22050, 24000, 16000 },
			{ 44100, 48000, 32000 },
		};

		if (offset + 4 > size)
		{
			return false;
		}

		const auto header = ReadBigEndian32(data + offset);

		if ((header & 0xFFE00000) != 0xFFE00000)
		{
			return false;
		}

		const auto version = (header >> 19) & 3;
		const auto layerBits = (header >> 17) & 3;
		const auto bitrateIndex = (header >> 12) & 0xF;
		const auto rateIndex = (header >> 10) & 3;

		if (version == 1 || layerBits == 0 || bitrateIndex == 0 || bitrateIndex == 15 || rateIndex == 3)
		{
			return false;
		}

		const int layer = 4 - static_cast<int>(layerBits);
		const bool isMpeg1 = version == 3;
		const int padding = (header >> 9) & 1;

		frame.offset = offset;
		frame.isMpeg1 = isMpeg1;
		frame.rate = rates[version][rateIndex];
		frame.channels = 2;

		if (((header >> 6) & 3) == 3)
		{
			frame.channels = 1;
		}

		frame.bitrate = bitrates[isMpeg1 ? 1 : 0][layer - 1][bitrateIndex] * 1000;

		if (layer == 1)
		{
			frame.samplesPerFrame = 384;
			frame.length = (12 * frame.bitrate / frame.rate + padding) * 4;
		}
		else
		{
			frame.samplesPerFrame = 1152;

			if (layer == 3 && !isMpeg1)
			{
				frame.samplesPerFrame = 576;
			}

			frame.length = frame.samplesPerFrame / 8 * frame.bitrate / frame.rate + padding;
		}

		return frame.length > 4;
	}

	static bool TryFindFirstFrame(const std::uint8_t* data, const std::uint32_t size, Mp3Frame& frame)
	{
		constexpr std::uint32_t searchLimit = 0x10000;

		std::uint32_t start = 0;

		if (size >= 10 && data[0] == 'I' && data[1] == 'D' && data[2] == '3')
		{
			start = (((data[6] & 0x7F) << 21) | ((data[7] & 0x7F) << 14) | ((data[8] & 0x7F) << 7) | (data[9] & 0x7F)) + 10;

			if (data[5] & 0x10)
			{
				start += 10;
			}
		}

		for (auto offset = start; offset < size && offset < start + searchLimit; ++offset)
		{
			if (!TryReadFrame(data, size, offset, frame))
			{
				continue;
			}

			Mp3Frame next{};
			const auto nextOffset = offset + static_cast<std::uint32_t>(frame.length);

			if (nextOffset + 4 > size)
			{
				return true;
			}

			if (TryReadFrame(data, size, nextOffset, next) && next.rate == frame.rate && next.isMpeg1 == frame.isMpeg1)
			{
				return true;
			}
		}

		return false;
	}

	static bool IsMisreadByEngine(const std::uint8_t* data, const std::uint32_t size)
	{
		constexpr std::uint32_t riff = 0x46464952;
		constexpr std::uint32_t id3v23 = 0x03334449;
		constexpr std::uint32_t ownHeaderSize = 36;

		if (size < ownHeaderSize)
		{
			return true;
		}

		std::uint32_t magic = 0;
		std::memcpy(&magic, data, sizeof(magic));

		if (magic == riff)
		{
			return false;
		}

		const auto ownHeaderSamples = static_cast<std::int32_t>(ReadBigEndian32(data + 24)) - static_cast<std::int32_t>(ReadBigEndian16(data + 32)) - static_cast<std::int32_t>(ReadBigEndian16(data + 30));

		std::uint32_t frameOffset = 0;
		std::int32_t samples = ownHeaderSamples;

		if (magic == id3v23)
		{
			frameOffset = (((data[6] & 0x7F) << 21) | ((data[7] & 0x7F) << 14) | ((data[8] & 0x7F) << 7) | (data[9] & 0x7F)) + 10;

			const auto tagWord = ReadBigEndian32(data + 20);

			if (tagWord < 0x80000000)
			{
				samples = static_cast<std::int32_t>(tagWord);
			}
			else if (tagWord != 0x80000001)
			{
				samples = 0;
			}
		}

		if (frameOffset + 3 >= size)
		{
			return true;
		}

		const std::uint32_t frameHeader = ReadBigEndian32(data + frameOffset) & 0xFFFFFF;
		const auto* const rates = reinterpret_cast<const std::int32_t*>(Utils::Hook::Rebase(streamSampleRateRows[(frameHeader >> 19) & 3]));
		const std::int64_t rate = rates[(frameHeader >> 10) & 3];

		if ((1000LL * samples) / rate == 0)
		{
			return true;
		}

		Mp3Frame frame{};

		if (!TryFindFirstFrame(data, size, frame))
		{
			return false;
		}

		int channels = 2;

		if ((frameHeader & 0xC0) == 0xC0)
		{
			channels = 1;
		}

		if (rate != frame.rate || channels != frame.channels)
		{
			return true;
		}

		const bool hasTag = data[0] == 'I' && data[1] == 'D' && data[2] == '3';
		const bool isOwnTag = magic == id3v23 && std::memcmp(data + 10, "PRIV", 4) == 0;

		return hasTag && !isOwnTag;
	}

	static std::int64_t CountSamples(const std::uint8_t* data, const std::uint32_t size, const Mp3Frame& frame, const std::int64_t fileLength)
	{
		int sideInfo = frame.isMpeg1 ? 32 : 17;

		if (frame.channels == 1)
		{
			sideInfo = frame.isMpeg1 ? 17 : 9;
		}

		const auto xing = frame.offset + 4 + sideInfo;

		if (xing + 12 <= size && (std::memcmp(data + xing, "Xing", 4) == 0 || std::memcmp(data + xing, "Info", 4) == 0) && (ReadBigEndian32(data + xing + 4) & 1))
		{
			return static_cast<std::int64_t>(ReadBigEndian32(data + xing + 8)) * frame.samplesPerFrame;
		}

		const auto vbri = frame.offset + 36;

		if (vbri + 18 <= size && std::memcmp(data + vbri, "VBRI", 4) == 0)
		{
			return static_cast<std::int64_t>(ReadBigEndian32(data + vbri + 14)) * frame.samplesPerFrame;
		}

		auto bytes = fileLength - frame.offset;

		if (bytes <= 0)
		{
			bytes = static_cast<std::int64_t>(size) - frame.offset;
		}

		return bytes * 8 * frame.rate / frame.bitrate;
	}

	static bool TryFillMp3Header(StreamHeader* header, const std::uint8_t* data, const std::uint32_t size, const std::int64_t fileLength)
	{
		Mp3Frame frame{};

		if (!TryFindFirstFrame(data, size, frame))
		{
			return false;
		}

		const auto samples = CountSamples(data, size, frame, fileLength);

		if (samples <= 0 || samples > std::numeric_limits<std::int32_t>::max() || (1000 * samples) / frame.rate == 0)
		{
			return false;
		}

		std::memset(header, 0, sizeof(StreamHeader));

		header->data = data;
		header->size = size;
		header->readPosition = 8;
		header->dataOffset = frame.offset;
		header->rate = frame.rate;
		header->dataSize = static_cast<std::int32_t>(size - frame.offset);
		header->samples = static_cast<std::int32_t>(samples);
		header->averageBytes = static_cast<std::int32_t>((2 * samples) / ((1000 * samples) / frame.rate) / 1000);
		header->channels = static_cast<std::uint16_t>(frame.channels);
		header->bits = 16;
		header->blockAlign = static_cast<std::uint16_t>(2 * frame.channels);
		header->format = streamFormatMp3;

		return true;
	}

	extern "C" bool Sound_ParseStreamHeader(void* header, const std::uint8_t* data, const std::uint32_t size, const std::uint8_t* stream)
	{
		if (!IsMisreadByEngine(data, size))
		{
			return reinterpret_cast<bool(*)(void*, const std::uint8_t*, std::uint32_t)>(Utils::Hook::Rebase(SND_ParseStreamHeader))(header, data, size);
		}

		std::int32_t fileLength = 0;
		std::memcpy(&fileLength, stream + streamFileLength, sizeof(fileLength));

		if (TryFillMp3Header(static_cast<StreamHeader*>(header), data, size, fileLength))
		{
			return true;
		}

		static bool isReported = false;

		if (!isReported)
		{
			isReported = true;
			Logger::Warning("sound: a streamed sound has no MP3 frame 2.0.13 can read, so it is skipped\n");
		}

		return false;
	}

	Sound::Sound()
	{
		if (!Dedicated::IsEnabled() && !ZoneBuilder::IsEnabled())
		{
			InstallFrontendVolume();

			Scheduler::Loop([]
			{
				std::lock_guard lock(lobbyAudioMutex);
				ReleaseLobbyVoice(false);
			}, Scheduler::Pipeline::ASYNC, 100ms);

			Scheduler::OnShutdown([]
			{
				std::lock_guard lock(lobbyAudioMutex);
				isLobbyAudioStopping = true;
				ReleaseLobbyVoice(true);
				lobbyAudioSamples.clear();
			});
		}

		if (!Utils::Hook::BranchesTo(SND_OpenStream_ParseCall, SND_ParseStreamHeader, HOOK_CALL)
			|| !streamHeaderHook.Initialize(SND_OpenStream_ParseCall, SND_ParseStreamHeaderStub, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("sound: the stream header parse does not read as expected, a plain MP3 still crashes the stream thread\n");
		}
		else
		{
			streamHeaderHook.Quick();
		}

		const bool isExpected = Utils::Hook::MatchesBytes(CreateBuffer_DirectSoundLoad, directSoundLoad, sizeof(directSoundLoad))
			&& Utils::Hook::MatchesBytes(Voice_Init_BufferStore, bufferStore, sizeof(bufferStore));

		if (!isExpected)
		{
			Logger::Error("sound: the voice buffer code does not read as expected, left alone\n");
			return;
		}

		Sound_DirectSound = Utils::Hook::Rebase(directSound);
		Sound_CreateBufferNext = Utils::Hook::Rebase(CreateBuffer_Next);
		Sound_CreateBufferFail = Utils::Hook::Rebase(CreateBuffer_Fail);
		Sound_VoiceLoopNext = Utils::Hook::Rebase(Voice_Init_LoopNext);
		Sound_VoiceInitReturn = Utils::Hook::Rebase(Voice_Init_Return);

		bool isSeated = hooks[0].Initialize(CreateBuffer_DirectSoundLoad, DirectSoundCheckStub, HOOK_JUMP)->Install()->IsInstalled();
		isSeated = hooks[1].Initialize(Voice_Init_BufferStore, VoiceLoopStub, HOOK_JUMP)->Install()->IsInstalled() && isSeated;

		if (!isSeated)
		{
			for (auto& hook : hooks)
			{
				hook.Uninstall();
			}

			Logger::Error("sound: could not seat the voice buffer hooks\n");
			return;
		}

		Utils::Hook::Nop(CreateBuffer_DirectSoundLoad + 5, sizeof(directSoundLoad) - 5);
		Utils::Hook::Nop(Voice_Init_BufferStore + 5, sizeof(bufferStore) - 5);
	}
}
