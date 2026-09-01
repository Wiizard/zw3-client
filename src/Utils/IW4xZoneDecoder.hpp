#pragma once

#include <cstddef>
#include <cstdint>
#include <emmintrin.h>
#include <intrin.h>

namespace Utils
{
	// The original feedback decoder is p[i] = rotr8(c[i] ^ p[i-1], 2) ^ 0xff.
	// Rotation is linear over XOR. A parallel prefix scan resolves 16 bytes
	// in four steps, preserving the previous decoded byte across read calls.
	inline void DecodeIW4xZone(unsigned char* data, std::size_t size, unsigned char& previous)
	{
		const auto low2 = _mm_set1_epi8(0x3f);
		const auto high2 = _mm_set1_epi8(static_cast<char>(0xc0));
		const auto low4 = _mm_set1_epi8(0x0f);
		const auto high4 = _mm_set1_epi8(static_cast<char>(0xf0));
		const auto invert = _mm_set1_epi8(static_cast<char>(0xff));
		while (size >= 16)
		{
			auto value = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(data)), _mm_cvtsi32_si128(previous));
			value = _mm_or_si128(_mm_and_si128(_mm_srli_epi16(value, 2), low2), _mm_and_si128(_mm_slli_epi16(value, 6), high2));
			value = _mm_xor_si128(value, invert);

			auto shifted = _mm_slli_si128(value, 1);
			shifted = _mm_or_si128(_mm_and_si128(_mm_srli_epi16(shifted, 2), low2), _mm_and_si128(_mm_slli_epi16(shifted, 6), high2));
			value = _mm_xor_si128(value, shifted);
			shifted = _mm_slli_si128(value, 2);
			shifted = _mm_or_si128(_mm_and_si128(_mm_srli_epi16(shifted, 4), low4), _mm_and_si128(_mm_slli_epi16(shifted, 4), high4));
			value = _mm_xor_si128(value, shifted);
			value = _mm_xor_si128(value, _mm_slli_si128(value, 4));
			value = _mm_xor_si128(value, _mm_slli_si128(value, 8));

			_mm_storeu_si128(reinterpret_cast<__m128i*>(data), value);
			previous = static_cast<unsigned char>(_mm_extract_epi16(value, 7) >> 8);
			data += 16;
			size -= 16;
		}

		while (size--)
		{
			previous = static_cast<unsigned char>(_rotr8(static_cast<unsigned char>(*data ^ previous), 2) ^ 0xff);
			*data++ = previous;
		}
	}
}
