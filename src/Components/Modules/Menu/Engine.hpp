#pragma once

// Private, complete renderer inputs for iw4mp 2.0.13. Game::refdef_t is a
// deliberately short overlay of cg_s and cannot be submitted on its own.
namespace Components::Menu::Engine
{
	using Game::XModel;
	using Game::XAnimParts;
	using Game::DObj;
	using Game::cpose_t;
	using Game::GfxWorld;
	using Game::Material;
	using Game::XZoneInfo;
	using Game::dvar_t;
	using Game::UiContext;
	using Game::menuDef_t;
	using Game::connstate_t;
	using Game::CA_DISCONNECTED;
	using Game::CA_CONNECTING;
	using Game::CA_PRIMED;
	using Game::DVAR_ARCHIVE;
	using Game::DVAR_ROM;
	using Game::ASSET_TYPE_XMODEL;
	using Game::ASSET_TYPE_XANIMPARTS;

	struct XAnim_s;
	struct XAnimTree_s;
	struct DObjModel_s
	{
		XModel* model;
		std::uint16_t boneName;
		bool ignoreCollision;
		char padding[5];
	};
	static_assert(sizeof(DObjModel_s) == 16);

	struct GfxLight
	{
		std::byte bytes[64];
		void* definition;
	};
	static_assert(sizeof(GfxLight) == 72);

	struct DepthOfField
	{
		float viewModelStart, viewModelEnd;
		float nearStart, nearEnd, farStart, farEnd, nearBlur, farBlur;
	};

	struct refdef_t
	{
		unsigned int x, y, width, height;
		Game::RefdefView view;
		float viewOffset[3];
		int time;
		float blurRadius;
		DepthOfField dof;
		std::byte vision[80];
		struct { float diffuseScale, specularScale; } charPrimaryLightScale;
		std::byte sunAndFilm[112];
		GfxLight primaryLights[248];
		std::byte scissorAndFlags[24];
		int localClientNum;
	};
	static_assert(offsetof(refdef_t, blurRadius) == 92);
	static_assert(offsetof(refdef_t, dof) == 96);
	static_assert(offsetof(refdef_t, charPrimaryLightScale) == 208);
	static_assert(offsetof(refdef_t, primaryLights) == 328);
	static_assert(offsetof(refdef_t, localClientNum) == 18208);
	static_assert(sizeof(refdef_t) == 18216);

	template <typename T> T Bind(std::uintptr_t address)
	{
		return reinterpret_cast<T>(Utils::Hook::Rebase(address));
	}

	// Bound after Hook::Bind, so all data and code respect ASLR.
	inline Game::clipMap_t* cm;
	inline GfxWorld** rendererWorld;
	inline void (*R_ClearScene)();
	inline void (*R_ClearSceneView)(int);
	inline void (*CL_ResetSkeletonCache)();
	inline void (*R_SetLodOrigin)(refdef_t*, int);
	inline void (*R_AddDObjToScene)(DObj*, cpose_t*, unsigned int, unsigned int, const float*, float);
	inline void (*R_EndDObjScene)();
	inline void (*CL_GetScreenDimensions)(int*, int*, float*);
	inline float (*CL_GetMenuBlurRadius)(int);
	inline void (*CM_LoadMap)(const char*, int*);
	using LoadWorld_t = std::int64_t (*)(const char*);
	inline LoadWorld_t LoadWorld;
	inline XAnim_s* (*XAnimCreateAnims)(const char*, int, void* (*)(int));
	inline void (*XAnimBlend)(XAnim_s*, unsigned int, const char*, unsigned short, unsigned int, unsigned short);
	inline const XAnimParts* (*XAnimCreate)(XAnim_s*, unsigned int, const char*);
	inline XAnimTree_s* (*XAnimCreateTree)(XAnim_s*, void* (*)(int));
	inline void (*XAnimSetCompleteGoalWeight)(DObj*, unsigned int, float, float, float, unsigned int, unsigned int, unsigned int);
	inline void (*XAnimFreeTree)(XAnimTree_s*, void (*)(void*, int));
	inline void (*DObjCreate)(DObjModel_s*, unsigned int, XAnimTree_s*, char*, unsigned int);
	inline void (*DObjFree)(DObj*);
	inline void (*DObjUpdateClientInfo)(DObj*, float, bool);
	inline int (*DObjGetNextChildBoneIndex)(DObj*, unsigned char, unsigned char*);
	inline void (*DObjSetHidePartBits)(DObj*, const unsigned int*);
	using R_AddCmdClearScreen_t = void (*)(int, const float*, float, unsigned char);
	using CG_DrawShellshockBlend_t = void (*)(unsigned int);
	using CG_Draw2D_t = void (*)(int);
	using R_BeginRemoteScreenUpdate_t = void (*)();
	using R_EndRemoteScreenUpdate_t = void (*)();
	inline R_BeginRemoteScreenUpdate_t R_BeginRemoteScreenUpdate;
	inline R_EndRemoteScreenUpdate_t R_EndRemoteScreenUpdate;
	inline float (*DB_GetLoadedFraction)();
	inline bool (*DB_IsFileLoaded)(const char*);
	inline void (*Sys_SyncDatabase)();
	inline bool (*Com_ErrorEntered)();

	void Initialize();
	std::optional<std::filesystem::path> ZoneFile(const std::string& name);
	std::vector<std::string> LoadedNames(Game::XAssetType type);
	void LoadMap(Game::XZoneInfo* zones, unsigned int count, int sync);

	using Game::DObjGetBoneIndex;
	using Game::Dvar_RegisterString;
	using Game::Dvar_GetString;
	using Game::Dvar_SetString;
	using Game::R_SyncRenderThread;
	using Game::Sys_IsDatabaseReady2;
	using Game::Sys_Milliseconds;
	inline int I_stricmp(const char* a, const char* b) { return _stricmp(a, b); }
	inline bool headless() { return Dedicated::IsEnabled() || ZoneBuilder::IsEnabled(); }

	class LogLine
	{
		std::ostringstream text;
	public:
		~LogLine() { Logger::Print("{}\n", text.str()); }
		template <typename T> LogLine& operator<<(const T& value) { text << value; return *this; }
	};
}
