#include "STDInclude.hpp"

#include "IPCPipe.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "Singleton.hpp"

namespace Components
{
	Pipe IPCPipe::serverPipe;
	Pipe IPCPipe::clientPipe;

	constexpr const char* pipeNameServer = "ZW3-Server";
	constexpr const char* pipeNameClient = "ZW3-Client";
	constexpr unsigned int maxReconnects = 3;

	Pipe::~Pipe()
	{
		this->Destroy();
	}

	bool Pipe::Connect(const std::string& name)
	{
		this->Destroy();

		this->type = Type::Client;
		this->pipeFile = std::format("\\\\.\\Pipe\\{}", name);
		this->pipe = CreateFileA(this->pipeFile.data(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);

		if (this->pipe == INVALID_HANDLE_VALUE)
		{
			if (this->reconnectAttempt < maxReconnects)
			{
				++this->reconnectAttempt;
				std::this_thread::sleep_for(std::chrono::milliseconds(500));
				return this->Connect(name);
			}

			this->Destroy();
			return false;
		}

		this->reconnectAttempt = 0;
		return true;
	}

	bool Pipe::Create(const std::string& name)
	{
		this->Destroy();

		this->type = Type::Server;
		this->pipeFile = std::format("\\\\.\\Pipe\\{}", name);
		this->pipe = CreateNamedPipeA(this->pipeFile.data(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES, sizeof(this->packet), sizeof(this->packet), NMPWAIT_USE_DEFAULT_WAIT, nullptr);

		if (this->pipe == INVALID_HANDLE_VALUE || !this->pipe)
		{
			this->Destroy();
			return false;
		}

		this->isThreadAttached = true;
		this->thread = std::jthread(ReceiveThread, this);
		return true;
	}

	void Pipe::OnConnect(const std::function<void()>& callback)
	{
		this->connectCallback = callback;
	}

	void Pipe::SetCallback(const std::string& command, const PacketCallback& callback)
	{
		this->packetCallbacks[command] = callback;
	}

	bool Pipe::Write(const std::string& command, const std::string& data)
	{
		if (this->type != Type::Client || this->pipe == INVALID_HANDLE_VALUE || !this->pipe)
		{
			return false;
		}

		Packet outgoing{};
		strncpy_s(outgoing.command, command.data(), _TRUNCATE);
		strncpy_s(outgoing.buffer, data.data(), _TRUNCATE);

		DWORD written = 0;
		return WriteFile(this->pipe, &outgoing, sizeof(outgoing), &written, nullptr) || GetLastError() == ERROR_IO_PENDING;
	}

	void Pipe::Destroy()
	{
		if (this->pipe && this->pipe != INVALID_HANDLE_VALUE)
		{
			CancelIoEx(this->pipe, nullptr);

			if (this->type == Type::Server)
			{
				DisconnectNamedPipe(this->pipe);
			}

			CloseHandle(this->pipe);
		}

		this->pipe = INVALID_HANDLE_VALUE;
		this->isThreadAttached = false;

		if (this->thread.joinable())
		{
			this->thread.join();
		}
	}

	void Pipe::ReceiveThread(Pipe* pipe)
	{
		if (!pipe || pipe->type != Type::Server || pipe->pipe == INVALID_HANDLE_VALUE || !pipe->pipe)
		{
			return;
		}

		if (!ConnectNamedPipe(pipe->pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED)
		{
			return;
		}

		if (pipe->connectCallback)
		{
			pipe->connectCallback();
		}

		while (pipe->isThreadAttached && pipe->pipe && pipe->pipe != INVALID_HANDLE_VALUE)
		{
			DWORD read = 0;

			if (ReadFile(pipe->pipe, &pipe->packet, sizeof(pipe->packet), &read, nullptr) && read)
			{
				pipe->packet.command[commandSize - 1] = 0;
				pipe->packet.buffer[bufferSize - 1] = 0;

				const auto callback = pipe->packetCallbacks.find(pipe->packet.command);

				if (callback != pipe->packetCallbacks.end())
				{
					callback->second(pipe->packet.buffer);
				}
			}
			else if (pipe->isThreadAttached && pipe->pipe != INVALID_HANDLE_VALUE)
			{
				DisconnectNamedPipe(pipe->pipe);
				ConnectNamedPipe(pipe->pipe, nullptr);

				if (pipe->connectCallback)
				{
					pipe->connectCallback();
				}
			}

			ZeroMemory(&pipe->packet, sizeof(pipe->packet));
		}
	}

	void IPCPipe::ConnectClient()
	{
		if (Singleton::IsFirstInstance())
		{
			clientPipe.Connect(pipeNameClient);
		}
	}

	bool IPCPipe::Write(const std::string& command, const std::string& data)
	{
		return clientPipe.Write(command, data);
	}

	void IPCPipe::On(const std::string& command, const Pipe::PacketCallback& callback)
	{
		serverPipe.SetCallback(command, callback);
	}

	IPCPipe::IPCPipe()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		serverPipe.OnConnect(ConnectClient);

		if (!Singleton::IsFirstInstance())
		{
			clientPipe.Connect(pipeNameServer);
		}

		Scheduler::Once([]
		{
			const char* name = pipeNameClient;

			if (Singleton::IsFirstInstance())
			{
				name = pipeNameServer;
			}

			if (!serverPipe.Create(name))
			{
				Logger::Error("ipcpipe: could not create the pipe, iw4x64:// links cannot reach this copy\n");
			}
		}, Scheduler::Pipeline::MAIN);

		On("ping", [](const std::string& data)
		{
			Logger::Print("Received ping from pipe, sending pong!\n");
			Write("pong", data);
		});

		On("pong", []([[maybe_unused]] const std::string& data)
		{
			Logger::Print("Received pong from pipe!\n");
		});

		Command::Add("ipcping", []()
		{
			Logger::Print("Sending ping to pipe!\n");
			Write("ping", {});
		});
	}
}
