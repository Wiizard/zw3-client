#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "Components/Modules/Remix/QueryPoll.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
	void Require(const bool condition, const char* message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	void Check(const DWORD flags, const bool ownsRenderFrame, const bool canAcquire,
		const std::vector<HRESULT>& results, const std::vector<DWORD>& expectedFlags,
		const unsigned expectedAcquires, const unsigned expectedReleases)
	{
		std::vector<DWORD> calls;
		unsigned acquires = 0;
		unsigned releases = 0;
		bool locked = false;
		const auto result = Components::Remix::Device::PollQuery(flags, ownsRenderFrame,
			[&](const DWORD forwardedFlags)
			{
				Require((flags & D3DGETDATA_FLUSH) != 0 || (forwardedFlags & D3DGETDATA_FLUSH) == 0,
					"A non-flushing GPU sync poll was escalated to a Remix flush (startup crash regression)");
				Require(calls.size() < results.size(), "Unexpected extra query poll");
				if (forwardedFlags & D3DGETDATA_FLUSH)
					Require(ownsRenderFrame || locked, "Flush ran without owning the render frame");
				calls.push_back(forwardedFlags);
				return results[calls.size() - 1];
			}, [&]
			{
				++acquires;
				locked = canAcquire;
				return canAcquire;
			}, [&]
			{
				Require(locked, "Released a render frame that was not acquired");
				++releases;
				locked = false;
			});
		Require(calls == expectedFlags, "Query flags or number of calls changed");
		Require(result == results.back(), "Query result was not preserved");
		Require(acquires == expectedAcquires && releases == expectedReleases && !locked,
			"Render frame ownership was not balanced");
	}
}

int main()
{
	try
	{
		for (const bool owner : { false, true })
		{
			for (const HRESULT status : { S_FALSE, S_OK, D3DERR_DEVICELOST })
				Check(0, owner, true, { status }, { 0 }, 0, 0);
		}
		Check(D3DGETDATA_FLUSH, true, false, { S_FALSE }, { D3DGETDATA_FLUSH }, 0, 0);
		Check(D3DGETDATA_FLUSH, false, true, { S_FALSE, S_OK }, { 0, D3DGETDATA_FLUSH }, 1, 1);
		Check(D3DGETDATA_FLUSH, false, true, { S_FALSE, S_FALSE }, { 0, D3DGETDATA_FLUSH }, 1, 1);
		Check(D3DGETDATA_FLUSH, false, true, { S_FALSE, D3DERR_DEVICELOST }, { 0, D3DGETDATA_FLUSH }, 1, 1);
		Check(D3DGETDATA_FLUSH, false, false, { S_FALSE }, { 0 }, 1, 0);
		Check(D3DGETDATA_FLUSH, false, true, { S_OK }, { 0 }, 0, 0);
		Check(D3DGETDATA_FLUSH, false, true, { D3DERR_DEVICELOST }, { 0 }, 0, 0);
		Check(0x80, false, true, { D3DERR_INVALIDCALL }, { 0x80 }, 0, 0);
		Check(D3DGETDATA_FLUSH | 0x80, false, true, { S_FALSE, S_OK }, { 0x80, D3DGETDATA_FLUSH | 0x80 }, 1, 1);
		std::cout << "Remix query polling: non-flush pending/ready/error states, explicit flush, ownership, timeout and flag preservation passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
