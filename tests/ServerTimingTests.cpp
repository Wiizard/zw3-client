#include "Utils/SnapshotRatePolicy.hpp"
#include "Utils/DedicatedTimeBudget.hpp"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>

extern "C"
{
	std::uintptr_t Dedicated_FrameMsec_Resume = 0;
	std::uintptr_t Com_Frame_SvRunningSlot = 0;
	void Dedicated_ComClampMsec(int) {}
	int TestDedicatedFrameMsec(int elapsed);
	int TestDedicatedSnapshotInterval(int requested, int tickFloor, int expected);
}

int main()
{
	// Execute the production MASM continuation, including its stack/register
	// contract. Uneven elapsed durations must neither be smoothed nor counted
	// again when the dedicated loop hands them to native timescale processing.
	for (const int elapsed : {0, 1, 3, 8, 16, 33, 40, 41, 49, 50, 57, 100, 500})
		assert(TestDedicatedFrameMsec(elapsed) == 0);
	// Exercise the production userinfo continuation: online players requesting
	// snaps 30 reach the 20 Hz simulation floor, while slower requests survive.
	for (const int requested : {0, 33, 49, 50, 100, 200, 1000, std::numeric_limits<int>::max()})
		assert(TestDedicatedSnapshotInterval(requested, 50, std::max(requested, 50)) == 0);

	// Reproduce timeout-driven worker progress while the main thread stalls.
	// Reconciliation must conserve elapsed budget, including across repeated
	// autonomous ticks, a paused main thread, and a fresh map's reset clocks.
	for (const int ahead : {0, 50, 100, 400, 2050})
	{
		int target = 1000, residual = 17;
		Utils::DedicatedTimeBudget::Reconcile(1000 + ahead, target, residual);
		assert(target == 1000 + ahead);
		assert(target + residual == 1017);
		residual += ahead + 33;
		while (residual >= 50) { target += 50; residual -= 50; }
		assert(target == 1050 + ahead && residual == 0);
	}
	for (const int worker : {0, 50, 1000})
	{
		int target = 1000, residual = 17;
		Utils::DedicatedTimeBudget::Reconcile(worker, target, residual);
		assert(target == 1000 && residual == 17);
	}

	using Utils::SnapshotRatePolicy::Deadline;
	assert(Deadline(100, 57, 157) == 115);
	assert(Deadline(150, 115, 207) == 173);
	assert(Deadline(100, 100, 157) == 158);
	assert(Deadline(100, 101, 157) == 157); // Early sends cannot borrow future budget.
	assert(Deadline(100, 0, 157) == 157); // New connection.
	assert(Deadline(100, 50, 157) == 157); // Hitch: discard old credit.
	assert(Deadline(100, 57, 100) == 100); // No new native deadline.
	assert(Deadline(100, 57, 110) == 101); // Never eligible twice in this tick.
	assert(Deadline(10, 9000, 67) == 67); // Map clock restarted.
	assert(Deadline(std::numeric_limits<int>::max(), 1, 57) == 57);

	// Full 1400-byte packet plus 48-byte overhead at the native 25000 B/s
	// cap costs 57.92 ms (native truncates to 57). At a 50 ms tick, retain
	// the conservative 58 ms average rate instead of
	// repeatedly rounding each individual packet up to a 100 ms delay.
	int due = 57, count = 0;
	for (int now = 50; now <= 60000; now += 50)
	{
		if (now < due) continue;
		due = Deadline(now, due, now + 57);
		assert(due > now);
		++count;
	}
	assert(count >= 1033 && count <= 1035);
	assert(count * 1448 <= 25000 * 60 + 1448);
	std::cout << "13 production MASM timing cases, 8 production MASM snapshot intervals, 8 worker budget cases and 11 snapshot scheduling checks passed; "
		<< count << " snapshots/60s within the native byte budget\n";
}
