#pragma once

#include "Dvar.hpp"

namespace Components
{
	class Renderer : public Component
	{
	public:
		typedef void(BackendCallback)(IDirect3DDevice9*);
		typedef void(Callback)();

		Renderer();

		static int Width();
		static int Height();

		static void OnBackendFrame(const std::function<BackendCallback>& callback);
		static void OnNextBackendFrame(const std::function<BackendCallback>& callback);

		static void OnDeviceRecoveryEnd(const std::function<Callback>& callback);
		static void OnDeviceRecoveryBegin(const std::function<Callback>& callback);

		static bool IsDeviceRecoveryActive();
		static void FinishLoading();

		static void BackendFrameHandler();

	private:
		static Dvar::Var r_forceTechnique;
		static Dvar::Var r_listSamplers;

		static void ForceTechnique();
		static void ListSamplers();

		static void PreVidRestart();
		static void PostVidRestart();

		static void DB_BeginRecoverLostDevice_Hk();
		static void DB_EndRecoverLostDevice_Hk();
		static void R_Shutdown_Hk(int destroyWindow);
		static void CL_InitRenderer_Hk();

		static std::vector<std::function<Callback>> endRecoverDeviceSignal;
		static std::vector<std::function<Callback>> beginRecoverDeviceSignal;

		static std::vector<std::function<BackendCallback>> backendFrameSignal;
		static std::vector<std::function<BackendCallback>> singleBackendFrameSignal;
	};
}
