include(FetchContent)

set(lzma_sdk_url
  "https://snapshot.debian.org/archive/debian/20110904T031803Z/pool/main/l/lzma/lzma_9.22.orig.tar.gz")

# Debian's timestamped snapshot preserves the upstream LZMA SDK 9.22 archive.
FetchContent_Declare(lzma_sdk
  URL "${lzma_sdk_url}"
  URL_HASH SHA256=0fa2ec459701e403b72662cf920d252466910161fa2077dd0f6d7c078002da19
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
  # This adapter owns the build; never execute build files supplied by an override.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(lzma_sdk)

set(lzma_sdk_source_dir "${lzma_sdk_SOURCE_DIR}/C")
if(NOT EXISTS "${lzma_sdk_source_dir}/7zVersion.h")
  message(FATAL_ERROR "${lzma_sdk_SOURCE_DIR} does not contain an LZMA SDK source tree")
endif()
file(READ "${lzma_sdk_source_dir}/7zVersion.h" lzma_sdk_version_header)
if(NOT lzma_sdk_version_header MATCHES "MY_VERSION[ \t]+\"9\.22 beta\"")
  message(FATAL_ERROR "${lzma_sdk_SOURCE_DIR} is not LZMA SDK 9.22 beta")
endif()

function(ja2_verify_lzma_sdk_file file expected_hash)
  file(SHA256 "${lzma_sdk_source_dir}/${file}" actual_hash)
  if(NOT actual_hash STREQUAL expected_hash)
    message(FATAL_ERROR "LZMA SDK file ${file} has unexpected SHA-256 ${actual_hash}")
  endif()
endfunction()

# Check every source and transitive header used in production. Besides guarding
# local overrides, this makes the selected subset independently reproducible.
ja2_verify_lzma_sdk_file(7zAlloc.c  4e913bf359299423cf32787e706a20bd7f9c06fcbfbce835f515317272080a7c)
ja2_verify_lzma_sdk_file(7zBuf.c    3b30e2c0f7b15d1c42c22e7bbd02a807fb6d41875335be6da8bb4472eacc3106)
ja2_verify_lzma_sdk_file(7zCrc.c    6f0c0084c2c97d6ac7344b5ea895e21ea05ef6b81076b302c888ed2ee17666bc)
ja2_verify_lzma_sdk_file(7zCrcOpt.c d032a09f9b2759eccfe068b5103896b8b3c7ca77d81b6471a421953f660659a0)
ja2_verify_lzma_sdk_file(7zDec.c    d36dad5208e5cb9722bf41adecae1d851b9e01639310d17583d1c1f0532dd2b1)
ja2_verify_lzma_sdk_file(7zIn.c     fc582a7513eb75fec6dec87c5b36cd9f99051fcd15f8378a1b504dc5979e13e9)
ja2_verify_lzma_sdk_file(7zStream.c be6c7145611d1243fa9b4d82cbcfb50dd3cb6915b0f0c273d2ede08547f5b85f)
ja2_verify_lzma_sdk_file(Bcj2.c     4316d63a03f6ba73c265b6505602bc564a8f7387152f8d3238ee84efda1651fd)
ja2_verify_lzma_sdk_file(Bra.c      b2a722506580f960e2be01e66f0a703a36a196f18f32aaaafb67afe5d104733a)
ja2_verify_lzma_sdk_file(Bra86.c    643a07bed9bac98218e4a4a057c7c6db115c3c640e1cb7312dcdf617c1ca4156)
ja2_verify_lzma_sdk_file(CpuArch.c  e8914e910378300750da6b5c807b163c118205db19764a6e3c6e45a18d5343c3)
ja2_verify_lzma_sdk_file(Lzma2Dec.c e934d9fc563d057603f1f006c920ff02775342ef1012a7ab5bbccbd374b44c82)
ja2_verify_lzma_sdk_file(LzmaDec.c  7d841214224e4c122be77c0088c4d6af709e3123f8bacaf76ac6867b02fbf46f)
ja2_verify_lzma_sdk_file(7zAlloc.h  848c45dca737944d7fbe0a5bbf132e4d68ee77ce9bc32994a409ad3defef4c13)
ja2_verify_lzma_sdk_file(7zBuf.h    f3f8a38737229b6b29e49bf8f2631abbc27345bde466896632b65715de9c8ccb)
ja2_verify_lzma_sdk_file(Types.h    1e367ddb4e774cabcc8f44321215d6593fb02dd799a4c7cb2a83b7b456768154)
ja2_verify_lzma_sdk_file(7zCrc.h    99731dd4f184d0396c014efd068a38173213628ae9b22465f60e65f166e89874)
ja2_verify_lzma_sdk_file(CpuArch.h  d3db1772711009eb1064b8b3569556e44956e0fd5edaa8a581adaf0cf32567d3)
ja2_verify_lzma_sdk_file(7z.h       a02bd00c33d39f03726bd56aa09b9fe47b0b6ba118b20d6a7a757a1ccfc72862)
ja2_verify_lzma_sdk_file(Bcj2.h     e3a9a99f0ed0e2a6e45554cf2662b952716a4509b2a5c05a8173cae1d0f68d3e)
ja2_verify_lzma_sdk_file(Bra.h      1865ca3e7e97a695406a602330a5e8feb58c09182f363dbd3d2497b22cac1359)
ja2_verify_lzma_sdk_file(LzmaDec.h  f43966ab7858974a38f7a6c3c7dade35bcfb8763811e9cfa8690f416e7efb819)
ja2_verify_lzma_sdk_file(Lzma2Dec.h 1f673258441d2a76e540dea10278209ca84b8a0f0d6c79d768a3e9fc5fa63408)
ja2_verify_lzma_sdk_file(7zVersion.h 61e728d7295a86f8d716bf7f66c1d90a0206ef73e5ae841ada045a41adf1aa96)
message(STATUS "Using LZMA SDK 9.22 from ${lzma_sdk_SOURCE_DIR}")

# bfVFS needs only the ANSI-C decoder and allocator closure. Its 7z writer emits
# Copy-method archives itself and does not use the SDK's encoder implementation.
add_library(ja2_lzma_sdk STATIC
  "${lzma_sdk_source_dir}/7zAlloc.c"
  "${lzma_sdk_source_dir}/7zBuf.c"
  "${lzma_sdk_source_dir}/7zCrc.c"
  "${lzma_sdk_source_dir}/7zCrcOpt.c"
  "${lzma_sdk_source_dir}/7zDec.c"
  "${lzma_sdk_source_dir}/7zIn.c"
  "${lzma_sdk_source_dir}/7zStream.c"
  "${lzma_sdk_source_dir}/Bcj2.c"
  "${lzma_sdk_source_dir}/Bra.c"
  "${lzma_sdk_source_dir}/Bra86.c"
  "${lzma_sdk_source_dir}/CpuArch.c"
  "${lzma_sdk_source_dir}/Lzma2Dec.c"
  "${lzma_sdk_source_dir}/LzmaDec.c"
)
add_library(LZMA::SDK ALIAS ja2_lzma_sdk)
target_include_directories(ja2_lzma_sdk SYSTEM PUBLIC "${lzma_sdk_source_dir}")

# Keep third-party diagnostics separate from the project's /W4 /WX policy.
if(MSVC)
  target_compile_options(ja2_lzma_sdk PRIVATE /W3 /WX-)
endif()
