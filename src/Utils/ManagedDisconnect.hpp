#pragma once

namespace Utils
{
	constexpr bool ShouldHandleManagedDisconnect(const bool hasAssignedMatch, const bool isManaged, const bool wasConnected)
	{
		// The engine's normal disconnect command also performs a second,
		// unconnected Com_Shutdown callback. The managed DVAR can still be set
		// until the main-thread lobby return runs; it is not an assignment.
		return hasAssignedMatch || (isManaged && wasConnected);
	}
}
