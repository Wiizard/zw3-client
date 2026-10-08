#pragma once

template <class Type, std::size_t n>
constexpr auto ARRAY_COUNT(Type(&)[n]) { return n; }

namespace Utils::String
{
	template <std::size_t Buffers, std::size_t MinBufferSize>
	class VAProvider
	{
	public:
		static_assert(Buffers != 0 && MinBufferSize != 0, "Buffers and MinBufferSize must not be 0");

		VAProvider() : currentBuffer(0) {}

		[[nodiscard]] const char* Get(const char* format, va_list ap)
		{
			++this->currentBuffer %= ARRAY_COUNT(this->stringPool);
			auto* const entry = &this->stringPool[this->currentBuffer];

			if (!entry->size || !entry->buffer)
			{
				return "";
			}

			while (true)
			{
				const auto written = vsnprintf_s(entry->buffer, entry->size, _TRUNCATE, format, ap);

				if (written > 0)
				{
					break;
				}

				if (written == 0)
				{
					return "";
				}

				entry->DoubleSize();
			}

			return entry->buffer;
		}

	private:
		class Entry
		{
		public:
			Entry(std::size_t size = MinBufferSize) : size(size), buffer(nullptr)
			{
				if (this->size < MinBufferSize)
				{
					this->size = MinBufferSize;
				}

				this->Allocate();
			}

			~Entry()
			{
				if (this->buffer)
				{
					Memory::GetAllocator()->Free(this->buffer);
				}

				this->size = 0;
				this->buffer = nullptr;
			}

			void Allocate()
			{
				if (this->buffer)
				{
					Memory::GetAllocator()->Free(this->buffer);
				}

				this->buffer = Memory::GetAllocator()->AllocateArray<char>(this->size + 1);
			}

			void DoubleSize()
			{
				this->size *= 2;
				this->Allocate();
			}

			std::size_t size;
			char* buffer;
		};

		std::size_t currentBuffer;
		Entry stringPool[Buffers];
	};

	template <typename Arg>
	static void SanitizeFormatArgs(Arg& arg)
	{
		if constexpr (std::is_same_v<Arg, char*> || std::is_same_v<Arg, const char*>)
		{
			if (arg == nullptr)
			{
				arg = const_cast<char*>("nullptr");
			}
		}
	}

	[[nodiscard]] const char* VA(const char* format, ...);

	template <typename... Args>
	[[nodiscard]] const char* Format(std::string_view format, Args&&... args)
	{
		static thread_local std::string buffer;
		(SanitizeFormatArgs(args), ...);
		std::vformat(format, std::make_format_args(args...)).swap(buffer);

		return buffer.data();
	}

	[[nodiscard]] std::string ToLower(const std::string& text);
	[[nodiscard]] std::string ToUpper(const std::string& text);

	[[nodiscard]] bool Compare(const std::string& lhs, const std::string& rhs);

	[[nodiscard]] std::vector<std::string> Split(const std::string& text, char delimiter);
	void Replace(std::string& text, const std::string& from, const std::string& to);

	[[nodiscard]] bool StartsWith(const std::string& haystack, const std::string& needle);
	[[nodiscard]] bool EndsWith(const std::string& haystack, const std::string& needle);
	[[nodiscard]] bool Contains(const std::string& haystack, const std::string& needle);

	[[nodiscard]] bool IsNumber(const std::string& text);

	std::string& LTrim(std::string& text);
	std::string& RTrim(std::string& text);
	void Trim(std::string& text);

	[[nodiscard]] std::string Convert(const std::wstring& text);
	[[nodiscard]] std::wstring Convert(const std::string& text);

	[[nodiscard]] std::string FormatTimeSpan(int milliseconds);
	[[nodiscard]] std::string FormatBandwidth(std::size_t bytes, int milliseconds);

	[[nodiscard]] std::string DumpHex(const std::string& data, const std::string& separator = " ");

	[[nodiscard]] std::string XOR(std::string text, char value);
}
