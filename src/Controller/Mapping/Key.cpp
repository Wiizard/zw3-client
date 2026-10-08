#include "STDInclude.hpp"

#include "Controller/Mapping/Key.hpp"

namespace Controller::Mapping
{
	struct NamedKey
	{
		EngineKey key;
		const char* name;
	};

	static constexpr NamedKey keyNames[] =
	{
		{ EngineKey::ButtonA, "BUTTON_A" },
		{ EngineKey::ButtonB, "BUTTON_B" },
		{ EngineKey::ButtonX, "BUTTON_X" },
		{ EngineKey::ButtonY, "BUTTON_Y" },
		{ EngineKey::ButtonLShoulder, "BUTTON_LSHLDR" },
		{ EngineKey::ButtonRShoulder, "BUTTON_RSHLDR" },
		{ EngineKey::ButtonStart, "BUTTON_START" },
		{ EngineKey::ButtonBack, "BUTTON_BACK" },
		{ EngineKey::ButtonLStick, "BUTTON_LSTICK" },
		{ EngineKey::ButtonRStick, "BUTTON_RSTICK" },
		{ EngineKey::ButtonLTrigger, "BUTTON_LTRIG" },
		{ EngineKey::ButtonRTrigger, "BUTTON_RTRIG" },
		{ EngineKey::DpadUp, "DPAD_UP" },
		{ EngineKey::DpadDown, "DPAD_DOWN" },
		{ EngineKey::DpadLeft, "DPAD_LEFT" },
		{ EngineKey::DpadRight, "DPAD_RIGHT" },
		{ EngineKey::ApadUp, "APAD_UP" },
		{ EngineKey::ApadDown, "APAD_DOWN" },
		{ EngineKey::ApadLeft, "APAD_LEFT" },
		{ EngineKey::ApadRight, "APAD_RIGHT" },
		{ EngineKey::RStickUp, "RSTICK_UP" },
		{ EngineKey::RStickDown, "RSTICK_DOWN" },
		{ EngineKey::RStickLeft, "RSTICK_LEFT" },
		{ EngineKey::RStickRight, "RSTICK_RIGHT" },
	};

	static_assert(std::size(keyNames) == engineKeyCount, "the key name table must cover every controller key");

	static constexpr std::array<EngineKey, engineKeyCount> MakeKeyList() noexcept
	{
		std::array<EngineKey, engineKeyCount> list{};

		for (std::size_t i = 0; i != engineKeyCount; ++i)
		{
			list[i] = keyNames[i].key;
		}

		return list;
	}

	static constexpr std::array<EngineKey, engineKeyCount> keyList = MakeKeyList();

	std::span<const EngineKey> Keys() noexcept
	{
		return { keyList.data(), keyList.size() };
	}

	std::size_t KeyIndex(EngineKey key) noexcept
	{
		for (std::size_t i = 0; i != engineKeyCount; ++i)
		{
			if (keyList[i] == key)
			{
				return i;
			}
		}

		assert(false);
		return 0;
	}

	bool IsControllerKey(int keyNum) noexcept
	{
		const bool isFirstRange = keyNum >= 0x01 && keyNum <= 0x06;
		const bool isSecondRange = keyNum >= 0x0E && keyNum <= 0x19;
		const bool isThirdRange = keyNum >= 0x1C && keyNum <= 0x1F;
		const bool isRightStick = keyNum >= static_cast<int>(EngineKey::RStickUp) && keyNum <= static_cast<int>(EngineKey::RStickRight);

		return isFirstRange || isSecondRange || isThirdRange || isRightStick;
	}

	const char* KeyName(EngineKey key) noexcept
	{
		return keyNames[KeyIndex(key)].name;
	}
}
