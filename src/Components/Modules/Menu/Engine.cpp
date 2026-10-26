#include "STDInclude.hpp"
#include "../Dedicated.hpp"
#include "../ZoneBuilder.hpp"
#include "../Logger.hpp"
#include "../FastFiles.hpp"
#include "../Maps.hpp"
#include "Engine.hpp"
#include "../Remix/Core.hpp"

namespace Components::Menu::Engine
{
	LogLine::~LogLine()
	{
		const std::string message = text.str();
		Remix::WriteDiagnosticLog("", message);
		Logger::Print("{}\n", message);
	}

	void Initialize()
	{
		cm = Bind<Game::clipMap_t*>(0x141BD3180);
		rendererWorld = Bind<GfxWorld**>(0x148CCC7A0);
		R_ClearScene = Bind<decltype(R_ClearScene)>(0x1400240A0);
		R_ClearSceneView = Bind<decltype(R_ClearSceneView)>(0x140024190);
		CL_ResetSkeletonCache = Bind<decltype(CL_ResetSkeletonCache)>(0x1400FD340);
		R_SetLodOrigin = Bind<decltype(R_SetLodOrigin)>(0x140024690);
		R_AddDObjToScene = Bind<decltype(R_AddDObjToScene)>(0x14001F290);
		R_EndDObjScene = Bind<decltype(R_EndDObjScene)>(0x14001F760);
		CL_GetScreenDimensions = Bind<decltype(CL_GetScreenDimensions)>(0x1400F4820);
		CL_GetMenuBlurRadius = Bind<decltype(CL_GetMenuBlurRadius)>(0x140101860);
		CM_LoadMap = Bind<decltype(CM_LoadMap)>(0x1401E84C0);
		LoadWorld = Bind<LoadWorld_t>(0x1400F5B10);
		XAnimCreateAnims = Bind<decltype(XAnimCreateAnims)>(0x1402B18B0);
		XAnimBlend = Bind<decltype(XAnimBlend)>(0x1402B0460);
		XAnimCreate = Bind<decltype(XAnimCreate)>(0x1402B1870);
		XAnimCreateTree = Bind<decltype(XAnimCreateTree)>(0x1402B1940);
		XAnimSetCompleteGoalWeight = Bind<decltype(XAnimSetCompleteGoalWeight)>(0x1402B3860);
		XAnimFreeTree = Bind<decltype(XAnimFreeTree)>(0x1402B2090);
		DObjCreate = Bind<decltype(DObjCreate)>(0x1402AB640);
		DObjFree = Bind<decltype(DObjFree)>(0x1402ABB00);
		DObjUpdateClientInfo = Bind<decltype(DObjUpdateClientInfo)>(0x1402AFF10);
		DObjGetNextChildBoneIndex = Bind<decltype(DObjGetNextChildBoneIndex)>(0x1402AC3E0);
		DObjSetHidePartBits = Bind<decltype(DObjSetHidePartBits)>(0x1402ACAE0);
		R_BeginRemoteScreenUpdate = Bind<R_BeginRemoteScreenUpdate_t>(0x14001DAC0);
		R_EndRemoteScreenUpdate = Bind<R_EndRemoteScreenUpdate_t>(0x14001DB30);
		DB_GetLoadedFraction = Bind<decltype(DB_GetLoadedFraction)>(0x140117CE0);
		DB_IsFileLoaded = Bind<decltype(DB_IsFileLoaded)>(0x14012E0E0);
		Sys_SyncDatabase = Bind<decltype(Sys_SyncDatabase)>(0x14012F990);
		Com_ErrorEntered = Bind<decltype(Com_ErrorEntered)>(0x1401F3F40);
	}

	std::optional<std::filesystem::path> ZoneFile(const std::string& name)
	{
		return FastFiles::FindZoneFile(name);
	}

	std::vector<std::string> LoadedNames(Game::XAssetType type)
	{
		struct Context { Game::XAssetType type; std::vector<std::string> names; } context{type, {}};
		Game::DB_EnumXAssets_FastFile(type, [](void* header, void* data)
		{
			auto& context = *static_cast<Context*>(data);
			Game::XAsset asset{static_cast<unsigned int>(context.type), {}};
			asset.header.data = header;
			if (const char* name = Game::DB_GetXAssetName(&asset); name && *name)
				context.names.emplace_back(name);
		}, &context, false);
		return std::move(context.names);
	}

	void LoadMap(Game::XZoneInfo* zones, unsigned int count, int sync)
	{
		// Reuse the project's team, dependency, patch and localization routing.
		// Mount custom-map IWDs without changing the user's selected lobby map,
		// and without the loading screen a real level load opens: for campaign
		// maps (gulag, oilrig, the test map...) that screen took the menus'
		// input and never closed, since no level load follows.
		if (count && zones[0].name) Maps::PrepareUsermap(zones[0].name, false, false);
		Maps::LoadMapZones(zones, count, sync);
	}
}
