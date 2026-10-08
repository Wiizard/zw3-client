#pragma once

#include "Controller/Types.hpp"

#include "Controller/Context.hpp"
#include "Controller/Calibration/Profile.hpp"

namespace Controller::Calibration
{
	class Store
	{
	public:
		Store(const Context& context, std::filesystem::path directory);

		std::optional<Profile> TryLoad(Controller::Family family, std::optional<std::uint64_t> deviceKey) const;

	private:
		std::filesystem::path FileFor(Controller::Family family, std::optional<std::uint64_t> deviceKey) const;

		std::optional<Profile> Reject(const char* why) const;

		const Context& context;
		std::filesystem::path directory;
	};
}
