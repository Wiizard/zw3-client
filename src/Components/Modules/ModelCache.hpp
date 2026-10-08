#pragma once

namespace Components
{
	class ModelCache : public Component
	{
	public:
		static constexpr int BASE_GMODEL_COUNT = 512;
		static constexpr int ADDITIONAL_GMODELS = 512;

		static constexpr int G_MODELINDEX_LIMIT = BASE_GMODEL_COUNT + 2400 - 1200 + ADDITIONAL_GMODELS;

		static constexpr int SERVER_MODEL_LIMIT = BASE_GMODEL_COUNT + ADDITIONAL_GMODELS + 1;

		static Game::XModel** gameModelsReallocated;

		static Game::XModel** cachedModelsReallocated;

		static constexpr int firstCloneModelIndex = SERVER_MODEL_LIMIT;
		static bool HasCloneSlots();

		ModelCache();

	private:
		static void WidenModelIndexFields();
		static void RelocateGameModels();
		static void RelocateCachedModels();
		static void* CG_Init_Memset_Hook(void* dest, int value, std::size_t size);
	};
}
