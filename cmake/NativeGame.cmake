# Assemble the production game from the same module source lists as the Windows
# oracle. This target deliberately remains separate from the strict shared-core
# gates: legacy gameplay translation units are migrated incrementally while the
# portable boundaries retain -Werror coverage.

include(cmake/dependencies/Expat.cmake)
include(cmake/dependencies/Libpng.cmake)
include(cmake/dependencies/Libsmacker.cmake)
include(cmake/dependencies/Lua.cmake)
include_directories("${lua_SOURCE_DIR}/src")
include_directories("${bfvfs_SOURCE_DIR}/include")

set(native_game_definitions
  ENABLE_BRIEFINGROOM
  ROBOT_ALWAYS_READY
  FORCE_ASSERTS_ON
  BMP_RANDOM
  DISABLE_MP_INTERRUPTS_IN_COOP
  INTERRUPT_MP_DEADLOCK_FIX
  ENABLE_MP_FRIENDLY_PLAYERS_SHARE_SAME_FOV
)

add_subdirectory(lua)
target_compile_definitions(Lua PRIVATE ${native_game_definitions})
target_compile_options(Lua PRIVATE
  -fpermissive
  -include "${CMAKE_SOURCE_DIR}/sgp/platform/portable/LegacyCompilerCompatibility.h")
add_subdirectory(Multiplayer)
target_compile_definitions(Multiplayer PRIVATE ${native_game_definitions})
target_compile_options(Multiplayer PRIVATE
  -fpermissive
  -include "${CMAKE_SOURCE_DIR}/sgp/platform/portable/LegacyCompilerCompatibility.h")

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
add_subdirectory(i18n)

# The historical translation tables are engine UTF-16 data. Keep their source
# spelling unchanged for the Windows oracle, and generate a native-only copy
# whose literal prefix matches CHAR16=char16_t. Non-table i18n sources continue
# to use host wchar_t deliberately at the bfVFS boundary.
set(native_i18n_dir "${CMAKE_CURRENT_BINARY_DIR}/native-i18n")
file(MAKE_DIRECTORY "${native_i18n_dir}")
file(GLOB native_i18n_tables "${CMAKE_SOURCE_DIR}/i18n/_*Text.cpp")
foreach(table IN LISTS native_i18n_tables)
  get_filename_component(table_name "${table}" NAME)
  file(READ "${table}" table_contents)
  string(REPLACE "L\"" "u\"" table_contents "${table_contents}")
  file(WRITE "${native_i18n_dir}/${table_name}" "${table_contents}")
endforeach()
file(READ "${CMAKE_SOURCE_DIR}/i18n/LanguageStrings.cpp" language_strings_contents)
file(WRITE "${native_i18n_dir}/LanguageStrings.cpp" "${language_strings_contents}")
list(REMOVE_ITEM i18nSrc "${CMAKE_SOURCE_DIR}/i18n/LanguageStrings.cpp")
list(APPEND i18nSrc "${native_i18n_dir}/LanguageStrings.cpp")

# The old DirectSound utility is not part of the active audio service and has no
# callers in the game. The Windows build keeps compiling it as an historical
# oracle; the native build does not carry dead DirectSound implementation code.
list(REMOVE_ITEM UtilsSrc "${CMAKE_SOURCE_DIR}/Utils/dsutil.cpp")

# These INI-controlled developer tools export/import the complete localization
# corpus through bfVFS wchar_t buffers. They are not part of the game runtime;
# keep their API as native no-ops until the tool is ported independently.
list(REMOVE_ITEM i18nSrc
  "${CMAKE_SOURCE_DIR}/i18n/ExportStrings.cpp"
  "${CMAKE_SOURCE_DIR}/i18n/ImportStrings.cpp")
list(APPEND i18nSrc "${CMAKE_SOURCE_DIR}/i18n/PortableStringTools.cpp")

# The Windows crash reporter is built around vectored exception handling. Keep
# the common API present on Linux while native crash capture is introduced as a
# separate platform service.
list(REMOVE_ITEM sgpSrc "${CMAKE_SOURCE_DIR}/sgp/crash_report.cpp")
list(APPEND sgpSrc "${CMAKE_SOURCE_DIR}/sgp/crash_report_portable.cpp")
list(REMOVE_ITEM sgpSrc "${CMAKE_SOURCE_DIR}/sgp/DirectX Common.cpp")

