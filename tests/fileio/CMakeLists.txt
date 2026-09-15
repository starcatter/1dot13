cmake_minimum_required(VERSION 3.12)
project(ja2_portable_fileio_tests LANGUAGES CXX)

enable_testing()

add_executable(portable_fileio_tests
	fileio_tests.cpp
	../../sgp/fileio/PhysicalWritableStore.cpp
	../../sgp/fileio/StoreRouter.cpp
)
target_include_directories(portable_fileio_tests PRIVATE ../../sgp)
set_target_properties(portable_fileio_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(portable_fileio_tests PRIVATE /W4)
else()
	target_compile_options(portable_fileio_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME portable_fileio_tests COMMAND portable_fileio_tests)

add_executable(save_transaction_tests
	save_transaction_tests.cpp
	FakeDurableFileOperations.cpp
	../../sgp/fileio/PhysicalWritableStore.cpp
	../../sgp/fileio/SaveTransaction.cpp
	../../sgp/fileio/WindowsDurableFileOperations.cpp
)
target_include_directories(save_transaction_tests PRIVATE ../../sgp .)
set_target_properties(save_transaction_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(save_transaction_tests PRIVATE /W4 /WX)
else()
	target_compile_options(save_transaction_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME save_transaction_tests COMMAND save_transaction_tests)

add_executable(local_time_tests
	local_time_tests.cpp
	../../sgp/fileio/LocalTime.cpp
)
target_include_directories(local_time_tests PRIVATE ../../sgp)
set_target_properties(local_time_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(local_time_tests PRIVATE /W4 /WX)
else()
	target_compile_options(local_time_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME local_time_tests COMMAND local_time_tests)

add_executable(log_store_tests
	log_store_tests.cpp
	../../sgp/fileio/LogStore.cpp
	../../sgp/fileio/PhysicalWritableStore.cpp
	../../sgp/fileio/StoreRouter.cpp
)
target_include_directories(log_store_tests PRIVATE ../../sgp)
set_target_properties(log_store_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(log_store_tests PRIVATE /W4 /WX)
else()
	target_compile_options(log_store_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

find_package(Threads REQUIRED)
target_link_libraries(log_store_tests PRIVATE Threads::Threads)

add_test(NAME log_store_tests COMMAND log_store_tests)
