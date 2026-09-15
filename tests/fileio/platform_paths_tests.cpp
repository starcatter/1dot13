#include "fileio/PlatformPaths.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
namespace fs = std::filesystem;

void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory
{
public:
	TemporaryDirectory()
	{
		const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		path_ = fs::temp_directory_path() / ("ja2-platform-path-tests-" + std::to_string(stamp));
		if (!fs::create_directory(path_)) throw std::runtime_error("could not create test directory");
	}
	~TemporaryDirectory()
	{
		std::error_code ignored;
		fs::remove_all(path_, ignored);
	}
	const fs::path& path() const { return path_; }

private:
	fs::path path_;
};

class CurrentDirectoryGuard
{
public:
	CurrentDirectoryGuard() : original_(fs::current_path()) {}
	~CurrentDirectoryGuard()
	{
		std::error_code ignored;
		fs::current_path(original_, ignored);
	}

private:
	fs::path original_;
};
}

int main()
{
	try
	{
		const fs::path executableDirectory = fs::u8path(ja2::fileio::executableDirectory());
		require(fs::is_directory(executableDirectory), "executable directory is not a directory");

#ifdef __linux__
		const fs::path expected = fs::canonical("/proc/self/exe").parent_path();
		require(fs::equivalent(executableDirectory, expected),
			"Linux executable directory does not match /proc/self/exe");
		CurrentDirectoryGuard guard;
		TemporaryDirectory temporary;
		require(ja2::fileio::setCurrentDirectory(temporary.path().u8string()),
			"could not change current directory");
		require(fs::equivalent(fs::u8path(ja2::fileio::executableDirectory()), expected),
			"Linux executable directory depends on current directory");
#endif

		std::cout << "platform path tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "platform path tests failed: " << error.what() << '\n';
		return 1;
	}
}
