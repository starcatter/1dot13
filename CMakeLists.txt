cmake_minimum_required(VERSION 3.20)

# /Z7 embedded debug info so objects are self-contained and sccache-able.
# CMP0141 is a 3.25 policy; our floor is 3.20, so opt in explicitly where known.
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
endif()

project(ja2)

if(WIN32)
  set(_ja2_default_platform_backend "WINDOWS")
else()
  set(_ja2_default_platform_backend "LINUX")
endif()
set(JA2_PLATFORM_BACKEND "${_ja2_default_platform_backend}" CACHE STRING
  "Platform backend implementation")
set_property(CACHE JA2_PLATFORM_BACKEND PROPERTY STRINGS WINDOWS LINUX)
set(_ja2_valid_platform_backends WINDOWS LINUX)
if(NOT JA2_PLATFORM_BACKEND IN_LIST _ja2_valid_platform_backends)
  message(FATAL_ERROR
    "JA2_PLATFORM_BACKEND=\"${JA2_PLATFORM_BACKEND}\" is not supported. Valid options are: WINDOWS, LINUX.")
endif()
if(JA2_PLATFORM_BACKEND STREQUAL "WINDOWS" AND NOT WIN32)
  message(FATAL_ERROR "The WINDOWS platform backend requires a Windows target.")
endif()
if(JA2_PLATFORM_BACKEND STREQUAL "LINUX" AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message(FATAL_ERROR "The LINUX platform backend requires a Linux target.")
endif()
message(STATUS "Configuring JA2_PLATFORM_BACKEND=\"${JA2_PLATFORM_BACKEND}\"")

option(USE_SCCACHE "Cache compiled objects with sccache if installed" ON)
if(USE_SCCACHE)
  find_program(SCCACHE_PROGRAM sccache)
  if(SCCACHE_PROGRAM)
    message(STATUS "Using sccache: ${SCCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER   "${SCCACHE_PROGRAM}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${SCCACHE_PROGRAM}")
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LTO_OPTION "Enable link-time optimization if supported by compiler" OFF)

if(LTO_OPTION)
  include(CheckIPOSupported)
  check_ipo_supported(RESULT LinkTimeOptimization OUTPUT IpoError LANGUAGES C CXX)
  if(LinkTimeOptimization)
    message(STATUS "Configuring WITH link-time optimization")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
  else()
    message(FATAL_ERROR "LTO was requested but is unavailable: ${IpoError}")
  endif()
else()
  message(STATUS "Configuring WITHOUT link-time optimization")
endif()

# declares the ADDRESS_SANITIZER option and the _asan genexpr used below
include(cmake/AddressSanitizer.cmake)

# Keep frame pointers in first-party code on every supported compiler.
if(MSVC)
  add_compile_options(/Oy-)
else()
  add_compile_options(-fno-omit-frame-pointer)
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
  add_compile_options(-fno-delete-null-pointer-checks)
endif()

# Standard MSVC presets use the toolchain's default dynamic CRT. The Wine
# baseline explicitly selects the static CRT for portable runtime validation.
# make Debug builds /Z7 instead of /Zi so they can be sccache'd
set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
  "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>")

add_compile_definitions(CINTERFACE VFS_STATIC VFS_WITH_SLF VFS_WITH_7ZIP)
include_directories(
  "${CMAKE_SOURCE_DIR}/Ja2"
  "${CMAKE_SOURCE_DIR}/Utils"
  "${CMAKE_SOURCE_DIR}/TileEngine"
  "${CMAKE_SOURCE_DIR}/TacticalAI"
  "${CMAKE_SOURCE_DIR}/ModularizedTacticalAI/include"
  "${CMAKE_SOURCE_DIR}/Tactical"
  "${CMAKE_SOURCE_DIR}/Strategic"
  "${CMAKE_SOURCE_DIR}/sgp"
  "${CMAKE_SOURCE_DIR}/Ja2/Res"
  "${CMAKE_SOURCE_DIR}/lua"
  "${CMAKE_SOURCE_DIR}/Laptop"
  "${CMAKE_SOURCE_DIR}/Multiplayer"
  "${CMAKE_SOURCE_DIR}/Editor"
  "${CMAKE_SOURCE_DIR}/i18n/include"
)

if(JA2_PLATFORM_BACKEND STREQUAL "LINUX")
  include(cmake/NativeSharedCore.cmake)
  return()
endif()

# external libraries
include(cmake/dependencies/Expat.cmake)
include(cmake/dependencies/Zlib.cmake)
include(cmake/dependencies/Libpng.cmake)
include(cmake/dependencies/Libsmacker.cmake)
include(cmake/dependencies/Lua.cmake)
include_directories("${lua_SOURCE_DIR}/src")
include(cmake/dependencies/LzmaSdk.cmake)
include(cmake/dependencies/Utf8cpp.cmake)
include(cmake/dependencies/BfVfs.cmake)
include_directories("${bfvfs_SOURCE_DIR}/include")

# turn on asan for everything defined below; 3rd party libs above keep defaults
ja2_asan_instrument_first_party()

# from here on down our own code only: vendored code above keeps default flags
include(cmake/Warnings.cmake)

