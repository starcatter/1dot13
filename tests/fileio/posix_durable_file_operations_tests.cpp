#include "fileio/DurableFileOperationsFactory.h"
#include "fileio/DurableFileOperations.h"
#include "fileio/FileIO.h"
#include "fileio/PhysicalWritableStore.h"
#include "fileio/SaveTransaction.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
namespace fs = std::filesystem;
using ja2::fileio::DurableFileOperations;
using ja2::fileio::Error;
using ja2::fileio::ErrorCode;
using ja2::fileio::PhysicalWritableStore;
using ja2::fileio::SaveTransaction;
using ja2::fileio::StagedBytes;

void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

template<typename Callable>
void requireError(ErrorCode expected, Callable&& callable, const char* message)
{
	try
	{
		callable();
	}
	catch (const Error& error)
	{
		if (error.code() == expected) return;
		throw std::runtime_error(std::string(message) + ": wrong error code");
	}
	throw std::runtime_error(std::string(message) + ": no error");
}

class TemporaryDirectory
{
public:
	TemporaryDirectory()
	{
		const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		for (unsigned int index = 0; index < 100; ++index)
		{
			path_ = fs::temp_directory_path() /
				("ja2-posix-durable-tests-" + std::to_string(stamp) + "-" + std::to_string(index));
			std::error_code error;
			if (fs::create_directory(path_, error)) return;
			if (error && error != std::errc::file_exists)
				throw fs::filesystem_error("create test directory", error);
		}
		throw std::runtime_error("could not create a unique test directory");
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

std::string read(PhysicalWritableStore& store, std::string_view name)
{
	auto file = store.openRead(name);
	std::string result(static_cast<std::size_t>(file->size()), '\0');
	file->readExact(result.data(), result.size());
	return result;
}

void write(PhysicalWritableStore& store, std::string_view name, std::string_view value)
{
	auto file = store.create(name);
	file->writeExact(value.data(), value.size());
	file->flush();
}

void testSyncAndNoClobberMove()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	std::unique_ptr<DurableFileOperations> durable =
		ja2::fileio::makeDurableFileOperations(store);
	store.ensureDirectory("nested");
	write(store, "nested/source.dat", "source");
	durable->syncFile("nested/source.dat");
	durable->syncDirectory("nested");
	durable->replace("nested/source.dat", "nested/destination.dat");
	durable->syncDirectory("nested");
	require(!store.exists("nested/source.dat"), "durable move retained its source");
	require(read(store, "nested/destination.dat") == "source", "durable move changed bytes");

	write(store, "nested/second.dat", "second");
	requireError(ErrorCode::alreadyExists, [&]
	{
		durable->replace("nested/second.dat", "nested/destination.dat");
	}, "durable move clobbered its destination");
	require(read(store, "nested/second.dat") == "second", "failed move removed source");
}

void testValidationAndMissingFiles()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	std::unique_ptr<DurableFileOperations> durable =
		ja2::fileio::makeDurableFileOperations(store);
	store.ensureDirectory("a");
	store.ensureDirectory("b");
	write(store, "a/source.dat", "source");

	requireError(ErrorCode::invalidPath, [&]
	{
		durable->replace("a/source.dat", "b/destination.dat");
	}, "cross-directory durable move accepted");
	requireError(ErrorCode::invalidPath, [&] { durable->syncFile("../escape"); },
		"traversing durable path accepted");
	requireError(ErrorCode::notFound, [&] { durable->syncFile("missing.dat"); },
		"missing durable file accepted");
}

void testRealSaveTransaction()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	std::unique_ptr<DurableFileOperations> durable =
		ja2::fileio::makeDurableFileOperations(store);
	store.ensureDirectory("SavedGames");
	write(store, "SavedGames/slot.sav", "old");
	const std::string replacement = "new-save";
	SaveTransaction transaction(store, *durable, "SavedGames/slot.sav");
	transaction.commit(StagedBytes{replacement.data(), replacement.size()});
	require(read(store, "SavedGames/slot.sav") == replacement,
		"real POSIX save transaction did not publish replacement");
	require(store.list("SavedGames/*").size() == 1,
		"real POSIX save transaction left recovery artifacts");
}
}

int main()
{
	try
	{
		testSyncAndNoClobberMove();
		testValidationAndMissingFiles();
		testRealSaveTransaction();
		std::cout << "POSIX durable file operation tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "POSIX durable file operation tests failed: " << error.what() << '\n';
		return 1;
	}
}
