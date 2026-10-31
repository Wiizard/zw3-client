#pragma once

#include <source_location>

namespace Components
{
	class Scheduler : public Component
	{
	public:
		enum class Pipeline : int
		{
			ASYNC,
			RENDERER,
			SERVER,
			CLIENT,
			MAIN,
			QUIT,
			COUNT,
		};

		Scheduler();

		static void Schedule(const std::function<bool()>& callback, Pipeline type,
			std::chrono::milliseconds delay = std::chrono::milliseconds(0),
			std::source_location source = std::source_location::current());
		static void Loop(const std::function<void()>& callback, Pipeline type,
			std::chrono::milliseconds delay = std::chrono::milliseconds(0),
			std::source_location source = std::source_location::current());
		static void Once(const std::function<void()>& callback, Pipeline type,
			std::chrono::milliseconds delay = std::chrono::milliseconds(0),
			std::source_location source = std::source_location::current());
		static void OnGameInitialized(const std::function<void()>& callback, Pipeline type,
			std::chrono::milliseconds delay = std::chrono::milliseconds(0),
			std::source_location source = std::source_location::current());
		static void OnShutdown(const std::function<void()>& callback,
			std::source_location source = std::source_location::current());

	private:
		struct Task
		{
			std::function<bool()> handler{};
			std::chrono::milliseconds interval{};
			std::chrono::steady_clock::time_point lastCall{};
			std::source_location source;
		};

		using TaskList = std::vector<Task>;

		class TaskPipeline
		{
		public:
			void Add(Task&& task);
			void Execute(Pipeline type);

		private:
			Utils::Concurrency::Container<TaskList> newCallbacks;
			Utils::Concurrency::Container<TaskList, std::recursive_mutex> callbacks;
			bool isExecuting = false;

			void MergeCallbacks();
		};

		static std::jthread thread;
		static TaskPipeline pipelines[];

		static Utils::Hook mainFrameHook;
		static Utils::Hook clientFrameHook;
		static Utils::Hook serverFrameHook;
		static Utils::Hook rendererFrameHook;
		static Utils::Hook quitHook;

		static void Execute(Pipeline type);

		static void MainFrame_Hook();
		static void CL_Frame_Hook(int localClientNum);
		static void G_Glass_Update_Hook(int a1, int a2, const float* a3, const float* a4);
		static void ScrPlace_EndFrame_Hook();
		static void Sys_SetBlockSystemHotkeys_Hook(int block);
	};
}