# Compile-time switches for our own code. These belong to the build system, not
# to a header: a #define buried in the source tree is invisible to anyone reading
# the build files, and cannot be varied per configuration. Nothing below should
# ever be reintroduced as a bare #define.
add_compile_definitions(
  # Laptop briefing room (Jazz). Also needs BRIEFING_ROOM in ja2_options.ini and
  # a briefing-room mod, otherwise the room comes up blank.
  ENABLE_BRIEFINGROOM
  ROBOT_ALWAYS_READY
  # keep Assert() live in configurations that would otherwise compile it out
  FORCE_ASSERTS_ON
  # sgp/random.h: the 32-bit PRNG. Turning this off returns the old generator,
  # which only ever yields 2^15 distinct values -- it breaks big maps and
  # invalidates saves.
  BMP_RANDOM
  # Multiplayer (WANNE), formerly Multiplayer/connect.h.
  # Interrupts stay off in COOP: AI interrupts computed on a pure client are
  # still wrong. This also drops the server-side ALT+E "override turn" dialog,
  # which should no longer be needed.
  DISABLE_MP_INTERRUPTS_IN_COOP
  # r5623 workaround for the enemy AI deadlocking on a pure-client interrupt
  INTERRUPT_MP_DEADLOCK_FIX
  ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
)

# ja2export utility
add_subdirectory("export/src")

# developer tools, built alongside the game
add_subdirectory(tools)

# static libraries whose translation units don't rely on Application preprocessor definitions.
add_subdirectory(lua)
add_subdirectory(Multiplayer)
add_subdirectory(wine)

set(Ja2_Libraries
"winmm.lib"
"winhttp.lib"
JA2::bfVFS
Lua
Multiplayer
wine
)

# static libraries whose translation units rely on Application preprocessor definitions.
set(Ja2_Libs
  Editor
  Ja2
  Laptop
  ModularizedTacticalAI
  sgp
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
)
foreach(lib IN LISTS Ja2_Libs)
  add_subdirectory(${lib})
endforeach()

# language library relies on Application preprocessor definition; compiled once per app
# with all 8 languages built in and selected at runtime (docs/plans/language-design.md).
add_subdirectory(i18n)

# simple function to validate the Application choice
include(cmake/ValidateOptions.cmake)

set(ValidApplications JA2 JA2MAPEDITOR JA2UB JA2UBMAPEDITOR)
ValidateOptions("${ValidApplications}" "Applications" "${Applications}" "ApplicationTargets")

set(ExpatConsumers Editor Ja2 Laptop ModularizedTacticalAI sgp Strategic Tactical TacticalAI TileEngine Utils)


# preprocessor definitions for Debug build, per the legacy MSBuild
# Define WINDOWED_MODE explicitly instead of relying on a toolchain CRT macro.
set(debugFlags $<IF:$<CONFIG:Debug>,JA2BETAVERSION;JA2TESTVERSION;DEBUG_ATTACKBUSY;WINDOWED_MODE,>)

foreach(app IN LISTS ApplicationTargets)
  set(isEditor $<STREQUAL:${app},JA2MAPEDITOR>)
  set(isUb $<STREQUAL:${app},JA2UB>)
  set(isUbEditor $<STREQUAL:${app},JA2UBMAPEDITOR>)
  set(compilationFlags
    $<IF:${isEditor},JA2EDITOR;JA2BETAVERSION,>
    $<IF:${isUb},JA2UB;JA2UBMAPS,>
    $<IF:${isUbEditor},JA2UB;JA2UBMAPS;JA2EDITOR;JA2BETAVERSION,>
  )

  foreach(lib IN LISTS Ja2_Libs)
    # library for an application, e.g. JA2UB_sgp
    set(game_library ${app}_${lib})
    add_library(${game_library})
    target_sources(${game_library} PRIVATE ${${lib}Src})
    target_compile_definitions(${game_library} PRIVATE ${compilationFlags} ${debugFlags})
    if(lib IN_LIST ExpatConsumers)
      target_link_libraries(${game_library} PRIVATE EXPAT::EXPAT)
    endif()
  endforeach()

  # executable for the application, e.g. JA2.exe
  set(exe ${app})
  add_executable(${exe} WIN32
    sgp/sgp.cpp
    Ja2/Res/ja2.rc
  )
  target_link_libraries(${exe} PRIVATE ${Ja2_Libraries})
  target_compile_definitions(${exe} PRIVATE ${compilationFlags} ${debugFlags})

  # language library for the application, e.g. JA2MAPEDITOR_i18n — one per app, all 8
  # languages compiled in, selected at runtime by BindLanguageStrings
  set(language_library ${exe}_i18n)
  add_library(${language_library})
  target_sources(${language_library} PRIVATE ${i18nSrc})
  target_compile_definitions(${language_library} PRIVATE ${compilationFlags} ${debugFlags})
  target_compile_options(${language_library} PRIVATE /w14062)
  target_link_libraries(${exe} PRIVATE ${language_library})

  # go through all game libraries again and link them to the app executable
  foreach(lib IN LISTS Ja2_Libs)
    target_link_libraries(${exe} PRIVATE ${app}_${lib})
  endforeach()

  # for Utils only
  target_link_libraries(${app}_Utils PRIVATE libsmacker::libsmacker)

  # for SGP only
  target_link_libraries(${app}_sgp PRIVATE "ddraw.lib" "${PROJECT_SOURCE_DIR}/fmodvc.lib")
  target_link_libraries(${app}_sgp PRIVATE PNG::PNG ZLIB::ZLIB)
  target_compile_definitions(${app}_sgp PRIVATE ${compilationFlags} ${debugFlags} NO_ZLIB_COMPRESSION)
endforeach()
