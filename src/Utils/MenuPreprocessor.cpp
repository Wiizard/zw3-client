#include "STDInclude.hpp"

#include "MenuPreprocessor.hpp"

namespace Utils
{
	static constexpr std::size_t maxIncludeDepth = 32;
	static constexpr std::size_t maxContinuationPulls = 64;

	static bool IsIdentifierStart(char character)
	{
		return (character >= 'a' && character <= 'z')
			|| (character >= 'A' && character <= 'Z')
			|| character == '_';
	}

	static bool IsIdentifierChar(char character)
	{
		return IsIdentifierStart(character) || (character >= '0' && character <= '9');
	}

	static bool IsSpace(char character)
	{
		return character == ' ' || character == '\t' || character == '\r';
	}

	static std::string TrimCopy(const std::string& text)
	{
		std::size_t begin = 0;
		while (begin < text.size() && IsSpace(text[begin]))
		{
			++begin;
		}

		std::size_t end = text.size();
		while (end > begin && IsSpace(text[end - 1]))
		{
			--end;
		}

		return text.substr(begin, end - begin);
	}

	static std::string SpliceAndStripComments(const std::string& text)
	{
		std::string out;
		out.reserve(text.size());

		std::size_t at = 0;
		while (at < text.size())
		{
			const char current = text[at];

			if (current == '\\')
			{
				std::size_t probe = at + 1;
				while (probe < text.size() && (text[probe] == ' ' || text[probe] == '\t' || text[probe] == '\r'))
				{
					++probe;
				}

				if (probe < text.size() && text[probe] == '\n')
				{
					at = probe + 1;
					continue;
				}
			}

			if (current == '"')
			{
				out.push_back(current);
				++at;

				while (at < text.size() && text[at] != '"' && text[at] != '\n')
				{
					if (text[at] == '\\' && at + 1 < text.size())
					{
						out.push_back(text[at]);
						++at;
					}

					out.push_back(text[at]);
					++at;
				}

				if (at < text.size() && text[at] == '"')
				{
					out.push_back('"');
					++at;
				}

				continue;
			}

			if (current == '/' && at + 1 < text.size() && text[at + 1] == '/')
			{
				while (at < text.size() && text[at] != '\n')
				{
					++at;
				}

				continue;
			}

			if (current == '/' && at + 1 < text.size() && text[at + 1] == '*')
			{
				at += 2;
				out.push_back(' ');

				while (at + 1 < text.size() && !(text[at] == '*' && text[at + 1] == '/'))
				{
					if (text[at] == '\n')
					{
						out.push_back('\n');
					}

					++at;
				}

				if (at + 1 < text.size())
				{
					at += 2;
				}
				else
				{
					at = text.size();
				}

				continue;
			}

			out.push_back(current);
			++at;
		}

		return out;
	}

	static std::vector<std::string> SplitLines(const std::string& text)
	{
		std::vector<std::string> lines;
		std::string current;

		for (const char character : text)
		{
			if (character == '\n')
			{
				lines.push_back(current);
				current.clear();
				continue;
			}

			if (character != '\r')
			{
				current.push_back(character);
			}
		}

		lines.push_back(current);
		return lines;
	}

	class ExpressionReader
	{
	public:
		explicit ExpressionReader(const std::string& text) : text(text), at(0), isValid(true) {}

		double Read()
		{
			const double result = ReadTernary();
			SkipSpace();

			if (this->at < this->text.size())
			{
				this->isValid = false;
			}

			return result;
		}

		bool IsValid() const
		{
			return this->isValid;
		}

	private:
		void SkipSpace()
		{
			while (this->at < this->text.size() && IsSpace(this->text[this->at]))
			{
				++this->at;
			}
		}

