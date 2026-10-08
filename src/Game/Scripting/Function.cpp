#include "STDInclude.hpp"

namespace Game::Scripting
{
	Function::Function(const char* pos)
		: pos(pos)
	{
	}

	const char* Function::GetPos() const
	{
		return this->pos;
	}
}
