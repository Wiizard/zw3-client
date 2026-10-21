#include "STDInclude.hpp"

#include "Menu.hpp"

#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "Core.hpp"
#include "Culling.hpp"
#include "DebugDraw.hpp"
#include "Engine.hpp"
#include "FixedFunction.hpp"
#include "Fog.hpp"
#include "Lights.hpp"
#include "Look.hpp"
#include "MapEditor.hpp"
#include "MapSettings.hpp"
#include "MotionTracker.hpp"
#include "Options.hpp"
#include "Outdoor.hpp"
#include "PbrMaterials.hpp"
#include "Profiler.hpp"
#include "Scene.hpp"
#include "Settings.hpp"
#include "Sky.hpp"
#include "TextureTags.hpp"
#include "Triggers.hpp"
#include "Weather.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Components::Remix::Menu
{
	static bool isReady = false;
	static bool isOpen = false;
	static bool isRemixMenuOpen = false;
	static bool wasRemixChord = false;
	static IDirect3DDevice9* deviceIdentity = nullptr;
	static std::string settingsMessage;

	void UnhookWindow();

	static WNDPROC originalFunction = nullptr;
	static HWND hookedWindow = nullptr;
	static HHOOK messageHook = nullptr;

	static std::atomic<bool> menuOwnsInput{ false };

	struct QueuedMessage
	{
		HWND hwnd;
		UINT message;
		WPARAM wparam;
		LPARAM lparam;
	};

	static std::mutex queueMutex;
	static std::vector<QueuedMessage> queue;

	constexpr std::size_t maxQueued = 512;

	static bool ImGuiWants(UINT message)
	{
		if ((message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
			|| (message >= WM_KEYFIRST && message <= WM_KEYLAST))
		{
			return true;
		}

		switch (message)
		{
		case WM_MOUSELEAVE:
		case WM_NCMOUSELEAVE:
		case WM_SETFOCUS:
		case WM_KILLFOCUS:
		case WM_INPUTLANGCHANGE:
		case WM_CHAR:
			return true;
		default:
			return false;
		}
	}

	static bool IsRelease(UINT message)
	{
		switch (message)
		{
		case WM_KEYUP:
		case WM_SYSKEYUP:
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_XBUTTONUP:
			return true;

		default:
			return false;
		}
	}

	static bool ShouldSwallow(UINT message)
	{
		if (!menuOwnsInput.load(std::memory_order_relaxed))
		{
			return false;
		}

		if (IsRelease(message))
		{
			return false;
		}

		if (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST)
		{
			return true;
		}

		if (message >= WM_KEYFIRST && message <= WM_KEYLAST)
		{
			return true;
		}

		return false;
	}

	static LRESULT CALLBACK WndProcDetour(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
	{

		if (ImGuiWants(message))
		{
			std::lock_guard<std::mutex> lock(queueMutex);

			if (queue.size() >= maxQueued)
			{
				queue.erase(queue.begin());
			}

			queue.push_back({ hwnd, message, wparam, lparam });
		}

		if (ShouldSwallow(message))
		{
			return TRUE;
		}

		return CallWindowProcA(originalFunction, hwnd, message, wparam, lparam);
	}

	static void QueueMessage(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
	{
		std::lock_guard<std::mutex> lock(queueMutex);

		if (queue.size() >= maxQueued)
		{
			queue.erase(queue.begin());
		}

		queue.push_back({ hwnd, message, wparam, lparam });
	}

	static LRESULT CALLBACK GetMessageHook(int code, WPARAM wparam, LPARAM lparam)
	{
		if (code == HC_ACTION && wparam == PM_REMOVE && lparam)
		{
			MSG* message = reinterpret_cast<MSG*>(lparam);

			const bool isOurWindow = message->hwnd == hookedWindow || IsChild(hookedWindow, message->hwnd);

			if (isOurWindow && ImGuiWants(message->message))
			{
				QueueMessage(message->hwnd, message->message, message->wParam, message->lParam);
			}

			if (isOurWindow && ShouldSwallow(message->message))
			{
				message->message = WM_NULL;
			}
		}

		return CallNextHookEx(messageHook, code, wparam, lparam);
	}

	static bool InstallMessageHook(HWND window)
	{
		const DWORD thread = GetWindowThreadProcessId(window, nullptr);

		messageHook = SetWindowsHookExA(WH_GETMESSAGE, &GetMessageHook, nullptr, thread);

		if (!messageHook)
		{
			LogError() << "overlay: could not hook the game window's message queue, GetLastError=" << GetLastError();
			return false;
		}

		hookedWindow = window;

		Log() << "overlay: reading input from the game window's message queue (Remix mode: Remix replaces the"
				 " window procedure when the device resets)";

		return true;
	}

	struct PolledKey
	{
		int virtualKey;
		ImGuiKey key;
	};

	static const PolledKey polledKeys[] = {
		{ VK_INSERT, ImGuiKey_Insert },
		{ VK_END, ImGuiKey_End },
		{ VK_NEXT, ImGuiKey_PageDown },
		{ VK_PRIOR, ImGuiKey_PageUp },
		{ VK_HOME, ImGuiKey_Home },
		{ VK_F5, ImGuiKey_F5 },
		{ VK_F6, ImGuiKey_F6 },
		{ VK_F7, ImGuiKey_F7 },
		{ VK_F8, ImGuiKey_F8 },
		{ VK_F9, ImGuiKey_F9 },
		{ VK_F10, ImGuiKey_F10 },
		{ VK_F11, ImGuiKey_F11 },
		{ VK_ESCAPE, ImGuiKey_Escape },
		{ VK_RETURN, ImGuiKey_Enter },
		{ VK_TAB, ImGuiKey_Tab },
		{ VK_BACK, ImGuiKey_Backspace },
		{ VK_DELETE, ImGuiKey_Delete },
		{ VK_SPACE, ImGuiKey_Space },
		{ VK_LEFT, ImGuiKey_LeftArrow },
		{ VK_RIGHT, ImGuiKey_RightArrow },
		{ VK_UP, ImGuiKey_UpArrow },
		{ VK_DOWN, ImGuiKey_DownArrow },
		{ VK_LCONTROL, ImGuiKey_LeftCtrl },
		{ VK_RCONTROL, ImGuiKey_RightCtrl },
		{ VK_LSHIFT, ImGuiKey_LeftShift },
		{ VK_RSHIFT, ImGuiKey_RightShift },
		{ VK_LMENU, ImGuiKey_LeftAlt },
		{ VK_RMENU, ImGuiKey_RightAlt },
	};

	static bool wasDown[IM_ARRAYSIZE(polledKeys)] = {};
	static bool wasControl = false;
	static bool wasShift = false;
	static bool wasAlt = false;

	static bool IsHeld(int virtualKey)
	{
		return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
	}

	static void SendModifier(ImGuiIO& io, ImGuiKey modifier, bool isDown, bool& wasModifierDown)
	{
		if (isDown == wasModifierDown)
		{
			return;
		}

		io.AddKeyEvent(modifier, isDown);
		wasModifierDown = isDown;
	}

	void PollKeyboard()
	{
		if (!hookedWindow || GetForegroundWindow() != GetAncestor(hookedWindow, GA_ROOT))
		{
			return;
		}

		ImGuiIO& io = ImGui::GetIO();

		for (int i = 0; i < IM_ARRAYSIZE(polledKeys); ++i)
		{
			const bool isDown = IsHeld(polledKeys[i].virtualKey);

			if (isDown != wasDown[i])
			{
				io.AddKeyEvent(polledKeys[i].key, isDown);
				wasDown[i] = isDown;
			}
		}

		SendModifier(io, ImGuiMod_Ctrl, IsHeld(VK_CONTROL), wasControl);
		SendModifier(io, ImGuiMod_Shift, IsHeld(VK_SHIFT), wasShift);
		SendModifier(io, ImGuiMod_Alt, IsHeld(VK_MENU), wasAlt);
	}

	void DrainInput()
	{
		std::vector<QueuedMessage> pending;

		{
			std::lock_guard<std::mutex> lock(queueMutex);
			pending.swap(queue);
		}

		for (const QueuedMessage& queued : pending)
		{
			ImGui_ImplWin32_WndProcHandler(queued.hwnd, queued.message, queued.wparam, queued.lparam);
		}
	}

	void SetMenuOwnsInput(bool shouldOwnInput)
	{
		menuOwnsInput.store(shouldOwnInput, std::memory_order_relaxed);
	}

	bool HookWindow(HWND window)
	{
		if ((originalFunction || messageHook) && hookedWindow == window)
		{
			return true;
		}

		if (originalFunction || messageHook)
		{
			UnhookWindow();
		}

		if (!window || !IsWindow(window))
		{
			return false;
		}

		if (Core::IsRemixMode())
		{
			return InstallMessageHook(window);
		}

		originalFunction = reinterpret_cast<WNDPROC>(
			SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WndProcDetour)));

		if (!originalFunction)
		{
			LogError() << "overlay: could not subclass the game window, GetLastError="
					   << GetLastError();
			return false;
		}

		hookedWindow = window;

		Log() << "overlay: window subclassed";

		return true;
	}

	void UnhookWindow()
	{
		if (messageHook)
		{
			UnhookWindowsHookEx(messageHook);
			messageHook = nullptr;
		}

		if (originalFunction && hookedWindow && IsWindow(hookedWindow))
		{
			SetWindowLongPtrA(hookedWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalFunction));
		}

		originalFunction = nullptr;
		hookedWindow = nullptr;

		menuOwnsInput.store(false, std::memory_order_relaxed);

		std::lock_guard<std::mutex> lock(queueMutex);
		queue.clear();
	}

	enum class CheckState
	{
		Off,
		Working,
		Problem,
	};

	struct CheckRow
	{
		const char* name;
		CheckState state;
		std::string detail;
	};

	static std::string Format(const char* format, ...)
	{
		char text[1024];

		va_list arguments;
		va_start(arguments, format);
		std::vsnprintf(text, sizeof(text), format, arguments);
		va_end(arguments);

		return text;
	}

	static std::vector<CheckRow> CollectRemixChecks()
	{
		const Core::Config& config = Core::GetConfig();
		std::vector<CheckRow> rows;

		const RemixStats remixStats = GetStats();

		if (!Core::IsRemixLoaded())
		{
			rows.push_back({ "RTX Remix", CheckState::Problem, Format(
				"the runtime did not load from %s (see the console)", config.remixDll.c_str()) });
		}
		else if (!remixStats.isHookInstalled)
		{
			rows.push_back({ "RTX Remix", CheckState::Problem,
				"runtime loaded, but the camera hook is not installed (unmapped build, or see the log)" });
		}
		else if (remixStats.cameraFrames == 0)
		{
			rows.push_back({ "RTX Remix", CheckState::Problem, "runtime loaded and hooked, no 3D scene drawn yet" });
		}
		else
		{
			rows.push_back({ "RTX Remix", CheckState::Working, Format(
				"runtime loaded, camera handed over %llu times (%llu other views), frame composited before"
				" the HUD %llu times, %d of 8 engine patches applied, effect alpha hooked %d, heartbeat sensor"
				" clipped %d; ALT+X opens Remix",
				remixStats.cameraFrames, remixStats.skippedViews, remixStats.injectionFrames,
				remixStats.suppressedPasses, static_cast<int>(remixStats.isEffectAlphaHooked),
				static_cast<int>(remixStats.isMotionTrackerHooked)) });
		}

		const LightStats lightStats = GetLightStats();

		if (!lightStats.isApiReady)
		{
			rows.push_back({ "Remix lights", CheckState::Problem,
				"the Remix API is not ready (see the console), Remix uses its fallback light" });
		}
		else if (!lightStats.hasSun)
		{
			rows.push_back({ "Remix lights", CheckState::Working, Format(
				"API ready, no sun on this map or stage, %d map lights, %d fixture lights (up to 384)", lightStats.mapLights,
				lightStats.fixtureLights) });
		}
		else
		{
			rows.push_back({ "Remix lights", CheckState::Working, Format(
				"sun radiance %.2f %.2f %.2f, %d map lights, %d fixture lights (up to 384), %d dynamic lights now (%llu updates), submitted %llu"
				" frames",
				lightStats.sunRadiance[0], lightStats.sunRadiance[1], lightStats.sunRadiance[2],
				lightStats.mapLights, lightStats.fixtureLights, lightStats.dynamicLights, lightStats.dynamicLightUpdates,
				lightStats.submittedFrames) });
		}

		const FogStats fogStats = GetFogStats();

		if (!fogStats.hasMapFog || !fogStats.isEnabled)
		{
			rows.push_back({ "Remix fog", CheckState::Working, Format(
				"no map fog in use (map without fog, r_fog 0 or Map fog off), Remix keeps the light haze from"
				" rtx.conf, %d updates", fogStats.submissions) });
		}
		else
		{
			rows.push_back({ "Remix fog", CheckState::Working, Format(
				"map fog from %.0f units, halfway %.0f, max opacity %.2f, colour %.2f %.2f %.2f, sun fog %d;"
				" Remix haze up to %.0f%% over %.1f m steps, albedo %.2f %.2f %.2f, anisotropy %.2f, %d updates",
				fogStats.start, fogStats.halfway, fogStats.maxOpacity, fogStats.colour[0], fogStats.colour[1],
				fogStats.colour[2], static_cast<int>(fogStats.hasSunFog), fogStats.farOpacity * 100.0f,
				fogStats.distanceMeters, fogStats.albedo[0], fogStats.albedo[1], fogStats.albedo[2],
				fogStats.anisotropy, fogStats.submissions) });
		}

		const OutdoorStats outdoorStats = GetOutdoorStats();

		if (!outdoorStats.hasMap || !outdoorStats.isEnabled)
		{
			rows.push_back({ "Remix rain mask", CheckState::Working, Format(
				"off (no readable outdoor map on this map, or the toggle is off), rain and snow draw everywhere;"
				" %d outdoor cloud materials seen", outdoorStats.maskedMaterials) });
		}
		else
		{
			double culledPercent = 0.0;

			if (outdoorStats.cloudsTested > 0)
			{
				culledPercent = 100.0 * static_cast<double>(outdoorStats.cloudsCulled)
					/ static_cast<double>(outdoorStats.cloudsTested);
			}

			rows.push_back({ "Remix rain mask", CheckState::Working, Format(
				"outdoor map %dx%d (format %u), %d outdoor cloud materials, %llu cloud draws tested, %llu skipped"
				" under cover (%.0f%%), largest cloud scale %.0f",
				outdoorStats.width, outdoorStats.height, outdoorStats.format, outdoorStats.maskedMaterials,
				outdoorStats.cloudsTested, outdoorStats.cloudsCulled, culledPercent, outdoorStats.largestScale) });
		}

		const SkyStats skyStats = GetSkyStats();

		rows.push_back({ "Remix sky", CheckState::Working, Format(
			"sky brightness %.2f (map sky luminance %.3f, read from the sky cube %d, sun elevation %.2f, sky share"
			" of outdoor light %.0f%%), %d updates", skyStats.brightness, skyStats.skyLuminance,
			static_cast<int>(skyStats.didReadSky), skyStats.sunElevation, skyStats.hazeShare * 100.0f,
			skyStats.submissions) });

		const WeatherStats weatherStats = GetWeatherStats();

		rows.push_back({ "Remix weather", CheckState::Working, Format(
			"%llu clouds rebuilt as quads (%llu spark or unreadable clouds left to the engine), %llu flakes drawn,"
			" %llu dropped under cover, %llu faded out near the eye or far away", weatherStats.cloudsDrawn,
			weatherStats.cloudsPassedToEngine, weatherStats.particlesDrawn, weatherStats.particlesUnderCover,
			weatherStats.particlesFaded) });

		const TextureTagStats tagStats = GetTextureTagStats();

		if (tagStats.passes == 0)
		{
			rows.push_back({ "Remix tags", CheckState::Problem, "no texture tagging pass yet" });
		}
		else
		{
			rows.push_back({ "Remix tags", CheckState::Working, Format(
				"%d materials read, %d colour maps without a hash; tagged sky %d, world UI %d, beam %d, decal %d,"
				" particle %d, water %d, UI %d, ignore %d; %d self-lit surfaces glowing; %d restored from earlier"
				" sessions at start-up; %d stale saved tags withdrawn (zw3_remix_textures.conf)",
				tagStats.materials, tagStats.texturesWithoutHash, tagStats.tagged[0], tagStats.tagged[1],
				tagStats.tagged[2], tagStats.tagged[3], tagStats.tagged[4], tagStats.tagged[5], tagStats.tagged[6],
				tagStats.tagged[7], tagStats.selfLit, tagStats.restored, tagStats.withdrawn) });
		}

		const WorldStats worldStats = GetWorldStats();

		if (!worldStats.isHookInstalled)
		{
			rows.push_back({ "Remix world", CheckState::Problem,
				"the world draw hook is not installed (unmapped build, or see the log), the map stays on"
				" vertex capture" });
		}
		else if (worldStats.builds == 0)
		{
			rows.push_back({ "Remix world", CheckState::Problem, "hooked, no map geometry built yet" });
		}
		else
		{
			const char* brushModelState = "not hooked";

			if (worldStats.isBrushModelHookInstalled)
			{
				brushModelState = "hooked";
			}

			const char* skinnedModelState = "not hooked";

			if (worldStats.isSkinnedModelHookInstalled)
			{
				skinnedModelState = "hooked";
			}

			rows.push_back({ "Remix world", CheckState::Working, Format(
				"fixed-function map: %u vertices, %u indices, %u surfaces in the last scene; %llu lists drawn,"
				" %llu sky lists sent to Remix as sky, %llu left to the engine (other views); brush models %s,"
				" %llu lists; %llu terrain blend layers drawn with plain alpha; static models: %llu lists, %u"
				" surfaces built; entity models: %llu lists; skinned models %s, %llu lists, %llu cached surfaces"
				" skipped; %llu scrolling lists; %u camo textures built; %llu cut-out decal lists blended; %llu"
				" dynamic light lists skipped; %u material batches, %llu batch draws; %llu shader tint lists"
				" skipped; %u static model batches from %u baked props, %llu prop batch draws",
				worldStats.vertexCount, worldStats.indexCount, worldStats.lastViewSurfaces,
				worldStats.fixedFunctionLists, worldStats.skyLists, worldStats.passedThroughLists,
				brushModelState, worldStats.brushModelLists, worldStats.blendLayerLists,
				worldStats.staticModelLists, worldStats.staticModelSurfaces, worldStats.entityModelLists,
				skinnedModelState, worldStats.skinnedModelLists, worldStats.cachedSkinnedSurfaces,
				worldStats.scrollingLists, worldStats.camoTextures, worldStats.cutoutDecalLists,
				worldStats.dynamicLightLists, worldStats.materialBatches, worldStats.batchDraws,
				worldStats.shaderTintLists, worldStats.staticModelBatches, worldStats.bakedStaticModels,
				worldStats.staticBatchDraws) });
		}

		rows.push_back({ "Remix prop lists", CheckState::Working, Format(
			"filter hooked %d; last frame %u baked props kept to trigger their batches, %u left out of the"
			" engine's static model lists so big models fit; %u map and %u prop batches the engine culled were"
			" filled in last frame", static_cast<int>(worldStats.isPropListFilterHooked),
			worldStats.keptBakedProps, worldStats.hiddenBakedProps, worldStats.completedWorldBatches,
			worldStats.completedPropBatches) });

		const PbrStats pbrStats = GetPbrStats();

		rows.push_back({ "Remix PBR", CheckState::Working, Format(
			"%d materials in the PBR mod (rtx-remix\\mods\\zw3_pbr), %d to convert on this map, %d converted"
			" this session, %d failed, %d publishes, %d camo aliases, %d glass, %d water, %d glowing, %d skin,"
			" %d foliage, worker running %d", pbrStats.indexedMaterials, pbrStats.candidateMaterials,
			pbrStats.convertedMaterials, pbrStats.failedMaterials, pbrStats.publishes, pbrStats.aliases,
			pbrStats.glassMaterials, pbrStats.waterMaterials, pbrStats.emissiveMaterials, pbrStats.skinMaterials,
			pbrStats.foliageMaterials, static_cast<int>(pbrStats.isWorkerRunning)) });

		const CullingStats cullingStats = GetCullingStats();
		const std::string engineWarnings = DescribeEngineWarnings();
		CheckState cullingState = CheckState::Working;

		if (cullingStats.appliedPatches == 0)
		{
			cullingState = CheckState::Problem;
		}

		rows.push_back({ "Remix culling", cullingState, Format(
			"level %d, %d of 4 fixed patches, radius walk %d, static model LOD %d; engine warnings fired (4 entity"
			" lighting, 13 smodel lighting, 14 too many smodels, 17 surface list full, 51 worker queue): %s",
			static_cast<int>(cullingStats.level), cullingStats.appliedPatches,
			static_cast<int>(cullingStats.isRadiusHooked), static_cast<int>(cullingStats.isLodHooked),
			engineWarnings.c_str()) });

		return rows;
	}

	static void DrawStatusTab()
	{
		if (ImGui::Button("Dump frame"))
		{
			RequestSceneDump();
		}

		ImGui::SetItemTooltip("Writes every world and model draw group of the next 3D frame to"
							  " zw3_remix_frame_N.txt. Press it while something looks wrong, then once more"
							  " while it looks right.");

		ImGui::Separator();

		for (const CheckRow& row : CollectRemixChecks())
		{
			ImVec4 colour(0.6f, 0.6f, 0.6f, 1.0f);

			if (row.state == CheckState::Working)
			{
				colour = ImVec4(0.4f, 1.0f, 0.5f, 1.0f);
			}
			else if (row.state == CheckState::Problem)
			{
				colour = ImVec4(1.0f, 0.85f, 0.4f, 1.0f);
			}

			ImGui::TextColored(colour, "%s", row.name);
			ImGui::Indent();
			ImGui::TextWrapped("%s", row.detail.c_str());
			ImGui::Unindent();
		}
	}

	static void DrawLightingTab()
	{
		LookSettings look = GetLookSettings();
		bool isChanged = false;
		ImGui::SeparatorText("Sky");

		float skyScale = GetSkyScale();

		if (ImGui::SliderFloat("Sky brightness", &skyScale, 0.0f, 4.0f, "x%.2f"))
		{
			SetSkyScale(skyScale);
		}

		ImGui::SetItemTooltip("Scales the light and the visible sky the map's sky gives. 0 is a black sky with no sky"
							  " light, so only the sun and lamps light the map.");

		isChanged |= ImGui::Checkbox("Sky follows the world camera", &look.isSkyReprojected);
		ImGui::SetItemTooltip("For maps whose sky slides with the view like a picture: Remix redraws the sky through the world camera (rtx.skyReprojectToMainCameraSpace).");

		ImGui::SeparatorText("Sun");

		SunSettings sunSettings = GetSunSettings();
		bool isSunChanged = false;

		if (ImGui::SliderFloat("Sun brightness", &sunSettings.brightness, 0.0f, 8.0f, "x%.2f"))
		{
			isSunChanged = true;
		}

		ImGui::SetItemTooltip("Scales the map's sun. The sky dims with it.");

		if (ImGui::Checkbox("Cap sun brightness", &sunSettings.isCapped))
		{
			isSunChanged = true;
		}

		ImGui::SetItemTooltip("Limits only the maps whose sun is brighter than the cap, keeping its colour.");

		if (sunSettings.isCapped)
		{
			if (ImGui::SliderFloat("Sun cap", &sunSettings.maxRadiance, 0.0f, 40.0f, "%.2f"))
			{
				isSunChanged = true;
			}

			ImGui::SetItemTooltip("Highest sun radiance allowed on any channel. The Remix lights row on the Status tab"
								  " list shows the current sun.");
		}

		if (ImGui::ColorEdit3("Sun tint", sunSettings.tint, ImGuiColorEditFlags_NoInputs))
		{
			isSunChanged = true;
		}

		ImGui::SetItemTooltip("Multiplies the map's sun colour. White leaves it as the map made it.");

		const bool wasDirectionOverridden = sunSettings.isDirectionOverridden;

		if (ImGui::Checkbox("Override sun direction", &sunSettings.isDirectionOverridden))
		{
			isSunChanged = true;

			const LightStats sunStats = GetLightStats();

			if (!wasDirectionOverridden && sunStats.hasSun)
			{
				const float radToDeg = 180.0f / 3.14159265f;
				sunSettings.pitchDeg = std::asin(std::clamp(-sunStats.sunDirection[2], -1.0f, 1.0f)) * radToDeg;
				sunSettings.yawDeg = std::atan2(-sunStats.sunDirection[1], -sunStats.sunDirection[0]) * radToDeg;
			}
		}

		ImGui::SetItemTooltip("Moves the sun. Shadows will no longer line up with the map's baked lighting.");

		if (sunSettings.isDirectionOverridden)
		{
			if (ImGui::SliderFloat("Sun yaw", &sunSettings.yawDeg, -180.0f, 180.0f, "%.0f deg"))
			{
				isSunChanged = true;
			}

			if (ImGui::SliderFloat("Sun height", &sunSettings.pitchDeg, -10.0f, 90.0f, "%.0f deg"))
			{
				isSunChanged = true;
			}
		}

		if (ImGui::Button("Reset sun"))
		{
			sunSettings = SunSettings{};
			isSunChanged = true;
		}

		if (isSunChanged)
		{
			SetSunSettings(sunSettings);
		}

		ImGui::SeparatorText("Lights");

		float lightBrightness = GetLightBrightness();

		if (ImGui::SliderFloat("Light brightness", &lightBrightness, 0.0f, 8.0f, "x%.2f"))
		{
			SetLightBrightness(lightBrightness);
		}

		ImGui::SetItemTooltip("Scales the lights the map places and the lights effects cast: fire, glowsticks,"
							  " muzzle flashes and explosions.");

		isChanged |= ImGui::Checkbox("Lit fixtures", &look.isFixtureLights);
		ImGui::SetItemTooltip("Lamps, fluorescent tubes, signs and screens glow from their bright parts and each cluster"
							  " gets a small real light, so rooms are lit by their fixtures.");

		if (look.isFixtureLights)
		{
			isChanged |= ImGui::SliderFloat("Fixture brightness", &look.fixtureBrightness, 0.0f, 4.0f, "x%.2f");
		}
		isChanged |= ImGui::Checkbox("Laser light", &look.isLaserLights);
		ImGui::SetItemTooltip("The glow along a laser beam becomes a real red light that lights walls and players."
							  " The game draws it as a screen effect Remix cannot see.");

		ImGui::SeparatorText("Time and night");

		isChanged |= ImGui::Checkbox("Time of day", &look.isTimeOfDay);
		ImGui::SetItemTooltip("Moves the sun through the day from the map's own sun direction. Low sun is warm and"
							  " casts long shadows; the sky dims with it. Overrides the sun direction on the Scene tab.");

		if (look.isTimeOfDay)
		{
			isChanged |= ImGui::SliderFloat("Hour", &look.timeOfDayHours, 5.0f, 19.0f, "%.1f h");
			ImGui::SetItemTooltip("6 is sunrise, 12 is noon, 18 is sunset. 17 to 18 is golden hour.");
		}
		isChanged |= ImGui::Checkbox("Night mode", &look.isNight);
		ImGui::SetItemTooltip("No sun and a near black sky. Only the map's lamps, muzzle flashes, tracers and explosions"
							  " light the scene. Lower Brightest adapt under Exposure for a darker night.");

		if (look.isNight)
		{
			isChanged |= ImGui::SliderFloat("Night sky", &look.nightSky, 0.0f, 0.3f, "%.3f");
			ImGui::SetItemTooltip("Moonlight: how much light the sky still gives.");
		}

		if (isChanged)
		{
			SetLookSettings(look);
		}
	}

	static void DrawExposureTab()
	{
		LookSettings look = GetLookSettings();
		bool isChanged = false;
		ImGui::SeparatorText("Exposure");

		isChanged |= ImGui::Checkbox("Auto exposure", &look.isAutoExposure);
		ImGui::SetItemTooltip("Remix adapts the exposure like an eye or camera. Off holds a fixed exposure, so dark"
							  " areas stay dark and bright ones stay bright.");

		isChanged |= ImGui::SliderFloat("Darkest adapt (EV)", &look.exposureMinEv, -8.0f, 0.0f, "%.1f");
		ImGui::SetItemTooltip("How far auto exposure may darken the image in bright scenes.");

		isChanged |= ImGui::SliderFloat("Brightest adapt (EV)", &look.exposureMaxEv, 0.0f, 10.0f, "%.1f");
		ImGui::SetItemTooltip("How far auto exposure may brighten dark scenes. Lower keeps night and indoor areas dark.");

		isChanged |= ImGui::SliderFloat("Adapt speed", &look.exposureSpeed, 0.1f, 10.0f, "%.1f");
		ImGui::SetItemTooltip("How quickly exposure follows the scene, like eyes adjusting after going outside.");

		ImGui::SeparatorText("Tonemapping");

		static const char* tonemappers[] = { "Global (colour grading)", "Local (default)" };
		isChanged |= ImGui::Combo("Tonemapper", &look.tonemapper, tonemappers, 2);
		ImGui::SetItemTooltip("Local balances bright and dark areas separately, like an HDR photo. Global is a single"
							  " curve for the whole image and is the one that does contrast, saturation and tint.");

		if (look.tonemapper == 1)
		{
			isChanged |= ImGui::SliderFloat("Exposure", &look.localExposure, 0.0f, 3.0f, "%.2f");
			ImGui::SetItemTooltip("Overall brightness on top of auto exposure.");

			isChanged |= ImGui::SliderFloat("Shadows", &look.localShadows, 0.0f, 6.0f, "%.2f");
			ImGui::SetItemTooltip("Higher lifts the shadows. Lower keeps them deep, closer to a real camera.");

			isChanged |= ImGui::SliderFloat("Highlights", &look.localHighlights, 0.0f, 8.0f, "%.2f");
			ImGui::SetItemTooltip("Higher pulls bright areas down so skies and lamps keep detail.");

			isChanged |= ImGui::Checkbox("Boost local contrast", &look.isLocalContrastBoosted);
			ImGui::SetItemTooltip("Sharper, punchier detail in surfaces.");

			isChanged |= ImGui::Checkbox("Film curve (ACES)", &look.isLocalAces);
			ImGui::SetItemTooltip("Finishes with the ACES film curve used in film and many games.");
		}
		else
		{
			isChanged |= ImGui::SliderFloat("Exposure (EV)", &look.exposureBiasEv, -5.0f, 5.0f, "%.2f");
			ImGui::SetItemTooltip("Brightens or darkens the whole image in camera stops.");

			isChanged |= ImGui::Checkbox("Film curve (ACES)", &look.isGlobalAces);
			ImGui::SetItemTooltip("Finishes with the ACES film curve used in film and many games.");

			isChanged |= ImGui::Checkbox("Colour grading", &look.isColourGrading);
			ImGui::SetItemTooltip("Turns on the contrast, saturation and tint below.");

			if (look.isColourGrading)
			{
				isChanged |= ImGui::SliderFloat("Contrast", &look.contrast, 0.0f, 3.0f, "%.2f");

				isChanged |= ImGui::SliderFloat("Saturation", &look.saturation, 0.0f, 5.0f, "%.2f");
				ImGui::SetItemTooltip("0 is black and white, 1 is the original colour, above 1 is oversaturated.");

				isChanged |= ImGui::ColorEdit3("Colour tint", look.colourBalance, ImGuiColorEditFlags_NoInputs);
				ImGui::SetItemTooltip("Tints the final image, white leaves it neutral. Warm for golden hour, cool"
									  " for night.");
			}
		}

		ImGui::SeparatorText("Camera effects");

		isChanged |= ImGui::Checkbox("Bloom", &look.isBloom);

		if (look.isBloom)
		{
			isChanged |= ImGui::SliderFloat("Bloom strength", &look.bloomIntensity, 0.0f, 4.0f, "%.2f");

			isChanged |= ImGui::SliderFloat("Bloom threshold", &look.bloomThreshold, 0.0f, 4.0f, "%.2f");
			ImGui::SetItemTooltip("Only light brighter than this glows.");
		}

		isChanged |= ImGui::Checkbox("Vignette", &look.isVignette);

		if (look.isVignette)
		{
			isChanged |= ImGui::SliderFloat("Vignette darkness", &look.vignetteIntensity, 0.0f, 1.0f, "%.2f");
			isChanged |= ImGui::SliderFloat("Vignette size", &look.vignetteRadius, 0.0f, 2.0f, "%.2f");
			isChanged |= ImGui::SliderFloat("Vignette softness", &look.vignetteSoftness, 0.0f, 1.0f, "%.2f");
		}

		isChanged |= ImGui::Checkbox("Chromatic aberration", &look.isChromaticAberration);

		if (look.isChromaticAberration)
		{
			isChanged |= ImGui::SliderFloat("Aberration amount", &look.chromaticAmount, 0.0f, 0.1f, "%.3f");
		}

		isChanged |= ImGui::Checkbox("Motion blur", &look.isMotionBlur);

		if (isChanged)
		{
			SetLookSettings(look);
		}
	}

	static void DrawAtmosphereTab()
	{
		LookSettings look = GetLookSettings();
		bool isChanged = false;
		ImGui::SeparatorText("Fog and haze");

		const FogStats fogStats = GetFogStats();
		bool isMapFogEnabled = fogStats.isEnabled;

		if (ImGui::Checkbox("Map fog", &isMapFogEnabled))
		{
			SetMapFogEnabled(isMapFogEnabled);
		}

		ImGui::SetItemTooltip("Drives Remix's volumetric haze from the fog each map sets. Off keeps the light haze"
							  " from rtx.conf.");

		float fogStrength = fogStats.strength;

		if (ImGui::SliderFloat("Fog strength", &fogStrength, 0.0f, 6.0f, "x%.2f"))
		{
			SetMapFogStrength(fogStrength);
		}

		ImGui::SetItemTooltip("Scales the map fog's density and how opaque the far distance gets.");

		isChanged |= ImGui::Checkbox("Volumetric haze", &look.isHaze);
		ImGui::SetItemTooltip("Light shafts and haze in the air.");
		isChanged |= ImGui::Checkbox("God rays", &look.isGodRays);
		ImGui::SetItemTooltip("Thicker air that scatters sunlight forward, so light shafts show through windows, trees"
							  " and gaps. Turns volumetric haze on.");

		if (look.isGodRays)
		{
			isChanged |= ImGui::SliderFloat("God ray strength", &look.godRayStrength, 1.0f, 6.0f, "x%.1f");
			ImGui::SetItemTooltip("How dense the air is and how brightly the sun lights it.");
		}

		ImGui::SeparatorText("Weather");

		bool isWeatherEnabled = IsWeatherEnabled();

		if (ImGui::Checkbox("Weather (rain, snow, dust)", &isWeatherEnabled))
		{
			SetWeatherEnabled(isWeatherEnabled);
		}

		ImGui::SetItemTooltip("Draws the map's rain, snow and dust particle clouds. Off removes them entirely.");

		bool isRainMaskEnabled = GetOutdoorStats().isEnabled;

		if (ImGui::Checkbox("Rain and snow only outdoors", &isRainMaskEnabled))
		{
			SetRainMaskEnabled(isRainMaskEnabled);
		}

		ImGui::SetItemTooltip("Skips rain and snow clouds that the map's outdoor height map puts under a roof,"
							  " which the game does in a pixel shader Remix never runs.");

		isChanged |= ImGui::Checkbox("Rain on any map", &look.isRainEverywhere);
		ImGui::SetItemTooltip("Path traced rain streaks around you on every map, kept outdoors by the map's rain mask.");

		if (look.isRainEverywhere)
		{
			isChanged |= ImGui::SliderFloat("Rain amount", &look.rainAmount, 0.0f, 10.0f, "%.1f");
			ImGui::SetItemTooltip("1 is a steady shower, 10 is a downpour (about ten thousand drops).");
		}
		static const char* wetModes[] = { "Off", "When it rains", "Always" };
		isChanged |= ImGui::Combo("Wet world", &look.wetMode, wetModes, 3);
		ImGui::SetItemTooltip("Wet surfaces are darker and glossier. When it rains follows the map's own rain and Rain"
							  " on any map.");

		if (look.wetMode != 0)
		{
			isChanged |= ImGui::Checkbox("Puddles", &look.isPuddles);
			ImGui::SetItemTooltip("Water collects in the dips of floors and ground (from the exported height maps) as"
								  " mirror smooth puddles. Remix reloads the materials when this changes.");
		}

		if (isChanged)
		{
			SetLookSettings(look);
		}
	}

	static void DrawEffectsTab()
	{
		LookSettings look = GetLookSettings();
		bool isChanged = false;
		ImGui::SeparatorText("Effects Remix draws");

		static const char* effectNames[effectKindCount] = {
			"Effects (smoke, sprites, trails)",
			"Decals (bullet holes, blood, scorch)",
			"Breakable glass",
			"Particle clouds (dust motes)",
			"Spark fountains",
			"Bullet tracers",
		};

		for (int i = 0; i < effectKindCount; ++i)
		{
			const auto kind = static_cast<EffectKind>(i);
			bool isDrawn = IsEffectDrawn(kind);

			if (ImGui::Checkbox(effectNames[i], &isDrawn))
			{
				TrySetEffectDrawn(kind, isDrawn);
			}
		}

		ImGui::SetItemTooltip("Untick to stop the engine drawing that kind of effect, to see which one Remix"
							  " shows as white shards. Tag their textures in ALT+X instead for the real fix.");

		ImGui::SeparatorText("Glow");

		float glowBrightness = GetGlowBrightness();

		if (ImGui::SliderFloat("Glow brightness", &glowBrightness, 0.0f, 8.0f, "x%.2f"))
		{
			SetGlowBrightness(glowBrightness);
		}

		ImGui::SetItemTooltip("Scales every glowing surface live: lamps, screens, signs, reticles and fire.");

		isChanged |= ImGui::SliderFloat("Effect glow", &look.effectGlow, 0.0f, 5.0f, "%.2f");
		ImGui::SetItemTooltip("How brightly additive and screen-blended effects glow: mist, smoke glows, muzzle flashes, lasers. Remix's default is 0.20 (rtx.emissiveBlendOverrideEmissiveIntensity).");

		float trackerBrightness = GetTrackerBrightness();

		if (ImGui::SliderFloat("Heartbeat sensor brightness", &trackerBrightness, 0.0f, 255.0f / 186.0f, "x%.2f"))
		{
			SetTrackerBrightness(trackerBrightness);
		}

		ImGui::SetItemTooltip("How bright the heartbeat sensor screen draws, and so how much it glows.");

		ImGui::SeparatorText("Effect lights");

		bool isEffectCullingDisabled = IsEffectCullingDisabled();

		if (ImGui::Checkbox("Effects outside the view", &isEffectCullingDisabled))
		{
			SetEffectCullingDisabled(isEffectCullingDisabled);
		}

		ImGui::SetItemTooltip("Keeps fire, explosions and muzzle flashes behind you alive, so their light and"
							  " reflections reach Remix. Costs more effect draws.");

		bool isAllEffectLights = IsAllEffectLights();

		if (ImGui::Checkbox("Lights from all effects", &isAllEffectLights))
		{
			SetAllEffectLights(isAllEffectLights);
		}

		ImGui::SetItemTooltip("Sends every effect light the game adds (up to its 32), not just the four nearest"
							  " ones inside the view that the engine would light with.");

		if (isChanged)
		{
			SetLookSettings(look);
		}
	}

	static void DrawMaterialsTab()
	{
		LookSettings look = GetLookSettings();
		bool isChanged = false;
		ImGui::SeparatorText("Surfaces");

		isChanged |= ImGui::SliderFloat("Surface brightness", &look.albedoScale, 0.0f, 2.0f, "x%.2f");
		ImGui::SetItemTooltip("Scales every surface's colour. Real world surfaces are darker than most game textures.");

		isChanged |= ImGui::SliderFloat("Roughness", &look.roughnessScale, 0.0f, 2.0f, "x%.2f");
		ImGui::SetItemTooltip("Lower makes surfaces glossier and more reflective, higher makes them matte.");

		isChanged |= ImGui::SliderFloat("Roughness offset", &look.roughnessBias, -0.5f, 0.5f, "%.2f");
		ImGui::SetItemTooltip("Adds or removes roughness evenly on every surface.");

		isChanged |= ImGui::SliderFloat("Metallic", &look.metallicScale, 0.0f, 2.0f, "x%.2f");

		isChanged |= ImGui::SliderFloat("Surface detail (normal maps)", &look.normalIntensity, 0.0f, 4.0f, "x%.2f");
		ImGui::SetItemTooltip("Depth of bumps, grooves and grain on surfaces.");

		isChanged |= ImGui::Checkbox("Clay render (white materials)", &look.isWhiteMaterial);
		ImGui::SetItemTooltip("Draws everything plain white to show only the lighting.");

		isChanged |= ImGui::Checkbox("Parallax depth", &look.isParallax);
		ImGui::SetItemTooltip("Bricks, rubble, bark and ground get real depth from height maps made from the game's"
							  " normal maps. Costs some performance.");

		if (look.isParallax)
		{
			isChanged |= ImGui::SliderFloat("Parallax strength", &look.parallaxDepth, 0.1f, 4.0f, "x%.2f");
		}
		isChanged |= ImGui::Checkbox("Mirror mode", &look.isMirror);
		ImGui::SetItemTooltip("Every surface becomes near mirror smooth, like wet glass.");

		if (look.isMirror)
		{
			isChanged |= ImGui::SliderFloat("Mirror metal", &look.mirrorMetal, 0.0f, 1.0f, "%.2f");
			ImGui::SetItemTooltip("0 keeps surfaces glassy, 1 turns everything to chrome.");
		}

		ImGui::SeparatorText("Map colour");

		bool isMapColourGrade = IsMapColourGrade();

		if (ImGui::Checkbox("Map colour grade", &isMapColourGrade))
		{
			SetMapColourGrade(isMapColourGrade);
		}

		ImGui::SetItemTooltip("Runs the game's own film pass on Remix's frame: each map's vision set, night vision,"
							  " thermal and the pain look. Depth of field, glow and blur stay off.");

		float gradeStrength = GetMapColourGradeStrength();

		if (ImGui::SliderFloat("Map colour grade strength", &gradeStrength, 0.0f, 1.0f, "%.2f"))
		{
			SetMapColourGradeStrength(gradeStrength);
		}

		ImGui::SetItemTooltip("How much of the map's grade lands on Remix's frame. Remix already lights the map with"
							  " its own warm or cold sun, so the full grade doubles the tint.");

		bool isSharedDecalTagged = IsSharedDecalTags();

		if (ImGui::Checkbox("Decal tags on shared wall textures", &isSharedDecalTagged))
		{
			SetSharedDecalTags(isSharedDecalTagged);
		}

		ImGui::SetItemTooltip("Tags a texture as a decal when a wall and a blended layer on top of it share it, so the"
							  " layer does not hide the wall behind it. Save, then restart the game to apply.");

		ImGui::SeparatorText("Neon");

		bool isNeonFoliage = IsNeonFoliage();
		float neonIntensity = GetNeonIntensity();
		bool isNeonChanged = ImGui::Checkbox("Neon foliage", &isNeonFoliage);
		ImGui::SetItemTooltip("Trees, bushes, grass and leaves glow in their own colours and light the area around"
							  " them. Applies within a second.");

		if (isNeonFoliage)
		{
			isNeonChanged |= ImGui::SliderFloat("Neon glow", &neonIntensity, 0.0f, 50.0f, "%.1f",
				ImGuiSliderFlags_Logarithmic);
			ImGui::SetItemTooltip("How bright the foliage glows. Glow brightness on the Scene tab scales it too.");
		}

		if (isNeonChanged)
		{
			SetNeonFoliage(isNeonFoliage, neonIntensity);
		}

		isChanged |= ImGui::Checkbox("Light cache off while neon is on", &look.isNeonCacheOff);
		ImGui::SetItemTooltip("Turns off Remix's emissive light cache (rtx.neeCache.enable) while neon foliage glows, to"
							  " avoid a GPU hang seen with neon on. Other glowing lights may look a little noisier.");

		if (isNeonFoliage)
		{
			NeonPulse pulse = GetNeonPulse();
			bool isPulseChanged = ImGui::Checkbox("Neon pulse", &pulse.isEnabled);
			ImGui::SetItemTooltip("Each kind of plant pulses on its own random rhythm. The leaves brighten and dim with"
								  " their glow.");

			if (pulse.isEnabled)
			{
				isPulseChanged |= ImGui::SliderFloat("Pulse speed", &pulse.speed, 0.05f, 3.0f, "%.2f per second");
				ImGui::SetItemTooltip("Average pulses per second. Each plant varies around this.");

				isPulseChanged |= ImGui::SliderFloat("Pulse depth", &pulse.depth, 0.0f, 1.0f, "%.2f");
				ImGui::SetItemTooltip("How far the glow dims between peaks. 1 fades to dark, low values are a gentle"
									  " shimmer.");
			}

			if (isPulseChanged)
			{
				SetNeonPulse(pulse);
			}

			AlienGlow alien = GetAlienGlow();
			bool isAlienChanged = ImGui::Checkbox("Alien glow", &alien.isEnabled);
			ImGui::SetItemTooltip("Gives every tree and bush its own timing and colour. Draws those plants one by one"
								  " while on, which costs some performance.");

			if (alien.isEnabled)
			{
				static const char* alienStyles[] = { "Each plant on its own timing", "Waves across plants" };
				isAlienChanged |= ImGui::Combo("Alien style", &alien.style, alienStyles, 2);
				ImGui::SetItemTooltip("Own timing pulses every tree and bush separately. Waves sweep the glow from plant"
									  " to plant across the area. Ground cover like grass glows per plant type.");

				isAlienChanged |= ImGui::SliderFloat("Alien speed", &alien.speed, 0.02f, 3.0f, "%.2f");
				isAlienChanged |= ImGui::SliderFloat("Alien depth", &alien.depth, 0.0f, 1.0f, "%.2f");
				ImGui::SetItemTooltip("How dark the dim parts get. 1 goes fully dark between bands.");

				if (alien.style == 1)
				{
					isAlienChanged |= ImGui::SliderFloat("Wave spacing", &alien.scale, 0.1f, 6.0f, "x%.2f",
						ImGuiSliderFlags_Logarithmic);
					ImGui::SetItemTooltip("Higher packs the waves closer together.");
				}

				static const char* alienColours[] = { "Natural", "Single colour", "Random per plant",
					"Shifting rainbow" };
				isAlienChanged |= ImGui::Combo("Glow colours", &alien.colourMode, alienColours, 4);
				ImGui::SetItemTooltip("Natural keeps the leaves' own colours. The others colour the glow and the leaves"
									  " with it. Random picks new colours every map.");

				if (alien.colourMode == 1)
				{
					isAlienChanged |= ImGui::ColorEdit3("Glow colour", alien.tint, ImGuiColorEditFlags_NoInputs);
				}

				if (alien.colourMode == 2 && ImGui::Button("New colours"))
				{
					RerollAlienColours();
				}
			}

			if (isAlienChanged)
			{
				SetAlienGlow(alien);
			}
		}

		if (isChanged)
		{
			SetLookSettings(look);
		}
	}

	static void DrawQualityTab()
	{
		LookSettings look = GetLookSettings();
		bool isChanged = false;
		if (ImGui::Button("Lightweight"))
		{
			look.maxBounces = 2;
			look.isGlassShadows = false;
			look.isSkinScattering = false;
			look.isParallax = false;
			look.isGodRays = false;
			look.isMotionBlur = false;
			look.isChromaticAberration = false;
			look.isHaze = false;
			look.isLaserLights = false;
			look.isRainEverywhere = false;
			look.isPuddles = false;
			look.isMirror = false;
			look.isPlayerShadowCopies = false;
			look.isPlayerVirtualInstances = false;

			isChanged = true;

			// the extras outside the look settings cost as much as the path tracing ones
			SetEffectCullingDisabled(false);
			SetAllEffectLights(false);
			SetOffscreenWorldEnabled(false);
			SetEntityCullingDisabled(false);

			AlienGlow alien = GetAlienGlow();

			if (alien.isEnabled)
			{
				alien.isEnabled = false;
				SetAlienGlow(alien);
			}
		}

		ImGui::SetItemTooltip("Cheaper path tracing that keeps the look: 2 light bounces, and off: haze, laser"
							  " light, rain on any map, puddles, mirror mode, alien glow, coloured glass"
							  " shadows, skin scattering, parallax, god rays, motion blur, chromatic aberration, effects"
							  " outside the view, lights from all effects, off-screen world and players behind you. Lit fixtures"
							  " and view culling are left alone, converted maps get their light from them. For upscaling (DLSS mode) use ALT+X. Save keeps it.");

		if (ImGui::Button("Reset look"))
		{
			look = LookSettings{};
			SetSkyScale(1.0f);
			isChanged = true;
		}

		ImGui::SeparatorText("Path tracing");

		static const char* indirectModes[] = { "Reference (noisiest)", "ReSTIR GI", "Neural radiance cache (NRC)" };
		isChanged |= ImGui::Combo("Bounced light", &look.indirectMode, indirectModes, 3);
		ImGui::SetItemTooltip("How light bouncing between surfaces is traced. NRC allows unlimited bounces and is usually"
							  " closest to a real photo. ReSTIR GI is cleaner than Reference.");

		isChanged |= ImGui::SliderInt("Light bounces", &look.maxBounces, 0, 15, "%d", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SetItemTooltip("More bounces fill rooms and shadows with realistic light. Costs performance.");

		isChanged |= ImGui::Checkbox("Indirect light", &look.isBounceLight);
		ImGui::SetItemTooltip("Off lights surfaces only directly, for a harsh, high contrast look.");

		isChanged |= ImGui::Checkbox("Coloured shadows through glass and water", &look.isGlassShadows);
		ImGui::SetItemTooltip("Glass and water tint the light passing through them.");

		isChanged |= ImGui::Checkbox("Skin light scattering", &look.isSkinScattering);
		ImGui::SetItemTooltip("Light passes into skin and softens it, the way real skin looks.");

		ImGui::SeparatorText("What stays drawn");

		static const char* cullingNames[cullingLevelCount] = {
			"Engine (default, the map batches keep walls and roofs)",
			"Nearby rooms whole (props can drop on open maps)",
			"Plus props at any distance",
			"Whole map every frame (slowest)",
		};

		int cullingLevel = static_cast<int>(GetCullingStats().level);

		if (ImGui::Combo("View culling", &cullingLevel, cullingNames, cullingLevelCount))
		{
			TrySetCullingLevel(static_cast<CullingLevel>(cullingLevel));
		}

		ImGui::SetItemTooltip("How much the engine keeps out of view. Remix only lights what is drawn, so less"
							  " culling means steadier light and shadows, at a frame rate cost.");

		bool isOffscreenWorldEnabled = IsOffscreenWorldEnabled();

		if (ImGui::Checkbox("Off-screen world for reflections", &isOffscreenWorldEnabled))
		{
			SetOffscreenWorldEnabled(isOffscreenWorldEnabled);
		}

		ImGui::SetItemTooltip("Redraws the map surfaces and props the engine culled this frame, so reflections and"
							  " shadows still see them. Off draws only what the engine drew.");

		bool isEntityCullingDisabled = IsEntityCullingDisabled();

		if (ImGui::Checkbox("Players and vehicles behind you", &isEntityCullingDisabled))
		{
			SetEntityCullingDisabled(isEntityCullingDisabled);
		}

		ImGui::SetItemTooltip("Stops the engine leaving out players, vehicles, doors and script models outside the"
							  " view, so they cast shadows and show in reflections. Costs CPU skinning.");

		bool isSteadyModelDetail = IsSteadyModelDetail();

		if (ImGui::Checkbox("Steady model detail when aiming", &isSteadyModelDetail))
		{
			SetSteadyModelDetail(isSteadyModelDetail);
		}

		ImGui::SetItemTooltip("Keeps model detail at the hip-fire level while zoomed, so aiming does not swap every"
							  " model's geometry and throw away Remix's history.");

		bool isPlayerBodyShadow = IsPlayerBodyShadow();

		if (ImGui::Checkbox("Your shadow and reflection", &isPlayerBodyShadow))
		{
			SetPlayerBodyShadow(isPlayerBodyShadow);
		}

		ImGui::SetItemTooltip("Draws your own body for Remix as a player model: hidden from the camera, but it casts"
							  " a shadow and shows in reflections.");

		if (isPlayerBodyShadow)
		{
			LookSettings playerLook = GetLookSettings();
			bool isPlayerLookChanged = ImGui::Checkbox("Remix player model shadows", &playerLook.isPlayerShadowCopies);
			ImGui::SetItemTooltip("rtx.playerModel.enablePrimaryShadows. Untick to test whether it moves the sky.");

			isPlayerLookChanged |= ImGui::Checkbox("Remix player model copies", &playerLook.isPlayerVirtualInstances);
			ImGui::SetItemTooltip("rtx.playerModel.enableVirtualInstances. Untick to test whether it moves the sky.");

			if (isPlayerLookChanged)
			{
				SetLookSettings(playerLook);
			}
		}

		if (isChanged)
		{
			SetLookSettings(look);
		}
	}

	static void DrawPhotoSection()
	{
		LookSettings look = GetLookSettings();
		bool isChanged = false;
		isChanged |= ImGui::Checkbox("Photo mode", &look.isPhotoMode);
		ImGui::SetItemTooltip("Remix's free camera, starting at your eye: numpad 8 4 5 6 move, 7 and 9 down and up, 0"
							  " faster, arrow keys turn. The match keeps running, so stand still somewhere safe.");

		if (look.isPhotoMode)
		{
			isChanged |= ImGui::Checkbox("Accumulate (noise free still)", &look.isPhotoAccumulate);
			ImGui::SetItemTooltip("Averages frames while the camera holds still for a clean photo. Moving things"
								  " smear, like a long exposure.");

			isChanged |= ImGui::Checkbox("Lock photo camera", &look.isPhotoLocked);
		}

		if (isChanged)
		{
			SetLookSettings(look);
		}
	}

	static std::string mapEditorMessage;
	static bool isLabellingLights = false;
	static int editedLightIndex = -1;
	static LightOverride editedLight;
	static AuthoredLight newLight;
	static int newMarkerId = 1;
	static float newAreaHalfSize[3] = { 512.0f, 512.0f, 256.0f };
	static float newAreaRadius = 3000.0f;

	struct NearbyLight
	{
		MapLightInfo info;
		float distance = 0.0f;
	};

	static std::vector<NearbyLight> CollectNearbyLights(const float* camera, std::size_t count)
	{
		std::vector<NearbyLight> nearby;

		for (const MapLightInfo& info : GetMapLightInfo())
		{
			const float offset[3] = { info.origin[0] - camera[0], info.origin[1] - camera[1], info.origin[2] - camera[2] };

			NearbyLight light;
			light.info = info;
			light.distance = std::sqrt(offset[0] * offset[0] + offset[1] * offset[1] + offset[2] * offset[2]);
			nearby.push_back(light);
		}

		std::sort(nearby.begin(), nearby.end(), [](const NearbyLight& left, const NearbyLight& right)
		{
			return left.distance < right.distance;
		});

		if (nearby.size() > count)
		{
			nearby.resize(count);
		}

		return nearby;
	}

	static void DrawLightLabels(const std::vector<NearbyLight>& nearby)
	{
		const ImVec2 display = ImGui::GetIO().DisplaySize;
		ImDrawList* drawList = ImGui::GetBackgroundDrawList();

		for (const NearbyLight& light : nearby)
		{
			float screen[2];

			if (!TryProjectToScreen(light.info.origin, display.x, display.y, screen))
			{
				continue;
			}

			char label[64];
			std::snprintf(label, sizeof(label), "0x%016llX", static_cast<unsigned long long>(light.info.hash));

			ImU32 colour = IM_COL32(255, 220, 60, 255);

			if (light.info.isAuthored)
			{
				colour = IM_COL32(60, 255, 90, 255);
			}
			else if (light.info.isIgnored)
			{
				colour = IM_COL32(255, 40, 40, 255);
			}
			else if (light.info.isChanged)
			{
				colour = IM_COL32(60, 120, 255, 255);
			}

			drawList->AddText(ImVec2(screen[0] + 1.0f, screen[1] + 1.0f), IM_COL32(0, 0, 0, 255), label);
			drawList->AddText(ImVec2(screen[0], screen[1]), colour, label);
		}
	}

	static void DrawMapEditor(const float* camera, bool hasCamera)
	{
		MapDraft& draft = GetMapDraft();

		if (!hasCamera)
		{
			ImGui::TextWrapped("The editor needs a map loaded and the camera handed to Remix.");
			return;
		}

		const std::vector<NearbyLight> nearby = CollectNearbyLights(camera, 12);

		ImGui::Checkbox("Label nearby lights", &isLabellingLights);
		ImGui::SetItemTooltip("Writes each nearby light's hash on screen, the key a map file uses to change it.");

		if (isLabellingLights)
		{
			DrawLightLabels(nearby);
		}

		if (ImGui::BeginTable("nearby lights", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
		{
			for (int row = 0; row < static_cast<int>(nearby.size()); ++row)
			{
				const NearbyLight& light = nearby[row];
				ImGui::PushID(row);
				ImGui::TableNextRow();

				ImGui::TableNextColumn();
				ImGui::Text("0x%016llX", static_cast<unsigned long long>(light.info.hash));

				ImGui::TableNextColumn();

				if (light.info.isAuthored)
				{
					ImGui::TextUnformatted("added");
				}
				else if (light.info.isSpot)
				{
					ImGui::TextUnformatted("spot");
				}
				else
				{
					ImGui::TextUnformatted("omni");
				}

				ImGui::TableNextColumn();
				ImGui::Text("%.0f", light.distance);

				ImGui::TableNextColumn();

				if (!light.info.isAuthored)
				{
					if (ImGui::SmallButton("Ignore"))
					{
						draft.ignoredLights.push_back(light.info.hash);
					}

					ImGui::SameLine();

					if (ImGui::SmallButton("Change"))
					{
						editedLightIndex = row;
						editedLight = LightOverride{};
						editedLight.hash = light.info.hash;
						editedLight.brightness = 1.0f;
					}
				}

				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		if (editedLightIndex >= 0)
		{
			ImGui::SeparatorText("Change light");
			ImGui::Text("0x%016llX", static_cast<unsigned long long>(editedLight.hash));

			float brightness = editedLight.brightness.value_or(1.0f);

			if (ImGui::SliderFloat("Brightness##edit", &brightness, 0.0f, 16.0f, "x%.2f"))
			{
				editedLight.brightness = brightness;
			}

			ImGui::Checkbox("Replace colour##edit", &editedLight.hasColour);

			if (editedLight.hasColour)
			{
				ImGui::ColorEdit3("Colour##edit", editedLight.colour, ImGuiColorEditFlags_NoInputs);
			}

			float radius = editedLight.radius.value_or(4.0f);

			if (ImGui::SliderFloat("Size##edit", &radius, 0.5f, 32.0f, "%.1f"))
			{
				editedLight.radius = radius;
			}

			float volumetric = editedLight.volumetric.value_or(1.0f);

			if (ImGui::SliderFloat("Volumetric##edit", &volumetric, 0.0f, 8.0f, "x%.2f"))
			{
				editedLight.volumetric = volumetric;
			}

			if (ImGui::Button("Keep change"))
			{
				draft.lightOverrides.push_back(editedLight);
				editedLightIndex = -1;
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
			{
				editedLightIndex = -1;
			}
		}

		ImGui::SeparatorText("Add at the camera");

		ImGui::ColorEdit3("Light colour", newLight.colour, ImGuiColorEditFlags_NoInputs);
		ImGui::SliderFloat("Light intensity", &newLight.intensity, 0.0f, 500.0f, "%.1f");
		ImGui::SliderFloat("Light size", &newLight.radius, 0.5f, 32.0f, "%.1f");
		ImGui::Checkbox("Point down", &newLight.hasDirection);

		if (newLight.hasDirection)
		{
			ImGui::SliderFloat("Cone", &newLight.coneDeg, 1.0f, 179.0f, "%.0f deg");
			ImGui::SliderFloat("Softness", &newLight.softness, 0.0f, 1.0f, "%.2f");
		}

		if (ImGui::Button("Add light here"))
		{
			AuthoredLight light = newLight;
			std::copy(camera, camera + 3, light.position);
			light.direction[0] = 0.0f;
			light.direction[1] = 0.0f;
			light.direction[2] = -1.0f;
			draft.addedLights.push_back(light);
		}

		ImGui::InputInt("Marker id", &newMarkerId);

		if (ImGui::Button("Add marker here"))
		{
			MapMarker marker;
			marker.id = newMarkerId;
			std::copy(camera, camera + 3, marker.position);
			draft.markers.push_back(marker);
			++newMarkerId;
		}

		ImGui::SetItemTooltip("A small quad with its own Remix hash, for hanging toolkit replacements or lights on.");

		ImGui::InputFloat3("Area half size", newAreaHalfSize, "%.0f");
		ImGui::SliderFloat("Area culling radius", &newAreaRadius, 0.0f, 20000.0f, "%.0f");

		if (ImGui::Button("Add culling area here"))
		{
			CullingArea area;

			for (int axis = 0; axis < 3; ++axis)
			{
				area.boxMin[axis] = camera[axis] - newAreaHalfSize[axis];
				area.boxMax[axis] = camera[axis] + newAreaHalfSize[axis];
			}

			area.radius = newAreaRadius;
			draft.cullingAreas.push_back(area);
		}

		if (IsMapDraftEmpty())
		{
			ImGui::TextDisabled("Nothing changed yet.");
		}
		else
		{
			const std::string toml = BuildMapDraftToml();
			ImGui::InputTextMultiline("##draft", const_cast<char*>(toml.c_str()), toml.size() + 1,
				ImVec2(-1.0f, ImGui::GetTextLineHeight() * 10.0f), ImGuiInputTextFlags_ReadOnly);

			if (ImGui::Button("Copy"))
			{
				if (TryCopyToClipboard(toml))
				{
					mapEditorMessage = "Copied.";
				}
				else
				{
					mapEditorMessage = "Could not open the clipboard.";
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("Add to map file"))
			{
				TryAppendMapDraft(mapEditorMessage);
			}

			ImGui::SameLine();

			if (ImGui::Button("Clear"))
			{
				ClearMapDraft();
				mapEditorMessage.clear();
			}
		}

		if (!mapEditorMessage.empty())
		{
			ImGui::TextWrapped("%s", mapEditorMessage.c_str());
		}
	}

	static void DrawRemixTab()
	{
		float camera[3] = {};
		const bool hasCamera = TryGetCameraOrigin(camera);
		const MapSettings& mapSettings = GetMapSettings();

		ImGui::SeparatorText("Map file");

		if (mapSettings.mapName.empty())
		{
			ImGui::TextDisabled("No map loaded.");
		}
		else
		{
			ImGui::Text("zw3_remix\\maps\\%s.toml", mapSettings.mapName.c_str());

			if (!mapSettings.loadError.empty())
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Did not parse:");
				ImGui::TextWrapped("%s", mapSettings.loadError.c_str());
			}
			else if (!mapSettings.hasFile)
			{
				ImGui::TextDisabled("No file for this map, the editor below can start one.");
			}
			else
			{
				ImGui::Text("%d Remix options, %d lights ignored, %d changed, %d added, %d attached, %d markers, %d triggers",
					static_cast<int>(mapSettings.remixOptions.size()), static_cast<int>(mapSettings.ignoredLights.size()),
					static_cast<int>(mapSettings.lightOverrides.size()), static_cast<int>(mapSettings.addedLights.size()),
					static_cast<int>(mapSettings.attachedLights.size()), static_cast<int>(mapSettings.markers.size()),
					static_cast<int>(mapSettings.triggers.size()));
			}

			if (ImGui::Button("Reload map file"))
			{
				ReloadMapSettings();
			}

			ImGui::SetItemTooltip("Files also reload by themselves a second after they are saved.");
		}

		if (ImGui::CollapsingHeader("Light tuning"))
		{
			LightTuning tuning = GetLightTuning();
			bool isTuningChanged = false;

			isTuningChanged |= ImGui::SliderFloat("Spot lights", &tuning.spotBrightness, 0.0f, 8.0f, "x%.2f");
			isTuningChanged |= ImGui::SliderFloat("Omni lights", &tuning.omniBrightness, 0.0f, 8.0f, "x%.2f");
			isTuningChanged |= ImGui::SliderFloat("Light size", &tuning.sphereRadius, 0.5f, 32.0f, "%.1f");
			ImGui::SetItemTooltip("Radius of each map light's sphere. Bigger gives softer shadows; brightness is kept.");
			isTuningChanged |= ImGui::SliderFloat("Cone offset", &tuning.coneOffsetDeg, -45.0f, 45.0f, "%.1f deg");
			isTuningChanged |= ImGui::SliderFloat("Cone softness", &tuning.softnessOffset, -1.0f, 1.0f, "%.2f");
			isTuningChanged |= ImGui::SliderFloat("Cone focus", &tuning.focusExponent, 0.0f, 8.0f, "%.2f");
			isTuningChanged |= ImGui::SliderFloat("Light volumetrics", &tuning.volumetric, 0.0f, 8.0f, "x%.2f");
			ImGui::SetItemTooltip("How much map lights light the haze.");
			isTuningChanged |= ImGui::SliderFloat("Skip dim lights", &tuning.dimLightCutoff, 0.0f, 2.0f, "%.2f");
			ImGui::SetItemTooltip("Leaves out map lights dimmer than this, the filler lights mappers add for baked light.");

			if (isTuningChanged)
			{
				SetLightTuning(tuning);
			}

			int samples = GetDiSampleCount();

			if (ImGui::SliderInt("Light samples", &samples, 0, 32, "%d"))
			{
				SetDiSampleCount(samples);
			}

			ImGui::SetItemTooltip("rtx.di.initialSampleCount, re-sent every second because Remix resets it. More samples"
								  " help many small lights. 0 leaves it to Remix.");
		}

		if (ImGui::CollapsingHeader("Culling"))
		{
			CullingTuning tuning = GetCullingTuning();
			bool isCullingChanged = false;

			isCullingChanged |= ImGui::SliderFloat("Culling radius", &tuning.radius, 0.0f, 20000.0f, "%.0f");
			ImGui::SetItemTooltip("How far outside the view the world is kept once view culling is raised.");
			isCullingChanged |= ImGui::SliderFloat("Prop distance", &tuning.propDistanceScale, 0.1f, 20.0f, "x%.2f");
			ImGui::SetItemTooltip("Keeps props out to this many times the distance the map culls them at.");

			if (isCullingChanged)
			{
				SetCullingTuning(tuning);
			}

			const CullingStats cullingStats = GetCullingStats();
			ImGui::Text("Applied radius %.0f, prop distance x%.2f, %d models kept by the map file",
				cullingStats.appliedRadius, cullingStats.appliedPropScale, cullingStats.keptModels);

			if (cullingStats.isInCullingArea)
			{
				ImGui::TextUnformatted("The camera is in a culling area.");
			}

			if (IsCullingLevelFromMap())
			{
				ImGui::TextDisabled("This map's file sets the culling level.");
			}
		}

		if (ImGui::CollapsingHeader("Debug view"))
		{
			DebugDrawSettings debug = GetDebugDrawSettings();
			bool isDebugChanged = false;

			isDebugChanged |= ImGui::Checkbox("Show lights", &debug.isLights);
			ImGui::SetItemTooltip("Yellow map lights, red ignored, blue changed, green added.");
			isDebugChanged |= ImGui::Checkbox("Show markers", &debug.isMarkers);
			isDebugChanged |= ImGui::Checkbox("Show trigger areas", &debug.isTriggers);
			isDebugChanged |= ImGui::Checkbox("Show culling areas", &debug.isCullingAreas);
			isDebugChanged |= ImGui::SliderFloat("Line width", &debug.lineWidth, 0.1f, 8.0f, "%.1f");
			isDebugChanged |= ImGui::SliderFloat("Light range", &debug.lightRange, 100.0f, 20000.0f, "%.0f");

			if (isDebugChanged)
			{
				SetDebugDrawSettings(debug);
			}

			ImGui::Text("%d debug lines", GetDebugDrawStats().lines);
		}

		if (ImGui::CollapsingHeader("Map editor"))
		{
			DrawMapEditor(camera, hasCamera);
		}

		if (ImGui::CollapsingHeader("Remix options"))
		{
			const OptionStats optionStats = GetOptionStats();
			ImGui::Text("%d options known, %d from rtx.conf, %d ours, %d overridden by %d layers, %d blending",
				optionStats.knownOptions, optionStats.fileOptions, optionStats.ownOptions, optionStats.overriddenOptions,
				optionStats.layers, optionStats.blendingOptions);
			ImGui::Text("%d conf files in zw3_remix\\conf", optionStats.confFiles);

			if (optionStats.unrestorableOptions > 0)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f),
					"%d options had no value to go back to (not ours, not in rtx.conf)", optionStats.unrestorableOptions);
			}

			if (ImGui::Button("Reset changed options"))
			{
				ResetModifiedOptions();
			}

			ImGui::SetItemTooltip("Drops every conf file, map file and trigger change until the map or files reload.");
			ImGui::SameLine();

			if (ImGui::Button("Reload conf files"))
			{
				ReloadConfFiles();
			}

			for (const auto& [name, value] : DescribeOverriddenOptions())
			{
				ImGui::BulletText("%s = %s", name.c_str(), value.c_str());
			}
		}

		if (ImGui::CollapsingHeader("Triggers"))
		{
			const TriggerStats triggerStats = GetTriggerStats();
			ImGui::Text("Vision set: %s", triggerStats.visionName.c_str());

			if (triggerStats.hasOutdoorMap && triggerStats.isCameraOutdoors)
			{
				ImGui::TextUnformatted("Camera is outdoors");
			}
			else if (triggerStats.hasOutdoorMap)
			{
				ImGui::TextUnformatted("Camera is indoors");
			}
			else
			{
				ImGui::TextDisabled("This map has no outdoor mask, indoors and outdoors triggers stay off.");
			}

			if (!triggerStats.isSoundHooked)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "Sound hook not installed, sound triggers stay off");
			}

			ImGui::Text("%llu sounds heard", triggerStats.soundsHeard);

			for (const TriggerState& state : GetTriggerStates())
			{
				const char* status = "idle";

				if (state.isActive)
				{
					status = "active";
				}

				const char* scope = "";

				if (state.isGlobal)
				{
					scope = " (all maps)";
				}

				ImGui::BulletText("%s%s: %s (%llu times)", state.name.c_str(), scope, status, state.activations);
			}

			if (ImGui::TreeNode("Recent sounds"))
			{
				for (const std::string& sound : GetRecentSounds())
				{
					ImGui::TextUnformatted(sound.c_str());
				}

				ImGui::TreePop();
			}
		}

		if (ImGui::CollapsingHeader("Timing"))
		{
			if (ImGui::BeginTable("timing", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
			{
				ImGui::TableSetupColumn("Section");
				ImGui::TableSetupColumn("Last ms");
				ImGui::TableSetupColumn("Median");
				ImGui::TableSetupColumn("Peak");
				ImGui::TableSetupColumn("Spikes");
				ImGui::TableHeadersRow();

				for (const SectionTiming& timing : GetProfile())
				{
					ImGui::TableNextRow();
					ImGui::TableNextColumn();
					ImGui::TextUnformatted(timing.name);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", timing.lastMs);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", timing.medianMs);
					ImGui::TableNextColumn();
					ImGui::Text("%.3f", timing.peakMs);
					ImGui::TableNextColumn();
					ImGui::Text("%llu (%.2f)", timing.spikes, timing.spikeMs);
				}

				ImGui::EndTable();
			}
		}
	}

	constexpr std::uintptr_t in_mouseActive = 0x146786F6C;

	static bool didSuspendGameMouse = false;

	static void SetGameMouseActive(bool isActive)
	{
		auto* const mouseActive = reinterpret_cast<int*>(Engine::RebaseAddress(in_mouseActive));

		if (!isActive)
		{
			*mouseActive = 0;
			didSuspendGameMouse = true;
		}
		else if (didSuspendGameMouse)
		{
			*mouseActive = 1;
			didSuspendGameMouse = false;
		}
	}

	static void DrawSettingsButtons(Core::Config& config)
	{
		if (ImGui::Button("Save"))
		{
			Core::SaveConfig(settingsMessage);
		}

		ImGui::SetItemTooltip("Writes these settings into zw3_remix\\remix.ini so they apply at the next launch.");

		ImGui::SameLine();

		if (ImGui::Button("Reload"))
		{
			Core::ReloadConfig();
			ApplyConfig(Core::GetConfig());
			settingsMessage = "Reloaded the settings from zw3_remix\\remix.ini";
		}

		ImGui::SetItemTooltip("Throws away menu changes since the last save.");

		ImGui::SameLine();

		if (ImGui::Button("Reset all"))
		{
			ImGui::OpenPopup("Reset all settings?");
		}

		ImGui::SetItemTooltip("Every Remix setting back to the built-in defaults.");

		if (ImGui::BeginPopupModal("Reset all settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::TextUnformatted("Every Remix setting goes back to the built-in defaults.\n"
								   "Nothing is saved until you press Save.");

			if (ImGui::Button("Reset"))
			{
				Core::ResetAllSettings(config);
				ApplyConfig(config);
				settingsMessage = "All settings reset to defaults (not saved yet)";
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel"))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}

		const std::string& mapName = Core::GetActiveMap();

		if (!mapName.empty() && Core::HasMapConfig())
		{
			ImGui::SameLine();

			if (ImGui::Button("Use global on this map"))
			{
				Core::ForgetMapConfig(settingsMessage);
				ApplyConfig(Core::GetConfig());
			}

			ImGui::SetItemTooltip("Deletes this map's own settings file; the map goes back to the global settings.");
			ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "Settings for %s (Save writes zw3_remix\\maps\\%s.ini)",
				mapName.c_str(), mapName.c_str());
		}
		else if (!mapName.empty())
		{
			ImGui::SameLine();

			if (ImGui::Button("Save for this map"))
			{
				Core::SaveMapConfig(settingsMessage);
			}

			ImGui::SetItemTooltip("Keeps what differs from the global settings for this map only. It loads every time"
								  " this map does, on top of the global settings.");
			ImGui::TextDisabled("Global settings on %s (Save writes zw3_remix\\remix.ini)", mapName.c_str());
		}

		if (Core::HasUnsavedChanges())
		{
			ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Unsaved changes -- Save keeps them for next launch");
		}

		if (!settingsMessage.empty())
		{
			ImGui::TextWrapped("%s", settingsMessage.c_str());
		}

		ImGui::Separator();
	}

	static void DrawWindow()
	{
		Core::Config& config = Core::MutableConfig();
		CaptureConfig(config);

		ImGui::SetNextWindowSize(ImVec2(460, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("zw3 RTX Remix (INSERT)");

		DrawSettingsButtons(config);

		if (ImGui::BeginTabBar("tabs"))
		{
			if (ImGui::BeginTabItem("Lighting"))
			{
				DrawLightingTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Exposure"))
			{
				DrawExposureTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Atmosphere"))
			{
				DrawAtmosphereTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Effects"))
			{
				DrawEffectsTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Materials"))
			{
				DrawMaterialsTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Quality"))
			{
				DrawQualityTab();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Map"))
			{
				DrawRemixTab();
				ImGui::SeparatorText("Photo mode");
				DrawPhotoSection();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem("Status"))
			{
				DrawStatusTab();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}

		ImGui::End();
	}

	static bool Attach(IDirect3DDevice9* device, HWND window)
	{
		if (!device || !window)
		{
			return false;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		ImGuiIO& io = ImGui::GetIO();
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		ImGui::StyleColorsDark();

		if (!ImGui_ImplWin32_Init(window))
		{
			LogError() << "remix: ImGui_ImplWin32_Init failed, no menu";
			ImGui::DestroyContext();
			return false;
		}

		if (!ImGui_ImplDX9_Init(device))
		{
			LogError() << "remix: ImGui_ImplDX9_Init failed, no menu";
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			return false;
		}

		if (!HookWindow(window))
		{
			LogWarn() << "remix: no input hook, the menu will draw but not respond";
		}

		deviceIdentity = device;
		isReady = true;

		Log() << "remix: menu ready, INSERT toggles it";

		return true;
	}

	void Render(IDirect3DDevice9* device, HWND window)
	{
		static bool hasFailed = false;

		if (!isReady && !hasFailed && !Attach(device, window))
		{
			hasFailed = true;
		}

		if (!isReady || device != deviceIdentity)
		{
			return;
		}

		DrainInput();
		PollKeyboard();

		ImGui_ImplDX9_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

		if (ImGui::IsKeyPressed(ImGuiKey_Insert, false))
		{
			isOpen = !isOpen;
		}

		// Remix's own menu (ALT+X) reads the window's messages itself, but the game keeps pulling the cursor back to
		// the centre, so the game mouse is let go while it is open, the same as for this menu
		const bool isRemixChord = hookedWindow && GetForegroundWindow() == GetAncestor(hookedWindow, GA_ROOT)
			&& IsHeld(VK_MENU) && IsHeld('X');

		if (isRemixChord && !wasRemixChord)
		{
			isRemixMenuOpen = !isRemixMenuOpen;
		}

		wasRemixChord = isRemixChord;

		if (isOpen)
		{
			DrawWindow();
		}

		SetMenuOwnsInput(isOpen);
		SetGameMouseActive(!isOpen && !isRemixMenuOpen);

		ImGui::GetIO().MouseDrawCursor = isOpen;

		ImGui::EndFrame();
		ImGui::Render();

		if (SUCCEEDED(device->BeginScene()))
		{
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			device->EndScene();
		}
	}

	void OnDeviceLost()
	{
		if (isReady)
		{
			ImGui_ImplDX9_InvalidateDeviceObjects();
		}
	}

	bool IsOpen()
	{
		return isOpen;
	}
}
