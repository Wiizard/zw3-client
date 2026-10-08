#pragma once

#include "../AssetHandler.hpp"

namespace Assets
{
	class ImenuDef_t : public Components::AssetHandler::IAsset
	{
	public:
		Game::XAssetType GetType() override
		{
			return Game::ASSET_TYPE_MENU;
		}

		void Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Mark(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder) override;
		void Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder) override;

	private:
		void Save_windowDef_t(const Game::windowDef_t* asset, Game::X86::windowDef_t* dest, Components::ZoneBuilder::Zone* builder);
		void Save_ExpressionSupportingData(const Game::ExpressionSupportingData* asset, Components::ZoneBuilder::Zone* builder);
		void Save_Statement_s(const Game::Statement_s* asset, Components::ZoneBuilder::Zone* builder);
		void Save_StatementPtr(const Game::Statement_s* asset, std::uint32_t* dest, Components::ZoneBuilder::Zone* builder);
		void Save_MenuEventHandlerSet(const Game::MenuEventHandlerSet* asset, Components::ZoneBuilder::Zone* builder);
		void Save_MenuEventHandlerSetPtr(const Game::MenuEventHandlerSet* asset, std::uint32_t* dest, Components::ZoneBuilder::Zone* builder);
		void Save_ItemKeyHandler(const Game::ItemKeyHandler* asset, Components::ZoneBuilder::Zone* builder);
		void Save_itemDefData_t(const Game::itemDefData_t* asset, int type, Game::X86::itemDef_s* dest, Components::ZoneBuilder::Zone* builder);
		void Save_itemDef_s(const Game::itemDef_s* asset, Components::ZoneBuilder::Zone* builder);
	};
}
