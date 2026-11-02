#include "STDInclude.hpp"
#include "Controller/Engine/Key.hpp"
#include "Controller/Engine/Engine.hpp"
#include "Components/Modules/Command.hpp"

#include <iostream>

namespace
{
	using Controller::Button;
	using Controller::CanonicalSample;
	using Controller::Mapping::EngineKey;
	unsigned int now = 1000;
	int catcher = Game::KEYCATCH_UI;
	int waiting = 0;
	int stanceNotifies = 0;
	Game::PlayerKeyState keyState{};
	Game::AimAssistGlobals aimState{};
	Game::UiContext ui{};
	struct Event { int key; int down; };
	std::vector<Event> events;
	std::vector<int> bindings;

	void Require(bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}

	int Binding(const char* name)
	{
		if (std::strcmp(name, "+stance") == 0) return 13;
		if (std::strcmp(name, "+breath_sprint") == 0) return 15;
		if (std::strcmp(name, "+sprint") == 0) return 17;
		if (std::strcmp(name, "togglecrouch") == 0) return 20;
		if (std::strcmp(name, "+actionslot 4") == 0) return 21;
		if (std::strcmp(name, "+attack") == 0) return 23;
		return 0;
	}

	struct Sink final : Controller::DiagnosticSink
	{
		void Consume(const Controller::Diagnostic&) override {}
	};

	struct Fixture
	{
		Sink sink;
		Controller::Context context{sink};
		Controller::Engine::Dvars dvars;
		Controller::Engine::KeyDispatcher keys{context, dvars};
		Fixture()
		{
			now = 1000;
			catcher = Game::KEYCATCH_UI;
			waiting = stanceNotifies = 0;
			keyState = {};
			ui = {};
			events.clear();
			bindings.clear();
		}
		void ButtonEvent(Button button, bool down)
		{
			CanonicalSample sample{};
			sample.buttons.Set(button, down);
			keys.Dispatch(sample);
		}
		void Stick(float x, float y)
		{
			CanonicalSample sample{};
			sample.sticks[static_cast<std::size_t>(Controller::Stick::Left)].calibrated = {x, y};
			keys.Dispatch(sample);
		}
	};
}

namespace Game
{
	PlayerKeyState* playerKeys = &keyState;
	AimAssistGlobals* aaGlobArray = &aimState;
	UiContext* uiContext = &ui;
	int* g_waitingForKey = &waiting;
	Sys_Milliseconds_t Sys_Milliseconds = [] { return static_cast<int>(now); };
	Key_IsCatcherActive_t Key_IsCatcherActive = [](int, int mask) { return (catcher & mask) != 0; };
	UI_GetActiveMenu_t UI_GetActiveMenu = [](int) { return 0; };
	UI_KeyEvent_t UI_KeyEvent = [](int, int key, int down) { events.push_back({key, down}); };
	Scoreboard_HandleInput_t Scoreboard_HandleInput = [](int, int key) { events.push_back({key, 1}); return true; };
	Key_GetBindingForCmd_t Key_GetBindingForCmd = Binding;
	Dvar_SetBool_t Dvar_SetBool = [](const dvar_t* dvar, bool value) { const_cast<dvar_t*>(dvar)->current.enabled = value; };
}

namespace Controller::Engine
{
	int SnapPing(int) noexcept { return 0; }
	bool IsSprintButtonUpRequired(int) noexcept { return false; }
	void NotifyStancePress(int) { ++stanceNotifies; }
}

namespace Components
{
	void Command::ExecBinding(int, int binding, int) { bindings.push_back(binding); }
}

