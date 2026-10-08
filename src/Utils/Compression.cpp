#include "STDInclude.hpp"

#include <zlib.h>

#include "Compression.hpp"

namespace Utils::Compression
{
	constexpr std::size_t chunk = 16384;

	std::string ZLib::Compress(const std::string& data)
	{
		Memory::Allocator allocator;
		auto length = static_cast<uLongf>(data.size() * 2);

		if (!length)
		{
			length = 2;
		}

		if (length < 100)
		{
			length *= 10;
		}

		auto* buffer = allocator.AllocateArray<char>(length);

		if (compress2(reinterpret_cast<Bytef*>(buffer), &length, reinterpret_cast<const Bytef*>(data.data()), static_cast<uLong>(data.size()), Z_BEST_COMPRESSION) != Z_OK)
		{
			return {};
		}

		return std::string(buffer, length);
	}

	std::string ZLib::Decompress(const std::string& data)
	{
		z_stream stream{};
		std::string buffer;

		if (inflateInit(&stream) != Z_OK)
		{
			return {};
		}

		int result;
		Memory::Allocator allocator;

		auto* dest = allocator.AllocateArray<std::uint8_t>(chunk);
		const auto* dataPtr = data.data();

		do
		{
			stream.avail_in = static_cast<uInt>(std::min(chunk, data.size() - (dataPtr - data.data())));
			stream.next_in = reinterpret_cast<const Bytef*>(dataPtr);
			dataPtr += stream.avail_in;

			do
			{
				stream.avail_out = static_cast<uInt>(chunk);
				stream.next_out = dest;

				result = inflate(&stream, Z_NO_FLUSH);

				if (result != Z_OK && result != Z_STREAM_END)
				{
					inflateEnd(&stream);
					return {};
				}

				buffer.append(reinterpret_cast<const char*>(dest), chunk - stream.avail_out);

			} while (stream.avail_out == 0);

		} while (result != Z_STREAM_END);

		inflateEnd(&stream);
		return buffer;
	}
}
