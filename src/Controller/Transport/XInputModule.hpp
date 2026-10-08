#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"

namespace Controller::Transport
{
	class XInputModule
	{
	public:
		explicit XInputModule(const Context& context);

		XInputModule(const XInputModule&) = delete;
		XInputModule& operator=(const XInputModule&) = delete;
		XInputModule(XInputModule&&) = delete;
		XInputModule& operator=(XInputModule&&) = delete;

		bool IsLoaded() const noexcept
		{
			return this->library != nullptr;
		}

		bool HasGuideButton() const noexcept
		{
			return this->getStateEx != nullptr;
		}

		DWORD GetState(DWORD userIndex, XINPUT_STATE& state) const noexcept;
		DWORD GetCapabilities(DWORD userIndex, DWORD flags, XINPUT_CAPABILITIES& capabilities) const noexcept;
		DWORD SetState(DWORD userIndex, XINPUT_VIBRATION& vibration) const noexcept;

	private:
		using GetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
		using GetCapabilitiesFunction = DWORD(WINAPI*)(DWORD, DWORD, XINPUT_CAPABILITIES*);
		using SetStateFunction = DWORD(WINAPI*)(DWORD, XINPUT_VIBRATION*);

		void Load(const Context& context);

		HMODULE library = nullptr;

		GetStateFunction getState = nullptr;
		GetStateFunction getStateEx = nullptr;
		GetCapabilitiesFunction getCapabilities = nullptr;
		SetStateFunction setState = nullptr;
	};
}
