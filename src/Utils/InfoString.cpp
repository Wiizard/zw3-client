#include "STDInclude.hpp"

namespace Utils
{
	InfoString::InfoString(const std::string& buffer)
	{
		this->Parse(buffer);
	}

	void InfoString::Set(const std::string& key, const std::string& value)
	{
		this->pairs[key] = value;
	}

	void InfoString::Remove(const std::string& key)
	{
		this->pairs.erase(key);
	}

	std::string InfoString::Get(const std::string& key) const
	{
		const auto pair = this->pairs.find(key);

		if (pair == this->pairs.end())
		{
			return {};
		}

		return pair->second;
	}

	bool InfoString::Has(const std::string& key) const
	{
		return this->pairs.contains(key);
	}

	void InfoString::Parse(std::string buffer)
	{
		if (buffer.empty())
		{
			return;
		}

		if (buffer.front() == '\\')
		{
			buffer.erase(buffer.begin());
		}

		const auto tokens = String::Split(buffer, '\\');

		for (std::size_t i = 0; i + 1 < tokens.size(); i += 2)
		{
			if (!this->pairs.contains(tokens[i]))
			{
				this->pairs[tokens[i]] = tokens[i + 1];
			}
		}
	}

	nlohmann::json InfoString::ToJson() const
	{
		return this->pairs;
	}

	std::string InfoString::Build() const
	{
		std::string result;

		for (const auto& [key, value] : this->pairs)
		{
			result.append("\\");
			result.append(key);
			result.append("\\");
			result.append(value);
		}

		return result;
	}
}
