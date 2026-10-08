#pragma once

namespace Utils
{
	class CSV
	{
	public:
		CSV(const std::string& file, bool isFile = true, bool allowComments = true);

		[[nodiscard]] std::size_t GetRows() const;
		[[nodiscard]] std::size_t GetColumns() const;
		[[nodiscard]] std::size_t GetColumns(std::size_t row) const;

		[[nodiscard]] std::string GetElementAt(std::size_t row, std::size_t column) const;

		[[nodiscard]] bool IsValid() const;

	private:
		bool isValid = false;
		std::vector<std::vector<std::string>> rows;

		void Parse(const std::string& file, bool isFile, bool allowComments);
		void ParseRow(const std::string& row, bool allowComments);
	};
}
