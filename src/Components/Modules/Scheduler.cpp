#include "STDInclude.hpp"

#include "Scheduler.hpp"

namespace Components
{
	constexpr bool conditionContinue = false;
	constexpr bool conditionEnd = true;

	volatile bool Scheduler::kill = false;
	std::jthread Scheduler::thread;
	Scheduler::TaskPipeline Scheduler::pipelines[static_cast<int>(Pipeline::COUNT)];

	Utils::Hook Scheduler::mainFrameHook;
	Utils::Hook Scheduler::clientFrameHook;
	Utils::Hook Scheduler::serverFrameHook;
	Utils::Hook Scheduler::rendererFrameHook;
	Utils::Hook Scheduler::quitHook;

	constexpr std::uintptr_t MainFrameCall = 0x1401F41B4;
	constexpr std::uintptr_t CL_FrameCall = 0x1401F478C;
	constexpr std::uintptr_t G_Glass_UpdateCall = 0x14019EFD4;
	constexpr std::uintptr_t ScrPlace_EndFrameCall = 0x140101D31;
	constexpr std::uintptr_t Sys_SetBlockSystemHotkeysCall = 0x1402A58C1;

	void Scheduler::TaskPipeline::Add(Task&& task)
	{
		this->newCallbacks.Access([&task](TaskList& tasks)
		{
			tasks.emplace_back(std::move(task));
		});
	}

	void Scheduler::TaskPipeline::MergeCallbacks()
	{
		this->callbacks.Access([&](TaskList& tasks)
		{
			this->newCallbacks.Access([&](TaskList& pending)
			{
				tasks.insert(tasks.end(),
					std::move_iterator<TaskList::iterator>(pending.begin()),
					std::move_iterator<TaskList::iterator>(pending.end()));

				pending = {};
			});
		});
	}

	void Scheduler::TaskPipeline::Execute()
	{
		this->callbacks.Access([&](TaskList& tasks)
		{
			this->MergeCallbacks();

			for (auto task = tasks.begin(); task != tasks.end();)
			{
				const auto now = std::chrono::high_resolution_clock::now();

				if ((now - task->lastCall) < task->interval)
				{
					++task;
					continue;
				}

				task->lastCall = now;

				if (task->handler() == conditionEnd)
				{
					task = tasks.erase(task);
					continue;
				}

				++task;
			}
		});
	}

	void Scheduler::Execute(Pipeline type)
	{
		pipelines[static_cast<int>(type)].Execute();
	}

	void Scheduler::Schedule(const std::function<bool()>& callback, Pipeline type,
		std::chrono::milliseconds delay)
	{
		Task task;
		task.handler = callback;
		task.interval = delay;
		task.lastCall = std::chrono::high_resolution_clock::now();

		pipelines[static_cast<int>(type)].Add(std::move(task));
	}

	void Scheduler::Loop(const std::function<void()>& callback, Pipeline type,
		std::chrono::milliseconds delay)
	{
		Schedule([callback]
		{
			callback();
			return conditionContinue;
		}, type, delay);
	}

	void Scheduler::Once(const std::function<void()>& callback, Pipeline type,
		std::chrono::milliseconds delay)
	{
		Schedule([callback]
		{
			callback();
			return conditionEnd;
		}, type, delay);
	}

	void Scheduler::OnGameInitialized(const std::function<void()>& callback, Pipeline type,
		std::chrono::milliseconds delay)
	{
		Schedule([=]
		{
			if (Game::Sys_IsDatabaseReady2())
			{
				Once(callback, type, delay);
				return conditionEnd;
			}

			return conditionContinue;
		}, Pipeline::MAIN);
	}

	void Scheduler::OnShutdown(const std::function<void()>& callback)
	{
		Once(callback, Pipeline::QUIT);
	}

	void Scheduler::MainFrame_Hook()
	{
		reinterpret_cast<void(*)()>(mainFrameHook.GetOriginal())();

		Execute(Pipeline::MAIN);
	}

	void Scheduler::CL_Frame_Hook(int localClientNum)
	{
		reinterpret_cast<void(*)(int)>(clientFrameHook.GetOriginal())(localClientNum);

		Execute(Pipeline::CLIENT);
	}

	void Scheduler::G_Glass_Update_Hook(int a1, int a2, const float* a3, const float* a4)
	{
		reinterpret_cast<void(*)(int, int, const float*, const float*)>(
			serverFrameHook.GetOriginal())(a1, a2, a3, a4);

		Execute(Pipeline::SERVER);
	}

	void Scheduler::ScrPlace_EndFrame_Hook()
	{
		reinterpret_cast<void(*)()>(rendererFrameHook.GetOriginal())();

		Execute(Pipeline::RENDERER);
	}

	void Scheduler::Sys_SetBlockSystemHotkeys_Hook(int block)
	{
		Execute(Pipeline::QUIT);

		kill = true;

		if (thread.joinable())
		{
			thread.join();
		}

		reinterpret_cast<void(*)(int)>(quitHook.GetOriginal())(block);
	}

	Scheduler::Scheduler()
	{
		mainFrameHook.Initialize(MainFrameCall, MainFrame_Hook, HOOK_CALL)->Install()->Quick();
		clientFrameHook.Initialize(CL_FrameCall, CL_Frame_Hook, HOOK_CALL)->Install()->Quick();
		serverFrameHook.Initialize(G_Glass_UpdateCall, G_Glass_Update_Hook, HOOK_CALL)->Install()->Quick();
		rendererFrameHook.Initialize(ScrPlace_EndFrameCall, ScrPlace_EndFrame_Hook, HOOK_CALL)->Install()->Quick();
		quitHook.Initialize(Sys_SetBlockSystemHotkeysCall, Sys_SetBlockSystemHotkeys_Hook, HOOK_CALL)->Install()->Quick();

		thread = Utils::Thread::CreateNamedThread("Async Scheduler", []
		{
			while (!kill)
			{
				Execute(Pipeline::ASYNC);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		});
	}
}
