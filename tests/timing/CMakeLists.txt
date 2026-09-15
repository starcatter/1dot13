cmake_minimum_required(VERSION 3.12)
project(ja2_timing_tests LANGUAGES CXX)

enable_testing()

add_executable(clock_tests
	clock_tests.cpp
	../../sgp/platform/Clock.cpp
	../../sgp/timer.cpp
)
target_include_directories(clock_tests PRIVATE ../../sgp)
set_target_properties(clock_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(clock_tests PRIVATE /W4 /WX)
else()
	target_compile_options(clock_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Threads REQUIRED)
target_link_libraries(clock_tests PRIVATE Threads::Threads)
add_test(NAME clock_tests COMMAND clock_tests)
