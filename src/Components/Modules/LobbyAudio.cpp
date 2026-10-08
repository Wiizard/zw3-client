#include "STDInclude.hpp"

#include <mmsystem.h>

#include "LobbyAudio.hpp"
#include "Scheduler.hpp"

namespace Components
{
	struct LobbyAudioVoice
	{
		HWAVEOUT device = nullptr;
		WAVEHDR header{};
		std::vector<short> samples;
	};

	static std::mutex lobbyAudioMutex;
	static WAVEFORMATEX lobbyAudioFormat{};
	static std::vector<short> lobbyAudioSamples;
	static std::unique_ptr<LobbyAudioVoice> lobbyVoice;
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

	void LobbyAudio::PrepareLobbyRoundStart(const std::string& wav)
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

	void LobbyAudio::PlayLobbyRoundStart()
	{
		const auto* const volume = Game::Dvar_FindVar("snd_volume");
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

			auto voice = std::make_unique<LobbyAudioVoice>();
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

	void LobbyAudio::Shutdown()
	{
		std::lock_guard lock(lobbyAudioMutex);

		isLobbyAudioStopping = true;
		ReleaseLobbyVoice(true);
		lobbyAudioSamples.clear();
	}

	void LobbyAudio::Poll()
	{
		std::lock_guard lock(lobbyAudioMutex);

		ReleaseLobbyVoice(false);
	}
}
