#pragma once

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
			std::chrono::milliseconds delay = std::chrono::milliseconds(0));
		static void Loop(const std::function<void()>& callback, Pipeline type,
			std::chrono::milliseconds delay = std::chrono::milliseconds(0));
		static void Once(const std::function<void()>& callback, Pipeline type,
			std::chrono::milliseconds delay = std::chrono::milliseconds(0));
		static void OnGameInitialized(const std::function<void()>& callback, Pipeline type,
			std::chrono::milliseconds delay = std::chrono::milliseconds(0));
		static void OnShutdown(const std::function<void()>& callback);

	private:
		struct Task
		{
			std::function<bool()> handler{};
			std::chrono::milliseconds interval{};
			std::chrono::high_resolution_clock::time_point lastCall{};
		};

		using TaskList = std::vector<Task>;

		class TaskPipeline
		{
		public:
			void Add(Task&& task);
			void Execute();

		private:
			Utils::Concurrency::Container<TaskList> newCallbacks;
			Utils::Concurrency::Container<TaskList, std::recursive_mutex> callbacks;

			void MergeCallbacks();
		};

		static volatile bool kill;
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
