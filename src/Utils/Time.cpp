#include "STDInclude.hpp"

namespace Utils::Time
{
	void Interval::Update()
	{
		this->lastPoint = std::chrono::high_resolution_clock::now();
	}

	bool Interval::Elapsed(std::chrono::nanoseconds duration) const
	{
		return (std::chrono::high_resolution_clock::now() - this->lastPoint) >= duration;
	}

	Point::Point() : lastPoint(Game::Sys_Milliseconds())
	{
	}

	void Point::Update()
	{
		this->lastPoint = Game::Sys_Milliseconds();
	}

	int Point::Diff(Point point) const
	{
		return point.lastPoint - this->lastPoint;
	}

	bool Point::After(Point point) const
	{
		return this->Diff(point) < 0;
	}

	bool Point::Elapsed(int milliseconds) const
	{
		return (Game::Sys_Milliseconds() - this->lastPoint) >= milliseconds;
	}
}
