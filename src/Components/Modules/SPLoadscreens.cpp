#include "STDInclude.hpp"

#include "SPLoadscreens.hpp"
#include "AssetHandler.hpp"
#include "Command.hpp"
#include "D3D9Ex.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Events.hpp"
#include "FastFiles.hpp"
#include "LobbyScene.hpp"
#include "Logger.hpp"
#include "Maps.hpp"
#include "Materials.hpp"
#include "Menus.hpp"
#include "Renderer.hpp"
#include "Scheduler.hpp"

#include "Utils/MapPreview.hpp"

namespace Components
{
	constexpr std::uintptr_t Cmd_ExecuteSingleCommand_WaitServerCall = 0x1401E78B8;
	constexpr std::uintptr_t SV_WaitServer = 0x14023D9E0;

	struct Preview
	{
		std::string map;
		Game::GfxImage* image;
		Game::Material* material;
	};

	struct MenuPatch
	{
		Game::menuDef_t* menu;
		Game::Material** slot;
		Game::Material* original;
		float* alpha;
		float originalAlpha;
	};

	static std::atomic<std::shared_ptr<const Preview>> activePreview;

	static std::unordered_map<std::string, Game::GfxImage*> previewImages;
	static std::vector<MenuPatch> menuPatches;
	static std::string loadingMap;
	static bool isTransitionPending = false;
	static bool didSeeLoadingState = false;
	static unsigned int mapCommandDepth = 0;

	static std::atomic_bool shouldClearPreview = false;

	static Utils::Hook waitServerHook;

	static bool IsMainMenuOpen()
	{
		const auto* const context = Game::uiContext;
		const int openCount = std::min(context->openMenuCount, static_cast<int>(std::size(context->menuStack)));

		for (int i = 0; i < openCount; ++i)
		{
			const auto* const menu = context->menuStack[i];

			if (menu && menu->window.name && Utils::String::Compare(menu->window.name, "main_text"))
			{
				return true;
			}
		}

		return false;
	}

	static void RestoreMenus()
	{
		for (const MenuPatch& patch : menuPatches)
		{
			*patch.slot = patch.original;
			*patch.alpha = patch.originalAlpha;
		}

		menuPatches.clear();
	}

	static void ClearPreview()
	{
		shouldClearPreview = false;
		activePreview.store(nullptr);
		RestoreMenus();
		loadingMap.clear();
		isTransitionPending = false;
		didSeeLoadingState = false;
	}

	static bool IsPreviewMaterial(const std::string_view name)
	{
		if (name == "$levelbriefing" || name == "level_loadscreen" || name == "loading_image")
		{
			return true;
		}

		return name.starts_with("loadscreen_") || name.starts_with("preview_") || name.starts_with("zw3_sp_preview_");
	}

	static void RunMapCommand()
	{
		const Command::ClientParams params;

		++mapCommandDepth;

		const bool isMapCommand = Utils::String::Compare(params.Get(0), "map") || Utils::String::Compare(params.Get(0), "devmap");

		if (params.Size() > 1 && isMapCommand && Game::IsMapOnDisk(params.Get(1)))
		{
			SPLoadscreens::SetLoadingMap(params.Get(1));
		}

		reinterpret_cast<void(*)()>(waitServerHook.GetOriginal())();

		--mapCommandDepth;
	}

	void SPLoadscreens::OnMenusFreed()
	{
		RestoreMenus();
	}

	void SPLoadscreens::PatchConnectMenu()
	{
		const std::shared_ptr<const Preview> preview = activePreview.load();

		if (!preview)
		{
			return;
		}

		Game::menuDef_t* const menu = Menus::FindDiskMenu("connect");

		if (!menu)
		{
			return;
		}

		const auto patchWindow = [&menu, &preview](Game::windowDef_t& window)
		{
			const bool isPatched = std::ranges::any_of(menuPatches, [&window](const MenuPatch& patch)
			{
				return patch.slot == &window.background;
			});

			if (isPatched)
			{
				return;
			}

			Game::Material* const material = window.background;

			const bool isNamedPreview = window.name && (std::strstr(window.name, "loadscreen") || std::strstr(window.name, "preview"));
			const bool isPreviewMaterial = material && material->info.name && IsPreviewMaterial(material->info.name);

			if (!isNamedPreview && !isPreviewMaterial)
			{
				return;
			}

			menuPatches.push_back({ menu, &window.background, material, &window.foreColor[3], window.foreColor[3] });

			window.background = preview->material;
			window.foreColor[3] = 1.0f;
		};

		patchWindow(menu->window);

		for (int i = 0; i < menu->itemCount; ++i)
		{
			if (menu->items[i])
			{
				patchWindow(menu->items[i]->window);
			}
		}
	}

	void SPLoadscreens::SetLoadingMap(const std::string& name)
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		Maps::SynchronizeMapDvars(name);

		const std::string map = Utils::MapPreview::Normalize(name);

		D3D9Ex::BeginMapLoading(map);

		const bool isLoadZone = map.ends_with("_load");

		if (!map.empty() && !isLoadZone)
		{
			FastFiles::PrefetchZone(map + "_load");
		}

		FastFiles::PrefetchZone(map);

