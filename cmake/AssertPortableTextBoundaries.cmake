# UTF conversion policy is owned by sgp/UtfConversion. Keep host conversion
# APIs out of gameplay and platform code so invalid-input and truncation
# behavior remain identical across backends.
set(_ja2_text_boundary_roots
  Editor
  Ja2
  Laptop
  ModularizedTacticalAI
  Multiplayer
  Strategic
  Tactical
  TacticalAI
  TileEngine
  Utils
  export/src
  i18n
  lua
  sgp
  wine
)

set(_ja2_text_boundary_sources)
foreach(_ja2_text_boundary_root IN LISTS _ja2_text_boundary_roots)
  file(GLOB_RECURSE _ja2_text_boundary_root_sources CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/${_ja2_text_boundary_root}/*.c"
    "${CMAKE_SOURCE_DIR}/${_ja2_text_boundary_root}/*.cc"
    "${CMAKE_SOURCE_DIR}/${_ja2_text_boundary_root}/*.cpp"
    "${CMAKE_SOURCE_DIR}/${_ja2_text_boundary_root}/*.cxx"
    "${CMAKE_SOURCE_DIR}/${_ja2_text_boundary_root}/*.h"
    "${CMAKE_SOURCE_DIR}/${_ja2_text_boundary_root}/*.hpp"
  )
  list(APPEND _ja2_text_boundary_sources ${_ja2_text_boundary_root_sources})
endforeach()

set(_ja2_text_boundary_violations)
foreach(_ja2_text_boundary_source IN LISTS _ja2_text_boundary_sources)
  file(READ "${_ja2_text_boundary_source}" _ja2_text_boundary_contents)
  if(_ja2_text_boundary_contents MATCHES "MultiByteToWideChar|WideCharToMultiByte")
    list(APPEND _ja2_text_boundary_violations "${_ja2_text_boundary_source}")
  endif()
endforeach()

if(_ja2_text_boundary_violations)
  list(JOIN _ja2_text_boundary_violations "\n  " _ja2_text_boundary_violation_list)
  message(FATAL_ERROR
    "Direct Win32 text conversion bypasses sgp/UtfConversion in:\n"
    "  ${_ja2_text_boundary_violation_list}")
endif()

unset(_ja2_text_boundary_contents)
unset(_ja2_text_boundary_root)
unset(_ja2_text_boundary_root_sources)
unset(_ja2_text_boundary_roots)
unset(_ja2_text_boundary_source)
unset(_ja2_text_boundary_sources)
unset(_ja2_text_boundary_violation_list)
unset(_ja2_text_boundary_violations)
