# TODO: make this work
# Usage:
# cmake --toolchain cmake/toolchains/mingw.cmake ...
#
# Unlike clang-cl.cmake this needs no MSVC_SDK: mingw-w64 ships its own Windows
# headers and import libraries in its sysroot, so the compiler finds everything
# itself. Install the i686 cross toolchain (mingw-w64-gcc) and it is on PATH as
# i686-w64-mingw32-*.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(triple ${CMAKE_SYSTEM_PROCESSOR}-w64-mingw32)

# Find every tool here rather than trusting a bare name, so the toolchain file
# alone decides which cross compiler, resource compiler and archiver are used.
foreach(tool gcc g++ windres ar)
  string(TOUPPER "${tool}" _variable)
  string(REPLACE "+" "X" _variable "${_variable}")
  find_program(MINGW_${_variable} NAMES ${triple}-${tool} REQUIRED)
endforeach()

set(CMAKE_C_COMPILER "${MINGW_GCC}")
set(CMAKE_CXX_COMPILER "${MINGW_GXX}")
set(CMAKE_RC_COMPILER "${MINGW_WINDRES}")
set(CMAKE_AR "${MINGW_AR}")

set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER_TARGET ${triple})
