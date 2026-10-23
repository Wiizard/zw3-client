#pragma once

namespace Components::Remix::Menu
{
	class MenuInputState
	{
	public:
		bool overlayOpen = false;

		void UpdateRemixChord(const bool held)
		{
			if (held && !wasRemixChord) remixOpen = !remixOpen;
			wasRemixChord = held;
		}

		void ApplyMouse(int& mouseActive)
		{
			if (overlayOpen || remixOpen)
			{
				if (!ownsMouseSuspension) savedMouseActive = mouseActive;
				ownsMouseSuspension = true;
				mouseActive = 0;
			}
			else
			{
				ReleaseMouse(mouseActive);
			}
		}

		void Reset(int& mouseActive, const bool remixChordHeld)
		{
			overlayOpen = false;
			remixOpen = false;
			// A chord held across a map/device transition must be released before reopening.
			wasRemixChord = remixChordHeld;
			ReleaseMouse(mouseActive);
		}

	private:
		bool remixOpen = false;
		bool wasRemixChord = false;
		bool ownsMouseSuspension = false;
		int savedMouseActive = 0;

		void ReleaseMouse(int& mouseActive)
		{
			if (ownsMouseSuspension)
			{
				mouseActive = savedMouseActive;
				ownsMouseSuspension = false;
			}
		}
	};
}
