#include "STDInclude.hpp"

#include "ApiStock.hpp"

// The stock NVIDIA header shares its type names and include guard with the Remix Plus header, so only this file
// includes it, inside its own namespace. Its <stdint.h> and <windows.h> includes are already satisfied by STDInclude.
namespace Components::Remix::Stock
{
#include <remix_stock/remix_c.h>
}

namespace Components::Remix
{
	template <typename T>
	static StockEntry ToEntry(T function)
	{
		return reinterpret_cast<StockEntry>(function);
	}

	std::uint32_t TryInitializeStockApi(StockEntry initializeLibrary, StockApiEntries& entries)
	{
		const auto initialize = reinterpret_cast<Stock::PFN_remixapi_InitializeLibrary>(initializeLibrary);

		Stock::remixapi_InitializeLibraryInfo info{};
		info.sType = Stock::REMIXAPI_STRUCT_TYPE_INITIALIZE_LIBRARY_INFO;
		info.version = REMIXAPI_VERSION_MAKE(REMIXAPI_VERSION_MAJOR, REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH);

		Stock::remixapi_Interface stock{};
		const Stock::remixapi_ErrorCode result = initialize(&info, &stock);

		if (result != Stock::REMIXAPI_ERROR_CODE_SUCCESS)
		{
			return static_cast<std::uint32_t>(result);
		}

		entries.shutdown = ToEntry(stock.Shutdown);
		entries.createMaterial = ToEntry(stock.CreateMaterial);
		entries.destroyMaterial = ToEntry(stock.DestroyMaterial);
		entries.createMesh = ToEntry(stock.CreateMesh);
		entries.destroyMesh = ToEntry(stock.DestroyMesh);
		entries.setupCamera = ToEntry(stock.SetupCamera);
		entries.drawInstance = ToEntry(stock.DrawInstance);
		entries.createLight = ToEntry(stock.CreateLight);
		entries.destroyLight = ToEntry(stock.DestroyLight);
		entries.drawLightInstance = ToEntry(stock.DrawLightInstance);
		entries.setConfigVariable = ToEntry(stock.SetConfigVariable);
		entries.dxvkCreateD3D9 = ToEntry(stock.dxvk_CreateD3D9);
		entries.dxvkRegisterD3D9Device = ToEntry(stock.dxvk_RegisterD3D9Device);
		entries.dxvkGetExternalSwapchain = ToEntry(stock.dxvk_GetExternalSwapchain);
		entries.dxvkGetVkImage = ToEntry(stock.dxvk_GetVkImage);
		entries.dxvkCopyRenderingOutput = ToEntry(stock.dxvk_CopyRenderingOutput);
		entries.dxvkSetDefaultOutput = ToEntry(stock.dxvk_SetDefaultOutput);
		entries.pickRequestObjectPicking = ToEntry(stock.pick_RequestObjectPicking);
		entries.pickHighlightObjects = ToEntry(stock.pick_HighlightObjects);
		entries.startup = ToEntry(stock.Startup);
		entries.present = ToEntry(stock.Present);
		entries.setCameraMediumMaterial = ToEntry(stock.SetCameraMediumMaterial);

		return static_cast<std::uint32_t>(result);
	}

	StockApiVersion GetStockApiVersion()
	{
		return { REMIXAPI_VERSION_MAJOR, REMIXAPI_VERSION_MINOR, REMIXAPI_VERSION_PATCH };
	}
}
