#pragma once

namespace Utils
{
	class MenuPreprocessor
	{
	public:
		using FileReader = std::function<bool(const std::string& path, std::string* contents)>;

		explicit MenuPreprocessor(FileReader reader);

		void Define(const std::string& name, const std::string& value);

		bool Process(const std::string& path, std::string* out);
		bool ProcessText(const std::string& path, const std::string& text, std::string* out);

		[[nodiscard]] const std::vector<std::string>& GetErrors() const;

		static bool TryEvaluate(const std::string& expression, double* out);

	private:
		struct Macro
		{
			std::vector<std::string> parameters;
			std::string body;
			bool isFunctionLike = false;
		};

		struct Conditional
		{
			bool isLive = false;
			bool hasTakenBranch = false;
			bool wasParentLive = false;
		};

		bool IsLive() const;
		void Fail(const std::string& path, std::size_t line, const std::string& reason);

		void HandleDirective(const std::string& path, const std::string& line, std::size_t number, std::string* out);
		void HandleDefine(const std::string& arguments);
		void HandleInclude(const std::string& path, const std::string& arguments, std::size_t number, std::string* out);
		bool EvaluateCondition(const std::string& expression);

		std::string Expand(const std::string& text, std::vector<std::string>* active, bool* isIncomplete);
		std::string ExpandFunctionMacro(const Macro& macro, const std::string& name,
			const std::vector<std::string>& arguments, std::vector<std::string>* active);

		FileReader reader;
		std::unordered_map<std::string, Macro> macros;
		std::vector<Conditional> conditionals;
		std::vector<std::string> includeStack;
		std::vector<std::string> errors;
	};
}
