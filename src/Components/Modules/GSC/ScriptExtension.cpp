
#include <Components/Modules/Events.hpp>
#include <Components/Modules/ModelCache.hpp>
#include <Components/Modules/ModelSurfs.hpp>
#include <Components/Modules/Scheduler.hpp>
#include <Components/Modules/ServerCommands.hpp>

#include "ScriptExtension.hpp"
#include "Script.hpp"

#include <windows.h>
#include <wininet.h>
#pragma comment(lib, "wininet.lib")

namespace Components::GSC
{
	namespace
	{
		constexpr auto ResizeServerCommand = 22;

		bool IsValidScale(const float scale)
		{
			return std::isfinite(scale) && scale > 0.0f;
		}

		float Lerp(const float from, const float to, const float fraction)
		{
			return from + ((to - from) * fraction);
		}
	}

	std::unordered_map<const char*, const char*> ScriptExtension::ReplacedFunctions;
	const char* ScriptExtension::ReplacedPos = nullptr;
	std::vector<ScriptExtension::ModelScaleTransition> ScriptExtension::ModelScaleTransitions;
	std::unordered_map<Game::XModel*, ScriptExtension::VisualScaledModel> ScriptExtension::VisualScaledModels;
	std::vector<ScriptExtension::SceneScaleTransition> ScriptExtension::SceneScaleTransitions;
	std::unordered_map<std::string, float> ScriptExtension::ModelSceneScales;
	std::unordered_map<int, float> ScriptExtension::EntitySceneScales;
	int ScriptExtension::LastSceneScaleApplyTime = std::numeric_limits<int>::min();
	std::unordered_map<int, int> ScriptExtension::EntityModelCloneIndexes;
	std::unordered_map<int, std::string> ScriptExtension::EntityModelCloneSources;
	std::unordered_map<int, std::array<float, 3>> ScriptExtension::EntityModelCloneOrigins;
	std::unordered_map<int, Game::XModel*> ScriptExtension::EntityVisualModelOverrides;

	const char* ScriptExtension::GetCodePosForParam(int index)
	{
		if (static_cast<unsigned int>(index) >= Game::scrVmPub->outparamcount)
		{
			Game::Scr_ParamError(static_cast<unsigned int>(index), "GetCodePosForParam: Index is out of range!");
			return "";
		}

		const auto* value = &Game::scrVmPub->top[-index];

		if (value->type != Game::VAR_FUNCTION)
		{
			Game::Scr_ParamError(static_cast<unsigned int>(index), "GetCodePosForParam: Expects a function as parameter!");
			return "";
		}

		return value->u.codePosValue;
	}

	void ScriptExtension::GetReplacedPos(const char* pos)
	{
		if (!pos)
		{
			// This seems to happen often and there should not be pointers to NULL in our map
			return;
		}

		if (const auto itr = ReplacedFunctions.find(pos); itr != ReplacedFunctions.end())
		{
			ReplacedPos = itr->second;
		}
	}

	void ScriptExtension::SetReplacedPos(const char* what, const char* with)
	{
		if (!*what || !*with)
		{
			Logger::Warning(Game::CON_CHANNEL_SCRIPT, "Invalid parameters passed to ReplacedFunctions\n");
			return;
		}

		if (ReplacedFunctions.contains(what))
		{
			Logger::Warning(Game::CON_CHANNEL_SCRIPT, "ReplacedFunctions already contains codePosValue for a function\n");
		}

		ReplacedFunctions[what] = with;
	}

	__declspec(naked) void ScriptExtension::VMExecuteInternalStub()
	{
		__asm
		{
			pushad

			push edx
			call GetReplacedPos

			pop edx
			popad

			cmp ReplacedPos, 0
			jne SetPos

			movzx eax, byte ptr [edx]
			inc edx

		Loc1:
			cmp eax, 0x8B

			push ecx

			mov ecx, 0x2045094
			mov [ecx], eax

			mov ecx, 0x2040CD4
			mov [ecx], edx

			pop ecx

			push 0x61E944
			ret

		SetPos:
			mov edx, ReplacedPos
			mov ReplacedPos, 0

			movzx eax, byte ptr [edx]
			inc edx

			jmp Loc1
		}
	}

	Game::XModel* ScriptExtension::GetResizeModel(const char* name, const unsigned int paramIndex)
	{
		if (!name || !*name)
		{
			Game::Scr_ParamError(paramIndex, "ResizeModel: Illegal model parameter!");
			return nullptr;
		}

		auto* model = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_XMODEL, name).model;
		if (!model || Game::DB_IsXAssetDefault(Game::ASSET_TYPE_XMODEL, name))
		{
			Game::Scr_ParamError(paramIndex, Utils::String::VA("ResizeModel: xmodel '%s' does not exist", name));
			return nullptr;
		}

