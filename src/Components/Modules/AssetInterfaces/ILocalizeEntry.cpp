#include "STDInclude.hpp"

#include "ILocalizeEntry.hpp"
#include "../Flags.hpp"
#include "../Logger.hpp"

namespace Assets
{
	using LocalizedStrings = std::vector<std::pair<std::string, std::string>>;

	struct StringFileParse
	{
		std::string reference;
		bool hasEndMarker = false;
		LocalizedStrings strings;
	};

	static bool TryTakeKeyword(const char* keyword, const char*& cursor)
	{
		const auto length = std::strlen(keyword);

		if (_strnicmp(keyword, cursor, length) != 0)
		{
			return false;
		}

		cursor += length;

		while (*cursor == ' ' || *cursor == '\t')
		{
			++cursor;
		}

		return true;
	}

	static std::string StripValue(const char* text)
	{
		while (*text == ' ' || *text == '\t')
		{
			++text;
		}

		if (*text == '"')
		{
			++text;
		}

		std::string value(text);

		while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
		{
			value.pop_back();
		}

		if (!value.empty() && value.back() == '"')
		{
			value.pop_back();
		}

		return value;
	}

	static void ReplaceNewlines(std::string& value)
	{
		auto at = value.find("\\n");

		while (at != std::string::npos)
		{
			value[at] = '\n';
			value.erase(at + 1, 1);
			at = value.find("\\n");
		}
	}

	static bool IsValidFormat(const std::string& value)
	{
		bool isDigitUsed[10]{};
		auto at = value.find("&&");

		while (at != std::string::npos)
		{
			const auto digitAt = at + 2;

			if (digitAt >= value.size() || !std::isdigit(static_cast<unsigned char>(value[digitAt])))
			{
				return false;
			}

			auto& isUsed = isDigitUsed[value[digitAt] - '0'];

			if (isUsed)
			{
				return false;
			}

			isUsed = true;
			at = value.find("&&", digitAt + 1);
		}

		return true;
	}

	static void TrimTrailingSpace(std::string& line)
	{
		while (!line.empty() && std::isspace(static_cast<unsigned char>(line.back())))
		{
			line.pop_back();
		}
	}

	static void StripComment(std::string& line)
	{
		for (auto at = line.find("//"); at != std::string::npos; at = line.find("//", at + 1))
		{
			const auto quoteCount = std::count(line.begin(), line.begin() + static_cast<std::ptrdiff_t>(at), '"');

			if (quoteCount % 2 == 0)
			{
				line.resize(at);
				TrimTrailingSpace(line);
				return;
			}
		}
	}

	static bool TryParseLine(const std::string& line, StringFileParse& parse, std::string& error)
	{
		const char* cursor = line.data();

		if (TryTakeKeyword("VERSION", cursor))
		{
			const auto version = std::atol(StripValue(cursor).data());

			if (version != 1)
			{
				error = std::format("Unexpected version number {}, expecting {}!", version, 1);
				return false;
			}

			return true;
		}

		if (TryTakeKeyword("CONFIG", cursor) || TryTakeKeyword("FILENOTES", cursor) || TryTakeKeyword("NOTES", cursor) || TryTakeKeyword("FLAGS", cursor))
		{
			return true;
		}

		if (TryTakeKeyword("REFERENCE", cursor))
		{
			parse.reference = StripValue(cursor);
			return true;
		}

		if (TryTakeKeyword("ENDMARKER", cursor))
		{
			parse.hasEndMarker = true;
			return true;
		}

		if (_strnicmp("LANG_", cursor, 5) != 0)
		{
			error = std::format("Unknown keyword at linestart: \"{}\"", line);
			return false;
		}

		if (parse.reference.empty())
		{
			error = "Error parsing file: Unexpected \"LANG_\"";
			return false;
		}

		const char* const language = cursor + 5;
		const char* languageEnd = language;

		while (*languageEnd && *languageEnd != ' ' && *languageEnd != '\t')
		{
			++languageEnd;
		}

		if (languageEnd - language > 1023)
		{
			languageEnd = language + 1023;
		}

		auto value = StripValue(languageEnd);
		ReplaceNewlines(value);

		if (!IsValidFormat(value))
		{
			error = std::format("Illegal string format \"{}\"", value);
			return false;
		}

		parse.strings.emplace_back(parse.reference, value);
		return true;
	}

