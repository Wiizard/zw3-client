#include "STDInclude.hpp"

#include "Controller/Engine/Dvar.hpp"

#include "Components/Modules/Dvar.hpp"

namespace Controller::Engine
{
	using Components::Dvar;

	Dvars& RegisteredDvars() noexcept
	{
		static Dvars dvars;
		return dvars;
	}

	void RegisterDvars()
	{
		auto& dvars = RegisteredDvars();

		dvars.enabled = Dvar::Register("gpad_enabled", true, Game::DVAR_ARCHIVE, "Game pad enabled").Get();
		dvars.present = Dvar::Register("gpad_present", false, Game::DVAR_ROM, "A game pad is present").Get();
		dvars.inUse = Dvar::Register("gpad_in_use", false, Game::DVAR_ROM, "A game pad is in use").Get();
		dvars.rumble = Dvar::Register("gpad_rumble", true, Game::DVAR_ARCHIVE, "Enable game pad rumble").Get();
		dvars.style = Dvar::Register("gpad_style", 0, 0, 2, Game::DVAR_ARCHIVE,
			"Which button glyphs to present: 0 follows the controller that is connected, 1 is PlayStation and 2 is Xbox").Get();

		dvars.haptics = Dvar::Register("gpad_haptics", true, Game::DVAR_ARCHIVE,
			"Play rumble on the PlayStation controller's actuators, through the audio endpoint it presents, rather than through its motor emulation").Get();
		dvars.hapticIntensity = Dvar::Register("gpad_haptic_intensity", 1.0f, 0.0f, 1.0f, Game::DVAR_ARCHIVE,
			"Scale applied to every haptic effect played on the controller's actuators").Get();
		dvars.rumbleScaleLow = Dvar::Register("gpad_rumble_scale_low", 1.0f, 0.0f, 1.0f, Game::DVAR_ARCHIVE,
			"Scale applied to the low-frequency (heavy) rumble motor").Get();
		dvars.rumbleScaleHigh = Dvar::Register("gpad_rumble_scale_high", 1.0f, 0.0f, 1.0f, Game::DVAR_ARCHIVE,
			"Scale applied to the high-frequency (light) rumble motor").Get();

		dvars.adaptiveTriggers = Dvar::Register("gpad_adaptive_triggers", false, Game::DVAR_ARCHIVE,
			"Resist the PlayStation controller's triggers according to the weapon held").Get();
		dvars.adaptiveTriggerStrength = Dvar::Register("gpad_adaptive_trigger_strength", 1.0f, 0.0f, 1.0f, Game::DVAR_ARCHIVE,
			"Scale applied to every adaptive trigger resistance. Zero leaves the triggers free without turning the effects off").Get();
		dvars.adaptiveTriggerLight = Dvar::Register("gpad_adaptive_trigger_light", 7, 0, 8, Game::DVAR_ARCHIVE,
			"Resistance of the light effects: the submachine gun ramp's start, the pistol's break and the grenade's break").Get();
		dvars.adaptiveTriggerHeavy = Dvar::Register("gpad_adaptive_trigger_heavy", 8, 0, 8, Game::DVAR_ARCHIVE,
			"Resistance of the heavy effects: the rifle and machine gun's constant load, the submachine gun ramp's end, and the shotgun, sniper and launcher's break").Get();
		dvars.adaptiveTriggerLightStart = Dvar::Register("gpad_adaptive_trigger_light_start", 2, 0, 9, Game::DVAR_ARCHIVE,
			"Zone at which a light break begins, out of ten along the trigger's travel").Get();
		dvars.adaptiveTriggerLightEnd = Dvar::Register("gpad_adaptive_trigger_light_end", 5, 0, 9, Game::DVAR_ARCHIVE,
			"Zone at which a light break ends, out of ten along the trigger's travel").Get();
		dvars.adaptiveTriggerHeavyStart = Dvar::Register("gpad_adaptive_trigger_heavy_start", 3, 0, 9, Game::DVAR_ARCHIVE,
			"Zone at which a heavy break begins, out of ten along the trigger's travel").Get();
		dvars.adaptiveTriggerHeavyEnd = Dvar::Register("gpad_adaptive_trigger_heavy_end", 7, 0, 9, Game::DVAR_ARCHIVE,
			"Zone at which a heavy break ends, out of ten along the trigger's travel").Get();
		dvars.adaptiveTriggerAds = Dvar::Register("gpad_adaptive_trigger_ads", 0, 0, 8, Game::DVAR_ARCHIVE,
			"Constant resistance held on the trigger bound to aiming down the sights. Zero leaves that trigger free").Get();

		dvars.outputInterval = Dvar::Register("gpad_output_interval", 4, 0, 50, Game::DVAR_ARCHIVE,
			"Shortest gap between rumble output reports, in milliseconds. The game changes the rumble level every frame, which is faster than the controller's output endpoint can carry, so the reports queue up in the driver and the rumble falls further behind the longer it runs. Zero sends every change").Get();

		dvars.lightBar = Dvar::Register("gpad_light_bar", true, Game::DVAR_ARCHIVE, "Light the PlayStation controller's bar in the menu accent colour").Get();
		dvars.lightBarBrightness = Dvar::Register("gpad_light_bar_brightness", 1.0f, 0.0f, 1.0f, Game::DVAR_ARCHIVE,
			"Scale applied to the light bar colour, dimming the bar without changing its hue").Get();
		dvars.lightBarRed = Dvar::Register("gpad_light_bar_r", 196, 0, 255, Game::DVAR_ARCHIVE, "Light bar red").Get();
		dvars.lightBarGreen = Dvar::Register("gpad_light_bar_g", 151, 0, 255, Game::DVAR_ARCHIVE, "Light bar green").Get();
		dvars.lightBarBlue = Dvar::Register("gpad_light_bar_b", 54, 0, 255, Game::DVAR_ARCHIVE, "Light bar blue").Get();

		dvars.stickDeadzoneMin = Dvar::Register("gpad_stick_deadzone_min", 0.2f, 0.0f, 1.0f, Game::DVAR_ARCHIVE, "Game pad inner stick deadzone").Get();
		dvars.stickDeadzoneMax = Dvar::Register("gpad_stick_deadzone_max", 0.01f, 0.0f, 1.0f, Game::DVAR_ARCHIVE, "Game pad outer stick deadzone").Get();
		dvars.stickAntiDeadzone = Dvar::Register("gpad_stick_anti_deadzone", 0.0f, 0.0f, 0.9f, Game::DVAR_ARCHIVE,
			"Deflection the stick jumps to the moment it leaves the inner deadzone, to cancel a weapon or engine dead band").Get();
		dvars.buttonDeadzone = Dvar::Register("gpad_button_deadzone", 0.13f, 0.0f, 1.0f, Game::DVAR_ARCHIVE, "Game pad trigger button deadzone").Get();
		dvars.buttonDeadzoneHysteresis = Dvar::Register("gpad_button_deadzone_hysteresis", 0.05f, 0.0f, 0.5f, Game::DVAR_ARCHIVE,
			"How far below the press point a trigger must fall again before it counts as released, so a trigger resting on the threshold does not chatter").Get();
		dvars.stickPressed = Dvar::Register("gpad_stick_pressed", 0.4f, 0.0f, 1.0f, Game::DVAR_ARCHIVE, "Deflection at which a stick counts as pressed").Get();
		dvars.stickPressedHysteresis = Dvar::Register("gpad_stick_pressed_hysteresis", 0.1f, 0.0f, 1.0f, Game::DVAR_ARCHIVE,
			"No-change band around the stick pressed threshold").Get();

		dvars.buttonsConfig = Dvar::Register("gpad_buttonConfig", "buttons_default", Game::DVAR_ARCHIVE, "Game pad button configuration").Get();
		dvars.sticksConfig = Dvar::Register("gpad_sticksConfig", "thumbstick_default", Game::DVAR_ARCHIVE, "Game pad stick configuration").Get();

		dvars.menuScrollDelayFirst = Dvar::Register("gpad_menu_scroll_delay_first", 420, 0, 1000, Game::DVAR_ARCHIVE,
			"Menu scroll key-repeat delay, for the first repeat, in milliseconds").Get();
		dvars.menuScrollDelayRest = Dvar::Register("gpad_menu_scroll_delay_rest", 210, 0, 1000, Game::DVAR_ARCHIVE,
			"Menu scroll key-repeat delay, for later repeats, in milliseconds").Get();
		dvars.menuScrollDelayMin = Dvar::Register("gpad_menu_scroll_delay_min", 50, 0, 1000, Game::DVAR_ARCHIVE,
			"Menu scroll key-repeat delay at maximum acceleration, in milliseconds").Get();
		dvars.menuScrollAccelTime = Dvar::Register("gpad_menu_scroll_accel_time", 1500, 0, 5000, Game::DVAR_ARCHIVE,
			"How long a held direction takes to reach the minimum scroll delay, in milliseconds").Get();

		dvars.useHoldTime = Dvar::Register("gpad_use_hold_time", 250, 0, 10000, Game::DVAR_ARCHIVE,
			"How long the use/reload button must be held on a game pad before it uses rather than reloads").Get();

		dvars.releaseDelayEnabled = Dvar::Register("gpad_button_release_delay_enabled", true, Game::DVAR_ARCHIVE,
			"Hold a tapped button down long enough for the server to see it, so a quick tap is not swallowed on a laggy connection").Get();
		dvars.releaseDelay = Dvar::Register("gpad_button_release_delay", 50, 0, 2000, Game::DVAR_ARCHIVE,
			"Shortest press the server is shown, in milliseconds. Scales up with ping through gpad_button_release_delay_scale").Get();
		dvars.releaseDelayScale = Dvar::Register("gpad_button_release_delay_scale", 3.5f, 0.0f, 10.0f, Game::DVAR_ARCHIVE,
			"Ping multiplier for the release delay (delay = ping * scale). Zero pins the delay at gpad_button_release_delay").Get();
		dvars.releaseDelaySprintOnly = Dvar::Register("gpad_button_release_delay_sprint_only", true, Game::DVAR_ARCHIVE,
			"Only hold the sprint button, so firing and menu navigation stay immediate").Get();
		dvars.releaseGrace = Dvar::Register("gpad_button_release_grace", 75, 0, 500, Game::DVAR_ARCHIVE,
			"How long after the physical release the button is still reported held, in milliseconds").Get();

		dvars.invertPitch = Dvar::Register("input_invertPitch", false, Game::DVAR_ARCHIVE, "Invert game pad pitch").Get();
		dvars.viewSensitivity = Dvar::Register("input_viewSensitivity", 1.0f, 0.0001f, 5.0f, Game::DVAR_ARCHIVE, "Game pad look sensitivity multiplier").Get();
		dvars.aimAssistEnabled = Game::Dvar_FindVar("sv_allowAimAssist");
		dvars.turnRatePitch = Dvar::Register("aim_turnrate_pitch", 90.0f, 0.0f, 1080.0f, Game::DVAR_ARCHIVE, "Hip vertical turn rate (deg/s)").Get();
		dvars.turnRatePitchAds = Dvar::Register("aim_turnrate_pitch_ads", 55.0f, 0.0f, 1080.0f, Game::DVAR_ARCHIVE, "ADS vertical turn rate (deg/s)").Get();
		dvars.turnRateYaw = Dvar::Register("aim_turnrate_yaw", 260.0f, 0.0f, 1080.0f, Game::DVAR_ARCHIVE, "Hip horizontal turn rate (deg/s)").Get();
		dvars.turnRateYawAds = Dvar::Register("aim_turnrate_yaw_ads", 90.0f, 0.0f, 1080.0f, Game::DVAR_ARCHIVE, "ADS horizontal turn rate (deg/s)").Get();
		dvars.accelEnabled = Dvar::Register("aim_accel_turnrate_enabled", true, Game::DVAR_ARCHIVE,
			"Ramp the stick's turn rate up while a direction is held, rather than turning at the full rate immediately").Get();
		dvars.accelRate = Dvar::Register("aim_accel_turnrate_lerp", 1200.0f, 0.0f, 4000.0f, Game::DVAR_ARCHIVE, "Turn-rate acceleration (deg/s per second)").Get();
		dvars.graphEnabled = Dvar::Register("aim_input_graph_enabled", true, Game::DVAR_ARCHIVE, "Use the aim graph to shape view input").Get();
		dvars.graphIndex = Dvar::Register("aim_input_graph_index", 3, 0, 3, Game::DVAR_ARCHIVE, "Which aim graph to use").Get();
		dvars.scaleViewAxis = Dvar::Register("aim_scale_view_axis", true, Game::DVAR_ARCHIVE, "Scale the view axes by the dominant axis").Get();

		dvars.slowdownEnabled = Dvar::Register("aim_slowdown_enabled", true, Game::DVAR_ARCHIVE, "Enable aim slowdown").Get();
		dvars.gpadSlowdownEnabled = Dvar::Register("gpad_slowdown_enabled", true, Game::DVAR_ARCHIVE, "Game pad slowdown aim assist enabled").Get();
		dvars.slowdownPitchScale = Dvar::Register("aim_slowdown_pitch_scale", 0.4f, 0.0f, 1.0f, Game::DVAR_CHEAT,
			"The vertical aim assist slowdown ratio from the hip").Get();
		dvars.slowdownPitchScaleAds = Dvar::Register("aim_slowdown_pitch_scale_ads", 0.5f, 0.0f, 1.0f, Game::DVAR_CHEAT,
			"The vertical aim assist slowdown ratio when aiming down the sight").Get();
		dvars.slowdownYawScale = Dvar::Register("aim_slowdown_yaw_scale", 0.4f, 0.0f, 1.0f, Game::DVAR_CHEAT,
			"The horizontal aim assist slowdown ratio from the hip").Get();
		dvars.slowdownYawScaleAds = Dvar::Register("aim_slowdown_yaw_scale_ads", 0.5f, 0.0f, 1.0f, Game::DVAR_CHEAT,
			"The horizontal aim assist slowdown ratio when aiming down the sight").Get();
		dvars.lockOnEnabled = Dvar::Register("aim_lockon_enabled", true, Game::DVAR_ARCHIVE, "Enable lock-on aim assist").Get();
		dvars.gpadLockOnEnabled = Dvar::Register("gpad_lockon_enabled", true, Game::DVAR_ARCHIVE, "Game pad lockon aim assist enabled").Get();
		dvars.lockOnDeflection = Dvar::Register("aim_lockon_deflection", 0.05f, 0.0f, 1.0f, Game::DVAR_CHEAT, "Stick deflection at which lock-on activates").Get();
		dvars.lockOnStrength = Dvar::Register("aim_lockon_strength", 0.6f, 0.0f, 1.0f, Game::DVAR_CHEAT, "Lock-on yaw assistance").Get();
		dvars.lockOnPitchStrength = Dvar::Register("aim_lockon_pitch_strength", 0.6f, 0.0f, 1.0f, Game::DVAR_CHEAT, "Lock-on pitch assistance").Get();
		dvars.aimAssistRangeScale = Dvar::Register("aim_aimAssistRangeScale", 1.0f, 0.0f, 2.0f, Game::DVAR_CHEAT, "Aim-assist target range scale").Get();
	}

	void PublishPresent(const Dvars& dvars, bool isPresent)
	{
		if (dvars.present != nullptr && dvars.present->current.enabled != isPresent)
		{
			Game::Dvar_SetBool(dvars.present, isPresent);
		}
	}
}
