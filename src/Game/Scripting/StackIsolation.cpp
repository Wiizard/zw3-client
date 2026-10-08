#include "STDInclude.hpp"

namespace Game::Scripting
{
	StackIsolation::StackIsolation()
	{
		this->inParamCount = *scrVmPub_inparamcount;
		this->outParamCount = *scrVmPub_outparamcount;
		this->top = *scrVmPub_top;
		this->maxStack = *scrVmPub_maxstack;

		*scrVmPub_top = this->stack;
		*scrVmPub_maxstack = &this->stack[std::size(this->stack) - 1];
		*scrVmPub_inparamcount = 0;
		*scrVmPub_outparamcount = 0;
	}

	StackIsolation::~StackIsolation()
	{
		Scr_ClearOutParams();

		*scrVmPub_inparamcount = this->inParamCount;
		*scrVmPub_outparamcount = this->outParamCount;
		*scrVmPub_top = this->top;
		*scrVmPub_maxstack = this->maxStack;
	}
}
