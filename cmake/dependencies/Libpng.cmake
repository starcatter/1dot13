include(FetchContent)
find_package(Git REQUIRED)

set(libpng_revision f54a50aa4ee5ff33dde46f3a20c9838c5db6f9ca)

# A FETCHCONTENT_SOURCE_DIR_LIBPNG override must already contain both patches.
FetchContent_Declare(libpng
  GIT_REPOSITORY https://github.com/pnggroup/libpng.git
  GIT_TAG ${libpng_revision} # v1.2.50
  GIT_SHALLOW FALSE
  PATCH_COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR=<SOURCE_DIR>"
    "-DPATCH_DIR=${CMAKE_CURRENT_LIST_DIR}/patches"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    -P "${CMAKE_CURRENT_LIST_DIR}/ApplyLibpngPatches.cmake"
  # This adapter owns the build; never execute CMake supplied by an override.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(libpng)

execute_process(
  COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
  WORKING_DIRECTORY "${libpng_SOURCE_DIR}"
  RESULT_VARIABLE libpng_revision_result
  OUTPUT_VARIABLE libpng_actual_revision
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(NOT libpng_revision_result EQUAL 0 OR NOT libpng_actual_revision STREQUAL libpng_revision)
  message(FATAL_ERROR "${libpng_SOURCE_DIR} is not the pinned libpng commit ${libpng_revision}")
endif()

function(ja2_verify_libpng_file file expected_hash)
  file(SHA256 "${libpng_SOURCE_DIR}/${file}" actual_hash)
  if(NOT actual_hash STREQUAL expected_hash)
    message(FATAL_ERROR "Patched libpng file ${file} has unexpected SHA-256 ${actual_hash}")
  endif()
endfunction()

ja2_verify_libpng_file(png.c       1b01832a44a94287fea2143cb88444729d44fac6d19729d85172569c21828975)
ja2_verify_libpng_file(pngerror.c  2c77f9875b392c5ecac78faf23d1c03682860eed00add6f9bf4daa1163f8f32c)
ja2_verify_libpng_file(pnggccrd.c  de8c80db09f32eb427a50f62c223b846e0835c2fd015d63bc6fb2344b130c15f)
ja2_verify_libpng_file(pngget.c    848897d41a02a07b6a8ccadebaaa8cbfe8c35f39b4d4d05597606840ca6d5803)
ja2_verify_libpng_file(pngmem.c    c4263074fc75687c1941fc13073425f76901c7fa478cbd8e6cfc9cbe995796de)
ja2_verify_libpng_file(pngpread.c  ea98ccb535efc81457ee307eff4b6d45bb0f9089ec18e4130bdcd570b5397ec7)
ja2_verify_libpng_file(pngread.c   549fd6cee7726f82c8c26c86861e9d96b7a229e3258d26a458f5fd0db3ed9db4)
ja2_verify_libpng_file(pngrio.c    087e2bb9f0a7a2df591e4e0fc5184a1d34b76856b98fc272448c40b2b3a00c0f)
ja2_verify_libpng_file(pngrtran.c  eba34cbdbe41f59547cfa88e8ffbfcdd18ab56977769ab2bb0786edb8e1a86b4)
ja2_verify_libpng_file(pngrutil.c  dda0fe1b12d6b3f09d165e4710cca226b14f7d47e96aac128b35a4cb8034cb5a)
ja2_verify_libpng_file(pngset.c    bfdc42f10bb1bd4d949351d838b3cb6fed39b6a06eac8c265db9132ec7870bf6)
ja2_verify_libpng_file(pngtrans.c  4afa5b38ee3d15e3e72b648d1677e09a56ff0d9007e19dc414735352e35954fe)
ja2_verify_libpng_file(pngwio.c    25a8e731c9b7cbaa3a689a02baab1d3043fdde750fa9158e36458157cf99bdaf)
ja2_verify_libpng_file(pngwrite.c  40fec5aad7cec79df729d92e09e64fcd682567534c925a541f6e7ae13f3499e3)
ja2_verify_libpng_file(pngwtran.c  28c60ccd507dcc8565981e29af780ea4feac1387c708227ffb19785f6ecdb269)
ja2_verify_libpng_file(pngwutil.c  872bed4724b486e38a5645f91fc06a787e7b4e49dc20d735a7eacba38caaa20e)
ja2_verify_libpng_file(png.h       bb591f6422a7779eff51c4b14571f0bdaee36d8dcc31cec64288cde798ac65fd)
ja2_verify_libpng_file(pngconf.h   4c3a75ff01a50cf222a4a3ae64ba6e6e344b5ff4db4d0e5fa23fccebc1436de4)
message(STATUS "Using APNG-patched libpng 1.2.50 from ${libpng_SOURCE_DIR}")

add_library(ja2_png STATIC
  "${libpng_SOURCE_DIR}/png.c"
  "${libpng_SOURCE_DIR}/pngerror.c"
  "${libpng_SOURCE_DIR}/pnggccrd.c"
  "${libpng_SOURCE_DIR}/pngget.c"
  "${libpng_SOURCE_DIR}/pngmem.c"
  "${libpng_SOURCE_DIR}/pngpread.c"
  "${libpng_SOURCE_DIR}/pngread.c"
  "${libpng_SOURCE_DIR}/pngrio.c"
  "${libpng_SOURCE_DIR}/pngrtran.c"
  "${libpng_SOURCE_DIR}/pngrutil.c"
  "${libpng_SOURCE_DIR}/pngset.c"
  "${libpng_SOURCE_DIR}/pngtrans.c"
  "${libpng_SOURCE_DIR}/pngwio.c"
  "${libpng_SOURCE_DIR}/pngwrite.c"
  "${libpng_SOURCE_DIR}/pngwtran.c"
  "${libpng_SOURCE_DIR}/pngwutil.c"
)
add_library(PNG::PNG ALIAS ja2_png)

target_include_directories(ja2_png SYSTEM PUBLIC "${libpng_SOURCE_DIR}")
target_link_libraries(ja2_png PUBLIC ZLIB::ZLIB)
target_compile_definitions(ja2_png PRIVATE _CRT_SECURE_NO_DEPRECATE)

# Keep third-party diagnostics separate from the project's /W4 /WX policy.
if(MSVC)
  target_compile_options(ja2_png PRIVATE /W3 /WX- /wd4101)
endif()