int main()
{
	try
	{
		{
			Fixture f;
			f.Stick(0.0f, 1.0f);
			Require(events.size() == 1 && events.back().key == Game::K_UPARROW, "stick press must navigate immediately");
			now += 419; f.keys.Tick();
			Require(events.size() == 1, "stick hold repeated before its initial delay");
			now += 1; f.keys.Tick();
			Require(events.size() == 2, "unchanged XInput stick packets must keep repeating at the deadline");
			now += 210; f.keys.Tick();
			Require(events.size() == 3, "unchanged stick stopped repeating");
			f.Stick(0.0f, -1.0f);
			Require(keyState.keys[static_cast<int>(EngineKey::ApadUp)].down == 0
				&& keyState.keys[static_cast<int>(EngineKey::ApadDown)].down == 1, "crossing center left the old direction held");
			Require(events[3].key == Game::K_UPARROW && events[3].down == 0
				&& events[4].key == Game::K_DOWNARROW && events[4].down == 1, "direction reversal must release before pressing");
			f.Stick(0.0f, 0.0f);
			Require(keyState.anyKeyDown == 0, "neutral stick left a navigation key down");
		}
		{
			Fixture f;
			f.Stick(1.0f, 1.0f);
			Require(events.size() == 1 && events[0].key == Game::K_UPARROW, "diagonal menu input must choose one direction");
		}
		{
			Fixture f;
			f.ButtonEvent(Button::DpadDown, true);
			now += 420; f.keys.Tick();
			Require(events.size() == 2, "D-pad repeat deadline was reset while held");
			f.ButtonEvent(Button::DpadDown, false);
			Require(events.back().down == 0, "D-pad release must reach the UI immediately");
		}
		{
			Fixture f;
			f.ButtonEvent(Button::Start, true);
			Require(events.size() == 1 && events[0].key == Game::K_ESCAPE, "pause button must back out, not activate a menu item");
			f.ButtonEvent(Button::Start, false);
			f.ButtonEvent(Button::L1, true);
			Require(events.back().key == Game::K_PGUP, "left shoulder must page upward");
			f.ButtonEvent(Button::L1, false);
			f.ButtonEvent(Button::R1, true);
			Require(events.back().key == Game::K_PGDN, "right shoulder must page downward");
		}
		{
			Fixture f;
			Game::menuDef_t title{};
			title.window.name = "main_text";
			title.window.dynamicFlags[0] = Game::WINDOW_DYNAMIC_VISIBLE;
			ui.menuStack[0] = &title; ui.openMenuCount = 1;
			f.ButtonEvent(Button::Start, true);
			Require(events.back().key == Game::K_ENTER, "start must still activate the title screen");
			f.ButtonEvent(Button::Start, false);
			Game::ItemKeyHandler shortcut{};
			shortcut.key = static_cast<int>(EngineKey::ButtonX);
			title.onKey = &shortcut;
			f.ButtonEvent(Button::FaceWest, true);
			Require(events.back().key == shortcut.key && events.back().down == 1, "explicit controller menu shortcuts must receive raw keys");
			title.onKey = nullptr;
			catcher = 0;
			f.ButtonEvent(Button::FaceWest, false);
			Require(events.back().key == shortcut.key && events.back().down == 0, "key release lost its route after the menu closed");
		}
		{
			Fixture f;
			waiting = 1;
			f.ButtonEvent(Button::FaceSouth, true);
			Require(events.back().key == static_cast<int>(EngineKey::ButtonA), "rebind capture must receive the physical controller key");
			waiting = 0;
			f.ButtonEvent(Button::FaceSouth, false);
			Require(events.back().key == static_cast<int>(EngineKey::ButtonA) && events.back().down == 0, "capture changed the release route");
		}
		{
			Fixture f;
			catcher = 0;
			keyState.keys[static_cast<int>(EngineKey::ButtonB)].binding = Binding("+stance");
			f.ButtonEvent(Button::FaceEast, true);
			Require(stanceNotifies == 1 && bindings == std::vector<int>{13}, "stance must notify sliding while still executing native +stance once");
			now += 1000; f.keys.Tick();
			Require(stanceNotifies == 1 && bindings.size() == 1, "holding stance repeated the slide request");
			f.ButtonEvent(Button::FaceEast, false);
			Require(bindings == std::vector<int>({13, 14}), "tap/hold stance must retain its matching native release");
			catcher = Game::KEYCATCH_UI;
			f.ButtonEvent(Button::FaceEast, true);
			Require(stanceNotifies == 1 && events.back().key == Game::K_ESCAPE, "menu back must not request a slide");
		}
		{
			Fixture f;
			catcher = 0;
			keyState.keys[static_cast<int>(EngineKey::ButtonRStick)].binding = Binding("+stance");
			f.ButtonEvent(Button::R3, true);
			Require(stanceNotifies == 1, "tactical and custom layouts must slide from their bound stance button");
			f.keys.NoteOtherInput();
			f.ButtonEvent(Button::R3, false);
			Require(!f.keys.IsInUse(), "a controller release stole keyboard/mouse prompt ownership");
		}
		{
			Fixture f;
			catcher = Game::KEYCATCH_CONSOLE;
			keyState.keys[static_cast<int>(EngineKey::ButtonB)].binding = Binding("+stance");
			f.ButtonEvent(Button::FaceEast, true);
			Require(stanceNotifies == 0 && bindings.empty() && events.empty(), "controller gameplay input leaked through the console");
		}
		{
			Fixture f;
			now = 0xFFFFFF00u;
			f.Stick(0.0f, 1.0f);
			now += 420u; f.keys.Tick();
			Require(events.size() == 2, "navigation repeat failed across the millisecond counter wrap");
			f.keys.ReleaseAll();
			Require(keyState.anyKeyDown == 0, "disconnect cleanup left a key held");
		}
		std::cout << "Controller input: held sticks, repeats, reversals, diagonals, menu routes, rebinds, stance/slide and prompt ownership passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
