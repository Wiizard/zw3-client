#pragma once

#include <d3d9.h>

namespace Components::Remix::Device
{
	template <typename Poll, typename Acquire, typename Release>
	HRESULT PollQuery(const DWORD flags, const bool ownsRenderFrame,
		const Poll& poll, const Acquire& acquire, const Release& release)
	{
		// GPU sync polls deliberately omit FLUSH while the backend is drawing.
		// A pending result must not turn a read-only poll into a device submission.
		if (ownsRenderFrame || (flags & D3DGETDATA_FLUSH) == 0)
		{
			return poll(flags);
		}

		const HRESULT result = poll(flags & ~static_cast<DWORD>(D3DGETDATA_FLUSH));
		if (result != S_FALSE || !acquire())
		{
			return result;
		}

		const HRESULT flushed = poll(flags | D3DGETDATA_FLUSH);
		release();
		return flushed;
	}
}
