#include "STDInclude.hpp"

#include <Utils/Compression.hpp>

#include <proto/node.pb.h>

#include "Node.hpp"
#include "Command.hpp"
#include "Dedicated.hpp"
#include "Events.hpp"
#include "FileSystem.hpp"
#include "Logger.hpp"
#include "Scheduler.hpp"
#include "ServerInfo.hpp"
#include "ServerList.hpp"
#include "Session.hpp"
#include "ZoneBuilder.hpp"

namespace Components
{
	constexpr int halfLife = 3 * 60 * 1000;
	constexpr std::size_t maxNodesToSend = 64;
	constexpr auto sendRate = 500ms;

	std::recursive_mutex Node::mutex;
	std::vector<Node::Entry> Node::nodes;

	bool Node::wasIngame = false;

	Dvar::Var Node::net_natFix;

	bool Node::Entry::IsValid() const
	{
		return this->lastResponse.has_value() && !this->lastResponse->Elapsed(halfLife * 2);
	}

	bool Node::Entry::IsDead() const
	{
		if (!this->lastResponse.has_value())
		{
			if (this->lastRequest.has_value() && this->lastRequest->Elapsed(halfLife))
			{
				return true;
			}
		}
		else if (this->lastResponse->Elapsed(halfLife * 2) && this->lastRequest.has_value() && this->lastRequest->After(*this->lastResponse))
		{
			return true;
		}

		return false;
	}

	bool Node::Entry::RequiresRequest() const
	{
		return !this->IsDead() && (!this->lastRequest.has_value() || this->lastRequest->Elapsed(halfLife));
	}

	void Node::Entry::SendRequest()
	{
		if (!this->lastRequest.has_value())
		{
			this->lastRequest.emplace();
		}

		this->lastRequest->Update();

		Session::Send(this->address, "nodeListRequest");
		SendList(this->address);
	}

	void Node::Entry::Reset()
	{
		this->lastRequest.reset();
	}

	void Node::LoadNodePreset()
	{
		Proto::Node::List list;

		FileSystem::File defaultNodes("nodes_default.dat");

		if (!defaultNodes.Exists() || !list.ParseFromString(Utils::Compression::ZLib::Decompress(defaultNodes.GetBuffer())))
		{
			return;
		}

		for (auto i = 0; i < list.nodes_size(); ++i)
		{
			const auto& node = list.nodes(i);

			if (node.size() == sizeof(sockaddr))
			{
				Add(Network::Address(reinterpret_cast<const sockaddr*>(node.data())));
			}
		}
	}

	void Node::LoadNodes()
	{
		std::string data;

		if (!Utils::IO::ReadFile("players/nodes.json", &data) || data.empty())
		{
			return;
		}

		nlohmann::json json;

		try
		{
			json = nlohmann::json::parse(data);
		}
		catch (const std::exception& ex)
		{
			Logger::Error("JSON Parse Error: {}\n", ex.what());
			return;
		}

		if (!json.contains("nodes"))
		{
			Logger::Error("nodes.json contains invalid data\n");
			return;
		}

		const auto& list = json["nodes"];

		if (!list.is_array())
		{
			return;
		}

		const nlohmann::json::array_t entries = list;
		Logger::Print("Parsing {} nodes from nodes.json\n", entries.size());

		for (const auto& entry : entries)
		{
			if (entry.is_string())
			{
				const Network::Address address(entry.get<std::string>());
				Add(address);
			}
		}
	}

	void Node::StoreNodes(bool force)
	{
		if (Dedicated::IsEnabled() && Dedicated::sv_lanOnly.Get<bool>())
		{
			return;
		}

		std::vector<std::string> addresses;

		static Utils::Time::Interval interval;

		if (!force && !interval.Elapsed(1min))
		{
			return;
		}

		interval.Update();

		{
			std::lock_guard _(mutex);

			for (const auto& node : nodes)
			{
				if (node.IsValid() || force)
				{
					addresses.emplace_back(node.address.GetString());
				}
			}
		}

		nlohmann::json out;
		out["nodes"] = addresses;

		Utils::IO::WriteFile("players/nodes.json", out.dump());
	}

	void Node::Add(const Network::Address& address)
	{
#ifndef DEBUG
		if (address.IsLocal() || address.IsSelf())
		{
			return;
		}
#endif

		if (!address.IsValid())
		{
			return;
		}

		std::lock_guard _(mutex);

		for (const auto& node : nodes)
		{
			if (node.address == address)
			{
				return;
			}
		}

		Entry node;
		node.address = address;

		nodes.push_back(node);
	}

	std::vector<Node::Entry> Node::GetNodes()
	{
		std::lock_guard _(mutex);

		return nodes;
	}

	void Node::RunFrame()
	{
		if (Dedicated::IsEnabled() && Dedicated::sv_lanOnly.Get<bool>())
		{
			return;
		}

		if (!Dedicated::IsEnabled())
		{
			if (ServerList::useMasterServer)
			{
				return;
			}

			if (Game::CL_GetLocalClientConnectionState(0) != Game::CA_DISCONNECTED)
			{
				wasIngame = true;
				return;
			}
		}

		if (wasIngame)
		{
			for (auto& entry : nodes)
			{
				entry.lastRequest.reset();
				entry.lastResponse.reset();
			}

			wasIngame = false;
		}

		static Utils::Time::Interval frameLimit;
		const auto interval = 1000 / std::max(1, ServerList::netServerFrames.Get<int>());

		if (!frameLimit.Elapsed(std::chrono::milliseconds(interval)))
		{
			return;
		}

		frameLimit.Update();

		std::lock_guard _(mutex);

		auto sentRequests = 0;

		for (auto i = nodes.begin(); i != nodes.end();)
		{
			if (i->IsDead())
			{
				i = nodes.erase(i);
				continue;
			}

			if (sentRequests < ServerList::netServerQueryLimit.Get<int>() && i->RequiresRequest())
			{
				++sentRequests;
				i->SendRequest();
			}

			++i;
		}
	}

