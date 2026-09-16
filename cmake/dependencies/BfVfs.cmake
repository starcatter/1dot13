include(FetchContent)
find_package(Git REQUIRED)

set(bfvfs_revision 45979f389886d9d9e6ac9fc097e8fdff536ddd47)
set(bfvfs_url
  "https://sourceforge.net/code-snapshots/hg/b/bf/bfvfs/code/bfvfs-code-${bfvfs_revision}.zip")

# SourceForge generates this revision-pinned ZIP on demand. The archive hash
# protects the download, while the production manifest below identifies the
# patched source independently of ZIP metadata and also validates overrides.
FetchContent_Declare(bfvfs
  URL "${bfvfs_url}"
  URL_HASH SHA256=339da355cc76ba5defb12d0a6b502278610dbfb74be7b9cf54738cead8456ff2
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  PATCH_COMMAND
    "${CMAKE_COMMAND}"
    "-DSOURCE_DIR=<SOURCE_DIR>"
    "-DPATCH_FILE=${CMAKE_CURRENT_LIST_DIR}/patches/bfvfs-ja2.patch"
    "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
    "-DWORK_DIR=${CMAKE_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_LIST_DIR}/ApplyBfVfsPatch.cmake"
  # This adapter owns the build; never execute bfVFS's legacy CMake files.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(bfvfs)

file(GLOB_RECURSE bfvfs_production_files
  RELATIVE "${bfvfs_SOURCE_DIR}"
  "${bfvfs_SOURCE_DIR}/include/*.h"
  "${bfvfs_SOURCE_DIR}/src/*.cpp"
)
list(SORT bfvfs_production_files)
list(LENGTH bfvfs_production_files bfvfs_production_file_count)
if(NOT bfvfs_production_file_count EQUAL 71)
  message(FATAL_ERROR
    "Patched bfVFS has ${bfvfs_production_file_count} production files; expected 71")
endif()

set(bfvfs_manifest "")
set(bfvfs_sources "")
foreach(file IN LISTS bfvfs_production_files)
  file(SHA256 "${bfvfs_SOURCE_DIR}/${file}" file_hash)
  string(APPEND bfvfs_manifest "${file_hash}  ${file}\n")
  if(file MATCHES "\\.cpp$")
    list(APPEND bfvfs_sources "${bfvfs_SOURCE_DIR}/${file}")
  endif()
endforeach()
string(SHA256 bfvfs_manifest_hash "${bfvfs_manifest}")
if(NOT bfvfs_manifest_hash STREQUAL
    "5fda690de112c151aa2071a2ccc5eefa55a8ac8ea6c7a7bd57de186dd39bca7b")
  message(FATAL_ERROR
    "Patched bfVFS production manifest has unexpected SHA-256 ${bfvfs_manifest_hash}")
endif()
message(STATUS "Using patched bfVFS 1.0.1 from ${bfvfs_SOURCE_DIR}")

add_library(bfVFS STATIC ${bfvfs_sources})
add_library(JA2::bfVFS ALIAS bfVFS)
target_include_directories(bfVFS SYSTEM PUBLIC "${bfvfs_SOURCE_DIR}/include")
target_compile_definitions(bfVFS PUBLIC VFS_STATIC VFS_WITH_SLF VFS_WITH_7ZIP)
target_link_libraries(bfVFS PRIVATE JA2::utf8cpp LZMA::SDK)

if(WIN32)
  target_compile_definitions(bfVFS PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# Keep third-party diagnostics separate from the project's /W4 /WX policy.
if(MSVC)
  target_compile_options(bfVFS PRIVATE /W3 /WX-)
endif()
