#include "STDInclude.hpp"

#include "Controller/Calibration/Store.hpp"
#include "Controller/Calibration/Validate.hpp"

namespace Controller::Calibration
{
	static constexpr const char* magic = "iw4x-controller-calibration";

	static constexpr int familyMax = static_cast<int>(Family::DualSenseEdge);
	static constexpr int sourceMax = static_cast<int>(ValueSource::User);

	Store::Store(const Context& context, std::filesystem::path directory)
		: context(context),
		directory(std::move(directory))
	{
	}

	std::filesystem::path Store::FileFor(Controller::Family family, std::optional<std::uint64_t> deviceKey) const
	{
		if (deviceKey)
		{
			return this->directory / std::format("controller-{}-{:016x}.cal", ToString(family), *deviceKey);
		}

		return this->directory / std::format("controller-{}.cal", ToString(family));
	}

	std::optional<Profile> Store::Reject(const char* why) const
	{
		this->context.Report(Severity::Warning, Facility::Calibration, ErrorCode::CalibrationInvalid, std::format("calibration profile rejected: {}", why));
		return std::nullopt;
	}

	std::optional<Profile> Store::TryLoad(Controller::Family family, std::optional<std::uint64_t> deviceKey) const
	{
		std::ifstream stream(this->FileFor(family, deviceKey));

		if (!stream)
		{
			return std::nullopt;
		}

		std::string token;
		unsigned int version = 0;

		if (!(stream >> token >> version) || token != magic)
		{
			return this->Reject("bad header");
		}

		if (version == 0 || version > Profile::currentVersion)
		{
			return this->Reject("unsupported version");
		}

		Profile profile;
		profile.version = static_cast<std::uint16_t>(version);

		int familyIndex = 0;
		int sourceIndex = 0;
		int hasKey = 0;
		std::uint64_t keyValue = 0;

		if (!(stream >> token >> familyIndex) || token != "family" || familyIndex < 0 || familyIndex > familyMax)
		{
			return this->Reject("bad family");
		}

		if (!(stream >> token >> sourceIndex) || token != "source" || sourceIndex < 0 || sourceIndex > sourceMax)
		{
			return this->Reject("bad source");
		}

		if (!(stream >> token >> hasKey >> keyValue) || token != "device_key")
		{
			return this->Reject("bad device key");
		}

		profile.family = static_cast<Controller::Family>(familyIndex);
		profile.source = static_cast<ValueSource>(sourceIndex);

		if (hasKey != 0)
		{
			profile.deviceKey = keyValue;
		}

		for (std::size_t i = 0; i < stickCount; ++i)
		{
			std::size_t index = 0;
			StickCalibration stick;

			const bool isRead = static_cast<bool>(stream >> token >> index >> stick.centerX >> stick.centerY >> stick.rangeX >> stick.rangeY >> stick.driftThreshold);

			if (!isRead || token != "stick" || index != i)
			{
				return this->Reject("bad stick record");
			}

			profile.sticks[i] = stick;
		}

		for (std::size_t i = 0; i < triggerCount; ++i)
		{
			std::size_t index = 0;
			TriggerCalibration trigger;

			const bool isRead = static_cast<bool>(stream >> token >> index >> trigger.min >> trigger.max);

			if (!isRead || token != "trigger" || index != i)
			{
				return this->Reject("bad trigger record");
			}

			profile.triggers[i] = trigger;
		}

		auto& motion = profile.motion;

		const bool isMotionRead = static_cast<bool>(stream >> token >> motion.gyroBias.x >> motion.gyroBias.y >> motion.gyroBias.z
			>> motion.accelBias.x >> motion.accelBias.y >> motion.accelBias.z >> motion.gyroScale >> motion.accelScale);

		if (!isMotionRead || token != "motion")
		{
			return this->Reject("bad motion record");
		}

		if (!(stream >> token >> profile.smoothing) || token != "smoothing")
		{
			return this->Reject("bad smoothing record");
		}

		if (profile.family != family || profile.deviceKey != deviceKey)
		{
			return this->Reject("family or device key mismatch");
		}

		std::string why;

		if (!IsValid(profile, why))
		{
			return this->Reject(why.c_str());
		}

		return profile;
	}
}
