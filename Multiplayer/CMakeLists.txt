add_library(Multiplayer
"client.cpp"
"network_stub.h"
"server.cpp"
"transfer_rules.cpp"
)

if(TARGET SDL3_net::SDL3_net)
  target_sources(Multiplayer PRIVATE
    "network_availability.cpp"
    "netshim/netshim.cpp"
  )
  target_compile_definitions(Multiplayer PRIVATE JA2_USE_SDL3_NET)
  target_link_libraries(Multiplayer PRIVATE SDL3_net::SDL3_net SDL3::SDL3)
else()
  # Keep the 32-bit Wine oracle build available while its host remains on the
  # Win32 presentation stack. Native builds always take the SDL3_net branch.
  target_sources(Multiplayer PRIVATE "network_stub.cpp")
endif()
target_include_directories(Multiplayer PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}
${CMAKE_CURRENT_SOURCE_DIR}/netshim
)
target_link_libraries(Multiplayer PRIVATE
EXPAT::EXPAT
)
