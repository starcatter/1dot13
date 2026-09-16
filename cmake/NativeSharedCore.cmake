# Native foothold for platform-neutral engine services. This target is
# intentionally independent of the Windows application, renderer, input, and
# audio source groups; grow it as those boundaries become portable.

find_package(Threads REQUIRED)

include(cmake/dependencies/LzmaSdk.cmake)
include(cmake/dependencies/Utf8cpp.cmake)
include(cmake/dependencies/BfVfs.cmake)
include(cmake/dependencies/Zlib.cmake)

ja2_asan_instrument_first_party()
include(cmake/Warnings.cmake)

add_library(ja2_shared_core STATIC
  sgp/Compression.cpp
  sgp/English.cpp
  sgp/FileMan.cpp
  sgp/himage.cpp
  sgp/LegacyStringConversion.cpp
  sgp/fileio/BfVfsResourceStore.cpp
  sgp/fileio/DurableFileOperationsFactory.cpp
  sgp/fileio/FileServices.cpp
  sgp/fileio/LocalTime.cpp
  sgp/fileio/LogStore.cpp
  sgp/fileio/PhysicalWritableStore.cpp
  sgp/fileio/PlatformPaths.cpp
  sgp/fileio/PosixDurableFileOperations.cpp
  sgp/fileio/SaveTransaction.cpp
  sgp/fileio/StoreRouter.cpp
  sgp/line.cpp
  sgp/platform/Clock.cpp
  sgp/platform/Thread.cpp
  sgp/platform/portable/Sleep.cpp
  sgp/stringicmp.cpp
  sgp/timer.cpp
  sgp/timing/MainLoopScheduler.cpp
  sgp/UtfConversion.cpp
)
target_include_directories(ja2_shared_core PUBLIC "${CMAKE_SOURCE_DIR}/sgp")
target_link_libraries(ja2_shared_core PUBLIC JA2::bfVFS Threads::Threads ZLIB::ZLIB PRIVATE JA2::utf8cpp)
target_compile_options(ja2_shared_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(ja2_shared_core_smoke tests/native/shared_core_smoke.cpp)
target_link_libraries(ja2_shared_core_smoke PRIVATE ja2_shared_core)
target_compile_options(ja2_shared_core_smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_shared_core_smoke COMMAND ja2_shared_core_smoke)

add_executable(ja2_legacy_types_tests tests/native/legacy_types_tests.cpp)
target_include_directories(ja2_legacy_types_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/Ja2"
  "${CMAKE_SOURCE_DIR}/sgp"
)
target_link_libraries(ja2_legacy_types_tests PRIVATE JA2::bfVFS)
target_compile_options(ja2_legacy_types_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_legacy_types_tests COMMAND ja2_legacy_types_tests)

add_executable(ja2_render_primitives_tests tests/native/render_primitives_tests.cpp)
target_link_libraries(ja2_render_primitives_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_render_primitives_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_render_primitives_tests COMMAND ja2_render_primitives_tests)

add_executable(ja2_legacy_fileman_tests tests/native/legacy_fileman_tests.cpp)
target_link_libraries(ja2_legacy_fileman_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_legacy_fileman_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_legacy_fileman_tests COMMAND ja2_legacy_fileman_tests)

add_executable(ja2_compression_tests tests/native/compression_tests.cpp)
target_link_libraries(ja2_compression_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_compression_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_compression_tests COMMAND ja2_compression_tests)

add_executable(ja2_himage_tests tests/native/himage_tests.cpp)
target_link_libraries(ja2_himage_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_himage_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_himage_tests COMMAND ja2_himage_tests)

add_executable(ja2_utf_conversion_tests tests/native/utf_conversion_tests.cpp)
target_link_libraries(ja2_utf_conversion_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_utf_conversion_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_utf_conversion_tests COMMAND ja2_utf_conversion_tests)

# Keep the focused characterization suites independently linkable while also
# making the complete native checkpoint available through one root CTest run.
add_subdirectory(tests/timing)
add_subdirectory(tests/fileio)
