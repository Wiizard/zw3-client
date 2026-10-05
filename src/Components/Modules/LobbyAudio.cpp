#include "LobbyAudio.hpp"
#include <Components/Modules/Scheduler.hpp>
#include <mmsystem.h>

namespace Components
{
	namespace
	{
		struct LobbyVoice
		{
			HWAVEOUT device = nullptr;
			WAVEHDR header{};
			std::vector<short> samples;
		};
		std::mutex lobbyAudioMutex;
		WAVEFORMATEX lobbyAudioFormat{};
		std::vector<short> lobbyAudioSamples;
		std::unique_ptr<LobbyVoice> lobbyVoice;
		bool lobbyAudioStopping = false;

		void ReleaseLobbyVoice(bool stop)
		{
			if (!lobbyVoice) return;
			if (stop) waveOutReset(lobbyVoice->device);
			if (!stop && !(lobbyVoice->header.dwFlags & WHDR_DONE)) return;
			if (waveOutUnprepareHeader(lobbyVoice->device, &lobbyVoice->header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR) return;
			if (waveOutClose(lobbyVoice->device) != MMSYSERR_NOERROR) return;
			lobbyVoice.reset();
		}
	}

	void LobbyAudio::PrepareLobbyRoundStart(const std::string& wav)
	{
		if (wav.size() < 44 || wav.size() > 4 * 1024 * 1024 ||
			std::memcmp(wav.data(), "RIFF", 4) || std::memcmp(wav.data() + 8, "WAVE", 4)) return;
		WAVEFORMATEX format{};
		std::vector<short> samples;
		for (size_t offset = 12; offset + 8 <= wav.size();)
		{
			unsigned length = 0;
			std::memcpy(&length, wav.data() + offset + 4, 4);
			if (length > wav.size() - offset - 8) return;
			if (!std::memcmp(wav.data() + offset, "fmt ", 4) && length >= 16)
				std::memcpy(&format, wav.data() + offset + 8, 16);
			else if (!std::memcmp(wav.data() + offset, "data", 4))
			{
				if (length % sizeof(short)) return;
				samples.resize(length / sizeof(short));
				std::memcpy(samples.data(), wav.data() + offset + 8, length);
			}
			offset += 8ull + length + (length & 1u);
		}
		if (format.wFormatTag != WAVE_FORMAT_PCM || format.wBitsPerSample != 16 ||
			(format.nChannels != 1 && format.nChannels != 2) ||
			format.nSamplesPerSec < 8000 || format.nSamplesPerSec > 48000 ||
			format.nBlockAlign != format.nChannels * 2 ||
			format.nAvgBytesPerSec != format.nSamplesPerSec * format.nBlockAlign ||
			samples.empty() || samples.size() % format.nChannels) return;
		std::lock_guard lock(lobbyAudioMutex);
		if (lobbyAudioStopping) return;
		lobbyAudioFormat = format;
		lobbyAudioSamples = std::move(samples);
	}

	void LobbyAudio::PlayLobbyRoundStart()
	{
		const auto* volume = Game::Dvar_FindVar("snd_volume");
		const float gain = volume ? std::clamp(volume->current.value, 0.0f, 1.0f) : 0.0f;
		if (gain <= 0.0f) return;
		Scheduler::Once([gain]
		{
			std::lock_guard lock(lobbyAudioMutex);
			ReleaseLobbyVoice(false);
			if (lobbyAudioStopping || lobbyVoice || lobbyAudioSamples.empty()) return;
			auto voice = std::make_unique<LobbyVoice>();
			voice->samples = lobbyAudioSamples;
			for (auto& sample : voice->samples) sample = static_cast<short>(sample * gain);
			if (waveOutOpen(&voice->device, WAVE_MAPPER, &lobbyAudioFormat, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR) return;
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
		lobbyAudioStopping = true;
		ReleaseLobbyVoice(true);
		lobbyAudioSamples.clear();
	}
	void LobbyAudio::Poll()
	{
		std::lock_guard lock(lobbyAudioMutex);
		ReleaseLobbyVoice(false);
	}
}
