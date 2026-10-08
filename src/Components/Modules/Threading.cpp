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

		std::uintptr_t Com_Frame_WaitResume = 0;

		int Threading_ComFrameWait(int minMsec);
	}

	constexpr std::uintptr_t Com_Frame_WaitLoop = 0x1401F4400;
	constexpr std::uintptr_t Com_Frame_WaitDone = 0x1401F4432;

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

	static double droppedFractionMs = 0.0;

	static void NetSleep(const int waitMs)
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

		if (readyCount <= 0)
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
				NetSleep(50 - residual);
			}
		}

		return isRestarting;
	}

	Threading::Threading()
	{
		timeBeginPeriod(1);

		if (Dedicated::IsEnabled())
		{
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
