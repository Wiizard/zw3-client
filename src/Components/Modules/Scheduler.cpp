#include "STDInclude.hpp"

#include "Scheduler.hpp"
#include "Command.hpp"
#include "Dvar.hpp"
#include "Logger.hpp"
#include "Utils/StutterDiagnostics.hpp"

#include <filesystem>
#include <fstream>
#include <iomanip>

namespace Components
{
	constexpr bool conditionContinue = false;
	constexpr bool conditionEnd = true;

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

	static const char* PipelineName(Scheduler::Pipeline type)
	{
		switch (type)
		{
		case Scheduler::Pipeline::ASYNC: return "async_task";
		case Scheduler::Pipeline::RENDERER: return "render_task";
		case Scheduler::Pipeline::SERVER: return "server_task";
		case Scheduler::Pipeline::CLIENT: return "client_task";
		case Scheduler::Pipeline::MAIN: return "main_task";
		default: return "shutdown_task";
		}
	}

	static void SaveStutterDiagnostics()
	{
		auto capture = Utils::StutterDiagnostics::recorder.Stop();
		std::stable_sort(capture.entries.begin(), capture.entries.end(), [](const auto& left, const auto& right)
		{
			return left.started < right.started;
		});
		wchar_t executable[32768]{};
		const auto length = GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
		if (!length || length >= std::size(executable))
		{
			Logger::Error("stutterdiag: could not resolve game directory\n");
			return;
		}
		const auto name = std::format("zw3_stutter_{}_{}.tsv", GetCurrentProcessId(), GetTickCount64());
		const auto path = std::filesystem::path(executable).parent_path() / name;
		std::ofstream file(path, std::ios::binary);
		file << "# DLL build " << __DATE__ << ' ' << __TIME__ << "\n"
			<< "# map " << Dvar::Var("mapname").Get<std::string>()
			<< "; r_rtxRemix " << Dvar::Var("r_rtxRemix").Get<bool>() << "\n"
			<< "# Slow callbacks >=4 ms, rank file reads >=1 ms, native engine frames >=25 ms. Nested timings overlap.\n"
			<< "# Old entries overwritten: " << capture.overwritten << "\n"
			<< "time_ms\tduration_ms\tthread\tcategory\tsource\tline\n" << std::fixed << std::setprecision(3);
		for (const auto& entry : capture.entries)
		{
			std::string_view source = entry.source.file_name();
			const auto slash = source.find_last_of("/\\");
			if (slash != std::string_view::npos) source.remove_prefix(slash + 1);
			file << static_cast<double>(entry.started - capture.started) / 1000000.0 << '\t'
				<< static_cast<double>(entry.duration) / 1000000.0 << '\t' << entry.thread << '\t'
				<< entry.category << '\t' << source << '\t' << entry.source.line() << '\n';
		}
		file.close();
		if (!file)
		{
			Logger::Error("stutterdiag: could not write {}\n", name);
			return;
		}
		Logger::Print("stutterdiag: saved {} slow operations to {} ({} older entries overwritten)\n",
			capture.entries.size(), name, capture.overwritten);
	}

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

