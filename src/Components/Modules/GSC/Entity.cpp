#include "STDInclude.hpp"

#include "Entity.hpp"
#include "Script.hpp"

namespace Components::GSC
{
	constexpr std::uintptr_t cm_numSubModels = 0x141BD3280;

	constexpr std::uintptr_t Scr_GetConstLowercaseString = 0x140229BD0;
	constexpr std::uintptr_t SV_DObjExists = 0x140233090;
	constexpr std::uintptr_t G_DObjGetWorldTagMatrix = 0x1401AA720;

	constexpr char modelTypeBrush = 4;

	void Entity::AddScriptMethods()
	{
		Script::AddMethod("SetBrushModel", [](const Game::scr_entref_t entref)
		{
			if (Game::Scr_GetNumParam() != 1)
			{
				Script::Scr_Error("usage: <entity> SetBrushModel( <index> )\n");
				return;
			}

			auto* const ent = Game::GetEntity(entref);
			const int index = Game::Scr_GetInt(0);
			const auto subModelCount = Utils::Hook::Get<unsigned int>(cm_numSubModels);

			if (index < 0 || static_cast<unsigned int>(index) >= subModelCount)
			{
				Script::Scr_ParamError(0, "brush model index out of range");
				return;
			}

			Game::SV_UnlinkEntity(ent);
			ent->s.index = index;
			ent->r.modelType = modelTypeBrush;

			Game::SV_SetBrushModel(ent);
			Game::SV_LinkEntity(ent);
		});

		Script::AddMethod("TagExists", [](const Game::scr_entref_t entref)
		{
			if (Game::Scr_GetNumParam() != 1)
			{
				Script::Scr_Error("usage: <entity> TagExists( <tag name> )\n");
				return;
			}

			auto* const ent = Game::GetEntity(entref);
			const auto tagName = Utils::Hook::Call<unsigned int(unsigned int)>(Scr_GetConstLowercaseString)(0);

			if (!Utils::Hook::Call<int(Game::gentity_s*)>(SV_DObjExists)(ent))
			{
				Game::Scr_AddBool(false);
				return;
			}

			float tagMatrix[4][3]{};
			const int hasTag = Utils::Hook::Call<int(Game::gentity_s*, unsigned int, float(*)[3])>(G_DObjGetWorldTagMatrix)(ent, tagName, tagMatrix);

			Game::Scr_AddBool(hasTag != 0);
		});
	}

	Entity::Entity()
	{
		AddScriptMethods();
	}
}
