include(FetchContent)
find_package(Git REQUIRED)

set(expat_revision 4d2c9d031a786bc19f0cd3bc594acc58e94c234f)

# Override with FETCHCONTENT_SOURCE_DIR_EXPAT for an existing checkout, or use
# FetchContent's disconnected modes after the source has been populated.
FetchContent_Declare(expat
  GIT_REPOSITORY https://github.com/libexpat/libexpat.git
  GIT_TAG ${expat_revision} # R_2_0_1
  GIT_SHALLOW FALSE
  # This adapter owns the build; never execute CMake supplied by an override.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(expat)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${expat_SOURCE_DIR}"
  RESULT_VARIABLE expat_revision_result
  OUTPUT_VARIABLE expat_actual_revision
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT expat_revision_result EQUAL 0 OR NOT expat_actual_revision STREQUAL expat_revision)
  message(FATAL_ERROR "${expat_SOURCE_DIR} is not the pinned Expat commit ${expat_revision}")
endif()

set(expat_include_dir "${expat_SOURCE_DIR}/expat/lib")
if(NOT EXISTS "${expat_include_dir}/expat.h")
  message(FATAL_ERROR "${expat_SOURCE_DIR} does not contain an Expat source tree")
endif()
file(READ "${expat_include_dir}/expat.h" expat_header)
foreach(version_part IN ITEMS
    "XML_MAJOR_VERSION 2"
    "XML_MINOR_VERSION 0"
    "XML_MICRO_VERSION 1")
  if(NOT expat_header MATCHES "#define[ \t]+${version_part}")
    message(FATAL_ERROR "${expat_SOURCE_DIR} is not Expat 2.0.1")
  endif()
endforeach()
message(STATUS "Using Expat 2.0.1 from ${expat_SOURCE_DIR}")

add_library(ja2_expat STATIC
  "${expat_SOURCE_DIR}/expat/lib/xmlparse.c"
  "${expat_SOURCE_DIR}/expat/lib/xmlrole.c"
  "${expat_SOURCE_DIR}/expat/lib/xmltok.c"
)
add_library(EXPAT::EXPAT ALIAS ja2_expat)

target_include_directories(ja2_expat SYSTEM PUBLIC
  "${expat_include_dir}"
)
target_compile_definitions(ja2_expat
  PUBLIC XML_STATIC
  PRIVATE HAVE_MEMMOVE XML_DTD XML_NS XML_CONTEXT_BYTES=1024
)

# Keep third-party diagnostics separate from the project's /W4 /WX policy.
if(MSVC)
  target_compile_options(ja2_expat PRIVATE /W3 /WX-)
endif()
