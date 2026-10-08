#pragma once

#include "Controller/Types.hpp"

namespace Controller
{
	class Runtime;
}

namespace Controller::Engine
{
	bool TryInstall();

	void InstallProtocol();

	void Attach(Runtime* runtime) noexcept;

	void NoteMouseMove(int dx, int dy);
}
