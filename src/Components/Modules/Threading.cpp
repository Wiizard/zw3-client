#include "STDInclude.hpp"

#include "Threading.hpp"
#include "Dedicated.hpp"
#include "Dvar.hpp"
#include "Logger.hpp"

namespace Components
{
	extern "C"
	{
		void Com_Frame_WaitStub();
		void Com_Frame_DedicatedMsecStub();
		void SV_DedicatedFrameBudgetStub();

		std::uintptr_t Com_Frame_WaitResume = 0;
		std::uintptr_t Com_Frame_DedicatedMsecResume = 0;
		std::uintptr_t SV_DedicatedFrameRun = 0;
		std::uintptr_t SV_DedicatedFrameSkip = 0;
		std::uintptr_t SV_DedicatedTime = 0;
		std::uintptr_t SV_DedicatedTargetTime = 0;

		int Threading_ComFrameWait(int minMsec);
	}

	constexpr std::uintptr_t Com_Frame_WaitLoop = 0x1401F4400;
	constexpr std::uintptr_t Com_Frame_WaitDone = 0x1401F4432;
	constexpr std::uintptr_t Com_Frame_SmoothMsec = 0x1401F446D;
	constexpr std::uintptr_t Com_Frame_ApplyTimeScale = 0x1401F4600;
	static const std::uint8_t smoothMsecEntry[] = { 0x4C, 0x63, 0xD7, 0x44, 0x8B, 0xC8 };
	static const std::uint8_t applyTimeScaleEntry[] = { 0x41, 0x8B, 0xD9 };
	constexpr std::uintptr_t SV_Thread_FrameTest = 0x14023D488;
	constexpr std::uintptr_t SV_Thread_FrameRun = 0x14023D491;
	constexpr std::uintptr_t SV_Thread_FrameSkip = 0x14023D561;
	static const std::uint8_t serverFrameTest[] = { 0x45, 0x85, 0xF6, 0x0F, 0x84, 0xD0, 0x00, 0x00, 0x00 };
	constexpr std::uintptr_t svs_time = 0x14340EC80;
	constexpr std::uintptr_t sv_thread_targetTime = 0x1464FDF0C;

	static const std::uint8_t waitLoop[] =
	{
		0xE8, 0x8B, 0xFB, 0xFF, 0xFF, 0xE8, 0x16, 0x42, 0x0B, 0x00, 0x8B, 0x15, 0xC8, 0x56, 0x9E, 0x01,
		0x8B, 0xC8, 0x3B, 0xC2, 0x0F, 0x48, 0xD0, 0x2B, 0xCA, 0x89, 0x15, 0xB9, 0x56, 0x9E, 0x01, 0x3B,
		0xCB, 0x7D, 0x09, 0x8B, 0xCE, 0xE8, 0x66, 0x67, 0x01, 0x00, 0xEB, 0xD4, 0x89, 0x05, 0xA6, 0x56,
		0x9E, 0x01, 0x48, 0x8B, 0x05, 0x57, 0x56, 0x9E, 0x01, 0x80, 0x78, 0x10, 0x00,
	};

	constexpr std::uintptr_t Com_EventLoop = 0x1401F3F90;
	constexpr std::uintptr_t com_frameTime = 0x141BD9AD8;

	constexpr std::uintptr_t com_sv_running = 0x141BD9A90;
	constexpr std::uintptr_t Com_ServerPacketEvent = 0x1401F5F20;

	constexpr std::uintptr_t ip_socket = 0x14678C430;

	constexpr std::uintptr_t SV_Frame_SV_CheckLoadGameCall = 0x14023C557;
	constexpr std::uintptr_t SV_CheckLoadGame = 0x1402360D0;
	constexpr std::uintptr_t sv_timeResidual = 0x1464FDF04;

	static Utils::Hook frameWaitHook;
	static Utils::Hook serverFrameWaitHook;
	static Utils::Hook dedicatedMsecHook;
	static Utils::Hook dedicatedFrameBudgetHook;

	static double droppedFractionMs = 0.0;

