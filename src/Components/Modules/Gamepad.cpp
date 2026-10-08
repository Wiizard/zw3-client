#include "STDInclude.hpp"

#include "Gamepad.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "Scheduler.hpp"
#include "ZoneBuilder.hpp"

#include "Controller/Runtime.hpp"
#include "Controller/Engine/Hook.hpp"

namespace Components
{
	Dvar::Var Gamepad::sv_allowAimAssist;

	float Gamepad::lowRumble = 0.0f;
	float Gamepad::highRumble = 0.0f;

	static std::unique_ptr<Controller::Runtime> runtime;

	static float submittedLowRumble = 0.0f;
	static float submittedHighRumble = 0.0f;
	static bool isRumbleSubmitted = false;

	static float ScaleRumble(float rumble, const Game::dvar_t* scale)
	{
		const float clampedScale = std::clamp(Controller::Engine::Read(scale, 1.0f), 0.0f, 1.0f);
		return std::clamp(rumble * clampedScale, 0.0f, 1.0f);
	}

	void Gamepad::OnMouseMove([[maybe_unused]] int x, [[maybe_unused]] int y, int dx, int dy)
	{
		Controller::Engine::NoteMouseMove(dx, dy);
	}

	void Gamepad::SubmitRumble()
	{
		if (runtime == nullptr)
		{
			return;
		}

		const auto& dvars = runtime->GetDvars();

		float low = 0.0f;
		float high = 0.0f;

		if (Controller::Engine::Read(dvars.rumble, true))
		{
			low = ScaleRumble(lowRumble, dvars.rumbleScaleLow);
			high = ScaleRumble(highRumble, dvars.rumbleScaleHigh);
		}

		if (isRumbleSubmitted && low == submittedLowRumble && high == submittedHighRumble)
		{
			return;
		}

		if (!runtime->TrySubmit(Controller::Driver::RumbleRequest{ low, high }))
		{
			return;
		}

		submittedLowRumble = low;
		submittedHighRumble = high;
		isRumbleSubmitted = true;
	}

	void Gamepad::PlayHapticEffect(const Controller::Haptic::Effect& effect)
	{
		if (runtime == nullptr)
		{
			return;
		}

		const auto& dvars = runtime->GetDvars();

		if (!Controller::Engine::Read(dvars.rumble, true) || !Controller::Engine::Read(dvars.haptics, true))
		{
			return;
		}

		if (!runtime->SupportsHaptics())
		{
			return;
		}

		const float intensity = std::clamp(Controller::Engine::Read(dvars.hapticIntensity, 1.0f), 0.0f, 1.0f);

		if (intensity <= 0.0f)
		{
			return;
		}

		auto scaled = effect;
		scaled.intensity *= intensity;

		runtime->TrySubmit(scaled);
	}

	void Gamepad::StopHapticEffect(std::uint32_t tag)
	{
		if (runtime != nullptr)
		{
			runtime->StopHaptic(tag);
		}
	}

	void Gamepad::GPad_SetLowRumble([[maybe_unused]] int gamePadIndex, double rumble)
	{
		lowRumble = static_cast<float>(std::clamp(rumble, 0.0, 1.0));
		SubmitRumble();
	}

	void Gamepad::GPad_SetHighRumble([[maybe_unused]] int gamePadIndex, double rumble)
	{
		highRumble = static_cast<float>(std::clamp(rumble, 0.0, 1.0));
		SubmitRumble();
	}

	void Gamepad::GPad_StopRumbles([[maybe_unused]] int gamePadIndex)
	{
		lowRumble = 0.0f;
		highRumble = 0.0f;

		SubmitRumble();
	}

	void Gamepad::GPad_UpdateFeedbacks()
	{
		SubmitRumble();
	}

	Gamepad::Gamepad()
	{
		if (ZoneBuilder::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			sv_allowAimAssist = Dvar::Register("sv_allowAimAssist", true, Game::DVAR_SYSTEMINFO, "Controls whether aim assist features on clients are enabled");
		});

		if (Dedicated::IsEnabled())
		{
			Controller::Engine::InstallProtocol();
			return;
		}

		if (!Controller::Engine::TryInstall())
		{
			return;
		}

		Scheduler::Once([]
		{
			runtime = std::make_unique<Controller::Runtime>();
			Controller::Engine::Attach(runtime.get());
			runtime->EngineReady();
		}, Scheduler::Pipeline::MAIN);
	}
}
