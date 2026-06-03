#pragma once

namespace Components::GSC
{
	class ScriptExtension : public Component
	{
	public:
		ScriptExtension();

		static const char* GetCodePosForParam(int index);

	private:
		static std::unordered_map<const char*, const char*> ReplacedFunctions;
		static const char* ReplacedPos;

		struct ModelScaleTransition
		{
			Game::XModel* model;
			float startScale;
			float targetScale;
			std::chrono::steady_clock::time_point startTime;
			std::chrono::milliseconds duration;
		};

		struct VisualScaledModel
		{
			Game::XModelLodInfo originalLodInfo[4];
			Game::XModelSurfs* scaledSurfs[4];
			float originalRadius;
			Game::Bounds originalBounds;
			float originalScale;
			float scale;
			bool initialized;
		};

		enum class SceneScaleTarget
		{
			ModelName,
			Entity,
		};

		struct SceneScaleTransition
		{
			SceneScaleTarget target;
			std::string modelName;
			int entNum;
			float startScale;
			float targetScale;
			std::chrono::steady_clock::time_point startTime;
			std::chrono::milliseconds duration;
		};

		static std::vector<ModelScaleTransition> ModelScaleTransitions;
		static std::unordered_map<Game::XModel*, VisualScaledModel> VisualScaledModels;
		static std::vector<SceneScaleTransition> SceneScaleTransitions;
		static std::unordered_map<std::string, float> ModelSceneScales;
		static std::unordered_map<int, float> EntitySceneScales;
		static int LastSceneScaleApplyTime;
		static std::unordered_map<int, int> EntityModelCloneIndexes;
		static std::unordered_map<int, std::string> EntityModelCloneSources;
		static std::unordered_map<int, std::array<float, 3>> EntityModelCloneOrigins;
		static std::unordered_map<int, Game::XModel*> EntityVisualModelOverrides;

		static void GetReplacedPos(const char* pos);
		static void SetReplacedPos(const char* what, const char* with);
		static void VMExecuteInternalStub();

		static Game::XModel* GetResizeModel(const char* name, unsigned int paramIndex);
		static void RemoveModelScaleTransition(Game::XModel* model);
		static float GetXModelVisualScale(Game::XModel* model);
		static void SetXModelVisualScale(Game::XModel* model, float scale);
		static void ResizeXModel(Game::XModel* model, float targetScale, float time);
		static void ResizeXModelsByName(const std::string& modelName, Game::XModel* model, float targetScale, float time);
		static void UpdateModelScaleTransitions();
		static void ClearVisualScaledModels();
		static float GetModelSceneScale(const std::string& modelName);
		static float GetEntitySceneScale(int entNum);
		static void RemoveSceneScaleTransition(SceneScaleTarget target, const std::string& modelName, int entNum);
		static void ResizeModelSceneScale(const std::string& modelName, float targetScale, float time);
		static void ResizeEntitySceneScale(int entNum, float targetScale, float time);
		static void UpdateSceneScaleTransitions();
		static void ApplyDObjModelOverride(Game::DObj* obj);
		static void ApplyEntityModelOverrides();
		static void ApplySceneScales();
		static void R_AddDObjToScene_Stub();
		static void R_GenerateSortedDrawSurfs_Hk(void* viewInfo);
		static void R_AddSceneSurfaces_Hk(int viewIndex);
		static void ClearSceneScales();
		static Game::gentity_s* Scr_GetEntity(Game::scr_entref_t entref);
		static Game::XModel* GetModelByIndex(int modelIndex);
		static int AllocateEntityModelCloneIndex();
		static Game::XModel* CloneModelForIndex(Game::XModel* sourceModel, int modelIndex, const char* cloneName);
		static Game::XModel* GetOrCreateEntityModelClone(Game::gentity_s* ent);
		static Game::XModel* GetOrCreateClientModelClone(int entNum, int modelIndex, const char* sourceModelName);
		static void ClearEntityModelClones();
		static void GScr_ResizeModels();
		static void ScrCmd_ResizeModel(Game::scr_entref_t entref);
		static void AddResizeFunction();

		static void AddFunctions();
	};
}
