#include "Compression.h"

#include <array>
#include <cstring>

int main()
{
	std::array<BYTE, 4096> source{};
	for (std::size_t index = 0; index < source.size(); ++index)
	{
		source[index] = static_cast<BYTE>((index * 37) & 0xff);
	}

	std::array<BYTE, 8192> compressed{};
	PTR compressor = CompressInit(source.data(), source.size());
	if (compressor == nullptr)
	{
		return 1;
	}
	const UINT32 compressedSize = Compress(compressor, compressed.data(), compressed.size());
	CompressFini(compressor);
	if (compressedSize == 0 || compressedSize > CompressedBufferSize(source.size()))
	{
		return 2;
	}

	std::array<BYTE, 4096> restored{};
	PTR decompressor = DecompressInit(compressed.data(), compressedSize);
	if (decompressor == nullptr)
	{
		return 3;
	}
	const UINT32 restoredSize = Decompress(decompressor, restored.data(), restored.size());
	DecompressFini(decompressor);
	if (restoredSize != source.size() || std::memcmp(source.data(), restored.data(), source.size()) != 0)
	{
		return 4;
	}

	return 0;
}
