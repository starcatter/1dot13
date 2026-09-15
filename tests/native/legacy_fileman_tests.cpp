#include "FileMan.h"
#include "fileio/PhysicalWritableStore.h"

#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
class TemporaryDirectory
{
public:
	TemporaryDirectory()
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		path_ = std::filesystem::temp_directory_path() /
			("ja2-legacy-fileman-" + std::to_string(stamp));
		if (!std::filesystem::create_directory(path_))
		{
			throw std::runtime_error("failed to create test directory");
		}
	}

	~TemporaryDirectory()
	{
		std::error_code error;
		std::filesystem::remove_all(path_, error);
	}

	const std::filesystem::path& path() const { return path_; }

private:
	std::filesystem::path path_;
};
}

int main()
{
	TemporaryDirectory temporary;
	ja2::fileio::PhysicalWritableStore store(temporary.path());
	const std::array<char, 6> expected{{'J', 'A', '2', '-', '1', '3'}};

	{
		std::unique_ptr<ja2::fileio::File> file = store.create("legacy.bin");
		BorrowedFileHandle handle(*file, FILE_ACCESS_WRITE);
		UINT32 written = 0;
		if (!handle.get() || !FileWrite(handle, expected.data(), expected.size(), &written) ||
			written != expected.size() || FileGetPos(handle) != static_cast<INT32>(expected.size()))
		{
			return 1;
		}
	}

	{
		std::unique_ptr<ja2::fileio::File> file = store.openRead("legacy.bin");
		BorrowedFileHandle handle(*file, FILE_ACCESS_READ);
		std::array<char, 6> actual{};
		UINT32 read = 0;
		if (FileGetSize(handle) != expected.size() ||
			!FileRead(handle, actual.data(), actual.size(), &read) ||
			read != actual.size() || actual != expected || !FileCheckEndOfFile(handle))
		{
			return 2;
		}
	}

	ShutdownFileManager();
	return 0;
}
