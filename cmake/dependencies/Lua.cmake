include(FetchContent)
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

FetchContent_Declare(lua
  URL https://www.lua.org/ftp/lua-5.1.5.tar.gz
  URL_HASH SHA256=2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333
  # This adapter owns the build; never execute CMake supplied by an override.
  SOURCE_SUBDIR _ja2_no_upstream_cmake
)
FetchContent_MakeAvailable(lua)

# FetchContent's URL hash does not apply to FETCHCONTENT_SOURCE_DIR_LUA.
# Verify every file that can affect the library so overrides remain explicit,
# reproducible production inputs.
set(lua_production_inputs
  src/lapi.c=6aa0d9fdd88b13fe46568ad6722ecf44164fb84476cfdf6480cf05b5704935f8
  src/lauxlib.c=b564650bf4aca2757b9966311d32c8d5fea8803f6f33d72a856448d3b5d6d992
  src/lbaselib.c=b14c49fae6fa4690662185bf842cb05d87756585d2f5bdd8495251fde83ef331
  src/lcode.c=027030bbef34115588bd4a64cf4c8c18236a56936835475a154277478767bac5
  src/ldblib.c=eb8b056711f037e764f746d989f4e40621d8f9ff45add3336790c389849ed0ab
  src/ldebug.c=afa5840195befaec55177273c8168748026b38df3243767fc740cc7ced0c4c8f
  src/ldo.c=d5942081ba661286327e30e22af5d11a1f4308d1f029a203c342b5f541cfa9b6
  src/ldump.c=30d83d766351bb053b3d20793a6bcd2ef0210e8fad76a7032abbe574bafa0ca3
  src/lfunc.c=525681d79098958530e5c937f7cbe94a4dafa679746ffdb16258f82b7ece29ce
  src/lgc.c=35be212ad0e4d89336f50f11cd0e991c04fe59b023ec03a626bf3f98e69fd1dc
  src/linit.c=c0c97c45862f6c978620a9682cd2201f0fc29c2afbcab824ba20d26cc1745e56
  src/liolib.c=a40f1d2c4aed9835885264cc6caf81c3bd2d5db93af3d313cc0478db37d93169
  src/llex.c=f1ade23f957e69f164ff28e133b201dff5d81726c390b060110b60907831e79b
  src/lmathlib.c=e2933a56332d567cdd69f0fe514d638a8f82da6878754ff259e77cf2157263c3
  src/lmem.c=e63c50cb1c75356bd4123c9ca05f77bd0cd99e05c099cf6d1bf472086761e9c5
  src/loadlib.c=b01bc465bb2e280fb45d24aea98d5ddd9dfd0999f44422e1151aafa1d7f5385c
  src/lobject.c=d704915c79c721556cc5850d741ed3bef1611221356f0e3a2f0ee5cfd5fd3632
  src/lopcodes.c=63cd74edc75970092a8ce078c4ab970efa1ee18de960d00eb826d49fe98d8a76
  src/loslib.c=f0d5892f152cf7e1001b086e48774ccb6e567d11f1bf521f27050c15d9a0f040
  src/lparser.c=3590f24bab5763af673182e40d33194a7de6f5eabe5432c02a548d44225d2b63
  src/lstate.c=c36d6b9354aaa30b5323b4984f3172783f69e0deb3186c2ef6f16a8281c59c29
  src/lstring.c=aa7e934c435d870b9d937ac2fb03dbfa15d367012d46e7e995a2ad8d440f2f6d
  src/lstrlib.c=e8048c79ae34be0fd29aa8db1a341cd600f5e09b9133b5810910e07b855a32d5
  src/ltable.c=3a853ce89bf5e834ec53f2a0dd2386eb8b84accbe5fdc18dabd7515b819f0f7c
  src/ltablib.c=e934dc4b2fc117a501c717302a00147f7b44eea978ef87a58a3e40e9bfe45d20
  src/ltm.c=9759ae5d1aa3b11e1c7faa6c0f65e05c7126417f7f8f3f989d82fc170b07f891
  src/lundump.c=55ccb55abde44e94e6c4d0bafc94f5870d2674d1944e9a81f6537d9ac92e8ebc
  src/lvm.c=b560aad0a1b8bfc4e4b732b2393e8f8ecc68b6c772e6d25763d6ef71c38ab709
  src/lzio.c=892bc24d898c19cbedceba419c68f910b99321c0b462be2c2c00b9d763563173
  src/lapi.h=4aea4c1b975d43184f4f1fd1a42c79cf11d74d2801315740cec07fe9abebb56d
  src/lauxlib.h=c741f9c1587f29fd08a79acbe0c46884768a47f89bee4147669f774c5cf50fc0
  src/lcode.h=97d5a37f3b1b19eda1ef39607b3358aa0f7e917568ccd27ed0f14bc6a70d4b17
  src/ldebug.h=d036f0dbb996bdecd48d505746f72c31b82fb2fc38f111b780568338d4ed9e9c
  src/ldo.h=591ad9a90098d3fbf06839da3de752d2c2585cbf5801dde4cdaffe12b23a182e
  src/lfunc.h=3cea0071ffda94eac378adb5f1ecb467ef2ffafff4315459e4ccdf05b5119da4
  src/lgc.h=fbab2b989515377729d4c029dd58b3b6242877a88a7c4cfa703ea0deeb069a1e
  src/llex.h=0f3715d22b025727ef11debd641600a123b81718d195cd8445ac6378814458e6
  src/llimits.h=1910669296681d9d2814ca46c325063724bb634c0428dae4e5ff59be54e0b758
  src/lmem.h=abd1258418486c69bf5a93ceea3a1b9f8af2c7497a4fa6c27b442ffec2c5bbb1
  src/lobject.h=274c18373fbbad71bfebd534cae4b2d2e49934abf6672f44735edc6646fb6a8b
  src/lopcodes.h=a15fe349da7c1e73b563e8c3249fe7d535eccc844cb30ec80b4e335b0699279b
  src/lparser.h=84512d6540b83ab599cd8de5a1c8c4deaf71ff353f8e4fbc2e4ca135ba4e9ef0
  src/lstate.h=dc64ea7ec74d7c1efaeaf6a9e2257f0b7856a5c988747948af2e76cf079d20fe
  src/lstring.h=c0e8af4eb33e3357be79f906e7e1191ba53a3a790d36ca15b50471d3a339e1f0
  src/ltable.h=219e6ac2bf43aaa93093c6bcd571c012660325e6b87ea3d6c1b40679a0acfd64
  src/ltm.h=54f4c2fa76b8ed8d48163e31807a19999f5d65b009ae862cb757e0773f2aa485
  src/luaconf.h=0410ff22f66c275ba8fcee1fa87a0749d26d7952ed30d3bc9161688b39775464
  src/lua.h=470551c185f058360f8d0f9e5c54a29a3950f78af6a93f3fe9e4039a380c7b87
  src/lualib.h=13f880e9dd997a0f7fd0b17ccd949c98f33e95bd120f084de66084e888217413
  src/lundump.h=9f3a2924d1585620993a70eea6cf3def34acb8322b2d9c3629bcdacc037edc64
  src/lvm.h=db4b79dc7f4e7656078aa527350fbf14b2fde5d136ccf1cb4f3dc397def5fb67
  src/lzio.h=0ec77d0179bf29c1ccab97028fed761944d5c21c02808e10cd7b5883b58b2ca6
)

