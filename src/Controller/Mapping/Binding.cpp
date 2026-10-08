#include "STDInclude.hpp"

#include "Controller/Mapping/Binding.hpp"

namespace Controller::Mapping
{
	struct ButtonLayout
	{
		const char* name;
		const char* keyword;

		Action triggerRight;
		Action triggerLeft;
		Action shoulderRight;
		Action shoulderLeft;
		Action stickRight;
		Action stickLeft;
		Action faceB;
	};

	static constexpr ButtonLayout layouts[] =
	{
		{ "buttons_default", "default", Action::Fire, Action::Ads, Action::Frag, Action::SpecialGrenade, Action::Melee, Action::Sprint, Action::Stance },
		{ "buttons_tactical", "tactical", Action::Fire, Action::Ads, Action::Frag, Action::SpecialGrenade, Action::Stance, Action::Sprint, Action::Melee },
		{ "buttons_lefty", "lefty", Action::Ads, Action::Fire, Action::SpecialGrenade, Action::Frag, Action::Sprint, Action::Melee, Action::Stance },
		{ "buttons_nomad", "nomad", Action::Fire, Action::AdsToggle, Action::Frag, Action::SpecialGrenade, Action::Stance, Action::Sprint, Action::Melee },
	};

	static constexpr std::string_view altSuffix = "_alt";

	static std::string Lowercase(std::string_view text)
	{
		std::string lowered(text);

		for (auto& character : lowered)
		{
			character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
		}

		return lowered;
	}

	static const ButtonLayout& LayoutFor(const std::string& name) noexcept
	{
		for (const auto& layout : layouts)
		{
			if (name == layout.name)
			{
				return layout;
			}
		}

		for (const auto& layout : layouts)
		{
			if (name.find(layout.keyword) != std::string::npos)
			{
				return layout;
			}
		}

		return layouts[0];
	}

	static bool HaveSameBindings(const BindingTable& left, const BindingTable& right) noexcept
	{
		for (const auto key : Keys())
		{
			const auto* leftCommand = left.CommandFor(key);
			const auto* rightCommand = right.CommandFor(key);

			if ((leftCommand == nullptr) != (rightCommand == nullptr))
			{
				return false;
			}

			if (leftCommand != nullptr && *leftCommand != *rightCommand)
			{
				return false;
			}
		}

		return true;
	}

	void BindingTable::Bind(EngineKey key, std::string command)
	{
		this->commands[KeyIndex(key)] = std::move(command);
	}

	void BindingTable::Bind(EngineKey key, Action action)
	{
		this->commands[KeyIndex(key)] = Mapping::CommandFor(action);
	}

	void BindingTable::Clear() noexcept
	{
		for (auto& command : this->commands)
		{
			command.clear();
		}
	}

	const std::string* BindingTable::CommandFor(EngineKey key) const noexcept
	{
		const auto& command = this->commands[KeyIndex(key)];

		if (command.empty())
		{
			return nullptr;
		}

		return &command;
	}

	void BindingTable::ForEach(const std::function<void(EngineKey, const std::string&)>& visit) const
	{
		const auto all = Keys();

		for (std::size_t i = 0; i < engineKeyCount; ++i)
		{
			if (!this->commands[i].empty())
			{
				visit(all[i], this->commands[i]);
			}
		}
	}

	std::size_t BindingTable::Size() const noexcept
	{
		std::size_t bound = 0;

		for (const auto& command : this->commands)
		{
			if (!command.empty())
			{
				++bound;
			}
		}

		return bound;
	}

	void ApplyButtonLayout(BindingTable& table, std::string_view name)
	{
		std::string lowered = Lowercase(name);

		const bool isAlt = lowered.size() > altSuffix.size() && lowered.compare(lowered.size() - altSuffix.size(), altSuffix.size(), altSuffix) == 0;

		if (isAlt)
		{
			lowered.resize(lowered.size() - altSuffix.size());
		}

		const auto& layout = LayoutFor(lowered);

		table.Clear();

		table.Bind(EngineKey::ButtonStart, Action::Menu);
		table.Bind(EngineKey::ButtonBack, Action::Scoreboard);

		table.Bind(EngineKey::ButtonA, Action::JumpStand);
		table.Bind(EngineKey::ButtonB, layout.faceB);
		table.Bind(EngineKey::ButtonX, Action::UseReload);
		table.Bind(EngineKey::ButtonY, Action::NextWeapon);

		table.Bind(EngineKey::ButtonRStick, layout.stickRight);
		table.Bind(EngineKey::ButtonLStick, layout.stickLeft);

		if (isAlt)
		{
			table.Bind(EngineKey::ButtonRShoulder, layout.triggerRight);
			table.Bind(EngineKey::ButtonLShoulder, layout.triggerLeft);
			table.Bind(EngineKey::ButtonRTrigger, layout.shoulderRight);
			table.Bind(EngineKey::ButtonLTrigger, layout.shoulderLeft);
		}
		else
		{
			table.Bind(EngineKey::ButtonRTrigger, layout.triggerRight);
			table.Bind(EngineKey::ButtonLTrigger, layout.triggerLeft);
			table.Bind(EngineKey::ButtonRShoulder, layout.shoulderRight);
			table.Bind(EngineKey::ButtonLShoulder, layout.shoulderLeft);
		}

		table.Bind(EngineKey::DpadUp, Action::ActionSlot1);
		table.Bind(EngineKey::DpadDown, Action::ActionSlot2);
		table.Bind(EngineKey::DpadLeft, Action::ActionSlot3);
		table.Bind(EngineKey::DpadRight, Action::ActionSlot4);
	}

	bool MatchesButtonLayout(const BindingTable& table)
	{
		BindingTable stock;

		for (const auto& layout : layouts)
		{
			ApplyButtonLayout(stock, layout.name);

			if (HaveSameBindings(table, stock))
			{
				return true;
			}

			ApplyButtonLayout(stock, std::string(layout.name) + std::string(altSuffix));

			if (HaveSameBindings(table, stock))
			{
				return true;
			}
		}

		return false;
	}
}
