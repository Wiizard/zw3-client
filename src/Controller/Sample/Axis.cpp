#include "STDInclude.hpp"

#include "Controller/Sample/Axis.hpp"

namespace Controller
{
	float StickVector::Magnitude() const noexcept
	{
		return std::sqrt(this->x * this->x + this->y * this->y);
	}
}
