include(FetchContent)
find_package(Git REQUIRED)

function(ja2_add_sdl3_mixer)
# SDL3_mixer 3.2.4 is the first released version we use for the native audio
# backend. Keep the dependency pinned: audio decoding and callback behaviour
# are part of the compatibility surface, not a moving build-time detail.
set(sdl3_mixer_revision 72a81869b45e249e8e67102db4e98dd2441f05a1)

# JA2 data uses WAV, Ogg Vorbis and MP3. Select SDL3_mixer's self-contained
# decoders so a native build does not acquire a platform-dependent codec set.
# The remaining formats pull sizeable optional dependency trees and are not
# used by the game or the supported 1.13 data packages.
set(BUILD_SHARED_LIBS OFF)
set(SDLMIXER_INSTALL OFF)
set(SDLMIXER_TESTS OFF)
set(SDLMIXER_EXAMPLES OFF)
set(SDLMIXER_DEPS_SHARED OFF)
set(SDLMIXER_VENDORED OFF)
set(SDLMIXER_WERROR OFF)
set(SDLMIXER_FLAC OFF)
set(SDLMIXER_GME OFF)
set(SDLMIXER_MOD OFF)
set(SDLMIXER_MIDI OFF)
set(SDLMIXER_OPUS OFF)
set(SDLMIXER_WAVPACK OFF)
set(SDLMIXER_MP3 ON)
set(SDLMIXER_MP3_DRMP3 ON)
set(SDLMIXER_MP3_MPG123 OFF)
set(SDLMIXER_VORBIS_STB ON)
set(SDLMIXER_VORBIS_VORBISFILE OFF)
set(SDLMIXER_VORBIS_TREMOR OFF)

FetchContent_Declare(SDL3_mixer
  GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
  GIT_TAG ${sdl3_mixer_revision} # release-3.2.4
  GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(SDL3_mixer)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${sdl3_mixer_SOURCE_DIR}"
  RESULT_VARIABLE sdl3_mixer_revision_result
  OUTPUT_VARIABLE sdl3_mixer_actual_revision
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT sdl3_mixer_revision_result EQUAL 0 OR
    NOT sdl3_mixer_actual_revision STREQUAL sdl3_mixer_revision)
  message(FATAL_ERROR
    "${sdl3_mixer_SOURCE_DIR} is not the pinned SDL3_mixer commit ${sdl3_mixer_revision}")
endif()

message(STATUS "Using SDL3_mixer 3.2.4 from ${sdl3_mixer_SOURCE_DIR}")
endfunction()

ja2_add_sdl3_mixer()
