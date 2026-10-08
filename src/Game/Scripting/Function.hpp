#pragma once

namespace Game::Scripting
{
	class Function
	{
	public:
		Function(const char* pos);

		[[nodiscard]] const char* GetPos() const;

	private:
		const char* pos;
	};
}
