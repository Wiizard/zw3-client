#pragma once

#include <zlib.h>
#include <algorithm>
#include <cstring>
#include <memory>

namespace Utils
{
	// For the engine's serial compressed-memory reader, which requests individual
	// bytes. Input/checksum progress may run ahead of delivered output. This is
	// deliberately not a general replacement for zlib's public streaming API.
	class InflateReadAhead
	{
	public:
		void Reset()
		{
			buffer_.reset();
			begin_ = end_ = 0;
			status_ = Z_OK;
		}

		int Read(z_streamp stream, int flush)
		{
			if (!stream || !stream->next_out || !stream->avail_out) return Z_BUF_ERROR;
			const auto requested = stream->avail_out;
			while (stream->avail_out)
			{
				if (begin_ != end_)
				{
					const auto count = (std::min)(stream->avail_out, end_ - begin_);
					std::memcpy(stream->next_out, buffer_.get() + begin_, count);
					begin_ += count;
					stream->next_out += count;
					stream->avail_out -= count;
					stream->total_out += count;
					if (begin_ != end_) return Z_OK;
				}
				if (status_ != Z_OK && status_ != Z_BUF_ERROR) return status_;
				if (!stream->avail_out) return Z_OK;
				if (stream->avail_out >= Capacity)
				{
					status_ = inflate(stream, flush);
					return status_;
				}
				if (!buffer_)
				{
					buffer_.reset(new (std::nothrow) unsigned char[Capacity]);
					if (!buffer_) return inflate(stream, flush);
				}
				auto* output = stream->next_out;
				const auto available = stream->avail_out;
				stream->next_out = buffer_.get();
				stream->avail_out = Capacity;
				status_ = inflate(stream, flush);
				begin_ = 0;
				end_ = Capacity - stream->avail_out;
				stream->total_out -= end_;
				stream->next_out = output;
				stream->avail_out = available;
				if (!end_)
					return requested != available && status_ == Z_BUF_ERROR ? Z_OK : status_;
			}
			return Z_OK;
		}

	private:
		static constexpr unsigned Capacity = 32 * 1024;
		std::unique_ptr<unsigned char[]> buffer_;
		unsigned begin_ = 0, end_ = 0;
		int status_ = Z_OK;
	};
}
