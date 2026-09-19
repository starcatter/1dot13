#include "audio/AudioBackend.h"
#include "audio/LegacySoundParameters.h"
#include "soundman.h"

#include <cassert>
#include <cstring>
#include <limits>
#include <type_traits>

// The null backend cannot reach random-sound scheduling, but linking the real
// sound-manager policy still verifies that its engine dependency is explicit.
UINT32 GameRandom(UINT32)
{
	return 0;
}

void EndOfStreamCallback(void*)
{
}

int main()
{
	static_assert(std::is_trivially_copyable<Audio::StreamHandle>::value,
		"sound channel slots rely on trivial stream handles");
	static_assert(Audio::InvalidChannel < 0, "valid mixer channels are non-negative");

	SOUNDPARMS defaultParameters;
	std::memset(&defaultParameters, 0xff, sizeof(defaultParameters));
	assert(reinterpret_cast<std::uintptr_t>(defaultParameters.EOSCallback) ==
		std::numeric_limits<std::uintptr_t>::max());
	assert(!Audio::IsSpecifiedEndOfStreamCallback(defaultParameters.EOSCallback));
	assert(!Audio::IsSpecifiedEndOfStreamCallback(nullptr));
	assert(Audio::IsSpecifiedEndOfStreamCallback(EndOfStreamCallback));

	auto backend = Audio::CreatePlatformBackend();
	assert(backend);
	assert(std::strcmp(backend->name(), "Null audio") == 0);
	assert(!backend->initialize({44100, 128, 100}));
	assert(backend->outputHandle() == nullptr);
	assert(backend->outputRate() == 0);
	assert(!backend->openFileStream("missing.ogg"));
	assert(!backend->openMemoryStream(nullptr, 0));
	assert(backend->play({}, 0, true) == Audio::InvalidChannel);
	assert(!backend->isPlaying(0));
	assert(std::strlen(backend->lastError()) != 0);
	backend->shutdown();

	assert(InitializeSoundManager());
	assert(SoundGetDriverHandle() == nullptr);
	assert(!SoundIsPlaying(0));
	assert(SoundPlayFromBuffer("silent", nullptr, 0, nullptr) == SOUND_ERROR);
	SoundSetDefaultVolume(1000);
	assert(SoundGetDefaultVolume() == 127);
	assert(SoundStopAll());
	ShutdownSoundManager();

	return 0;
}