# The production implementation is a collection of MSVC/x86 inline-assembly
# specializations.  Native builds use one portable ETRLE policy engine behind
# the same public compatibility API.
list(REMOVE_ITEM sgpSrc "${CMAKE_SOURCE_DIR}/sgp/vobject_blitters.cpp")
list(APPEND sgpSrc "${CMAKE_SOURCE_DIR}/sgp/vobject_blitters_portable.cpp")
list(REMOVE_ITEM sgpSrc "${CMAKE_SOURCE_DIR}/sgp/WinFont.cpp")
list(APPEND sgpSrc "${CMAKE_SOURCE_DIR}/sgp/WinFontPortable.cpp")

set(ExpatConsumers Editor Ja2 Laptop ModularizedTacticalAI sgp Strategic Tactical TacticalAI TileEngine Utils)
set(debugFlags $<IF:$<CONFIG:Debug>,JA2BETAVERSION;JA2TESTVERSION;DEBUG_ATTACKBUSY;WINDOWED_MODE,>)

foreach(lib IN LISTS Ja2_Libs)
  set(game_library JA2_native_${lib})
  add_library(${game_library} STATIC ${${lib}Src})
  target_compile_definitions(${game_library} PRIVATE
    ${native_game_definitions} ${debugFlags})
  target_compile_options(${game_library} PRIVATE
    -fpermissive
    -include "${CMAKE_SOURCE_DIR}/sgp/platform/portable/LegacyCompilerCompatibility.h")
  if(lib IN_LIST ExpatConsumers)
    target_link_libraries(${game_library} PRIVATE EXPAT::EXPAT)
  endif()
  if(lib STREQUAL "sgp")
    target_link_libraries(${game_library} PRIVATE
      JA2::utf8cpp PNG::PNG ZLIB::ZLIB SDL3::SDL3)
    target_compile_definitions(${game_library} PRIVATE NO_ZLIB_COMPRESSION)
  endif()
endforeach()

add_library(JA2_native_i18n STATIC ${i18nSrc})
target_include_directories(JA2_native_i18n PRIVATE "${CMAKE_SOURCE_DIR}/i18n")
target_compile_definitions(JA2_native_i18n PRIVATE
  ${native_game_definitions} ${debugFlags})
target_compile_options(JA2_native_i18n PRIVATE
  -fpermissive
  -include "${CMAKE_SOURCE_DIR}/sgp/platform/portable/LegacyCompilerCompatibility.h")

add_executable(JA2-native sgp/sgp.cpp)
target_compile_definitions(JA2-native PRIVATE
  ${native_game_definitions} ${debugFlags})
target_compile_options(JA2-native PRIVATE
  -fpermissive
  -include "${CMAKE_SOURCE_DIR}/sgp/platform/portable/LegacyCompilerCompatibility.h")
set(native_game_link_group
  JA2_native_i18n
  JA2::bfVFS
  Lua
  Multiplayer
)
foreach(lib IN LISTS Ja2_Libs)
  list(APPEND native_game_link_group JA2_native_${lib})
endforeach()
target_link_libraries(JA2-native PRIVATE
  "$<LINK_GROUP:RESCAN,${native_game_link_group}>"
  SDL3::SDL3
  Threads::Threads)
target_link_libraries(JA2_native_Utils PRIVATE libsmacker::libsmacker)

add_executable(ja2_native_blitter_tests
  tests/native/vobject_blitters_tests.cpp
  sgp/vobject_blitters_portable.cpp
  "TileEngine/renderworld_blitters_portable.cpp")
target_include_directories(ja2_native_blitter_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/sgp"
  "${CMAKE_SOURCE_DIR}/TileEngine")
target_compile_options(ja2_native_blitter_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror
  -include "${CMAKE_SOURCE_DIR}/sgp/platform/portable/LegacyCompilerCompatibility.h")
target_compile_definitions(ja2_native_blitter_tests PRIVATE
  ${native_game_definitions})
add_test(NAME ja2_native_blitter_tests COMMAND ja2_native_blitter_tests)
