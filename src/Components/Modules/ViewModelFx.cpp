#include "STDInclude.hpp"

#include "ViewModelFxSetup.hpp"
#include "GSC/Script.hpp"

namespace Components::ViewModelFxSetup
{
	static void AddScriptFunctions()
	{
		GSC::Script::AddFunction("PlayViewmodelFX", []
		{
			if (Game::Scr_GetNumParam() != 2)
			{
				GSC::Script::Scr_Error("PlayViewmodelFX() called with wrong params.\n");
				return;
			}

			const char* fxName = Game::Scr_GetString(0);
			const unsigned int tagName = Game::Scr_GetConstString(1);

			auto* const fx = static_cast<const Game::FxEffectDef*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_FX, fxName));

			if (!fx)
			{
				GSC::Script::Scr_Error(Utils::String::VA("PlayViewmodelFX(): FX '%s' not found", fxName));
				return;
			}

			const int clientNum = 0;
			const Game::DObj* dobj = Game::Com_GetClientDObj(clientNum, 0);

			if (!dobj)
			{
				GSC::Script::Scr_Error("PlayViewmodelFX(): Could not get DObj for local player");
				return;
			}

			unsigned char boneIndex = 0;

			if (!Game::DObjGetBoneIndex(dobj, tagName, &boneIndex))
			{
				GSC::Script::Scr_Error(Utils::String::VA("PlayViewmodelFX(): clientNum '%d' does not have bone '%s'", clientNum, Game::SL_ConvertToString(tagName)));
				return;
			}

			Game::CG_PlayBoltedEffect(0, fx, dobj->entnum, tagName);
		});

		GSC::Script::AddFunction("PVMFX_BOTH", []
		{
			if (Game::Scr_GetNumParam() != 2)
			{
				GSC::Script::Scr_Error("PVMFX_BOTH() called with wrong params. Expected 2.\n");
				return;
			}

			const char* fxName = Game::Scr_GetString(0);
			const unsigned int tagName = Game::Scr_GetConstString(1);

			auto* const fx = static_cast<const Game::FxEffectDef*>(Game::DB_FindXAssetHeader(Game::ASSET_TYPE_FX, fxName));

			if (!fx)
			{
				GSC::Script::Scr_Error(Utils::String::VA("PVMFX_BOTH(): FX '%s' not found", fxName));
				return;
			}

			for (int hand = 0; hand <= 1; ++hand)
			{
				const int fpDObjHandle = Game::CG_WeaponDObjHandle(hand);

				if (fpDObjHandle)
				{
					Game::CG_PlayBoltedEffect(0, fx, fpDObjHandle, tagName);
					Game::CG_StopBoltedEffects(0, nullptr, fpDObjHandle, tagName);
				}
			}

			const int clientNum = 0;
			const Game::DObj* tpDObj = Game::Com_GetClientDObj(clientNum, 0);

			if (tpDObj)
			{
				Game::CG_PlayBoltedEffect(0, fx, tpDObj->entnum, tagName);
				Game::CG_StopBoltedEffects(0, nullptr, tpDObj->entnum, tagName);
			}
		});
	}

	Setup::Setup()
	{
		AddScriptFunctions();
	}
}
