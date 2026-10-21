#include "STDInclude.hpp"

#include "Api.hpp"

#include "Core.hpp"

namespace Components::Remix
{
	static remixapi_Interface api{};
	static bool hasApi = false;
	static bool didApiFail = false;
	static IDirect3DDevice9* registeredDevice = nullptr;

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

			const remixapi_ErrorCode result = initialize(&info, &api);

			if (result != REMIXAPI_ERROR_CODE_SUCCESS)
			{
				didApiFail = true;
				LogError() << "remix: remixapi_InitializeLibrary failed with code " << static_cast<int>(result);
				return nullptr;
			}

			hasApi = true;

			Log() << "remix: Remix API " << REMIXAPI_VERSION_MAJOR << "." << REMIXAPI_VERSION_MINOR << "."
				  << REMIXAPI_VERSION_PATCH << " ready";
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
}