				pending.clear();
			});
		});
	}

	void Scheduler::TaskPipeline::Execute(Pipeline type)
	{
		const bool isProfiling = Utils::StutterDiagnostics::recorder.Token() != 0;
		this->callbacks.Access([&](TaskList& tasks)
		{
			// A callback can pump the engine and enter this pipeline again.
			// Merging there would invalidate the outer loop's active task.
			if (this->isExecuting)
			{
				return;
			}

			struct ExecutionScope
			{
				bool& executing;
				explicit ExecutionScope(bool& value) : executing(value) { executing = true; }
				~ExecutionScope() { executing = false; }
			} scope(this->isExecuting);

			this->MergeCallbacks();
			const auto now = std::chrono::steady_clock::now();

			for (auto task = tasks.begin(); task != tasks.end();)
			{
				if ((now - task->lastCall) < task->interval)
				{
					++task;
					continue;
				}

				task->lastCall = now;

				bool result;
				if (isProfiling)
				{
					const Utils::StutterDiagnostics::Scope sample(PipelineName(type), 4, task->source);
					result = task->handler();
				}
				else result = task->handler();

				if (result == conditionEnd)
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
		pipelines[static_cast<int>(type)].Execute(type);
	}

	void Scheduler::Schedule(const std::function<bool()>& callback, Pipeline type,
		std::chrono::milliseconds delay, std::source_location source)
	{
		Task task;
		task.handler = callback;
		task.interval = delay;
		task.lastCall = std::chrono::steady_clock::now();
		task.source = source;

		pipelines[static_cast<int>(type)].Add(std::move(task));
	}

	void Scheduler::Loop(const std::function<void()>& callback, Pipeline type,
		std::chrono::milliseconds delay, std::source_location source)
	{
		Schedule([callback]
		{
			callback();
			return conditionContinue;
		}, type, delay, source);
	}

	void Scheduler::Once(const std::function<void()>& callback, Pipeline type,
		std::chrono::milliseconds delay, std::source_location source)
	{
		Schedule([callback]
		{
			callback();
			return conditionEnd;
		}, type, delay, source);
	}

	void Scheduler::OnGameInitialized(const std::function<void()>& callback, Pipeline type,
		std::chrono::milliseconds delay, std::source_location source)
	{
		Schedule([=]
		{
			if (Game::Sys_IsDatabaseReady2())
			{
				Once(callback, type, delay, source);
				return conditionEnd;
			}

			return conditionContinue;
		}, Pipeline::MAIN, 0ms, source);
	}

	void Scheduler::OnShutdown(const std::function<void()>& callback, std::source_location source)
	{
		Once(callback, Pipeline::QUIT, 0ms, source);
	}

	void Scheduler::MainFrame_Hook()
	{
		{
			const Utils::StutterDiagnostics::Scope sample("engine_frame", 25);
			reinterpret_cast<void(*)()>(mainFrameHook.GetOriginal())();
		}

		Execute(Pipeline::MAIN);
	}

	void Scheduler::CL_Frame_Hook(int localClientNum)
	{
		{
			const Utils::StutterDiagnostics::Scope sample("client_frame", 25);
			reinterpret_cast<void(*)(int)>(clientFrameHook.GetOriginal())(localClientNum);
		}

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
		thread.request_stop();

		if (thread.joinable())
		{
			thread.join();
		}

		Execute(Pipeline::QUIT);

		reinterpret_cast<void(*)(int)>(quitHook.GetOriginal())(block);
	}

	Scheduler::Scheduler()
	{
		Command::Add("stutterdiag", [](const Command::Params* params)
		{
			if (params->Size() == 2 && std::strcmp(params->Get(1), "start") == 0)
			{
				Utils::StutterDiagnostics::recorder.Start(Utils::StutterDiagnostics::Now());
				Logger::Print("stutterdiag: capturing slow operations in memory; use stutterdiag stop after a drop\n");
			}
			else if (params->Size() == 2 && std::strcmp(params->Get(1), "stop") == 0
				&& Utils::StutterDiagnostics::recorder.Token()) SaveStutterDiagnostics();
			else Logger::Print("Usage: stutterdiag start | stutterdiag stop\n");
		});

		mainFrameHook.Initialize(MainFrameCall, MainFrame_Hook, HOOK_CALL)->Install()->Quick();
		clientFrameHook.Initialize(CL_FrameCall, CL_Frame_Hook, HOOK_CALL)->Install()->Quick();
		serverFrameHook.Initialize(G_Glass_UpdateCall, G_Glass_Update_Hook, HOOK_CALL)->Install()->Quick();
		rendererFrameHook.Initialize(ScrPlace_EndFrameCall, ScrPlace_EndFrame_Hook, HOOK_CALL)->Install()->Quick();
		quitHook.Initialize(Sys_SetBlockSystemHotkeysCall, Sys_SetBlockSystemHotkeys_Hook, HOOK_CALL)->Install()->Quick();

		thread = Utils::Thread::CreateNamedThread("Async Scheduler", [](const std::stop_token stopToken)
		{
			while (!stopToken.stop_requested())
			{
				Execute(Pipeline::ASYNC);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
		});
	}
}
