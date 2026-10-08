#include "STDInclude.hpp"

#include "HuffmanTree.hpp"

namespace Utils::Huffman
{
	using namespace Utils::Huffman::Tree;

	int Compress(const unsigned char* input, unsigned char* output, int inputSize, int outputSize)
	{
		int outputBitCount = 0;

		for (int inputByteCount = 0; inputByteCount < inputSize && outputBitCount < outputSize * 8; ++inputByteCount)
		{
			const auto byte = input[inputByteCount];
			const auto nodeCount = compressionData[byte].nodeData.front();

			for (unsigned int nodeIndex = 1; nodeIndex <= nodeCount; ++nodeIndex)
			{
				const auto bit = static_cast<unsigned char>(
					compressionData[byte].nodeData[nodeIndex] << (outputBitCount & 7));

				if ((outputBitCount & 7) == 0)
				{
					output[outputBitCount / 8] = bit;
				}
				else
				{
					output[outputBitCount / 8] |= bit;
				}

				++outputBitCount;

				if (outputBitCount >= outputSize * 8)
				{
					break;
				}
			}
		}

		return (outputBitCount + 7) / 8;
	}

	int Decompress(const unsigned char* input, unsigned char* output, int inputSize, int outputSize)
	{
		int outputByteCount = 0;

		for (int inputBitCount = 0; inputBitCount < inputSize * 8 && outputByteCount < outputSize; ++outputByteCount)
		{
			auto nodeIndex = decompressionData.size() - 1;

			do
			{
				const bool isRightNode = ((input[inputBitCount / 8] >> (inputBitCount & 7)) & 1) != 0;

				nodeIndex = isRightNode
					? decompressionData[nodeIndex % 256].right
					: decompressionData[nodeIndex % 256].left;

				++inputBitCount;

				if (inputBitCount >= inputSize * 8)
				{
					break;
				}
			}
			while (nodeIndex >= 256);

			output[outputByteCount] = static_cast<unsigned char>(nodeIndex);
		}

		return outputByteCount;
	}
}
