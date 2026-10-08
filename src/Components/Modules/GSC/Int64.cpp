#include "STDInclude.hpp"

#include "Int64.hpp"
#include "Script.hpp"

namespace Components::GSC
{
	std::unordered_map<std::string, Int64::int64_OP> Int64::operations =
	{
		{ "+", [](const std::int64_t a, const std::int64_t b) { return a + b; } },
		{ "-", [](const std::int64_t a, const std::int64_t b) { return a - b; } },
		{ "*", [](const std::int64_t a, const std::int64_t b) { return a * b; } },
		{ "/", [](const std::int64_t a, const std::int64_t b) { return a / b; } },
		{ "&", [](const std::int64_t a, const std::int64_t b) { return a & b; } },
		{ "^", [](const std::int64_t a, const std::int64_t b) { return a ^ b; } },
		{ "|", [](const std::int64_t a, const std::int64_t b) { return a | b; } },
		{ "~", [](const std::int64_t a, [[maybe_unused]] const std::int64_t b) { return ~a; } },
		{ "%", [](const std::int64_t a, const std::int64_t b) { return a % b; } },
		{ ">>", [](const std::int64_t a, const std::int64_t b) { return a >> b; } },
		{ "<<", [](const std::int64_t a, const std::int64_t b) { return a << b; } },
		{ "++", [](const std::int64_t a, [[maybe_unused]] const std::int64_t b) { return a + 1; } },
		{ "--", [](const std::int64_t a, [[maybe_unused]] const std::int64_t b) { return a - 1; } },
	};

	std::unordered_map<std::string, Int64::int64_Comp> Int64::comparisons =
	{
		{ ">", [](const std::int64_t a, const std::int64_t b) { return a > b; } },
		{ ">=", [](const std::int64_t a, const std::int64_t b) { return a >= b; } },
		{ "==", [](const std::int64_t a, const std::int64_t b) { return a == b; } },
		{ "<=", [](const std::int64_t a, const std::int64_t b) { return a <= b; } },
		{ "<", [](const std::int64_t a, const std::int64_t b) { return a < b; } },
	};

	std::int64_t Int64::GetInt64Arg(unsigned int index, bool optional)
	{
		if (optional && index >= Game::Scr_GetNumParam())
		{
			return 0;
		}

		if (Game::Scr_GetType(index) == Game::VAR_INTEGER)
		{
			return Game::Scr_GetInt(index);
		}

		if (Game::Scr_GetType(index) == Game::VAR_STRING)
		{
			return std::strtoll(Game::Scr_GetString(index), nullptr, 0);
		}

		Script::Scr_ParamError(index, Utils::String::VA("cannot cast %s to int64", Game::Scr_GetTypeName(index)));
		return 0;
	}

	void Int64::AddFunctions()
	{
		Script::AddFunction("Int64IsInt", []
		{
			const auto value = GetInt64Arg(0, false);
			Game::Scr_AddBool(value <= std::numeric_limits<std::int32_t>::max() && value >= std::numeric_limits<std::int32_t>::min());
		});

		Script::AddFunction("Int64ToInt", []
		{
			Game::Scr_AddInt(static_cast<std::int32_t>(GetInt64Arg(0, false)));
		});

		Script::AddFunction("Int64OP", []
		{
			const auto a = GetInt64Arg(0, false);
			const auto* op = Game::Scr_GetString(1);
			const auto b = GetInt64Arg(2, true);

			const auto operation = operations.find(op);

			if (operation != operations.end())
			{
				Game::Scr_AddString(Utils::String::VA("%lld", operation->second(a, b)));
				return;
			}

			const auto comparison = comparisons.find(op);

			if (comparison != comparisons.end())
			{
				Game::Scr_AddBool(comparison->second(a, b));
				return;
			}

			Script::Scr_ParamError(1, "Invalid int64 operation");
		});
	}

	Int64::Int64()
	{
		AddFunctions();
	}
}
