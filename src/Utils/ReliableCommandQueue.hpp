#pragma once

#include <cstddef>
#include <deque>
#include <string>
#include <string_view>
#include <utility>

namespace Utils
{
	class ReliableCommandQueue
	{
		struct Command { int type; std::string text; };
		std::deque<Command> commands;
		std::size_t bytes = 0;

	public:
		// Leave room in the engine's 128-entry ring for commands already being prepared.
		static constexpr unsigned nativeWindow = 96;
		static constexpr std::size_t maximumCommands = 4096;
		static constexpr std::size_t maximumBytes = 4 * 1024 * 1024;

		bool Empty() const { return commands.empty(); }
		void Clear() { commands.clear(); bytes = 0; }

		template <typename Pending, typename Send>
		void Drain(const Pending& pending, const Send& send)
		{
			while (!commands.empty() && pending() < nativeWindow)
			{
				auto command = std::move(commands.front());
				commands.pop_front();
				bytes -= command.text.size();
				send(command.type, command.text.c_str());
			}
		}

		template <typename Pending, typename Send>
		bool Submit(int type, const char* text, const Pending& pending, const Send& send)
		{
			Drain(pending, send);
			if (commands.empty() && pending() < nativeWindow)
			{
				send(type, text);
				return true;
			}
			const std::string_view view(text);
			if (commands.size() >= maximumCommands || view.size() > maximumBytes - bytes) return false;
			commands.push_back({type, std::string(view)});
			bytes += view.size();
			return true;
		}
	};
}