set(lua_sources)
foreach(lua_input IN LISTS lua_production_inputs)
  string(REPLACE "=" ";" lua_input_parts "${lua_input}")
  list(GET lua_input_parts 0 lua_file)
  list(GET lua_input_parts 1 lua_expected_hash)
  if(NOT EXISTS "${lua_SOURCE_DIR}/${lua_file}")
    message(FATAL_ERROR "${lua_SOURCE_DIR} does not contain Lua production input ${lua_file}")
  endif()
  file(SHA256 "${lua_SOURCE_DIR}/${lua_file}" lua_actual_hash)
  if(NOT lua_actual_hash STREQUAL lua_expected_hash)
    message(FATAL_ERROR "Lua production input ${lua_file} has unexpected SHA-256 ${lua_actual_hash}")
  endif()
  if(lua_file MATCHES "\\.c$")
    list(APPEND lua_sources "${lua_SOURCE_DIR}/${lua_file}")
  endif()
endforeach()
message(STATUS "Using Lua 5.1.5 from ${lua_SOURCE_DIR}")

# This is the same Lua core and standard-library closure previously vendored.
# lua.c, luac.c, and print.c are command-line tools and remain excluded.
add_library(ja2_lua STATIC ${lua_sources})
add_library(Lua::Lua ALIAS ja2_lua)
target_include_directories(ja2_lua PUBLIC "${lua_SOURCE_DIR}/src")
