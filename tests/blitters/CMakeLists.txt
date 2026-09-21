# Configure the repository with an x86 Windows toolchain and
# -DJA2_BUILD_BLITTER_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release, then build
# ja2_blitter_benchmarks. For Wine CTest runs set CMAKE_CROSSCOMPILING_EMULATOR.
# The executable's --help describes filtering, CSV output and ratio thresholds.
# No game assets are needed. A correctness mismatch fails and is never timed.

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
  message(FATAL_ERROR "Blitter benchmarks require 32-bit Windows with MSVC or clang-cl (x86 inline assembly).")
endif()

if(ADDRESS_SANITIZER)
  message(FATAL_ERROR "Blitter benchmarks require ADDRESS_SANITIZER=OFF for comparable timings.")
endif()
foreach(flags CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELWITHDEBINFO)
  if("${${flags}}" MATCHES "sanitize[=:]address")
    message(FATAL_ERROR "Blitter benchmarks do not support AddressSanitizer flags.")
  endif()
endforeach()

if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES "Release;RelWithDebInfo" CACHE STRING "Benchmark configurations" FORCE)
elseif(NOT CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo)$")
  message(FATAL_ERROR "Blitter benchmarks require CMAKE_BUILD_TYPE=Release or RelWithDebInfo.")
endif()

include(CheckCXXSourceCompiles)
check_cxx_source_compiles("#ifndef _M_IX86\n#error x86 required\n#endif\nint main() { __asm { nop } return 0; }" JA2_BLITTER_HAS_X86_ASM)
if(NOT JA2_BLITTER_HAS_X86_ASM)
  message(FATAL_ERROR "Blitter benchmarks require MSVC-compatible x86 inline assembly.")
endif()

# The original blitter's debug/log headers require bfVFS declarations only.
# Reuse the pinned dependency recipes, without building their libraries.
include("${PROJECT_SOURCE_DIR}/cmake/dependencies/Utf8cpp.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/dependencies/LzmaSdk.cmake")
include("${PROJECT_SOURCE_DIR}/cmake/dependencies/BfVfs.cmake")
set_target_properties(bfVFS ja2_lzma_sdk PROPERTIES EXCLUDE_FROM_ALL TRUE)

set(JA2_BLITTER_MAX_RATIO "" CACHE STRING "Maximum portable/baseline median ratio; empty reports without a timing gate")
option(JA2_BLITTER_PERFORMANCE_TEST "Register the timing benchmark with CTest" OFF)

add_library(ja2_blitters_asm SHARED
  "${PROJECT_SOURCE_DIR}/sgp/vobject_blitters.cpp")
add_library(ja2_blitters_portable SHARED
  "${PROJECT_SOURCE_DIR}/sgp/vobject_blitters_portable.cpp")
add_library(ja2_blitters_generic SHARED
  "${PROJECT_SOURCE_DIR}/sgp/vobject_blitters_portable.cpp")
target_compile_definitions(ja2_blitters_generic PRIVATE JA2_BLITTER_GENERIC_REFERENCE)
foreach(backend asm portable generic)
  target_sources(ja2_blitters_${backend} PRIVATE
    BlitterBackend.cpp BlitterBackend.def "${PROJECT_SOURCE_DIR}/sgp/ClipRectangle.cpp")
  target_include_directories(ja2_blitters_${backend} SYSTEM PRIVATE "${bfvfs_SOURCE_DIR}/include")
  target_compile_definitions(ja2_blitters_${backend} PRIVATE
    FORCE_ASSERTS_ON "JA2_BLITTER_BACKEND_NAME=\"${backend}\"")
  set_target_properties(ja2_blitters_${backend} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS OFF)
endforeach()

add_executable(ja2_blitter_benchmarks blitter_benchmarks.cpp)
# GetProcAddress's function-pointer cast is required by the Windows loader API.
target_compile_options(ja2_blitter_benchmarks PRIVATE /W4 /WX /wd4191)
add_dependencies(ja2_blitter_benchmarks ja2_blitters_asm ja2_blitters_portable ja2_blitters_generic)
foreach(target ja2_blitters_asm ja2_blitters_portable ja2_blitters_generic ja2_blitter_benchmarks)
  target_compile_options(${target} PRIVATE /O2)
  set_target_properties(${target} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/bin/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/lib/$<CONFIG>")
endforeach()

add_test(NAME blitter_correctness COMMAND ja2_blitter_benchmarks --verify-only)
add_test(NAME blitter_generic_correctness COMMAND ja2_blitter_benchmarks --baseline generic --verify-only)
set_tests_properties(blitter_correctness blitter_generic_correctness
  PROPERTIES LABELS "blitters;correctness" TIMEOUT 60)

# Check the runner's exit contract independently of any existing rendering differences.
function(blitter_runner_test name expected_exit arguments)
  add_test(NAME ${name} COMMAND "${CMAKE_COMMAND}"
    "-DBENCHMARK=$<TARGET_FILE:ja2_blitter_benchmarks>"
    "-DEMULATOR=${CMAKE_CROSSCOMPILING_EMULATOR}"
    "-DEXPECTED_EXIT=${expected_exit}" "-DARGUMENTS=${arguments}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/CheckRunner.cmake")
  set_tests_properties(${name} PROPERTIES LABELS "blitters;harness" TIMEOUT 60 RUN_SERIAL TRUE)
endfunction()
blitter_runner_test(blitter_verify_smoke 0 "--verify-only;--filter;transparent/tiny_dense")
blitter_runner_test(blitter_regression_shadow_znb_equal 0
  "--verify-only;--filter;trans_shadow_znb/equal_z")
blitter_runner_test(blitter_regression_outline_z_depth 0
  "--verify-only;--filter;outline_z/tiny_dense")
blitter_runner_test(blitter_regression_obscured_pixel_z 0
  "--verify-only;--filter;pixelate_obscured/hidden_z")
blitter_runner_test(blitter_regression_fill_endpoints 0
  "--verify-only;--filter;fill16/small_rect")
blitter_runner_test(blitter_regression_hatch_phase 0
  "--verify-only;--filter;hatch/small_rect")
blitter_runner_test(blitter_reject_samples 3 "--samples;0")
blitter_runner_test(blitter_reject_baseline 3 "--baseline;no_such_backend")
blitter_runner_test(blitter_reject_empty_selection 3 "--filter;no_such_blitter")
blitter_runner_test(blitter_ratio_gate 1
  "--filter;transparent/tiny_dense;--samples;3;--batch-ms;1;--max-ratio;0.000001")

if(JA2_BLITTER_PERFORMANCE_TEST)
  set(ratio_args)
  if(NOT JA2_BLITTER_MAX_RATIO STREQUAL "")
    list(APPEND ratio_args --max-ratio "${JA2_BLITTER_MAX_RATIO}")
  endif()
  add_test(NAME blitter_performance COMMAND ja2_blitter_benchmarks
    --samples 5 --batch-ms 5 ${ratio_args})
  set_tests_properties(blitter_performance PROPERTIES LABELS "blitters;performance" TIMEOUT 180 RUN_SERIAL TRUE)
endif()
