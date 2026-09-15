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

add_executable(scheduler_tests
	scheduler_tests.cpp
	../../sgp/timing/MainLoopScheduler.cpp
)
target_include_directories(scheduler_tests PRIVATE ../../sgp)
set_target_properties(scheduler_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(scheduler_tests PRIVATE /W4 /WX)
else()
	target_compile_options(scheduler_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME scheduler_tests COMMAND scheduler_tests)

add_executable(thread_tests
	thread_tests.cpp
	../../sgp/platform/Thread.cpp
)
target_include_directories(thread_tests PRIVATE ../../sgp)
set_target_properties(thread_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(thread_tests PRIVATE /W4 /WX)
else()
	target_compile_options(thread_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

target_link_libraries(thread_tests PRIVATE Threads::Threads)
add_test(NAME thread_tests COMMAND thread_tests)