	void Node::Synchronize()
	{
		std::lock_guard _(mutex);

		for (auto& node : nodes)
		{
			node.Reset();
		}
	}

	void Node::HandleResponse(const Network::Address& address, const std::string& data)
	{
		Proto::Node::List list;

		if (!list.ParseFromString(data))
		{
			return;
		}

		std::lock_guard _(mutex);

		for (auto i = 0; i < list.nodes_size(); ++i)
		{
			const auto& node = list.nodes(i);

			if (node.size() == sizeof(sockaddr))
			{
				Add(Network::Address(reinterpret_cast<const sockaddr*>(node.data())));
			}
		}

		if (list.isnode() && (!list.port() || list.port() == address.GetPort()))
		{
			const auto protocol = static_cast<std::uint64_t>(ServerInfo::GetProtocol());

			if (!Dedicated::IsEnabled() && ServerList::IsOnlineList() && !ServerList::useMasterServer && list.protocol() == protocol)
			{
				ServerList::InsertRequest(address);
			}

			for (auto& node : nodes)
			{
				if (address == node.address)
				{
					if (!node.lastResponse.has_value())
					{
						node.lastResponse.emplace();
					}

					node.lastResponse->Update();

					node.data.protocol = list.protocol();
					return;
				}
			}

			Entry entry;
			entry.address = address;
			entry.data.protocol = list.protocol();
			entry.lastResponse.emplace();

			nodes.push_back(entry);
		}
	}

	void Node::SendList(const Network::Address& address)
	{
		std::lock_guard _(mutex);

		std::vector<std::string> nodeListResponseMessages;

		for (std::size_t currentNode = 0; currentNode < nodes.size();)
		{
			Proto::Node::List list;
			list.set_isnode(Dedicated::IsEnabled());
			list.set_protocol(static_cast<std::uint64_t>(ServerInfo::GetProtocol()));
			list.set_port(GetPort());

			for (std::size_t i = 0; i < maxNodesToSend;)
			{
				if (currentNode >= nodes.size())
				{
					break;
				}

				const auto& node = nodes.at(currentNode++);

				if (node.IsValid())
				{
					auto* entry = list.add_nodes();

					const auto sockAddr = node.address.GetSockAddr();
					entry->append(reinterpret_cast<const char*>(&sockAddr), sizeof(sockAddr));

					++i;
				}
			}

			nodeListResponseMessages.push_back(list.SerializeAsString());
		}

		auto i = 0;

		for (const auto& nodeListData : nodeListResponseMessages)
		{
			Scheduler::Once([=]
			{
				Session::Send(address, "nodeListResponse", nodeListData);
			}, Scheduler::Pipeline::MAIN, sendRate * i++);
		}
	}

	std::uint16_t Node::GetPort()
	{
		if (net_natFix.Get<bool>())
		{
			return 0;
		}

		return Network::GetPort();
	}

	void Node::Migrate()
	{
		Proto::Node::List list;
		std::string data;

		if (!Utils::IO::ReadFile("players/nodes.dat", &data) || data.empty())
		{
			return;
		}

		if (!list.ParseFromString(Utils::Compression::ZLib::Decompress(data)))
		{
			return;
		}

		std::vector<std::string> addresses;

		for (auto i = 0; i < list.nodes_size(); ++i)
		{
			const auto& node = list.nodes(i);

			if (node.size() == sizeof(sockaddr))
			{
				const Network::Address address(reinterpret_cast<const sockaddr*>(node.data()));
				addresses.emplace_back(address.GetString());
			}
		}

		nlohmann::json out;
		out["nodes"] = addresses;

		if (!Utils::IO::FileExists("players/nodes.json"))
		{
			Utils::IO::WriteFile("players/nodes.json", out.dump());
		}

		Utils::IO::RemoveFile("players/nodes.dat");
	}

	Node::Node()
	{
		if (ZoneBuilder::IsEnabled())
		{
			return;
		}

		Events::OnDvarInit([]
		{
			net_natFix = Dvar::Register("net_natFix", false, Game::DVAR_NONE, "Fix node registration for certain firewalls/routers");
		});

		Scheduler::Loop([]
		{
			StoreNodes(false);
		}, Scheduler::Pipeline::ASYNC, 5min);

		Scheduler::Loop(RunFrame, Scheduler::Pipeline::MAIN);

		Command::Add("listNodes", []([[maybe_unused]] const Command::Params* params)
		{
			Logger::Print("Nodes: {}\n", nodes.size());

			std::lock_guard _(mutex);

			for (const auto& node : nodes)
			{
				std::string_view validity = "Invalid";

				if (node.IsValid())
				{
					validity = "Valid";
				}

				Logger::Print("{}\t({})\n", node.address.GetString(), validity);
			}
		});

		Command::Add("addNode", [](const Command::Params* params)
		{
			if (params->Size() < 2)
			{
				return;
			}

			const Network::Address address{ std::string(params->Get(1)) };

			if (address.IsValid())
			{
				Add(address);
			}
		});
	}
}