	static bool TryParseStringFile(const std::string& contents, const std::string& filename, LocalizedStrings& strings, std::string& error)
	{
		const std::string text(contents.c_str());
		StringFileParse parse;
		std::size_t at = 0;

		while (at < text.size())
		{
			std::string line;
			const auto newline = text.find('\n', at);

			if (newline == std::string::npos)
			{
				line = text.substr(at);
				at = text.size();
			}
			else
			{
				line = text.substr(at, newline - at);
				at = newline;

				while (at < text.size() && (text[at] == '\r' || text[at] == '\n'))
				{
					++at;
				}
			}

			TrimTrailingSpace(line);
			StripComment(line);

			if (line.empty())
			{
				continue;
			}

			std::string lineError;

			if (!TryParseLine(line, parse, lineError))
			{
				error = std::format("{} in {}", lineError, filename);
				return false;
			}
		}

		if (!parse.hasEndMarker)
		{
			error = "Truncated file, failed to find \"ENDMARKER\" at file end!";
			return false;
		}

		strings = std::move(parse.strings);
		return true;
	}

	static void AddLocalizedStrings(Components::ZoneBuilder::Zone* builder, const LocalizedStrings& strings)
	{
		auto* const allocator = builder->GetAllocator();
		std::unordered_map<std::string, Game::LocalizeEntry*> entries;
		std::vector<Game::LocalizeEntry*> order;

		for (const auto& [key, value] : strings)
		{
			const auto existing = entries.find(key);

			if (existing != entries.end())
			{
				existing->second->value = allocator->DuplicateString(value);
				continue;
			}

			auto* const entry = allocator->Allocate<Game::LocalizeEntry>();
			entry->name = allocator->DuplicateString(key);
			entry->value = allocator->DuplicateString(value);

			entries.emplace(key, entry);
			order.push_back(entry);
		}

		for (auto* const entry : order)
		{
			builder->AddRawAsset(Game::ASSET_TYPE_LOCALIZE_ENTRY, entry);
		}
	}

	void ILocalizeEntry::Load(Game::XAssetHeader* header, const std::string& name, Components::ZoneBuilder::Zone* builder)
	{
		const auto path = "localizedstrings/" + name;

		Components::FileSystem::File rawFile(path);

		if (!rawFile.Exists())
		{
			return;
		}

		Components::Logger::Debug("Parsing localized string \"{}\"...", path);

		auto* const allocator = builder->GetAllocator();
		auto* const asset = allocator->Allocate<Game::LocalizeEntry>();

		asset->name = allocator->DuplicateString(name);
		asset->value = allocator->DuplicateString(rawFile.GetBuffer());

		header->localize = asset;
	}

	void ILocalizeEntry::Save(Game::XAssetHeader header, Components::ZoneBuilder::Zone* builder)
	{
		auto* const buffer = builder->GetBuffer();
		const auto* const asset = header.localize;
		auto* const dest = buffer->Dest<Game::X86::LocalizeEntry>();
		const auto record = Game::X86::Convert(*asset);
		buffer->Save(&record);

		buffer->PushBlock(Game::XFILE_BLOCK_VIRTUAL);

		if (asset->value)
		{
			buffer->SaveString(asset->value);
			Utils::Stream::ClearPointer(&dest->value);
		}

		if (asset->name)
		{
			buffer->SaveString(builder->GetAssetName(this->GetType(), asset->name));
			Utils::Stream::ClearPointer(&dest->name);
		}

		buffer->PopBlock();
	}

	void ILocalizeEntry::ParseLocalizedStringsFile(Components::ZoneBuilder::Zone* builder, const std::string& name, const std::string& filename)
	{
		std::string prefix;

		if (Components::Flags::HasFlag("-original-str-parsing"))
		{
			prefix = Utils::String::ToUpper(name) + "_";
		}

		Components::FileSystem::File file(filename);

		if (!file.Exists())
		{
			Components::Logger::Error("Localization ERROR: Unable to load \"{}\"!\n", filename);
			return;
		}

		LocalizedStrings strings;
		std::string error;

		if (!TryParseStringFile(file.GetBuffer(), filename, strings, error))
		{
			Components::Logger::Error("Localization ERROR: {}\n", error);
			return;
		}

		for (auto& [key, value] : strings)
		{
			key.insert(0, prefix);
		}

		AddLocalizedStrings(builder, strings);
	}

	void ILocalizeEntry::ParseLocalizedStringsJSON(Components::ZoneBuilder::Zone* builder, Components::FileSystem::File& file)
	{
		Components::Logger::Debug("Parsing localized string \"{}\"...", file.GetName());

		const auto localize = nlohmann::json::parse(file.GetBuffer(), nullptr, false);

		if (localize.is_discarded())
		{
			Components::Logger::Error("Localized strings json file '{}' is not valid json\n", file.GetName());
			return;
		}

		if (!localize.is_object())
		{
			Components::Logger::Error("Localized strings json file '{}' should be an object!", file.GetName());
			return;
		}

		LocalizedStrings strings;

		for (const auto& [key, value] : localize.items())
		{
			if (!value.is_string())
			{
				Components::Logger::Error("Localized strings json file '{}' contains invalid data!", file.GetName());
				break;
			}

			strings.emplace_back(key, value.get<std::string>());
		}

		AddLocalizedStrings(builder, strings);
	}
}
