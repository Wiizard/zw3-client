#pragma once

#include "Dvar.hpp"
#include "Network.hpp"

namespace Components
{
	class Node : public Component
	{
	public:
		class Data
		{
		public:
			std::uint64_t protocol;
		};

		class Entry
		{
		public:
			Network::Address address;
			Data data;

			std::optional<Utils::Time::Point> lastRequest;
			std::optional<Utils::Time::Point> lastResponse;

			[[nodiscard]] bool IsValid() const;
			[[nodiscard]] bool IsDead() const;

			[[nodiscard]] bool RequiresRequest() const;
			void SendRequest();

			void Reset();
		};

		Node();

		static void Add(const Network::Address& address);
		static std::vector<Entry> GetNodes();
		static void RunFrame();
		static void Synchronize();

	private:
		static std::recursive_mutex mutex;
		static std::vector<Entry> nodes;
		static bool wasIngame;

		static Dvar::Var net_natFix;

		static void HandleResponse(const Network::Address& address, const std::string& data);

		static void SendList(const Network::Address& address);

		static void LoadNodePreset();
		static void LoadNodes();
		static void StoreNodes(bool force);

		static std::uint16_t GetPort();

		static void Migrate();
	};
}
