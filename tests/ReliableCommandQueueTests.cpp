#include "Utils/ReliableCommandQueue.hpp"
#include <iostream>
#include <stdexcept>
#include <vector>

static void Require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

int main()
{
	try
	{
		Utils::ReliableCommandQueue queue;
		unsigned sequence = 0, acknowledged = 0;
		std::vector<std::pair<int, std::string>> delivered;
		const auto pending = [&] { return sequence - acknowledged; };
		const auto send = [&](int type, const char* command)
		{
			++sequence;
			Require(pending() <= Utils::ReliableCommandQueue::nativeWindow,
				"A stalled client overflowed the native reliable-command ring");
			delivered.emplace_back(type, command);
		};
		// Simulate the host's renderer stalling while scripts keep sending initial dvars and configstrings.
		for (unsigned i = 0; i < 1000; ++i)
			Require(queue.Submit(static_cast<int>(i % 2), ("startup " + std::to_string(i)).c_str(), pending, send),
				"A normal startup burst was rejected");
		Require(delivered.size() == 96 && !queue.Empty(), "Commands were not buffered while acknowledgements stopped");
		for (unsigned frame = 0; frame < 100; ++frame) queue.Drain(pending, send);
		Require(delivered.size() == 96, "Waiting longer bypassed the reliable-command limit");
		while (!queue.Empty())
		{
			acknowledged = sequence;
			queue.Drain(pending, send);
		}
		Require(delivered.size() == 1000, "A deferred command was lost");
		for (unsigned i = 0; i < delivered.size(); ++i)
			Require(delivered[i] == std::pair<int, std::string>(static_cast<int>(i % 2), "startup " + std::to_string(i)),
				"Deferred server commands changed order, type or contents");
		for (std::size_t i = 0; i < Utils::ReliableCommandQueue::maximumCommands; ++i)
			Require(queue.Submit(1, "bounded", [] { return 128u; }, send), "Bounded queue filled too early");
		Require(!queue.Submit(1, "overflow", [] { return 128u; }, send), "Backlog grew without a bound");
		queue.Clear();
		Require(queue.Empty(), "A disconnected client's commands leaked into the next connection");
		const std::string tooLarge(Utils::ReliableCommandQueue::maximumBytes + 1, 'x');
		Require(!queue.Submit(1, tooLarge.c_str(), [] { return 128u; }, send), "Backlog exceeded its byte budget");
		std::cout << "Reliable commands: stalled startup, ordered delivery, native window, disconnect reset and bounds passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
