#include <vfs/Core/File/vfs_file.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
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
			("ja2-bfvfs-case-path-" + std::to_string(stamp));
		if (!std::filesystem::create_directory(path_))
			throw std::runtime_error("failed to create test directory");
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

char readByte(const std::filesystem::path& path)
{
	vfs::CFile file(vfs::Path(path.string()));
	if (!file.openRead()) return '\0';
	char value = '\0';
	if (file.read(reinterpret_cast<vfs::Byte*>(&value), 1) != 1) return '\0';
	return value;
}
}

int main()
{
	TemporaryDirectory temporary;
	const std::filesystem::path mixedDirectory = temporary.path() / "MixedDirectory";
	std::filesystem::create_directory(mixedDirectory);

	{
		std::ofstream(mixedDirectory / "Bigitems.slf", std::ios::binary).put('F');
		const std::filesystem::path differentlyCased =
			temporary.path() / "mixeddirectory" / "BigItems.slf";
		if (readByte(differentlyCased) != 'F') return 1;
	}

	{
		std::ofstream(mixedDirectory / "Exact.slf", std::ios::binary).put('A');
		std::ofstream(mixedDirectory / "exact.slf", std::ios::binary).put('B');
		if (readByte(mixedDirectory / "exact.slf") != 'B') return 2;
	}

	return 0;
}
