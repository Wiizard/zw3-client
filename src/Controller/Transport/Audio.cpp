#include "STDInclude.hpp"

#include <cfgmgr32.h>
#include <devpropdef.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <ksmedia.h>
#include <avrt.h>

#include "Controller/Transport/Audio.hpp"

#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "Avrt.lib")

namespace Controller::Transport
{
	static constexpr std::size_t hapticChannelLeft = 2;
	static constexpr std::size_t hapticChannelRight = 3;
	static constexpr std::size_t requiredChannels = 4;

	static constexpr REFERENCE_TIME sharedBufferDuration = 100000;

	static constexpr DWORD waitTimeoutMs = 50;

	static const DEVPROPKEY containerIdKey =
	{
		{ 0x8c7ed206, 0x3f8a, 0x4827, { 0xb3, 0xab, 0xae, 0x9e, 0x1f, 0xae, 0xfc, 0x6c } },
		2
	};

	static const PROPERTYKEY friendlyNameKey =
	{
		{ 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0 } },
		14
	};

	static const PROPERTYKEY deviceFormatKey =
	{
		{ 0xf19f064d, 0x082c, 0x4e27, { 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c } },
		0
	};

	static_assert(sizeof(DEVPROPKEY) == sizeof(PROPERTYKEY), "DEVPROPKEY and PROPERTYKEY are the same key in two spellings");

	static const PROPERTYKEY& AsPropertyKey(const DEVPROPKEY& key) noexcept
	{
		return reinterpret_cast<const PROPERTYKEY&>(key);
	}

	template <typename T>
	class ComPointer
	{
	public:
		ComPointer() = default;

		~ComPointer()
		{
			this->Reset();
		}

		ComPointer(const ComPointer&) = delete;
		ComPointer& operator=(const ComPointer&) = delete;

		T** Put() noexcept
		{
			this->Reset();
			return &this->pointer;
		}

		T* Get() const noexcept
		{
			return this->pointer;
		}

		T* operator->() const noexcept
		{
			return this->pointer;
		}

		explicit operator bool() const noexcept
		{
			return this->pointer != nullptr;
		}

		void Reset() noexcept
		{
			if (this->pointer != nullptr)
			{
				this->pointer->Release();
				this->pointer = nullptr;
			}
		}

	private:
		T* pointer = nullptr;
	};

	class PropertyValue
	{
	public:
		PropertyValue() noexcept
		{
			PropVariantInit(&this->variant);
		}

		~PropertyValue()
		{
			PropVariantClear(&this->variant);
		}

		PropertyValue(const PropertyValue&) = delete;
		PropertyValue& operator=(const PropertyValue&) = delete;

		PROPVARIANT* Put() noexcept
		{
			return &this->variant;
		}

		const PROPVARIANT& Get() const noexcept
		{
			return this->variant;
		}

	private:
		PROPVARIANT variant{};
	};

	struct RenderStream
	{
		RenderStream() = default;

		~RenderStream()
		{
			if (this->ready != nullptr)
			{
				CloseHandle(this->ready);
			}
		}

		RenderStream(const RenderStream&) = delete;
		RenderStream& operator=(const RenderStream&) = delete;

		ComPointer<IAudioClient> client;
		ComPointer<IAudioRenderClient> render;

		HANDLE ready = nullptr;
		UINT32 bufferFrames = 0;
		UINT32 cushionFrames = 0;
		REFERENCE_TIME period = 0;
		std::uint32_t rate = 0;
		std::size_t channels = 0;
		bool isExclusive = false;
		bool isFloat = false;

		std::string described;
	};

	struct EndpointSearch
	{
		ComPointer<IMMDevice> endpoint;
		std::string name;
		std::vector<std::string> seen;
	};

	static std::string Narrow(const wchar_t* wide)
	{
		if (wide == nullptr)
		{
			return {};
		}

		const int length = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);

		if (length <= 1)
		{
			return {};
		}

		std::string narrow(static_cast<std::size_t>(length - 1), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide, -1, narrow.data(), length, nullptr, nullptr);
		return narrow;
	}

	static std::optional<GUID> TryGetContainer(const std::wstring& interfacePath)
	{
		GUID container{};
		ULONG size = sizeof(container);
		DEVPROPTYPE type = 0;

		const CONFIGRET result = CM_Get_Device_Interface_PropertyW(interfacePath.c_str(), &containerIdKey, &type, reinterpret_cast<PBYTE>(&container), &size, 0);

		if (result != CR_SUCCESS || type != DEVPROP_TYPE_GUID)
		{
			return std::nullopt;
		}

		return container;
	}

	static std::optional<GUID> TryGetContainer(IMMDevice& device)
	{
		ComPointer<IPropertyStore> properties;

		if (FAILED(device.OpenPropertyStore(STGM_READ, properties.Put())))
		{
			return std::nullopt;
		}

		PropertyValue value;

		if (FAILED(properties->GetValue(AsPropertyKey(containerIdKey), value.Put())))
		{
			return std::nullopt;
		}

		if (value.Get().vt != VT_CLSID || value.Get().puuid == nullptr)
		{
			return std::nullopt;
		}

		return *value.Get().puuid;
	}

	static std::string FriendlyName(IMMDevice& device)
	{
		ComPointer<IPropertyStore> properties;

		if (FAILED(device.OpenPropertyStore(STGM_READ, properties.Put())))
		{
			return {};
		}

		PropertyValue value;

		if (FAILED(properties->GetValue(friendlyNameKey, value.Put())) || value.Get().vt != VT_LPWSTR)
		{
			return {};
		}

		return Narrow(value.Get().pwszVal);
	}

	static bool TryFindEndpoint(const std::wstring& hidPath, EndpointSearch& search)
	{
		const auto wanted = TryGetContainer(hidPath);

		if (!wanted)
		{
			return false;
		}

		ComPointer<IMMDeviceEnumerator> devices;

		if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(devices.Put()))))
		{
			return false;
		}

		ComPointer<IMMDeviceCollection> endpoints;

		if (FAILED(devices->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, endpoints.Put())))
		{
			return false;
		}

		UINT count = 0;

		if (FAILED(endpoints->GetCount(&count)))
		{
			return false;
		}

		for (UINT i = 0; i != count; ++i)
		{
			ComPointer<IMMDevice> candidate;

			if (FAILED(endpoints->Item(i, candidate.Put())))
			{
				continue;
			}

			const auto theirs = TryGetContainer(*candidate.Get());

			if (theirs && IsEqualGUID(*theirs, *wanted))
			{
				search.name = FriendlyName(*candidate.Get());
				return SUCCEEDED(endpoints->Item(i, search.endpoint.Put()));
			}

			search.seen.push_back(FriendlyName(*candidate.Get()));
		}

		return false;
	}

	static bool TryGetDeclaredFormat(IMMDevice& device, WAVEFORMATEXTENSIBLE& format)
	{
		ComPointer<IPropertyStore> properties;

		if (FAILED(device.OpenPropertyStore(STGM_READ, properties.Put())))
		{
			return false;
		}

		PropertyValue value;

		if (FAILED(properties->GetValue(deviceFormatKey, value.Put())))
		{
			return false;
		}

		const auto& blob = value.Get().blob;

		if (value.Get().vt != VT_BLOB || blob.pBlobData == nullptr || blob.cbSize < sizeof(WAVEFORMATEX))
		{
			return false;
		}

		const auto* const declared = reinterpret_cast<const WAVEFORMATEX*>(blob.pBlobData);

		std::size_t wantedSize = sizeof(WAVEFORMATEX);

		if (declared->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		{
			wantedSize = sizeof(WAVEFORMATEXTENSIBLE);
		}

		if (blob.cbSize < wantedSize)
		{
			return false;
		}

		std::memset(&format, 0, sizeof(format));
		std::memcpy(&format, blob.pBlobData, wantedSize);
		return true;
	}

	static bool IsWritable(const WAVEFORMATEX& format, bool& isFloat) noexcept
	{
		if (format.nChannels < requiredChannels)
		{
			return false;
		}

		if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT && format.wBitsPerSample == 32)
		{
			isFloat = true;
			return true;
		}

		if (format.wFormatTag == WAVE_FORMAT_PCM && format.wBitsPerSample == 16)
		{
			isFloat = false;
			return true;
		}

		const bool isExtensible = format.wFormatTag == WAVE_FORMAT_EXTENSIBLE && format.cbSize >= sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);

		if (!isExtensible)
		{
			return false;
		}

		const auto& extensible = reinterpret_cast<const WAVEFORMATEXTENSIBLE&>(format);

		if (IsEqualGUID(extensible.SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) && format.wBitsPerSample == 32)
		{
			isFloat = true;
			return true;
		}

		if (IsEqualGUID(extensible.SubFormat, KSDATAFORMAT_SUBTYPE_PCM) && format.wBitsPerSample == 16)
		{
			isFloat = false;
			return true;
		}

		return false;
	}

	static std::int16_t Quantize(float value) noexcept
	{
		return static_cast<std::int16_t>(std::lround(std::clamp(value, -1.0f, 1.0f) * 32767.0f));
	}

	static std::string Describe(const WAVEFORMATEX& format, bool isExclusive)
	{
		const char* mode = "shared";

		if (isExclusive)
		{
			mode = "exclusive";
		}

		return std::format("{} channels, {} Hz, {}-bit, {}", format.nChannels, format.nSamplesPerSec, format.wBitsPerSample, mode);
	}

	static HRESULT TryInitializeExclusive(IMMDevice& device, RenderStream& stream)
	{
		REFERENCE_TIME defaultPeriod = 0;
		REFERENCE_TIME minimumPeriod = 0;
		stream.client->GetDevicePeriod(&defaultPeriod, &minimumPeriod);

		WAVEFORMATEXTENSIBLE declared{};

		if (!TryGetDeclaredFormat(device, declared) || !IsWritable(declared.Format, stream.isFloat))
		{
			return AUDCLNT_E_UNSUPPORTED_FORMAT;
		}

		REFERENCE_TIME periods[2] = { defaultPeriod, 0 };
		std::size_t periodCount = 1;

		if (minimumPeriod > 0 && minimumPeriod < defaultPeriod)
		{
			periods[0] = minimumPeriod;
			periods[1] = defaultPeriod;
			periodCount = 2;
		}

		HRESULT result = AUDCLNT_E_UNSUPPORTED_FORMAT;
		REFERENCE_TIME used = 0;

		for (std::size_t i = 0; i != periodCount; ++i)
		{
			used = periods[i];

			result = stream.client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, periods[i], periods[i], &declared.Format, nullptr);

			if (result == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
			{
				UINT32 frames = 0;

				if (SUCCEEDED(stream.client->GetBufferSize(&frames)) && frames != 0)
				{
					const auto aligned = static_cast<REFERENCE_TIME>(10000.0 * 1000 * frames / declared.Format.nSamplesPerSec + 0.5);

					used = aligned;

					result = device.Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(stream.client.Put()));

					if (SUCCEEDED(result))
					{
						result = stream.client->Initialize(AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, aligned, aligned, &declared.Format, nullptr);
					}
				}
			}

			if (SUCCEEDED(result))
			{
				break;
			}

			if (FAILED(device.Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(stream.client.Put()))))
			{
				return AUDCLNT_E_UNSUPPORTED_FORMAT;
			}
		}

		if (FAILED(result))
		{
			return result;
		}

		stream.isExclusive = true;
		stream.period = used;
		stream.rate = declared.Format.nSamplesPerSec;
		stream.channels = declared.Format.nChannels;
		stream.described = Describe(declared.Format, true);
		return result;
	}

	static HRESULT TryInitialize(IMMDevice& device, RenderStream& stream)
	{
		if (SUCCEEDED(TryInitializeExclusive(device, stream)))
		{
			return S_OK;
		}

		if (!stream.client)
		{
			return AUDCLNT_E_UNSUPPORTED_FORMAT;
		}

		REFERENCE_TIME defaultPeriod = 0;
		REFERENCE_TIME minimumPeriod = 0;
		stream.client->GetDevicePeriod(&defaultPeriod, &minimumPeriod);

		WAVEFORMATEX* mix = nullptr;

		if (FAILED(stream.client->GetMixFormat(&mix)) || mix == nullptr)
		{
			return AUDCLNT_E_UNSUPPORTED_FORMAT;
		}

		HRESULT result = AUDCLNT_E_UNSUPPORTED_FORMAT;

		if (IsWritable(*mix, stream.isFloat))
		{
			result = stream.client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_EVENTCALLBACK, sharedBufferDuration, 0, mix, nullptr);

			if (SUCCEEDED(result))
			{
				stream.isExclusive = false;
				stream.period = defaultPeriod;
				stream.rate = mix->nSamplesPerSec;
				stream.channels = mix->nChannels;
				stream.described = Describe(*mix, false);
			}
		}

		CoTaskMemFree(mix);
		return result;
	}

	static bool TryPlace(RenderStream& stream, UINT32 count, std::span<const Haptic::Frame> frames) noexcept
	{
		BYTE* data = nullptr;

		if (FAILED(stream.render->GetBuffer(count, &data)) || data == nullptr)
		{
			return false;
		}

		const std::size_t samples = static_cast<std::size_t>(count) * stream.channels;

		if (stream.isFloat)
		{
			auto* const out = reinterpret_cast<float*>(data);
			std::fill_n(out, samples, 0.0f);

			for (std::size_t i = 0; i != count; ++i)
			{
				out[i * stream.channels + hapticChannelLeft] = frames[i].left;
				out[i * stream.channels + hapticChannelRight] = frames[i].right;
			}
		}
		else
		{
			auto* const out = reinterpret_cast<std::int16_t*>(data);
			std::fill_n(out, samples, std::int16_t{ 0 });

			for (std::size_t i = 0; i != count; ++i)
			{
				out[i * stream.channels + hapticChannelLeft] = Quantize(frames[i].left);
				out[i * stream.channels + hapticChannelRight] = Quantize(frames[i].right);
			}
		}

		return SUCCEEDED(stream.render->ReleaseBuffer(count, 0));
	}

	static bool TryPrepare(RenderStream& stream)
	{
		stream.ready = CreateEventW(nullptr, FALSE, FALSE, nullptr);

		if (stream.ready == nullptr)
		{
			return false;
		}

		if (FAILED(stream.client->SetEventHandle(stream.ready)))
		{
			return false;
		}

		if (FAILED(stream.client->GetBufferSize(&stream.bufferFrames)))
		{
			return false;
		}

		if (FAILED(stream.client->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(stream.render.Put()))))
		{
			return false;
		}

		stream.cushionFrames = stream.bufferFrames;

		if (!stream.isExclusive && stream.period > 0 && stream.rate != 0)
		{
			std::uint64_t wantedFrames = static_cast<std::uint64_t>(stream.period) * static_cast<std::uint64_t>(stream.rate) * 2 / 10000000ull;

			if (wantedFrames == 0)
			{
				wantedFrames = 1;
			}

			if (wantedFrames < static_cast<std::uint64_t>(stream.cushionFrames))
			{
				stream.cushionFrames = static_cast<UINT32>(wantedFrames);
			}
		}

		if (stream.rate != 0)
		{
			stream.described += std::format(", {} ms queued", static_cast<std::uint64_t>(stream.cushionFrames) * 1000ull / stream.rate);
		}

		return true;
	}

	static void RenderLoop(RenderStream& stream, const AudioEndpoint::Source& fill, std::atomic<bool>& isRunning, const std::stop_token& stop)
	{
		std::vector<Haptic::Frame> frames(stream.bufferFrames);

		const auto tryRenderBlock = [&](UINT32 count)
		{
			const std::span<Haptic::Frame> block(frames.data(), count);

			std::fill(block.begin(), block.end(), Haptic::Frame{});
			fill(block, stream.rate);

			return TryPlace(stream, count, block);
		};

		if (!tryRenderBlock(stream.cushionFrames) || FAILED(stream.client->Start()))
		{
			return;
		}

		isRunning.store(true, std::memory_order_release);

		while (!stop.stop_requested())
		{
			if (WaitForSingleObject(stream.ready, waitTimeoutMs) != WAIT_OBJECT_0)
			{
				break;
			}

			UINT32 count = stream.bufferFrames;

			if (!stream.isExclusive)
			{
				UINT32 queued = 0;

				if (FAILED(stream.client->GetCurrentPadding(&queued)))
				{
					break;
				}

				if (queued >= stream.cushionFrames)
				{
					continue;
				}

				count = stream.cushionFrames - queued;
			}

			if (count != 0 && !tryRenderBlock(count))
			{
				break;
			}
		}

		isRunning.store(false, std::memory_order_release);
		stream.client->Stop();
	}

	AudioEndpoint::AudioEndpoint(const Context& context, DeviceId device, const std::wstring& hidPath, Source fill)
		: fill(std::move(fill)),
		thread([this, context, device, path = hidPath](std::stop_token stop)
		{
			this->Run(stop, context, device, path);
		})
	{
	}

	std::string AudioEndpoint::Status() const
	{
		std::lock_guard lock(this->statusMutex);
		return this->status;
	}

	void AudioEndpoint::Note(std::string text) const
	{
		std::lock_guard lock(this->statusMutex);
		this->status = std::move(text);
	}

	void AudioEndpoint::Run(const std::stop_token& stop, const Context& context, DeviceId device, const std::wstring& hidPath)
	{
		if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
		{
			this->Note("COM could not be initialized for the audio thread");
			return;
		}

		{
			EndpointSearch search;

			if (!TryFindEndpoint(hidPath, search))
			{
				this->Note("no audio endpoint belongs to this controller");

				std::string found = "no active render endpoints were found at all";

				if (!search.seen.empty())
				{
					found = std::format("{} active render endpoint(s) were found, none of them this controller's:", search.seen.size());
				}

				for (const auto& seenName : search.seen)
				{
					found += std::format(" '{}'", seenName);
				}

				context.Report(Severity::Info, Facility::Transport, ErrorCode::None, device,
					"the controller exposes no audio endpoint, so haptics fall back to the HID rumble path; " + found);

				CoUninitialize();
				return;
			}

			RenderStream stream;

			if (FAILED(search.endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(stream.client.Put()))))
			{
				this->Note(std::format("audio endpoint '{}' could not be activated", search.name));
				CoUninitialize();
				return;
			}

			if (FAILED(TryInitialize(*search.endpoint.Get(), stream)) || !TryPrepare(stream))
			{
				this->Note(std::format("audio endpoint '{}' could not be opened for four-channel rendering", search.name));

				context.Report(Severity::Warning, Facility::Transport, ErrorCode::TransportFailure,
					"the controller's audio endpoint could not be opened for the four channels its actuators sit on, either because it offers no such format or because another application holds it; haptics fall back to the HID rumble path");

				CoUninitialize();
				return;
			}

			this->Note(std::format("haptics on '{}' ({})", search.name, stream.described));

			context.Report(Severity::Info, Facility::Transport, ErrorCode::None, std::format("controller haptics streaming to '{}' ({})", search.name, stream.described));

			DWORD taskIndex = 0;
			const HANDLE task = AvSetMmThreadCharacteristicsW(L"Pro Audio", &taskIndex);

			RenderLoop(stream, this->fill, this->isRunning, stop);

			if (task != nullptr)
			{
				AvRevertMmThreadCharacteristics(task);
			}

			if (!stop.stop_requested())
			{
				this->Note(std::format("haptics stream on '{}' ended", search.name));
			}
		}

		CoUninitialize();
	}
}
