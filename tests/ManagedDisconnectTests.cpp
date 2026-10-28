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

		for (const auto state : { "RESETTING", "POST_MATCH", "FINISHED", "IDLE" })
			Require(Utils::IsCompletedMatchmakingState(state), "Authoritative completion must permit a lobby return");
		for (const auto state : { "SEARCHING", "MAP_VOTE", "SERVER_STARTING", "CONNECTING", "IN_MATCH", "FAILED", "ERROR", "" })
			Require(!Utils::IsCompletedMatchmakingState(state), "An active, unavailable or failed match is not a completed lobby return");
		for (const auto state : { "RESETTING", "POST_MATCH", "FINISHED" })
			Require(Utils::IsConfirmedAssignedMatchEnd("mat_a", "mat_a", state, true), "A confirmed assigned end survives native reset errors");
		Require(!Utils::IsConfirmedAssignedMatchEnd("mat_a", "mat_b", "FINISHED", true), "Another match cannot suppress an error");
		Require(!Utils::IsConfirmedAssignedMatchEnd("", "", "FINISHED", true), "Missing assignments cannot suppress errors");
		Require(!Utils::IsConfirmedAssignedMatchEnd("mat_a", "mat_a", "IDLE", true), "Generic idle is not completion proof");
		Require(!Utils::IsConfirmedAssignedMatchEnd("mat_a", "mat_a", "FAILED", true), "Server failure is not a successful end");
		Require(!Utils::IsConfirmedAssignedMatchEnd("mat_a", "mat_a", "FINISHED", false), "Unconfirmed completion cannot suppress errors");

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
