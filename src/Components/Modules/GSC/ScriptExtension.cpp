#include "STDInclude.hpp"

#include <shared_mutex>

#include "ScriptExtension.hpp"
#include "Script.hpp"
#include "../Command.hpp"
#include "../Dedicated.hpp"
#include "../Events.hpp"
#include "../Logger.hpp"
#include "../ModelCache.hpp"
#include "../ModelSurfs.hpp"
#include "../Scheduler.hpp"
#include "../ServerCommands.hpp"

namespace Components::GSC
{
	std::unordered_map<const char*, const char*> ScriptExtension::replacedFunctions;
	const char* ScriptExtension::replacedPos = nullptr;

	constexpr std::uintptr_t VM_ExecuteDispatch_CodePosLoad = 0x14022BA74;
	constexpr std::uintptr_t scrVm_codePos = 0x14228A0F8;
	static const std::uint8_t codePosLoad[] = { 0x48, 0x8B, 0x0D, 0x7D, 0xE6, 0x05, 0x02 };

	static Utils::Hook codePosHook;

	extern "C"
	{
		void VM_Execute_CodePosStub();
		std::uintptr_t ScriptExtension_CodePos = 0;

		const char* ScriptExtension_NextCodePos(const char* pos)
		{
			return ScriptExtension::NextCodePos(pos);
		}
	}

	const char* ScriptExtension::GetCodePosForParam(int index)
	{
		if (static_cast<unsigned int>(index) >= *Game::scrVmPub_outparamcount)
		{
			Script::Scr_ParamError(static_cast<unsigned int>(index), "GetCodePosForParam: Index is out of range!");
			return "";
		}

		const auto* value = &(*Game::scrVmPub_top)[-index];

		if (value->type != Game::VAR_FUNCTION)
		{
			Script::Scr_ParamError(static_cast<unsigned int>(index), "GetCodePosForParam: Expects a function as parameter!");
			return "";
		}

		return value->u.codePosValue;
	}

	void ScriptExtension::GetReplacedPos(const char* pos)
	{
		if (!pos)
		{
			return;
		}

		const auto itr = replacedFunctions.find(pos);

		if (itr != replacedFunctions.end())
		{
			replacedPos = itr->second;
		}
	}

	void ScriptExtension::SetReplacedPos(const char* what, const char* with)
	{
		if (!*what || !*with)
		{
			Logger::Warning("Invalid parameters passed to ReplacedFunctions\n");
			return;
		}

		if (replacedFunctions.contains(what))
		{
			Logger::Warning("ReplacedFunctions already contains codePosValue for a function\n");
		}

		replacedFunctions[what] = with;
	}

	const char* ScriptExtension::NextCodePos(const char* pos)
	{
		GetReplacedPos(pos);

		if (!replacedPos)
		{
			return pos;
		}

		const char* const next = replacedPos;
		replacedPos = nullptr;

		return next;
	}

	constexpr int resizeServerCommand = 22;
	constexpr float minModelScale = 0.01f;
	constexpr float maxModelScale = 64.0f;
	constexpr auto scriptModelAnimationClearDelay = std::chrono::milliseconds(75);

	constexpr int scriptModelEntityType = 6;
	constexpr std::size_t scriptModelAnimIndex = 0x5C;

	static bool IsValidScale(const float scale)
	{
		return std::isfinite(scale) && scale >= minModelScale && scale <= maxModelScale;
	}

	static std::chrono::milliseconds GetScaleDuration(const float timeSeconds)
	{
		if (timeSeconds <= 0.0f)
		{
			return std::chrono::milliseconds(0);
		}

		const auto durationMs = std::min(timeSeconds * 1000.0f, static_cast<float>(std::numeric_limits<int>::max()));

		return std::chrono::milliseconds(std::max(1, static_cast<int>(durationMs)));
	}

