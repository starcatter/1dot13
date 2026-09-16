#ifndef JA2_AUDIO_BACKEND_H
#define JA2_AUDIO_BACKEND_H

#include <cstddef>
#include <cstdint>
#include <memory>

namespace Audio
{
struct StreamHandle
{
	std::uintptr_t value = 0;

	explicit operator bool() const noexcept { return value != 0; }
};

using Channel = int;
constexpr Channel InvalidChannel = -1;

struct FileCallbacks
{
	void* (*open)(const char* name) = nullptr;
	void (*close)(void* handle) = nullptr;
	int (*read)(void* buffer, int size, void* handle) = nullptr;
	int (*seek)(void* handle, int position, int mode) = nullptr;
	int (*tell)(void* handle) = nullptr;
};

struct Capabilities
{
	bool hardwareAcceleration = false;
	bool eax2 = false;
	bool eax3 = false;
};

struct InitializationParameters
{
	int sampleRate = 44100;
	int channelCount = 128;
	int outputBufferMilliseconds = 100;
};

class Backend
{
public:
	virtual ~Backend() = default;

	virtual const char* name() const noexcept = 0;
	virtual void setFileCallbacks(const FileCallbacks& callbacks) = 0;
	virtual bool initialize(const InitializationParameters& parameters) = 0;
	virtual void shutdown() = 0;

	virtual const char* driverName() const noexcept = 0;
	virtual Capabilities capabilities() const noexcept = 0;
	virtual int outputRate() const noexcept = 0;
	virtual void* outputHandle() const noexcept = 0;
	virtual const char* lastError() const noexcept = 0;

	virtual bool setStreamBufferSize(int milliseconds) = 0;
	virtual StreamHandle openFileStream(const char* filename) = 0;
	virtual StreamHandle openMemoryStream(const void* data, std::size_t size) = 0;
	virtual bool setLoopCount(StreamHandle stream, int count) = 0;
	virtual Channel play(StreamHandle stream, Channel requestedChannel, bool paused) = 0;
	virtual bool stop(StreamHandle stream) = 0;
	virtual bool close(StreamHandle stream) = 0;

	virtual bool isPlaying(Channel channel) const = 0;
	virtual bool setPaused(Channel channel, bool paused) = 0;
	virtual bool setVolume(Channel channel, int volume) = 0;
	virtual int volume(Channel channel) const = 0;
	virtual bool setPan(Channel channel, int pan) = 0;
};

// Defined by the backend selected for the target platform.
std::unique_ptr<Backend> CreatePlatformBackend();
}

#endif
