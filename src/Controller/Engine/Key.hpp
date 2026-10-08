#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Engine/Dvar.hpp"
#include "Controller/Mapping/Key.hpp"
#include "Controller/Sample/Sample.hpp"

namespace Controller::Engine
{
	class KeyDispatcher
	{
	public:
		KeyDispatcher(const Context& context, const Dvars& dvars);

		void Dispatch(const CanonicalSample& sample);
		void Tick();
		void ReleaseAll();

		bool IsInUse() const noexcept
		{
			return this->isInUse;
		}

		void NoteOtherInput();

		void SetTriggerEngage(float left, float right) noexcept;

	private:
		enum class KeyEvent : std::uint8_t
		{
			Pressed,
			Repeated,
			Released,
		};

		static constexpr std::size_t buttonStateCount = static_cast<std::size_t>(Button::Count);
		static constexpr std::size_t axisCount = 4;
		static constexpr float defaultTriggerReleaseMargin = 0.05f;

		void SetInUse(bool isNowInUse);

		unsigned int ReleaseDelay() const noexcept;
		bool DefersRelease(Mapping::EngineKey key) const noexcept;

		void EmitButton(Mapping::EngineKey key, KeyEvent event, unsigned int time);
		void Emit(Mapping::EngineKey key, KeyEvent event, unsigned int time);

		void DispatchApad(unsigned int time);
		void UpdateAds() noexcept;
		void DispatchButtons(const ButtonSet& current, unsigned int time);

		bool ShouldIgnoreRepeat(Mapping::EngineKey key, int repeats, unsigned int time);
		void ResetScroll(Mapping::EngineKey key, bool isDown, unsigned int time);

		void MenuKeyEvent(Mapping::EngineKey key, bool isDown);
		bool TryScoreboardKeyEvent(Mapping::EngineKey key);

		const Context& context;
		const Dvars& dvars;

		bool isInUse = false;
		bool hasReportedDeadzone = false;

		std::array<float, triggerCount> engage{};
		std::array<bool, triggerCount> isTriggerHeld{};

		ButtonSet buttons;
		ButtonSet deferred;

		float adsLerp = 0.0f;
		bool isAdsLowering = false;

		std::array<unsigned int, buttonStateCount> pressedAt{};
		std::array<unsigned int, buttonStateCount> releasedAt{};

		std::array<std::array<bool, 2>, axisCount> isDeflected{};
		std::array<std::array<bool, 2>, axisCount> wasDeflected{};

		unsigned int nextScroll = 0;
		unsigned int scrollHoldStart = 0;
		std::optional<Mapping::EngineKey> scrollHoldKey;
	};
}
