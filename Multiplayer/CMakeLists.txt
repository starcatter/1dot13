add_library(Multiplayer
"client.cpp"
"network_stub.cpp"
"network_stub.h"
"server.cpp"
"transfer_rules.cpp"
)
target_include_directories(Multiplayer PUBLIC
${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(Multiplayer PRIVATE
EXPAT::EXPAT
)
