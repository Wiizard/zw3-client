#include "Utils/ManagedDisconnect.hpp"

#include <iostream>
#include <stdexcept>

namespace
{
	void Require(const bool value, const char* message)
	{
		if (!value) throw std::runtime_error(message);
	}
}

int main()
{
	try
	{
		for (const bool managed : { false, true })
			for (const bool connected : { false, true })
				Require(Utils::ShouldHandleManagedDisconnect(true, managed, connected),
					"Assigned joins and real connection errors must retain their existing handling");
		Require(Utils::ShouldHandleManagedDisconnect(false, true, true),
			"A genuine managed connection must retain the connected fallback");
		Require(!Utils::ShouldHandleManagedDisconnect(false, false, true),
			"An unmanaged private connection must not become matchmaking");
		Require(!Utils::ShouldHandleManagedDisconnect(false, true, false),
			"A stale managed DVAR must not turn completed teardown into a failed join");
		Require(!Utils::ShouldHandleManagedDisconnect(false, false, false),
			"An idle client must not be treated as an assigned connection");

		// Exercise the same predicate at receipt and at queued-worker execution:
		// both engine callbacks may arrive before the first return worker finishes.
		const bool firstQueued = Utils::ShouldHandleManagedDisconnect(true, true, true);
		const bool duplicateQueued = Utils::ShouldHandleManagedDisconnect(true, true, false);
		Require(firstQueued && duplicateQueued, "The test must cover both callbacks queued while assigned");
		const bool hasAssignmentAfterCompletedReturn = false;
		Require(!Utils::ShouldHandleManagedDisconnect(hasAssignmentAfterCompletedReturn, duplicateQueued, false),
			"The queued duplicate must revalidate after the preceding completed return clears the assignment");
		Require(Utils::ShouldHandleManagedDisconnect(true, duplicateQueued, false),
			"A still-assigned failed join must not lose its native error");
		std::cout << "Managed disconnect assignment, completed-return duplicate and failed-join tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
