#pragma once

#include <remix/remix_c.h>

namespace Components::Remix
{
	const remixapi_Interface* TryGetApi(IDirect3DDevice9* device);

	void ForgetApiDevice();
}
