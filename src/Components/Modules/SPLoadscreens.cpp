#include "SPLoadscreens.hpp"
#include "Command.hpp"
#include "Events.hpp"
#include "AssetHandler.hpp"
#include "FastFiles.hpp"
#include "Materials.hpp"
#include "Menus.hpp"
#include "D3D9Ex.hpp"
#include <Utils/MapPreview.hpp>

namespace Components
{
	namespace
	{
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

		std::atomic<std::shared_ptr<const Preview>> ActivePreview;
		std::unordered_map<std::string, Game::GfxImage*> PreviewImages;
		std::vector<MenuPatch> MenuPatches;
		std::string LoadingMap;
		bool TransitionPending = false;
		bool SawLoadingState = false;
		unsigned int MapCommandDepth = 0;
		void (*NativeDevmapCommand)() = nullptr;
		std::chrono::steady_clock::time_point MapLoadStart{};
		std::string TimedMap;
		bool MapTimingActive = false;
		bool MapTimingSawLoadingState = false;

		bool IsMainMenuOpen()
		{
			if (!Game::uiContext) return false;

			for (int i = 0; i < Game::uiContext->openMenuCount; ++i)
			{
				const auto* menu = Game::uiContext->menuStack[i];
				if (menu && menu->window.name && Utils::String::Compare(menu->window.name, "main_text"))
				{
					return true;
				}
			}

			return false;
		}

		void RestoreMenus(Game::menuDef_t* only = nullptr)
		{
			std::erase_if(MenuPatches, [only](const MenuPatch& patch)
				{
					if (only && patch.menu != only) return false;

					*patch.slot = patch.original;
					*patch.alpha = patch.originalAlpha;
					return true;
				});
		}

		void ClearPreview()
		{
			ActivePreview.store(nullptr);
			RestoreMenus();
			LoadingMap.clear();
			TransitionPending = SawLoadingState = false;
		}

		bool IsPreviewMaterial(const std::string_view name)
		{
			return name == "$levelbriefing"
				|| name == "level_loadscreen"
				|| name == "loading_image"
				|| name.starts_with("loadscreen_")
				|| name.starts_with("preview_")
				|| name.starts_with("zw3_sp_preview_");
		}

		void PatchConnectMenu()
		{
			const auto preview = ActivePreview.load();
			if (!preview) return;

			auto* menu = Menus::FindDiskMenu("connect");
			if (!menu) return;

			auto patchWindow = [&](Game::windowDef_t& window)
				{
					if (std::ranges::any_of(MenuPatches, [&](const MenuPatch& patch)
						{
							return patch.slot == &window.background;
						}))
					{
						return;
					}

					auto* material = window.background;

					const bool namedPreview = window.name
						&& (strstr(window.name, "loadscreen") || strstr(window.name, "preview"));

					const bool realPreview = material
						&& material->info.name
						&& IsPreviewMaterial(material->info.name);

					if (!namedPreview && !realPreview) return;

					MenuPatches.push_back(
						{
							menu,
							&window.background,
							material,
							&window.foreColor[3],
							window.foreColor[3]
						});

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

		void RunMapCommand()
		{
			Command::ClientParams params;

			++MapCommandDepth;
			const auto guard = gsl::finally([]
				{
					--MapCommandDepth;
				});

			if (params.size() > 1
				&& (Utils::String::Compare(params[0], "map")
					|| Utils::String::Compare(params[0], "devmap")))
			{
				SPLoadscreens::SetLoadingMap(params[1]);
			}

			Utils::Hook::Call<void()>(0x4256F0)();
		}

		void RunDevmapCommand()
		{
			Command::ClientParams params;
			++MapCommandDepth;
			const auto guard = gsl::finally([] { --MapCommandDepth; });
			if (params.size() > 1) SPLoadscreens::SetLoadingMap(params[1]);
			NativeDevmapCommand();
		}
	}

	void SPLoadscreens::OnMenuFreed(Game::menuDef_t* menu)
	{
		RestoreMenus(menu);
	}

	void SPLoadscreens::SetLoadingMap(const std::string& name)
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		const auto map = Utils::MapPreview::Normalize(name);
		if (!map.empty() && (!MapTimingActive || TimedMap != map))
		{
			TimedMap = map;
			MapLoadStart = std::chrono::steady_clock::now();
			MapTimingActive = true;
			MapTimingSawLoadingState = false;
		}

		D3D9Ex::BeginMapLoading(map);
		FastFiles::PrefetchZone(map);
		// The engine loads the lightweight *_load zone before the main map
		// zone.  Start both reads as soon as map/devmap is issued so large
		// maps (notably the MW3 conversions) can warm the file cache in
		// parallel with the disconnect/loading-screen transition.
		if (!map.empty() && !map.ends_with("_load"))
		{
			FastFiles::PrefetchZone(map + "_load");
			FastFiles::PrefetchZone("patch_" + map);
		}

		if (map.empty() || Utils::MapPreview::IsMultiplayer(map))
		{
			ClearPreview();
			return;
		}

		if (TransitionPending && LoadingMap == map && ActivePreview.load())
		{
			PatchConnectMenu();
			Game::Key_RemoveCatcher(0, ~Game::KEYCATCH_CONSOLE);
			Menus::OpenLoadingScreen();
			return;
		}

		ClearPreview();

		LoadingMap = map;
		TransitionPending = true;

		if (*Game::ui_mapname)
		{
			Game::Dvar_SetString(*Game::ui_mapname, map.c_str());
		}

		PreloadMapPreview(map);

		if (ActivePreview.load())
		{
			Game::Key_RemoveCatcher(0, ~Game::KEYCATCH_CONSOLE);
			Menus::OpenLoadingScreen();
		}
	}

