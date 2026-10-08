#pragma once

namespace Utils
{
	class InfoString
	{
	public:
		InfoString() = default;
		explicit InfoString(const std::string& buffer);

		void Set(const std::string& key, const std::string& value);
		void Remove(const std::string& key);

		[[nodiscard]] std::string Get(const std::string& key) const;
		[[nodiscard]] bool Has(const std::string& key) const;
		[[nodiscard]] std::string Build() const;

		[[nodiscard]] nlohmann::json ToJson() const;

	private:
		std::unordered_map<std::string, std::string> pairs;

		void Parse(std::string buffer);
	};
}
