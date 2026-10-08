#include "STDInclude.hpp"

#include "UIScript.hpp"
#include "Dedicated.hpp"
#include "Logger.hpp"

namespace Components
{
	std::unordered_map<std::string, UIScript::Handler> UIScript::handlers;
	std::unordered_map<int, std::function<void()>> UIScript::ownerDraws;

	Utils::Hook UIScript::runMenuScriptHook;
	Utils::Hook UIScript::ownerDrawHandleKeyHook;

	static constexpr std::uintptr_t runMenuScriptCall = 0x14025EF99;
	static constexpr std::uintptr_t ownerDrawHandleKeyCall = 0x14025F67B;

	static constexpr int scriptNameSize = 1024;

	static constexpr int tokenSize = 256;

	static constexpr int mouseButton1 = 200;
	static constexpr int mouseButton2 = 201;

	UIScript::Token::Token(const char** args)
	{
		if (!args)
		{
			return;
		}

		char buffer[tokenSize]{};

		if (Game::String_Parse(args, buffer, tokenSize))
		{
			this->token = buffer;
		}
	}

	template <> int UIScript::Token::Get() const
	{
		if (!this->IsValid())
		{
			return 0;
		}

		return std::strtol(this->token.data(), nullptr, 0);
	}

	template <> const char* UIScript::Token::Get() const
	{
		return this->token.data();
	}

	template <> std::string UIScript::Token::Get() const
	{
		return this->token;
	}

	bool UIScript::Token::IsValid() const
	{
		return !this->token.empty();
	}

	void UIScript::Add(const std::string& name, const Handler& callback)
	{
		handlers[Utils::String::ToLower(name)] = callback;
	}

	void UIScript::AddOwnerDraw(int ownerDraw, const std::function<void()>& callback)
	{
		ownerDraws[ownerDraw] = callback;
	}

	bool UIScript::RunMenuScript(const char** args)
	{
		char name[scriptNameSize]{};

		if (!Game::String_Parse(args, name, scriptNameSize))
		{
			return false;
		}

		const auto handler = handlers.find(Utils::String::ToLower(name));

		if (handler == handlers.end())
		{
			return false;
		}

		handler->second(Token(args));
		return true;
	}

	void UIScript::UI_RunMenuScript_Hook(int localClientNum, const char** args, const char** rest)
	{
		const char* const restore = args ? *args : nullptr;

		if (args && RunMenuScript(args))
		{
			return;
		}

		if (args)
		{
			*args = restore;
		}

		reinterpret_cast<void(*)(int, const char**, const char**)>(
			runMenuScriptHook.GetOriginal())(localClientNum, args, rest);
	}

	void UIScript::UI_OwnerDrawHandleKey_Hook(int ownerDraw, int flags, float* special, int key)
	{
		if (key == mouseButton1 || key == mouseButton2)
		{
			const auto handler = ownerDraws.find(ownerDraw);

			if (handler != ownerDraws.end())
			{
				handler->second();
			}
		}

		reinterpret_cast<void(*)(int, int, float*, int)>(
			ownerDrawHandleKeyHook.GetOriginal())(ownerDraw, flags, special, key);
	}

	UIScript::UIScript()
	{
		if (Dedicated::IsEnabled())
		{
			return;
		}

		runMenuScriptHook.Initialize(runMenuScriptCall, UI_RunMenuScript_Hook, HOOK_CALL)->Install();
		ownerDrawHandleKeyHook.Initialize(ownerDrawHandleKeyCall, UI_OwnerDrawHandleKey_Hook, HOOK_CALL)->Install();

		const bool isSeated = runMenuScriptHook.IsInstalled() && ownerDrawHandleKeyHook.IsInstalled();

		if (!isSeated)
		{
			Logger::Error("uiscript: a menu script call site could not be redirected, menu buttons will not reach us\n");
			return;
		}

		runMenuScriptHook.Quick();
		ownerDrawHandleKeyHook.Quick();
	}
}