	static void NetSleep(const int waitMs, const bool dispatchEvents = true)
	{
		const SOCKET socketHandle = *reinterpret_cast<const SOCKET*>(Utils::Hook::Rebase(ip_socket));

		if (socketHandle == 0 || socketHandle == INVALID_SOCKET)
		{
			Sleep(static_cast<DWORD>(waitMs));
			return;
		}

		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(socketHandle, &readSet);

		timeval timeout{};
		timeout.tv_sec = waitMs / 1000;
		timeout.tv_usec = (waitMs % 1000) * 1000;

		const int readyCount = select(0, &readSet, nullptr, nullptr, &timeout);

		if (readyCount == SOCKET_ERROR)
		{
			Logger::Warning("WinAPI: select failed: {}\n", WSAGetLastError());
			return;
		}

		if (readyCount <= 0 || !dispatchEvents)
		{
			return;
		}

		const auto* const runningDvar = *reinterpret_cast<Game::dvar_t* const*>(Utils::Hook::Rebase(com_sv_running));

		if (runningDvar && runningDvar->current.enabled)
		{
			reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Com_ServerPacketEvent))();
		}
		else
		{
			reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Com_EventLoop))();
		}
	}

	int Threading_ComFrameWait(int minMsec)
	{
		static const Dvar::Var com_maxfps("com_maxfps");

		int* const frameTime = reinterpret_cast<int*>(Utils::Hook::Rebase(com_frameTime));
		const int maxFps = com_maxfps.Get<int>();
		int targetTime = 0;

		if (maxFps > 0)
		{
			int waitMs = 1000 / maxFps;

			droppedFractionMs += 1000.0 / maxFps - waitMs;

			while (droppedFractionMs >= 1.0)
			{
				waitMs += 1;
				droppedFractionMs -= 1.0;
			}

			targetTime = *frameTime + waitMs;
		}
		else
		{
			targetTime = *frameTime + minMsec;
		}

		for (;;)
		{
			const int remainingMs = targetTime - Game::Sys_Milliseconds();

			if (remainingMs <= 0)
			{
				break;
			}

			if (remainingMs > 2)
			{
				NetSleep(remainingMs - 2);
			}
			else
			{
				NetSleep(0);
			}
		}

		const int lastFrameTime = *frameTime;

		reinterpret_cast<void(*)()>(Utils::Hook::Rebase(Com_EventLoop))();
		*frameTime = Game::Sys_Milliseconds();

		return *frameTime - lastFrameTime;
	}

	static int SV_CheckLoadGame_Hk()
	{
		const int isRestarting = reinterpret_cast<int(*)()>(Utils::Hook::Rebase(SV_CheckLoadGame))();

		if (!isRestarting)
		{
			const int residual = *reinterpret_cast<int*>(Utils::Hook::Rebase(sv_timeResidual));

			if (residual < 50)
			{
				// The native game worker can still own the script VM here. A
				// connect packet dispatched from this pacing hook would enter the
				// same VM on MAIN and overwrite the worker's shared jump buffers.
				// Only wait/wake here; the regular engine event loop owns dispatch.
				NetSleep(50 - residual, false);
			}
		}

		return isRestarting;
	}

	Threading::Threading()
	{
		timeBeginPeriod(1);

		if (Dedicated::IsEnabled())
		{
			// A native worker timeout can race with the main thread's frame event.
			// Both wakeups used to simulate a tick, even if the target was already
			// reached. Only run a game tick while SV_Frame has budgeted one; packet
			// processing, worker ownership and event signaling stay in native code.
			if (Utils::Hook::MatchesBytes(SV_Thread_FrameTest, serverFrameTest, sizeof(serverFrameTest)))
			{
				SV_DedicatedFrameRun = Utils::Hook::Rebase(SV_Thread_FrameRun);
				SV_DedicatedFrameSkip = Utils::Hook::Rebase(SV_Thread_FrameSkip);
				SV_DedicatedTime = Utils::Hook::Rebase(svs_time);
				SV_DedicatedTargetTime = Utils::Hook::Rebase(sv_thread_targetTime);
				if (dedicatedFrameBudgetHook.Initialize(SV_Thread_FrameTest, SV_DedicatedFrameBudgetStub, HOOK_CALL)->Install()->IsInstalled())
				{
					dedicatedFrameBudgetHook.Quick();
					Logger::Print("threading: dedicated worker advances only to the scheduled server time\n");
				}
				else
				{
					Logger::Error("threading: could not install dedicated worker timing fix\n");
				}
			}
			else
			{
				Logger::Error("threading: server frame gate does not match the supported engine; worker timing left unchanged\n");
			}

			// The client's three-frame filter carries time from frames over 40 ms
			// into later frames even though it already passed that time through.
			// A sleeping dedicated server hits this path regularly and advances
			// faster than wall time. Keep the clamped elapsed milliseconds; the
			// native timescale/fixedtime handling and server worker remain intact.
			if (Utils::Hook::MatchesBytes(Com_Frame_SmoothMsec, smoothMsecEntry, sizeof(smoothMsecEntry))
				&& Utils::Hook::MatchesBytes(Com_Frame_ApplyTimeScale, applyTimeScaleEntry, sizeof(applyTimeScaleEntry)))
			{
				Com_Frame_DedicatedMsecResume = Utils::Hook::Rebase(Com_Frame_ApplyTimeScale);
				if (dedicatedMsecHook.Initialize(Com_Frame_SmoothMsec, Com_Frame_DedicatedMsecStub, HOOK_CALL)->Install()->IsInstalled())
				{
					dedicatedMsecHook.Quick();
					Logger::Print("threading: dedicated simulation uses elapsed time without client frame smoothing\n");
				}
				else
				{
					Logger::Error("threading: could not install dedicated frame timing fix\n");
				}
			}
			else
			{
				Logger::Error("threading: frame smoothing does not match the supported engine; dedicated timing left unchanged\n");
			}

			if (!Utils::Hook::BranchesTo(SV_Frame_SV_CheckLoadGameCall, SV_CheckLoadGame, HOOK_CALL)
				|| !serverFrameWaitHook.Initialize(SV_Frame_SV_CheckLoadGameCall, reinterpret_cast<void*>(SV_CheckLoadGame_Hk), HOOK_CALL)->Install()->IsInstalled())
			{
				Logger::Error("threading: could not seat the server frame wait, the dedicated server keeps the engine's pacing\n");
				return;
			}

			serverFrameWaitHook.Quick();
			return;
		}

		if (!Utils::Hook::MatchesBytes(Com_Frame_WaitLoop, waitLoop, sizeof(waitLoop)))
		{
			Logger::Error("threading: Com_Frame's frame wait does not read as expected, left alone, frames keep the engine's pacing\n");
			return;
		}

		Com_Frame_WaitResume = Utils::Hook::Rebase(Com_Frame_WaitDone);

		if (!frameWaitHook.Initialize(Com_Frame_WaitLoop, Com_Frame_WaitStub, HOOK_CALL)->Install()->IsInstalled())
		{
			Logger::Error("threading: could not seat the frame wait hook, frames keep the engine's pacing\n");
			return;
		}

		frameWaitHook.Quick();
	}
}
