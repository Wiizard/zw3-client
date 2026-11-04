#pragma once

#include <string_view>

namespace Utils
{
	constexpr bool IsCompletedMatchmakingState(const std::string_view state)
	{
		return state == "RESETTING" || state == "POST_MATCH" || state == "FINISHED" || state == "IDLE";
	}

	constexpr bool ShouldHandleManagedDisconnect(const bool hasAssignedMatch, const bool isManaged, const bool wasConnected)
	{
		// The engine's normal disconnect command also performs a second,
		// unconnected Com_Shutdown callback. The managed DVAR can still be set
		// until the main-thread lobby return runs; it is not an assignment.
		return hasAssignedMatch || (isManaged && wasConnected);
	}

	constexpr bool IsConfirmedAssignedMatchEnd(const std::string_view expectedMatch, const std::string_view reportedMatch,
		const std::string_view state, const bool completed)
	{
		return completed && !expectedMatch.empty() && expectedMatch == reportedMatch
			&& (state == "RESETTING" || state == "POST_MATCH" || state == "FINISHED");
	}
}
