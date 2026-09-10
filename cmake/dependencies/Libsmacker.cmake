include(FetchContent)
find_package(Git REQUIRED)

set(libsmacker_revision 76094fb9c8e98bd5fac982c504e8d9aeff3ece01)

# A FETCHCONTENT_SOURCE_DIR_LIBSMACKER override must already contain the patch.
FetchContent_Declare(libsmacker
  GIT_REPOSITORY https://github.com/greg-kennedy/libsmacker.git
  GIT_TAG ${libsmacker_revision}
  GIT_SHALLOW FALSE
  PATCH_COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR=<SOURCE_DIR>"
    "-DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/patches/libsmacker-ja2-audio-buffer.patch"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    -P "${CMAKE_CURRENT_LIST_DIR}/ApplyLibsmackerPatch.cmake"
  # This adapter owns the build; never execute CMake supplied by an override.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(libsmacker)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${libsmacker_SOURCE_DIR}"
  RESULT_VARIABLE libsmacker_revision_result
  OUTPUT_VARIABLE libsmacker_actual_revision
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT libsmacker_revision_result EQUAL 0 OR
    NOT libsmacker_actual_revision STREQUAL libsmacker_revision)
  message(FATAL_ERROR
    "${libsmacker_SOURCE_DIR} is not the pinned libsmacker commit ${libsmacker_revision}")
endif()

if(NOT EXISTS "${libsmacker_SOURCE_DIR}/smacker.c" OR
    NOT EXISTS "${libsmacker_SOURCE_DIR}/smacker.h")
  message(FATAL_ERROR "${libsmacker_SOURCE_DIR} does not contain a libsmacker source tree")
endif()

# Filled with the patched production source hash. This also rejects an unpatched
# FETCHCONTENT_SOURCE_DIR_LIBSMACKER override.
file(SHA256 "${libsmacker_SOURCE_DIR}/smacker.c" libsmacker_source_hash)
if(NOT libsmacker_source_hash STREQUAL
    "b92a42ea4564dd8660a60662d754b67ff1e3f47b8c30a8eb17eebd8f2ab054c2")
  message(FATAL_ERROR
    "Patched libsmacker file smacker.c has unexpected SHA-256 ${libsmacker_source_hash}")
endif()
message(STATUS "Using patched libsmacker from ${libsmacker_SOURCE_DIR}")

add_library(ja2_libsmacker STATIC "${libsmacker_SOURCE_DIR}/smacker.c")
add_library(libsmacker::libsmacker ALIAS ja2_libsmacker)
target_include_directories(ja2_libsmacker SYSTEM PUBLIC "${libsmacker_SOURCE_DIR}")

# Keep third-party diagnostics separate from the project's /W4 /WX policy.
if(MSVC)
  target_compile_options(ja2_libsmacker PRIVATE /W3 /WX-)
endif()
