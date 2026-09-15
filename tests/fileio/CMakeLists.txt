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

if(UNIX)
	add_executable(posix_durable_file_operations_tests
		posix_durable_file_operations_tests.cpp
		../../sgp/fileio/DurableFileOperationsFactory.cpp
		../../sgp/fileio/PhysicalWritableStore.cpp
		../../sgp/fileio/PosixDurableFileOperations.cpp
		../../sgp/fileio/SaveTransaction.cpp
	)
	target_include_directories(posix_durable_file_operations_tests PRIVATE ../../sgp)
	set_target_properties(posix_durable_file_operations_tests PROPERTIES
		CXX_STANDARD 17
		CXX_STANDARD_REQUIRED YES
		CXX_EXTENSIONS NO
	)
	target_compile_options(posix_durable_file_operations_tests PRIVATE
		-Wall -Wextra -Wpedantic -Werror)
	add_test(NAME posix_durable_file_operations_tests
		COMMAND posix_durable_file_operations_tests)
endif()

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

add_executable(platform_paths_tests
	platform_paths_tests.cpp
	../../sgp/fileio/PlatformPaths.cpp
)
target_include_directories(platform_paths_tests PRIVATE ../../sgp)
set_target_properties(platform_paths_tests PROPERTIES
	CXX_STANDARD 17
	CXX_STANDARD_REQUIRED YES
	CXX_EXTENSIONS NO
)

if(MSVC)
	target_compile_options(platform_paths_tests PRIVATE /W4 /WX)
else()
	target_compile_options(platform_paths_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_test(NAME platform_paths_tests COMMAND platform_paths_tests)

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
