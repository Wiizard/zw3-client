#pragma once

#include <d3d9.h>

namespace Components::Remix::Device
{
	template <typename Poll, typename Acquire, typename Release>
	HRESULT PollQuery(const DWORD flags, const bool ownsRenderFrame,
		const Poll& poll, const Acquire& acquire, const Release& release)
	{
		if (ownsRenderFrame)
		{
			return poll(flags);
		}

		// Even GetData(0) may implicitly issue a new query in Remix. Serialize the
		// entire call, not just its FLUSH retry, against rendering and uploads.
		if (!acquire())
		{
			return S_FALSE;
		}

		const HRESULT result = poll(flags);
		release();
		return result;
	}
}
