#include "STDInclude.hpp"

#include "Controller/Engine/Command.hpp"
#include "Controller/Runtime.hpp"
#include "Controller/Haptic/Effect.hpp"

#include "Components/Modules/Command.hpp"

namespace Controller::Engine
{
	static Runtime* commandRuntime = nullptr;

	static void StatusCommand()
	{
		if (commandRuntime == nullptr)
		{
			return;
		}

		const auto& context = commandRuntime->GetContext();

		const char* source = "keyboard and mouse";

		if (commandRuntime->Keys().IsInUse())
		{
			source = "the controller";
		}

		context.Report(Severity::Info, Facility::Engine, ErrorCode::None, std::format("{} device(s) bound; input source is {}", commandRuntime->DeviceCount(), source));

		const DeviceId active = commandRuntime->Active();

		if (!active)
		{
			return;
		}

		const auto& frame = commandRuntime->Latest();

		context.Report(Severity::Info, Facility::Engine, ErrorCode::None, active,
			std::format("active device: {} over {}, sequence {}", ToString(frame.family), ToString(frame.link), frame.sequence));

		const std::string diagnostics = commandRuntime->ActiveDiagnostics();

		if (!diagnostics.empty())
		{
			context.Report(Severity::Info, Facility::Engine, ErrorCode::None, active, diagnostics);
		}
	}

	static float NumberArgument(const Components::Command::Params* params, int index, float fallback)
	{
		if (params->Size() <= index)
		{
			return fallback;
		}

		return static_cast<float>(std::atof(params->Get(index)));
	}

	static void HapticCommand(const Components::Command::Params* params)
	{
		if (commandRuntime == nullptr)
		{
			return;
		}

		const auto& context = commandRuntime->GetContext();

		const float intensity = NumberArgument(params, 1, 1.0f);
		const float sharpness = NumberArgument(params, 2, 0.5f);
		const float duration = NumberArgument(params, 3, 0.0f);

		Haptic::Effect effect = Haptic::Transient(intensity, sharpness);
		const char* kind = "transient";

		if (duration > 0.0f)
		{
			effect = Haptic::Continuous(intensity, sharpness, Seconds{ duration });
			kind = "continuous";
		}

		commandRuntime->TrySubmit(effect);

		context.Report(Severity::Info, Facility::Engine, ErrorCode::None, std::format("played a {} effect at intensity {}, sharpness {}", kind, intensity, sharpness));

		std::string diagnostics = commandRuntime->ActiveDiagnostics();

		if (diagnostics.empty())
		{
			diagnostics = "no device is active to play it on";
		}

		context.Report(Severity::Info, Facility::Engine, ErrorCode::None, diagnostics);
	}

	static void ButtonsConfigCommand()
	{
		if (commandRuntime != nullptr)
		{
			commandRuntime->Binds().ReapplyLayout();
		}
	}

	static void SticksConfigCommand()
	{
		if (commandRuntime == nullptr)
		{
			return;
		}

		const auto& context = commandRuntime->GetContext();

		context.Report(Severity::Info, Facility::Engine, ErrorCode::None, std::format("stick layout: {}", Read(commandRuntime->GetDvars().sticksConfig, "thumbstick_default")));
	}

	void RegisterCommands(const Context& context, Runtime& runtime)
	{
		commandRuntime = &runtime;

		Components::Command::Add("controller_status", StatusCommand);
		Components::Command::Add("controller_haptic", HapticCommand);

		Components::Command::Add("bindgpbuttonsconfigs", ButtonsConfigCommand);
		Components::Command::Add("bindgpsticksconfigs", SticksConfigCommand);

		context.Report(Severity::Info, Facility::Engine, ErrorCode::None, "controller commands registered");
	}
}
