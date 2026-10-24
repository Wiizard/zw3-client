#include "STDInclude.hpp"

#include "Api.hpp"
#include "ApiStock.hpp"

#include "Core.hpp"

namespace Components::Remix
{
	static remixapi_Interface api{};
	static bool hasApi = false;
	static bool didApiFail = false;
	static ApiRuntime apiRuntime = ApiRuntime::None;
	static IDirect3DDevice9* registeredDevice = nullptr;

	static PFN_remixapi_SetConfigVariable plusSetConfigVariable = nullptr;

	// Remix Plus files API values in its "derived" layer, below user.conf, unless the value starts with "user ".
	// Stock Remix files them in the user layer, so zw3's settings win over a saved user.conf; this keeps that on Plus.
	static remixapi_ErrorCode REMIXAPI_CALL SetUserConfigVariable(const char* key, const char* value)
	{
		if (!value)
		{
			return plusSetConfigVariable(key, value);
		}

		const std::string userValue = std::string("user ") + value;
		return plusSetConfigVariable(key, userValue.c_str());
	}

	template <typename T>
	static void FromEntry(T& slot, StockEntry entry)
	{
		slot = reinterpret_cast<T>(entry);
	}

	// Stock dxvk-remix rejects the Remix Plus version (minor 1000) with INCOMPATIBLE_VERSION, and the two interface
	// tables have different layouts, so the stock table is read through its own header and copied into ours.
	static remixapi_ErrorCode TryInitializeStock(PFN_remixapi_InitializeLibrary initialize)
	{
		StockApiEntries stock{};
		const auto result = static_cast<remixapi_ErrorCode>(
			TryInitializeStockApi(reinterpret_cast<StockEntry>(initialize), stock));

		if (result != REMIXAPI_ERROR_CODE_SUCCESS)
		{
			return result;
		}

		api = {};
		FromEntry(api.Shutdown, stock.shutdown);
		FromEntry(api.CreateMaterial, stock.createMaterial);
		FromEntry(api.DestroyMaterial, stock.destroyMaterial);
		FromEntry(api.CreateMesh, stock.createMesh);
		FromEntry(api.DestroyMesh, stock.destroyMesh);
		FromEntry(api.SetupCamera, stock.setupCamera);
		FromEntry(api.DrawInstance, stock.drawInstance);
		FromEntry(api.CreateLight, stock.createLight);
		FromEntry(api.DestroyLight, stock.destroyLight);
		FromEntry(api.DrawLightInstance, stock.drawLightInstance);
		FromEntry(api.SetConfigVariable, stock.setConfigVariable);
		FromEntry(api.dxvk_CreateD3D9, stock.dxvkCreateD3D9);
		FromEntry(api.dxvk_RegisterD3D9Device, stock.dxvkRegisterD3D9Device);
		FromEntry(api.dxvk_GetExternalSwapchain, stock.dxvkGetExternalSwapchain);
		FromEntry(api.dxvk_GetVkImage, stock.dxvkGetVkImage);
		FromEntry(api.dxvk_CopyRenderingOutput, stock.dxvkCopyRenderingOutput);
		FromEntry(api.dxvk_SetDefaultOutput, stock.dxvkSetDefaultOutput);
		FromEntry(api.pick_RequestObjectPicking, stock.pickRequestObjectPicking);
		FromEntry(api.pick_HighlightObjects, stock.pickHighlightObjects);
		FromEntry(api.Startup, stock.startup);
		FromEntry(api.Present, stock.present);
		FromEntry(api.SetCameraMediumMaterial, stock.setCameraMediumMaterial);

		return result;
	}

	const remixapi_Interface* TryGetApi(IDirect3DDevice9* device)
	{
		if (!device || didApiFail)
		{
			return nullptr;
		}

		if (hasApi && registeredDevice == device)
		{
			return &api;
		}

		if (!hasApi)
		{
			const HMODULE runtime = Core::ResolveForwardTarget();

			if (!runtime || !Core::IsRemixLoaded())
			{
				didApiFail = true;
				LogError() << "remix: the Remix runtime module is not loaded, the Remix API is unavailable";
				return nullptr;
			}

			const auto initialize = reinterpret_cast<PFN_remixapi_InitializeLibrary>(
				GetProcAddress(runtime, "remixapi_InitializeLibrary"));

			if (!initialize)
			{
				didApiFail = true;
				LogError() << "remix: the runtime exports no remixapi_InitializeLibrary, the Remix API is unavailable";
				return nullptr;
			}

			remixapi_InitializeLibraryInfo info{};
			info.sType = REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
			info.version = REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR, REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH);

			remixapi_ErrorCode result = initialize(&info, &api);

			if (result == REMIXAPI_ERROR_CODE_SUCCESS)
			{
				apiRuntime = ApiRuntime::Plus;
				plusSetConfigVariable = api.SetConfigVariable;
				api.SetConfigVariable = &SetUserConfigVariable;

				Log() << "remix: Remix Plus API " << REMIXAPI_VERSION_MAJOR << "." << REMIXAPI_VERSION_MINOR << "."
					  << REMIXAPI_VERSION_PATCH << " ready";
			}
			else if (result == REMIXAPI_ERROR_CODE_INCOMPATIBLE_VERSION)
			{
				result = TryInitializeStock(initialize);

				if (result == REMIXAPI_ERROR_CODE_SUCCESS)
				{
					apiRuntime = ApiRuntime::Stock;

					const StockApiVersion stock = GetStockApiVersion();
					Log() << "remix: the runtime is stock dxvk-remix, Remix API " << stock.major << "." << stock.minor << "."
						  << stock.patch << " ready, Remix Plus features stay off";
				}
			}

			if (result != REMIXAPI_ERROR_CODE_SUCCESS)
			{
				api = {};
				didApiFail = true;
				LogError() << "remix: remixapi_InitializeLibrary failed with code " << static_cast<int>(result);
				return nullptr;
			}

			hasApi = true;
		}

		const remixapi_ErrorCode result = api.dxvk_RegisterD3D9Device(reinterpret_cast<IDirect3DDevice9Ex*>(device));

		if (result != REMIXAPI_ERROR_CODE_SUCCESS)
		{
			didApiFail = true;
			LogError() << "remix: dxvk_RegisterD3D9Device failed with code " << static_cast<int>(result);
			return nullptr;
		}

		registeredDevice = device;

		return &api;
	}

	void ForgetApiDevice()
	{
		registeredDevice = nullptr;
	}

	const remixapi_Interface* GetReadyApi()
	{
		return hasApi && registeredDevice ? &api : nullptr;
	}

	ApiRuntime GetApiRuntime()
	{
		return apiRuntime;
	}

	bool IsRemixPlus()
	{
		return apiRuntime == ApiRuntime::Plus;
	}
}
