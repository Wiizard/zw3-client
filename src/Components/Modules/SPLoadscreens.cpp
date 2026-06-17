#include "SPLoadscreens.hpp"
#include "Command.hpp"
#include "AssetHandler.hpp"
#include "FileSystem.hpp"
#include "Materials.hpp"
#include "STDInclude.hpp"

namespace Components {
	void(*SPLoadscreens::OriginalMapCommand)() = nullptr;

	namespace
	{
		std::string LoadingMap;

		void StorePreviewMaterial(const std::string& name, Game::GfxImage* image)
		{
			if (!image || strstr(image->name, "default"))
			{
				return;
			}

			Game::XAssetHeader existing = AssetHandler::FindTemporaryAsset(Game::ASSET_TYPE_MATERIAL, name.data());
			if (existing.material && existing.material->textureCount > 0 && existing.material->textureTable && existing.material->textureTable[0].u.image == image)
			{
				return;
			}

			AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_MATERIAL, { Materials::Create(name, image) });
		}

		Game::GfxImage* GetPreviewImageForMap(const std::string& mapname)
		{
			if (mapname.empty()) return nullptr;

			// If the map name starts with "mp_", we skip it entirely
			if (Utils::String::StartsWith(mapname, "mp_"))
			{
				return nullptr;
			}

			std::vector<std::string> variants = { "preview_" + mapname, "loadscreen_" + mapname };

			// Handle potential underscores
			if (mapname.find('_') != std::string::npos)
			{
				std::string simpleName = mapname;
				Utils::String::Replace(simpleName, "_", "");
				variants.push_back("preview_" + simpleName);
				variants.push_back("loadscreen_" + simpleName);
			}

			for (const auto& variant : variants)
			{
				Game::XAssetHeader imageHeader = Game::DB_FindXAssetHeader(Game::ASSET_TYPE_IMAGE, variant.data());
				if (imageHeader.image && !strstr(imageHeader.image->name, "default"))
				{
					return imageHeader.image;
				}

				if (FileSystem::File(std::format("images/{}.iwi", variant)).exists())
				{
					imageHeader.image = Materials::CreateImage(variant, 0, 0, 0, 0, D3DFMT_UNKNOWN);
					AssetHandler::StoreTemporaryAsset(Game::ASSET_TYPE_IMAGE, imageHeader);

					return imageHeader.image;
				}
			}

			return nullptr;
		}

		std::string GetTargetMapName(const std::string& materialName)
		{
			if (!LoadingMap.empty())
			{
				return LoadingMap;
			}

			if (Utils::String::StartsWith(materialName, "loadscreen_"))
			{
				return materialName.substr(11);
			}

			// Fallback to current map dvar if it's a generic loadscreen material
			Game::dvar_t* ui_mapname = Game::Dvar_FindVar("ui_mapname");
			if (ui_mapname && ui_mapname->current.string)
			{
				return ui_mapname->current.string;
			}

			return {};
		}

		void PatchMaterialWithImage(Game::Material* material, Game::GfxImage* image, bool isElementExplicit)
		{
			if (!material || !image || strstr(image->name, "default"))
			{
				return;
			}

			for (int i = 0; i < material->textureCount; ++i)
			{
				if (material->textureTable[i].u.image)
				{
					bool isDefault = strstr(material->textureTable[i].u.image->name, "default") != nullptr;
					bool isOldPreview = strstr(material->textureTable[i].u.image->name, "preview_") != nullptr &&
						strstr(material->textureTable[i].u.image->name, image->name) == nullptr;
					bool isOldLoadscreen = strstr(material->textureTable[i].u.image->name, "loadscreen_") != nullptr &&
						strstr(material->textureTable[i].u.image->name, image->name) == nullptr;
					if (isDefault || ((isOldPreview || isOldLoadscreen) && isElementExplicit))
					{
						material->textureTable[i].u.image = image;
					}
				}
			}
		}

