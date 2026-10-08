#pragma once

namespace Utils::Time
{
	class Interval
	{
	public:
		Interval() : lastPoint(std::chrono::high_resolution_clock::now()) {}

		void Update();
		[[nodiscard]] bool Elapsed(std::chrono::nanoseconds duration) const;

	protected:
		std::chrono::high_resolution_clock::time_point lastPoint;
	};

	class Point
	{
	public:
		Point();

		void Update();
		[[nodiscard]] int Diff(Point point) const;
		[[nodiscard]] bool After(Point point) const;
		[[nodiscard]] bool Elapsed(int milliseconds) const;

	private:
		int lastPoint;
	};
}
