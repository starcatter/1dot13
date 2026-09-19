#include "audio/AudioBackend.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace Audio
{
namespace
{
constexpr int kMaximumVolume = 255;
constexpr int kMaximumPan = 255;
constexpr int kCenterPan = 128;

struct RiffSizeRepair
{
	bool needed = false;
	std::uint32_t correctedSize = 0;
};

std::uint32_t ReadLittleEndian32(const std::uint8_t* bytes)
{
	return static_cast<std::uint32_t>(bytes[0]) |
		(static_cast<std::uint32_t>(bytes[1]) << 8) |
		(static_cast<std::uint32_t>(bytes[2]) << 16) |
		(static_cast<std::uint32_t>(bytes[3]) << 24);
}

void WriteLittleEndian32(std::uint8_t* bytes, std::uint32_t value)
{
	bytes[0] = static_cast<std::uint8_t>(value);
	bytes[1] = static_cast<std::uint8_t>(value >> 8);
	bytes[2] = static_cast<std::uint8_t>(value >> 16);
	bytes[3] = static_cast<std::uint8_t>(value >> 24);
}

RiffSizeRepair FindRiffSizeRepair(const std::uint8_t* data, std::size_t size)
{
	if (!data || size < 12 || size - 8 > std::numeric_limits<std::uint32_t>::max())
		return {};

	if (std::memcmp(data, "RIFF", 4) != 0 || std::memcmp(data + 8, "WAVE", 4) != 0)
		return {};

	const auto actualPayloadSize = static_cast<std::uint32_t>(size - 8);
	if (ReadLittleEndian32(data + 4) >= actualPayloadSize)
		return {};

	return {true, actualPayloadSize};
}

// Several original JA2 speech WAVs contain a RIFF size smaller than the real
// file. FMOD tolerated this; strict decoders can reject the data chunk as
// lying beyond the declared end of file. Repair only this known undersized
// case and leave every other byte untouched.
bool RepairRiffSize(std::vector<std::uint8_t>& data)
{
	const RiffSizeRepair repair = FindRiffSizeRepair(data.data(), data.size());
	if (!repair.needed)
		return false;

	WriteLittleEndian32(data.data() + 4, repair.correctedSize);
	return true;
}

struct CallbackIo
{
	FileCallbacks callbacks;
	void* handle = nullptr;
	std::int64_t size = -1;
	RiffSizeRepair riffRepair;

	~CallbackIo()
	{
		if (handle && callbacks.close)
			callbacks.close(handle);
	}
};

Sint64 SDLCALL CallbackSize(void* userdata)
{
	return static_cast<CallbackIo*>(userdata)->size;
}

Sint64 SDLCALL CallbackSeek(void* userdata, Sint64 offset, SDL_IOWhence whence)
{
	auto& io = *static_cast<CallbackIo*>(userdata);
	if (!io.callbacks.seek || !io.callbacks.tell)
	{
		return -1;
	}

	std::int64_t base = 0;
	if (whence == SDL_IO_SEEK_CUR)
		base = io.callbacks.tell(io.handle);
	else if (whence == SDL_IO_SEEK_END)
		base = io.size;
	else if (whence != SDL_IO_SEEK_SET)
		return -1;
	if (base < 0 || offset > std::numeric_limits<std::int64_t>::max() - base ||
		offset < std::numeric_limits<std::int64_t>::min() + base)
	{
		return -1;
	}

	const std::int64_t position = base + offset;
	if (position < 0 || position > io.size || position > std::numeric_limits<int>::max())
		return -1;

	// bfVFS historically implements SEEK_END incorrectly for uncompressed SLF
	// members. Normalize decoder seeks to absolute offsets so packaged assets
	// behave exactly like loose files.
	if (io.callbacks.seek(io.handle, static_cast<int>(position), SEEK_SET) != 0)
		return -1;
	return io.callbacks.tell(io.handle);
}

size_t SDLCALL CallbackRead(void* userdata, void* destination, size_t requested,
	SDL_IOStatus* status)
{
	auto& io = *static_cast<CallbackIo*>(userdata);
	if (!io.callbacks.read || !io.callbacks.tell || requested == 0)
		return 0;

	const int start = io.callbacks.tell(io.handle);
	if (start < 0)
	{
		if (status)
			*status = SDL_IO_STATUS_ERROR;
		return 0;
	}

	const int requestSize = static_cast<int>(std::min(
		requested, static_cast<size_t>(std::numeric_limits<int>::max())));
	const int result = io.callbacks.read(destination, requestSize, io.handle);
	if (result < 0)
	{
		if (status)
			*status = SDL_IO_STATUS_ERROR;
		return 0;
	}

	const size_t bytesRead = static_cast<size_t>(result);
	if (io.riffRepair.needed && bytesRead != 0)
	{
		auto* bytes = static_cast<std::uint8_t*>(destination);
		std::array<std::uint8_t, 4> corrected{};
		WriteLittleEndian32(corrected.data(), io.riffRepair.correctedSize);
		for (int fileOffset = 4; fileOffset < 8; ++fileOffset)
		{
			if (fileOffset >= start && static_cast<size_t>(fileOffset - start) < bytesRead)
				bytes[fileOffset - start] = corrected[static_cast<size_t>(fileOffset - 4)];
		}
	}

	if (bytesRead < requested && status)
		*status = (static_cast<std::int64_t>(start) + static_cast<std::int64_t>(bytesRead) >= io.size)
			? SDL_IO_STATUS_EOF : SDL_IO_STATUS_ERROR;
	return bytesRead;
}

bool SDLCALL CallbackClose(void* userdata)
{
	std::unique_ptr<CallbackIo> io(static_cast<CallbackIo*>(userdata));
	return true;
}

SDL_IOStream* OpenCallbackIo(const FileCallbacks& callbacks, const char* filename)
{
	if (!filename || !callbacks.open || !callbacks.close || !callbacks.read ||
		!callbacks.seek || !callbacks.tell || !callbacks.size)
	{
		SDL_SetError("Incomplete JA2 audio file callback set");
		return nullptr;
	}

	auto io = std::make_unique<CallbackIo>();
	io->callbacks = callbacks;
	io->handle = callbacks.open(filename);
	if (!io->handle)
	{
		SDL_SetError("Could not open JA2 audio resource '%s'", filename);
		return nullptr;
	}

	const int originalPosition = callbacks.tell(io->handle);
	io->size = callbacks.size(io->handle);
	if (originalPosition < 0 || io->size < 0)
	{
		SDL_SetError("Could not measure JA2 audio resource '%s'", filename);
		return nullptr;
	}
	if (callbacks.seek(io->handle, 0, SEEK_SET) != 0)
	{
		SDL_SetError("Could not rewind JA2 audio resource '%s'", filename);
		return nullptr;
	}

	std::array<std::uint8_t, 12> header{};
	const int headerSize = callbacks.read(header.data(), static_cast<int>(header.size()), io->handle);
	if (headerSize == static_cast<int>(header.size()))
		io->riffRepair = FindRiffSizeRepair(header.data(), static_cast<std::size_t>(io->size));
	if (callbacks.seek(io->handle, originalPosition, SEEK_SET) != 0)
	{
		SDL_SetError("Could not restore JA2 audio resource '%s'", filename);
		return nullptr;
	}

	SDL_IOStreamInterface interface{};
	SDL_INIT_INTERFACE(&interface);
	interface.size = CallbackSize;
	interface.seek = CallbackSeek;
	interface.read = CallbackRead;
	interface.close = CallbackClose;

	SDL_IOStream* stream = SDL_OpenIO(&interface, io.get());
	if (stream)
		io.release();
	return stream;
}

struct SdlMixerStream
{
	MIX_Audio* audio = nullptr;
	SDL_IOStream* io = nullptr;
	MIX_Track* track = nullptr;
	int loopCount = -1;
	std::vector<std::uint8_t> repairedData;
};

SdlMixerStream* ToStream(StreamHandle handle)
{
	return reinterpret_cast<SdlMixerStream*>(handle.value);
}

StreamHandle FromStream(SdlMixerStream* stream)
{
	return {reinterpret_cast<std::uintptr_t>(stream)};
}

struct ChannelState
{
	MIX_Track* track = nullptr;
	SdlMixerStream* stream = nullptr;
	int volume = kMaximumVolume;
	int pan = kCenterPan;
};

class SdlMixerAudioBackend final : public Backend
{
public:
	~SdlMixerAudioBackend() override { shutdown(); }

	const char* name() const noexcept override { return "SDL3_mixer"; }

	void setFileCallbacks(const FileCallbacks& callbacks) override
	{
		fileCallbacks_ = callbacks;
	}

	bool initialize(const InitializationParameters& parameters) override
	{
		shutdown();
		if (parameters.sampleRate <= 0 || parameters.channelCount <= 0)
			return Fail("Invalid SDL3_mixer initialization parameters");
		if (!MIX_Init())
			return FailFromSdl("Could not initialize SDL3_mixer");
		mixerLibraryInitialized_ = true;

		SDL_AudioSpec requested{};
		requested.format = SDL_AUDIO_F32;
		requested.channels = 2;
		requested.freq = parameters.sampleRate;
		mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requested);
		if (!mixer_)
			return FailFromSdl("Could not create SDL3_mixer playback device");

		channels_.resize(static_cast<size_t>(parameters.channelCount));
		for (auto& channel : channels_)
		{
			channel.track = MIX_CreateTrack(mixer_);
			if (!channel.track)
			{
				FailFromSdl("Could not allocate SDL3_mixer tracks");
				shutdown();
				return false;
			}
		}

		SDL_AudioSpec actual{};
		if (MIX_GetMixerFormat(mixer_, &actual))
			outputRate_ = actual.freq;
		else
			outputRate_ = parameters.sampleRate;

		driverName_ = "SDL default playback device";
		const SDL_PropertiesID properties = MIX_GetMixerProperties(mixer_);
		if (properties)
		{
			const auto device = static_cast<SDL_AudioDeviceID>(SDL_GetNumberProperty(
				properties, MIX_PROP_MIXER_DEVICE_NUMBER, 0));
			if (device)
			{
				const char* name = SDL_GetAudioDeviceName(device);
				if (name && *name)
					driverName_ = name;
			}
		}

		initialized_ = true;
		lastError_.clear();
		return true;
	}

	void shutdown() override
	{
		while (!streams_.empty())
			CloseStream(*streams_.begin());

		channels_.clear();
		if (mixer_)
			MIX_DestroyMixer(mixer_);
		mixer_ = nullptr;
		if (mixerLibraryInitialized_)
			MIX_Quit();
		mixerLibraryInitialized_ = false;
		initialized_ = false;
		outputRate_ = 0;
	}

	const char* driverName() const noexcept override { return driverName_.c_str(); }
	Capabilities capabilities() const noexcept override { return {}; }
	int outputRate() const noexcept override { return outputRate_; }
	void* outputHandle() const noexcept override { return mixer_; }
	const char* lastError() const noexcept override
	{
		return lastError_.empty() ? "No SDL3_mixer error" : lastError_.c_str();
	}

	bool setStreamBufferSize(int milliseconds) override
	{
		// SDL3_mixer owns decoder buffering. Retain the compatibility call so the
		// legacy sound manager need not know which backend is active.
		return initialized_ && milliseconds >= 0;
	}

	StreamHandle openFileStream(const char* filename) override
	{
		if (!initialized_)
		{
			Fail("SDL3_mixer is not initialized");
			return {};
		}

		SDL_IOStream* io = OpenCallbackIo(fileCallbacks_, filename);
		if (!io)
		{
			FailFromSdl("Could not create SDL stream for JA2 audio resource");
			return {};
		}

		auto stream = std::make_unique<SdlMixerStream>();
		stream->io = io;
		return Register(std::move(stream));
	}

	StreamHandle openMemoryStream(const void* data, std::size_t size) override
	{
		if (!initialized_ || !data || size == 0)
		{
			Fail(!initialized_ ? "SDL3_mixer is not initialized" : "Empty in-memory audio source");
			return {};
		}

		auto stream = std::make_unique<SdlMixerStream>();
		const auto* source = static_cast<const std::uint8_t*>(data);
		if (FindRiffSizeRepair(source, size).needed)
		{
			stream->repairedData.assign(source, source + size);
			RepairRiffSize(stream->repairedData);
			source = stream->repairedData.data();
		}

		stream->audio = MIX_LoadAudioNoCopy(mixer_, source, size, false);
		if (!stream->audio)
		{
			FailFromSdl("Could not decode in-memory JA2 audio");
			return {};
		}

		return Register(std::move(stream));
	}

	bool setLoopCount(StreamHandle handle, int count) override
	{
		SdlMixerStream* stream = Find(handle);
		if (!stream || count < -1)
			return Fail("Invalid SDL3_mixer loop count");
		stream->loopCount = count;
		return true;
	}

	Channel play(StreamHandle handle, Channel requestedChannel, bool paused) override
	{
		SdlMixerStream* stream = Find(handle);
		if (!stream || requestedChannel < 0 ||
			static_cast<size_t>(requestedChannel) >= channels_.size())
		{
			Fail("Invalid SDL3_mixer stream or channel");
			return InvalidChannel;
		}

		ChannelState& channel = channels_[static_cast<size_t>(requestedChannel)];
		if (channel.stream && channel.stream != stream)
		{
			Fail("SDL3_mixer channel is already assigned");
			return InvalidChannel;
		}

		// Tracks are pooled just like the old FMOD channels. Do not let gain or
		// panning from their previous occupant bleed into a newly started sound.
		if (!MIX_SetTrackGain(channel.track, 1.0f) ||
			!MIX_SetTrackStereo(channel.track, nullptr))
		{
			FailFromSdl("Could not reset SDL3_mixer track state");
			return InvalidChannel;
		}
		channel.volume = kMaximumVolume;
		channel.pan = kCenterPan;

		bool assigned = false;
		if (stream->audio)
			assigned = MIX_SetTrackAudio(channel.track, stream->audio);
		else if (stream->io)
			assigned = MIX_SetTrackIOStream(channel.track, stream->io, false);
		if (!assigned)
		{
			FailFromSdl("Could not assign SDL3_mixer track input");
			return InvalidChannel;
		}

		SDL_PropertiesID options = SDL_CreateProperties();
		const bool configured = options != 0 && SDL_SetNumberProperty(
			options, MIX_PROP_PLAY_LOOPS_NUMBER, stream->loopCount);
		const bool started = configured && MIX_PlayTrack(channel.track, options);
		if (options)
			SDL_DestroyProperties(options);
		if (!started)
		{
			if (stream->audio)
				MIX_SetTrackAudio(channel.track, nullptr);
			else
				MIX_SetTrackIOStream(channel.track, nullptr, false);
			FailFromSdl("Could not start SDL3_mixer track");
			return InvalidChannel;
		}

		channel.stream = stream;
		stream->track = channel.track;
		if (paused && !MIX_PauseTrack(channel.track))
		{
			MIX_StopTrack(channel.track, 0);
			if (stream->audio)
				MIX_SetTrackAudio(channel.track, nullptr);
			else
				MIX_SetTrackIOStream(channel.track, nullptr, false);
			channel.stream = nullptr;
			stream->track = nullptr;
			FailFromSdl("Could not pause newly-started SDL3_mixer track");
			return InvalidChannel;
		}

		lastError_.clear();
		return requestedChannel;
	}

	bool stop(StreamHandle handle) override
	{
		SdlMixerStream* stream = Find(handle);
		if (!stream)
			return false;
		if (!stream->track)
			return true;
		if (!MIX_StopTrack(stream->track, 0))
			return FailFromSdl("Could not stop SDL3_mixer track");
		return true;
	}

	bool close(StreamHandle handle) override
	{
		SdlMixerStream* stream = Find(handle);
		return stream ? CloseStream(stream) : false;
	}

	bool isPlaying(Channel channel) const override
	{
		const ChannelState* state = GetChannel(channel);
		return state && state->stream &&
			(MIX_TrackPlaying(state->track) || MIX_TrackPaused(state->track));
	}

	bool setPaused(Channel channel, bool paused) override
	{
		ChannelState* state = GetChannel(channel);
		if (!state || !state->stream)
			return false;
		const bool result = paused ? MIX_PauseTrack(state->track) : MIX_ResumeTrack(state->track);
		return result ? true : FailFromSdl(paused ? "Could not pause SDL3_mixer track" :
			"Could not resume SDL3_mixer track");
	}

	bool setVolume(Channel channel, int volume) override
	{
		ChannelState* state = GetChannel(channel);
		if (!state || !state->stream)
			return false;
		state->volume = std::clamp(volume, 0, kMaximumVolume);
		if (!MIX_SetTrackGain(state->track,
			static_cast<float>(state->volume) / static_cast<float>(kMaximumVolume)))
		{
			return FailFromSdl("Could not set SDL3_mixer track volume");
		}
		return true;
	}

	int volume(Channel channel) const override
	{
		const ChannelState* state = GetChannel(channel);
		return state && state->stream ? state->volume : 0;
	}

	bool setPan(Channel channel, int pan) override
	{
		ChannelState* state = GetChannel(channel);
		if (!state || !state->stream)
			return false;

		state->pan = std::clamp(pan, 0, kMaximumPan);
		if (state->pan == kCenterPan)
		{
			if (!MIX_SetTrackStereo(state->track, nullptr))
				return FailFromSdl("Could not centre SDL3_mixer track");
			return true;
		}

		const float normalized = static_cast<float>(state->pan - kCenterPan) /
			static_cast<float>(state->pan < kCenterPan ? kCenterPan : kMaximumPan - kCenterPan);
		MIX_StereoGains gains{};
		gains.left = normalized <= 0.0f ? 1.0f : 1.0f - normalized;
		gains.right = normalized >= 0.0f ? 1.0f : 1.0f + normalized;
		if (!MIX_SetTrackStereo(state->track, &gains))
			return FailFromSdl("Could not pan SDL3_mixer track");
		return true;
	}

private:
	StreamHandle Register(std::unique_ptr<SdlMixerStream> stream)
	{
		SdlMixerStream* pointer = stream.release();
		streams_.insert(pointer);
		lastError_.clear();
		return FromStream(pointer);
	}

	SdlMixerStream* Find(StreamHandle handle) const
	{
		SdlMixerStream* stream = ToStream(handle);
		return stream && streams_.find(stream) != streams_.end() ? stream : nullptr;
	}

	ChannelState* GetChannel(Channel channel)
	{
		return channel >= 0 && static_cast<size_t>(channel) < channels_.size()
			? &channels_[static_cast<size_t>(channel)] : nullptr;
	}

	const ChannelState* GetChannel(Channel channel) const
	{
		return channel >= 0 && static_cast<size_t>(channel) < channels_.size()
			? &channels_[static_cast<size_t>(channel)] : nullptr;
	}

	bool CloseStream(SdlMixerStream* stream)
	{
		if (stream->track)
		{
			MIX_StopTrack(stream->track, 0);
			if (stream->audio)
				MIX_SetTrackAudio(stream->track, nullptr);
			else
				MIX_SetTrackIOStream(stream->track, nullptr, false);

			for (auto& channel : channels_)
			{
				if (channel.stream == stream)
				{
					channel.stream = nullptr;
					channel.volume = kMaximumVolume;
					channel.pan = kCenterPan;
					break;
				}
			}
		}

		if (stream->audio)
			MIX_DestroyAudio(stream->audio);
		if (stream->io)
			SDL_CloseIO(stream->io);
		streams_.erase(stream);
		delete stream;
		return true;
	}

	bool Fail(const char* message)
	{
		lastError_ = message;
		return false;
	}

	bool FailFromSdl(const char* context)
	{
		lastError_ = context;
		const char* detail = SDL_GetError();
		if (detail && *detail)
		{
			lastError_ += ": ";
			lastError_ += detail;
		}
		return false;
	}

	FileCallbacks fileCallbacks_;
	MIX_Mixer* mixer_ = nullptr;
	std::vector<ChannelState> channels_;
	std::unordered_set<SdlMixerStream*> streams_;
	std::string driverName_ = "SDL audio unavailable";
	std::string lastError_;
	int outputRate_ = 0;
	bool initialized_ = false;
	bool mixerLibraryInitialized_ = false;
};
}

std::unique_ptr<Backend> CreatePlatformBackend()
{
	return std::make_unique<SdlMixerAudioBackend>();
}
}
