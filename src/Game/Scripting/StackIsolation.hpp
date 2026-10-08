#pragma once

namespace Game::Scripting
{
	class StackIsolation final
	{
	public:
		StackIsolation();
		~StackIsolation();

		StackIsolation(StackIsolation&&) = delete;
		StackIsolation(const StackIsolation&) = delete;
		StackIsolation& operator=(StackIsolation&&) = delete;
		StackIsolation& operator=(const StackIsolation&) = delete;

	private:
		VariableValue stack[512]{};
		VariableValue* maxStack;
		VariableValue* top;
		unsigned int inParamCount;
		unsigned int outParamCount;
	};
}
