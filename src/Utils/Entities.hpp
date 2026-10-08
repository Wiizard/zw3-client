#pragma once

namespace Utils
{
	class Entities
	{
	public:
		Entities() = default;
		Entities(const std::string& buffer) : Entities() { this->Parse(buffer); }
		Entities(const char* string, std::size_t lenPlusOne) : Entities(std::string(string, lenPlusOne - 1)) {}
		Entities(const Entities& obj) = default;

		[[nodiscard]] std::string Build() const;

		std::vector<std::string> GetModels();
		std::vector<std::string> GetWeapons();

	private:
		enum
		{
			PARSE_AWAIT_KEY,
			PARSE_READ_KEY,
			PARSE_AWAIT_VALUE,
			PARSE_READ_VALUE,
		};

		std::vector<std::unordered_map<std::string, std::string>> entities;
		void Parse(const std::string& buffer);
	};
}
