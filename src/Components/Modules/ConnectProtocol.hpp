#pragma once

namespace Components
{
	class ConnectProtocol : public Component
	{
	public:
		ConnectProtocol();

		static bool IsEvaluated();
		static bool Used();

	private:
		static bool isEvaluated;
		static std::string connectString;

		static void EvaluateProtocol();
		static bool InstallProtocol();
		static void Invocation();
	};
}
