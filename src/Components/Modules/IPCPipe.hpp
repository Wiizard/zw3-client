#pragma once

namespace Components
{
	class Pipe
	{
	public:
		static constexpr std::size_t commandSize = 100;
		static constexpr std::size_t bufferSize = 0x2000;

		struct Packet
		{
			char command[commandSize];
			char buffer[bufferSize];
		};

		enum class Type
		{
			None,
			Server,
			Client,
		};

		using PacketCallback = std::function<void(const std::string& data)>;

		Pipe() = default;
		~Pipe();

		Pipe(const Pipe&) = delete;
		Pipe& operator=(const Pipe&) = delete;

		bool Connect(const std::string& name);
		bool Create(const std::string& name);
		bool Write(const std::string& command, const std::string& data);
		void SetCallback(const std::string& command, const PacketCallback& callback);
		void OnConnect(const std::function<void()>& callback);
		void Destroy();

	private:
		std::function<void()> connectCallback;
		std::map<std::string, PacketCallback> packetCallbacks;
		HANDLE pipe = INVALID_HANDLE_VALUE;
		std::jthread thread;
		std::atomic<bool> isThreadAttached = false;
		Type type = Type::None;
		Packet packet{};
		std::string pipeFile;
		unsigned int reconnectAttempt = 0;

		static void ReceiveThread(Pipe* pipe);
	};

	class IPCPipe : public Component
	{
	public:
		IPCPipe();

		static bool Write(const std::string& command, const std::string& data);
		static void On(const std::string& command, const Pipe::PacketCallback& callback);

	private:
		static Pipe serverPipe;
		static Pipe clientPipe;

		static void ConnectClient();
	};
}
