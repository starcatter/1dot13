# Native foothold for platform-neutral engine services. This target is
# intentionally independent of the Windows application, renderer, input, and
# audio source groups; grow it as those boundaries become portable.

find_package(Threads REQUIRED)

include(cmake/dependencies/LzmaSdk.cmake)
include(cmake/dependencies/Utf8cpp.cmake)
include(cmake/dependencies/BfVfs.cmake)

ja2_asan_instrument_first_party()
include(cmake/Warnings.cmake)

add_library(ja2_shared_core STATIC
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
  sgp/platform/Clock.cpp
  sgp/platform/Thread.cpp
  sgp/platform/portable/Sleep.cpp
  sgp/stringicmp.cpp
  sgp/timer.cpp
  sgp/timing/MainLoopScheduler.cpp
)
target_include_directories(ja2_shared_core PUBLIC "${CMAKE_SOURCE_DIR}/sgp")
target_link_libraries(ja2_shared_core PUBLIC JA2::bfVFS Threads::Threads)
target_compile_options(ja2_shared_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(ja2_shared_core_smoke tests/native/shared_core_smoke.cpp)
target_link_libraries(ja2_shared_core_smoke PRIVATE ja2_shared_core)
target_compile_options(ja2_shared_core_smoke PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_shared_core_smoke COMMAND ja2_shared_core_smoke)

# Keep the focused characterization suites independently linkable while also
# making the complete native checkpoint available through one root CTest run.
add_subdirectory(tests/timing)
add_subdirectory(tests/fileio)