		return model;
	}

	void ScriptExtension::RemoveModelScaleTransition(Game::XModel* model)
	{
		std::erase_if(ModelScaleTransitions, [model](const ModelScaleTransition& transition)
		{
			return transition.model == model;
		});
	}

	float ScriptExtension::GetXModelVisualScale(Game::XModel* model)
	{
		if (const auto visualModel = VisualScaledModels.find(model); visualModel != VisualScaledModels.end())
		{
			return visualModel->second.scale;
		}

		return model ? model->scale : 1.0f;
	}

	void ScriptExtension::SetXModelVisualScale(Game::XModel* model, const float scale)
	{
		if (!model)
		{
			return;
		}

		auto& visualModel = VisualScaledModels[model];
		if (!visualModel.initialized)
		{
			std::memcpy(visualModel.originalLodInfo, model->lodInfo, sizeof(model->lodInfo));
			std::memset(visualModel.scaledSurfs, 0, sizeof(visualModel.scaledSurfs));
			visualModel.originalRadius = model->radius;
			visualModel.originalBounds = model->bounds;
			visualModel.originalScale = model->scale;
			visualModel.scale = model->scale;
			visualModel.initialized = true;
		}

		for (auto lodIndex = 0; lodIndex < ARRAYSIZE(model->lodInfo); ++lodIndex)
		{
			const auto& originalLod = visualModel.originalLodInfo[lodIndex];
			if (!originalLod.modelSurfs || !originalLod.modelSurfs->surfs)
			{
				continue;
			}

			if (!visualModel.scaledSurfs[lodIndex])
			{
				visualModel.scaledSurfs[lodIndex] = ModelSurfs::CloneAndScaleSurfaces(
					originalLod.modelSurfs,
					Utils::String::VA("resize_%s_lod%i", model->name, lodIndex),
					scale);
			}
			else
			{
				ModelSurfs::UpdateScaledSurfaces(visualModel.scaledSurfs[lodIndex], originalLod.modelSurfs, scale);
			}

			if (visualModel.scaledSurfs[lodIndex])
			{
				model->lodInfo[lodIndex].modelSurfs = visualModel.scaledSurfs[lodIndex];
				model->lodInfo[lodIndex].surfs = visualModel.scaledSurfs[lodIndex]->surfs;
				model->lodInfo[lodIndex].numsurfs = visualModel.scaledSurfs[lodIndex]->numsurfs;
			}
		}

		model->scale = scale;
		model->radius = visualModel.originalRadius * scale;
		model->bounds = visualModel.originalBounds;
		model->bounds.midPoint[0] *= scale;
		model->bounds.midPoint[1] *= scale;
		model->bounds.midPoint[2] *= scale;
		model->bounds.halfSize[0] *= scale;
		model->bounds.halfSize[1] *= scale;
		model->bounds.halfSize[2] *= scale;
		visualModel.scale = scale;
	}

	void ScriptExtension::ResizeXModel(Game::XModel* model, const float targetScale, const float time)
	{
		if (!model)
		{
			return;
		}

		RemoveModelScaleTransition(model);

		if (time <= 0.0f)
		{
			SetXModelVisualScale(model, targetScale);
			return;
		}

		const auto duration = std::chrono::milliseconds(std::max(1, static_cast<int>(std::min(time * 1000.0f, static_cast<float>(std::numeric_limits<int>::max())))));

		ModelScaleTransitions.push_back({
			model,
			GetXModelVisualScale(model),
			targetScale,
			std::chrono::steady_clock::now(),
			duration,
		});
	}

	void ScriptExtension::ResizeXModelsByName(const std::string& modelName, Game::XModel* model, const float targetScale, const float time)
	{
		ResizeXModel(model, targetScale, time);

		for (const auto& [entNum, clone] : EntityVisualModelOverrides)
		{
			static_cast<void>(entNum);

			if (!clone || clone == model)
			{
				continue;
			}

			const auto sourceModel = EntityModelCloneSources.find(entNum);
			if (sourceModel == EntityModelCloneSources.end() || sourceModel->second != modelName)
			{
				continue;
			}

			ResizeXModel(clone, targetScale, time);
		}
	}

	void ScriptExtension::UpdateModelScaleTransitions()
	{
		const auto now = std::chrono::steady_clock::now();

		for (auto i = ModelScaleTransitions.begin(); i != ModelScaleTransitions.end();)
		{
			if (!i->model)
			{
				i = ModelScaleTransitions.erase(i);
				continue;
			}

			const auto elapsed = now - i->startTime;
			if (elapsed >= i->duration)
			{
				SetXModelVisualScale(i->model, i->targetScale);
				i = ModelScaleTransitions.erase(i);
				continue;
			}

			const auto fraction = std::clamp(static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) / static_cast<float>(i->duration.count()), 0.0f, 1.0f);
			SetXModelVisualScale(i->model, Lerp(i->startScale, i->targetScale, fraction));
			++i;
		}
	}

	void ScriptExtension::ClearVisualScaledModels()
	{
		for (auto& [model, visualModel] : VisualScaledModels)
		{
			if (!model)
			{
				continue;
			}

			std::memcpy(model->lodInfo, visualModel.originalLodInfo, sizeof(model->lodInfo));
			model->radius = visualModel.originalRadius;
			model->bounds = visualModel.originalBounds;
			model->scale = visualModel.originalScale;
		}

		VisualScaledModels.clear();
		ModelScaleTransitions.clear();
	}

	float ScriptExtension::GetModelSceneScale(const std::string& modelName)
	{
		if (const auto scale = ModelSceneScales.find(modelName); scale != ModelSceneScales.end())
		{
			return scale->second;
		}

		return 1.0f;
	}

	float ScriptExtension::GetEntitySceneScale(const int entNum)
	{
		if (const auto scale = EntitySceneScales.find(entNum); scale != EntitySceneScales.end())
		{
			return scale->second;
		}

		return 1.0f;
	}

	void ScriptExtension::RemoveSceneScaleTransition(const SceneScaleTarget target, const std::string& modelName, const int entNum)
	{
		std::erase_if(SceneScaleTransitions, [target, &modelName, entNum](const SceneScaleTransition& transition)
		{
			if (transition.target != target)
			{
				return false;
			}

			if (target == SceneScaleTarget::Entity)
			{
				return transition.entNum == entNum;
			}

			return transition.modelName == modelName;
		});
	}

	void ScriptExtension::ResizeModelSceneScale(const std::string& modelName, const float targetScale, const float time)
	{
		RemoveSceneScaleTransition(SceneScaleTarget::ModelName, modelName, 0);

		if (time <= 0.0f)
		{
			ModelSceneScales[modelName] = targetScale;
			return;
		}

		const auto duration = std::chrono::milliseconds(std::max(1, static_cast<int>(std::min(time * 1000.0f, static_cast<float>(std::numeric_limits<int>::max())))));

		SceneScaleTransitions.push_back({
			SceneScaleTarget::ModelName,
			modelName,
			0,
			GetModelSceneScale(modelName),
			targetScale,
			std::chrono::steady_clock::now(),
			duration,
		});
	}

	void ScriptExtension::ResizeEntitySceneScale(const int entNum, const float targetScale, const float time)
	{
		RemoveSceneScaleTransition(SceneScaleTarget::Entity, {}, entNum);

		if (time <= 0.0f)
		{
			EntitySceneScales[entNum] = targetScale;
			return;
		}

		const auto duration = std::chrono::milliseconds(std::max(1, static_cast<int>(std::min(time * 1000.0f, static_cast<float>(std::numeric_limits<int>::max())))));

		SceneScaleTransitions.push_back({
			SceneScaleTarget::Entity,
			{},
			entNum,
			GetEntitySceneScale(entNum),
			targetScale,
			std::chrono::steady_clock::now(),
			duration,
		});
	}

	void ScriptExtension::UpdateSceneScaleTransitions()
	{
		const auto now = std::chrono::steady_clock::now();

		for (auto i = SceneScaleTransitions.begin(); i != SceneScaleTransitions.end();)
		{
			const auto elapsed = now - i->startTime;
			const auto done = elapsed >= i->duration;
			const auto fraction = done ? 1.0f : std::clamp(static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count()) / static_cast<float>(i->duration.count()), 0.0f, 1.0f);
			const auto scale = Lerp(i->startScale, i->targetScale, fraction);

			if (i->target == SceneScaleTarget::Entity)
			{
				EntitySceneScales[i->entNum] = scale;
			}
			else
			{
				ModelSceneScales[i->modelName] = scale;
			}

			if (done)
			{
				i = SceneScaleTransitions.erase(i);
				continue;
			}

			++i;
		}
	}

	void ScriptExtension::ApplyDObjModelOverride(Game::DObj* obj)
	{
		if (!obj || !obj->models)
		{
			return;
		}

		auto overrideModel = EntityVisualModelOverrides.find(obj->entnum);
		if (overrideModel == EntityVisualModelOverrides.end() || !overrideModel->second)
		{
			return;
		}

		const auto sourceModel = EntityModelCloneSources.find(overrideModel->first);
		auto replaced = false;

		for (auto modelIndex = 0; modelIndex < obj->numModels; ++modelIndex)
		{
			auto*& model = obj->models[modelIndex];
			if (!model)
			{
				continue;
			}

			if (model == overrideModel->second)
			{
				replaced = true;
				continue;
			}

			if (sourceModel != EntityModelCloneSources.end() && model->name && sourceModel->second == model->name)
			{
				model = overrideModel->second;
				replaced = true;
			}
		}

		if (!replaced && obj->numModels == 1)
		{
			obj->models[0] = overrideModel->second;
		}

		obj->radius = overrideModel->second->radius;
	}

	void ScriptExtension::ApplyEntityModelOverrides()
	{
		if (EntityVisualModelOverrides.empty() || !Game::scene)
		{
			return;
		}

		const auto applyOverride = [](Game::GfxSceneEntity& sceneDObj, Game::XModel* overrideModel, const std::string* sourceModelName)
		{
			if (!overrideModel || !sceneDObj.obj || !sceneDObj.obj->models)
			{
				return;
			}

			auto replaced = false;

			for (auto modelIndex = 0; modelIndex < sceneDObj.obj->numModels; ++modelIndex)
			{
				auto*& model = sceneDObj.obj->models[modelIndex];
				if (!model)
				{
					continue;
				}

				if (model == overrideModel)
				{
					replaced = true;
					continue;
				}

				if (sourceModelName && model->name && *sourceModelName == model->name)
				{
					model = overrideModel;
					replaced = true;
				}
			}

			if (!replaced && sceneDObj.obj->numModels == 1)
			{
				sceneDObj.obj->models[0] = overrideModel;
			}

			sceneDObj.obj->radius = overrideModel->radius;
			sceneDObj.cull.bounds.halfSize[0] = std::max(sceneDObj.cull.bounds.halfSize[0], overrideModel->bounds.halfSize[0]);
			sceneDObj.cull.bounds.halfSize[1] = std::max(sceneDObj.cull.bounds.halfSize[1], overrideModel->bounds.halfSize[1]);
			sceneDObj.cull.bounds.halfSize[2] = std::max(sceneDObj.cull.bounds.halfSize[2], overrideModel->bounds.halfSize[2]);
		};

		for (const auto& [entNum, overrideModel] : EntityVisualModelOverrides)
		{
			if (!overrideModel)
			{
				continue;
			}

			const auto sourceModel = EntityModelCloneSources.find(entNum);
			const auto sourceModelName = sourceModel == EntityModelCloneSources.end() ? nullptr : &sourceModel->second;
			auto applied = false;

			for (auto i = 0; i < Game::scene->sceneDObjCount; ++i)
			{
				auto& sceneDObj = Game::scene->sceneDObj[i];
				if (sceneDObj.entnum == static_cast<unsigned int>(entNum))
				{
					applyOverride(sceneDObj, overrideModel, sourceModelName);
					applied = true;
				}
			}

			if (applied || !sourceModelName)
			{
				continue;
			}

			const auto origin = EntityModelCloneOrigins.find(entNum);
			if (origin == EntityModelCloneOrigins.end())
			{
				continue;
			}

			auto bestSceneDObj = -1;
			auto bestDistance = std::numeric_limits<float>::max();

			for (auto i = 0; i < Game::scene->sceneDObjCount; ++i)
			{
				auto& sceneDObj = Game::scene->sceneDObj[i];
				if (!sceneDObj.obj || !sceneDObj.obj->models)
				{
					continue;
				}

				auto hasSourceModel = false;
				for (auto modelIndex = 0; modelIndex < sceneDObj.obj->numModels; ++modelIndex)
				{
					const auto* model = sceneDObj.obj->models[modelIndex];
					if (model && model->name && *sourceModelName == model->name)
					{
						hasSourceModel = true;
						break;
					}
				}

				if (!hasSourceModel)
				{
					continue;
				}

				const auto dx = sceneDObj.placement.origin[0] - origin->second[0];
				const auto dy = sceneDObj.placement.origin[1] - origin->second[1];
				const auto dz = sceneDObj.placement.origin[2] - origin->second[2];
				const auto distance = dx * dx + dy * dy + dz * dz;

				if (distance < bestDistance)
				{
					bestDistance = distance;
					bestSceneDObj = i;
				}
			}

			if (bestSceneDObj >= 0)
			{
				applyOverride(Game::scene->sceneDObj[bestSceneDObj], overrideModel, sourceModelName);
			}
		}
	}

	void ScriptExtension::ApplySceneScales()
	{
		if (!Game::CL_IsCgameInitialized() || !Game::scene)
		{
			return;
		}

		UpdateSceneScaleTransitions();
		ApplyEntityModelOverrides();

		if (LastSceneScaleApplyTime == Game::scene->def.time)
		{
			return;
		}

		LastSceneScaleApplyTime = Game::scene->def.time;

		for (auto i = 0; i < Game::scene->sceneModelCount; ++i)
		{
			auto& sceneModel = Game::scene->sceneModel[i];

			auto scale = GetEntitySceneScale(sceneModel.entnum);
			if (scale == 1.0f && sceneModel.model && sceneModel.model->name)
			{
				scale = GetModelSceneScale(sceneModel.model->name);
			}

			if (scale != 1.0f)
			{
				sceneModel.placement.scale *= scale;
			}
		}

		for (auto i = 0; i < Game::scene->sceneDObjCount; ++i)
		{
			auto& sceneDObj = Game::scene->sceneDObj[i];
			if (!sceneDObj.obj || sceneDObj.obj->numModels <= 0 || !sceneDObj.obj->models)
			{
				continue;
			}

			auto scale = GetEntitySceneScale(sceneDObj.entnum);
			auto* model = sceneDObj.obj->models[0];

			if (scale == 1.0f)
			{
				for (auto modelIndex = 0; modelIndex < sceneDObj.obj->numModels; ++modelIndex)
				{
					auto* candidate = sceneDObj.obj->models[modelIndex];
					if (!candidate || !candidate->name)
					{
						continue;
					}

					scale = GetModelSceneScale(candidate->name);
					if (scale != 1.0f)
					{
						model = candidate;
						break;
					}
				}
			}

			if (scale == 1.0f || !model)
			{
				continue;
			}

			if (Game::scene->sceneModelCount >= ARRAYSIZE(Game::scene->sceneModel))
			{
				continue;
			}

			const auto sceneModelIndex = Game::scene->sceneModelCount++;
			auto& sceneModel = Game::scene->sceneModel[sceneModelIndex];
			std::memset(&sceneModel, 0, sizeof(sceneModel));

			sceneModel.model = model;
			sceneModel.obj = sceneDObj.obj;
			sceneModel.placement.base = sceneDObj.placement;
			sceneModel.placement.scale = scale;
			sceneModel.gfxEntIndex = sceneDObj.gfxEntIndex;
			sceneModel.entnum = sceneDObj.entnum;
			sceneModel.renderFxFlags = sceneDObj.renderFxFlags;
			sceneModel.radius = model->radius * scale;
			sceneModel.cachedLightingHandle = sceneDObj.info.cachedLightingHandle;
			std::memcpy(sceneModel.lightingOrigin, sceneDObj.lightingOrigin, sizeof(sceneModel.lightingOrigin));
			sceneModel.reflectionProbeIndex = sceneDObj.reflectionProbeIndex;

			for (auto viewIndex = 0; viewIndex < ARRAYSIZE(Game::scene->sceneModelVisData); ++viewIndex)
			{
				Game::scene->sceneModelVisData[viewIndex][sceneModelIndex] = 1;
				if (i < ARRAYSIZE(Game::scene->sceneDObjVisData[viewIndex]))
				{
					Game::scene->sceneDObjVisData[viewIndex][i] = 0;
				}
			}

			std::memset(sceneDObj.lods, -1, sizeof(sceneDObj.lods));
		}
	}

	__declspec(naked) void ScriptExtension::R_AddDObjToScene_Stub()
	{
		__asm
		{
			pushad

			push dword ptr [esp + 24h]
			call ApplyDObjModelOverride
			add esp, 4h

			popad

			fldz
			sub esp, 14h
			push 50B705h
			ret
		}
	}

	void ScriptExtension::R_GenerateSortedDrawSurfs_Hk(void* viewInfo)
	{
		ApplySceneScales();
		Utils::Hook::Call<void(void*)>(0x53BEB0)(viewInfo);
	}

	void ScriptExtension::R_AddSceneSurfaces_Hk(const int viewIndex)
	{
		ApplySceneScales();
		Utils::Hook::Call<void(int)>(0x514A60)(viewIndex);
	}

	void ScriptExtension::ClearSceneScales()
	{
		SceneScaleTransitions.clear();
		ModelSceneScales.clear();
		EntitySceneScales.clear();
		LastSceneScaleApplyTime = std::numeric_limits<int>::min();
	}

	Game::gentity_s* ScriptExtension::Scr_GetEntity(const Game::scr_entref_t entref)
	{
		if (entref.classnum)
		{
			Game::Scr_ObjectError("not an entity");
			return nullptr;
		}

		assert(entref.entnum < Game::MAX_GENTITIES);
		return &Game::g_entities[entref.entnum];
	}

	Game::XModel* ScriptExtension::GetModelByIndex(const int modelIndex)
	{
		if (modelIndex <= 0)
		{
			return nullptr;
		}

		if (ModelCache::modelsHaveBeenReallocated)
		{
			if (modelIndex >= ModelCache::G_MODELINDEX_LIMIT)
			{
				return nullptr;
			}

			return ModelCache::cached_models_reallocated[modelIndex];
		}

		if (modelIndex >= Game::MAX_MODELS)
		{
			return nullptr;
		}

		return Game::G_GetModel(modelIndex);
	}

	int ScriptExtension::AllocateEntityModelCloneIndex()
	{
		if (!ModelCache::modelsHaveBeenReallocated)
		{
			return 0;
		}

		const auto firstPrivateModel = ModelCache::BASE_GMODEL_COUNT + ModelCache::ADDITIONAL_GMODELS + 1;
		for (auto i = ModelCache::G_MODELINDEX_LIMIT - 1; i >= firstPrivateModel; --i)
		{
			if (!ModelCache::cached_models_reallocated[i] && !ModelCache::gameModels_reallocated[i])
			{
				return i;
			}
		}

		return 0;
	}

	Game::XModel* ScriptExtension::CloneModelForIndex(Game::XModel* sourceModel, const int modelIndex, const char* cloneName)
	{
		if (!sourceModel || modelIndex <= 0 || !ModelCache::modelsHaveBeenReallocated || modelIndex >= ModelCache::G_MODELINDEX_LIMIT)
		{
			return nullptr;
		}

		auto* clone = Utils::Memory::GetAllocator()->allocate<Game::XModel>();
		std::memcpy(clone, sourceModel, sizeof(Game::XModel));
		clone->name = Utils::Memory::GetAllocator()->duplicateString(cloneName);

		if (const auto visualModel = VisualScaledModels.find(sourceModel); visualModel != VisualScaledModels.end() && visualModel->second.initialized)
		{
			std::memcpy(clone->lodInfo, visualModel->second.originalLodInfo, sizeof(clone->lodInfo));
			clone->radius = visualModel->second.originalRadius;
			clone->bounds = visualModel->second.originalBounds;
			clone->scale = visualModel->second.originalScale;
		}

		ModelCache::cached_models_reallocated[modelIndex] = clone;
		ModelCache::gameModels_reallocated[modelIndex] = clone;
		return clone;
	}

	Game::XModel* ScriptExtension::GetOrCreateEntityModelClone(Game::gentity_s* ent)
	{
		if (!ent)
		{
			return nullptr;
		}

		const auto entNum = ent->s.number;
		if (const auto cloneIndex = EntityModelCloneIndexes.find(entNum); cloneIndex != EntityModelCloneIndexes.end())
		{
			if (ent->model == cloneIndex->second)
			{
				return GetModelByIndex(cloneIndex->second);
			}

			RemoveModelScaleTransition(GetModelByIndex(cloneIndex->second));
			ModelCache::cached_models_reallocated[cloneIndex->second] = nullptr;
			ModelCache::gameModels_reallocated[cloneIndex->second] = nullptr;
			EntityModelCloneIndexes.erase(cloneIndex);
			EntityModelCloneSources.erase(entNum);
			EntityModelCloneOrigins.erase(entNum);
			EntityVisualModelOverrides.erase(entNum);
		}

		auto* sourceModel = GetModelByIndex(ent->model);
		if (!sourceModel)
		{
			Game::Scr_ObjectError("ResizeModel: entity has no xmodel");
			return nullptr;
		}

		const auto cloneIndex = AllocateEntityModelCloneIndex();
		if (!cloneIndex)
		{
			Game::Scr_Error("ResizeModel: no free model index for entity clone");
			return nullptr;
		}

		const auto sourceName = std::string(sourceModel->name);
		auto* clone = CloneModelForIndex(sourceModel, cloneIndex, Utils::String::VA("resizeModel_%i_%s", entNum, sourceModel->name));
		if (!clone)
		{
			Game::Scr_Error("ResizeModel: failed to clone xmodel");
			return nullptr;
		}

		EntityModelCloneIndexes[entNum] = cloneIndex;
		EntityModelCloneSources[entNum] = sourceName;
		EntityVisualModelOverrides[entNum] = clone;

		ent->model = static_cast<unsigned __int16>(cloneIndex);
		ent->s.index.xmodel = cloneIndex;

		return clone;
	}

	Game::XModel* ScriptExtension::GetOrCreateClientModelClone(const int entNum, const int modelIndex, const char* sourceModelName)
	{
		if (modelIndex <= 0 || modelIndex >= ModelCache::G_MODELINDEX_LIMIT)
		{
			return nullptr;
		}

		if (const auto cloneIndex = EntityModelCloneIndexes.find(entNum); cloneIndex != EntityModelCloneIndexes.end())
		{
			if (cloneIndex->second == modelIndex)
			{
				return GetModelByIndex(cloneIndex->second);
			}

			RemoveModelScaleTransition(GetModelByIndex(cloneIndex->second));
			ModelCache::cached_models_reallocated[cloneIndex->second] = nullptr;
			ModelCache::gameModels_reallocated[cloneIndex->second] = nullptr;
			EntityModelCloneIndexes.erase(cloneIndex);
			EntityModelCloneSources.erase(entNum);
			EntityModelCloneOrigins.erase(entNum);
			EntityVisualModelOverrides.erase(entNum);
		}

		auto* sourceModel = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_XMODEL, sourceModelName).model;
		if (!sourceModel || Game::DB_IsXAssetDefault(Game::ASSET_TYPE_XMODEL, sourceModelName))
		{
			return nullptr;
		}

		auto* clone = CloneModelForIndex(sourceModel, modelIndex, Utils::String::VA("resizeModel_%i_%s", entNum, sourceModelName));
		if (clone)
		{
			EntityModelCloneIndexes[entNum] = modelIndex;
			EntityModelCloneSources[entNum] = sourceModelName;
			EntityVisualModelOverrides[entNum] = clone;
		}

		return clone;
	}

	void ScriptExtension::ClearEntityModelClones()
	{
		for (const auto& [entNum, modelIndex] : EntityModelCloneIndexes)
		{
			static_cast<void>(entNum);

			if (modelIndex > 0 && modelIndex < ModelCache::G_MODELINDEX_LIMIT)
			{
				if (auto* clone = GetModelByIndex(modelIndex))
				{
					RemoveModelScaleTransition(clone);
					VisualScaledModels.erase(clone);
				}

				ModelCache::cached_models_reallocated[modelIndex] = nullptr;
				ModelCache::gameModels_reallocated[modelIndex] = nullptr;
			}
		}

		EntityModelCloneIndexes.clear();
		EntityModelCloneSources.clear();
		EntityModelCloneOrigins.clear();
		EntityVisualModelOverrides.clear();
	}

	void ScriptExtension::GScr_ResizeModels()
	{
		const auto numParams = Game::Scr_GetNumParam();
		if (numParams < 2 || numParams > 3)
		{
			Game::Scr_Error("ResizeModels: Usage resizeModels(<xmodel>, <factor>, [time])");
			return;
		}

		const auto* modelName = Game::Scr_GetString(0);
		auto* model = GetResizeModel(modelName, 0);
		if (!model)
		{
			return;
		}

		const auto targetScale = Game::Scr_GetFloat(1);
		if (!IsValidScale(targetScale))
		{
			Game::Scr_ParamError(1, "ResizeModels: factor must be greater than 0");
			return;
		}

		auto time = 0.0f;
		if (numParams == 3)
		{
			time = Game::Scr_GetFloat(2);
			if (!std::isfinite(time))
			{
				Game::Scr_ParamError(2, "ResizeModels: time must be finite");
				return;
			}
		}

		ResizeXModelsByName(modelName, model, targetScale, time);
		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} resizeModels \"{}\" {} {}", ResizeServerCommand, modelName, targetScale, time));
	}

	void ScriptExtension::ScrCmd_ResizeModel(const Game::scr_entref_t entref)
	{
		const auto numParams = Game::Scr_GetNumParam();
		if (numParams < 1 || numParams > 2)
		{
			Game::Scr_Error("ResizeModel: Usage <entity> resizeModel(<factor>, [time])");
			return;
		}

		auto* ent = Scr_GetEntity(entref);
		if (!ent)
		{
			return;
		}

		const auto targetScale = Game::Scr_GetFloat(0);
		if (!IsValidScale(targetScale))
		{
			Game::Scr_ParamError(0, "ResizeModel: factor must be greater than 0");
			return;
		}

		auto time = 0.0f;
		if (numParams == 2)
		{
			time = Game::Scr_GetFloat(1);
			if (!std::isfinite(time))
			{
				Game::Scr_ParamError(1, "ResizeModel: time must be finite");
				return;
			}
		}

		const auto entNum = ent->s.number;
		std::string sourceModelName;
		if (const auto source = EntityModelCloneSources.find(entNum); source != EntityModelCloneSources.end())
		{
			sourceModelName = source->second;
		}
		else if (auto* sourceModel = GetModelByIndex(ent->model); sourceModel && sourceModel->name)
		{
			sourceModelName = sourceModel->name;
		}

		auto* clone = GetOrCreateEntityModelClone(ent);
		if (!clone)
		{
			return;
		}

		if (sourceModelName.empty())
		{
			sourceModelName = clone->name;
		}

		ResizeXModel(clone, targetScale, time);
		EntityModelCloneOrigins[entNum] = {ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2]};
		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_RELIABLE, Utils::String::Format(
			"{:c} resizeModel {} {} \"{}\" {} {} {} {} {}",
			ResizeServerCommand,
			entNum,
			ent->model,
			sourceModelName,
			ent->r.currentOrigin[0],
			ent->r.currentOrigin[1],
			ent->r.currentOrigin[2],
			targetScale,
			time));
	}

	void ScriptExtension::AddResizeFunction()
	{
		Script::AddFunction("resize", GScr_ResizeModels); // gsc: resize(<xmodel>, <factor>, [time])
		Script::AddFunction("resizeModels", GScr_ResizeModels); // gsc: resizeModels(<xmodel>, <factor>, [time])
		Script::AddMethod("resizeModel", ScrCmd_ResizeModel); // gsc: <entity> resizeModel(<factor>, [time])

		ServerCommands::OnCommand(ResizeServerCommand, [](const Command::Params* params)
		{
			if (params->size() < 4)
			{
				return false;
			}

			if (std::strcmp(params->get(1), "resizeModels") == 0)
			{
				auto* model = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_XMODEL, params->get(2)).model;
				if (!model || Game::DB_IsXAssetDefault(Game::ASSET_TYPE_XMODEL, params->get(2)))
				{
					return true;
				}

				const auto targetScale = static_cast<float>(std::atof(params->get(3)));
				const auto time = params->size() >= 5 ? static_cast<float>(std::atof(params->get(4))) : 0.0f;

				if (!IsValidScale(targetScale) || !std::isfinite(time))
				{
					return true;
				}

				ResizeXModelsByName(params->get(2), model, targetScale, time);
				return true;
			}

			if (std::strcmp(params->get(1), "resizeModel") == 0)
			{
				const auto entNum = std::atoi(params->get(2));
				if (entNum < 0 || entNum >= static_cast<int>(Game::MAX_GENTITIES))
				{
					return true;
				}

				if (params->size() < 9)
				{
					return true;
				}

				const auto modelIndex = std::atoi(params->get(3));
				auto* model = GetOrCreateClientModelClone(entNum, modelIndex, params->get(4));
				if (!model)
				{
					return true;
				}

				EntityModelCloneOrigins[entNum] = {
					static_cast<float>(std::atof(params->get(5))),
					static_cast<float>(std::atof(params->get(6))),
					static_cast<float>(std::atof(params->get(7))),
				};

				const auto targetScale = static_cast<float>(std::atof(params->get(8)));
				const auto time = params->size() >= 10 ? static_cast<float>(std::atof(params->get(9))) : 0.0f;

				if (!IsValidScale(targetScale) || !std::isfinite(time))
				{
					return true;
				}

				ResizeXModel(model, targetScale, time);
				return true;
			}

			return false;
		});
	}

	void ScriptExtension::AddFunctions()
	{
		Script::AddFunction("IsArray", [] // gsc: IsArray(<object>)
		{
			auto type = Game::Scr_GetType(0);

			bool result;
			if (type == Game::VAR_POINTER)
			{
				type = Game::Scr_GetPointerType(0);
				assert(type >= Game::FIRST_OBJECT);
				result = (type == Game::VAR_ARRAY);
			}
			else
			{
				assert(type < Game::FIRST_OBJECT);
				result = false;
			}

			Game::Scr_AddBool(result);
		});

		Script::AddFunction("ReplaceFunc", [] // gsc: ReplaceFunc(<function>, <function>)
		{
			if (Game::Scr_GetNumParam() != 2)
			{
				Game::Scr_Error("ReplaceFunc: Needs two parameters!");
				return;
			}

			const auto what = GetCodePosForParam(0);
			const auto with = GetCodePosForParam(1);

			SetReplacedPos(what, with);
		});


		Script::AddFunction("GetSystemMilliseconds", [] // gsc: GetSystemMilliseconds()
		{
			SYSTEMTIME time;
			GetSystemTime(&time);

			Game::Scr_AddInt(time.wMilliseconds);
		});

		Script::AddFunction("Exec", [] // gsc: Exec(<string>)
		{
			const auto* str = Game::Scr_GetString(0);
			if (!str)
			{
				Game::Scr_ParamError(0, "Exec: Illegal parameter!");
				return;
			}

			Command::Execute(str, false);
		});

		// Allow printing to the console even when developer is 0
		Script::AddFunction("PrintConsole", [] // gsc: PrintConsole(<string>)
		{
			for (std::size_t i = 0; i < Game::Scr_GetNumParam(); ++i)
			{
				const auto* str = Game::Scr_GetString(i);
				if (!str)
				{
					Game::Scr_ParamError(i, "PrintConsole: Illegal parameter!");
					return;
				}

				Logger::Print(Game::level->scriptPrintChannel, "{}", str);
			}
		});

		AddResizeFunction();
	}

	ScriptExtension::ScriptExtension()
	{
		AddFunctions();

		Utils::Hook(0x61E92E, VMExecuteInternalStub, HOOK_JUMP).install()->quick();
		Utils::Hook::Nop(0x61E933, 1);

		Events::OnVMShutdown([]
		{
			ReplacedFunctions.clear();
			ClearVisualScaledModels();
			ClearSceneScales();
			ClearEntityModelClones();
		});

		Events::OnCLDisconnected([]([[maybe_unused]] bool wasConnected)
		{
			ClearVisualScaledModels();
			ClearSceneScales();
			ClearEntityModelClones();
		});

		Scheduler::Loop(UpdateModelScaleTransitions, Scheduler::Pipeline::MAIN);

		Utils::Hook(0x50B700, R_AddDObjToScene_Stub, HOOK_JUMP).install()->quick();
		Utils::Hook(0x50E6F3, R_GenerateSortedDrawSurfs_Hk, HOOK_CALL).install()->quick();
		Utils::Hook(0x50E762, R_AddSceneSurfaces_Hk, HOOK_CALL).install()->quick();
	}
}