		if (!map.empty() && !isLoadZone)
		{
			FastFiles::PrefetchZone("patch_" + map);
		}

		FastFiles::PrefetchPath(std::filesystem::path("main") / "video" / (map + "_load.bik"));

		if (LobbyScene::IsTransitionActive())
		{
			ClearPreview();
			loadingMap = map;
			isTransitionPending = false;

			const Dvar::Var ui_mapname("ui_mapname");
			ui_mapname.Set(map);

			LobbyScene::StartTransition();
			return;
		}

		if (map.empty() || Utils::MapPreview::IsMultiplayer(map))
		{
			ClearPreview();
			return;
		}

		if (isTransitionPending && loadingMap == map && activePreview.load())
		{
			PatchConnectMenu();
			Game::Key_RemoveCatcher(0, ~Game::KEYCATCH_CONSOLE);
			Menus::OpenLoadingScreen();
			return;
		}

		ClearPreview();

		loadingMap = map;
		isTransitionPending = true;

		const Dvar::Var ui_mapname("ui_mapname");
		ui_mapname.Set(map);

		PreloadMapPreview(map);

		if (activePreview.load())
		{
			Game::Key_RemoveCatcher(0, ~Game::KEYCATCH_CONSOLE);
			Menus::OpenLoadingScreen();
		}
	}

	void SPLoadscreens::PreloadMapPreview(const std::string& name)
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		const std::string map = Utils::MapPreview::Normalize(name);

		if (map.empty() || Utils::MapPreview::IsMultiplayer(map))
		{
			return;
		}

		Game::GfxImage* image = nullptr;

		for (const std::string& variant : Utils::MapPreview::ImageNames(map))
		{
			const auto cached = previewImages.find(variant);

			if (cached != previewImages.end() && cached->second->texture.basemap)
			{
				image = cached->second;
			}
			else
			{
				image = Materials::LoadPreviewImage(variant);

				if (image)
				{
					previewImages[variant] = image;
				}
			}

			if (image)
			{
				break;
			}
		}

		if (!image)
		{
			return;
		}

		Game::Material* const material = Materials::Create("zw3_sp_preview_" + map, image);

		if (!material)
		{
			return;
		}

		material->textureTable[0].u.image = image;

		activePreview.store(std::make_shared<const Preview>(Preview{ map, image, material }));

		PatchConnectMenu();
	}

	SPLoadscreens::SPLoadscreens()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		const bool canSeatMapHook = Utils::Hook::BranchesTo(Cmd_ExecuteSingleCommand_WaitServerCall, SV_WaitServer, HOOK_CALL);

		if (canSeatMapHook && waitServerHook.Initialize(Cmd_ExecuteSingleCommand_WaitServerCall, RunMapCommand, HOOK_CALL)->Install()->IsInstalled())
		{
			waitServerHook.Quick();
		}
		else
		{
			Logger::Error("sploadscreens: could not hook the map command, a preview only shows once the map's _load zone loads\n");
		}

		Events::OnCLDisconnected([](bool)
		{
			if (!mapCommandDepth && (!isTransitionPending || didSeeLoadingState))
			{
				ClearPreview();
			}
		});

		Renderer::OnDeviceRecoveryBegin([]
		{
			if (Game::Sys_IsMainThread())
			{
				ClearPreview();
				return;
			}

			activePreview.store(nullptr);
			shouldClearPreview = true;
		});

		const bool isFindAnswered = AssetHandler::OnFind(Game::ASSET_TYPE_MATERIAL, [](unsigned int, const std::string& name) -> void*
		{
			if (!name.starts_with("loadscreen_") && !name.starts_with("preview_"))
			{
				return nullptr;
			}

			const std::shared_ptr<const Preview> preview = activePreview.load();

			if (!preview || !Utils::MapPreview::MatchesMaterial(name, preview->map))
			{
				return nullptr;
			}

			return preview->material;
		});

		if (!isFindAnswered)
		{
			Logger::Error("sploadscreens: a menu's preview_ or loadscreen_ material keeps its stock image\n");
		}

		Scheduler::Loop([]
		{
			if (IsMainMenuOpen() || Game::CL_GetLocalClientConnectionState(0) == Game::CA_ACTIVE)
			{
				FastFiles::MarkMainMenuReady();
			}

			if (shouldClearPreview.exchange(false))
			{
				ClearPreview();
			}

			if (!isTransitionPending)
			{
				return;
			}

			const Game::connstate_t state = Game::CL_GetLocalClientConnectionState(0);

			if (state >= Game::CA_CONNECTING && state < Game::CA_ACTIVE)
			{
				didSeeLoadingState = true;
			}

			if (!mapCommandDepth && didSeeLoadingState && (state == Game::CA_ACTIVE || state == Game::CA_DISCONNECTED))
			{
				ClearPreview();
				return;
			}

			if (!mapCommandDepth && !didSeeLoadingState && state == Game::CA_ACTIVE)
			{
				ClearPreview();
				Menus::CloseLoadingScreen();
				return;
			}

			PatchConnectMenu();
		}, Scheduler::Pipeline::MAIN);

		Scheduler::OnShutdown(ClearPreview);
	}
}
