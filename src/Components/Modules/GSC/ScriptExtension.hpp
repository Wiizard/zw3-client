#pragma once

namespace Components::GSC
{
	class ScriptExtension : public Component
	{
	public:
		ScriptExtension();

		static const char* GetCodePosForParam(int index);

		static const char* NextCodePos(const char* pos);

	private:
		static std::unordered_map<const char*, const char*> replacedFunctions;
		static const char* replacedPos;

		static void GetReplacedPos(const char* pos);
		static void SetReplacedPos(const char* what, const char* with);

		static void AddFunctions();

		static void AddResizeFunctions();
	};
}
