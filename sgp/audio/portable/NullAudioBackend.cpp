#include "audio/AudioBackend.h"

namespace Audio
{
namespace
{
class NullAudioBackend final : public Backend
{
public:
	const char* name() const noexcept override { return "Null audio"; }
	void setFileCallbacks(const FileCallbacks&) override {}
	bool initialize(const InitializationParameters&) override { return false; }
	void shutdown() override {}

	const char* driverName() const noexcept override { return "No audio device"; }
	Capabilities capabilities() const noexcept override { return {}; }
	int outputRate() const noexcept override { return 0; }
	void* outputHandle() const noexcept override { return nullptr; }
	const char* lastError() const noexcept override { return "No portable audio backend is configured"; }

	bool setStreamBufferSize(int) override { return false; }
	StreamHandle openFileStream(const char*) override { return {}; }
	StreamHandle openMemoryStream(const void*, std::size_t) override { return {}; }
	bool setLoopCount(StreamHandle, int) override { return false; }
	Channel play(StreamHandle, Channel, bool) override { return InvalidChannel; }
	bool stop(StreamHandle) override { return false; }
	bool close(StreamHandle) override { return false; }
	bool isPlaying(Channel) const override { return false; }
	bool setPaused(Channel, bool) override { return false; }
	bool setVolume(Channel, int) override { return false; }
	int volume(Channel) const override { return 0; }
	bool setPan(Channel, int) override { return false; }
};
}

std::unique_ptr<Backend> CreatePlatformBackend()
{
	return std::make_unique<NullAudioBackend>();
}
}
