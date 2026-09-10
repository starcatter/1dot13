include(FetchContent)
find_package(Git REQUIRED)

set(zlib_revision 50893291621658f355bc5b4d450a8d06a563053d)

# Override with FETCHCONTENT_SOURCE_DIR_ZLIB for an existing checkout, or use
# FetchContent's disconnected modes after the source has been populated.
FetchContent_Declare(zlib
  GIT_REPOSITORY https://github.com/madler/zlib.git
  GIT_TAG ${zlib_revision} # v1.2.8
  GIT_SHALLOW FALSE
  # This adapter owns the build; never execute CMake supplied by an override.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(zlib)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${zlib_SOURCE_DIR}"
  RESULT_VARIABLE zlib_revision_result
  OUTPUT_VARIABLE zlib_actual_revision
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT zlib_revision_result EQUAL 0 OR NOT zlib_actual_revision STREQUAL zlib_revision)
  message(FATAL_ERROR "${zlib_SOURCE_DIR} is not the pinned zlib commit ${zlib_revision}")
endif()
if(NOT FETCHCONTENT_SOURCE_DIR_ZLIB)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
    WORKING_DIRECTORY "${zlib_SOURCE_DIR}"
    OUTPUT_VARIABLE zlib_source_changes
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(zlib_source_changes)
    message(FATAL_ERROR "Fetched zlib sources contain local changes: ${zlib_source_changes}")
  endif()
endif()

if(NOT EXISTS "${zlib_SOURCE_DIR}/zlib.h")
  message(FATAL_ERROR "${zlib_SOURCE_DIR} does not contain a zlib source tree")
endif()
file(READ "${zlib_SOURCE_DIR}/zlib.h" zlib_header)
if(NOT zlib_header MATCHES "#define[ \t]+ZLIB_VERSION[ \t]+\"1\\.2\\.8\"")
  message(FATAL_ERROR "${zlib_SOURCE_DIR} is not zlib 1.2.8")
endif()
message(STATUS "Using zlib 1.2.8 from ${zlib_SOURCE_DIR}")

add_library(ja2_zlib STATIC
  "${zlib_SOURCE_DIR}/adler32.c"
  "${zlib_SOURCE_DIR}/compress.c"
  "${zlib_SOURCE_DIR}/crc32.c"
  "${zlib_SOURCE_DIR}/deflate.c"
  "${zlib_SOURCE_DIR}/gzclose.c"
  "${zlib_SOURCE_DIR}/gzlib.c"
  "${zlib_SOURCE_DIR}/gzread.c"
  "${zlib_SOURCE_DIR}/gzwrite.c"
  "${zlib_SOURCE_DIR}/infback.c"
  "${zlib_SOURCE_DIR}/inffast.c"
  "${zlib_SOURCE_DIR}/inflate.c"
  "${zlib_SOURCE_DIR}/inftrees.c"
  "${zlib_SOURCE_DIR}/trees.c"
  "${zlib_SOURCE_DIR}/uncompr.c"
  "${zlib_SOURCE_DIR}/zutil.c"
)
add_library(ZLIB::ZLIB ALIAS ja2_zlib)

target_include_directories(ja2_zlib SYSTEM PUBLIC "${zlib_SOURCE_DIR}")
target_compile_definitions(ja2_zlib PRIVATE
  NO_FSEEKO
  _CRT_NONSTDC_NO_DEPRECATE
  _CRT_SECURE_NO_DEPRECATE
)

# Keep third-party diagnostics separate from the project's /W4 /WX policy.
if(MSVC)
  target_compile_options(ja2_zlib PRIVATE /W3 /WX-)
endif()
