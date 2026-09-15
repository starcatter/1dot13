#include "fileio/LogStore.h"
#include "fileio/PhysicalWritableStore.h"
#include "fileio/StoreRouter.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace
{
namespace fs = std::filesystem;
using ja2::fileio::File;
using ja2::fileio::LogStore;
using ja2::fileio::PhysicalWritableStore;
using ja2::fileio::ResourceStore;
using ja2::fileio::ResourceVersion;
using ja2::fileio::StoreRouter;

void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory
{
public:
	TemporaryDirectory()
	{
		path_ = fs::temp_directory_path() / ("ja2-log-store-tests-" + std::to_string(
			std::chrono::high_resolution_clock::now().time_since_epoch().count()));
		fs::create_directory(path_);
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

class EmptyResourceStore final : public ResourceStore
{
public:
	std::unique_ptr<File> open(std::string_view) override { throwMissing(); }
	std::unique_ptr<File> openFromProfile(std::string_view, std::string_view) override
	{
		throwMissing();
	}
	std::vector<ResourceVersion> openAll(std::string_view) override { return {}; }
	bool exists(std::string_view) const override { return false; }
	std::vector<ja2::fileio::DirectoryEntry> list(std::string_view) const override { return {}; }

private:
	[[noreturn]] static void throwMissing()
	{
		throw ja2::fileio::Error(ja2::fileio::ErrorCode::notFound, "missing resource");
	}
};

std::string readAll(PhysicalWritableStore& store, std::string_view name)
{
	auto file = store.openRead(name);
	std::string result(static_cast<std::size_t>(file->size()), '\0');
	file->readExact(result.data(), result.size());
	return result;
}

void testAppendTruncateRemoveAndRootRefresh()
{
	TemporaryDirectory temporary;
	EmptyResourceStore resources;
	fs::path root = temporary.path() / "root-a";
	StoreRouter router(resources, [&] { return root; });
	LogStore logs(router);

	const char bytes[] = {'a', '\0', 'b'};
	require(logs.appendBytes("portable.log", bytes, sizeof(bytes)), "append bytes");
	require(logs.appendLine("portable.log", "line\nsecond\r\nthird"), "append line");
	auto first = router.currentWritableStore();
	require(readAll(*first, "Logs/portable.log") ==
		std::string(bytes, sizeof(bytes)) + "line\r\nsecond\r\nthird\r\n",
		"binary append and CRLF line");

	root = temporary.path() / "root-b";
	require(logs.appendLine("portable.log", "new root"), "append after root switch");
	auto second = router.currentWritableStore();
	require(second != first, "router refreshed writable root");
	require(readAll(*second, "Logs/portable.log") == "new root\r\n", "current root contents");

	require(logs.truncate("portable.log"), "truncate log");
	require(second->metadata("Logs/portable.log").size == 0, "truncated size");
	require(logs.remove("portable.log"), "remove log");
	require(!second->exists("Logs/portable.log"), "removed log missing");
	require(!logs.remove("portable.log"), "remove missing log reports failure");
	require(!logs.appendLine("../escape.log", "escape"), "path escape rejected");
}

void testConcurrentLinesStayWhole()
{
	TemporaryDirectory temporary;
	EmptyResourceStore resources;
	StoreRouter router(resources, [&] { return temporary.path() / "root"; });
	LogStore logs(router);
	constexpr int threadCount = 8;
	constexpr int linesPerThread = 100;
	std::vector<std::thread> threads;
	for (int thread = 0; thread < threadCount; ++thread)
	{
		threads.emplace_back([&, thread]
		{
			for (int line = 0; line < linesPerThread; ++line)
			{
				const std::string value = std::to_string(thread) + ":" + std::to_string(line);
				require(logs.appendLine("concurrent.log", value), "concurrent append");
			}
		});
	}
	for (auto& thread : threads) thread.join();

	const std::string contents = readAll(*router.currentWritableStore(), "Logs/concurrent.log");
	std::size_t lines = 0;
	for (std::size_t offset = 0; (offset = contents.find("\r\n", offset)) != std::string::npos;
		offset += 2)
	{
		++lines;
	}
	require(lines == threadCount * linesPerThread, "concurrent line count");
	require(contents.size() >= 2 && contents.substr(contents.size() - 2) == "\r\n",
		"concurrent log ends with CRLF");
}
}

int main()
{
	try
	{
		testAppendTruncateRemoveAndRootRefresh();
		testConcurrentLinesStayWhole();
		std::cout << "log store tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "log store tests failed: " << error.what() << '\n';
		return 1;
	}
}
