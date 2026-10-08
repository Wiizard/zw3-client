#pragma once

#include <zlib.h>

namespace Utils
{
	class InflateReadAhead
	{
	public:
		void Reset()
		{
			this->buffer.reset();
			this->begin = 0;
			this->end = 0;
			this->status = Z_OK;
		}

		int Read(z_streamp stream, int flush)
		{
			if (!stream || !stream->next_out || !stream->avail_out)
			{
				return Z_BUF_ERROR;
			}

			const auto requested = stream->avail_out;

			while (stream->avail_out)
			{
				if (this->begin != this->end)
				{
					const auto count = std::min(stream->avail_out, this->end - this->begin);
					std::memcpy(stream->next_out, this->buffer.get() + this->begin, count);
					this->begin += count;
					stream->next_out += count;
					stream->avail_out -= count;
					stream->total_out += count;

					if (this->begin != this->end)
					{
						return Z_OK;
					}
				}

				if (this->status != Z_OK && this->status != Z_BUF_ERROR)
				{
					return this->status;
				}

				if (!stream->avail_out)
				{
					return Z_OK;
				}

				if (stream->avail_out >= capacity)
				{
					this->status = inflate(stream, flush);
					return this->status;
				}

				if (!this->buffer)
				{
					this->buffer.reset(new (std::nothrow) unsigned char[capacity]);

					if (!this->buffer)
					{
						return inflate(stream, flush);
					}
				}

				auto* const output = stream->next_out;
				const auto available = stream->avail_out;

				stream->next_out = this->buffer.get();
				stream->avail_out = capacity;
				this->status = inflate(stream, flush);
				this->begin = 0;
				this->end = capacity - stream->avail_out;
				stream->total_out -= this->end;
				stream->next_out = output;
				stream->avail_out = available;

				if (!this->end)
				{
					if (requested != available && this->status == Z_BUF_ERROR)
					{
						return Z_OK;
					}

					return this->status;
				}
			}

			return Z_OK;
		}

	private:
		static constexpr unsigned int capacity = 32 * 1024;

		std::unique_ptr<unsigned char[]> buffer;
		unsigned int begin = 0;
		unsigned int end = 0;
		int status = Z_OK;
	};
}