		bool Take(const char* token)
		{
			SkipSpace();

			const std::size_t length = std::strlen(token);
			if (this->text.compare(this->at, length, token) != 0)
			{
				return false;
			}

			if (length == 1 && this->at + 1 < this->text.size())
			{
				const char following = this->text[this->at + 1];
				const char leading = token[0];
				const bool wouldSplit = (leading == '<' && (following == '<' || following == '='))
					|| (leading == '>' && (following == '>' || following == '='))
					|| (leading == '&' && following == '&')
					|| (leading == '|' && following == '|')
					|| (leading == '=' && following == '=')
					|| (leading == '!' && following == '=');

				if (wouldSplit)
				{
					return false;
				}
			}

			this->at += length;
			return true;
		}

		static bool IsIntegral(double value)
		{
			return value == std::floor(value) && std::fabs(value) < 9.0e15;
		}

		double ReadTernary()
		{
			const double condition = ReadLogicalOr();

			if (!Take("?"))
			{
				return condition;
			}

			const double whenTrue = ReadTernary();

			if (!Take(":"))
			{
				this->isValid = false;
				return condition;
			}

			const double whenFalse = ReadTernary();
			return condition != 0.0 ? whenTrue : whenFalse;
		}

		double ReadLogicalOr()
		{
			double left = ReadLogicalAnd();

			while (Take("||"))
			{
				const double right = ReadLogicalAnd();
				left = (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
			}

			return left;
		}

		double ReadLogicalAnd()
		{
			double left = ReadBitOr();

			while (Take("&&"))
			{
				const double right = ReadBitOr();
				left = (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
			}

			return left;
		}

		double ReadBitOr()
		{
			double left = ReadBitXor();

			while (Take("|"))
			{
				const double right = ReadBitXor();
				left = static_cast<double>(static_cast<long long>(left) | static_cast<long long>(right));
			}

			return left;
		}

		double ReadBitXor()
		{
			double left = ReadBitAnd();

			while (Take("^"))
			{
				const double right = ReadBitAnd();
				left = static_cast<double>(static_cast<long long>(left) ^ static_cast<long long>(right));
			}

			return left;
		}

		double ReadBitAnd()
		{
			double left = ReadEquality();

			while (Take("&"))
			{
				const double right = ReadEquality();
				left = static_cast<double>(static_cast<long long>(left) & static_cast<long long>(right));
			}

			return left;
		}

		double ReadEquality()
		{
			double left = ReadRelational();

			for (;;)
			{
				if (Take("=="))
				{
					left = left == ReadRelational() ? 1.0 : 0.0;
					continue;
				}

				if (Take("!="))
				{
					left = left != ReadRelational() ? 1.0 : 0.0;
					continue;
				}

				return left;
			}
		}

		double ReadRelational()
		{
			double left = ReadShift();

			for (;;)
			{
				if (Take("<="))
				{
					left = left <= ReadShift() ? 1.0 : 0.0;
					continue;
				}

				if (Take(">="))
				{
					left = left >= ReadShift() ? 1.0 : 0.0;
					continue;
				}

				if (Take("<"))
				{
					left = left < ReadShift() ? 1.0 : 0.0;
					continue;
				}

				if (Take(">"))
				{
					left = left > ReadShift() ? 1.0 : 0.0;
					continue;
				}

				return left;
			}
		}

		double ReadShift()
		{
			double left = ReadAdditive();

			for (;;)
			{
				if (Take("<<"))
				{
					const double right = ReadAdditive();
					left = static_cast<double>(static_cast<long long>(left) << static_cast<long long>(right));
					continue;
				}

				if (Take(">>"))
				{
					const double right = ReadAdditive();
					left = static_cast<double>(static_cast<long long>(left) >> static_cast<long long>(right));
					continue;
				}

				return left;
			}
		}

		double ReadAdditive()
		{
			double left = ReadMultiplicative();

			for (;;)
			{
				if (Take("+"))
				{
					left += ReadMultiplicative();
					continue;
				}

				if (Take("-"))
				{
					left -= ReadMultiplicative();
					continue;
				}

				return left;
			}
		}

		double ReadMultiplicative()
		{
			double left = ReadUnary();

			for (;;)
			{
				if (Take("*"))
				{
					left *= ReadUnary();
					continue;
				}

				if (Take("/"))
				{
					const double right = ReadUnary();

					if (right == 0.0)
					{
						this->isValid = false;
						return 0.0;
					}

					if (IsIntegral(left) && IsIntegral(right))
					{
						left = static_cast<double>(static_cast<long long>(left) / static_cast<long long>(right));
					}
					else
					{
						left /= right;
					}

					continue;
				}

				if (Take("%"))
				{
					const double right = ReadUnary();

					if (static_cast<long long>(right) == 0)
					{
						this->isValid = false;
						return 0.0;
					}

					left = static_cast<double>(static_cast<long long>(left) % static_cast<long long>(right));
					continue;
				}

				return left;
			}
		}

		double ReadUnary()
		{
			if (Take("!"))
			{
				return ReadUnary() == 0.0 ? 1.0 : 0.0;
			}

			if (Take("~"))
			{
				return static_cast<double>(~static_cast<long long>(ReadUnary()));
			}

			if (Take("-"))
			{
				return -ReadUnary();
			}

			if (Take("+"))
			{
				return ReadUnary();
			}

			return ReadPrimary();
		}

		double ReadPrimary()
		{
			SkipSpace();

			if (this->at >= this->text.size())
			{
				this->isValid = false;
				return 0.0;
			}

			if (this->text[this->at] == '(')
			{
				++this->at;
				const double inner = ReadTernary();

				if (!Take(")"))
				{
					this->isValid = false;
				}

				return inner;
			}

			const char current = this->text[this->at];
			const bool isNumber = (current >= '0' && current <= '9')
				|| (current == '.' && this->at + 1 < this->text.size()
					&& this->text[this->at + 1] >= '0' && this->text[this->at + 1] <= '9');

			if (!isNumber)
			{
				this->isValid = false;
				return 0.0;
			}

			const char* const begin = this->text.c_str() + this->at;
			char* end = nullptr;
			const double value = std::strtod(begin, &end);
			this->at += static_cast<std::size_t>(end - begin);

			return value;
		}

		const std::string& text;
		std::size_t at;
		bool isValid;
	};

	bool MenuPreprocessor::TryEvaluate(const std::string& expression, double* out)
	{
		ExpressionReader reader(expression);
		const double value = reader.Read();

		if (!reader.IsValid())
		{
			return false;
		}

		*out = value;
		return true;
	}

	MenuPreprocessor::MenuPreprocessor(FileReader reader) : reader(std::move(reader))
	{
	}

	void MenuPreprocessor::Define(const std::string& name, const std::string& value)
	{
		Macro macro;
		macro.body = value;
		macro.isFunctionLike = false;

		this->macros[name] = macro;
	}

	const std::vector<std::string>& MenuPreprocessor::GetErrors() const
	{
		return this->errors;
	}

	bool MenuPreprocessor::IsLive() const
	{
		for (const auto& conditional : this->conditionals)
		{
			if (!conditional.isLive)
			{
				return false;
			}
		}

		return true;
	}

	void MenuPreprocessor::Fail(const std::string& path, std::size_t line, const std::string& reason)
	{
		this->errors.push_back(std::format("{}({}): {}", path, line, reason));
	}

	bool MenuPreprocessor::Process(const std::string& path, std::string* out)
	{
		std::string contents;

		if (!this->reader(path, &contents))
		{
			this->errors.push_back(std::format("{}: not found", path));
			return false;
		}

		return this->ProcessText(path, contents, out);
	}

	bool MenuPreprocessor::ProcessText(const std::string& path, const std::string& text, std::string* out)
	{
		if (this->includeStack.size() >= maxIncludeDepth)
		{
			this->errors.push_back(std::format("{}: include nesting is deeper than {}", path, maxIncludeDepth));
			return false;
		}

		for (const auto& open : this->includeStack)
		{
			if (_stricmp(open.c_str(), path.c_str()) == 0)
			{
				this->errors.push_back(std::format("{}: includes itself", path));
				return false;
			}
		}

		this->includeStack.push_back(path);

		const auto lines = SplitLines(SpliceAndStripComments(text));
		const std::size_t openConditionals = this->conditionals.size();
		const std::size_t errorsBefore = this->errors.size();

		for (std::size_t index = 0; index < lines.size(); ++index)
		{
			const std::string trimmed = TrimCopy(lines[index]);

			if (!trimmed.empty() && trimmed[0] == '#')
			{
				this->HandleDirective(path, trimmed, index + 1, out);
				continue;
			}

			if (!this->IsLive())
			{
				out->push_back('\n');
				continue;
			}

			std::string logical = lines[index];
			std::vector<std::string> active;
			bool isIncomplete = false;
			std::string expanded = this->Expand(logical, &active, &isIncomplete);

			for (std::size_t pull = 0; isIncomplete && pull < maxContinuationPulls; ++pull)
			{
				if (index + 1 >= lines.size())
				{
					break;
				}

				const std::string following = TrimCopy(lines[index + 1]);
				if (!following.empty() && following[0] == '#')
				{
					break;
				}

				++index;
				logical += "\n";
				logical += lines[index];

				active.clear();
				isIncomplete = false;
				expanded = this->Expand(logical, &active, &isIncomplete);
			}

			if (isIncomplete)
			{
				this->Fail(path, index + 1, "a macro argument list is never closed");
			}

			out->append(expanded);
			out->push_back('\n');
		}

		if (this->conditionals.size() != openConditionals)
		{
			this->Fail(path, lines.size(), "an #if is never closed");
			this->conditionals.resize(openConditionals);
		}

		this->includeStack.pop_back();
		return this->errors.size() == errorsBefore;
	}

	void MenuPreprocessor::HandleDirective(const std::string& path, const std::string& line,
		std::size_t number, std::string* out)
	{
		std::size_t at = 1;
		while (at < line.size() && IsSpace(line[at]))
		{
			++at;
		}

		std::string directive;
		while (at < line.size() && IsIdentifierChar(line[at]))
		{
			directive.push_back(line[at]);
			++at;
		}

		const std::string arguments = TrimCopy(line.substr(at));

		if (directive == "endif")
		{
			if (this->conditionals.empty())
			{
				this->Fail(path, number, "#endif without #if");
				return;
			}

			this->conditionals.pop_back();
			return;
		}

		if (directive == "else" || directive == "elif" || directive == "elifdef" || directive == "elifndef")
		{
			if (this->conditionals.empty())
			{
				this->Fail(path, number, std::format("#{} without #if", directive));
				return;
			}

			Conditional& frame = this->conditionals.back();

			if (frame.isLive)
			{
				frame.hasTakenBranch = true;
			}

			if (!frame.wasParentLive || frame.hasTakenBranch)
			{
				frame.isLive = false;
				return;
			}

			if (directive == "else")
			{
				frame.isLive = true;
			}
			else if (directive == "elifdef")
			{
				frame.isLive = this->macros.contains(arguments);
			}
			else if (directive == "elifndef")
			{
				frame.isLive = !this->macros.contains(arguments);
			}
			else
			{
				frame.isLive = this->EvaluateCondition(arguments);
			}

			return;
		}

		if (directive == "if" || directive == "ifdef" || directive == "ifndef")
		{
			Conditional frame;
			frame.wasParentLive = this->IsLive();
			frame.hasTakenBranch = false;

			if (!frame.wasParentLive)
			{
				frame.isLive = false;
			}
			else if (directive == "ifdef")
			{
				frame.isLive = this->macros.contains(arguments);
			}
			else if (directive == "ifndef")
			{
				frame.isLive = !this->macros.contains(arguments);
			}
			else
			{
				frame.isLive = this->EvaluateCondition(arguments);
			}

			this->conditionals.push_back(frame);
			return;
		}

		if (!this->IsLive())
		{
			return;
		}

		if (directive == "define")
		{
			this->HandleDefine(arguments);
			return;
		}

		if (directive == "undef")
		{
			this->macros.erase(arguments);
			return;
		}

		if (directive == "include")
		{
			this->HandleInclude(path, arguments, number, out);
			return;
		}

		if (directive == "error")
		{
			this->Fail(path, number, std::format("#error {}", arguments));
			return;
		}

		if (directive.empty())
		{
			return;
		}

		this->Fail(path, number, std::format("unknown directive #{}", directive));
	}

	void MenuPreprocessor::HandleDefine(const std::string& arguments)
	{
		std::size_t at = 0;
		std::string name;

		while (at < arguments.size() && IsIdentifierChar(arguments[at]))
		{
			name.push_back(arguments[at]);
			++at;
		}

		if (name.empty())
		{
			return;
		}

		Macro macro;

		if (at < arguments.size() && arguments[at] == '(')
		{
			macro.isFunctionLike = true;
			++at;

			std::string parameter;
			while (at < arguments.size() && arguments[at] != ')')
			{
				if (arguments[at] == ',')
				{
					macro.parameters.push_back(TrimCopy(parameter));
					parameter.clear();
				}
				else
				{
					parameter.push_back(arguments[at]);
				}

				++at;
			}

			if (at < arguments.size())
			{
				++at;
			}

			const std::string last = TrimCopy(parameter);
			if (!last.empty() || !macro.parameters.empty())
			{
				macro.parameters.push_back(last);
			}
		}

		macro.body = TrimCopy(arguments.substr(at));
		this->macros[name] = macro;
	}

	void MenuPreprocessor::HandleInclude(const std::string& path, const std::string& arguments,
		std::size_t number, std::string* out)
	{
		const std::size_t begin = arguments.find_first_of("\"<");

		if (begin == std::string::npos)
		{
			this->Fail(path, number, "#include without a path");
			return;
		}

		const char closing = arguments[begin] == '<' ? '>' : '"';
		const std::size_t end = arguments.find(closing, begin + 1);

		if (end == std::string::npos)
		{
			this->Fail(path, number, "#include without a closing quote");
			return;
		}

		const std::string target = arguments.substr(begin + 1, end - begin - 1);
		std::string contents;

		if (!this->reader(target, &contents))
		{
			this->Fail(path, number, std::format("cannot open {}", target));
			return;
		}

		this->ProcessText(target, contents, out);
	}

	bool MenuPreprocessor::EvaluateCondition(const std::string& expression)
	{
		std::string resolved;
		std::size_t at = 0;

		while (at < expression.size())
		{
			if (!IsIdentifierStart(expression[at]))
			{
				resolved.push_back(expression[at]);
				++at;
				continue;
			}

			std::string identifier;
			while (at < expression.size() && IsIdentifierChar(expression[at]))
			{
				identifier.push_back(expression[at]);
				++at;
			}

			if (identifier != "defined")
			{
				resolved.append(identifier);
				continue;
			}

			while (at < expression.size() && IsSpace(expression[at]))
			{
				++at;
			}

			const bool isParenthesised = at < expression.size() && expression[at] == '(';
			if (isParenthesised)
			{
				++at;
				while (at < expression.size() && IsSpace(expression[at]))
				{
					++at;
				}
			}

			std::string name;
			while (at < expression.size() && IsIdentifierChar(expression[at]))
			{
				name.push_back(expression[at]);
				++at;
			}

			if (isParenthesised)
			{
				while (at < expression.size() && expression[at] != ')')
				{
					++at;
				}

				if (at < expression.size())
				{
					++at;
				}
			}

			resolved.append(this->macros.contains(name) ? "1" : "0");
		}

		std::vector<std::string> active;
		bool isIncomplete = false;
		const std::string expanded = this->Expand(resolved, &active, &isIncomplete);

		std::string numeric;
		at = 0;

		while (at < expanded.size())
		{
			if (!IsIdentifierStart(expanded[at]))
			{
				numeric.push_back(expanded[at]);
				++at;
				continue;
			}

			while (at < expanded.size() && IsIdentifierChar(expanded[at]))
			{
				++at;
			}

			numeric.append("0");
		}

		double value = 0.0;
		if (!TryEvaluate(numeric, &value))
		{
			return false;
		}

		return value != 0.0;
	}

	static std::string MergePastedTokens(const std::string& text)
	{
		std::string merged = text;

		for (;;)
		{
			const std::size_t at = merged.find("##");

			if (at == std::string::npos)
			{
				return merged;
			}

			std::string left = merged.substr(0, at);
			std::string right = merged.substr(at + 2);

			while (!left.empty() && IsSpace(left.back()))
			{
				left.pop_back();
			}

			std::size_t skip = 0;
			while (skip < right.size() && IsSpace(right[skip]))
			{
				++skip;
			}
			right = right.substr(skip);

			if (!left.empty() && left.back() == '"' && !right.empty() && right[0] == '"')
			{
				left.pop_back();
				right = right.substr(1);
			}

			merged = left + right;
		}
	}

	static std::string StringizeArgument(const std::string& argument)
	{
		std::string literal = "\"";

		for (const char character : argument)
		{
			if (character == '"' || character == '\\')
			{
				literal.push_back('\\');
			}

			literal.push_back(character);
		}

		literal.push_back('"');
		return literal;
	}

	static void JoinStringLiteral(std::string* out, const std::string& literal)
	{
		std::size_t end = out->size();
		while (end > 0 && IsSpace((*out)[end - 1]))
		{
			--end;
		}

		if (end == 0 || (*out)[end - 1] != '"')
		{
			out->append(literal);
			return;
		}

		out->resize(end - 1);
		out->append(literal.substr(1));
	}

	std::string MenuPreprocessor::ExpandFunctionMacro(const Macro& macro, const std::string& name,
		const std::vector<std::string>& arguments, std::vector<std::string>* active)
	{
		std::string substituted;
		std::size_t at = 0;
		bool isAfterStringize = false;

		while (at < macro.body.size())
		{
			if (macro.body[at] == '"')
			{
				std::string literal = "\"";
				++at;

				while (at < macro.body.size() && macro.body[at] != '"')
				{
					literal.push_back(macro.body[at]);
					++at;
				}

				if (at < macro.body.size())
				{
					literal.push_back('"');
					++at;
				}

				if (isAfterStringize)
				{
					JoinStringLiteral(&substituted, literal);
				}
				else
				{
					substituted.append(literal);
				}

				continue;
			}

			const bool isPaste = macro.body[at] == '#' && at + 1 < macro.body.size() && macro.body[at + 1] == '#';

			if (isPaste)
			{
				substituted.append("##");
				at += 2;
				isAfterStringize = false;
				continue;
			}

			if (macro.body[at] == '#')
			{
				std::size_t probe = at + 1;
				while (probe < macro.body.size() && IsSpace(macro.body[probe]))
				{
					++probe;
				}

				std::string parameter;
				while (probe < macro.body.size() && IsIdentifierChar(macro.body[probe]))
				{
					parameter.push_back(macro.body[probe]);
					++probe;
				}

				const auto found = std::find(macro.parameters.begin(), macro.parameters.end(), parameter);
				const std::size_t index = static_cast<std::size_t>(found - macro.parameters.begin());

				if (parameter.empty() || found == macro.parameters.end() || index >= arguments.size())
				{
					substituted.push_back('#');
					++at;
					isAfterStringize = false;
					continue;
				}

				JoinStringLiteral(&substituted, StringizeArgument(arguments[index]));
				at = probe;
				isAfterStringize = true;
				continue;
			}

			if (IsSpace(macro.body[at]))
			{
				substituted.push_back(macro.body[at]);
				++at;
				continue;
			}

			isAfterStringize = false;

			if (!IsIdentifierStart(macro.body[at]))
			{
				substituted.push_back(macro.body[at]);
				++at;
				continue;
			}

			std::string identifier;
			while (at < macro.body.size() && IsIdentifierChar(macro.body[at]))
			{
				identifier.push_back(macro.body[at]);
				++at;
			}

			bool wasParameter = false;
			for (std::size_t index = 0; index < macro.parameters.size(); ++index)
			{
				if (macro.parameters[index] != identifier || index >= arguments.size())
				{
					continue;
				}

				substituted.append(arguments[index]);
				wasParameter = true;
				break;
			}

			if (!wasParameter)
			{
				substituted.append(identifier);
			}
		}

		active->push_back(name);
		bool isIncomplete = false;
		const std::string result = this->Expand(MergePastedTokens(substituted), active, &isIncomplete);
		active->pop_back();

		return result;
	}

	std::string MenuPreprocessor::Expand(const std::string& text, std::vector<std::string>* active,
		bool* isIncomplete)
	{
		std::string out;
		std::size_t at = 0;

		while (at < text.size())
		{
			if (text[at] == '"')
			{
				out.push_back('"');
				++at;

				while (at < text.size() && text[at] != '"')
				{
					out.push_back(text[at]);
					++at;
				}

				if (at < text.size())
				{
					out.push_back('"');
					++at;
				}

				continue;
			}

			if (!IsIdentifierStart(text[at]))
			{
				out.push_back(text[at]);
				++at;
				continue;
			}

			const std::size_t identifierAt = at;
			std::string identifier;

			while (at < text.size() && IsIdentifierChar(text[at]))
			{
				identifier.push_back(text[at]);
				++at;
			}

			const auto found = this->macros.find(identifier);
			const bool isExpanding = std::find(active->begin(), active->end(), identifier) != active->end();

			if (found == this->macros.end() || isExpanding)
			{
				out.append(identifier);
				continue;
			}

			if (!found->second.isFunctionLike)
			{
				active->push_back(identifier);
				bool nested = false;
				out.append(this->Expand(MergePastedTokens(found->second.body), active, &nested));
				active->pop_back();

				*isIncomplete = *isIncomplete || nested;
				continue;
			}

			std::size_t probe = at;
			while (probe < text.size() && (IsSpace(text[probe]) || text[probe] == '\n'))
			{
				++probe;
			}

			if (probe >= text.size() || text[probe] != '(')
			{
				out.append(identifier);
				continue;
			}

			++probe;

			std::vector<std::string> arguments;
			std::string argument;
			int depth = 1;
			bool isClosed = false;

			while (probe < text.size())
			{
				const char current = text[probe];

				if (current == '"')
				{
					argument.push_back(current);
					++probe;

					while (probe < text.size() && text[probe] != '"')
					{
						argument.push_back(text[probe]);
						++probe;
					}

					if (probe < text.size())
					{
						argument.push_back('"');
						++probe;
					}

					continue;
				}

				if (current == '(' || current == '{' || current == '[')
				{
					++depth;
				}
				else if (current == ')' || current == '}' || current == ']')
				{
					--depth;

					if (depth == 0)
					{
						++probe;
						isClosed = true;
						break;
					}
				}
				else if (current == ',' && depth == 1)
				{
					arguments.push_back(TrimCopy(argument));
					argument.clear();
					++probe;
					continue;
				}

				argument.push_back(current);
				++probe;
			}

			if (!isClosed)
			{
				*isIncomplete = true;
				out.append(text.substr(identifierAt));
				return out;
			}

			const std::string last = TrimCopy(argument);
			if (!last.empty() || !arguments.empty())
			{
				arguments.push_back(last);
			}

			std::vector<std::string> expandedArguments;
			expandedArguments.reserve(arguments.size());

			for (const auto& raw : arguments)
			{
				bool nested = false;
				expandedArguments.push_back(this->Expand(raw, active, &nested));
				*isIncomplete = *isIncomplete || nested;
			}

			out.append(this->ExpandFunctionMacro(found->second, identifier, expandedArguments, active));
			at = probe;
		}

		return out;
	}
}
