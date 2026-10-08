#pragma once

#include "Controller/Types.hpp"

#include "Controller/Mapping/Key.hpp"
#include "Controller/Mapping/Logical.hpp"

namespace Controller::Mapping
{
	class BindingTable
	{
	public:
		void Bind(EngineKey key, std::string command);
		void Bind(EngineKey key, Action action);

		void Clear() noexcept;

		const std::string* CommandFor(EngineKey key) const noexcept;

		void ForEach(const std::function<void(EngineKey, const std::string&)>& visit) const;

		std::size_t Size() const noexcept;

	private:
		std::array<std::string, engineKeyCount> commands;
	};

	void ApplyButtonLayout(BindingTable& table, std::string_view name);

	bool MatchesButtonLayout(const BindingTable& table);
}
