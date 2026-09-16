include(FetchContent)
find_package(Git REQUIRED)

set(utf8cpp_revision 819011bb01628fe1aa2f1da9f2c842a48fd5680b)

# Override with FETCHCONTENT_SOURCE_DIR_UTF8CPP for an existing checkout, or use
# FetchContent's disconnected modes after the source has been populated.
FetchContent_Declare(utf8cpp
  GIT_REPOSITORY https://github.com/nemtrif/utfcpp.git
  GIT_TAG ${utf8cpp_revision} # v4.1.1
  GIT_SHALLOW FALSE
  # This adapter owns the build; never execute CMake supplied by an override.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(utf8cpp)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${utf8cpp_SOURCE_DIR}"
  RESULT_VARIABLE utf8cpp_revision_result
  OUTPUT_VARIABLE utf8cpp_actual_revision
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT utf8cpp_revision_result EQUAL 0 OR NOT utf8cpp_actual_revision STREQUAL utf8cpp_revision)
  message(FATAL_ERROR "${utf8cpp_SOURCE_DIR} is not the pinned utf8cpp commit ${utf8cpp_revision}")
endif()
if(NOT FETCHCONTENT_SOURCE_DIR_UTF8CPP)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
    WORKING_DIRECTORY "${utf8cpp_SOURCE_DIR}"
    OUTPUT_VARIABLE utf8cpp_source_changes
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(utf8cpp_source_changes)
    message(FATAL_ERROR "Fetched utf8cpp sources contain local changes: ${utf8cpp_source_changes}")
  endif()
endif()

set(utf8cpp_include_dir "${utf8cpp_SOURCE_DIR}/source")
foreach(header IN ITEMS utf8.h utf8/core.h utf8/checked.h utf8/unchecked.h)
  if(NOT EXISTS "${utf8cpp_include_dir}/${header}")
    message(FATAL_ERROR "${utf8cpp_SOURCE_DIR} does not contain utf8cpp 4.1.1 production headers")
  endif()
endforeach()
message(STATUS "Using utf8cpp 4.1.1 from ${utf8cpp_SOURCE_DIR}")

add_library(ja2_utf8cpp INTERFACE)
add_library(JA2::utf8cpp ALIAS ja2_utf8cpp)
target_include_directories(ja2_utf8cpp SYSTEM INTERFACE "${utf8cpp_include_dir}")