	void SPLoadscreens::PreloadMapPreview(const std::string& name)
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		const auto map = Utils::MapPreview::Normalize(name);

		if (map.empty() || Utils::MapPreview::IsMultiplayer(map)) return;

		Game::GfxImage* image = nullptr;

		for (const auto& variant : Utils::MapPreview::ImageNames(map))
		{
			const auto cached = PreviewImages.find(variant);

			if (cached != PreviewImages.end() && cached->second->texture.basemap)
			{
				image = cached->second;
			}
			else
			{
				image = Materials::LoadPreviewImage(variant);

				if (image)
				{
					PreviewImages[variant] = image;
				}
			}

			if (image) break;
		}

		if (image)
		{
			auto* material = Materials::Create("zw3_sp_preview_" + map, image);

			if (material)
			{
				material->textureTable[0].u.image = image;

				ActivePreview.store(
					std::make_shared<const Preview>(
						Preview{ map, image, material }));

				PatchConnectMenu();
			}
		}
	}

	SPLoadscreens::SPLoadscreens()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		Utils::Hook(0x609666, RunMapCommand, HOOK_CALL).install()->quick();
		Scheduler::OnGameInitialized([]
		{
			// Unlike map, devmap can use a direct client handler rather than the
			// server-command dispatch path above. Select its preview before teardown.
			const auto command = Command::Find("devmap");
			if (command && command->function && command->function != Game::Cbuf_AddServerText_f)
			{
				NativeDevmapCommand = command->function;
				command->function = RunDevmapCommand;
			}
		}, Scheduler::Pipeline::MAIN);

		Events::OnCLDisconnected([](bool)
			{
				/*
				 * Preserve the preview during the internal disconnect generated
				 * by map/devmap before the new connection begins.
				 *
				 * Normal disconnects are handled here without replacing the
				 * engine's global "disconnect" command.
				 */
				if (!MapCommandDepth && (!TransitionPending || SawLoadingState))
				{
					ClearPreview();
				}
			});

		Events::AfterUIInit(PatchConnectMenu);

		Renderer::OnDeviceRecoveryBegin(ClearPreview);

		AssetHandler::OnFind(
			Game::ASSET_TYPE_MATERIAL,
			[](Game::XAssetType, const std::string& name) -> Game::XAssetHeader
			{
				if (!name.starts_with("loadscreen_")
					&& !name.starts_with("preview_"))
				{
					return { nullptr };
				}

				const auto preview = ActivePreview.load();

				if (!preview
					|| !Utils::MapPreview::MatchesMaterial(name, preview->map))
				{
					return { nullptr };
				}

				return { preview->material };
			});

		Scheduler::Loop([]
			{
			if (IsMainMenuOpen())
			{
				FastFiles::MarkMainMenuReady();
			}

				const auto state =
					*reinterpret_cast<Game::connstate_t*>(0xB2C540);

				// This is intentionally independent of the SP load-screen state:
				// multiplayer map loads clear the custom preview immediately, but
				// still need the same end-to-end timing measurement.
				if (MapTimingActive)
				{
					if (state >= Game::CA_CONNECTING && state < Game::CA_ACTIVE)
					{
						MapTimingSawLoadingState = true;
					}

					if (MapTimingSawLoadingState && state == Game::CA_ACTIVE)
					{
						const auto message = Utils::String::Format(
							"Map timing: {} reached active in {:.2f} ms.\n",
							TimedMap,
							std::chrono::duration<double, std::milli>(
								std::chrono::steady_clock::now() - MapLoadStart).count());
						Logger::Print(Game::CON_CHANNEL_SYSTEM, "{}", message);
						Utils::IO::WriteFile("zw3/logs/load_timings.log", message, true);
						MapTimingActive = false;
					}
					else if (MapTimingSawLoadingState && state == Game::CA_DISCONNECTED)
					{
						Logger::Print(Game::CON_CHANNEL_SYSTEM,
							"Map timing: {} load aborted before active.\n", TimedMap);
						MapTimingActive = false;
					}
				}

				if (!TransitionPending) return;

				if (state >= Game::CA_CONNECTING && state < Game::CA_ACTIVE)
				{
					SawLoadingState = true;
				}

				if (!MapCommandDepth
					&& SawLoadingState
					&& (state == Game::CA_ACTIVE
						|| state == Game::CA_DISCONNECTED))
				{
					ClearPreview();
					return;
				}

				PatchConnectMenu();
			}, Scheduler::Pipeline::MAIN);
	}

	void SPLoadscreens::preDestroy()
	{
		ClearPreview();
	}

	SPLoadscreens::~SPLoadscreens() = default;
}
