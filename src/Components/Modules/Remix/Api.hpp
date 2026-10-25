#pragma once

#include <remix/remix_c.h>

namespace Components::Remix
{
	enum class ApiRuntime
	{
		None,
		Plus,
		Stock,
	};

	// The table always has the Remix Plus layout. On a stock runtime, the entries Remix Plus added stay null.
	const remixapi_Interface* TryGetApi(IDirect3DDevice9* device);

	void ForgetApiDevice();

	// The table once a device has registered it, without registering one. Null before that.
	const remixapi_Interface* GetReadyApi();

	ApiRuntime GetApiRuntime();

	// True once the Remix Plus API is live, so the fork-only entries and render state channel exist.
	bool IsRemixPlus();
}
