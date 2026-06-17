#include "SPLoadscreens.hpp"
#include "AssetHandler.hpp"
#include "FileSystem.hpp"
#include "Materials.hpp"
#include "STDInclude.hpp"

namespace Components {
	std::string SPLoadscreens::LoadingMap;

	void SPLoadscreens::SetLoadingMap(const std::string& mapname)
	{
		LoadingMap = mapname;
	}

	SPLoadscreens::SPLoadscreens()
	{
		auto getPreviewImage = [](const std::string& materialName) -> Game::GfxImage*
			{
				auto findPreviewForMap = [](const std::string& mapname) -> Game::GfxImage*
				{
					if (mapname.empty() || Utils::String::StartsWith(mapname, "mp_"))
					{
						return nullptr;
					}

					std::vector<std::string> variants = { "preview_" + mapname };

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

				if (Utils::String::StartsWith(materialName, "loadscreen_"))
				{
					const auto explicitMap = materialName.substr(11);
					if (auto* image = findPreviewForMap(explicitMap))
					{
						LoadingMap = explicitMap;
						return image;
					}
				}

				const auto* uiMapDvar = Game::Dvar_FindVar("ui_mapname");
				const auto* mapDvar = Game::Dvar_FindVar("mapname");
				const auto* uiMap = (uiMapDvar && uiMapDvar->current.string) ? uiMapDvar->current.string : "";
				const auto* map = (mapDvar && mapDvar->current.string) ? mapDvar->current.string : "";

				// When changing SP maps from the console, ui_mapname can point at the requested
				// map while mapname still points at the currently running map. Prefer that
				// transition target before the last explicit loadscreen material we saw.
				if (*uiMap && _stricmp(uiMap, map))
				{
					if (auto* image = findPreviewForMap(uiMap))
					{
						return image;
					}
				}

				if (!LoadingMap.empty())
				{
					if (auto* image = findPreviewForMap(LoadingMap))
					{
						return image;
					}
				}

				// Generic loading materials are reused between SP maps. ui_mapname tends to be
				// updated for the requested map before the read-only mapname dvar changes.
				for (const auto* dvarName : { "ui_mapname", "mapname", "sv_mapname" })
				{
					Game::dvar_t* dvar = Game::Dvar_FindVar(dvarName);
					if (dvar && dvar->current.string)
					{
						if (auto* image = findPreviewForMap(dvar->current.string))
						{
							return image;
						}
					}
				}

				return nullptr;
			};

		auto patchMaterial = [getPreviewImage](Game::Material* material, const std::string& name, bool isElementExplicit)
			{
				if (!material) return;

				Game::GfxImage* image = getPreviewImage(name);
				if (!image && material->info.name)
				{
					image = getPreviewImage(material->info.name);
				}

				if (image && !strstr(image->name, "default"))
				{
					for (int i = 0; i < material->textureCount; ++i)
					{
						if (material->textureTable[i].u.image)
						{
							const auto* currentName = material->textureTable[i].u.image->name ? material->textureTable[i].u.image->name : "";
							const bool isDefault = strstr(currentName, "default") != nullptr;
							const bool isOldPreview = strstr(currentName, "preview_") != nullptr && strstr(currentName, image->name) == nullptr;

							if (isDefault || (isOldPreview && isElementExplicit))
							{
								material->textureTable[i].u.image = image;
							}
						}
					}
				}
			};

		// Passive hooks for materials
		AssetHandler::OnFind(Game::ASSET_TYPE_MATERIAL, [getPreviewImage](Game::XAssetType type, const std::string& name) -> Game::XAssetHeader
			{
				static_cast<void>(type);

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

		AssetHandler::OnLoad([patchMaterial](Game::XAssetType type, Game::XAssetHeader asset, const std::string& name, bool*)
			{
				if (type == Game::ASSET_TYPE_MATERIAL && (Utils::String::Contains(name, "loadscreen") || name == "loading_image"))
				{
					patchMaterial(asset.material, name, true);
				}
			});

		// Active monitoring for the connect menu
		Scheduler::Loop([patchMaterial]()
			{
				Game::menuDef_t* connectMenu = Game::Menus_FindByName(Game::uiContext, "connect");
				if (connectMenu && Game::Menu_IsVisible(Game::uiContext, connectMenu))
				{
					// Patch the menu's own background
					patchMaterial(connectMenu->window.background, "level_loadscreen", true);

					// Patch items in the connect menu
					for (int i = 0; i < connectMenu->itemCount; ++i)
					{
						Game::itemDef_s* item = connectMenu->items[i];
						if (item)
						{
							bool isExplicitTarget = false;
							if (item->window.name)
							{
								std::string winName = item->window.name;
								isExplicitTarget = Utils::String::Contains(winName, "loadscreen") || Utils::String::Contains(winName, "preview");
							}
							else
							{
								isExplicitTarget = true;
							}

							if (!isExplicitTarget && item->window.background && item->window.background->info.name)
							{
								isExplicitTarget = Utils::String::Contains(item->window.background->info.name, "loadscreen") ||
									Utils::String::Contains(item->window.background->info.name, "preview");
							}

							patchMaterial(item->window.background, item->window.name ? item->window.name : "level_loadscreen", isExplicitTarget);
						}
					}
				}
			}, Scheduler::Pipeline::MAIN);
	}

	SPLoadscreens::~SPLoadscreens() {}
} // namespace Components