	static Game::XModel* FindResizeModel(const char* name)
	{
		auto* const model = static_cast<Game::XModel*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_XMODEL, name));

		if (!model || Game::DB_IsXAssetDefault(Game::ASSET_TYPE_XMODEL, name))
		{
			return nullptr;
		}

		return model;
	}

	static Game::gentity_s* GetResizeEntity(const Game::scr_entref_t entref)
	{
		if (entref.classnum)
		{
			Script::Scr_ObjectError("not an entity");
			return nullptr;
		}

		return &Game::g_entities[entref.entnum];
	}

	static bool IsScriptModelEntity(const Game::gentity_s* ent)
	{
		return ent->s.eType == scriptModelEntityType && ent->script_classname == Game::scr_const->script_model;
	}

	static std::chrono::milliseconds GetScriptModelResizeDelay(const Game::gentity_s* ent)
	{
		if (!IsScriptModelEntity(ent))
		{
			return std::chrono::milliseconds(0);
		}

		const auto animIndex = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(ent) + scriptModelAnimIndex);

		if (animIndex == 0)
		{
			return std::chrono::milliseconds(0);
		}

		return scriptModelAnimationClearDelay;
	}

	struct ServerClone
	{
		int modelIndex;
		std::string sourceName;
		float scale = 1.0f;
	};

	struct ServerCloneRelease
	{
		int entNum;
		std::chrono::steady_clock::time_point releaseTime;
	};

	static std::unordered_map<int, ServerClone> serverClones;
	static std::vector<ServerCloneRelease> serverCloneReleases;

	static std::unordered_map<std::string, float> serverModelScales;

	static bool isClientReplayed[Game::MAX_CLIENTS];

	static Game::XModel* GetServerModel(const int modelIndex)
	{
		if (!ModelCache::cachedModelsReallocated || modelIndex <= 0 || modelIndex >= ModelCache::G_MODELINDEX_LIMIT)
		{
			return nullptr;
		}

		return ModelCache::cachedModelsReallocated[modelIndex];
	}

	static int AllocateCloneIndex(Game::XModel* sourceModel)
	{
		if (!ModelCache::HasCloneSlots())
		{
			return 0;
		}

		for (int modelIndex = ModelCache::G_MODELINDEX_LIMIT - 1; modelIndex >= ModelCache::firstCloneModelIndex; --modelIndex)
		{
			if (!ModelCache::cachedModelsReallocated[modelIndex])
			{
				ModelCache::cachedModelsReallocated[modelIndex] = sourceModel;
				return modelIndex;
			}
		}

		return 0;
	}

	static void RemoveServerCloneRelease(const int entNum)
	{
		std::erase_if(serverCloneReleases, [entNum](const ServerCloneRelease& release)
		{
			return release.entNum == entNum;
		});
	}

	static void ReleaseServerClone(const int entNum)
	{
		RemoveServerCloneRelease(entNum);

		const auto record = serverClones.find(entNum);

		if (record == serverClones.end())
		{
			return;
		}

		ModelCache::cachedModelsReallocated[record->second.modelIndex] = nullptr;
		serverClones.erase(record);
	}

	static void UpdateServerCloneReleases()
	{
		const auto now = std::chrono::steady_clock::now();
		std::vector<int> dueReleases;

		for (auto i = serverCloneReleases.begin(); i != serverCloneReleases.end();)
		{
			if (now < i->releaseTime)
			{
				++i;
				continue;
			}

			dueReleases.push_back(i->entNum);
			i = serverCloneReleases.erase(i);
		}

		for (const auto entNum : dueReleases)
		{
			ReleaseServerClone(entNum);
		}
	}

	static void ClearServerClones()
	{
		for (const auto& [entNum, record] : serverClones)
		{
			ModelCache::cachedModelsReallocated[record.modelIndex] = nullptr;
		}

		serverClones.clear();
		serverCloneReleases.clear();
		serverModelScales.clear();
	}

	static void ReplayResizes(const int clientNum)
	{
		for (const auto& [modelName, scale] : serverModelScales)
		{
			Game::SV_GameSendServerCommand(clientNum, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} resizeModels \"{}\" {} 0 0",
				resizeServerCommand, modelName, scale));
		}

		for (const auto& [entNum, record] : serverClones)
		{
			const bool isResetting = std::ranges::any_of(serverCloneReleases, [entNum](const ServerCloneRelease& release)
			{
				return release.entNum == entNum;
			});

			if (isResetting)
			{
				continue;
			}

			const Game::gentity_s* const ent = &Game::g_entities[entNum];
			const Game::XModel* const model = GetServerModel(ent->model);

			if (!ent->r.isInUse || !model || std::strcmp(model->name, record.sourceName.data()) != 0)
			{
				continue;
			}

			Game::SV_GameSendServerCommand(clientNum, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} resizeModel {} {} \"{}\" {} {} {} {} 0 0",
				resizeServerCommand, entNum, record.modelIndex, record.sourceName,
				ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2], record.scale));
		}
	}

	static void ReplayResizesToNewClients()
	{
		const int clientCount = std::clamp(*Game::svs_clientCount, 0, static_cast<int>(Game::MAX_CLIENTS));

		for (int clientNum = 0; clientNum < clientCount; ++clientNum)
		{
			const Game::client_s& client = Game::svs_clients[clientNum];

			if (client.header.state < Game::CS_ACTIVE)
			{
				isClientReplayed[clientNum] = false;
				continue;
			}

			if (isClientReplayed[clientNum])
			{
				continue;
			}

			isClientReplayed[clientNum] = true;

			if (!client.bIsTestClient)
			{
				ReplayResizes(clientNum);
			}
		}
	}

	static const char* GetServerSourceName(const int entNum)
	{
		const auto record = serverClones.find(entNum);

		if (record == serverClones.end())
		{
			return nullptr;
		}

		return record->second.sourceName.data();
	}

	static std::chrono::milliseconds GetScriptModelsResizeDelay(const char* modelName)
	{
		auto delay = std::chrono::milliseconds(0);
		const auto entityLimit = std::min(Game::level->num_entities, static_cast<int>(Game::MAX_GENTITIES));

		for (int entNum = 0; entNum < entityLimit; ++entNum)
		{
			const auto* ent = &Game::g_entities[entNum];

			if (!IsScriptModelEntity(ent))
			{
				continue;
			}

			const char* sourceName = GetServerSourceName(entNum);

			if (!sourceName)
			{
				const auto* model = GetServerModel(ent->model);

				if (model)
				{
					sourceName = model->name;
				}
			}

			if (!sourceName || std::strcmp(sourceName, modelName) != 0)
			{
				continue;
			}

			delay = std::max(delay, GetScriptModelResizeDelay(ent));
		}

		return delay;
	}

	static void GScr_ResizeModels()
	{
		const auto numParams = Game::Scr_GetNumParam();

		if (numParams < 2 || numParams > 3)
		{
			Script::Scr_Error("ResizeModels: Usage resizeModels(<xmodel>, <factor>, [time])");
			return;
		}

		const auto* modelName = Game::Scr_GetString(0);

		if (!modelName || !*modelName)
		{
			Script::Scr_ParamError(0, "ResizeModel: Illegal model parameter!");
			return;
		}

		if (!FindResizeModel(modelName))
		{
			Script::Scr_ParamError(0, Utils::String::VA("ResizeModel: xmodel '%s' does not exist", modelName));
			return;
		}

		const auto targetScale = Game::Scr_GetFloat(1);

		if (!IsValidScale(targetScale))
		{
			Script::Scr_ParamError(1, "ResizeModels: factor must be between 0.01 and 64.0");
			return;
		}

		float timeSeconds = 0.0f;

		if (numParams == 3)
		{
			timeSeconds = Game::Scr_GetFloat(2);

			if (!std::isfinite(timeSeconds))
			{
				Script::Scr_ParamError(2, "ResizeModels: time must be finite");
				return;
			}
		}

		const auto delay = GetScriptModelsResizeDelay(modelName);

		serverModelScales[modelName] = targetScale;

		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} resizeModels \"{}\" {} {} {}",
			resizeServerCommand, modelName, targetScale, timeSeconds, delay.count()));
	}

	static void GScr_ResetModels()
	{
		const auto numParams = Game::Scr_GetNumParam();

		if (numParams < 1 || numParams > 2)
		{
			Script::Scr_Error("ResetModels: Usage resetModels(<xmodel>, [time])");
			return;
		}

		const auto* modelName = Game::Scr_GetString(0);

		if (!modelName || !*modelName)
		{
			Script::Scr_ParamError(0, "ResizeModel: Illegal model parameter!");
			return;
		}

		if (!FindResizeModel(modelName))
		{
			Script::Scr_ParamError(0, Utils::String::VA("ResizeModel: xmodel '%s' does not exist", modelName));
			return;
		}

		float timeSeconds = 0.0f;

		if (numParams == 2)
		{
			timeSeconds = Game::Scr_GetFloat(1);

			if (!std::isfinite(timeSeconds))
			{
				Script::Scr_ParamError(1, "ResetModels: time must be finite");
				return;
			}
		}

		auto delay = std::chrono::milliseconds(0);

		if (timeSeconds > 0.0f)
		{
			delay = GetScriptModelsResizeDelay(modelName);
		}

		serverModelScales.erase(modelName);

		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} resetModels \"{}\" {} {}",
			resizeServerCommand, modelName, timeSeconds, delay.count()));
	}

	static void ScrCmd_ResizeModel(const Game::scr_entref_t entref)
	{
		const auto numParams = Game::Scr_GetNumParam();

		if (numParams < 1 || numParams > 2)
		{
			Script::Scr_Error("ResizeModel: Usage <entity> resizeModel(<factor>, [time])");
			return;
		}

		auto* const ent = GetResizeEntity(entref);

		if (!ent)
		{
			return;
		}

		const auto targetScale = Game::Scr_GetFloat(0);

		if (!IsValidScale(targetScale))
		{
			Script::Scr_ParamError(0, "ResizeModel: factor must be between 0.01 and 64.0");
			return;
		}

		float timeSeconds = 0.0f;

		if (numParams == 2)
		{
			timeSeconds = Game::Scr_GetFloat(1);

			if (!std::isfinite(timeSeconds))
			{
				Script::Scr_ParamError(1, "ResizeModel: time must be finite");
				return;
			}
		}

		const auto entNum = ent->s.number;
		auto* const currentModel = GetServerModel(ent->model);
		const char* recordedSource = GetServerSourceName(entNum);

		if (!currentModel && !recordedSource)
		{
			Script::Scr_ObjectError("ResizeModel: entity has no xmodel");
			return;
		}

		RemoveServerCloneRelease(entNum);

		const bool isSameSource = recordedSource && currentModel && std::strcmp(recordedSource, currentModel->name) == 0;

		if (recordedSource && !isSameSource)
		{
			ReleaseServerClone(entNum);
		}

		if (!isSameSource)
		{
			if (!currentModel)
			{
				Script::Scr_ObjectError("ResizeModel: entity has no xmodel");
				return;
			}

			const auto cloneIndex = AllocateCloneIndex(currentModel);

			if (!cloneIndex)
			{
				Script::Scr_Error("ResizeModel: no free model index for entity clone");
				return;
			}

			serverClones[entNum] = { cloneIndex, currentModel->name };
		}

		auto& record = serverClones[entNum];
		record.scale = targetScale;

		const auto delay = GetScriptModelResizeDelay(ent);

		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} resizeModel {} {} \"{}\" {} {} {} {} {} {}",
			resizeServerCommand, entNum, record.modelIndex, record.sourceName,
			ent->r.currentOrigin[0], ent->r.currentOrigin[1], ent->r.currentOrigin[2],
			targetScale, timeSeconds, delay.count()));
	}

	static void ScrCmd_ResetModel(const Game::scr_entref_t entref)
	{
		const auto numParams = Game::Scr_GetNumParam();

		if (numParams > 1)
		{
			Script::Scr_Error("ResetModel: Usage <entity> resetModel([time])");
			return;
		}

		auto* const ent = GetResizeEntity(entref);

		if (!ent)
		{
			return;
		}

		float timeSeconds = 0.0f;

		if (numParams == 1)
		{
			timeSeconds = Game::Scr_GetFloat(0);

			if (!std::isfinite(timeSeconds))
			{
				Script::Scr_ParamError(0, "ResetModel: time must be finite");
				return;
			}
		}

		const auto entNum = ent->s.number;
		auto delay = std::chrono::milliseconds(0);

		if (timeSeconds > 0.0f)
		{
			delay = GetScriptModelResizeDelay(ent);
		}

		if (timeSeconds <= 0.0f && delay.count() <= 0)
		{
			ReleaseServerClone(entNum);
		}
		else if (serverClones.contains(entNum))
		{
			RemoveServerCloneRelease(entNum);
			serverCloneReleases.push_back({ entNum, std::chrono::steady_clock::now() + delay + GetScaleDuration(timeSeconds) });
		}

		Game::SV_GameSendServerCommand(-1, Game::SV_CMD_RELIABLE, Utils::String::Format("{:c} resetModel {} {} {}",
			resizeServerCommand, entNum, timeSeconds, delay.count()));
	}

	enum class ResizeTarget
	{
		ModelName,
		Entity,
	};

	struct ModelScaleTransition
	{
		Game::XModel* model;
		float startScale;
		float targetScale;
		std::chrono::steady_clock::time_point startTime;
		std::chrono::milliseconds duration;
	};

	struct PendingResetCleanup
	{
		ResizeTarget target;
		std::string modelName;
		int entNum;
		std::chrono::steady_clock::time_point cleanupTime;
	};

	struct PendingOverride
	{
		ResizeTarget target;
		std::string modelName;
		int entNum;
		Game::XModel* model;
		std::chrono::steady_clock::time_point applyTime;
	};

	struct VisualScaledModel
	{
		Game::XModelLodInfo originalLodInfo[4];
		Game::XModelSurfs* scaledSurfs[4];
		Game::DObjAnimMat* originalBaseMat;
		float* originalTrans;
		Game::XBoneInfo* originalBoneInfo;
		std::vector<Game::DObjAnimMat> originalBaseMats;
		std::vector<Game::DObjAnimMat> scaledBaseMats;
		std::vector<float> originalTransValues;
		std::vector<float> scaledTransValues;
		std::vector<Game::XBoneInfo> originalBoneInfos;
		std::vector<Game::XBoneInfo> scaledBoneInfos;
		float originalRadius;
		Game::Bounds originalBounds;
		float scale;
		bool initialized;
	};

	struct EntityClone
	{
		int modelIndex;
		std::string sourceName;
		Game::XModel* model;
	};

	struct NameHash
	{
		using is_transparent = void;

		std::size_t operator()(std::string_view name) const
		{
			return std::hash<std::string_view>{}(name);
		}
	};

	using ModelsByName = std::unordered_map<std::string, Game::XModel*, NameHash, std::equal_to<>>;

	static std::shared_mutex overrideMutex;
	static std::unordered_map<int, EntityClone> entityClones;
	static std::unordered_map<int, Game::XModel*> entityOverrides;
	static ModelsByName globalClones;
	static ModelsByName globalOverrides;

	static std::unordered_map<const Game::XModel*, Game::XModel*> copySources;

	static std::unordered_map<Game::XModel*, VisualScaledModel> visualScaledModels;
	static std::vector<ModelScaleTransition> modelScaleTransitions;
	static std::vector<PendingResetCleanup> pendingResetCleanups;
	static std::vector<PendingOverride> pendingOverrides;
	static Utils::Memory::Allocator modelAllocator;

	static std::atomic_bool isClientClearRequested = false;

	static void RemoveModelScaleTransition(const Game::XModel* model)
	{
		std::erase_if(modelScaleTransitions, [model](const ModelScaleTransition& transition)
		{
			return transition.model == model;
		});
	}

	static float GetXModelVisualScale(Game::XModel* model)
	{
		const auto visualModel = visualScaledModels.find(model);

		if (visualModel == visualScaledModels.end())
		{
			return 1.0f;
		}

		return visualModel->second.scale;
	}

	static void ScaleBounds(Game::Bounds& bounds, const float scale)
	{
		for (int component = 0; component < 3; ++component)
		{
			bounds.midPoint[component] *= scale;
			bounds.halfSize[component] *= scale;
		}
	}

	static void SetXModelVisualSkeletonScale(Game::XModel* model, VisualScaledModel& visualModel, const float scale)
	{
		if (!visualModel.originalBaseMats.empty())
		{
			visualModel.scaledBaseMats.resize(visualModel.originalBaseMats.size());

			for (std::size_t i = 0; i < visualModel.originalBaseMats.size(); ++i)
			{
				auto boneMat = visualModel.originalBaseMats[i];
				boneMat.trans[0] *= scale;
				boneMat.trans[1] *= scale;
				boneMat.trans[2] *= scale;
				visualModel.scaledBaseMats[i] = boneMat;
			}

			model->baseMat = visualModel.scaledBaseMats.data();
		}
		else
		{
			model->baseMat = visualModel.originalBaseMat;
		}

		if (!visualModel.originalTransValues.empty())
		{
			visualModel.scaledTransValues.resize(visualModel.originalTransValues.size());

			for (std::size_t i = 0; i < visualModel.originalTransValues.size(); ++i)
			{
				visualModel.scaledTransValues[i] = visualModel.originalTransValues[i] * scale;
			}

			model->trans = visualModel.scaledTransValues.data();
		}
		else
		{
			model->trans = visualModel.originalTrans;
		}

		if (!visualModel.originalBoneInfos.empty())
		{
			visualModel.scaledBoneInfos.resize(visualModel.originalBoneInfos.size());

			for (std::size_t i = 0; i < visualModel.originalBoneInfos.size(); ++i)
			{
				auto boneInfo = visualModel.originalBoneInfos[i];
				ScaleBounds(boneInfo.bounds, scale);
				boneInfo.radiusSquared *= scale * scale;
				visualModel.scaledBoneInfos[i] = boneInfo;
			}

			model->boneInfo = visualModel.scaledBoneInfos.data();
		}
		else
		{
			model->boneInfo = visualModel.originalBoneInfo;
		}
	}

	static void RestoreXModelVisualScale(Game::XModel* model, const VisualScaledModel& visualModel)
	{
		if (!visualModel.initialized)
		{
			return;
		}

		std::memcpy(model->lodInfo, visualModel.originalLodInfo, sizeof(model->lodInfo));
		model->baseMat = visualModel.originalBaseMat;
		model->trans = visualModel.originalTrans;
		model->boneInfo = visualModel.originalBoneInfo;
		model->radius = visualModel.originalRadius;
		model->bounds = visualModel.originalBounds;
	}

	static void SetXModelVisualScale(Game::XModel* model, const float scale)
	{
		auto& visualModel = visualScaledModels[model];

		if (!visualModel.initialized)
		{
			std::memcpy(visualModel.originalLodInfo, model->lodInfo, sizeof(model->lodInfo));
			std::memset(visualModel.scaledSurfs, 0, sizeof(visualModel.scaledSurfs));
			visualModel.originalRadius = model->radius;
			visualModel.originalBounds = model->bounds;
			visualModel.originalBaseMat = model->baseMat;
			visualModel.originalTrans = model->trans;
			visualModel.originalBoneInfo = model->boneInfo;

			const auto boneCount = static_cast<std::size_t>(model->numBones);
			const auto rootBoneCount = static_cast<std::size_t>(model->numRootBones);

			if (visualModel.originalBaseMat && boneCount)
			{
				visualModel.originalBaseMats.assign(visualModel.originalBaseMat, visualModel.originalBaseMat + boneCount);
			}

			if (visualModel.originalTrans && boneCount > rootBoneCount)
			{
				const auto transValueCount = (boneCount - rootBoneCount) * 3;
				visualModel.originalTransValues.assign(visualModel.originalTrans, visualModel.originalTrans + transValueCount);
			}

			if (visualModel.originalBoneInfo && boneCount)
			{
				visualModel.originalBoneInfos.assign(visualModel.originalBoneInfo, visualModel.originalBoneInfo + boneCount);
			}

			visualModel.scale = 1.0f;
			visualModel.initialized = true;
		}

		for (int lodIndex = 0; lodIndex < static_cast<int>(std::size(model->lodInfo)); ++lodIndex)
		{
			const auto& originalLod = visualModel.originalLodInfo[lodIndex];

			if (!originalLod.modelSurfs || !originalLod.modelSurfs->surfs)
			{
				continue;
			}

			if (!visualModel.scaledSurfs[lodIndex])
			{
				visualModel.scaledSurfs[lodIndex] = ModelSurfs::CloneAndScaleSurfaces(originalLod.modelSurfs,
					Utils::String::VA("resize_%s_lod%i", model->name, lodIndex), scale);
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

		model->radius = visualModel.originalRadius * scale;
		model->bounds = visualModel.originalBounds;
		ScaleBounds(model->bounds, scale);
		SetXModelVisualSkeletonScale(model, visualModel, scale);
		visualModel.scale = scale;
	}

	static void ResetXModelVisualScale(Game::XModel* model)
	{
		RemoveModelScaleTransition(model);

		const auto visualModel = visualScaledModels.find(model);

		if (visualModel == visualScaledModels.end())
		{
			return;
		}

		RestoreXModelVisualScale(model, visualModel->second);
		visualModel->second.scale = 1.0f;
	}

	static void ResizeXModelDelayed(Game::XModel* model, const float targetScale, const float timeSeconds, const std::chrono::milliseconds delay)
	{
		RemoveModelScaleTransition(model);

		if (timeSeconds <= 0.0f && delay.count() <= 0)
		{
			SetXModelVisualScale(model, targetScale);
			return;
		}

		const auto startTime = std::chrono::steady_clock::now() + std::max(delay, std::chrono::milliseconds(0));

		modelScaleTransitions.push_back({ model, GetXModelVisualScale(model), targetScale, startTime, GetScaleDuration(timeSeconds) });
	}

	static void RemovePendingOverride(const ResizeTarget target, const std::string& modelName, const int entNum)
	{
		std::erase_if(pendingOverrides, [target, &modelName, entNum](const PendingOverride& pending)
		{
			if (pending.target != target)
			{
				return false;
			}

			if (target == ResizeTarget::Entity)
			{
				return pending.entNum == entNum;
			}

			return pending.modelName == modelName;
		});
	}

	static void ScheduleEntityOverride(const int entNum, Game::XModel* model, const std::chrono::milliseconds delay)
	{
		RemovePendingOverride(ResizeTarget::Entity, {}, entNum);

		if (delay.count() <= 0)
		{
			std::unique_lock lock(overrideMutex);
			entityOverrides[entNum] = model;
			return;
		}

		pendingOverrides.push_back({ ResizeTarget::Entity, {}, entNum, model, std::chrono::steady_clock::now() + delay });
	}

	static void ScheduleGlobalOverride(const std::string& modelName, Game::XModel* model, const std::chrono::milliseconds delay)
	{
		RemovePendingOverride(ResizeTarget::ModelName, modelName, 0);

		if (delay.count() <= 0)
		{
			std::unique_lock lock(overrideMutex);
			globalOverrides[modelName] = model;
			return;
		}

		pendingOverrides.push_back({ ResizeTarget::ModelName, modelName, 0, model, std::chrono::steady_clock::now() + delay });
	}

	static void UpdatePendingOverrides()
	{
		const auto now = std::chrono::steady_clock::now();

		for (auto i = pendingOverrides.begin(); i != pendingOverrides.end();)
		{
			if (now < i->applyTime)
			{
				++i;
				continue;
			}

			{
				std::unique_lock lock(overrideMutex);

				if (i->target == ResizeTarget::Entity)
				{
					entityOverrides[i->entNum] = i->model;
				}
				else
				{
					globalOverrides[i->modelName] = i->model;
				}
			}

			i = pendingOverrides.erase(i);
		}
	}

	static void RemovePendingResetCleanup(const ResizeTarget target, const std::string& modelName, const int entNum)
	{
		std::erase_if(pendingResetCleanups, [target, &modelName, entNum](const PendingResetCleanup& cleanup)
		{
			if (cleanup.target != target)
			{
				return false;
			}

			if (target == ResizeTarget::Entity)
			{
				return cleanup.entNum == entNum;
			}

			return cleanup.modelName == modelName;
		});
	}

	static void ScheduleResetCleanup(const ResizeTarget target, const std::string& modelName, const int entNum, const float timeSeconds, const std::chrono::milliseconds delay)
	{
		RemovePendingResetCleanup(target, modelName, entNum);

		const auto cleanupDelay = std::max(delay, std::chrono::milliseconds(0)) + GetScaleDuration(timeSeconds);

		pendingResetCleanups.push_back({ target, modelName, entNum, std::chrono::steady_clock::now() + cleanupDelay });
	}

	static Game::XModel* CloneModel(Game::XModel* sourceModel, const char* cloneName)
	{
		auto* const clone = modelAllocator.Allocate<Game::XModel>();
		std::memcpy(clone, sourceModel, sizeof(Game::XModel));
		clone->name = modelAllocator.DuplicateString(cloneName);

		std::unique_lock lock(overrideMutex);
		copySources[clone] = sourceModel;

		return clone;
	}

	static Game::XModel* GetOrCreateGlobalClone(const std::string& modelName, Game::XModel* sourceModel)
	{
		if (const auto clone = globalClones.find(modelName); clone != globalClones.end())
		{
			return clone->second;
		}

		auto* const clone = CloneModel(sourceModel, Utils::String::VA("resizeModels_%s", sourceModel->name));

		std::unique_lock lock(overrideMutex);
		globalClones[modelName] = clone;

		return clone;
	}

	static void ClearEntityClone(const int entNum)
	{
		RemovePendingResetCleanup(ResizeTarget::Entity, {}, entNum);
		RemovePendingOverride(ResizeTarget::Entity, {}, entNum);

		const auto record = entityClones.find(entNum);

		if (record != entityClones.end())
		{
			ResetXModelVisualScale(record->second.model);

			if (ModelCache::gameModelsReallocated[record->second.modelIndex] == record->second.model)
			{
				ModelCache::gameModelsReallocated[record->second.modelIndex] = nullptr;
			}
		}

		std::unique_lock lock(overrideMutex);
		entityClones.erase(entNum);
		entityOverrides.erase(entNum);
	}

	static Game::XModel* GetOrCreateEntityClone(const int entNum, const int modelIndex, const char* sourceName, const bool activateOverride)
	{
		if (const auto record = entityClones.find(entNum); record != entityClones.end())
		{
			if (record->second.modelIndex == modelIndex && record->second.sourceName == sourceName)
			{
				if (activateOverride)
				{
					std::unique_lock lock(overrideMutex);
					entityOverrides[entNum] = record->second.model;
				}

				return record->second.model;
			}

			ClearEntityClone(entNum);
		}

		auto* const sourceModel = FindResizeModel(sourceName);

		if (!sourceModel)
		{
			return nullptr;
		}

		auto* const clone = CloneModel(sourceModel, Utils::String::VA("resizeModel_%i_%s", entNum, sourceName));
		ModelCache::gameModelsReallocated[modelIndex] = clone;

		std::unique_lock lock(overrideMutex);
		entityClones[entNum] = { modelIndex, sourceName, clone };

		if (activateOverride)
		{
			entityOverrides[entNum] = clone;
		}

		return clone;
	}

	static void ResizeXModelsByName(const std::string& modelName, Game::XModel* model, const float targetScale, const float timeSeconds, const std::chrono::milliseconds delay)
	{
		RemovePendingResetCleanup(ResizeTarget::ModelName, modelName, 0);

		auto* const globalClone = GetOrCreateGlobalClone(modelName, model);
		ScheduleGlobalOverride(modelName, globalClone, delay);
		ResizeXModelDelayed(globalClone, targetScale, timeSeconds, delay);

		for (const auto& [entNum, record] : entityClones)
		{
			if (record.sourceName != modelName)
			{
				continue;
			}

			ScheduleEntityOverride(entNum, record.model, delay);
			ResizeXModelDelayed(record.model, targetScale, timeSeconds, delay);
		}
	}

	static void ResetXModelsByName(const std::string& modelName, const float timeSeconds, const std::chrono::milliseconds delay)
	{
		RemovePendingResetCleanup(ResizeTarget::ModelName, modelName, 0);
		RemovePendingOverride(ResizeTarget::ModelName, modelName, 0);

		if (timeSeconds > 0.0f || delay.count() > 0)
		{
			bool isResetScheduled = false;

			if (const auto clone = globalClones.find(modelName); clone != globalClones.end())
			{
				ResizeXModelDelayed(clone->second, 1.0f, timeSeconds, delay);
				isResetScheduled = true;
			}

			for (const auto& [entNum, model] : entityOverrides)
			{
				const auto record = entityClones.find(entNum);

				if (record == entityClones.end() || record->second.sourceName != modelName)
				{
					continue;
				}

				ResizeXModelDelayed(model, 1.0f, timeSeconds, delay);
				isResetScheduled = true;
			}

			if (isResetScheduled)
			{
				ScheduleResetCleanup(ResizeTarget::ModelName, modelName, 0, timeSeconds, delay);
				return;
			}
		}

		if (const auto clone = globalClones.find(modelName); clone != globalClones.end())
		{
			ResetXModelVisualScale(clone->second);

			std::unique_lock lock(overrideMutex);
			globalClones.erase(clone);
		}

		{
			std::unique_lock lock(overrideMutex);
			globalOverrides.erase(modelName);
		}

		for (const auto& [entNum, model] : entityOverrides)
		{
			const auto record = entityClones.find(entNum);

			if (record == entityClones.end() || record->second.sourceName != modelName)
			{
				continue;
			}

			ResetXModelVisualScale(model);
		}
	}

	static void UpdatePendingResetCleanups()
	{
		const auto now = std::chrono::steady_clock::now();
		std::vector<PendingResetCleanup> dueCleanups;

		for (auto i = pendingResetCleanups.begin(); i != pendingResetCleanups.end();)
		{
			if (now < i->cleanupTime)
			{
				++i;
				continue;
			}

			dueCleanups.push_back(*i);
			i = pendingResetCleanups.erase(i);
		}

		for (const auto& cleanup : dueCleanups)
		{
			if (cleanup.target == ResizeTarget::Entity)
			{
				ClearEntityClone(cleanup.entNum);
				continue;
			}

			ResetXModelsByName(cleanup.modelName, 0.0f, std::chrono::milliseconds(0));
		}
	}

	static void UpdateModelScaleTransitions()
	{
		const auto now = std::chrono::steady_clock::now();

		for (auto i = modelScaleTransitions.begin(); i != modelScaleTransitions.end();)
		{
			if (now < i->startTime)
			{
				++i;
				continue;
			}

			const auto elapsed = now - i->startTime;

			if (i->duration.count() <= 0 || elapsed >= i->duration)
			{
				SetXModelVisualScale(i->model, i->targetScale);
				i = modelScaleTransitions.erase(i);
				continue;
			}

			const auto elapsedMs = static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
			const auto fraction = std::clamp(elapsedMs / static_cast<float>(i->duration.count()), 0.0f, 1.0f);

			SetXModelVisualScale(i->model, i->startScale + (i->targetScale - i->startScale) * fraction);
			++i;
		}
	}

	static void ClearClientState()
	{
		for (auto& [model, visualModel] : visualScaledModels)
		{
			RestoreXModelVisualScale(model, visualModel);
			visualModel.scale = 1.0f;
		}

		for (const auto& [entNum, record] : entityClones)
		{
			if (ModelCache::gameModelsReallocated[record.modelIndex] == record.model)
			{
				ModelCache::gameModelsReallocated[record.modelIndex] = nullptr;
			}
		}

		modelScaleTransitions.clear();
		pendingResetCleanups.clear();
		pendingOverrides.clear();

		std::unique_lock lock(overrideMutex);
		entityClones.clear();
		entityOverrides.clear();
		globalClones.clear();
		globalOverrides.clear();
	}

	static void FreeCopies()
	{
		if (copySources.empty())
		{
			return;
		}

		Game::R_SyncRenderThread();
		Game::R_WaitWorkerCmds();

		ClearClientState();
		visualScaledModels.clear();

		{
			std::unique_lock lock(overrideMutex);
			copySources.clear();
		}

		ModelSurfs::FreeClones();
		modelAllocator.Clear();
	}

	static void UpdateClientResize()
	{
		if (isClientClearRequested.exchange(false))
		{
			ClearClientState();
		}

		UpdatePendingOverrides();
		UpdateModelScaleTransitions();
		UpdatePendingResetCleanups();
	}

	static bool HasCompatibleDObjHierarchy(const Game::XModel* source, const Game::XModel* replacement)
	{
		return source && replacement
			&& source->numBones == replacement->numBones
			&& source->numRootBones == replacement->numRootBones
			&& source->numsurfs == replacement->numsurfs
			&& source->boneNames == replacement->boneNames
			&& source->parentList == replacement->parentList
			&& source->quats == replacement->quats
			&& source->partClassification == replacement->partClassification;
	}

	static Game::XModel* FindGlobalOverride(const char* modelName)
	{
		const std::string_view name = modelName;

		if (const auto globalOverride = globalOverrides.find(name); globalOverride != globalOverrides.end())
		{
			return globalOverride->second;
		}

		constexpr std::string_view globalClonePrefix = "resizeModels_";

		if (name.starts_with(globalClonePrefix))
		{
			if (const auto globalOverride = globalOverrides.find(name.substr(globalClonePrefix.size())); globalOverride != globalOverrides.end())
			{
				return globalOverride->second;
			}
		}

		for (const auto& [sourceName, clone] : globalClones)
		{
			if (std::strcmp(modelName, clone->name) != 0)
			{
				continue;
			}

			if (const auto globalOverride = globalOverrides.find(sourceName); globalOverride != globalOverrides.end())
			{
				return globalOverride->second;
			}
		}

		return nullptr;
	}

	static float GetDObjRadiusBound(const Game::DObj* obj)
	{
		float radius = 0.0f;

		for (int modelIndex = 0; modelIndex < obj->numModels; ++modelIndex)
		{
			if (obj->models[modelIndex])
			{
				radius += obj->models[modelIndex]->radius;
			}
		}

		return radius;
	}

	static void ApplyDObjModelOverride(Game::DObj* obj, const unsigned int entNum)
	{
		std::shared_lock lock(overrideMutex);

		if (copySources.empty())
		{
			return;
		}

		auto entityOverride = entityOverrides.find(static_cast<int>(entNum));

		if (entityOverride == entityOverrides.end())
		{
			entityOverride = entityOverrides.find(obj->entnum);
		}

		Game::XModel* entityModel = nullptr;
		const char* entitySource = nullptr;

		if (entityOverride != entityOverrides.end())
		{
			entityModel = entityOverride->second;

			if (const auto record = entityClones.find(entityOverride->first); record != entityClones.end())
			{
				entitySource = record->second.sourceName.data();
			}
		}

		bool isRadiusStale = false;

		for (int modelIndex = 0; modelIndex < obj->numModels; ++modelIndex)
		{
			auto*& model = obj->models[modelIndex];

			if (!model)
			{
				continue;
			}

			auto* source = model;

			if (const auto copy = copySources.find(model); copy != copySources.end())
			{
				source = copy->second;
			}

			auto* replacement = source;

			if (entitySource && std::strcmp(entitySource, source->name) == 0 && HasCompatibleDObjHierarchy(source, entityModel))
			{
				replacement = entityModel;
			}
			else
			{
				auto* const globalOverride = FindGlobalOverride(source->name);

				if (HasCompatibleDObjHierarchy(source, globalOverride))
				{
					replacement = globalOverride;
				}
			}

			if (replacement != model || replacement != source)
			{
				isRadiusStale = true;
			}

			model = replacement;
		}

		if (isRadiusStale)
		{
			obj->radius = GetDObjRadiusBound(obj);
		}
	}

	constexpr std::uintptr_t R_AddDObjToScene = 0x14001F290;
	constexpr std::uintptr_t R_AddDObjToSceneCalls[] =
	{
		0x1400D4921, 0x1400D5414, 0x1400D55DC, 0x1400D58E7, 0x1400DFAB5, 0x1400DFFAE, 0x14029818B,
	};

	constexpr std::uintptr_t R_FilterXModelIntoScene = 0x14002A580;
	constexpr std::uintptr_t FX_DrawElem_Model_FilterCall = 0x14013A303;

	constexpr std::uintptr_t CG_Shutdown = 0x1400DA0B0;
	constexpr std::uintptr_t CL_ShutdownCGame_ShutdownCall = 0x1400F592D;

	using R_AddDObjToScene_t = void(*)(const Game::DObj* obj, const Game::cpose_t* pose, unsigned int entnum, unsigned int renderFxFlags, float* lightingOrigin, float materialTime);
	using R_FilterXModelIntoScene_t = void(*)(const Game::XModel* model, const void* placement, unsigned int renderFxFlags, unsigned short* cachedLightingHandle);
	using CG_Shutdown_t = void(*)(int localClientNum);

	static R_AddDObjToScene_t addDObjToScene = nullptr;
	static R_FilterXModelIntoScene_t filterXModelIntoScene = nullptr;
	static CG_Shutdown_t cgShutdown = nullptr;
	static Utils::Hook clientHooks[std::size(R_AddDObjToSceneCalls) + 2];

	static void CG_Shutdown_Hook(const int localClientNum)
	{
		cgShutdown(localClientNum);
		FreeCopies();
	}

	static void R_AddDObjToScene_Hook(const Game::DObj* obj, const Game::cpose_t* pose, unsigned int entnum, unsigned int renderFxFlags, float* lightingOrigin, float materialTime)
	{
		ApplyDObjModelOverride(const_cast<Game::DObj*>(obj), entnum);
		addDObjToScene(obj, pose, entnum, renderFxFlags, lightingOrigin, materialTime);
	}

	static void R_FilterXModelIntoScene_Hook(const Game::XModel* model, const void* placement, unsigned int renderFxFlags, unsigned short* cachedLightingHandle)
	{
		const Game::XModel* sceneModel = model;

		{
			std::shared_lock lock(overrideMutex);

			if (auto* const globalOverride = FindGlobalOverride(model->name))
			{
				sceneModel = globalOverride;
			}
		}

		filterXModelIntoScene(sceneModel, placement, renderFxFlags, cachedLightingHandle);
	}

	static bool InstallClientHooks()
	{
		struct HookSite
		{
			std::uintptr_t site;
			std::uintptr_t callee;
			void* replacement;
		};

		std::vector<HookSite> sites;

		for (const auto site : R_AddDObjToSceneCalls)
		{
			sites.push_back({ site, R_AddDObjToScene, reinterpret_cast<void*>(R_AddDObjToScene_Hook) });
		}

		sites.push_back({ FX_DrawElem_Model_FilterCall, R_FilterXModelIntoScene, reinterpret_cast<void*>(R_FilterXModelIntoScene_Hook) });
		sites.push_back({ CL_ShutdownCGame_ShutdownCall, CG_Shutdown, reinterpret_cast<void*>(CG_Shutdown_Hook) });

		for (const auto& hookSite : sites)
		{
			if (!Utils::Hook::BranchesTo(hookSite.site, hookSite.callee, false))
			{
				Logger::Error("scriptextension: 0x{:X} no longer calls 0x{:X}, models cannot be resized\n", hookSite.site, hookSite.callee);
				return false;
			}
		}

		addDObjToScene = reinterpret_cast<R_AddDObjToScene_t>(Utils::Hook::Rebase(R_AddDObjToScene));
		filterXModelIntoScene = reinterpret_cast<R_FilterXModelIntoScene_t>(Utils::Hook::Rebase(R_FilterXModelIntoScene));
		cgShutdown = reinterpret_cast<CG_Shutdown_t>(Utils::Hook::Rebase(CG_Shutdown));

		bool isSeated = true;

		for (std::size_t i = 0; i < std::size(clientHooks); ++i)
		{
			isSeated = clientHooks[i].Initialize(sites[i].site, sites[i].replacement, HOOK_CALL)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (auto& hook : clientHooks)
			{
				hook.Uninstall();
			}

			Logger::Error("scriptextension: could not seat every resize hook, models cannot be resized\n");
			return false;
		}

		for (auto& hook : clientHooks)
		{
			hook.Quick();
		}

		return true;
	}

	static float ParseFloat(const char* text)
	{
		return static_cast<float>(std::atof(text));
	}

	static std::chrono::milliseconds ParseDelay(const Command::Params* params, const int index)
	{
		if (params->Size() <= index)
		{
			return std::chrono::milliseconds(0);
		}

		return std::chrono::milliseconds(std::max(0, std::atoi(params->Get(index))));
	}

	static bool OnResizeCommand(const Command::Params* params)
	{
		if (params->Size() < 2)
		{
			return false;
		}

		const std::string_view subcommand = params->Get(1);

		if (subcommand == "resizeModels")
		{
			if (params->Size() < 4)
			{
				return true;
			}

			auto* const model = FindResizeModel(params->Get(2));
			const auto targetScale = ParseFloat(params->Get(3));
			float timeSeconds = 0.0f;

			if (params->Size() >= 5)
			{
				timeSeconds = ParseFloat(params->Get(4));
			}

			if (!model || !IsValidScale(targetScale) || !std::isfinite(timeSeconds))
			{
				return true;
			}

			ResizeXModelsByName(params->Get(2), model, targetScale, timeSeconds, ParseDelay(params, 5));
			return true;
		}

		if (subcommand == "resetModels")
		{
			if (params->Size() < 3)
			{
				return true;
			}

			float timeSeconds = 0.0f;

			if (params->Size() >= 4)
			{
				timeSeconds = ParseFloat(params->Get(3));
			}

			if (!FindResizeModel(params->Get(2)) || !std::isfinite(timeSeconds))
			{
				return true;
			}

			ResetXModelsByName(params->Get(2), timeSeconds, ParseDelay(params, 4));
			return true;
		}

		if (subcommand == "resizeModel")
		{
			if (params->Size() < 10)
			{
				return true;
			}

			const auto entNum = std::atoi(params->Get(2));
			const auto modelIndex = std::atoi(params->Get(3));
			const auto targetScale = ParseFloat(params->Get(8));
			const auto timeSeconds = ParseFloat(params->Get(9));
			const auto delay = ParseDelay(params, 10);

			if (entNum < 0 || entNum >= static_cast<int>(Game::MAX_GENTITIES))
			{
				return true;
			}

			if (!ModelCache::HasCloneSlots() || modelIndex < ModelCache::firstCloneModelIndex || modelIndex >= ModelCache::G_MODELINDEX_LIMIT)
			{
				return true;
			}

			if (!IsValidScale(targetScale) || !std::isfinite(timeSeconds))
			{
				return true;
			}

			RemovePendingResetCleanup(ResizeTarget::Entity, {}, entNum);

			auto* const model = GetOrCreateEntityClone(entNum, modelIndex, params->Get(4), delay.count() <= 0);

			if (!model)
			{
				return true;
			}

			ScheduleEntityOverride(entNum, model, delay);
			ResizeXModelDelayed(model, targetScale, timeSeconds, delay);
			return true;
		}

		if (subcommand == "resetModel")
		{
			if (params->Size() < 3)
			{
				return true;
			}

			const auto entNum = std::atoi(params->Get(2));
			float timeSeconds = 0.0f;

			if (params->Size() >= 4)
			{
				timeSeconds = ParseFloat(params->Get(3));
			}

			const auto delay = ParseDelay(params, 4);

			if (entNum < 0 || entNum >= static_cast<int>(Game::MAX_GENTITIES) || !std::isfinite(timeSeconds))
			{
				return true;
			}

			if (timeSeconds <= 0.0f && delay.count() <= 0)
			{
				ClearEntityClone(entNum);
				return true;
			}

			RemovePendingResetCleanup(ResizeTarget::Entity, {}, entNum);
			RemovePendingOverride(ResizeTarget::Entity, {}, entNum);

			if (const auto record = entityClones.find(entNum); record != entityClones.end())
			{
				ResizeXModelDelayed(record->second.model, 1.0f, timeSeconds, delay);
				ScheduleResetCleanup(ResizeTarget::Entity, {}, entNum, timeSeconds, delay);
			}

			return true;
		}

		return false;
	}

	void ScriptExtension::AddResizeFunctions()
	{
		Script::AddFunction("resize", GScr_ResizeModels);
		Script::AddFunction("resizeModels", GScr_ResizeModels);
		Script::AddFunction("resetModels", GScr_ResetModels);
		Script::AddMethod("resizeModel", ScrCmd_ResizeModel);
		Script::AddMethod("resetModel", ScrCmd_ResetModel);

		Scheduler::Loop(UpdateServerCloneReleases, Scheduler::Pipeline::SERVER);
		Scheduler::Loop(ReplayResizesToNewClients, Scheduler::Pipeline::SERVER);

		const bool isClientEnabled = !Dedicated::IsEnabled() && ModelSurfs::IsInstalled();

		Events::OnVMShutdown([isClientEnabled]
		{
			ClearServerClones();

			if (isClientEnabled)
			{
				isClientClearRequested = true;
			}
		});

		if (!isClientEnabled || !InstallClientHooks())
		{
			return;
		}

		ServerCommands::OnCommand(resizeServerCommand, OnResizeCommand);
		Scheduler::Loop(UpdateClientResize, Scheduler::Pipeline::MAIN);

		Events::OnCLDisconnected([]([[maybe_unused]] bool wasConnected)
		{
			ClearClientState();
		});

		Events::OnCGameInit([]
		{
			ClearClientState();
		});
	}

	void ScriptExtension::AddFunctions()
	{
		Script::AddFunction("IsArray", []
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

		Script::AddFunction("ReplaceFunc", []
		{
			if (Game::Scr_GetNumParam() != 2)
			{
				Script::Scr_Error("ReplaceFunc: Needs two parameters!");
				return;
			}

			const auto what = GetCodePosForParam(0);
			const auto with = GetCodePosForParam(1);

			SetReplacedPos(what, with);
		});

		Script::AddFunction("GetSystemMilliseconds", []
		{
			SYSTEMTIME time;
			GetSystemTime(&time);

			Game::Scr_AddInt(time.wMilliseconds);
		});

		Script::AddFunction("Exec", []
		{
			const auto* str = Game::Scr_GetString(0);

			if (!str)
			{
				Script::Scr_ParamError(0, "Exec: Illegal parameter!");
				return;
			}

			Command::Execute(str, false);
		});

		Script::AddFunction("PrintConsole", []
		{
			for (std::size_t i = 0; i < Game::Scr_GetNumParam(); ++i)
			{
				const auto* str = Game::Scr_GetString(static_cast<unsigned int>(i));

				if (!str)
				{
					Script::Scr_ParamError(static_cast<unsigned int>(i), "PrintConsole: Illegal parameter!");
					return;
				}

				Logger::Print("{}", str);
			}
		});
	}

	ScriptExtension::ScriptExtension()
	{
		AddFunctions();
		AddResizeFunctions();

		if (!Utils::Hook::MatchesBytes(VM_ExecuteDispatch_CodePosLoad, codePosLoad, sizeof(codePosLoad)))
		{
			Logger::Error("scriptextension: VM_ExecuteDispatch does not read as expected, ReplaceFunc will do nothing\n");
			return;
		}

		ScriptExtension_CodePos = Utils::Hook::Rebase(scrVm_codePos);

		if (!codePosHook.Initialize(VM_ExecuteDispatch_CodePosLoad, VM_Execute_CodePosStub, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("scriptextension: could not hook VM_ExecuteDispatch, ReplaceFunc will do nothing\n");
			return;
		}

		codePosHook.Quick();
		Utils::Hook::Nop(VM_ExecuteDispatch_CodePosLoad + 5, sizeof(codePosLoad) - 5);

		Events::OnVMShutdown([]
		{
			replacedFunctions.clear();
		});
	}
}
