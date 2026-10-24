#pragma once

#include <cstdint>

namespace Components::Remix
{
	// Plain function pointers, so this header compiles without either Remix header.
	using StockEntry = void (*)();

	// The entries stock NVIDIA dxvk-remix shares with Remix Plus. Signatures are identical in both headers, only the
	// interface table layout differs.
	struct StockApiEntries
	{
		StockEntry shutdown = nullptr;
		StockEntry createMaterial = nullptr;
		StockEntry destroyMaterial = nullptr;
		StockEntry createMesh = nullptr;
		StockEntry destroyMesh = nullptr;
		StockEntry setupCamera = nullptr;
		StockEntry drawInstance = nullptr;
		StockEntry createLight = nullptr;
		StockEntry destroyLight = nullptr;
		StockEntry drawLightInstance = nullptr;
		StockEntry setConfigVariable = nullptr;
		StockEntry dxvkCreateD3D9 = nullptr;
		StockEntry dxvkRegisterD3D9Device = nullptr;
		StockEntry dxvkGetExternalSwapchain = nullptr;
		StockEntry dxvkGetVkImage = nullptr;
		StockEntry dxvkCopyRenderingOutput = nullptr;
		StockEntry dxvkSetDefaultOutput = nullptr;
		StockEntry pickRequestObjectPicking = nullptr;
		StockEntry pickHighlightObjects = nullptr;
		StockEntry startup = nullptr;
		StockEntry present = nullptr;
		StockEntry setCameraMediumMaterial = nullptr;
	};

	struct StockApiVersion
	{
		std::uint64_t major = 0;
		std::uint64_t minor = 0;
		std::uint64_t patch = 0;
	};

	// Calls the runtime's remixapi_InitializeLibrary with the stock 0.6 version and the stock table layout.
	// Returns the runtime's remixapi_ErrorCode.
	std::uint32_t TryInitializeStockApi(StockEntry initializeLibrary, StockApiEntries& entries);

	StockApiVersion GetStockApiVersion();
}
