#pragma once

#include "Controller/Types.hpp"

#include "Controller/Device/Id.hpp"
#include "Controller/Device/Identity.hpp"
#include "Controller/Driver/Output.hpp"
#include "Controller/Sample/Sample.hpp"

namespace Controller::Driver
{
	inline constexpr std::size_t maxReportsPerPoll = 32;

	class Driver
	{
	public:
		virtual ~Driver() = default;

		virtual Controller::Family Family() const noexcept = 0;
		virtual DeviceId Device() const noexcept = 0;

		virtual bool TryPoll(RawSample& raw, CanonicalSample& canonical) = 0;
		virtual void Submit(const OutputRequest& request) = 0;

		virtual void Configure([[maybe_unused]] const OutputPolicy& policy)
		{
		}

		virtual void StopHaptic([[maybe_unused]] std::uint32_t tag)
		{
		}

		virtual std::string Diagnostics() const
		{
			return {};
		}
	};
}
