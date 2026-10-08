#include "STDInclude.hpp"

#include "Controller/Transport/XInputModule.hpp"

namespace Controller::Transport
{
	XInputModule::XInputModule(const Context& context)
	{
		this->Load(context);
	}

	void XInputModule::Load(const Context& context)
	{
		static constexpr const char* variants[] =
		{
			"xinput1_4.dll",
			"xinput1_3.dll",
			"xinput9_1_0.dll",
		};

		for (const auto* name : variants)
		{
			const HMODULE loaded = LoadLibraryA(name);

			if (loaded == nullptr)
			{
				continue;
			}

			const auto loadedGetState = reinterpret_cast<GetStateFunction>(GetProcAddress(loaded, "XInputGetState"));
			const auto loadedGetCapabilities = reinterpret_cast<GetCapabilitiesFunction>(GetProcAddress(loaded, "XInputGetCapabilities"));
			const auto loadedSetState = reinterpret_cast<SetStateFunction>(GetProcAddress(loaded, "XInputSetState"));

			if (loadedGetState == nullptr || loadedGetCapabilities == nullptr || loadedSetState == nullptr)
			{
				FreeLibrary(loaded);
				continue;
			}

			const auto loadedGetStateEx = reinterpret_cast<GetStateFunction>(GetProcAddress(loaded, MAKEINTRESOURCEA(100)));

			this->library = loaded;
			this->getState = loadedGetState;
			this->getStateEx = loadedGetStateEx;
			this->getCapabilities = loadedGetCapabilities;
			this->setState = loadedSetState;

			const char* guideNote = " (no guide button)";

			if (loadedGetStateEx != nullptr)
			{
				guideNote = " (guide button available)";
			}

			context.Report(Severity::Info, Facility::Transport, ErrorCode::None, std::format("XInput loaded via {}{}", name, guideNote));
			return;
		}

		context.Report(Severity::Warning, Facility::Transport, ErrorCode::TransportFailure, "no XInput runtime could be loaded; XInput controllers will not be available");
	}

	DWORD XInputModule::GetState(DWORD userIndex, XINPUT_STATE& state) const noexcept
	{
		if (this->getStateEx != nullptr)
		{
			return this->getStateEx(userIndex, &state);
		}

		if (this->getState != nullptr)
		{
			return this->getState(userIndex, &state);
		}

		return ERROR_DEVICE_NOT_CONNECTED;
	}

	DWORD XInputModule::GetCapabilities(DWORD userIndex, DWORD flags, XINPUT_CAPABILITIES& capabilities) const noexcept
	{
		if (this->getCapabilities != nullptr)
		{
			return this->getCapabilities(userIndex, flags, &capabilities);
		}

		return ERROR_DEVICE_NOT_CONNECTED;
	}

	DWORD XInputModule::SetState(DWORD userIndex, XINPUT_VIBRATION& vibration) const noexcept
	{
		if (this->setState != nullptr)
		{
			return this->setState(userIndex, &vibration);
		}

		return ERROR_DEVICE_NOT_CONNECTED;
	}
}
