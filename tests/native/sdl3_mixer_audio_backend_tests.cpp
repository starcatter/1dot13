#include "audio/AudioBackend.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
void Append16(std::vector<std::uint8_t>& bytes, std::uint16_t value)
{
	bytes.push_back(static_cast<std::uint8_t>(value));
	bytes.push_back(static_cast<std::uint8_t>(value >> 8));
}

void Append32(std::vector<std::uint8_t>& bytes, std::uint32_t value)
{
	bytes.push_back(static_cast<std::uint8_t>(value));
	bytes.push_back(static_cast<std::uint8_t>(value >> 8));
	bytes.push_back(static_cast<std::uint8_t>(value >> 16));
	bytes.push_back(static_cast<std::uint8_t>(value >> 24));
}

void Set32(std::vector<std::uint8_t>& bytes, size_t offset, std::uint32_t value)
{
	bytes[offset] = static_cast<std::uint8_t>(value);
	bytes[offset + 1] = static_cast<std::uint8_t>(value >> 8);
	bytes[offset + 2] = static_cast<std::uint8_t>(value >> 16);
	bytes[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

std::vector<std::uint8_t> MakeWave(bool undersizedRiff)
{
	constexpr std::uint32_t sampleRate = 8000;
	constexpr std::uint32_t sampleCount = 800;

	std::vector<std::uint8_t> bytes;
	bytes.insert(bytes.end(), {'R', 'I', 'F', 'F'});
	Append32(bytes, 36 + sampleCount);
	bytes.insert(bytes.end(), {'W', 'A', 'V', 'E'});
	bytes.insert(bytes.end(), {'f', 'm', 't', ' '});
	Append32(bytes, 16);
	Append16(bytes, 1); // PCM
	Append16(bytes, 1); // mono
	Append32(bytes, sampleRate);
	Append32(bytes, sampleRate); // 8-bit mono byte rate
	Append16(bytes, 1);
	Append16(bytes, 8);
	bytes.insert(bytes.end(), {'d', 'a', 't', 'a'});
	Append32(bytes, sampleCount);
	bytes.insert(bytes.end(), sampleCount, 128); // unsigned 8-bit silence

	if (undersizedRiff)
		Set32(bytes, 4, 20); // declares an EOF before the data chunk
	return bytes;
}

struct MemoryFile
{
	const std::vector<std::uint8_t>* bytes = nullptr;
	size_t position = 0;
};

std::vector<std::uint8_t> gFileBytes;
int gOpenCount = 0;
int gCloseCount = 0;

void* OpenFile(const char* name)
{
	if (!name || std::string(name) != "legacy.wav")
		return nullptr;
	++gOpenCount;
	return new MemoryFile{&gFileBytes, 0};
}

void CloseFile(void* opaque)
{
	++gCloseCount;
	delete static_cast<MemoryFile*>(opaque);
}

int ReadFile(void* destination, int requested, void* opaque)
{
	auto& file = *static_cast<MemoryFile*>(opaque);
	if (requested < 0)
		return -1;
	const size_t available = file.bytes->size() - file.position;
	const size_t count = std::min(available, static_cast<size_t>(requested));
	std::memcpy(destination, file.bytes->data() + file.position, count);
	file.position += count;
	return static_cast<int>(count);
}

int SeekFile(void* opaque, int offset, int origin)
{
	auto& file = *static_cast<MemoryFile*>(opaque);
	// Model bfVFS's useful compatibility subset. The SDL adapter must normalize
	// decoder seeks instead of depending on SEEK_CUR/SEEK_END support.
	if (origin != SEEK_SET)
		return 1;
	const std::int64_t position = offset;
	if (position < 0 || position > static_cast<std::int64_t>(file.bytes->size()))
		return 1;
	file.position = static_cast<size_t>(position);
	return 0;
}

int TellFile(void* opaque)
{
	const auto& file = *static_cast<MemoryFile*>(opaque);
	assert(file.position <= static_cast<size_t>(std::numeric_limits<int>::max()));
	return static_cast<int>(file.position);
}

std::int64_t SizeFile(void* opaque)
{
	return static_cast<std::int64_t>(static_cast<MemoryFile*>(opaque)->bytes->size());
}
}

int main()
{
	const std::vector<std::uint8_t> validWave = MakeWave(false);
	const std::vector<std::uint8_t> malformedWave = MakeWave(true);
	gFileBytes = malformedWave;

	auto backend = Audio::CreatePlatformBackend();
	assert(backend);
	assert(std::string(backend->name()) == "SDL3_mixer");
	backend->setFileCallbacks({OpenFile, CloseFile, ReadFile, SeekFile, TellFile, SizeFile});
	if (!backend->initialize({44100, 4, 100}))
	{
		std::fprintf(stderr, "SDL3_mixer initialization failed: %s\n", backend->lastError());
		return 1;
	}
	assert(backend->outputHandle() != nullptr);
	assert(backend->outputRate() > 0);

	// Ordinary cached samples retain the caller's buffer and map directly onto
	// a mixer track without an extra encoded-data copy.
	Audio::StreamHandle memory = backend->openMemoryStream(validWave.data(), validWave.size());
	assert(memory);
	assert(backend->setLoopCount(memory, 0)); // zero extra repeats: play once
	assert(backend->play(memory, 0, true) == 0);
	assert(backend->isPlaying(0));
	SDL_Delay(150);
	assert(backend->isPlaying(0)); // paused audio must not consume the clip
	assert(backend->setVolume(0, 173));
	assert(backend->volume(0) == 173);
	assert(backend->setPan(0, 0));
	assert(backend->setPan(0, 128));
	assert(backend->setPan(0, 255));
	assert(backend->setPaused(0, false));
	for (int retry = 0; retry < 100 && backend->isPlaying(0); ++retry)
		SDL_Delay(10);
	assert(!backend->isPlaying(0));
	assert(backend->close(memory));

	// The known malformed JA2 RIFF size is repaired for memory-backed samples,
	// and infinite looping is applied at play time rather than to a stopped
	// track (where SDL3_mixer deliberately ignores it).
	Audio::StreamHandle repairedMemory = backend->openMemoryStream(
		malformedWave.data(), malformedWave.size());
	assert(repairedMemory);
	assert(backend->setLoopCount(repairedMemory, -1));
	assert(backend->play(repairedMemory, 1, true) == 1);
	assert(backend->setPaused(1, false));
	SDL_Delay(250);
	assert(backend->isPlaying(1));
	assert(backend->close(repairedMemory));

	// The same repair is overlaid on reads from a genuinely streamed VFS file;
	// the complete encoded file is not copied into a second buffer.
	Audio::StreamHandle streamed = backend->openFileStream("legacy.wav");
	assert(streamed);
	assert(gOpenCount == 1);
	assert(gCloseCount == 0);
	assert(backend->setLoopCount(streamed, 0));
	assert(backend->play(streamed, 2, true) == 2);
	assert(backend->setPaused(2, false));
	assert(backend->stop(streamed));
	assert(backend->close(streamed));
	assert(gCloseCount == 1);

	assert(!backend->openFileStream("missing.wav"));
	assert(!backend->openMemoryStream(nullptr, 0));
	assert(backend->play({}, 0, false) == Audio::InvalidChannel);

	// Shutdown must safely release an outstanding streamed input and must remain
	// idempotent; the game normally closes streams first, but crash/error paths
	// are not allowed to leak VFS handles.
	Audio::StreamHandle outstanding = backend->openFileStream("legacy.wav");
	assert(outstanding);
	assert(gOpenCount == 2);
	backend->shutdown();
	assert(gCloseCount == 2);
	assert(backend->outputHandle() == nullptr);
	backend->shutdown();

	assert(backend->initialize({44100, 2, 100}));
	backend->shutdown();
	return 0;
}
