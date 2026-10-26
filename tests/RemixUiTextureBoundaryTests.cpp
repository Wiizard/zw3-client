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
		std::string sent;
		unsigned calls = 0;
		bool succeeds = true;
		const auto write = [&](const std::string& value) { sent = value; ++calls; return succeeds; };
		const auto require = [](bool value, const char* message) { if (!value) throw std::runtime_error(message); };
		const auto has = [&](const char* opinion) { return sent.find(opinion) != std::string::npos; };

		boundary.Update(false, saved, active, withdrawn, write);
		require(has("0x0000000000000010") && !has("-0x0000000000000010")
			&& !has("-0x0000000000000020") && has("-0x0000000000000030"), "Frontend tags were not restored correctly");
		boundary.Update(true, saved, active, withdrawn, write);
		require(has("-0x0000000000000010") && has("-0x0000000000000020")
			&& has("-0x0000000000000030"), "A saved UI texture could still terminate the 3D scene");
		boundary.Update(true, saved, active, withdrawn, write);
		require(calls == 2, "Unchanged frames must not resend the texture lists");
		active.insert(0x40);
		boundary.Update(true, saved, active, withdrawn, write);
		require(has("-0x0000000000000040"), "A late-loaded UI texture escaped the scene boundary");
		active.erase(0x20);
		withdrawn.insert(0x20);
		boundary.Update(false, saved, active, withdrawn, write);
		require(!has("-0x0000000000000010") && !has("-0x0000000000000040")
			&& has("-0x0000000000000020"), "A withdrawn texture was reintroduced on returning to the frontend");
		require(saved == std::unordered_set<std::uint64_t>{ 0x10 }, "Runtime suppression changed saved preferences");
		boundary.Invalidate();
		succeeds = false;
		boundary.Update(true, saved, active, withdrawn, write);
		const unsigned failedCalls = calls;
		succeeds = true;
		boundary.Update(true, saved, active, withdrawn, write);
		require(calls == failedCalls + 1, "A failed API write was not retried");

		std::cout << "Remix UI boundary: saved and late-loaded tags masked during 3D, frontend restoration, cached writes and API retry passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
