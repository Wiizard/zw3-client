#pragma once

namespace Components
{
	class ArenaLength : public Component
	{
	public:
		ArenaLength();

		static constexpr int newArenaCount = 128;

		static Game::newMapArena_t* newArenas;
		static char** newArenaInfos;
	};
}
