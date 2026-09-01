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

					if (Flags::HasFlag("mapProfile"))
					{
						Logger::Print(
							"SP menu preview: {} -> {}.\n",
							material ? material->info.name : "none",
							preview->map);
					}
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
	}

	void SPLoadscreens::OnMenuFreed(Game::menuDef_t* menu)
	{
		RestoreMenus(menu);
	}

	void SPLoadscreens::SetLoadingMap(const std::string& name)
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		const auto map = Utils::MapPreview::Normalize(name);

		D3D9Ex::BeginMapLoading(map);

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

		const auto start = std::chrono::steady_clock::now();

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

		if (Flags::HasFlag("mapProfile"))
		{
			Logger::Print(
				"SP preview: {} -> {} in {:.2f} ms.\n",
				map,
				image ? image->name : "unavailable",
				std::chrono::duration<double, std::milli>(
					std::chrono::steady_clock::now() - start).count());
		}
	}

	SPLoadscreens::SPLoadscreens()
	{
		if (Dedicated::IsEnabled() || ZoneBuilder::IsEnabled()) return;

		Utils::Hook(0x609666, RunMapCommand, HOOK_CALL).install()->quick();

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
				if (!TransitionPending) return;

				const auto state =
					*reinterpret_cast<Game::connstate_t*>(0xB2C540);

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
