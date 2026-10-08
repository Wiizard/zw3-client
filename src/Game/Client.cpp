#include "STDInclude.hpp"

namespace Game
{
	CL_GetClientName_t CL_GetClientName = nullptr;
	CL_IsCgameInitialized_t CL_IsCgameInitialized = nullptr;
	CL_ConnectFromParty_t CL_ConnectFromParty = nullptr;
	CL_DrawStretchPicPhysical_t CL_DrawStretchPicPhysical = nullptr;
	CL_GetConfigString_t CL_GetConfigString = nullptr;
	CL_ParseServerMessage_t CL_ParseServerMessage = nullptr;
	CL_GetRankForXp_t CL_GetRankForXp = nullptr;
	CL_GetRankIcon_t CL_GetRankIcon = nullptr;
	CL_DrawStretchPic_t CL_DrawStretchPic = nullptr;
	CL_MouseEvent_t CL_MouseEvent = nullptr;

	Key_ClearStates_t Key_ClearStates = nullptr;

	dvar_t** cl_paused = nullptr;

	connstate_t CL_GetLocalClientConnectionState([[maybe_unused]] int localClientNum)
	{
		assert(localClientNum < static_cast<int>(MAX_LOCAL_CLIENTS));

		return *reinterpret_cast<const connstate_t*>(Utils::Hook::Rebase(0x1406CECF8));
	}

	cg_s* CL_GetLocalClientGlobals([[maybe_unused]] int localClientNum)
	{
		assert(localClientNum < static_cast<int>(MAX_LOCAL_CLIENTS));

		return reinterpret_cast<cg_s*>(Utils::Hook::Rebase(0x1404769A0));
	}

	centity_s* CG_GetEntity([[maybe_unused]] int localClientNum, int entityIndex)
	{
		assert(localClientNum < static_cast<int>(MAX_LOCAL_CLIENTS));

		return &reinterpret_cast<centity_s*>(Utils::Hook::Rebase(0x14059EB20))[entityIndex];
	}

	void BindClient()
	{
		CL_GetClientName = BindFunction<CL_GetClientName_t>(0x140101D80);
		CL_IsCgameInitialized = BindFunction<CL_IsCgameInitialized_t>(0x1400F4DC0);
		CL_ConnectFromParty = BindFunction<CL_ConnectFromParty_t>(0x1400F8920);
		CL_DrawStretchPicPhysical = BindFunction<CL_DrawStretchPicPhysical_t>(0x1400F43F0);
		CL_GetConfigString = BindFunction<CL_GetConfigString_t>(0x1400F47C0);
		CL_ParseServerMessage = BindFunction<CL_ParseServerMessage_t>(0x140100CA0);
		CL_GetRankForXp = BindFunction<CL_GetRankForXp_t>(0x1401016A0);
		CL_GetRankIcon = BindFunction<CL_GetRankIcon_t>(0x140101770);
		CL_DrawStretchPic = BindFunction<CL_DrawStretchPic_t>(0x1400F4190);
		CL_MouseEvent = BindFunction<CL_MouseEvent_t>(0x1400F7050);

		Key_ClearStates = BindFunction<Key_ClearStates_t>(0x1400EF4F0);

		cl_paused = reinterpret_cast<dvar_t**>(Utils::Hook::Rebase(0x141BD9AC8));
	}
}
