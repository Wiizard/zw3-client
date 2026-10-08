#pragma once

#include "Dvar.hpp"

namespace Components
{
	class HudParallax : public Component
	{
	public:
		HudParallax();

	private:
		static Dvar::Var cg_hudParallax;
		static Dvar::Var cg_hudParallaxMenus;
		static Dvar::Var cg_hudParallaxHudElems;
		static Dvar::Var cg_hudParallaxUnanchored;
		static Dvar::Var cg_hudParallaxViewRate;
		static Dvar::Var cg_hudParallaxAnimRate;
		static Dvar::Var cg_hudParallaxLinearForward;
		static Dvar::Var cg_hudParallaxLinearSide;
		static Dvar::Var cg_hudParallaxLinearUp;
		static Dvar::Var cg_hudParallaxLinearMax;
		static Dvar::Var cg_hudParallaxLinearDecay;
		static Dvar::Var cg_hudParallaxSmoothing;
		static Dvar::Var cg_hudParallaxMaxPitch;
		static Dvar::Var cg_hudParallaxMaxYaw;
		static Dvar::Var cg_hudParallaxScale;
		static Dvar::Var cg_hudParallaxSnapDistance;
		static Dvar::Var cg_hudParallaxSnapAngle;
		static Dvar::Var cg_hudParallaxBobPitch;
		static Dvar::Var cg_hudParallaxBobYaw;

		static void RegisterDvars();
		static void StepMotion(int localClientNum);

		static void CG_Draw2D_Hk(int localClientNum);
		static void Menu_PaintViewport_Hk(void* dc);
		static void ScrPlace_ApplyRect_Hk(const float* placement, float* x, float* y, float* w, float* h, int horzAlign, int vertAlign);
		static float ScrPlace_ApplyX_Hk(const float* placement, float x, int horzAlign);
		static float ScrPlace_ApplyY_Hk(const float* placement, float y, int vertAlign);
		static float TextFloorX_Hk(float value);
		static float TextFloorY_Hk(float value);
		static float HudElemFloorX_Hk(float value);
		static float HudElemFloorY_Hk(float value);
		static void HudElemPlace_Hk(int localClientNum, const void* hudElem, float* origin, void* layout);
	};
}
