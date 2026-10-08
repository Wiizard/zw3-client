#include "STDInclude.hpp"

namespace Utils
{
	CSV::CSV(const std::string& file, const bool isFile, const bool allowComments)
	{
		this->Parse(file, isFile, allowComments);
	}

	std::size_t CSV::GetRows() const
	{
		return this->rows.size();
	}

	std::size_t CSV::GetColumns() const
	{
		std::size_t count = 0;

		for (std::size_t i = 0; i < this->GetRows(); ++i)
		{
			count = std::max(this->GetColumns(i), count);
		}

		return count;
	}

	std::size_t CSV::GetColumns(const std::size_t row) const
	{
		if (this->rows.size() > row)
		{
			return this->rows[row].size();
		}

		return 0;
	}

	std::string CSV::GetElementAt(const std::size_t row, const std::size_t column) const
	{
		if (this->rows.size() > row)
		{
			const auto& cells = this->rows[row];

			if (cells.size() > column)
			{
				return cells[column];
			}
		}

		return {};
	}

	bool CSV::IsValid() const
	{
		return this->isValid;
	}

	void CSV::Parse(const std::string& file, const bool isFile, const bool allowComments)
	{
		std::string buffer;

		if (isFile)
		{
			if (!IO::FileExists(file))
			{
				return;
			}

			buffer = IO::ReadFile(file);
			this->isValid = true;
		}
		else
		{
			buffer = file;
		}

		if (buffer.empty())
		{
			return;
		}

		for (const auto& row : String::Split(buffer, '\n'))
		{
			this->ParseRow(row, allowComments);
		}
	}

	void CSV::ParseRow(const std::string& row, const bool allowComments)
	{
		bool isString = false;
		std::string element;
		std::vector<std::string> cells;
		char character = 0;

		for (std::size_t i = 0; i < row.size(); ++i)
		{
			if (row[i] == ',' && !isString)
			{
				cells.push_back(element);
				element.clear();
				continue;
			}

			if (row[i] == '"')
			{
				isString = !isString;
				continue;
			}

			const bool isEscapedQuote = i < (row.size() - 1) && row[i] == '\\' && row[i + 1] == '"' && isString;
			const bool isDropped = !isString && (row[i] == '\n' || row[i] == '\r' || row[i] == '\t');
			const bool isComment = !isString && (row[i] == '#' || (row[i] == '/' && (i + 1) < row.size() && row[i + 1] == '/'));

			if (isEscapedQuote)
			{
				character = '"';
				++i;
			}
			else if (isDropped)
			{
				continue;
			}
			else if (isComment && allowComments)
			{
				return;
			}
			else
			{
				character = row[i];
			}

			element.append(&character, 1);
		}

		cells.push_back(element);

		if (cells.size() == 1 && cells[0].empty())
		{
			return;
		}

		this->rows.push_back(cells);
	}
}
