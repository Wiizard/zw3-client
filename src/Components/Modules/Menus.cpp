#include "STDInclude.hpp"

#include "Menus.hpp"
#include "MenuDvarList.hpp"
#include "Command.hpp"
#include "Scheduler.hpp"
#include "FileSystem.hpp"
#include "Flags.hpp"
#include "Logger.hpp"
#include "AssetHandler.hpp"
#include "Party.hpp"
#include "FastFiles.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Materials.hpp"
#include "TextRenderer.hpp"
#include "UIScript.hpp"
#include "SPLoadscreens.hpp"
#include "LobbyScene.hpp"
#include "Renderer.hpp"

#include "Utils/MenuPreprocessor.hpp"

namespace Components
{
	Utils::Memory::Allocator Menus::allocator;
	Game::ExpressionSupportingData Menus::supportingData;
	std::unordered_map<std::string, Game::menuDef_t*> Menus::loaded;
	std::unordered_map<std::string, Game::menuDef_t*> Menus::overridden;
	std::vector<std::string> Menus::custom;
	std::vector<std::string> Menus::deferred;
	struct BorderStyle
	{
		float size;
		float alpha;
		float appliedAlpha;
	};
	static std::unordered_map<Game::windowDef_t*, BorderStyle> requestedBorderSizes;
	static std::unordered_map<std::string, std::string> menuReadCache;
	static bool isCachingMenuReads = false;
	struct MenuReadBatch
	{
		MenuReadBatch() { menuReadCache.clear(); isCachingMenuReads = true; }
		~MenuReadBatch() { isCachingMenuReads = false; menuReadCache.clear(); }
	};
	bool Menus::isIngameLoaded = false;
	Utils::Hook Menus::uiInitHook;
	Utils::Hook Menus::cgameInitHook;
	Utils::Hook Menus::menusOpenHooks[6];
	Utils::Hook Menus::findForOpenHook;
	Utils::Hook Menus::paintVisibleHook;
	Utils::Hook Menus::levelshotOpenHook;
	Utils::Hook Menus::closeAllHooks[17];
	Utils::Hook Menus::closeRequestHooks[6];
	Utils::Hook Menus::responseHooks[3];

	constexpr std::uintptr_t UI_InitCall = 0x140101F26;

	constexpr std::uintptr_t CL_InitCGameTailCall = 0x1400F4D7D;

	constexpr std::uintptr_t UI_DrawMapLevelshot_MenusOpenCall = 0x14026C72B;

	constexpr std::uintptr_t Menu_Paint_IsVisibleCall = 0x140265DD3;
	constexpr std::uintptr_t Menu_IsVisible = 0x140265BB0;

	constexpr std::uintptr_t Com_InitHunkMemory_ReserveSize = 0x14027F335;
	constexpr std::uintptr_t Com_InitHunkMemory_TotalSize = 0x14027F340;
	static const std::uint8_t hunkSizes[] = { 0xBA, 0x00, 0x00, 0xA0, 0x00, 0x48, 0xC7, 0x05, 0xEC, 0x64, 0x3E, 0x06, 0x00, 0x00, 0xA0, 0x00 };
	constexpr std::uint32_t hunkSize = 0x10000000;

	static const std::uint8_t menusOpenCall[] = { 0xE8, 0x20, 0xB3, 0xFF, 0xFF };

	constexpr std::uintptr_t DB_DynamicCloneXAssetHandler = 0x140422020;
	constexpr std::uintptr_t DB_DynamicCloneMenu = 0x14012C620;

	constexpr std::uintptr_t UI_AddMenuList_FindCall = 0x14026956F;
	constexpr std::uintptr_t DB_FindXAssetHeader = 0x14012D6D0;

	static const std::unordered_set<std::string_view> hudMenuNames =
	{
		"scorebar_hd", "scorebar_sd", "weaponbar_hd", "weaponbar_sd",
		"xpbar_hd", "xpbar_sd", "perks_info_hd", "perks_info_sd",
		"dpad_hd", "dpad_sd", "scoreboard", "minimap_fullscreen",
		"hud_fullscreen",
	};

	static bool IsHudMenu(const char* name)
	{
		std::string lower = name;

		std::ranges::transform(lower, lower.begin(), [](const unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});

		return hudMenuNames.contains(lower);
	}

