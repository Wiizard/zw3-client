#include "Sound.hpp"
#include "LobbyScene.hpp"
#include "Dedicated.hpp"
#include "ZoneBuilder.hpp"
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

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

	void Sound::PrepareLobbyRoundStart(const std::string& wav)
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

	void Sound::PlayLobbyRoundStart()
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

	void Sound::preDestroy()
	{
		std::lock_guard lock(lobbyAudioMutex);
		lobbyAudioStopping = true;
		ReleaseLobbyVoice(true);
		lobbyAudioSamples.clear();
	}
	constexpr auto g_en(0x1AA4908); // global enable flag
	constexpr auto ppDS8(0x1AA490C); // global DirectSound8 interface pointer
	constexpr auto g_cf(0x79B174);  // config value
	constexpr auto f_new(0x454E40);  // allocator
	constexpr auto f_ini(0x6015D0);  // ds init
	constexpr auto f_log(0x402500);  // logger

	constexpr auto v_loop(0x4A23EB); // voice loop.

	const char err[]("Error: Failed to create DirectSound play buffer\n");

	// Patch a few bugs in the engine's DirectSound buffer initialization.
  //
  // First, we prevent a crash inside the init function by making sure
  // the global DirectSound8 interface is actually initialized before
  // we attempt to use it.
  //
  // Second, we fix a register overwrite bug. The original code clobbers
  // the first argument in eax with the global config value right before
  // the usercall.
  //
  // Finally, we fix a cleanup crash in the failure path. If the buffer
  // creation fails, the engine unconditionally calls Release() on an
  // uninitialized pointer. This results in an access violation when
  // trying to read the vtable.
  //
	__declspec (naked) int Sound::Init()
	{
		__asm
		{
			cmp  byte ptr ds : [g_en] , 0
			jnz  proceed
			xor eax, eax
			ret

			proceed :
			push esi
				mov  eax, f_new
				call eax
				mov  esi, eax

				test esi, esi
				jnz  ok
				pop  esi
				ret

				ok :
			push edi

				// Check if DirectSound8 device was actually initialized.
				//
				// Note that since ppDS8 is a pointer-to-pointer, we need to dereference
				// it twice. First to check the outer pointer, and then to verify the
				// actual interface pointer isn't null.
				//
				mov  eax, dword ptr ds : [ppDS8]
				test eax, eax
				jz   fail_e

				mov  eax, dword ptr ds : [eax]
				test eax, eax
				jz   fail_e

				// Push stack arguments first, then set up eax/ecx for the usercall.
				//
				mov  edx, dword ptr ds : [g_cf]
				push edx                       	 // Arg 4
				mov  dword ptr ds : [esi + 8] , edx
				lea  edi, [esi + 4]
				push edi                         // Arg 3: LPDIRECTSOUNDBUFFER *.
				mov  ecx, dword ptr ds : [esi + 38h] // Arg 2
				mov  eax, dword ptr ds : [esi + 2Ch] // Arg 1

				mov  edx, f_ini
				call edx
				add  esp, 8

				test eax, eax
				jge  success

				push offset err
				push 9
				mov  eax, f_log
				call eax
				add  esp, 8

				// Handle clean-up crash. The original code blindly dereferences the
				// buffer pointer to find the Release() vfunc. But if the init failed, the
				// pointer is likely invalid.
				//
				mov  eax, dword ptr ds : [edi]
				test eax, eax
				jz   fail_c

				// It's valid, so we release the interface. Release() is usually at
				// offset 8 in IUnknown.
				//
				mov  ecx, dword ptr ds : [eax]
				mov  edx, dword ptr ds : [ecx + 8]
				push eax
				call edx

				fail_c :
			// Otherwise, we zero out the slot and bail out.
			//
			mov  dword ptr ds : [edi] , 0
				fail_e :
				pop  edi
				xor eax, eax
				pop  esi
				ret

				success :
			pop  edi
				mov  eax, esi
				pop  esi
				ret
		}
	}

	// The engine's voice initialization loop blindly stores the result in an
	// array without checking if the allocation actually succeeded. If we returned
	// 0 above, it poisons the array with null pointers, which later causes a
	// divide-by-zero exception in the Miles MP3 decoder. *Sigh*
	//
	__declspec (naked) void Sound::Loop()
	{
		__asm
		{
			test eax, eax
			jz   fail_init

			mov  dword ptr ds : [esi] , eax
			add  esi, 4

			push v_loop
			ret

			fail_init :
			// If we hit a null pointer, the audio buffer creation failed so we must
			// abort the loop and return false to the caller.
			//
			xor al, al
				pop  esi
				add  esp, 8
				ret
		}
	}

	void Sound::UpdateFrontendVolume(int milliseconds)
	{
		// Gate the mixer, not snd_volume: never overwrite the player's saved settings.
		// This call runs before channel volumes are updated, including the first UI sound.
		static bool wasHeld = false;
		static unsigned int fadeStart = 0;
		const bool held = LobbyScene::IsStartupLoading();
		if (held)
		{
			wasHeld = true;
			fadeStart = 0;
		}
		else if (wasHeld)
		{
			wasHeld = false;
			fadeStart = timeGetTime();
		}

		const bool adjusting = held || fadeStart != 0;
		if (adjusting)
		{
			// Force the engine to recompute its unscaled gain each frame; otherwise
			// applying our envelope repeatedly would compound on the previous gain.
			auto* volume = *reinterpret_cast<Game::dvar_t**>(0x66CF230);
			if (volume) volume->modified = true;
		}
		Utils::Hook::Call<void(int)>(0x688820)(milliseconds);
		if (adjusting)
		{
			const auto elapsed = fadeStart ? timeGetTime() - fadeStart : 0u;
			const auto gain = held ? 0.0f : std::min(elapsed / 350.0f, 1.0f);
			*reinterpret_cast<float*>(0x66D3264) *= gain;
			if (!held && elapsed >= 350u) fadeStart = 0;
		}
	}

	Sound::
		Sound()
	{
		if (!Dedicated::IsEnabled() && !ZoneBuilder::IsEnabled())
		{
			Utils::Hook(0x4497FC, Sound::UpdateFrontendVolume, HOOK_CALL).install()->quick();
			Scheduler::Loop([]
			{
				std::lock_guard lock(lobbyAudioMutex);
				ReleaseLobbyVoice(false);
			}, Scheduler::Pipeline::ASYNC, 100ms);
		}
		Utils::Hook(0x0463A80, Sound::Init, HOOK_JUMP).install()->quick();
		Utils::Hook(0x04A23E6, Sound::Loop, HOOK_JUMP).install()->quick();
	}
}
