# Native foothold for platform-neutral engine services. This target is
# intentionally independent of the Windows application, renderer, input, and
# production audio backend; grow it as those boundaries become portable.

find_package(Threads REQUIRED)
find_package(SDL3 3.2 REQUIRED CONFIG)

include(cmake/dependencies/LzmaSdk.cmake)
include(cmake/dependencies/Utf8cpp.cmake)
include(cmake/dependencies/BfVfs.cmake)
include(cmake/dependencies/Zlib.cmake)

ja2_asan_instrument_first_party()
include(cmake/Warnings.cmake)

add_library(ja2_shared_core STATIC
  sgp/application/ApplicationLoop.cpp
  sgp/application/ShutdownOnce.cpp
  sgp/audio/portable/NullAudioBackend.cpp
  sgp/Compression.cpp
  sgp/crash_telemetry.cpp
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
  sgp/platform/Process.cpp
  sgp/platform/portable/Dialog.cpp
  sgp/platform/portable/Input.cpp
  sgp/platform/portable/NativeFonts.cpp
  sgp/platform/portable/Process.cpp
  sgp/platform/Thread.cpp
  sgp/platform/portable/Sleep.cpp
  sgp/platform/portable/Window.cpp
  sgp/presentation/DirtyRegionTracker.cpp
  sgp/presentation/PixelSurface.cpp
  sgp/soundman.cpp
  sgp/stringicmp.cpp
  sgp/timer.cpp
  sgp/timing/MainLoopScheduler.cpp
  sgp/UtfConversion.cpp
  Utils/Quantize.cpp
)
target_include_directories(ja2_shared_core PUBLIC "${CMAKE_SOURCE_DIR}/sgp")
target_link_libraries(ja2_shared_core PUBLIC JA2::bfVFS Threads::Threads ZLIB::ZLIB PRIVATE JA2::utf8cpp)
target_compile_options(ja2_shared_core PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_library(ja2_sdl3_backend STATIC
  sgp/platform/sdl/Sdl3ApplicationHost.cpp
  sgp/platform/sdl/Sdl3InputTranslator.cpp
  sgp/platform/sdl/Sdl3LegacyInputSink.cpp
  sgp/presentation/sdl/Sdl3Presenter.cpp
)
target_include_directories(ja2_sdl3_backend PUBLIC "${CMAKE_SOURCE_DIR}/sgp")
target_link_libraries(ja2_sdl3_backend PUBLIC SDL3::SDL3)
target_compile_options(ja2_sdl3_backend PRIVATE -Wall -Wextra -Wpedantic -Werror)

# Compile the real framebuffer/video-manager policy natively without yet
# pretending the complete game link is available. Its many gameplay callbacks
# remain resolved by the eventual game target; this gate keeps host APIs out of
# the translation unit while that target is assembled incrementally.
add_library(ja2_native_video_manager OBJECT sgp/video.cpp)
target_include_directories(ja2_native_video_manager PUBLIC
  "${CMAKE_SOURCE_DIR}/sgp")
target_link_libraries(ja2_native_video_manager PRIVATE ja2_shared_core)
target_compile_options(ja2_native_video_manager PRIVATE
  -Wall -Wextra -Wpedantic -Werror)

add_library(ja2_native_input_manager OBJECT sgp/input.cpp)
target_include_directories(ja2_native_input_manager PUBLIC
  "${CMAKE_SOURCE_DIR}/sgp")
target_link_libraries(ja2_native_input_manager PRIVATE ja2_shared_core)
target_compile_options(ja2_native_input_manager PRIVATE
  -Wall -Wextra -Wpedantic -Werror)

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

add_executable(ja2_process_services_tests tests/native/process_services_tests.cpp)
target_link_libraries(ja2_process_services_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_process_services_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_process_services_tests COMMAND ja2_process_services_tests)

add_executable(ja2_input_backend_tests tests/native/input_backend_tests.cpp)
target_link_libraries(ja2_input_backend_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_input_backend_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_input_backend_tests COMMAND ja2_input_backend_tests)

add_executable(ja2_audio_backend_tests tests/native/audio_backend_tests.cpp)
target_link_libraries(ja2_audio_backend_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_audio_backend_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_audio_backend_tests COMMAND ja2_audio_backend_tests)

add_executable(ja2_video_interface_tests tests/native/video_interface_tests.cpp)
target_link_libraries(ja2_video_interface_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_video_interface_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_video_interface_tests COMMAND ja2_video_interface_tests)

add_executable(ja2_platform_neutral_headers_tests
  tests/native/platform_neutral_headers_tests.cpp)
target_link_libraries(ja2_platform_neutral_headers_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_platform_neutral_headers_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_platform_neutral_headers_tests COMMAND ja2_platform_neutral_headers_tests)

add_executable(ja2_quantize_tests tests/native/quantize_tests.cpp)
target_link_libraries(ja2_quantize_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_quantize_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_quantize_tests COMMAND ja2_quantize_tests)

add_executable(ja2_application_lifecycle_tests
  tests/native/application_lifecycle_tests.cpp)
target_link_libraries(ja2_application_lifecycle_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_application_lifecycle_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_application_lifecycle_tests COMMAND ja2_application_lifecycle_tests)

add_executable(ja2_presentation_foundation_tests
  tests/native/presentation_foundation_tests.cpp)
target_link_libraries(ja2_presentation_foundation_tests PRIVATE ja2_shared_core)
target_compile_options(ja2_presentation_foundation_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_presentation_foundation_tests COMMAND ja2_presentation_foundation_tests)

add_executable(ja2_sdl3_presenter_tests
  tests/native/sdl3_presenter_tests.cpp)
target_link_libraries(ja2_sdl3_presenter_tests PRIVATE
  ja2_shared_core ja2_sdl3_backend)
target_compile_options(ja2_sdl3_presenter_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_sdl3_presenter_tests COMMAND ja2_sdl3_presenter_tests)
set_tests_properties(ja2_sdl3_presenter_tests PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

add_executable(ja2_sdl3_application_host_tests
  tests/native/sdl3_application_host_tests.cpp)
target_link_libraries(ja2_sdl3_application_host_tests PRIVATE
  ja2_shared_core ja2_sdl3_backend)
target_compile_options(ja2_sdl3_application_host_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_sdl3_application_host_tests
  COMMAND ja2_sdl3_application_host_tests)
set_tests_properties(ja2_sdl3_application_host_tests PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

add_executable(ja2_sdl3_input_translator_tests
  tests/native/sdl3_input_translator_tests.cpp)
target_link_libraries(ja2_sdl3_input_translator_tests PRIVATE
  ja2_shared_core ja2_sdl3_backend)
target_compile_options(ja2_sdl3_input_translator_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_sdl3_input_translator_tests
  COMMAND ja2_sdl3_input_translator_tests)
set_tests_properties(ja2_sdl3_input_translator_tests PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

add_executable(ja2_sdl3_legacy_input_sink_tests
  tests/native/sdl3_legacy_input_sink_tests.cpp)
target_link_libraries(ja2_sdl3_legacy_input_sink_tests PRIVATE
  ja2_shared_core ja2_sdl3_backend)
target_compile_options(ja2_sdl3_legacy_input_sink_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_sdl3_legacy_input_sink_tests
  COMMAND ja2_sdl3_legacy_input_sink_tests)

add_executable(ja2_sdl3_input_pipeline_tests
  tests/native/sdl3_input_pipeline_tests.cpp
  $<TARGET_OBJECTS:ja2_native_input_manager>)
target_link_libraries(ja2_sdl3_input_pipeline_tests PRIVATE
  ja2_shared_core ja2_sdl3_backend)
target_compile_options(ja2_sdl3_input_pipeline_tests PRIVATE
  -Wall -Wextra -Wpedantic -Werror)
add_test(NAME ja2_sdl3_input_pipeline_tests
  COMMAND ja2_sdl3_input_pipeline_tests)
set_tests_properties(ja2_sdl3_input_pipeline_tests PROPERTIES
  ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software")

# Keep the focused characterization suites independently linkable while also
# making the complete native checkpoint available through one root CTest run.
add_subdirectory(tests/timing)
add_subdirectory(tests/fileio)