	constexpr std::string_view pcOptionsCompatibility = R"(
#ifndef PC_OPTIONS_NAV
#define PC_OPTIONS_NAV
#endif
#ifndef PC_OPTIONS_SELECTION_BAR
#define PC_OPTIONS_SELECTION_BAR
#endif
#ifndef PC_OPTIONS_BUTTON_BACK
#define PC_OPTIONS_BUTTON_BACK(actionArg) PC_OPTIONS_BACK_TO(actionArg)
#endif
)";

	constexpr std::string_view iw4xOptionsMarker = "pc_options_button_back_is_defined";

	static bool IsIw4xOptionsInclude(const Utils::MenuPreprocessor::FileReader& reader)
	{
		const std::string probeText = std::format("#include \"ui_mp/pc_options.inc\"\n#ifdef PC_OPTIONS_BUTTON_BACK\n{}\n#endif\n", iw4xOptionsMarker);

		Utils::MenuPreprocessor probe(reader);
		std::string probed;

		probe.ProcessText("pc_options_probe", probeText, &probed);

		return probed.find(iw4xOptionsMarker) != std::string::npos;
	}

	static void DefinePcOptionsCompatibility(Utils::MenuPreprocessor& preprocessor, const std::string& path,
		const Utils::MenuPreprocessor::FileReader& reader)
	{
		const std::string fileName = Utils::String::ToLower(std::filesystem::path(path).filename().string());

		if (!fileName.starts_with("pc_options_") || !fileName.ends_with(".menu"))
		{
			return;
		}

		if (IsIw4xOptionsInclude(reader))
		{
			return;
		}

		std::string discarded;
		preprocessor.ProcessText("pc_options_compatibility", std::string(pcOptionsCompatibility), &discarded);
	}

	static Dvar::Var zw3_ui_loading_start_time;
	static Dvar::Var zw3_ui_loading_progress;
	static Dvar::Var zw3_ui_loading_visible;
	static Dvar::Var mapname;

	static std::mutex loadingMutex;

	static float EaseOutCubic(const float value)
	{
		const float inverse = 1.0f - std::clamp(value, 0.0f, 1.0f);
		return 1.0f - (inverse * inverse * inverse);
	}

	static float EaseOutQuad(const float value)
	{
		const float inverse = 1.0f - std::clamp(value, 0.0f, 1.0f);
		return 1.0f - (inverse * inverse);
	}

	static void RemoveMenuNameFromContext(Game::UiContext* context, const char* name, const Game::menuDef_t* keepMenu)
	{
		const auto isOtherNamed = [name, keepMenu](const Game::menuDef_t* menu)
		{
			return menu && menu != keepMenu && menu->window.name && std::strcmp(menu->window.name, name) == 0;
		};

		Game::menuDef_t** const linkedEnd = std::remove_if(context->Menus, context->Menus + context->menuCount, isOtherNamed);
		std::fill(linkedEnd, context->Menus + context->menuCount, nullptr);
		context->menuCount = static_cast<int>(linkedEnd - context->Menus);

		Game::menuDef_t** const openEnd = std::remove_if(context->menuStack, context->menuStack + context->openMenuCount, isOtherNamed);
		std::fill(openEnd, context->menuStack + context->openMenuCount, nullptr);
		context->openMenuCount = static_cast<int>(openEnd - context->menuStack);
	}

	struct NewsItem
	{
		std::string title;
		std::string body;
		std::string actionType;
		std::string actionTarget;
		std::vector<std::string> actionCommands;
		std::string imageUrl;
		std::string imageCachePath;
		std::string materialName;
		Game::Material* material = nullptr;
		int durationMs = 3000;
	};

	static Dvar::Var zw3_ui_news_index;
	static Dvar::Var zw3_ui_news_count;
	static Dvar::Var zw3_ui_news_progress;
	static Dvar::Var zw3_ui_news_hover;
	static Dvar::Var zw3_ui_news_title;
	static Dvar::Var zw3_ui_news_body;
	static Dvar::Var zw3_ui_news_counter;
	static Dvar::Var zw3_ui_news_image;
	static Dvar::Var zw3_ui_news_has_image;
	static Dvar::Var zw3_ui_news_loading;
	static Dvar::Var zw3_ui_news_page;

	static const char* const newsTileTitleNames[] =
	{
		"zw3_ui_news_tile_title0",
		"zw3_ui_news_tile_title1",
		"zw3_ui_news_tile_title2",
		"zw3_ui_news_tile_title3",
		"zw3_ui_news_tile_title4",
	};

	static Dvar::Var newsTileTitles[std::size(newsTileTitleNames)];

	constexpr int newsPageSize = static_cast<int>(std::size(newsTileTitleNames));

	static std::vector<NewsItem> newsItems;
	static int newsElapsedMs = 0;
	static int lastNewsUpdate = 0;
	static int holdNewsUntil = 0;
	static bool wasNewsHovered = false;
	static std::atomic_bool isNewsFetching = false;

	static const std::unordered_set<std::string_view> newsCommands =
	{
		"openlink", "openmenu", "closemenu",
		"xrequirelivesignin", "xstartprivateparty", "xstartprivatematch",
		"xcheckezpatch", "ui_enumeratesaved",
	};

	static const std::unordered_set<std::string_view> newsDvars =
	{
		"systemlink", "splitscreen", "onlinegame",
		"party_maxplayers", "party_maxprivatepartyplayers",
		"xblive_privateserver", "xblive_rankedmatch",
		"ui_mptype", "ui_mapname", "party_mapname",
	};

	static bool IsNewsCommandAllowed(const std::string& command)
	{
		if (command.find_first_of(";\r\n") != std::string::npos)
		{
			return false;
		}

		std::istringstream words(Utils::String::ToLower(command));
		std::string name;
		std::string argument;
		std::string rest;
		words >> name >> argument >> rest;

		if (name == "set")
		{
			return newsDvars.contains(argument);
		}

		if (name == "exec")
		{
			return argument == "default_xboxlive.cfg" && rest.empty();
		}

		return newsCommands.contains(name);
	}

	static std::string HashNewsString(const std::string& input)
	{
		std::uint64_t hash = 14695981039346656037ull;

		for (const char character : input)
		{
			hash ^= static_cast<unsigned char>(character);
			hash *= 1099511628211ull;
		}

		return std::format("{:016X}", hash);
	}

	static std::filesystem::path GetNewsImageCacheDir()
	{
		std::filesystem::path basePath = Utils::GetBaseFilesLocation();

		if (basePath.empty())
		{
			basePath = std::filesystem::current_path();
		}

		return basePath / "zw3" / "data" / "cache" / "news";
	}

	static std::string CacheNewsImage(const std::string& url)
	{
		if (url.empty())
		{
			return "";
		}

		std::error_code error;
		std::filesystem::create_directories(GetNewsImageCacheDir(), error);

		const std::string cachePath = (GetNewsImageCacheDir() / std::format("{}.iwi", HashNewsString(url))).string();

		bool isDownloaded = false;
		const std::string imageData = Utils::WebIO("zw3-news").SetTimeout(5000)->Get(url, &isDownloaded);

		if (isDownloaded && !imageData.empty() && imageData.size() <= 2 * 1024 * 1024)
		{
			const std::string iwiData = Materials::ConvertNewsImageBytesToIwi(imageData);

			if (!iwiData.empty())
			{
				Utils::IO::WriteFile(cachePath, iwiData);
				return cachePath;
			}
		}

		if (Utils::IO::FileExists(cachePath))
		{
			return cachePath;
		}

		return "";
	}

	static std::string CreateNewsImageMaterial(const NewsItem& item)
	{
		if (item.imageUrl.empty() || item.imageCachePath.empty())
		{
			return "";
		}

		const std::string iwiData = Utils::IO::ReadFile(item.imageCachePath);

		if (iwiData.empty())
		{
			return "";
		}

		const std::string materialName = std::format("zw3_news_{}", HashNewsString(item.imageUrl + "|" + HashNewsString(iwiData)));
		Game::Material* const material = Materials::CreateNewsMaterialFromIwiBytes(materialName, iwiData);

		if (!material || !Materials::IsValid(material))
		{
			return "";
		}

		return materialName;
	}

	static void ForEachNewsMenu(const auto& apply)
	{
		Game::UiContext* const contexts[] = { Game::uiContext, Game::cgDC };

		for (Game::UiContext* const context : contexts)
		{
			if (!context)
			{
				continue;
			}

			const int linkedCount = std::clamp(context->menuCount, 0, static_cast<int>(ARRAYSIZE(context->Menus)));
			const int openCount = std::clamp(context->openMenuCount, 0, static_cast<int>(ARRAYSIZE(context->menuStack)));

			for (int index = 0; index < linkedCount; ++index)
			{
				Game::menuDef_t* const menu = context->Menus[index];

				if (menu && menu->window.name && _stricmp(menu->window.name, "pregame_loaderror") == 0)
				{
					apply(menu);
				}
			}

			for (int index = 0; index < openCount; ++index)
			{
				Game::menuDef_t* const menu = context->menuStack[index];

				if (menu && menu->window.name && _stricmp(menu->window.name, "pregame_loaderror") == 0)
				{
					apply(menu);
				}
			}
		}
	}

	static void ApplyNewsImageMaterialToMenu(Game::Material* material)
	{
		ForEachNewsMenu([material](Game::menuDef_t* menu)
		{
			if (!menu->items)
			{
				return;
			}

			for (int index = 0; index < menu->itemCount; ++index)
			{
				Game::itemDef_s* const item = menu->items[index];

				if (!item || !item->window.name)
				{
					continue;
				}

				if (_stricmp(item->window.name, "news_featured_image") == 0 || _stricmp(item->window.name, "news_image") == 0)
				{
					item->window.background = material;
				}
			}
		});
	}

	static void ApplyNewsImageMaterialsToMenu()
	{
		const int page = zw3_ui_news_page.Get<int>();

		ForEachNewsMenu([page](Game::menuDef_t* menu)
		{
			if (!menu->items)
			{
				return;
			}

			for (int index = 0; index < menu->itemCount; ++index)
			{
				Game::itemDef_s* const item = menu->items[index];

				if (!item || !item->window.name || std::strncmp(item->window.name, "news_thumb_", 11) != 0)
				{
					continue;
				}

				item->window.background = nullptr;

				const int newsIndex = page + std::atoi(item->window.name + 11);

				if (newsIndex < 0 || newsIndex >= static_cast<int>(newsItems.size()))
				{
					continue;
				}

				Game::Material* const material = newsItems[newsIndex].material;

				if (material && Materials::IsValid(material))
				{
					item->window.background = material;
				}
			}
		});
	}

	static void ApplyNewsTileTitles()
	{
		const int page = zw3_ui_news_page.Get<int>();

		for (int slot = 0; slot < newsPageSize; ++slot)
		{
			const int index = page + slot;
			std::string title;

			if (index >= 0 && index < static_cast<int>(newsItems.size()))
			{
				title = newsItems[index].title;
			}

			if (title.length() > 11)
			{
				title = title.substr(0, 10) + ".";
			}

			newsTileTitles[slot].Set(title);
		}
	}

	static void ClearNews()
	{
		newsItems.clear();

		zw3_ui_news_index.Set(0);
		zw3_ui_news_page.Set(0);
		zw3_ui_news_count.Set(0);
		zw3_ui_news_progress.Set(0.0f);
		zw3_ui_news_hover.Set(false);
		zw3_ui_news_title.Set("");
		zw3_ui_news_body.Set("");
		zw3_ui_news_counter.Set("0 / 0");
		zw3_ui_news_image.Set("");
		zw3_ui_news_has_image.Set(false);
		zw3_ui_news_loading.Set(false);

		ApplyNewsTileTitles();
		ApplyNewsImageMaterialToMenu(nullptr);
		ApplyNewsImageMaterialsToMenu();

		newsElapsedMs = 0;
		lastNewsUpdate = 0;
		holdNewsUntil = 0;
		wasNewsHovered = false;
	}

	static void ApplyNewsItem()
	{
		if (newsItems.empty())
		{
			ClearNews();
			return;
		}

		const int count = static_cast<int>(newsItems.size());
		int index = zw3_ui_news_index.Get<int>();

		if (index < 0 || index >= count)
		{
			index = 0;
			zw3_ui_news_index.Set(index);
		}

		const int page = (index / newsPageSize) * newsPageSize;

		if (zw3_ui_news_page.Get<int>() != page)
		{
			zw3_ui_news_page.Set(page);
		}

		const NewsItem& item = newsItems[index];
		const bool hasValidImage = item.material && Materials::IsValid(item.material);
		Game::Material* shownMaterial = nullptr;
		std::string shownMaterialName;

		if (hasValidImage)
		{
			shownMaterial = item.material;
			shownMaterialName = item.materialName;
		}

		zw3_ui_news_title.Set(item.title);
		zw3_ui_news_body.Set(item.body);
		zw3_ui_news_image.Set(shownMaterialName);
		zw3_ui_news_has_image.Set(hasValidImage);
		zw3_ui_news_count.Set(count);
		zw3_ui_news_counter.Set(std::format("{} / {}", index + 1, count));

		ApplyNewsTileTitles();
		ApplyNewsImageMaterialToMenu(shownMaterial);
		ApplyNewsImageMaterialsToMenu();
	}

	static void SelectNewsSlot(const int slot)
	{
		if (newsItems.empty())
		{
			return;
		}

		const int index = zw3_ui_news_page.Get<int>() + slot;

		if (index < 0 || index >= static_cast<int>(newsItems.size()))
		{
			return;
		}

		zw3_ui_news_index.Set(index);
		zw3_ui_news_progress.Set(0.0f);
		zw3_ui_news_hover.Set(false);

		newsElapsedMs = 0;
		holdNewsUntil = Game::Sys_Milliseconds() + 1200;
		wasNewsHovered = false;

		ApplyNewsItem();
	}

	static void ShowNewsPage(const int page)
	{
		zw3_ui_news_page.Set(page);
		zw3_ui_news_index.Set(page);
		zw3_ui_news_progress.Set(0.0f);

		ApplyNewsImageMaterialsToMenu();
		ApplyNewsItem();
	}

	static void FetchNews()
	{
		std::vector<NewsItem> fetchedItems;

		try
		{
			const std::string url = std::format("https://stats.zw3.eu/client/news.json?t={}", Game::Sys_Milliseconds());
			const std::string response = Utils::WebIO("zw3-news").SetTimeout(5000)->Get(url);

			if (!response.empty())
			{
				const nlohmann::json json = nlohmann::json::parse(response);

				if (json.contains("items") && json.at("items").is_array())
				{
					for (const nlohmann::json& entry : json.at("items"))
					{
						NewsItem item;

						item.title = TextRenderer::StripMaterialTextIcons(entry.value("title", ""));
						item.body = TextRenderer::StripMaterialTextIcons(entry.value("body", ""));
						item.imageUrl = entry.value("image", entry.value("imageUrl", ""));
						item.durationMs = std::clamp(entry.value("duration", 3000), 1500, 15000);

						if (entry.contains("action") && entry.at("action").is_object())
						{
							const nlohmann::json& action = entry.at("action");

							item.actionType = action.value("type", "");
							item.actionTarget = action.value("target", "");

							if (action.contains("commands") && action.at("commands").is_array())
							{
								for (const nlohmann::json& command : action.at("commands"))
								{
									if (command.is_string())
									{
										item.actionCommands.push_back(command.get<std::string>());
									}
								}
							}
						}
						else
						{
							item.actionType = entry.value("actionType", entry.value("action", ""));
							item.actionTarget = entry.value("actionTarget", entry.value("url", ""));
						}

						if (!item.title.empty() && !item.body.empty())
						{
							item.imageCachePath = CacheNewsImage(item.imageUrl);
							fetchedItems.push_back(item);
						}
					}
				}
			}
		}
		catch (...)
		{
			fetchedItems.clear();
		}

		Scheduler::Once([items = std::move(fetchedItems)]() mutable
		{
			if (items.empty())
			{
				ClearNews();
				isNewsFetching.store(false);
				return;
			}

			for (NewsItem& item : items)
			{
				item.materialName = CreateNewsImageMaterial(item);

				if (!item.materialName.empty())
				{
					item.material = Materials::GetRuntimeMaterial(item.materialName);
				}
			}

			newsItems = std::move(items);
			ApplyNewsImageMaterialsToMenu();

			zw3_ui_news_index.Set(0);
			zw3_ui_news_page.Set(0);
			zw3_ui_news_count.Set(static_cast<int>(newsItems.size()));
			zw3_ui_news_progress.Set(0.0f);
			zw3_ui_news_hover.Set(false);
			zw3_ui_news_image.Set("");
			zw3_ui_news_has_image.Set(false);
			zw3_ui_news_loading.Set(false);

			isNewsFetching.store(false);

			newsElapsedMs = 0;
			lastNewsUpdate = 0;
			holdNewsUntil = 0;
			wasNewsHovered = false;

			Scheduler::Once(ApplyNewsItem, Scheduler::Pipeline::MAIN);
		}, Scheduler::Pipeline::MAIN);
	}

	static void BeginNewsFetch()
	{
		zw3_ui_news_loading.Set(true);

		if (isNewsFetching.exchange(true))
		{
			return;
		}

		Scheduler::Once(FetchNews, Scheduler::Pipeline::ASYNC);
	}

	static void UpdateNewsCarousel()
	{
		if (newsItems.empty())
		{
			return;
		}

		const int now = Game::Sys_Milliseconds();

		if (!lastNewsUpdate)
		{
			lastNewsUpdate = now;
		}

		const int deltaMs = std::clamp(now - lastNewsUpdate, 0, 100);
		lastNewsUpdate = now;

		int index = zw3_ui_news_index.Get<int>();

		if (index < 0 || index >= static_cast<int>(newsItems.size()))
		{
			index = 0;
			zw3_ui_news_index.Set(index);
			newsElapsedMs = 0;
			ApplyNewsItem();
		}

		const int durationMs = std::max(newsItems[index].durationMs, 1500);

		if (zw3_ui_news_hover.Get<bool>())
		{
			wasNewsHovered = true;
			zw3_ui_news_progress.Set(0.0f);
			ApplyNewsItem();
			return;
		}

		if (wasNewsHovered)
		{
			wasNewsHovered = false;
			holdNewsUntil = now + 1200;
			newsElapsedMs = std::min(durationMs / 2, durationMs - 500);
		}

		if (now >= holdNewsUntil)
		{
			newsElapsedMs += deltaMs;
		}

		if (newsElapsedMs >= durationMs)
		{
			newsElapsedMs = 0;
			index = (index + 1) % static_cast<int>(newsItems.size());

			zw3_ui_news_index.Set(index);
			ApplyNewsItem();
		}

		zw3_ui_news_progress.Set(std::clamp(static_cast<float>(newsElapsedMs) / static_cast<float>(durationMs), 0.0f, 1.0f));
		ApplyNewsItem();
	}

	static void RefreshNews()
	{
		zw3_ui_news_loading.Set(true);
		zw3_ui_news_index.Set(0);
		zw3_ui_news_page.Set(0);
		zw3_ui_news_count.Set(0);
		zw3_ui_news_progress.Set(0.0f);
		zw3_ui_news_counter.Set("0 / 0");
		zw3_ui_news_image.Set("");
		zw3_ui_news_has_image.Set(false);

		ApplyNewsTileTitles();
		ApplyNewsImageMaterialToMenu(nullptr);
		ApplyNewsImageMaterialsToMenu();

		BeginNewsFetch();
	}

	static void OpenNews()
	{
		const int index = zw3_ui_news_index.Get<int>();

		if (index < 0 || index >= static_cast<int>(newsItems.size()))
		{
			return;
		}

		const NewsItem& item = newsItems[index];

		for (const std::string& command : item.actionCommands)
		{
			if (command.empty())
			{
				continue;
			}

			if (!IsNewsCommandAllowed(command))
			{
				Logger::Print("menus: news item {} asks for \"{}\", which is not on the news command list, skipped\n", index, command);
				continue;
			}

			Command::Execute(command, true);

			constexpr std::string_view mapCommand = "set ui_mapname ";

			if (_strnicmp(command.data(), mapCommand.data(), mapCommand.size()) != 0)
			{
				continue;
			}

			const std::string mapName = command.substr(mapCommand.size());
			const bool isMapName = !mapName.empty()
				&& mapName.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_") == std::string::npos;

			if (isMapName)
			{
				Dvar::Find("zw3_pref_ui_mapname").Set(mapName);
			}
		}

		if (item.actionType.empty() || item.actionTarget.empty())
		{
			return;
		}

		if (_stricmp(item.actionType.data(), "menu") == 0)
		{
			Game::Menus_OpenByName(Game::uiContext, item.actionTarget.data());
			return;
		}

		if (_stricmp(item.actionType.data(), "link") == 0)
		{
			if (item.actionTarget.find_first_of("\";\r\n") != std::string::npos)
			{
				Logger::Print("menus: news item {} has a link that would break out of openLink's quotes, skipped\n", index);
				return;
			}

			Command::Execute(std::format("openLink \"{}\"", item.actionTarget), true);
			return;
		}

		if (_stricmp(item.actionType.data(), "command") == 0 || _stricmp(item.actionType.data(), "exec") == 0)
		{
			if (!IsNewsCommandAllowed(item.actionTarget))
			{
				Logger::Print("menus: news item {} asks for \"{}\", which is not on the news command list, skipped\n", index, item.actionTarget);
				return;
			}

			Command::Execute(item.actionTarget, true);
		}
	}

	static const Utils::Hook::LeaSite xboxLiveMenuNameLeas[] =
	{
		{ 0x140272BD1, Utils::Hook::leaRdx, 0x14037A4B0 },
		{ 0x140272CCC, Utils::Hook::leaRdx, 0x14037A4B0 },
		{ 0x140272D9E, Utils::Hook::leaRdx, 0x14037A4B0 },
	};

	constexpr std::uintptr_t Menus_OpenCalls[] = { 0x14025CAE4, 0x14025CB6C, 0x14025D634, 0x14025E467, 0x14025E576, 0x140267DC9 };

	static const std::uint8_t menusOpenCallBytes[][5] =
	{
		{ 0xE8, 0x67, 0xAF, 0x00, 0x00 },
		{ 0xE8, 0xDF, 0xAE, 0x00, 0x00 },
		{ 0xE8, 0x17, 0xA4, 0x00, 0x00 },
		{ 0xE8, 0xE4, 0x95, 0x00, 0x00 },
		{ 0xE8, 0xD5, 0x94, 0x00, 0x00 },
		{ 0xE8, 0x82, 0xFC, 0xFF, 0xFF },
	};

	constexpr std::uintptr_t Menus_OpenByName_FindCall = 0x140267DB9;

	static const std::uint8_t findForOpenCallBytes[] = { 0xE8, 0x72, 0xF5, 0xFF, 0xFF };

	constexpr std::uintptr_t Menus_CloseAllCalls[] =
	{
		0x14026BFBF, 0x14026C004, 0x14026C08D, 0x14026C156, 0x14026F7CB, 0x14026FA1E,
		0x140270C1D, 0x140272107, 0x140272159, 0x1402721B0, 0x14027294C, 0x140272A46,
		0x140272AB4, 0x140272B28, 0x140272E38, 0x140272E59, 0x14027302B,
	};

	static const std::uint8_t closeAllCallBytes[][5] =
	{
		{ 0xE8, 0xFC, 0xAF, 0xFF, 0xFF },
		{ 0xE9, 0xB7, 0xAF, 0xFF, 0xFF },
		{ 0xE9, 0x2E, 0xAF, 0xFF, 0xFF },
		{ 0xE8, 0x65, 0xAE, 0xFF, 0xFF },
		{ 0xE8, 0xF0, 0x77, 0xFF, 0xFF },
		{ 0xE8, 0x9D, 0x75, 0xFF, 0xFF },
		{ 0xE8, 0x9E, 0x63, 0xFF, 0xFF },
		{ 0xE8, 0xB4, 0x4E, 0xFF, 0xFF },
		{ 0xE8, 0x62, 0x4E, 0xFF, 0xFF },
		{ 0xE8, 0x0B, 0x4E, 0xFF, 0xFF },
		{ 0xE8, 0x6F, 0x46, 0xFF, 0xFF },
		{ 0xE8, 0x75, 0x45, 0xFF, 0xFF },
		{ 0xE8, 0x07, 0x45, 0xFF, 0xFF },
		{ 0xE8, 0x93, 0x44, 0xFF, 0xFF },
		{ 0xE8, 0x83, 0x41, 0xFF, 0xFF },
		{ 0xE8, 0x62, 0x41, 0xFF, 0xFF },
		{ 0xE8, 0x90, 0x3F, 0xFF, 0xFF },
	};

	constexpr std::uintptr_t Menus_CloseRequestCalls[] = { 0x14025CBEC, 0x14025D3D4, 0x14025D574, 0x14025D6E4, 0x1402671CE, 0x140269591 };

	static const std::uint8_t closeRequestCallBytes[][5] =
	{
		{ 0xE8, 0xEF, 0xA5, 0x00, 0x00 },
		{ 0xE8, 0x07, 0x9E, 0x00, 0x00 },
		{ 0xE8, 0x67, 0x9C, 0x00, 0x00 },
		{ 0xE8, 0xF7, 0x9A, 0x00, 0x00 },
		{ 0xE9, 0x0D, 0x00, 0x00, 0x00 },
		{ 0xE8, 0x4A, 0xDC, 0xFF, 0xFF },
	};

	constexpr std::uintptr_t menuResponseCalls[] = { 0x1400E6CC6, 0x1400E6D53, 0x14025E697 };

	static const std::uint8_t menuResponseCallBytes[][5] =
	{
		{ 0xE8, 0xF5, 0x00, 0x10, 0x00 },
		{ 0xE8, 0x68, 0x00, 0x10, 0x00 },
		{ 0xE8, 0x24, 0x87, 0xF8, 0xFF },
	};

	constexpr std::uintptr_t cinematicGlobRequestFlags = 0x1493C9760;
	constexpr std::uintptr_t cinematicGlobThreadFlags = 0x1493C9978;
	constexpr std::uintptr_t cinematicGlobBink = 0x1493C9BA8;

	constexpr std::uintptr_t R_Cinematic_StartPlayback_NowVolumeCall = 0x1400361BA;
	static const std::uint8_t volumeCallBytes[] = { 0xE8, 0x21, 0xFC, 0xFF, 0xFF };

	constexpr std::uintptr_t BinkSetSpeakerVolumesImport = 0x140362980;

	constexpr unsigned int cinematicLooping = 2;
	constexpr unsigned int cinematicMuted = 0x100;

	static constexpr int maxItemsPerMenu = 512;
	static constexpr int maxHandlersPerSet = 128;
	static constexpr int maxFloatExpressions = 32;
	static constexpr int maxStatementEntries = 512;
	static constexpr int maxColumns = 16;
	static constexpr std::size_t maxMenuFileSize = 4u << 20;

	struct Cursor
	{
		const char* at;
		const char* end;
	};

	struct HandlerBuild
	{
		Game::MenuEventHandler* handlers[maxHandlersPerSet];
		int count;
		bool isFull;
	};

	struct ItemExpressions
	{
		int targets[maxFloatExpressions];
		Game::Statement_s* statements[maxFloatExpressions];
		int count;
	};

	struct MenuCinematic
	{
		std::string name;
		bool hasSound;
	};

	static std::unordered_map<const Game::menuDef_t*, MenuCinematic> cinematics;

	static std::mutex cinematicsMutex;

	static bool IsWordChar(char character)
	{
		const bool isLetter = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
		const bool isDigit = character >= '0' && character <= '9';

		return isLetter || isDigit || character == '_' || character == '.' || character == '-' || character == '+';
	}

	static bool IsDigit(char character)
	{
		return character >= '0' && character <= '9';
	}

	static bool IsIdentifierStart(char character)
	{
		return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || character == '_';
	}

	static void SkipLine(Cursor& cursor)
	{
		while (cursor.at < cursor.end && *cursor.at != '\n')
		{
			++cursor.at;
		}
	}

	static void SkipSpace(Cursor& cursor)
	{
		while (cursor.at < cursor.end)
		{
			const char current = *cursor.at;
			const bool hasNext = cursor.at + 1 < cursor.end;

			if (current == '/' && hasNext && cursor.at[1] == '/')
			{
				SkipLine(cursor);
				continue;
			}

			if (current == '/' && hasNext && cursor.at[1] == '*')
			{
				cursor.at += 2;

				while (cursor.at + 1 < cursor.end && !(cursor.at[0] == '*' && cursor.at[1] == '/'))
				{
					++cursor.at;
				}

				if (cursor.at + 1 < cursor.end)
				{
					cursor.at += 2;
				}
				else
				{
					cursor.at = cursor.end;
				}

				continue;
			}

			if (current == '#')
			{
				SkipLine(cursor);
				continue;
			}

			if (static_cast<signed char>(current) > ' ')
			{
				return;
			}

			++cursor.at;
		}
	}

	static char ReadEscapeCharacter(const char*& at, const char* end)
	{
		++at;

		if (at >= end)
		{
			return '\0';
		}

		const char escaped = *at;

		switch (escaped)
		{
		case '"':
		case '\'':
		case '?':
		case '\\':
			++at;
			return escaped;
		case 'a':
			++at;
			return '\a';
		case 'b':
			++at;
			return '\b';
		case 'f':
			++at;
			return '\f';
		case 'n':
			++at;
			return '\n';
		case 'r':
			++at;
			return '\r';
		case 't':
			++at;
			return '\t';
		case 'v':
			++at;
			return '\v';
		default:
			break;
		}

		int value = 0;

		if (escaped == 'x')
		{
			++at;

			while (at < end)
			{
				const char digit = *at;

				if (digit >= '0' && digit <= '9')
				{
					value = value * 16 + (digit - '0');
				}
				else if (digit >= 'A' && digit <= 'Z')
				{
					value = value * 16 + (digit - 'A' + 10);
				}
				else if (digit >= 'a' && digit <= 'z')
				{
					value = value * 16 + (digit - 'a' + 10);
				}
				else
				{
					break;
				}

				value = std::min(value, 0x10000);
				++at;
			}
		}
		else
		{
			while (at < end && *at >= '0' && *at <= '9')
			{
				value = std::min(value * 10 + (*at - '0'), 0x10000);
				++at;
			}
		}

		return static_cast<char>(std::min(value, 255));
	}

	static bool NextToken(Cursor& cursor, char* out, int capacity)
	{
		SkipSpace(cursor);

		if (cursor.at >= cursor.end)
		{
			out[0] = '\0';
			return false;
		}

		int length = 0;

		if (*cursor.at == '"')
		{
			++cursor.at;

			while (cursor.at < cursor.end && *cursor.at != '"')
			{
				char character = *cursor.at;

				if (character == '\\')
				{
					character = ReadEscapeCharacter(cursor.at, cursor.end);
				}
				else
				{
					++cursor.at;
				}

				if (length < capacity - 1)
				{
					out[length] = character;
					++length;
				}
			}

			if (cursor.at < cursor.end)
			{
				++cursor.at;
			}

			out[length] = '\0';
			return true;
		}

		if (!IsWordChar(*cursor.at))
		{
			out[0] = *cursor.at;
			out[1] = '\0';
			++cursor.at;
			return true;
		}

		while (cursor.at < cursor.end && IsWordChar(*cursor.at))
		{
			if (length < capacity - 1)
			{
				out[length] = *cursor.at;
				++length;
			}

			++cursor.at;
		}

		out[length] = '\0';
		return true;
	}

	static bool PeekToken(Cursor cursor, char* out, int capacity)
	{
		return NextToken(cursor, out, capacity);
	}

	static void SkipBraceBlock(Cursor& cursor)
	{
		SkipSpace(cursor);

		if (cursor.at >= cursor.end || *cursor.at != '{')
		{
			return;
		}

		int depth = 0;

		while (cursor.at < cursor.end)
		{
			const char current = *cursor.at;

			if (current == '{')
			{
				++depth;
			}
			else if (current == '}')
			{
				--depth;
			}
			else if (current == '"')
			{
				++cursor.at;

				while (cursor.at < cursor.end && *cursor.at != '"')
				{
					if (*cursor.at == '\\' && cursor.at + 1 < cursor.end)
					{
						++cursor.at;
					}

					++cursor.at;
				}
			}

			++cursor.at;

			if (depth == 0)
			{
				return;
			}
		}
	}

	static bool CaptureParens(Cursor& cursor, char* out, int capacity)
	{
		SkipSpace(cursor);

		if (cursor.at >= cursor.end || *cursor.at != '(')
		{
			return false;
		}

		int depth = 0;
		int length = 0;
		bool isQuoted = false;

		while (cursor.at < cursor.end)
		{
			const char current = *cursor.at;
			const bool isEscape = isQuoted && current == '\\' && cursor.at + 1 < cursor.end;

			if (current == '"')
			{
				isQuoted = !isQuoted;
			}
			else if (!isQuoted && current == '(')
			{
				++depth;
			}
			else if (!isQuoted && current == ')')
			{
				--depth;
			}

			if (length < capacity - 1)
			{
				out[length] = current;
				++length;
			}

			++cursor.at;

			if (isEscape)
			{
				if (length < capacity - 1)
				{
					out[length] = *cursor.at;
					++length;
				}

				++cursor.at;
			}

			if (depth == 0)
			{
				break;
			}
		}

		out[length] = '\0';
		return depth == 0;
	}

	static bool CaptureExpression(Cursor& cursor, char* out, int capacity)
	{
		SkipSpace(cursor);

		if (cursor.at < cursor.end && *cursor.at == '(')
		{
			return CaptureParens(cursor, out, capacity);
		}

		int depth = 0;
		int length = 0;
		bool isQuoted = false;

		while (cursor.at < cursor.end)
		{
			const char current = *cursor.at;
			const bool isEscape = isQuoted && current == '\\' && cursor.at + 1 < cursor.end;

			if (!isQuoted && depth == 0 && (current == ';' || current == '\n'))
			{
				++cursor.at;
				break;
			}

			if (current == '"')
			{
				isQuoted = !isQuoted;
			}
			else if (!isQuoted && current == '(')
			{
				++depth;
			}
			else if (!isQuoted && current == ')')
			{
				--depth;
			}

			if (length < capacity - 1)
			{
				out[length] = current;
				++length;
			}

			++cursor.at;

			if (isEscape)
			{
				if (length < capacity - 1)
				{
					out[length] = *cursor.at;
					++length;
				}

				++cursor.at;
			}

			if (depth < 0)
			{
				break;
			}

			if (current == ')' && depth == 0)
			{
				break;
			}
		}

		out[length] = '\0';
		return length > 0 && depth == 0 && !isQuoted;
	}

	static bool CaptureBraceBody(Cursor& cursor, Cursor& body)
	{
		SkipSpace(cursor);

		if (cursor.at >= cursor.end || *cursor.at != '{')
		{
			return false;
		}

		const char* const open = cursor.at;
		SkipBraceBlock(cursor);

		body.at = open + 1;
		body.end = cursor.at;

		if (cursor.at > body.at && *(cursor.at - 1) == '}')
		{
			body.end = cursor.at - 1;
		}

		return true;
	}

	static double ReadNumber(Cursor& cursor)
	{
		SkipSpace(cursor);

		if (cursor.at < cursor.end && *cursor.at == '}')
		{
			return 0.0;
		}

		if (cursor.at < cursor.end && *cursor.at == '(')
		{
			char expression[1024];

			if (CaptureParens(cursor, expression, sizeof(expression)))
			{
				double value = 0.0;

				if (Utils::MenuPreprocessor::TryEvaluate(expression, &value))
				{
					return value;
				}
			}

			return 0.0;
		}

		char token[64];

		if (!NextToken(cursor, token, sizeof(token)))
		{
			return 0.0;
		}

		return std::atof(token);
	}

	static float ReadFloat(Cursor& cursor)
	{
		return static_cast<float>(ReadNumber(cursor));
	}

	static int ReadInt(Cursor& cursor)
	{
		return static_cast<int>(ReadNumber(cursor));
	}

	static bool IsNumberAhead(Cursor cursor)
	{
		SkipSpace(cursor);

		if (cursor.at >= cursor.end)
		{
			return false;
		}

		const char current = *cursor.at;
		const bool hasNext = cursor.at + 1 < cursor.end;

		return IsDigit(current)
			|| current == '('
			|| ((current == '-' || current == '+' || current == '.') && hasNext && IsDigit(cursor.at[1]));
	}

	static const char* OperatorName(int index)
	{
		if (index < 0 || index >= Game::OP_COUNT || !Game::expressionOperatorNames)
		{
			return nullptr;
		}

		return Game::expressionOperatorNames[index];
	}

	static int FindSymbolOperator(const char* text, int* length)
	{
		for (int wanted = 2; wanted >= 1; --wanted)
		{
			for (int index = 1; index <= Game::OP_LAST_SYMBOL; ++index)
			{
				const char* const name = OperatorName(index);

				if (!name || static_cast<int>(std::strlen(name)) != wanted)
				{
					continue;
				}

				if (std::strncmp(text, name, static_cast<std::size_t>(wanted)) == 0)
				{
					*length = wanted;
					return index;
				}
			}
		}

		return -1;
	}

	static int FindFunctionOperator(const char* text, int length)
	{
		for (int index = Game::OP_LAST_SYMBOL + 1; index < Game::OP_COUNT; ++index)
		{
			const char* const name = OperatorName(index);

			if (!name || static_cast<int>(std::strlen(name)) != length)
			{
				continue;
			}

			if (_strnicmp(text, name, static_cast<std::size_t>(length)) == 0)
			{
				return index;
			}
		}

		return -1;
	}

	static Game::Statement_s* CompileStatement(const char* expression)
	{
		Game::expressionEntry entries[maxStatementEntries] = {};
		int count = 0;
		int depth = 0;
		bool isExpectingOperand = true;
		bool isFinished = false;

		bool isAfterFunctionName = false;
		const char* at = expression;

		while (*at && !isFinished)
		{
			if (static_cast<signed char>(*at) <= ' ')
			{
				++at;
				continue;
			}

			if (count + 2 >= maxStatementEntries)
			{
				return nullptr;
			}

			Game::expressionEntry& entry = entries[count];

			if (*at == '"')
			{
				++at;
				const char* const end = at + std::strlen(at);
				std::string text;

				while (*at && *at != '"')
				{
					if (*at == '\\')
					{
						text += ReadEscapeCharacter(at, end);
						continue;
					}

					text += *at;
					++at;
				}

				if (*at == '"')
				{
					++at;
				}

				entry.type = Game::EET_OPERAND;
				entry.data.operand.dataType = Game::VAL_STRING;
				entry.data.operand.internals.stringVal.string = Menus::GetAllocator()->DuplicateString(text);
				++count;
				isExpectingOperand = false;
				isAfterFunctionName = false;
				continue;
			}

			if (IsDigit(*at) || (*at == '.' && IsDigit(at[1])))
			{
				char* end = nullptr;
				const double value = std::strtod(at, &end);
				at = end;

				const bool isIntegral = value == std::floor(value)
					&& value >= static_cast<double>(INT_MIN)
					&& value <= static_cast<double>(INT_MAX);

				entry.type = Game::EET_OPERAND;

				if (isIntegral)
				{
					entry.data.operand.dataType = Game::VAL_INT;
					entry.data.operand.internals.intVal = static_cast<int>(value);
				}
				else
				{
					entry.data.operand.dataType = Game::VAL_FLOAT;
					entry.data.operand.internals.floatVal = static_cast<float>(value);
				}

				++count;
				isExpectingOperand = false;
				isAfterFunctionName = false;
				continue;
			}

			if (IsIdentifierStart(*at))
			{
				const char* const begin = at;

				while (IsIdentifierStart(*at) || IsDigit(*at))
				{
					++at;
				}

				const int length = static_cast<int>(at - begin);
				const int index = FindFunctionOperator(begin, length);

				if (index < 0)
				{
					entry.type = Game::EET_OPERAND;
					entry.data.operand.dataType = Game::VAL_STRING;
					entry.data.operand.internals.stringVal.string =
						Menus::GetAllocator()->DuplicateString(std::string(begin, static_cast<std::size_t>(length)));
					++count;
					isExpectingOperand = false;
					isAfterFunctionName = false;
					continue;
				}

				entry.type = Game::EET_OPERATOR;
				entry.data.op = index;
				++count;
				isExpectingOperand = true;
				isAfterFunctionName = true;
				continue;
			}

			int length = 0;
			const int index = FindSymbolOperator(at, &length);

			if (index < 0)
			{
				return nullptr;
			}

			at += length;

			const bool isFunctionCall = index == Game::OP_LEFTPAREN && isAfterFunctionName;
			isAfterFunctionName = false;

			if (index == Game::OP_LEFTPAREN)
			{
				++depth;

				if (!isFunctionCall)
				{
					entry.type = Game::EET_OPERATOR;
					entry.data.op = Game::OP_LEFTPAREN;
					++count;
				}

				isExpectingOperand = true;
				continue;
			}

			if (index == Game::OP_RIGHTPAREN)
			{
				--depth;

				if (depth < 0)
				{
					return nullptr;
				}

				if (depth == 0)
				{
					isFinished = true;
					continue;
				}

				entry.type = Game::EET_OPERATOR;
				entry.data.op = Game::OP_RIGHTPAREN;
				++count;
				isExpectingOperand = false;
				continue;
			}

			if (index == Game::OP_SUBTRACT && isExpectingOperand)
			{
				entry.type = Game::EET_OPERAND;
				entry.data.operand.dataType = Game::VAL_INT;
				entry.data.operand.internals.intVal = 0;
				++count;
			}

			Game::expressionEntry& operatorEntry = entries[count];
			operatorEntry.type = Game::EET_OPERATOR;
			operatorEntry.data.op = index;
			++count;
			isExpectingOperand = true;
		}

		if (count == 0 || (!isFinished && depth != 0))
		{
			return nullptr;
		}

		auto* const block = Menus::GetAllocator()->AllocateArray<Game::expressionEntry>(static_cast<std::size_t>(count));
		std::memcpy(block, entries, sizeof(Game::expressionEntry) * static_cast<std::size_t>(count));

		auto* const statement = Menus::GetAllocator()->Allocate<Game::Statement_s>();
		statement->numEntries = count;
		statement->entries = block;
		statement->supportingData = Menus::GetSupportingData();
		statement->lastExecuteTime = -1;

		return statement;
	}

	static Game::Statement_s* CompileParens(Cursor& cursor, const char* field)
	{
		char expression[4096];

		if (!CaptureExpression(cursor, expression, sizeof(expression)))
		{
			SkipLine(cursor);
			return nullptr;
		}

		Game::Statement_s* const statement = CompileStatement(expression);

		if (!statement)
		{
			Logger::Warning("menus: cannot compile {} expression {}\n", field, expression);
		}

		return statement;
	}

	static Game::Material* ResolveMaterial(const char* name)
	{
		if (!name || !*name || !Game::Material_RegisterHandle)
		{
			return nullptr;
		}

		return Game::Material_RegisterHandle(name, 0);
	}

	static void ParseRect(Cursor& cursor, Game::windowDef_t* window)
	{
		const float x = ReadFloat(cursor);
		const float y = ReadFloat(cursor);
		const float width = ReadFloat(cursor);
		const float height = ReadFloat(cursor);

		unsigned char horzAlign = 0;
		unsigned char vertAlign = 0;

		if (IsNumberAhead(cursor))
		{
			horzAlign = static_cast<unsigned char>(ReadInt(cursor));

			if (IsNumberAhead(cursor))
			{
				vertAlign = static_cast<unsigned char>(ReadInt(cursor));
			}
		}

		window->rect.x = x;
		window->rect.y = y;
		window->rect.w = width;
		window->rect.h = height;
		window->rect.horzAlign = horzAlign;
		window->rect.vertAlign = vertAlign;

		window->rectClient = window->rect;
	}

	static void ParseColor(Cursor& cursor, float* color)
	{
		for (int channel = 0; channel < 4; ++channel)
		{
			color[channel] = ReadFloat(cursor);
		}
	}

	static int TypeDataSize(int type)
	{
		if (type == Game::ITEM_TYPE_LISTBOX)
		{
			return static_cast<int>(sizeof(Game::listBoxDef_s));
		}

		if (type == Game::ITEM_TYPE_MULTI)
		{
			return static_cast<int>(sizeof(Game::multiDef_s));
		}

		if (type == Game::ITEM_TYPE_NEWS_TICKER)
		{
			return static_cast<int>(sizeof(Game::newsTickerDef_s));
		}

		if (type == Game::ITEM_TYPE_TEXT_SCROLL)
		{
			return static_cast<int>(sizeof(Game::textScrollDef_s));
		}

		if (type >= 0 && type <= Game::ITEM_TYPE_PASSWORDFIELD
			&& ((Game::EDIT_FIELD_TYPE_MASK >> type) & 1u) != 0)
		{
			return static_cast<int>(sizeof(Game::editFieldDef_s));
		}

		return 0;
	}

	static Game::MenuEventHandlerSet* BuildHandlerSet(Cursor cursor, bool isNested);

	static void AddHandler(HandlerBuild& build, int type, void* payload)
	{
		if (build.count >= maxHandlersPerSet)
		{
			build.isFull = true;
			return;
		}

		auto* const handler = Menus::GetAllocator()->Allocate<Game::MenuEventHandler>();
		handler->eventData.conditionalScript = static_cast<Game::ConditionalScript*>(payload);
		handler->eventType = static_cast<char>(type);

		build.handlers[build.count] = handler;
		++build.count;
	}

	static void AddScript(HandlerBuild& build, const std::string& script)
	{
		if (script.empty())
		{
			return;
		}

		AddHandler(build, Game::EVENT_UNCONDITIONAL, Menus::GetAllocator()->DuplicateString(script));
	}

	static const char* const scriptPunctuation[] =
	{
		">>=", "<<=", "...", "##", "&&", "||", ">=", "<=", "==", "!=", "*=", "/=", "%=", "+=", "-=",
		"++", "--", "&=", "|=", "^=", ">>", "<<", "->", "::", ".*", "*", "/", "%", "+", "-", "&",
		"|", "^", "~", "!", "=", "<", ">", "?", ":", ";", ".", ",", "(", ")", "{", "}", "[", "]",
		"\\", "#", "$",
	};

	static std::size_t ScriptPunctuationLength(std::string_view text)
	{
		for (const char* const punctuation : scriptPunctuation)
		{
			const std::string_view candidate(punctuation);

			if (text.starts_with(candidate))
			{
				return candidate.size();
			}
		}

		return 0;
	}

	static std::string SerializeScriptTokens(std::string_view text)
	{
		std::string serialized;
		std::size_t at = 0;

		const auto emit = [&serialized](std::string_view token)
		{
			if (token.size() == 1)
			{
				serialized += token;
			}
			else
			{
				serialized += '"';
				serialized += token;
				serialized += '"';
			}

			serialized += ' ';
		};

		while (at < text.size())
		{
			const auto current = static_cast<unsigned char>(text[at]);

			if (static_cast<signed char>(current) <= ' ')
			{
				++at;
				continue;
			}

			if (current == '"')
			{
				std::string value;
				const char* close = text.data() + at + 1;
				const char* const end = text.data() + text.size();

				while (close < end && *close != '"')
				{
					if (*close == '\\')
					{
						value += ReadEscapeCharacter(close, end);
						continue;
					}

					value += *close;
					++close;
				}

				emit(value);
				at = static_cast<std::size_t>(close - text.data());

				if (at < text.size())
				{
					++at;
				}

				continue;
			}

			const bool startsNumber = std::isdigit(current)
				|| (current == '.' && at + 1 < text.size() && std::isdigit(static_cast<unsigned char>(text[at + 1])));

			if (startsNumber)
			{
				const std::size_t start = at;

				while (at < text.size() && (std::isalnum(static_cast<unsigned char>(text[at])) || text[at] == '.'))
				{
					++at;
				}

				emit(text.substr(start, at - start));
				continue;
			}

			const std::size_t punctuationLength = ScriptPunctuationLength(text.substr(at));

			if (punctuationLength > 0)
			{
				emit(text.substr(at, punctuationLength));
				at += punctuationLength;
				continue;
			}

			const std::size_t start = at;

			while (at < text.size() && static_cast<unsigned char>(text[at]) > ' ' && text[at] != '"'
				&& ScriptPunctuationLength(text.substr(at)) == 0)
			{
				++at;
			}

			emit(text.substr(start, at - start));
		}

		return serialized;
	}

	static bool ReadScriptStatement(Cursor& cursor, std::string* out)
	{
		const char* const begin = cursor.at;
		bool isInQuote = false;

		while (cursor.at < cursor.end)
		{
			const char current = *cursor.at;

			if (isInQuote && current == '\\' && cursor.at + 1 < cursor.end)
			{
				cursor.at += 2;
				continue;
			}

			if (current == '"')
			{
				isInQuote = !isInQuote;
			}
			else if (!isInQuote && (current == ';' || current == '\n' || current == '{' || current == '}'))
			{
				break;
			}

			++cursor.at;
		}

		if (cursor.at < cursor.end && *cursor.at == '{')
		{
			SkipBraceBlock(cursor);
			return false;
		}

		const char* end = cursor.at;

		while (end > begin && static_cast<signed char>(end[-1]) <= ' ')
		{
			--end;
		}

		if (cursor.at < cursor.end)
		{
			++cursor.at;
		}

		if (end <= begin)
		{
			return false;
		}

		out->assign(SerializeScriptTokens(std::string_view(begin, static_cast<std::size_t>(end - begin))));
		out->append("; ");
		return true;
	}

	static bool IsSetLocalVar(const char* command)
	{
		return _stricmp(command, "setLocalVarBool") == 0
			|| _stricmp(command, "setLocalVarInt") == 0
			|| _stricmp(command, "setLocalVarFloat") == 0
			|| _stricmp(command, "setLocalVarString") == 0;
	}

	static int SetLocalVarEventType(const char* command)
	{
		if (_stricmp(command, "setLocalVarBool") == 0)
		{
			return Game::EVENT_SET_LOCAL_VAR_BOOL;
		}

		if (_stricmp(command, "setLocalVarInt") == 0)
		{
			return Game::EVENT_SET_LOCAL_VAR_INT;
		}

		if (_stricmp(command, "setLocalVarFloat") == 0)
		{
			return Game::EVENT_SET_LOCAL_VAR_FLOAT;
		}

		return Game::EVENT_SET_LOCAL_VAR_STRING;
	}

	static void BuildIf(Cursor& cursor, HandlerBuild& build)
	{
		char expression[4096];
		expression[0] = '\0';

		if (!CaptureParens(cursor, expression, sizeof(expression)))
		{
			cursor.at = cursor.end;
			return;
		}

		Cursor thenBody = {};

		if (!CaptureBraceBody(cursor, thenBody))
		{
			return;
		}

		Game::Statement_s* const statement = CompileStatement(expression);

		if (!statement)
		{
			Logger::Warning("menus: cannot compile if {}\n", expression);
			return;
		}

		auto* const conditional = Menus::GetAllocator()->Allocate<Game::ConditionalScript>();
		conditional->eventHandlerSet = BuildHandlerSet(thenBody, true);
		conditional->eventExpression = statement;
		AddHandler(build, Game::EVENT_IF, conditional);

		char peek[16];

		if (!PeekToken(cursor, peek, sizeof(peek)) || _stricmp(peek, "else") != 0)
		{
			return;
		}

		NextToken(cursor, peek, sizeof(peek));
		SkipSpace(cursor);

		if (cursor.at < cursor.end && *cursor.at == '{')
		{
			Cursor elseBody = {};
			CaptureBraceBody(cursor, elseBody);
			AddHandler(build, Game::EVENT_ELSE, BuildHandlerSet(elseBody, true));
			return;
		}

		if (!PeekToken(cursor, peek, sizeof(peek)) || _stricmp(peek, "if") != 0)
		{
			return;
		}

		NextToken(cursor, peek, sizeof(peek));

		HandlerBuild nested = {};
		BuildIf(cursor, nested);

		auto* const set = Menus::GetAllocator()->Allocate<Game::MenuEventHandlerSet>();
		set->eventHandlerCount = nested.count;

		if (nested.count > 0)
		{
			set->eventHandlers = Menus::GetAllocator()->AllocateArray<Game::MenuEventHandler*>(
				static_cast<std::size_t>(nested.count));

			for (int index = 0; index < nested.count; ++index)
			{
				set->eventHandlers[index] = nested.handlers[index];
			}
		}

		AddHandler(build, Game::EVENT_ELSE, set);
	}

	static void BuildSetLocalVar(Cursor& cursor, const char* command, HandlerBuild& build)
	{
		char name[256];

		if (!NextToken(cursor, name, sizeof(name)))
		{
			return;
		}

		char expression[1024];
		expression[0] = '\0';

		if (!CaptureExpression(cursor, expression, sizeof(expression)))
		{
			return;
		}

		Game::Statement_s* const compiled = CompileStatement(expression);

		if (!compiled)
		{
			Logger::Warning("menus: cannot compile {} {} {}\n", command, name, expression);
			return;
		}

		auto* const data = Menus::GetAllocator()->Allocate<Game::SetLocalVarData>();
		data->localVarName = Menus::GetAllocator()->DuplicateString(name);
		data->expression = compiled;

		AddHandler(build, SetLocalVarEventType(command), data);
	}

	static Game::MenuEventHandlerSet* BuildHandlerSet(Cursor cursor, bool isNested)
	{
		HandlerBuild build = {};
		std::string script;
		char command[256];

		for (;;)
		{
			SkipSpace(cursor);

			if (cursor.at >= cursor.end)
			{
				break;
			}

			if (*cursor.at == ';')
			{
				++cursor.at;
				continue;
			}

			const Cursor statement = cursor;

			if (!NextToken(cursor, command, sizeof(command)))
			{
				break;
			}

			if (_stricmp(command, "if") == 0)
			{
				AddScript(build, script);
				script.clear();
				BuildIf(cursor, build);
				continue;
			}

			if (IsSetLocalVar(command))
			{
				AddScript(build, script);
				script.clear();
				BuildSetLocalVar(cursor, command, build);
				continue;
			}

			cursor = statement;
			std::string line;

			if (ReadScriptStatement(cursor, &line))
			{
				script.append(line);
			}
		}

		AddScript(build, script);

		if (build.isFull)
		{
			Logger::Warning("menus: a script block has more than {} handlers, the rest were dropped\n",
				maxHandlersPerSet);
		}

		if (build.count == 0 && !isNested)
		{
			return nullptr;
		}

		auto* const set = Menus::GetAllocator()->Allocate<Game::MenuEventHandlerSet>();
		set->eventHandlerCount = build.count;

		if (build.count > 0)
		{
			set->eventHandlers = Menus::GetAllocator()->AllocateArray<Game::MenuEventHandler*>(
				static_cast<std::size_t>(build.count));

			for (int index = 0; index < build.count; ++index)
			{
				set->eventHandlers[index] = build.handlers[index];
			}
		}

		return set;
	}

	static Game::MenuEventHandlerSet* ParseHandlerSet(Cursor& cursor)
	{
		Cursor body = {};

		if (!CaptureBraceBody(cursor, body))
		{
			return nullptr;
		}

		return BuildHandlerSet(body, false);
	}

	static void AppendHandlerSet(Cursor& cursor, Game::MenuEventHandlerSet** target)
	{
		Game::MenuEventHandlerSet* const parsed = ParseHandlerSet(cursor);

		if (!parsed)
		{
			return;
		}

		if (!*target)
		{
			*target = parsed;
			return;
		}

		Game::MenuEventHandlerSet* const existing = *target;
		const int count = existing->eventHandlerCount + parsed->eventHandlerCount;
		auto** const handlers = Menus::GetAllocator()->AllocateArray<Game::MenuEventHandler*>(static_cast<std::size_t>(count));

		for (int index = 0; index < existing->eventHandlerCount; ++index)
		{
			handlers[index] = existing->eventHandlers[index];
		}

		for (int index = 0; index < parsed->eventHandlerCount; ++index)
		{
			handlers[existing->eventHandlerCount + index] = parsed->eventHandlers[index];
		}

		existing->eventHandlers = handlers;
		existing->eventHandlerCount = count;
	}

	static void ParseKeyHandler(Cursor& cursor, bool isByName, Game::ItemKeyHandler** head)
	{
		int key = -1;

		if (isByName)
		{
			char name[64];
			NextToken(cursor, name, sizeof(name));
			key = Game::Key_StringToKeynum ? Game::Key_StringToKeynum(name) : -1;
		}
		else
		{
			key = ReadInt(cursor);
		}

		Game::MenuEventHandlerSet* const set = ParseHandlerSet(cursor);

		if (key < 1 || key > 255 || !set)
		{
			return;
		}

		auto* const node = Menus::GetAllocator()->Allocate<Game::ItemKeyHandler>();
		node->key = key;
		node->action = set;

		Game::ItemKeyHandler** tail = head;

		while (*tail)
		{
			tail = &(*tail)->next;
		}

		*tail = node;
	}

	static void ParseDvarList(Cursor& cursor, Game::itemDef_s* item, unsigned int flag)
	{
		Cursor body = {};

		if (!CaptureBraceBody(cursor, body))
		{
			return;
		}

		while (body.at < body.end && static_cast<signed char>(*body.at) <= ' ')
		{
			++body.at;
		}

		while (body.end > body.at && static_cast<signed char>(body.end[-1]) <= ' ')
		{
			--body.end;
		}

		const std::string list(body.at, static_cast<std::size_t>(body.end - body.at));

		item->enableDvar = Menus::GetAllocator()->DuplicateString(list);
		item->dvarFlags |= static_cast<int>(flag);
	}

	static void ParseColumns(Cursor& cursor, Game::listBoxDef_s* listBox)
	{
		int count = ReadInt(cursor);

		if (count < 0)
		{
			count = 0;
		}

		if (count > maxColumns)
		{
			count = maxColumns;
		}

		listBox->numColumns = count;

		for (int column = 0; column < count; ++column)
		{
			listBox->columnInfo[column].pos = ReadInt(cursor);
			listBox->columnInfo[column].width = ReadInt(cursor);
			listBox->columnInfo[column].maxChars = ReadInt(cursor);
			listBox->columnInfo[column].alignment = ReadInt(cursor);
		}
	}

	static int FloatExpressionTarget(const char* field, const char* component)
	{
		if (_stricmp(field, "rect") == 0)
		{
			static const char* const axes[] = { "x", "y", "w", "h" };

			for (int index = 0; index < 4; ++index)
			{
				if (_stricmp(component, axes[index]) == 0)
				{
					return index;
				}
			}

			return -1;
		}

		int base = -1;

		if (_stricmp(field, "forecolor") == 0)
		{
			base = 4;
		}
		else if (_stricmp(field, "glowcolor") == 0)
		{
			base = 9;
		}
		else if (_stricmp(field, "backcolor") == 0)
		{
			base = 14;
		}

		if (base < 0)
		{
			return -1;
		}

		static const char* const channels[] = { "r", "g", "b", "rgb", "a" };

		for (int index = 0; index < 5; ++index)
		{
			if (_stricmp(component, channels[index]) == 0)
			{
				return base + index;
			}
		}

		return -1;
	}

	static void ParseItemExpression(Cursor& cursor, Game::itemDef_s* item, ItemExpressions& expressions)
	{
		char field[32];
		NextToken(cursor, field, sizeof(field));

		const bool isText = _stricmp(field, "text") == 0;
		const bool isMaterial = _stricmp(field, "material") == 0;

		char component[16];
		component[0] = '\0';

		if (!isText && !isMaterial)
		{
			NextToken(cursor, component, sizeof(component));
		}

		if (isText || isMaterial)
		{
			Game::Statement_s* const statement = CompileParens(cursor, field);

			if (isText)
			{
				item->textExp = statement;
			}
			else
			{
				item->materialExp = statement;
			}

			return;
		}

		const int target = FloatExpressionTarget(field, component);

		if (target < 0 || expressions.count >= maxFloatExpressions)
		{
			SkipLine(cursor);
			return;
		}

		Game::Statement_s* const statement = CompileParens(cursor, field);

		if (!statement)
		{
			return;
		}

		expressions.targets[expressions.count] = target;
		expressions.statements[expressions.count] = statement;
		++expressions.count;
	}

	static void ParseMenuExpression(Cursor& cursor, Game::menuDef_t* menu)
	{
		char field[32];
		NextToken(cursor, field, sizeof(field));

		if (_stricmp(field, "rect") != 0)
		{
			SkipLine(cursor);
			return;
		}

		char component[16];
		NextToken(cursor, component, sizeof(component));

		Game::Statement_s* const statement = CompileParens(cursor, "rect");

		if (!statement)
		{
			return;
		}

		if (_stricmp(component, "x") == 0)
		{
			menu->rectXExp = statement;
		}
		else if (_stricmp(component, "y") == 0)
		{
			menu->rectYExp = statement;
		}
		else if (_stricmp(component, "w") == 0)
		{
			menu->rectWExp = statement;
		}
		else if (_stricmp(component, "h") == 0)
		{
			menu->rectHExp = statement;
		}
	}

	static bool ParseItem(Cursor& cursor, Game::itemDef_s* item)
	{
		SkipSpace(cursor);

		if (cursor.at >= cursor.end || *cursor.at != '{')
		{
			return false;
		}

		++cursor.at;

		Game::listBoxDef_s listBox = {};
		Game::editFieldDef_s editField = {};
		Game::newsTickerDef_s ticker = {};

		bool isVisible = false;
		bool hasListBox = false;
		bool hasEditField = false;
		bool hasTicker = false;

		ItemExpressions expressions = {};
		Game::ItemKeyHandler* keyHandlers = nullptr;

		char token[512];
		item->window.name = "";

		for (;;)
		{
			SkipSpace(cursor);

			if (cursor.at >= cursor.end)
			{
				return false;
			}

			if (*cursor.at == '}')
			{
				++cursor.at;
				break;
			}

			if (!NextToken(cursor, token, sizeof(token)))
			{
				return false;
			}

			if (_stricmp(token, "name") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				item->window.name = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "group") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				item->window.group = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "rect") == 0 || _stricmp(token, "rect480") == 0)
			{
				ParseRect(cursor, &item->window);
			}
			else if (_stricmp(token, "origin") == 0)
			{
				item->window.rectClient.x += ReadFloat(cursor);
				item->window.rectClient.y += ReadFloat(cursor);
			}
			else if (_stricmp(token, "style") == 0)
			{
				item->window.style = ReadInt(cursor);
			}
			else if (_stricmp(token, "border") == 0)
			{
				item->window.border = ReadInt(cursor);
			}
			else if (_stricmp(token, "bordersize") == 0)
			{
				item->window.borderSize = ReadFloat(cursor);
			}
			else if (_stricmp(token, "forecolor") == 0)
			{
				ParseColor(cursor, item->window.foreColor);
				item->window.dynamicFlags[0] |= static_cast<int>(Game::WINDOW_DYNAMIC_FORECOLOR_SET);
			}
			else if (_stricmp(token, "backcolor") == 0)
			{
				ParseColor(cursor, item->window.backColor);
			}
			else if (_stricmp(token, "bordercolor") == 0)
			{
				ParseColor(cursor, item->window.borderColor);
			}
			else if (_stricmp(token, "outlinecolor") == 0)
			{
				ParseColor(cursor, item->window.outlineColor);
			}
			else if (_stricmp(token, "disablecolor") == 0)
			{
				ParseColor(cursor, item->window.disableColor);
			}
			else if (_stricmp(token, "glowcolor") == 0)
			{
				ParseColor(cursor, item->glowColor);
			}
			else if (_stricmp(token, "background") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				item->window.background = ResolveMaterial(token);
			}
			else if (_stricmp(token, "ownerdraw") == 0)
			{
				item->window.ownerDraw = ReadInt(cursor);
			}
			else if (_stricmp(token, "ownerdrawflag") == 0 || _stricmp(token, "ownerdrawflags") == 0)
			{
				item->window.ownerDrawFlags |= ReadInt(cursor);
			}
			else if (_stricmp(token, "decoration") == 0)
			{
				item->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_DECORATION);
			}
			else if (_stricmp(token, "autowrapped") == 0)
			{
				item->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_AUTO_WRAPPED);
			}
			else if (_stricmp(token, "horizontalscroll") == 0)
			{
				item->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_HORIZONTAL_SCROLL);
			}
			else if (_stricmp(token, "screenSpace") == 0)
			{
				item->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_SCREEN_SPACE);
			}
			else if (_stricmp(token, "type") == 0)
			{
				const int type = ReadInt(cursor);
				item->type = type;
				item->dataType = type;
			}
			else if (_stricmp(token, "text") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				std::string text = token;
				Utils::String::Replace(text, "\\n", "\n");
				item->text = Menus::GetAllocator()->DuplicateString(text);
			}
			else if (_stricmp(token, "align") == 0)
			{
				item->alignment = ReadInt(cursor);
			}
			else if (_stricmp(token, "textalign") == 0)
			{
				item->textAlignMode = ReadInt(cursor);
			}
			else if (_stricmp(token, "textalignx") == 0)
			{
				item->textalignx = ReadFloat(cursor);
			}
			else if (_stricmp(token, "textaligny") == 0)
			{
				item->textaligny = ReadFloat(cursor);
			}
			else if (_stricmp(token, "textscale") == 0)
			{
				item->textscale = ReadFloat(cursor);
			}
			else if (_stricmp(token, "textstyle") == 0)
			{
				item->textStyle = ReadInt(cursor);
			}
			else if (_stricmp(token, "textfont") == 0)
			{
				item->fontEnum = ReadInt(cursor);
			}
			else if (_stricmp(token, "gamemsgwindowindex") == 0)
			{
				item->gameMsgWindowIndex = ReadInt(cursor);
			}
			else if (_stricmp(token, "gamemsgwindowmode") == 0)
			{
				item->gameMsgWindowMode = ReadInt(cursor);
			}
			else if (_stricmp(token, "dvar") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				item->dvar = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "dvarFloat") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				item->dvar = Menus::GetAllocator()->DuplicateString(token);
				editField.defVal = ReadFloat(cursor);
				editField.minVal = ReadFloat(cursor);
				editField.maxVal = ReadFloat(cursor);
				hasEditField = true;
			}
			else if (_stricmp(token, "dvarTest") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				item->dvarTest = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "localvar") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				item->localVar = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "showDvar") == 0)
			{
				ParseDvarList(cursor, item, Game::ITEM_DVAR_FLAG_SHOW);
			}
			else if (_stricmp(token, "hideDvar") == 0)
			{
				ParseDvarList(cursor, item, Game::ITEM_DVAR_FLAG_HIDE);
			}
			else if (_stricmp(token, "enableDvar") == 0)
			{
				ParseDvarList(cursor, item, Game::ITEM_DVAR_FLAG_ENABLE);
			}
			else if (_stricmp(token, "disableDvar") == 0)
			{
				ParseDvarList(cursor, item, Game::ITEM_DVAR_FLAG_DISABLE);
			}
			else if (_stricmp(token, "action") == 0)
			{
				AppendHandlerSet(cursor, &item->action);
			}
			else if (_stricmp(token, "accept") == 0)
			{
				AppendHandlerSet(cursor, &item->accept);
			}
			else if (_stricmp(token, "onFocus") == 0)
			{
				AppendHandlerSet(cursor, &item->onFocus);
			}
			else if (_stricmp(token, "leaveFocus") == 0)
			{
				AppendHandlerSet(cursor, &item->leaveFocus);
			}
			else if (_stricmp(token, "mouseEnter") == 0)
			{
				AppendHandlerSet(cursor, &item->mouseEnter);
			}
			else if (_stricmp(token, "mouseExit") == 0)
			{
				AppendHandlerSet(cursor, &item->mouseExit);
			}
			else if (_stricmp(token, "mouseEnterText") == 0)
			{
				AppendHandlerSet(cursor, &item->mouseEnterText);
			}
			else if (_stricmp(token, "mouseExitText") == 0)
			{
				AppendHandlerSet(cursor, &item->mouseExitText);
			}
			else if (_stricmp(token, "execKey") == 0)
			{
				ParseKeyHandler(cursor, true, &keyHandlers);
			}
			else if (_stricmp(token, "execKeyInt") == 0)
			{
				ParseKeyHandler(cursor, false, &keyHandlers);
			}
			else if (_stricmp(token, "visible") == 0)
			{
				char peek[64];

				if (PeekToken(cursor, peek, sizeof(peek)) && (_stricmp(peek, "when") == 0 || _stricmp(peek, "if") == 0))
				{
					NextToken(cursor, peek, sizeof(peek));
					item->visibleExp = CompileParens(cursor, "visible when");
					isVisible = true;
				}
				else
				{
					isVisible = ReadInt(cursor) != 0;
				}
			}
			else if (_stricmp(token, "disabled") == 0)
			{
				char peek[16];
				const bool hasArgument = PeekToken(cursor, peek, sizeof(peek));

				if (hasArgument && _stricmp(peek, "when") == 0)
				{
					NextToken(cursor, peek, sizeof(peek));
					item->disabledExp = CompileParens(cursor, "disabled when");
				}
				else if (hasArgument && (IsDigit(peek[0]) || peek[0] == '-' || peek[0] == '+'))
				{
					NextToken(cursor, peek, sizeof(peek));
				}
			}
			else if (_stricmp(token, "exp") == 0)
			{
				ParseItemExpression(cursor, item, expressions);
			}
			else if (_stricmp(token, "feeder") == 0 || _stricmp(token, "special") == 0)
			{
				item->special = ReadFloat(cursor);
			}
			else if (_stricmp(token, "elementwidth") == 0)
			{
				listBox.elementWidth = ReadFloat(cursor);
				hasListBox = true;
			}
			else if (_stricmp(token, "elementheight") == 0)
			{
				listBox.elementHeight = ReadFloat(cursor);
				hasListBox = true;
			}
			else if (_stricmp(token, "elementtype") == 0)
			{
				listBox.elementStyle = ReadInt(cursor);
				hasListBox = true;
			}
			else if (_stricmp(token, "columns") == 0)
			{
				ParseColumns(cursor, &listBox);
				hasListBox = true;
			}
			else if (_stricmp(token, "noscrollbars") == 0)
			{
				listBox.noScrollBars = 1;
				hasListBox = true;
			}
			else if (_stricmp(token, "notselectable") == 0)
			{
				listBox.notselectable = 1;
				hasListBox = true;
			}
			else if (_stricmp(token, "usepaging") == 0)
			{
				listBox.usePaging = 1;
				hasListBox = true;
			}
			else if (_stricmp(token, "selectBorder") == 0)
			{
				ParseColor(cursor, listBox.selectBorder);
				hasListBox = true;
			}
			else if (_stricmp(token, "selectIcon") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				listBox.selectIcon = ResolveMaterial(token);
				hasListBox = true;
			}
			else if (_stricmp(token, "doubleclick") == 0)
			{
				AppendHandlerSet(cursor, &listBox.onDoubleClick);
				hasListBox = true;
			}
			else if (_stricmp(token, "maxChars") == 0)
			{
				editField.maxChars = ReadInt(cursor);
				hasEditField = true;
			}
			else if (_stricmp(token, "maxCharsGotoNext") == 0)
			{
				editField.maxCharsGotoNext = 1;
				hasEditField = true;
			}
			else if (_stricmp(token, "maxPaintChars") == 0)
			{
				editField.maxPaintChars = ReadInt(cursor);
				hasEditField = true;
			}
			else if (_stricmp(token, "speed") == 0)
			{
				ticker.speed = ReadInt(cursor);
				hasTicker = true;
			}
			else if (_stricmp(token, "spacing") == 0)
			{
				ticker.spacing = ReadInt(cursor);
				hasTicker = true;
			}
			else if (_stricmp(token, "newsfeed") == 0)
			{
				ticker.feedId = ReadInt(cursor);
				hasTicker = true;
			}
			else if (_stricmp(token, "dvarStrList") == 0 || _stricmp(token, "dvarFloatList") == 0)
			{
				SkipBraceBlock(cursor);
			}
			else if (_stricmp(token, "dvarEnumList") == 0)
			{
				NextToken(cursor, token, sizeof(token));
			}
		}

		const int typeDataSize = TypeDataSize(item->type);

		if (typeDataSize > 0)
		{
			item->typeData.data = Menus::GetAllocator()->Allocate(static_cast<std::size_t>(typeDataSize));
		}

		if (hasListBox && item->type == Game::ITEM_TYPE_LISTBOX && item->typeData.listBox)
		{
			*item->typeData.listBox = listBox;
		}

		if (hasEditField && item->typeData.editField && typeDataSize == static_cast<int>(sizeof(Game::editFieldDef_s)))
		{
			*item->typeData.editField = editField;
		}

		if (hasTicker && item->type == Game::ITEM_TYPE_NEWS_TICKER && item->typeData.ticker)
		{
			*item->typeData.ticker = ticker;
		}

		item->onKey = keyHandlers;

		if (expressions.count > 0)
		{
			item->floatExpressions = Menus::GetAllocator()->AllocateArray<Game::ItemFloatExpression>(
				static_cast<std::size_t>(expressions.count));
			item->floatExpressionCount = expressions.count;

			for (int index = 0; index < expressions.count; ++index)
			{
				item->floatExpressions[index].target = expressions.targets[index];
				item->floatExpressions[index].expression = expressions.statements[index];
			}
		}

		if (isVisible)
		{
			item->window.dynamicFlags[0] |= static_cast<int>(Game::WINDOW_DYNAMIC_VISIBLE);
		}

		return true;
	}

	static Game::menuDef_t* ParseMenu(Cursor& cursor)
	{
		SkipSpace(cursor);

		if (cursor.at >= cursor.end || *cursor.at != '{')
		{
			return nullptr;
		}

		++cursor.at;

		auto* const menu = Menus::GetAllocator()->Allocate<Game::menuDef_t>();
		menu->window.name = "";
		menu->expressionData = Menus::GetSupportingData();

		auto** const items = Menus::GetAllocator()->AllocateArray<Game::itemDef_s*>(maxItemsPerMenu);
		int itemCount = 0;
		bool isVisible = false;

		char token[512];

		for (;;)
		{
			SkipSpace(cursor);

			if (cursor.at >= cursor.end)
			{
				break;
			}

			if (*cursor.at == '}')
			{
				++cursor.at;
				break;
			}

			if (!NextToken(cursor, token, sizeof(token)))
			{
				break;
			}

			if (_stricmp(token, "itemDef") == 0)
			{
				if (itemCount >= maxItemsPerMenu)
				{
					Logger::Warning("menus: {} has more than {} items\n", menu->window.name, maxItemsPerMenu);
					SkipBraceBlock(cursor);
					continue;
				}

				auto* const item = Menus::GetAllocator()->Allocate<Game::itemDef_s>();

				const char* const itemBody = cursor.at;

				if (!ParseItem(cursor, item))
				{
					Menus::GetAllocator()->Free(item);
					break;
				}

				MenuDvarList::Parse(item, itemBody, cursor.at);

				item->parent = menu;
				items[itemCount] = item;
				++itemCount;
			}
			else if (_stricmp(token, "name") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				menu->window.name = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "rect") == 0 || _stricmp(token, "rect480") == 0)
			{
				ParseRect(cursor, &menu->window);
			}
			else if (_stricmp(token, "style") == 0)
			{
				menu->window.style = ReadInt(cursor);
			}
			else if (_stricmp(token, "border") == 0)
			{
				menu->window.border = ReadInt(cursor);
			}
			else if (_stricmp(token, "bordersize") == 0)
			{
				menu->window.borderSize = ReadFloat(cursor);
			}
			else if (_stricmp(token, "forecolor") == 0)
			{
				ParseColor(cursor, menu->window.foreColor);
				menu->window.dynamicFlags[0] |= static_cast<int>(Game::WINDOW_DYNAMIC_FORECOLOR_SET);
			}
			else if (_stricmp(token, "backcolor") == 0)
			{
				ParseColor(cursor, menu->window.backColor);
			}
			else if (_stricmp(token, "bordercolor") == 0)
			{
				ParseColor(cursor, menu->window.borderColor);
			}
			else if (_stricmp(token, "outlinecolor") == 0)
			{
				ParseColor(cursor, menu->window.outlineColor);
			}
			else if (_stricmp(token, "focuscolor") == 0)
			{
				ParseColor(cursor, menu->focusColor);
			}
			else if (_stricmp(token, "background") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				menu->window.background = ResolveMaterial(token);
			}
			else if (_stricmp(token, "cinematic") == 0)
			{
				NextToken(cursor, token, sizeof(token));

				char peek[64];
				const bool hasSound = PeekToken(cursor, peek, sizeof(peek)) && _stricmp(peek, "sound") == 0;

				if (hasSound)
				{
					NextToken(cursor, peek, sizeof(peek));
				}

				if (token[0] != '\0')
				{
					const std::lock_guard lock(cinematicsMutex);
					cinematics[menu] = { token, hasSound };
				}
			}
			else if (_stricmp(token, "ownerdraw") == 0)
			{
				menu->window.ownerDraw = ReadInt(cursor);
			}
			else if (_stricmp(token, "ownerdrawFlag") == 0)
			{
				menu->window.ownerDrawFlags |= ReadInt(cursor);
			}
			else if (_stricmp(token, "fullscreen") == 0)
			{
				menu->fullScreen = ReadInt(cursor);
			}
			else if (_stricmp(token, "screenSpace") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_SCREEN_SPACE);
			}
			else if (_stricmp(token, "decoration") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_DECORATION);
			}
			else if (_stricmp(token, "popup") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_POPUP);
			}
			else if (_stricmp(token, "outOfBoundsClick") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_OUT_OF_BOUNDS_CLICK);
			}
			else if (_stricmp(token, "legacySplitScreenScale") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_LEGACY_SPLITSCREEN_SCALE);
			}
			else if (_stricmp(token, "hiddenDuringScope") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_HIDDEN_DURING_SCOPE);
			}
			else if (_stricmp(token, "hiddenDuringFlashbang") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_HIDDEN_DURING_FLASHBANG);
			}
			else if (_stricmp(token, "hiddenDuringUI") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_HIDDEN_DURING_UI);
			}
			else if (_stricmp(token, "textOnlyFocus") == 0)
			{
				menu->window.staticFlags |= static_cast<int>(Game::WINDOW_STATIC_TEXT_ONLY_FOCUS);
			}
			else if (_stricmp(token, "blurWorld") == 0)
			{
				menu->blurRadius = ReadFloat(cursor);
			}
			else if (_stricmp(token, "fadeClamp") == 0)
			{
				menu->fadeClamp = ReadFloat(cursor);
			}
			else if (_stricmp(token, "fadeCycle") == 0)
			{
				menu->fadeCycle = ReadInt(cursor);
			}
			else if (_stricmp(token, "fadeAmount") == 0)
			{
				menu->fadeAmount = ReadFloat(cursor);
			}
			else if (_stricmp(token, "fadeInAmount") == 0)
			{
				menu->fadeInAmount = ReadFloat(cursor);
			}
			else if (_stricmp(token, "soundLoop") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				menu->soundName = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "allowedBinding") == 0)
			{
				NextToken(cursor, token, sizeof(token));
				menu->allowedBinding = Menus::GetAllocator()->DuplicateString(token);
			}
			else if (_stricmp(token, "onOpen") == 0)
			{
				AppendHandlerSet(cursor, &menu->onOpen);
			}
			else if (_stricmp(token, "onClose") == 0)
			{
				AppendHandlerSet(cursor, &menu->onClose);
			}
			else if (_stricmp(token, "onRequestClose") == 0)
			{
				AppendHandlerSet(cursor, &menu->onCloseRequest);
			}
			else if (_stricmp(token, "onESC") == 0)
			{
				AppendHandlerSet(cursor, &menu->onESC);
			}
			else if (_stricmp(token, "execKey") == 0)
			{
				ParseKeyHandler(cursor, true, &menu->onKey);
			}
			else if (_stricmp(token, "execKeyInt") == 0)
			{
				ParseKeyHandler(cursor, false, &menu->onKey);
			}
			else if (_stricmp(token, "visible") == 0)
			{
				char peek[64];

				if (PeekToken(cursor, peek, sizeof(peek)) && (_stricmp(peek, "when") == 0 || _stricmp(peek, "if") == 0))
				{
					NextToken(cursor, peek, sizeof(peek));
					menu->visibleExp = CompileParens(cursor, "visible when");
					isVisible = true;
				}
				else
				{
					isVisible = ReadInt(cursor) != 0;
				}
			}
			else if (_stricmp(token, "exp") == 0)
			{
				ParseMenuExpression(cursor, menu);
			}
		}

		menu->itemCount = itemCount;
		menu->items = items;

		if (isVisible)
		{
			menu->window.dynamicFlags[0] |= static_cast<int>(Game::WINDOW_DYNAMIC_VISIBLE);
		}
		requestedBorderSizes[&menu->window] = { menu->window.borderSize, menu->window.borderColor[3], menu->window.borderColor[3] };
		for (int index = 0; index < menu->itemCount; ++index)
		{
			if (menu->items[index])
			{
				const auto& window = menu->items[index]->window;
				requestedBorderSizes[&menu->items[index]->window] = { window.borderSize, window.borderColor[3], window.borderColor[3] };
			}
		}

		if (menu->window.name && _stricmp(menu->window.name, "menu_xboxlive_privatelobby") == 0)
		{
			for (int index = 0; index < menu->itemCount; ++index)
			{
				auto* item = menu->items[index];
				if (!item) continue;
				const bool isSaveBanner = (item->window.name
					&& (_stricmp(item->window.name, "btn_load_save") == 0 || _stricmp(item->window.name, "btn_load_save_bar") == 0))
					|| (item->text && (std::strcmp(item->text, "LOAD SAVE GAME") == 0 || std::strcmp(item->text, "View your saved progress") == 0));
				if (!isSaveBanner) continue;
				item->dvarTest = Menus::GetAllocator()->DuplicateString("ui_autosave_banner_visible");
				item->enableDvar = Menus::GetAllocator()->DuplicateString("\"1\"");
				item->dvarFlags |= Game::ITEM_DVAR_FLAG_SHOW;
			}
		}

		return menu;
	}

	Utils::Memory::Allocator* Menus::GetAllocator()
	{
		return &allocator;
	}

	Game::ExpressionSupportingData* Menus::GetSupportingData()
	{
		return &supportingData;
	}

	bool Menus::TryReadFile(const std::string& path, std::string* contents)
	{
		auto key = Utils::String::ToLower(path);
		std::ranges::replace(key, '\\', '/');
		if (isCachingMenuReads)
		{
			const auto cached = menuReadCache.find(key);
			if (cached != menuReadCache.end())
			{
				*contents = cached->second;
				return true;
			}
		}
		void* buffer = nullptr;
		const int length = Game::FS_ReadFile(path.data(), &buffer);

		if (length < 0 || !buffer)
		{
			return false;
		}

		if (static_cast<std::size_t>(length) > maxMenuFileSize)
		{
			Game::FS_FreeFile(buffer);
			Logger::Error("menus: {} is larger than {} bytes\n", path, maxMenuFileSize);
			return false;
		}

		contents->assign(static_cast<const char*>(buffer), static_cast<std::size_t>(length));
		Game::FS_FreeFile(buffer);
		if (isCachingMenuReads) menuReadCache.emplace(key, *contents);
		return true;
	}

	bool Menus::Link(Game::menuDef_t* menu, bool allowNew)
	{
		if (!menu->window.name)
		{
			return false;
		}

		Game::UiContext* const contexts[] = { Game::uiContext, Game::cgDC };
		bool didOverride = false;
		bool isInCgame = false;

		for (Game::UiContext* const context : contexts)
		{
			if (!context)
			{
				continue;
			}

			const int openCount = std::min(context->openMenuCount, static_cast<int>(ARRAYSIZE(context->menuStack)));

			for (int index = 0; index < openCount; ++index)
			{
				Game::menuDef_t* const open = context->menuStack[index];

				if (!open || !open->window.name || _stricmp(open->window.name, menu->window.name) != 0)
				{
					continue;
				}

				if (!overridden.contains(menu->window.name))
				{
					overridden[menu->window.name] = open;
				}

				context->menuStack[index] = menu;
			}

			const int linkedCount = std::min(context->menuCount, static_cast<int>(ARRAYSIZE(context->Menus)));

			for (int index = 0; index < linkedCount; ++index)
			{
				Game::menuDef_t* const linked = context->Menus[index];

				if (!linked || !linked->window.name || _stricmp(linked->window.name, menu->window.name) != 0)
				{
					continue;
				}

				if (!overridden.contains(menu->window.name))
				{
					overridden[menu->window.name] = linked;
				}

				context->Menus[index] = menu;
				didOverride = true;

				if (context == Game::cgDC)
				{
					isInCgame = true;
				}
			}
		}

		if (!didOverride && !allowNew)
		{
			return false;
		}

		if (!didOverride && !AppendMenu(Game::uiContext, menu))
		{
			return false;
		}

		if (!isInCgame && IsHudMenu(menu->window.name))
		{
			AppendMenu(Game::cgDC, menu);
		}

		return true;
	}

	bool Menus::AppendMenu(Game::UiContext* context, Game::menuDef_t* menu)
	{
		if (!context)
		{
			return false;
		}

		const int linkedCount = std::min(context->menuCount, static_cast<int>(ARRAYSIZE(context->Menus)));

		for (int index = 0; index < linkedCount; ++index)
		{
			if (context->Menus[index] == menu)
			{
				return true;
			}
		}

		if (context->menuCount < 0 || context->menuCount >= static_cast<int>(ARRAYSIZE(context->Menus)))
		{
			Logger::Error("menus: a context holds {} menus, {} cannot be added\n",
				context->menuCount, menu->window.name);
			return false;
		}

		context->Menus[context->menuCount] = menu;
		++context->menuCount;
		return true;
	}

	void Menus::Reset()
	{
		requestedBorderSizes.clear();
		menuReadCache.clear();
		if (Game::cgDC)
		{
			Game::cgDC->menuCount = 0;
			Game::cgDC->openMenuCount = 0;
		}

		SPLoadscreens::OnMenusFreed();

		loaded.clear();
		overridden.clear();
		deferred.clear();
		{
			const std::lock_guard lock(cinematicsMutex);
			cinematics.clear();
		}
		isIngameLoaded = false;
		allocator.Clear();
	}

	int Menus::Load(const std::string& path, bool allowNew, int* droppedCount)
	{
		if (droppedCount)
		{
			*droppedCount = 0;
		}

		Utils::MenuPreprocessor preprocessor(TryReadFile);
		std::string text;

		DefinePcOptionsCompatibility(preprocessor, path, TryReadFile);

		if (!preprocessor.Process(path, &text))
		{
			for (const auto& error : preprocessor.GetErrors())
			{
				Logger::Error("menus: {}\n", error);
			}

			if (text.empty())
			{
				return 0;
			}
		}

		Cursor cursor = { text.data(), text.data() + text.size() };
		int parsed = 0;
		int linked = 0;
		char token[256];

		for (;;)
		{
			SkipSpace(cursor);

			if (cursor.at >= cursor.end)
			{
				break;
			}

			if (*cursor.at == '{' || *cursor.at == '}')
			{
				++cursor.at;
				continue;
			}

			if (!NextToken(cursor, token, sizeof(token)))
			{
				break;
			}

			if (_stricmp(token, "menuDef") != 0)
			{
				continue;
			}

			Game::menuDef_t* const menu = ParseMenu(cursor);

			if (!menu)
			{
				continue;
			}

			++parsed;

			if (!Link(menu, allowNew || !IsHudMenu(menu->window.name)))
			{
				continue;
			}

			loaded[menu->window.name] = menu;
			++linked;
		}

		if (parsed == 0)
		{
			Logger::Error("menus: no menuDef in {}\n", path);
		}

		if (droppedCount)
		{
			*droppedCount = parsed - linked;
		}

		return linked;
	}

	std::vector<Game::menuDef_t*> Menus::LoadMenuByName_Recursive(const std::string& menu)
	{
		std::vector<Game::menuDef_t*> menus;

		Utils::MenuPreprocessor preprocessor(TryReadFile);
		std::string text;

		DefinePcOptionsCompatibility(preprocessor, menu, TryReadFile);

		if (!preprocessor.Process(menu, &text) && text.empty())
		{
			return menus;
		}

		Cursor cursor = { text.data(), text.data() + text.size() };
		char token[256];

		for (;;)
		{
			if (!NextToken(cursor, token, sizeof(token)) || token[0] == '}')
			{
				break;
			}

			if (_stricmp(token, "loadmenu") == 0)
			{
				NextToken(cursor, token, sizeof(token));

				for (auto* const childMenu : LoadMenuByName_Recursive(std::format("ui_mp\\{}.menu", token)))
				{
					menus.push_back(childMenu);
				}
			}
			else if (_stricmp(token, "menudef") == 0)
			{
				auto* const menuDef = ParseMenu(cursor);

				if (menuDef)
				{
					menus.push_back(menuDef);
				}
			}
		}

		return menus;
	}

	void Menus::LoadAll()
	{
		Reset();
		const MenuReadBatch readBatch;

		const std::vector<std::string> files = FileSystem::GetFileList("ui_mp", "menu");

		if (files.empty())
		{
			Logger::Error("menus: no ui_mp/*.menu on the search path, is " BASEGAME " deployed?\n");
			return;
		}

		int linked = 0;

		for (const std::string& file : files)
		{
			const std::string path = "ui_mp/" + file;

			const bool isCustom = std::any_of(custom.begin(), custom.end(), [&path](const std::string& entry)
				{
					return _stricmp(entry.data(), path.data()) == 0;
				});

			if (isCustom)
			{
				continue;
			}

			int dropped = 0;
			linked += Load(path, false, &dropped);

			if (dropped > 0)
			{
				deferred.push_back(path);
			}
		}

		for (const std::string& path : custom)
		{
			linked += Load(path, true);
		}

		std::string heldBack;

		for (const std::string& path : deferred)
		{
			heldBack.append(" ");
			heldBack.append(path);
		}

		if (heldBack.empty())
		{
			heldBack = " none";
		}

		Logger::Print("menus: {} of ours are live out of {} files, {} of them replacing a stock menu, held for ingame:{}\n",
			linked, files.size(), overridden.size(), heldBack);

		SPLoadscreens::PatchConnectMenu();
	}

	void Menus::LoadIngame()
	{
		if (isIngameLoaded)
		{
			for (const auto& entry : loaded)
			{
				Link(entry.second, true);
			}

			return;
		}

		const MenuReadBatch readBatch;
		int linked = 0;

		for (const std::string& path : deferred)
		{
			linked += Load(path, true);
		}

		const std::vector<std::string> files = FileSystem::GetFileList("ui_mp/scriptmenus", "menu");

		for (const std::string& file : files)
		{
			linked += Load("ui_mp/scriptmenus/" + file, true);
		}

		isIngameLoaded = true;

		Logger::Print("menus: {} more are live ingame, out of {} files the front end held back and {} in ui_mp/scriptmenus\n",
			linked, deferred.size(), files.size());
	}

	void Menus::Add(const std::string& path)
	{
		custom.push_back(path);
	}

	void Menus::UI_Init_Hook(int localClientNum)
	{
		reinterpret_cast<void(*)(int)>(uiInitHook.GetOriginal())(localClientNum);

		LoadAll();
	}

	int Menus::CL_InitCGame_Hook()
	{
		LoadIngame();

		return reinterpret_cast<int(*)()>(cgameInitHook.GetOriginal())();
	}

	Game::menuDef_t* Menus::Find(const std::string& name)
	{
		if (!Game::Menus_FindByName || !Game::uiContext)
		{
			return nullptr;
		}

		return Game::Menus_FindByName(Game::uiContext, name.data());
	}

	static MenuCinematic playing{};
	static bool isPlaying = false;
	static Utils::Hook volumeHook;

	static void UpdateMenuCinematic()
	{
		const std::unique_lock lock(cinematicsMutex, std::try_to_lock);

		if (!lock.owns_lock() || !Game::uiContext)
		{
			return;
		}

		if (Game::CL_GetLocalClientConnectionState(0) == Game::CA_CINEMATIC)
		{
			isPlaying = false;
			return;
		}

		const MenuCinematic* wanted = nullptr;
		const int openCount = std::min(Game::uiContext->openMenuCount, static_cast<int>(ARRAYSIZE(Game::uiContext->menuStack)));

		for (int index = openCount - 1; index >= 0 && !wanted; --index)
		{
			const auto entry = cinematics.find(Game::uiContext->menuStack[index]);

			if (entry != cinematics.end())
			{
				wanted = &entry->second;
			}
		}

		if (!wanted)
		{
			if (isPlaying)
			{
				Game::R_Cinematic_StopPlayback();
				isPlaying = false;
			}

			return;
		}

		unsigned int flags = cinematicLooping;

		if (!wanted->hasSound)
		{
			flags |= cinematicMuted;
		}

		const bool isCurrent = isPlaying && playing.name == wanted->name
			&& Utils::Hook::Get<unsigned int>(cinematicGlobRequestFlags) == flags;

		if (isCurrent)
		{
			return;
		}

		Game::R_Cinematic_StartPlayback(wanted->name.data(), flags, 0);
		playing = *wanted;
		isPlaying = true;
	}

	static void ApplyCinematicVolume()
	{
		reinterpret_cast<void(*)()>(volumeHook.GetOriginal())();

		const bool isMuted = (Utils::Hook::Get<unsigned int>(cinematicGlobThreadFlags) & cinematicMuted) != 0;

		if (!isMuted)
		{
			return;
		}

		using BinkSetSpeakerVolumes_t = void(*)(void* bink, unsigned int track, const unsigned int* speakers,
			const int* volumes, unsigned int count);

		void* const bink = Utils::Hook::Get<void*>(cinematicGlobBink);
		const auto setSpeakerVolumes = Utils::Hook::Get<BinkSetSpeakerVolumes_t>(BinkSetSpeakerVolumesImport);
		const int silence[8]{};

		for (unsigned int track = 0; track < 5; ++track)
		{
			setSpeakerVolumes(bink, track, nullptr, silence, 8);
		}
	}

	constexpr std::uintptr_t clsState = 0x1406CECF8;
	constexpr std::uintptr_t uiActiveMenu = 0x1466436D8;

	static bool isMenuDebug = false;

	void Menus::ReportOpenMenus()
	{
		if (!isMenuDebug || !Game::uiContext)
		{
			return;
		}

		static int lastOpen = -1;
		static int lastActive = -1;

		const int open = Game::uiContext->openMenuCount;
		const int active = Utils::Hook::Get<int>(uiActiveMenu);

		if (open == lastOpen && active == lastActive)
		{
			return;
		}

		lastOpen = open;
		lastActive = active;

		std::string stack;

		for (int i = 0; i < open && i < static_cast<int>(ARRAYSIZE(Game::uiContext->menuStack)); ++i)
		{
			const Game::menuDef_t* const menu = Game::uiContext->menuStack[i];

			if (menu && menu->window.name)
			{
				stack += menu->window.name;
				stack += " ";
			}
		}

		Logger::Print("menudebug: client state {}, active menu {}, {} open: {}\n",
			Utils::Hook::Get<int>(clsState), active, open, stack);
	}

	static std::string CallChain()
	{
		const auto imageBase = reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
		const auto* const dosHeader = reinterpret_cast<const IMAGE_DOS_HEADER*>(imageBase);
		const auto* const ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS*>(imageBase + dosHeader->e_lfanew);
		const std::uintptr_t imageEnd = imageBase + ntHeaders->OptionalHeader.SizeOfImage;

		void* frames[16]{};
		const auto frameCount = RtlCaptureStackBackTrace(0, ARRAYSIZE(frames), frames, nullptr);

		std::string chain;
		int written = 0;

		for (USHORT i = 0; i < frameCount && written < 8; ++i)
		{
			const auto address = reinterpret_cast<std::uintptr_t>(frames[i]);

			if (address < imageBase || address >= imageEnd)
			{
				continue;
			}

			chain += std::format("{:X} ", address - imageBase + 0x140000000);
			++written;
		}

		return chain;
	}

	static std::string OpenMenuNames(const Game::UiContext* context)
	{
		std::string names;

		for (int i = 0; i < context->openMenuCount && i < static_cast<int>(ARRAYSIZE(context->menuStack)); ++i)
		{
			const Game::menuDef_t* const menu = context->menuStack[i];

			if (menu && menu->window.name)
			{
				names += menu->window.name;
				names += " ";
			}
		}

		return names;
	}

	static bool SeatWatch(const std::uintptr_t* sites, const std::uint8_t(*bytes)[5], std::size_t count,
		Utils::Hook* hooks, void* replacement)
	{
		for (std::size_t i = 0; i < count; ++i)
		{
			if (!Utils::Hook::MatchesBytes(sites[i], bytes[i], sizeof(bytes[i])))
			{
				return false;
			}
		}

		bool isSeated = true;

		for (std::size_t i = 0; i < count; ++i)
		{
			const bool asJump = bytes[i][0] == 0xE9;
			isSeated = hooks[i].Initialize(sites[i], replacement, asJump)->Install()->IsInstalled() && isSeated;
		}

		if (!isSeated)
		{
			for (std::size_t i = 0; i < count; ++i)
			{
				hooks[i].Uninstall();
			}

			return false;
		}

		for (std::size_t i = 0; i < count; ++i)
		{
			hooks[i].Quick();
		}

		return true;
	}

	void Menus::Menus_Open_Hook(Game::UiContext* context, Game::menuDef_t* menu)
	{
		if (isMenuDebug)
		{
			Logger::Print("menudebug: open {} in client state {}, from {}\n",
				(menu && menu->window.name) ? menu->window.name : "(unnamed)", Utils::Hook::Get<int>(clsState), CallChain());
		}

		reinterpret_cast<void(*)(Game::UiContext*, Game::menuDef_t*)>(menusOpenHooks[0].GetOriginal())(context, menu);
	}

	Game::menuDef_t* Menus::Menus_OpenByName_Find_Hook(Game::UiContext* context, const char* name)
	{
		auto* const menu = reinterpret_cast<Game::menuDef_t*(*)(Game::UiContext*, const char*)>(findForOpenHook.GetOriginal())(context, name);

		if (isMenuDebug && !menu)
		{
			Logger::Print("menudebug: open {} in client state {} found no such menu, from {}\n",
				name ? name : "(null)", Utils::Hook::Get<int>(clsState), CallChain());
		}

		return menu;
	}

	void Menus::Menus_CloseAll_Hook(Game::UiContext* context)
	{
		if (isMenuDebug && context && context->openMenuCount > 0)
		{
			Logger::Print("menudebug: close all of {}in client state {}, from {}\n",
				OpenMenuNames(context), Utils::Hook::Get<int>(clsState), CallChain());
		}

		reinterpret_cast<void(*)(Game::UiContext*)>(closeAllHooks[0].GetOriginal())(context);
	}

	std::uintptr_t Menus::Menus_CloseRequest_Hook(Game::UiContext* context, Game::menuDef_t* menu)
	{
		if (isMenuDebug)
		{
			Logger::Print("menudebug: close {} in client state {}, from {}\n",
				(menu && menu->window.name) ? menu->window.name : "(unnamed)", Utils::Hook::Get<int>(clsState), CallChain());
		}

		return reinterpret_cast<std::uintptr_t(*)(Game::UiContext*, Game::menuDef_t*)>(closeRequestHooks[0].GetOriginal())(context, menu);
	}

	void Menus::Cbuf_AddText_Response_Hook(int localClientNum, const char* text)
	{
		if (isMenuDebug && text)
		{
			std::string_view line(text);

			while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
			{
				line.remove_suffix(1);
			}

			Logger::Print("menudebug: send \"{}\" in client state {}, from {}\n",
				line, Utils::Hook::Get<int>(clsState), CallChain());
		}

		reinterpret_cast<void(*)(int, const char*)>(responseHooks[0].GetOriginal())(localClientNum, text);
	}

	void Menus::WatchMenuOpens()
	{
		if (!SeatWatch(Menus_OpenCalls, menusOpenCallBytes, std::size(Menus_OpenCalls), menusOpenHooks, reinterpret_cast<void*>(Menus_Open_Hook)))
		{
			Logger::Error("menus: could not seat the Menus_Open watch, so menudebug cannot name what opens a menu\n");
		}

		if (!SeatWatch(&Menus_OpenByName_FindCall, &findForOpenCallBytes, 1, &findForOpenHook, reinterpret_cast<void*>(Menus_OpenByName_Find_Hook)))
		{
			Logger::Error("menus: could not seat the Menus_OpenByName watch, so menudebug cannot report an open that finds nothing\n");
		}

		if (!SeatWatch(Menus_CloseAllCalls, closeAllCallBytes, std::size(Menus_CloseAllCalls), closeAllHooks, reinterpret_cast<void*>(Menus_CloseAll_Hook)))
		{
			Logger::Error("menus: could not seat the Menus_CloseAll watch, so menudebug cannot name what closes a menu\n");
		}

		if (!SeatWatch(Menus_CloseRequestCalls, closeRequestCallBytes, std::size(Menus_CloseRequestCalls), closeRequestHooks, reinterpret_cast<void*>(Menus_CloseRequest_Hook)))
		{
			Logger::Error("menus: could not seat the Menus_CloseRequest watch, so menudebug cannot name what closes a menu\n");
		}

		if (!SeatWatch(menuResponseCalls, menuResponseCallBytes, std::size(menuResponseCalls), responseHooks, reinterpret_cast<void*>(Cbuf_AddText_Response_Hook)))
		{
			Logger::Error("menus: could not seat the menu response watch, so menudebug cannot show what we answer the server\n");
		}
	}

	void Menus::MenuDebug_f()
	{
		isMenuDebug = !isMenuDebug;
		Logger::Print("menu open logging {}\n", isMenuDebug ? "on" : "off");
	}

	void Menus::LoadMenu_f(const Command::Params* params)
	{
		if (params->Size() < 2)
		{
			Logger::Print("usage: loadmenu <ui_mp/name.menu>\n");
			return;
		}

		Logger::Print("menus: {} linked from {}\n", Load(params->Get(1)), params->Get(1));
	}

	void Menus::OpenMenu_f(const Command::Params* params)
	{
		if (params->Size() < 2)
		{
			Logger::Print("usage: openmenu <name>\n");
			return;
		}

		if (!Game::Menus_OpenByName || !Game::uiContext)
		{
			Logger::Error("menus: the ui context is not bound yet\n");
			return;
		}

		const Game::dvar_t* const cl_ingame = *Game::cl_ingame;

		if (cl_ingame && cl_ingame->current.enabled)
		{
			Game::Key_SetCatcher(0, Game::KEYCATCH_UI);
		}

		if (!Game::Menus_OpenByName(Game::uiContext, params->Get(1)))
		{
			Logger::Error("menus: no menu named {} is registered\n", params->Get(1));
		}
	}

	void Menus::ListMenus_f(const Command::Params* params)
	{
		Game::UiContext* const context = Game::uiContext;

		if (!context)
		{
			Logger::Error("menus: the ui context is not bound yet\n");
			return;
		}

		const std::string filter = params->Size() > 1 ? Utils::String::ToLower(params->Get(1)) : "";

		for (int index = 0; index < context->menuCount && index < static_cast<int>(ARRAYSIZE(context->Menus)); ++index)
		{
			Game::menuDef_t* const menu = context->Menus[index];

			if (!menu || !menu->window.name)
			{
				continue;
			}

			const std::string name = Utils::String::ToLower(menu->window.name);

			if (!filter.empty() && name.find(filter) == std::string::npos)
			{
				continue;
			}

			const char* origin = "";

			if (loaded.contains(menu->window.name))
			{
				origin = overridden.contains(menu->window.name) ? " [ours, over stock]" : " [ours]";
			}

			Logger::Print("{}: {} ({} items){}\n", index, menu->window.name, menu->itemCount, origin);
		}

		Logger::Print("menus: {} registered, {} of them ours, {} of those over a stock menu\n",
			context->menuCount, loaded.size(), overridden.size());
	}

	void Menus::PointLobbyStatesAtMainText()
	{
		for (const auto& lea : xboxLiveMenuNameLeas)
		{
			if (!Utils::Hook::IsLeaIntact(lea))
			{
				Logger::Warning("menus: a lobby state does not name the live hub, left alone, so the live menus can open over a map load\n");
				return;
			}
		}

		if (!Utils::Hook::TryPointLeasAt(xboxLiveMenuNameLeas, Utils::Hook::PlaceNearImage("main_text")))
		{
			Logger::Warning("menus: no room beside the image for the menu name, the lobby states are left alone\n");
		}
	}

	Menus::Menus()
	{
		Command::Add("loadmenu", LoadMenu_f);
		Command::Add("openmenu", OpenMenu_f);
		Command::Add("listmenus", ListMenus_f);
		Command::Add("menudebug", MenuDebug_f);

		isMenuDebug = Flags::HasFlag("menudebug");

		Scheduler::Loop(ReportOpenMenus, Scheduler::Pipeline::RENDERER);

		if (!Utils::Hook::MatchesBytes(UI_DrawMapLevelshot_MenusOpenCall, menusOpenCall, sizeof(menusOpenCall)))
		{
			Logger::Error("menus: the loading screen's Menus_Open call does not read as expected, left alone, so the loading screen can stay up over a match\n");
		}
		else if (levelshotOpenHook.Initialize(UI_DrawMapLevelshot_MenusOpenCall, UI_DrawMapLevelshot_Open_Hook, HOOK_CALL)->Install()->IsInstalled())
		{
			levelshotOpenHook.Quick();
		}
		else
		{
			Logger::Error("menus: could not hook the loading screen's Menus_Open call, nopped instead, so no server motd shows while loading\n");
			Utils::Hook::Nop(UI_DrawMapLevelshot_MenusOpenCall, sizeof(menusOpenCall));
		}

		const bool isConnectAnswered = AssetHandler::OnFind(Game::ASSET_TYPE_MENU, [](unsigned int, const std::string& name) -> void*
		{
			std::string shortName = name;
			const std::size_t slash = shortName.find_last_of("/\\");

			if (slash != std::string::npos)
			{
				shortName = shortName.substr(slash + 1);
			}

			if (shortName.length() > 5 && _stricmp(shortName.data() + shortName.length() - 5, ".menu") == 0)
			{
				shortName.resize(shortName.length() - 5);
			}

			const auto found = loaded.find(shortName);

			if (found == loaded.end())
			{
				return nullptr;
			}

			return found->second;
		});

		const bool canSeatPaint = isConnectAnswered && Utils::Hook::BranchesTo(Menu_Paint_IsVisibleCall, Menu_IsVisible, HOOK_CALL);

		if (canSeatPaint && paintVisibleHook.Initialize(Menu_Paint_IsVisibleCall, Menu_Paint_IsVisible_Hook, HOOK_CALL)->Install()->IsInstalled())
		{
			paintVisibleHook.Quick();
		}
		else
		{
			Logger::Error("menus: could not hook Menu_Paint's visibility test, a stock connect menu can still draw\n");
		}

		if (!Dedicated::IsEnabled())
		{
			Scheduler::Once([]
			{
				const std::lock_guard lock(loadingMutex);

				mapname = Dvar::Var("mapname");
				zw3_ui_loading_start_time = Dvar::Register("zw3_ui_loading_start_time", 0, 0, std::numeric_limits<int>::max(), Game::DVAR_INIT, "Loading screen animation start time");
				zw3_ui_loading_progress = Dvar::Register("zw3_ui_loading_progress", 0.0f, 0.0f, 1.0f, Game::DVAR_INIT, "Loading screen progress");
				zw3_ui_loading_visible = Dvar::Register("zw3_ui_loading_visible", false, Game::DVAR_INIT, "Loading screen progress visibility");

				const Game::StringTable* didYouKnow = nullptr;
				Game::StringTable_GetAsset("mp/didyouknow.csv", &didYouKnow);
			}, Scheduler::Pipeline::MAIN);

			Scheduler::Loop([]
			{
				UpdateLoadingProgress(true);
				UpdateNewsCarousel();
			}, Scheduler::Pipeline::MAIN);

			Scheduler::Loop([]
			{
				UpdateLoadingProgress(false);
			}, Scheduler::Pipeline::RENDERER);

			Scheduler::Once([]
			{
				const int indexMax = std::numeric_limits<int>::max();

				zw3_ui_news_index = Dvar::Register("zw3_ui_news_index", 0, 0, indexMax, Game::DVAR_INTERNAL, "Current ZW3 news carousel item");
				zw3_ui_news_count = Dvar::Register("zw3_ui_news_count", 0, 0, indexMax, Game::DVAR_INTERNAL, "Current ZW3 news carousel item count");
				zw3_ui_news_progress = Dvar::Register("zw3_ui_news_progress", 0.0f, 0.0f, 1.0f, Game::DVAR_INTERNAL, "Current ZW3 news carousel progress");
				zw3_ui_news_hover = Dvar::Register("zw3_ui_news_hover", false, Game::DVAR_INTERNAL, "ZW3 news carousel hover state");
				zw3_ui_news_title = Dvar::Register("zw3_ui_news_title", "", Game::DVAR_INTERNAL, "Current ZW3 news title");
				zw3_ui_news_body = Dvar::Register("zw3_ui_news_body", "", Game::DVAR_INTERNAL, "Current ZW3 news body");
				zw3_ui_news_counter = Dvar::Register("zw3_ui_news_counter", "0 / 0", Game::DVAR_INTERNAL, "Current ZW3 news counter");
				zw3_ui_news_image = Dvar::Register("zw3_ui_news_image", "", Game::DVAR_INTERNAL, "Unused/internal ZW3 news image marker");
				zw3_ui_news_has_image = Dvar::Register("zw3_ui_news_has_image", false, Game::DVAR_INTERNAL, "Current ZW3 news image availability");
				zw3_ui_news_loading = Dvar::Register("zw3_ui_news_loading", true, Game::DVAR_INTERNAL, "Current ZW3 news loading state");
				zw3_ui_news_page = Dvar::Register("zw3_ui_news_page", 0, 0, indexMax, Game::DVAR_INTERNAL, "Current ZW3 news thumbnail page");

				for (std::size_t slot = 0; slot < std::size(newsTileTitleNames); ++slot)
				{
					Game::Dvar_SetFromStringByName(newsTileTitleNames[slot], "");
					newsTileTitles[slot] = Dvar::Var(newsTileTitleNames[slot]);
				}
			}, Scheduler::Pipeline::MAIN);

			UIScript::Add("RefreshNews", [](const UIScript::Token&)
			{
				RefreshNews();
			});

			UIScript::Add("OpenNews", [](const UIScript::Token&)
			{
				OpenNews();
			});

			for (int slot = 0; slot < newsPageSize; ++slot)
			{
				UIScript::Add(std::format("SelectNewsSlot{}", slot), [slot](const UIScript::Token&)
				{
					SelectNewsSlot(slot);
				});
			}

			UIScript::Add("NewsPrevPage", [](const UIScript::Token&)
			{
				ShowNewsPage(std::max(0, zw3_ui_news_page.Get<int>() - newsPageSize));
			});

			UIScript::Add("NewsNextPage", [](const UIScript::Token&)
			{
				int page = zw3_ui_news_page.Get<int>();

				if (page + newsPageSize < static_cast<int>(newsItems.size()))
				{
					page += newsPageSize;
				}

				ShowNewsPage(page);
			});

			Scheduler::OnGameInitialized(BeginNewsFetch, Scheduler::Pipeline::MAIN);
		}

		if (Utils::Hook::MatchesBytes(Com_InitHunkMemory_ReserveSize - 1, hunkSizes, sizeof(hunkSizes)))
		{
			Utils::Hook::Set<std::uint32_t>(Com_InitHunkMemory_ReserveSize, hunkSize);
			Utils::Hook::Set<std::uint32_t>(Com_InitHunkMemory_TotalSize, hunkSize);
		}
		else
		{
			Logger::Error("menus: Com_InitHunkMemory does not read as expected, the hunk stays 10 MB\n");
		}

		PointLobbyStatesAtMainText();
		WatchMenuOpens();

		if (!Dedicated::IsEnabled())
		{
			if (!Utils::Hook::MatchesBytes(R_Cinematic_StartPlayback_NowVolumeCall, volumeCallBytes, sizeof(volumeCallBytes)))
			{
				Logger::Error("menus: R_Cinematic_StartPlayback_Now does not read as expected, menu cinematics are off\n");
			}
			else if (!volumeHook.Initialize(R_Cinematic_StartPlayback_NowVolumeCall, ApplyCinematicVolume, HOOK_CALL)->Install()->IsInstalled())
			{
				Logger::Error("menus: could not hook R_Cinematic_StartPlayback_Now, menu cinematics are off\n");
			}
			else
			{
				volumeHook.Quick();
				Scheduler::Loop(UpdateMenuCinematic, Scheduler::Pipeline::RENDERER);
			}
		}

		if (!uiInitHook.Initialize(UI_InitCall, UI_Init_Hook, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("menus: could not hook UI_Init, nothing will load on its own\n");
			return;
		}

		uiInitHook.Quick();

		if (!cgameInitHook.Initialize(CL_InitCGameTailCall, CL_InitCGame_Hook, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("menus: could not hook CL_InitCGame, the ingame menus will not load\n");
			return;
		}

		cgameInitHook.Quick();

		Add("ui_mp/changelog.menu");
		Add("ui_mp/iw4x_credits.menu");
		Add("ui_mp/menu_first_launch.menu");
		Add("ui_mp/mod_download_popmenu.menu");
		Add("ui_mp/pc_options_game.menu");
		Add("ui_mp/pc_options_gamepad.menu");
		Add("ui_mp/pc_options_gamepad_advanced.menu");
		Add("ui_mp/pc_options_gamepad_sticks.menu");
		Add("ui_mp/pc_options_gamepad_triggers.menu");
		Add("ui_mp/pc_options_gamepad_response.menu");
		Add("ui_mp/pc_options_gamepad_feedback.menu");
		Add("ui_mp/pc_options_gamepad_timing.menu");
		Add("ui_mp/pc_options_multi.menu");
		Add("ui_mp/popup_customclan.menu");
		Add("ui_mp/popup_customtitle.menu");
		Add("ui_mp/popup_friends.menu");
		Add("ui_mp/resetclass.menu");
		Add("ui_mp/security_increase_popmenu.menu");
		Add("ui_mp/startup_messages.menu");
		Add("ui_mp/stats_reset.menu");
		Add("ui_mp/stats_unlock.menu");
		Add("ui_mp/stats_mod_warning.menu");
		Add("ui_mp/theater_menu.menu");

		Add("ui_mp/pc_options_interface.menu");
		Add("ui_mp/pc_options_network.menu");

		Add("ui_mp/connect.menu");
		Add("ui_mp/popup_partyconnect.menu");
		Add("ui_mp/popup_partyconnect_warning.menu");
		Add("ui_mp/popup_autosave.menu");
		Add("ui_mp/zw3changelog.menu");
		Add("ui_mp/popup_zwnet_connecting.menu");
		Add("ui_mp/zwnet_matchmaking.menu");
		Add("ui_mp/popup_zwnet_playlists.menu");
		Add("ui_mp/popup_zwnet_player_card.menu");
		Add("ui_mp/menu_quest_challenges.menu");
		Add("ui_mp/popup_upnp.menu");

		const auto cloneMenuEntry = DB_DynamicCloneXAssetHandler + Game::ASSET_TYPE_MENU * sizeof(std::uintptr_t);

		if (Utils::Hook::Get<std::uintptr_t>(cloneMenuEntry) == Utils::Hook::Rebase(DB_DynamicCloneMenu))
		{
			Utils::Hook::Set<std::uintptr_t>(cloneMenuEntry, 0);
		}
		else
		{
			Logger::Error("menus: the menu clone handler is not DB_DynamicCloneMenu, left alone\n");
		}

		if (Utils::Hook::BranchesTo(UI_AddMenuList_FindCall, DB_FindXAssetHeader, HOOK_CALL))
		{
			Utils::Hook::Nop(UI_AddMenuList_FindCall, 5);
		}
		else
		{
			Logger::Error("menus: UI_AddMenuList's menu lookup is not where it was, left alone\n");
		}
	}

	bool Menus::Menu_Paint_IsVisible_Hook(Game::UiContext* context, Game::menuDef_t* menu)
	{
		if (menu)
		{
			const float pixelSize = 480.0f / static_cast<float>(std::max(1, Renderer::Height()));
			const auto updateBorder = [pixelSize](Game::windowDef_t& window)
			{
				const auto requested = requestedBorderSizes.find(&window);
				if (requested != requestedBorderSizes.end() && requested->second.size > 0.0f)
				{
					auto& style = requested->second;
					if (window.borderColor[3] != style.appliedAlpha) style.alpha = window.borderColor[3];
					window.borderSize = std::max(pixelSize, style.size);
					style.appliedAlpha = style.alpha * std::clamp(style.size / pixelSize, 0.5f, 1.0f);
					window.borderColor[3] = style.appliedAlpha;
				}
			};
			updateBorder(menu->window);
			for (int index = 0; index < menu->itemCount; ++index)
			{
				if (menu->items[index]) updateBorder(menu->items[index]->window);
			}
		}
		const bool hasName = menu && menu->window.name;
		const bool isHeldForScene = hasName && (_stricmp(menu->window.name, "main_text") == 0
			|| _stricmp(menu->window.name, "pregame_loaderror") == 0
			|| _stricmp(menu->window.name, "menu_xboxlive_privatelobby") == 0
			|| _stricmp(menu->window.name, "zwnet_matchmaking") == 0);
		if (isHeldForScene && LobbyScene::IsStartupLoading()) return false;

		if (LobbyScene::IsTransitionActive())
		{
			return false;
		}

		const bool isConnect = menu && menu->window.name && _stricmp(menu->window.name, "connect") == 0;

		if (isConnect)
		{
			const auto diskConnect = loaded.find("connect");
			const bool hasDiskConnect = diskConnect != loaded.end() && diskConnect->second;

			if (hasDiskConnect && menu != diskConnect->second)
			{
				return false;
			}
		}

		return Game::Menu_IsVisible(context, menu);
	}

	void Menus::UI_DrawMapLevelshot_Open_Hook(Game::UiContext*, Game::menuDef_t*)
	{
		const std::string motd = Party::GetMotd();
		const bool isTargetConnected = Party::Target() == Network::Address(Game::clc_serverAddress);

		if (!motd.empty() && isTargetConnected)
		{
			Game::Dvar_SetFromStringByName("didyouknow", motd.data());
		}
	}

	void Menus::ForceOnlyCustomConnectMenu()
	{
		const auto diskConnect = loaded.find("connect");

		if (diskConnect == loaded.end() || !diskConnect->second)
		{
			return;
		}

		Game::menuDef_t* const connect = diskConnect->second;
		Game::UiContext* const contexts[] = { Game::uiContext, Game::cgDC };

		for (Game::UiContext* const context : contexts)
		{
			if (!context)
			{
				continue;
			}

			const bool isCountSane = context->menuCount >= 0 && context->menuCount <= static_cast<int>(ARRAYSIZE(context->Menus))
				&& context->openMenuCount >= 0 && context->openMenuCount <= static_cast<int>(ARRAYSIZE(context->menuStack));

			if (!isCountSane)
			{
				continue;
			}

			RemoveMenuNameFromContext(context, "connect", connect);

			Game::menuDef_t** const linkedEnd = context->Menus + context->menuCount;
			const bool isLinked = std::find(context->Menus, linkedEnd, connect) != linkedEnd;

			if (!isLinked && context->menuCount < static_cast<int>(ARRAYSIZE(context->Menus)))
			{
				context->Menus[context->menuCount] = connect;
				++context->menuCount;
			}
		}
	}

	void Menus::UpdateLoadingProgress(const bool isMainThread)
	{
		const std::unique_lock lock(loadingMutex, std::try_to_lock);

		if (!lock.owns_lock())
		{
			return;
		}

		if (LobbyScene::IsTransitionActive())
		{
			zw3_ui_loading_visible.Set(false);
			zw3_ui_loading_progress.Set(0.0f);
			return;
		}

		static Game::connstate_t lastConnState = Game::CA_DISCONNECTED;
		static std::string lastMapName;
		static bool wasLoading = false;
		static bool wasConnectMenuVisible = false;
		static int lastUpdateTime = 0;
		static bool isSessionActive = false;
		static int loadingStartTime = 0;

		const int now = Game::Sys_Milliseconds();

		if (!lastUpdateTime)
		{
			lastUpdateTime = now;
		}

		const float deltaSeconds = std::clamp(static_cast<float>(now - lastUpdateTime) / 1000.0f, 0.0f, 0.1f);
		lastUpdateTime = now;

		const Game::connstate_t connState = Game::CL_GetLocalClientConnectionState(0);
		const std::string currentMapName = mapname.Get<std::string>();
		const bool isLoading = connState >= Game::CA_CONNECTING && connState < Game::CA_ACTIVE;

		if (isMainThread && (isLoading || lastConnState >= Game::CA_CONNECTING))
		{
			ForceOnlyCustomConnectMenu();
		}

		Game::menuDef_t* const connectMenu = Game::Menus_FindByName(Game::uiContext, "connect");
		const bool isConnectMenuVisible = connectMenu && Game::Menu_IsVisible(Game::uiContext, connectMenu);

		const bool didConnectMenuOpen = !wasConnectMenuVisible && isConnectMenuVisible;
		const bool didStartLoading = !wasLoading && isLoading;
		const bool isNewConnection = lastConnState < Game::CA_CONNECTING && connState >= Game::CA_CONNECTING;
		const bool isMapRestart = lastConnState >= Game::CA_ACTIVE && connState < Game::CA_ACTIVE && connState > Game::CA_DISCONNECTED;
		const bool didMapChange = !lastMapName.empty() && !currentMapName.empty() && lastMapName != currentMapName;

		const bool shouldStartSession = (!isSessionActive && (didConnectMenuOpen || didStartLoading)) || isMapRestart || didMapChange;

		if (shouldStartSession)
		{
			isSessionActive = true;
			loadingStartTime = 0;

			zw3_ui_loading_start_time.Set(now);
			zw3_ui_loading_progress.Set(0.0f);
			zw3_ui_loading_visible.Set(true);

			if (isMainThread)
			{
				ForceOnlyCustomConnectMenu();
			}

			const Game::StringTable* table = nullptr;
			Game::StringTable_GetAsset("mp/didyouknow.csv", &table);

			if (table && table->rowCount > 0)
			{
				static std::mt19937 random(std::random_device{}());
				std::uniform_int_distribution<int> rows(0, table->rowCount - 1);
				const char* const tip = Game::StringTable_GetColumnValueForRow(table, rows(random), 0);

				if (tip && *tip)
				{
					Game::Dvar_SetFromStringByName("didyouknow", tip);
				}
			}
		}

		if (isConnectMenuVisible || isLoading)
		{
			int startTime = zw3_ui_loading_start_time.Get<int>();

			if (!startTime)
			{
				startTime = now;
			}

			const int totalElapsed = std::max(0, now - startTime);
			const float current = zw3_ui_loading_progress.Get<float>();

			float target = 0.05f;
			float rate = 0.60f;

			if (connState < Game::CA_CONNECTING)
			{
				const float serverFraction = std::clamp(static_cast<float>(totalElapsed) / 2200.0f, 0.0f, 1.0f);
				target = EaseOutCubic(serverFraction) * 0.28f;
				rate = 0.40f;
			}
			else if (connState < Game::CA_LOADING)
			{
				target = 0.32f;
				rate = 1.20f;
			}
			else if (connState == Game::CA_LOADING)
			{
				if (!loadingStartTime)
				{
					loadingStartTime = now;
				}

				const int loadingElapsed = std::max(0, now - loadingStartTime);
				const float zoneFraction = std::clamp(FastFiles::GetFullLoadedFraction(), 0.0f, 1.0f);
				const float timeFraction = EaseOutQuad(std::clamp(static_cast<float>(loadingElapsed) / 4800.0f, 0.0f, 1.0f));
				const float loadFraction = std::max(zoneFraction, timeFraction * 0.88f);

				target = std::clamp(0.32f + loadFraction * (0.94f - 0.32f), 0.32f, 0.94f);
				rate = std::clamp((target - current) * 3.5f, 0.30f, 2.2f);
			}
			else if (connState == Game::CA_PRIMED)
			{
				target = 0.98f;
				rate = 4.0f;
			}
			else if (connState >= Game::CA_ACTIVE)
			{
				target = 1.0f;
				rate = 8.0f;
			}

			float ceiling = 0.995f;

			if (connState >= Game::CA_ACTIVE)
			{
				ceiling = 1.0f;
			}

			const float maxStep = rate * deltaSeconds;
			const float next = current + std::clamp(target - current, 0.0f, maxStep);

			zw3_ui_loading_progress.Set(std::clamp(std::max(current, next), 0.0f, ceiling));
			zw3_ui_loading_visible.Set(true);
		}
		else
		{
			isSessionActive = false;
			loadingStartTime = 0;
			zw3_ui_loading_progress.Set(0.0f);
			zw3_ui_loading_visible.Set(false);
		}

		if (connState >= Game::CA_CONNECTING)
		{
			const std::string motd = Party::GetMotd();
			const bool isTargetConnected = Party::Target() == Network::Address(Game::clc_serverAddress);

			if (!motd.empty() && isTargetConnected)
			{
				Game::Dvar_SetFromStringByName("didyouknow", motd.data());
			}
		}

		if (didStartLoading || isNewConnection || isMapRestart)
		{
			Game::Dvar_SetFromStringByName("zw3_ui_sb_survived_time", "00:00:00");
		}

		lastConnState = connState;
		lastMapName = currentMapName;
		wasLoading = isLoading;
		wasConnectMenuVisible = isConnectMenuVisible;
	}

	Game::menuDef_t* Menus::FindDiskMenu(const std::string& name)
	{
		const auto found = loaded.find(name);

		if (found == loaded.end())
		{
			return nullptr;
		}

		return found->second;
	}

	void Menus::OpenLoadingScreen()
	{
		if (LobbyScene::IsTransitionActive())
		{
			return;
		}

		const auto diskConnect = loaded.find("connect");

		if (diskConnect == loaded.end() || !diskConnect->second)
		{
			return;
		}

		ForceOnlyCustomConnectMenu();

		Game::UiContext* const contexts[] = { Game::uiContext, Game::cgDC };

		for (Game::UiContext* const context : contexts)
		{
			if (context)
			{
				Game::Menus_OpenByName(context, "connect");
			}
		}
	}

	void Menus::CloseLoadingScreen()
	{
		const auto diskConnect = loaded.find("connect");

		if (diskConnect == loaded.end() || !diskConnect->second)
		{
			return;
		}

		Game::menuDef_t* const connect = diskConnect->second;
		Game::UiContext* const contexts[] = { Game::uiContext, Game::cgDC };

		for (Game::UiContext* const context : contexts)
		{
			if (!context)
			{
				continue;
			}

			const int openCount = std::clamp(context->openMenuCount, 0, static_cast<int>(ARRAYSIZE(context->menuStack)));
			Game::menuDef_t** const openEnd = context->menuStack + openCount;

			if (std::find(context->menuStack, openEnd, connect) != openEnd)
			{
				Game::Menus_CloseRequest(context, connect);
			}
		}
	}

	Menus::~Menus()
	{
		loaded.clear();
		overridden.clear();
		{
			const std::lock_guard lock(cinematicsMutex);
			cinematics.clear();
		}
		allocator.Clear();
	}
}
