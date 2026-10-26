#include "Components/Modules/Remix/UiTextureBoundary.hpp"

#include <iostream>
#include <stdexcept>

int main()
{
	try
	{
		Components::Remix::UiTextureBoundary boundary;
		std::unordered_set<std::uint64_t> saved{ 0x10 };
		std::unordered_set<std::uint64_t> active{ 0x10, 0x20 };
		std::unordered_set<std::uint64_t> withdrawn{ 0x30 };
		std::unordered_set<std::uint64_t> included{ 0x10, 0x30 };
		std::unordered_set<std::uint64_t> excluded;
		unsigned calls = 0;
		bool succeeds = true;
		const auto write = [&](std::uint64_t hash, bool include)
		{
			++calls;
			if (!succeeds) return false;
			(include ? excluded : included).erase(hash);
			(include ? included : excluded).insert(hash);
			return true;
		};
		const auto require = [](bool value, const char* message) { if (!value) throw std::runtime_error(message); };

		boundary.Update(false, saved, active, withdrawn, write);
		require(included.contains(0x10) && included.contains(0x20) && !included.contains(0x30)
			&& excluded.contains(0x30), "Frontend tags were not restored correctly");
		boundary.Update(true, saved, active, withdrawn, write);
		require(included.empty() && excluded.contains(0x10) && excluded.contains(0x20)
			&& excluded.contains(0x30), "A saved UI texture could still terminate the 3D scene");
		const unsigned suppressedCalls = calls;
		boundary.Update(true, saved, active, withdrawn, write);
		require(calls == suppressedCalls, "Unchanged frames must not resend the texture lists");
		active.insert(0x40);
		boundary.Update(true, saved, active, withdrawn, write);
		require(excluded.contains(0x40) && !included.contains(0x40), "A late-loaded UI texture escaped the scene boundary");
		active.erase(0x20);
		withdrawn.insert(0x20);
		boundary.Update(false, saved, active, withdrawn, write);
		require(included.contains(0x10) && included.contains(0x40) && !excluded.contains(0x10)
			&& !excluded.contains(0x40) && !included.contains(0x20) && excluded.contains(0x20),
			"A withdrawn texture was reintroduced on returning to the frontend");
		require(saved == std::unordered_set<std::uint64_t>{ 0x10 }, "Runtime suppression changed saved preferences");
		boundary.Invalidate();
		succeeds = false;
		boundary.Update(true, saved, active, withdrawn, write);
		const unsigned failedCalls = calls;
		succeeds = true;
		boundary.Update(true, saved, active, withdrawn, write);
		require(calls > failedCalls && included.empty(), "A failed API write was not retried");

		std::cout << "Remix UI boundary: saved and late-loaded tags masked during 3D, frontend restoration, cached writes and API retry passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
