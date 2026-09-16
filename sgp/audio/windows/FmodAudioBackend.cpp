#include "audio/AudioBackend.h"

#include "types.h"
#include "fmod.h"

#include <limits>
#include <string>

namespace Audio
{
namespace
{
FileCallbacks gFileCallbacks;

void* F_CALLBACKAPI FileOpen(const STR8 name)
{
	return gFileCallbacks.open ? gFileCallbacks.open(name) : nullptr;
}

void F_CALLBACKAPI FileClose(void* handle)
{
	if (gFileCallbacks.close)
		gFileCallbacks.close(handle);
}

int F_CALLBACKAPI FileRead(void* buffer, int size, void* handle)
{
	return gFileCallbacks.read ? gFileCallbacks.read(buffer, size, handle) : 0;
}

int F_CALLBACKAPI FileSeek(void* handle, int position, signed char mode)
{
	return gFileCallbacks.seek ? gFileCallbacks.seek(handle, position, mode) : -1;
}

int F_CALLBACKAPI FileTell(void* handle)
{
	return gFileCallbacks.tell ? gFileCallbacks.tell(handle) : -1;
}

const char* ErrorString(int error)
{
	switch (error)
	{
		case FMOD_ERR_NONE: return "No errors";
		case FMOD_ERR_BUSY: return "Cannot call this command after initialization";
		case FMOD_ERR_UNINITIALIZED: return "Audio backend is not initialized";
		case FMOD_ERR_PLAY: return "Playing the sound failed";
		case FMOD_ERR_INIT: return "Error initializing output device";
		case FMOD_ERR_ALLOCATED: return "The output device is already in use";
		case FMOD_ERR_OUTPUT_FORMAT: return "The output device does not support the requested format";
		case FMOD_ERR_COOPERATIVELEVEL: return "Error setting cooperative level for hardware";
		case FMOD_ERR_CREATEBUFFER: return "Error creating hardware sound buffer";
		case FMOD_ERR_FILE_NOTFOUND: return "File not found";
		case FMOD_ERR_FILE_FORMAT: return "Unknown file format";
		case FMOD_ERR_FILE_BAD: return "Error loading file";
		case FMOD_ERR_MEMORY: return "Not enough memory";
		case FMOD_ERR_VERSION: return "The file format version is not supported";
		case FMOD_ERR_INVALID_PARAM: return "An invalid parameter was passed to the audio backend";
		case FMOD_ERR_NO_EAX: return "EAX is unavailable on this channel";
		case FMOD_ERR_CHANNEL_ALLOC: return "Failed to allocate a new channel";
		case FMOD_ERR_RECORD: return "Recording is not supported on this device";
		case FMOD_ERR_MEDIAPLAYER: return "The required media codec is not installed";
		default: return "Unknown audio backend error";
	}
}

FSOUND_STREAM* ToFmodStream(StreamHandle stream)
{
	return reinterpret_cast<FSOUND_STREAM*>(stream.value);
}

StreamHandle FromFmodStream(FSOUND_STREAM* stream)
{
	return {reinterpret_cast<std::uintptr_t>(stream)};
}

class FmodAudioBackend final : public Backend
{
public:
	const char* name() const noexcept override { return "FMOD/DirectSound"; }

	void setFileCallbacks(const FileCallbacks& callbacks) override
	{
		gFileCallbacks = callbacks;
		FSOUND_File_SetCallbacks(FileOpen, FileClose, FileRead, FileSeek, FileTell);
	}

	bool initialize(const InitializationParameters& parameters) override
	{
		FSOUND_SetOutput(FSOUND_OUTPUT_DSOUND);
		FSOUND_SetBufferSize(parameters.outputBufferMilliseconds);

		const int driver = FSOUND_GetDriver();
		const char* selectedDriverName = FSOUND_GetDriverName(driver);
		driverName_ = selectedDriverName ? selectedDriverName : "Unknown DirectSound driver";

		unsigned int caps = 0;
		FSOUND_GetDriverCaps(driver, &caps);
		capabilities_.hardwareAcceleration = (caps & FSOUND_CAPS_HARDWARE) != 0;
		capabilities_.eax2 = (caps & FSOUND_CAPS_EAX2) != 0;
		capabilities_.eax3 = (caps & FSOUND_CAPS_EAX3) != 0;

		initialized_ = FSOUND_Init(parameters.sampleRate, parameters.channelCount,
			FSOUND_INIT_GLOBALFOCUS | FSOUND_INIT_DONTLATENCYADJUST) != 0;
		if (initialized_)
			outputRate_ = FSOUND_GetOutputRate();
		return initialized_;
	}

	void shutdown() override
	{
		if (initialized_)
			FSOUND_Close();
		initialized_ = false;
	}

	const char* driverName() const noexcept override { return driverName_.c_str(); }
	Capabilities capabilities() const noexcept override { return capabilities_; }
	int outputRate() const noexcept override { return outputRate_; }
	void* outputHandle() const noexcept override { return initialized_ ? FSOUND_GetOutputHandle() : nullptr; }
	const char* lastError() const noexcept override { return ErrorString(FSOUND_GetError()); }

	bool setStreamBufferSize(int milliseconds) override
	{
		return FSOUND_Stream_SetBufferSize(milliseconds) != 0;
	}

	StreamHandle openFileStream(const char* filename) override
	{
		return FromFmodStream(FSOUND_Stream_Open(const_cast<char*>(filename), FSOUND_LOOP_NORMAL | FSOUND_2D, 0, 0));
	}

	StreamHandle openMemoryStream(const void* data, std::size_t size) override
	{
		if (size > static_cast<std::size_t>(std::numeric_limits<int>::max()))
			return {};
		auto* mutableData = const_cast<char*>(static_cast<const char*>(data));
		return FromFmodStream(FSOUND_Stream_Open(mutableData,
			FSOUND_LOADMEMORY | FSOUND_LOOP_NORMAL | FSOUND_2D, 0, static_cast<int>(size)));
	}

	bool setLoopCount(StreamHandle stream, int count) override
	{
		return FSOUND_Stream_SetLoopCount(ToFmodStream(stream), count) != 0;
	}

	Channel play(StreamHandle stream, Channel requestedChannel, bool paused) override
	{
		return FSOUND_Stream_PlayEx(requestedChannel, ToFmodStream(stream), nullptr, paused ? 1 : 0);
	}

	bool stop(StreamHandle stream) override { return FSOUND_Stream_Stop(ToFmodStream(stream)) != 0; }
	bool close(StreamHandle stream) override { return FSOUND_Stream_Close(ToFmodStream(stream)) != 0; }
	bool isPlaying(Channel channel) const override { return FSOUND_IsPlaying(channel) != 0; }
	bool setPaused(Channel channel, bool paused) override { return FSOUND_SetPaused(channel, paused ? 1 : 0) != 0; }
	bool setVolume(Channel channel, int volume) override { return FSOUND_SetVolume(channel, volume) != 0; }
	int volume(Channel channel) const override { return FSOUND_GetVolume(channel); }
	bool setPan(Channel channel, int pan) override { return FSOUND_SetPan(channel, pan) != 0; }

private:
	bool initialized_ = false;
	std::string driverName_ = "Unknown DirectSound driver";
	Capabilities capabilities_;
	int outputRate_ = 0;
};
}

std::unique_ptr<Backend> CreatePlatformBackend()
{
	return std::make_unique<FmodAudioBackend>();
}
}
