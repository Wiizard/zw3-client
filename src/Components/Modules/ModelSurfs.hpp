#pragma once

namespace Components
{
	class ModelSurfs : public Component
	{
	public:
		ModelSurfs();

		static bool IsInstalled();

		static bool TryLoadMissing(Game::XModelSurfs* modelSurfs, const char* name);

		static Game::XModelSurfs* CloneAndScaleSurfaces(const Game::XModelSurfs* source, const std::string& name, float scale);

		static void UpdateScaledSurfaces(Game::XModelSurfs* target, const Game::XModelSurfs* source, float scale);

		static void FreeClones();
	};
}
