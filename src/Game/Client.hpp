#pragma once

namespace Game
{
	typedef int(*CL_GetClientName_t)(int localClientNum, int index, char* buf, int size);
	extern CL_GetClientName_t CL_GetClientName;

	typedef bool(*CL_IsCgameInitialized_t)(int localClientNum);
	extern CL_IsCgameInitialized_t CL_IsCgameInitialized;

	typedef void(*CL_ConnectFromParty_t)(int controllerIndex, _XSESSION_INFO* hostInfo,
		netadr_t addr, int numPublicSlots, int numPrivateSlots, const char* mapname, const char* gametype);
	extern CL_ConnectFromParty_t CL_ConnectFromParty;

	typedef void(*CL_DrawStretchPicPhysical_t)(float x, float y, float w, float h,
		float sl, float tl, float sh, float th, const float* color, Material* material);
	extern CL_DrawStretchPicPhysical_t CL_DrawStretchPicPhysical;

	typedef const char*(*CL_GetConfigString_t)(int index);
	extern CL_GetConfigString_t CL_GetConfigString;

	typedef void(*CL_ParseServerMessage_t)(int localClientNum, msg_t* msg);
	extern CL_ParseServerMessage_t CL_ParseServerMessage;

	typedef int(*CL_GetRankForXp_t)(int experience);
	extern CL_GetRankForXp_t CL_GetRankForXp;

	typedef bool(*CL_GetRankIcon_t)(int rank, int prestige, Material** icon);
	extern CL_GetRankIcon_t CL_GetRankIcon;

	typedef void(*CL_DrawStretchPic_t)(const float* placement, float x, float y, float w, float h,
		int horzAlign, int vertAlign, float s1, float t1, float s2, float t2, const float* color, Material* material);
	extern CL_DrawStretchPic_t CL_DrawStretchPic;

	typedef int(*CL_MouseEvent_t)(int x, int y, int dx, int dy);
	extern CL_MouseEvent_t CL_MouseEvent;

	typedef void(*Key_ClearStates_t)(int localClientNum);
	extern Key_ClearStates_t Key_ClearStates;

	connstate_t CL_GetLocalClientConnectionState(int localClientNum);
	cg_s* CL_GetLocalClientGlobals(int localClientNum);
	centity_s* CG_GetEntity(int localClientNum, int entityIndex);

	extern dvar_t** cl_paused;

	void BindClient();
}
