#pragma once

namespace Components::GSC
{
	class UsePress
	{
	public:
		void Update(const bool down)
		{
			if (down && !held) pending = true;
			held = down;
		}

		bool Consume(const bool active)
		{
			if (!active || !pending) return false;
			pending = false;
			return true;
		}

	private:
		bool held = false;
		bool pending = false;
	};
}
