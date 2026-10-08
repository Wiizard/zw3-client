#pragma once

#include "Network.hpp"

namespace Components
{
	class Session : public Component
	{
	public:
		static void Send(const Network::Address& target, const std::string& command, const std::string& data = "");
		static void Handle(const std::string& packet, const Network::Callback& callback);
	};
}
