include(FetchContent)

# SDL3_net supplies the portable TCP transport used by the legacy multiplayer
# protocol adapter. Keep it pinned: framing and socket lifecycle behaviour are
# part of the compatibility surface we test.
if(NOT TARGET SDL3_net::SDL3_net)
  set(sdl3_net_revision 1a84a2a6b9663572f77e2eb5348d42845bac0053)
  set(SDLNET_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDLNET_TESTS OFF CACHE BOOL "" FORCE)
  set(SDLNET_EXAMPLES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL3_net
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_net.git
    GIT_TAG ${sdl3_net_revision} # release-3.2.0
    GIT_SHALLOW FALSE
  )
  FetchContent_MakeAvailable(SDL3_net)

  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY "${sdl3_net_SOURCE_DIR}"
    RESULT_VARIABLE sdl3_net_revision_result
    OUTPUT_VARIABLE sdl3_net_actual_revision
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
  )
  if(NOT sdl3_net_revision_result EQUAL 0 OR
      NOT sdl3_net_actual_revision STREQUAL sdl3_net_revision)
    message(FATAL_ERROR
      "${sdl3_net_SOURCE_DIR} is not the pinned SDL3_net commit ${sdl3_net_revision}")
  endif()
endif()
