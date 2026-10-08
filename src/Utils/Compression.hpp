#pragma once

namespace Utils::Compression
{
	class ZLib
	{
	public:
		static std::string Compress(const std::string& data);
		static std::string Decompress(const std::string& data);
	};
}
