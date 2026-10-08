#include "STDInclude.hpp"

#include "Session.hpp"
#include "Scheduler.hpp"

namespace Components
{
	void Session::Send(const Network::Address& target, const std::string& command, const std::string& data)
	{
		Network::SendCommand(target, command, data);

		Scheduler::Once([target, command, data]
		{
			Network::SendCommand(target, command, data);
		}, Scheduler::Pipeline::MAIN, 500ms + std::chrono::milliseconds(std::rand() % 200));
	}

	void Session::Handle(const std::string& packet, const Network::Callback& callback)
	{
		Network::OnPacket(packet, callback);
	}
}
