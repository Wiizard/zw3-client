#pragma once

namespace Components
{
	class UIScript : public Component
	{
	public:
		class Token
		{
		public:
			Token() = default;
			explicit Token(const char** args);

			template <typename T> T Get() const;

			[[nodiscard]] bool IsValid() const;

		private:
			std::string token;
		};

		using Handler = std::function<void(const Token& token)>;

		UIScript();

		static void Add(const std::string& name, const Handler& callback);

		static void AddOwnerDraw(int ownerDraw, const std::function<void()>& callback);

	private:
		static std::unordered_map<std::string, Handler> handlers;
		static std::unordered_map<int, std::function<void()>> ownerDraws;

		static Utils::Hook runMenuScriptHook;
		static Utils::Hook ownerDrawHandleKeyHook;

		static bool RunMenuScript(const char** args);

		static void UI_RunMenuScript_Hook(int localClientNum, const char** args, const char** rest);

		static void UI_OwnerDrawHandleKey_Hook(int ownerDraw, int flags, float* special, int key);
	};

	template <> int UIScript::Token::Get() const;
	template <> const char* UIScript::Token::Get() const;
	template <> std::string UIScript::Token::Get() const;
}