		void PatchConnectMenu(Game::GfxImage* image)
		{
			Game::menuDef_t* connectMenu = Game::Menus_FindByName(Game::uiContext, "connect");
			if (!connectMenu)
			{
				return;
			}

			PatchMaterialWithImage(connectMenu->window.background, image, true);

			for (int i = 0; i < connectMenu->itemCount; ++i)
			{
				Game::itemDef_s* item = connectMenu->items[i];
				if (!item)
				{
					continue;
				}

				bool isExplicitTarget = false;
				if (item->window.name)
				{
					std::string winName = item->window.name;
					if (Utils::String::Contains(winName, "loadscreen") || Utils::String::Contains(winName, "preview"))
					{
						isExplicitTarget = true;
					}
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

				PatchMaterialWithImage(item->window.background, image, isExplicitTarget);
			}
		}
	}

	void SPLoadscreens::SetLoadingMap(const std::string& mapname)
	{
		if (mapname.empty() || Utils::String::StartsWith(mapname, "mp_"))
		{
			LoadingMap.clear();
			return;
		}

		LoadingMap = mapname;
		if (*Game::ui_mapname)
		{
			Game::Dvar_SetString(*Game::ui_mapname, mapname.data());
		}

		PreloadMapPreview(mapname);
	}

	void SPLoadscreens::PreloadMapPreview(const std::string& mapname)
	{
		Game::GfxImage* image = GetPreviewImageForMap(mapname);
		StorePreviewMaterial("loading_image", image);
		StorePreviewMaterial("level_loadscreen", image);
		StorePreviewMaterial("loadscreen_" + mapname, image);
		PatchConnectMenu(image);
	}

	void SPLoadscreens::InstallMapCommandHook()
	{
		Scheduler::Schedule([]
			{
				auto* mapCommand = Command::Find("map");
				if (!mapCommand || !mapCommand->function)
				{
					return false;
				}

				OriginalMapCommand = mapCommand->function;
				Command::Add("map", [](const Command::Params* params)
					{
						if (params->size() > 1)
						{
							SetLoadingMap(params->get(1));
						}

						if (OriginalMapCommand)
						{
							OriginalMapCommand();
						}
					});

				return true;
			}, Scheduler::Pipeline::MAIN);
	}

	SPLoadscreens::SPLoadscreens()
	{
		InstallMapCommandHook();
		auto getPreviewImage = [](const std::string& materialName) -> Game::GfxImage*
			{
				return GetPreviewImageForMap(GetTargetMapName(materialName));
			};

		auto patchMaterial = [getPreviewImage](Game::Material* material, const std::string& name, bool isElementExplicit)
			{
				if (!material) return;

				Game::GfxImage* image = getPreviewImage(name);
				if (!image && material->info.name)
				{
					image = getPreviewImage(material->info.name);
				}

				PatchMaterialWithImage(material, image, isElementExplicit);
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

		AssetHandler::OnLoad([getPreviewImage](Game::XAssetType type, Game::XAssetHeader asset, const std::string& name, bool*)
			{
				if (type == Game::ASSET_TYPE_MATERIAL && (Utils::String::Contains(name, "loadscreen") || name == "loading_image"))
				{
					PatchMaterialWithImage(asset.material, getPreviewImage(name), true);
				}
			});

		// Active monitoring for the connect menu
		Scheduler::Loop([getPreviewImage]()
			{
				static Game::menuDef_t* lastPatchedMenu = nullptr;
				static Game::GfxImage* lastPatchedImage = nullptr;

				Game::menuDef_t* connectMenu = Game::Menus_FindByName(Game::uiContext, "connect");
				if (connectMenu)
				{
					Game::GfxImage* image = getPreviewImage("level_loadscreen");
					if (image && (connectMenu != lastPatchedMenu || image != lastPatchedImage))
					{
						PatchConnectMenu(image);
						lastPatchedMenu = connectMenu;
						lastPatchedImage = image;
					}
				}
			}, Scheduler::Pipeline::MAIN);
	}

	SPLoadscreens::~SPLoadscreens() {}
} // namespace Components
