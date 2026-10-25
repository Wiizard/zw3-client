#include "Components/Modules/Remix/MenuInputState.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
	void Require(const bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	void OpenRemix(Components::Remix::Menu::MenuInputState& state, int& mouse)
	{
		state.UpdateRemixChord(true);
		state.ApplyMouse(mouse);
		state.UpdateRemixChord(false);
		state.ApplyMouse(mouse);
		Require(mouse == 0, "The Remix menu must suspend camera input after releasing Alt+X");
	}
}

int main()
{
	try
	{
		using Components::Remix::Menu::MenuInputState;
		for (const bool chordHeldAtTransition : { false, true })
		{
			MenuInputState state;
			int mouse = 1;
			OpenRemix(state, mouse);
			state.overlayOpen = true;
			state.Reset(mouse, chordHeldAtTransition);
			Require(mouse == 1 && !state.overlayOpen, "A map/device transition left menu input suspended");
			for (int frame = 0; frame < 120; ++frame)
			{
				state.UpdateRemixChord(chordHeldAtTransition);
				state.ApplyMouse(mouse);
				Require(mouse == 1, "A later frame restored the stale menu suspension");
			}
			state.UpdateRemixChord(false);
			OpenRemix(state, mouse);
			state.UpdateRemixChord(true);
			state.ApplyMouse(mouse);
			Require(mouse == 1, "Alt+X could not close the menu after a transition");
		}

		MenuInputState state;
		int mouse = 1;
		OpenRemix(state, mouse);
		state.overlayOpen = true;
		state.UpdateRemixChord(true);
		state.ApplyMouse(mouse);
		Require(mouse == 0, "Closing Remix must keep camera input suspended while the editor is open");
		state.overlayOpen = false;
		state.ApplyMouse(mouse);
		Require(mouse == 1, "Closing both menus did not restore camera input");
		mouse = 0;
		state.Reset(mouse, false);
		Require(mouse == 0, "A repeated reset enabled input suspended by the engine");
		OpenRemix(state, mouse);
		state.Reset(mouse, false);
		Require(mouse == 0, "A menu reset failed to preserve the engine's disabled input state");
		mouse = 1;
		state.ApplyMouse(mouse);
		Require(mouse == 1, "An old suspension survived the reset");

		state.UpdateRemixChord(false, true);
		state.ApplyMouse(mouse);
		Require(mouse == 0, "The Remix Plus first-use menu did not suspend camera input");
		state.UpdateRemixChord(true, true);
		state.ApplyMouse(mouse);
		Require(mouse == 0, "Alt+X overrode the open menu state reported by Remix Plus");
		state.UpdateRemixChord(false, false);
		state.ApplyMouse(mouse);
		Require(mouse == 1, "Closing Remix Plus without Alt+X left the camera suspended");
		state.UpdateRemixChord(true, false);
		state.ApplyMouse(mouse);
		Require(mouse == 1, "A closed Remix Plus menu was treated as open after Alt+X");
		state.UpdateRemixChord(false);
		OpenRemix(state, mouse);
		state.Reset(mouse, false);
		Require(mouse == 1, "The stock fallback lost transition cleanup after using the Plus API");

		std::cout << "Remix menu input: map/device transitions, held Alt+X, overlapping menus, Plus UI reports, stock fallback and input ownership passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
