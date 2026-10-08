#pragma once

#include <rapidjson/document.h>

template <typename ValueType>
struct rapidjson::internal::TypeHelper<ValueType, std::uint16_t>
{
	static std::uint16_t Get(const ValueType& value)
	{
		return static_cast<std::uint16_t>(value.GetInt());
	}
};

template <typename ValueType>
struct rapidjson::internal::TypeHelper<ValueType, short>
{
	static short Get(const ValueType& value)
	{
		return static_cast<short>(value.GetInt());
	}
};

template <typename ValueType>
struct rapidjson::internal::TypeHelper<ValueType, std::string>
{
	static std::string Get(const ValueType& value)
	{
		return value.GetString();
	}
};

template <typename ValueType>
struct rapidjson::internal::TypeHelper<ValueType, char>
{
	static char Get(const ValueType& value)
	{
		return static_cast<char>(value.GetInt());
	}
};

template <typename ValueType>
struct rapidjson::internal::TypeHelper<ValueType, std::uint8_t>
{
	static std::uint8_t Get(const ValueType& value)
	{
		return static_cast<std::uint8_t>(value.GetInt());
	}
};

namespace Utils::JSON
{
	using Allocator = rapidjson::MemoryPoolAllocator<rapidjson::CrtAllocator>;

	template <typename T>
	rapidjson::Value MakeArray(const T* values, std::size_t length, Allocator& allocator)
	{
		rapidjson::Value array(rapidjson::kArrayType);

		for (std::size_t i = 0; i < length; ++i)
		{
			rapidjson::Value value(values[i]);
			array.PushBack(value, allocator);
		}

		return array;
	}

	template <typename T>
	void CopyArray(T* destination, const rapidjson::Value& member, std::size_t count = 0)
	{
		if (count == 0)
		{
			count = member.Size();
		}

		for (std::size_t i = 0; i < count; ++i)
		{
			destination[i] = member[static_cast<rapidjson::SizeType>(i)].Get<T>();
		}
	}

	Game::Bounds ReadBounds(const rapidjson::Value& value);
	rapidjson::Value ToJson(const Game::Bounds& bounds, Allocator& allocator);

	bool TryReadFlags(const std::string& binaryFlags, std::size_t size, unsigned long& flags);
}
