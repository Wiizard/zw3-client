#include "SPLoadscreens.hpp"
#include "AssetHandler.hpp"
#include "FileSystem.hpp"
#include "Materials.hpp"
#include "STDInclude.hpp"

namespace Components {
	SPLoadscreens::SPLoadscreens()
	{
		auto getPreviewImage = [](const std::string& materialName) -> Game::GfxImage*
		{
			std::string mapname = "";
			if (Utils::String::StartsWith(materialName, "loadscreen_"))
			{
				mapname = materialName.substr(11);
			}
			else
			{
				// Fallback to current map dvar if it's a generic loadscreen material
				Game::dvar_t* ui_mapname = Game::Dvar_FindVar("ui_mapname");
				if (ui_mapname && ui_mapname->current.string)
				{
					mapname = ui_mapname->current.string;
				}
			}

			if (mapname.empty()) return nullptr;

			std::vector<std::string> variants = { "preview_" + mapname };
			
			// Handle potential underscores
			if (mapname.find('_') != std::string::npos)
			{
				std::string simpleName = mapname;
				Utils::String::Replace(simpleName, "_", "");
				variants.push_back("preview_" + simpleName);
			}

			for (const auto& variant : variants)
			{
				if (FileSystem::File(std::format("images/{}.iwi", variant)).exists())
				{
					Game::XAssetHeader imageHeader = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_IMAGE, variant.data());
					if (!imageHeader.image || strstr(imageHeader.image->name, "default"))
					{
						imageHeader.image = Materials::CreateImage(variant, 0, 0, 0, 0, D3DFMT_UNKNOWN);
						AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_IMAGE, imageHeader);
					}
					return imageHeader.image;
				}
			}

			return nullptr;
		};

		auto patchMaterial = [getPreviewImage](Game::Material* material, const std::string& name)
		{
			if (!material) return;

			Game::GfxImage* image = getPreviewImage(name);
			if (image && !strstr(image->name, "default"))
			{
				for (int i = 0; i < material->textureCount; ++i)
				{
					if (material->textureTable[i].u.image && strstr(material->textureTable[i].u.image->name, "default"))
					{
						material->textureTable[i].u.image = image;
					}
				}
			}
		};

		// Passive hooks for materials
		AssetHandler::OnFind(Game::ASSET_TYPE_MATERIAL, [getPreviewImage](Game::XAssetType type, const std::string& name) -> Game::XAssetHeader
		{
			if (Utils::String::Contains(name, "loadscreen") || name == "loading_image")
			{
				Game::GfxImage* image = getPreviewImage(name);
				if (image && !strstr(image->name, "default"))
				{
					Game::Material* material = Materials::Create(name, image);
					return { material };
				}
			}

			return { nullptr };
		});

		AssetHandler::OnLoad([getPreviewImage](Game::XAssetType type, Game::XAssetHeader asset, const std::string& name, bool*)
		{
			if (type == Game::ASSET_TYPE_MATERIAL && (Utils::String::Contains(name, "loadscreen") || name == "loading_image"))
			{
				Game::GfxImage* image = getPreviewImage(name);
				if (image && !strstr(image->name, "default"))
				{
					for (int i = 0; i < asset.material->textureCount; ++i)
					{
						if (asset.material->textureTable[i].u.image && strstr(asset.material->textureTable[i].u.image->name, "default"))
						{
							asset.material->textureTable[i].u.image = image;
						}
					}
				}
			}
		});

		// Active monitoring for the connect menu
		Scheduler::Loop([getPreviewImage, patchMaterial]()
		{
			Game::menuDef_t* connectMenu = Game::Menus_FindByName(Game::uiContext, "connect");
			if (connectMenu && Game::Menu_IsVisible(Game::uiContext, connectMenu))
			{
				// Patch the menu's own background
				patchMaterial(connectMenu->window.background, "level_loadscreen");

				// Patch all items in the connect menu
				for (int i = 0; i < connectMenu->itemCount; ++i)
				{
					Game::itemDef_s* item = connectMenu->items[i];
					if (item)
					{
						patchMaterial(item->window.background, item->window.name ? item->window.name : "level_loadscreen");
					}
				}
			}
		}, Scheduler::Pipeline::MAIN);
	}



SPLoadscreens::~SPLoadscreens() {}
} // namespace Components
