#include "fileio/PhysicalWritableStore.h"
#include "fileio/StoreRouter.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
namespace fs = std::filesystem;
using ja2::fileio::Error;
using ja2::fileio::ErrorCode;
using ja2::fileio::PhysicalWritableStore;
using ja2::fileio::ResourceStore;
using ja2::fileio::ResourceVersion;
using ja2::fileio::SeekOrigin;
using ja2::fileio::StoreRouter;

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
				("ja2-fileio-tests-" + std::to_string(stamp) + "-" + std::to_string(index));
			std::error_code error;
			if (fs::create_directory(path_, error)) return;
			if (error && error != std::errc::file_exists) throw fs::filesystem_error("create test directory", error);
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

std::string readAll(PhysicalWritableStore& store, std::string_view name)
{
	auto file = store.openRead(name);
	std::string result(static_cast<std::size_t>(file->size()), '\0');
	file->readExact(result.data(), result.size());
	return result;
}

std::string readAll(ja2::fileio::File& file)
{
	std::string result(static_cast<std::size_t>(file.size()), '\0');
	file.readExact(result.data(), result.size());
	return result;
}

class PhysicalResourceStore final : public ResourceStore
{
public:
	explicit PhysicalResourceStore(const fs::path& root) : store_(root) {}

	std::unique_ptr<ja2::fileio::File> open(std::string_view resource) override
	{
		return store_.openRead(resource);
	}

	std::unique_ptr<ja2::fileio::File> openFromProfile(
		std::string_view resource, std::string_view) override
	{
		return open(resource);
	}

	std::vector<ResourceVersion> openAll(std::string_view resource) override
	{
		std::vector<ResourceVersion> result;
		result.push_back({"test", open(resource)});
		return result;
	}

	bool exists(std::string_view resource) const override
	{
		return store_.exists(resource);
	}

	std::vector<ja2::fileio::DirectoryEntry> list(std::string_view pattern) const override
	{
		return store_.list(pattern);
	}

	PhysicalWritableStore& writable() { return store_; }

private:
	PhysicalWritableStore store_;
};

void testModesAndIo(PhysicalWritableStore& store)
{
	requireError(ErrorCode::notFound, [&] { store.openRead("missing.dat"); }, "read missing file");

	{
		auto file = store.create("modes.dat");
		file->writeExact(nullptr, 0);
		file->writeExact("abc", 3);
		require(file->position() == 3, "write position");
	}
	require(readAll(store, "modes.dat") == "abc", "created file contents");

	{
		auto file = store.openReadWrite("modes.dat");
		file->seek(1, SeekOrigin::begin);
		file->writeExact("Z", 1);
		file->flush();
	}
	require(readAll(store, "modes.dat") == "aZc", "read/write mode");

	{
		auto file = store.create("modes.dat");
		file->writeExact("x", 1);
	}
	require(readAll(store, "modes.dat") == "x", "create truncates");
	requireError(ErrorCode::notFound, [&] { store.openReadWrite("still-missing.dat"); },
		"read/write requires an existing file");
}

void testZeroPartialAndExactReads(PhysicalWritableStore& store)
{
	{
		auto empty = store.create("empty.dat");
		require(empty->size() == 0, "zero-byte file size");
	}
	{
		auto file = store.openRead("empty.dat");
		require(file->read(nullptr, 0) == 0, "zero-byte read");
	}

	{
		auto file = store.create("partial.dat");
		file->writeExact("abc", 3);
	}
	{
		auto file = store.openRead("partial.dat");
		char buffer[8] = {};
		require(file->read(buffer, sizeof(buffer)) == 3, "partial read count");
		require(std::string(buffer, 3) == "abc", "partial read bytes");
		require(file->read(buffer, 1) == 0, "EOF read count");
		file->seek(0, SeekOrigin::begin);
		requireError(ErrorCode::shortRead, [&] { file->readExact(buffer, 4); }, "exact short read");
	}
}

void testSeekTellAndSize(PhysicalWritableStore& store)
{
	auto file = store.create("seek.dat");
	file->writeExact("0123456789", 10);
	require(file->size() == 10, "file size after write");
	require(file->seek(-2, SeekOrigin::end) == 8, "end-relative seek");
	char value = '\0';
	file->readExact(&value, 1);
	require(value == '8' && file->position() == 9, "tell after read");
	require(file->seek(-4, SeekOrigin::current) == 5, "current-relative seek");
	file->readExact(&value, 1);
	require(value == '5', "current-relative read");
	requireError(ErrorCode::io, [&] { file->seek(-20, SeekOrigin::begin); }, "seek before beginning");
}

void testEnumerationAndMetadata(PhysicalWritableStore& store)
{
	const std::string unicodeName = "unicode-\xC3\xA9-\xE6\x96\x87.dat";
	store.create("a.txt")->writeExact("a", 1);
	store.create("b.bin")->writeExact("bb", 2);
	store.create("nested/c.txt")->writeExact("ccc", 3);
	store.create(unicodeName)->writeExact("utf8", 4);
	require(readAll(store, unicodeName) == "utf8", "native Unicode path round trip");

	const auto rootText = store.list("*.txt");
	require(rootText.size() == 1 && rootText[0].name == "a.txt", "root wildcard enumeration");
	require(rootText[0].metadata.size == 1 && !rootText[0].metadata.directory, "entry metadata");
	const auto nestedText = store.list("nested/?.txt");
	require(nestedText.size() == 1 && nestedText[0].name == "c.txt", "nested enumeration");
	store.create("MixedCase.TXT")->writeExact("case", 4);
	const auto mixedCase = store.list("mixedcase.txt");
	require(mixedCase.size() == 1 && mixedCase[0].name == "MixedCase.TXT",
		"wildcard matching is ASCII case-insensitive");
	require(store.list("absent/*.txt").empty(), "missing directory enumeration");

	const auto nestedMetadata = store.metadata("nested");
	require(nestedMetadata.directory && nestedMetadata.size == 0, "directory metadata");
	store.ensureDirectory("tree/empty");
	store.create("tree/a.dat")->writeExact("a", 1);
	store.create("tree/deep/b.dat")->writeExact("bb", 2);
	const auto recursive = store.listRecursive("tree");
	require(recursive.size() == 2 && recursive[0].name == "a.dat" &&
		recursive[1].name == "deep/b.dat", "recursive enumeration returns relative files");
	require(store.metadata("tree/empty").directory, "explicit directory creation");
	const auto modified = store.metadata(unicodeName).modifiedUnixNanoseconds;
	require(modified.has_value(), "modification timestamp is available");
	constexpr std::int64_t january2000Nanoseconds = 946684800LL * 1000000000LL;
	const auto now = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	require(*modified >= january2000Nanoseconds && *modified <= now + 60000000000LL,
		"modification timestamp uses the Unix epoch");
}

void testExclusiveAndRemove(PhysicalWritableStore& store)
{
	store.createExclusive("exclusive.dat")->writeExact("new", 3);
	requireError(ErrorCode::alreadyExists, [&] { store.createExclusive("exclusive.dat"); },
		"exclusive create collision");
	store.remove("exclusive.dat");
	require(!store.exists("exclusive.dat"), "remove file");
	requireError(ErrorCode::notFound, [&] { store.remove("exclusive.dat"); }, "remove missing file");
}

void testTraversalRejection(PhysicalWritableStore& store)
{
	const std::vector<std::string> invalid = {
		"../escape", "folder/../escape", "./local", "/rooted", "//server/share",
		"C:/drive", "C:drive-relative", "name:stream", "\\\\server\\share", "folder\\file",
		"CON", "nul.txt", "LPT1.log", "trailing.", "trailing ", "folder/"
	};
	for (const std::string& name : invalid)
	{
		requireError(ErrorCode::invalidPath, [&] { store.exists(name); }, "invalid path accepted");
	}
}

void testSymlinkEscape(PhysicalWritableStore& store, const fs::path& temporaryRoot)
{
	const fs::path outsidePath = temporaryRoot / "outside";
	PhysicalWritableStore outside(outsidePath);
	outside.create("secret.dat")->writeExact("secret", 6);

	std::error_code error;
	fs::create_directory_symlink(outsidePath, store.root() / "escape-link", error);
	if (error == std::errc::operation_not_permitted || error == std::errc::permission_denied ||
		error == std::errc::not_supported)
	{
		return;
	}
	if (error) throw fs::filesystem_error("create escape symlink", error);
	fs::create_symlink(outsidePath / "secret.dat", store.root() / "escape-file", error);
	if (error) throw fs::filesystem_error("create escape file symlink", error);

	requireError(ErrorCode::invalidPath, [&] { store.exists("escape-link/secret.dat"); },
		"symlink read escape accepted");
	requireError(ErrorCode::invalidPath, [&] { store.openRead("escape-file"); },
		"leaf symlink read escape accepted");
	requireError(ErrorCode::invalidPath, [&] { store.create("escape-link/new.dat"); },
		"symlink create escape accepted");
	requireError(ErrorCode::invalidPath, [&] { store.list("escape-link/*"); },
		"symlink list escape accepted");
	require(!outside.exists("new.dat"), "symlink escape created an outside file");
}

void testReplaceAndSync(PhysicalWritableStore& store)
{
	store.create("source.dat")->writeExact("new", 3);
	store.create("destination.dat")->writeExact("old", 3);
	store.replace("source.dat", "destination.dat");
	require(!store.exists("source.dat"), "replace removes source name");
	require(readAll(store, "destination.dat") == "new", "replace publishes source contents");

	auto file = store.openReadWrite("destination.dat");
	requireError(ErrorCode::unsupported, [&] { file->sync(); }, "portable durable sync");
}

void testRouterOverlayAndRefresh()
{
	TemporaryDirectory temporary;
	PhysicalResourceStore resources(temporary.path() / "resources");
	resources.writable().create("Data/Resource.TXT")->writeExact("resource", 8);
	resources.writable().create("Data/Shared.TXT")->writeExact("catalogue", 9);

	fs::path writableRoot = temporary.path() / "writable-a";
	StoreRouter router(resources, [&] { return writableRoot; }, {"SavedGames"});
	{
		auto file = router.openWrite("Data/shared.txt");
		file->writeExact("overlay", 7);
	}
	require(readAll(*router.openRead("Data/shared.txt")) == "overlay",
		"writable file overlays the matching resource");
	const auto entries = router.list("Data/*.txt");
	require(entries.size() == 2, "router merges writable and resource listings");
	for (const auto& entry : entries)
		require(entry.name.find('/') == std::string::npos && entry.name.find('\\') == std::string::npos,
			"router listing returns leaf names");

	const auto firstStore = router.currentWritableStore();
	std::error_code error;
	const fs::path alias = temporary.path() / "writable-alias";
	fs::create_directory_symlink(writableRoot, alias, error);
	if (!error)
	{
		writableRoot = alias;
		require(router.currentWritableStore() == firstStore,
			"canonical root aliases reuse the writable store");
	}

	writableRoot = temporary.path() / "writable-b";
	const auto secondStore = router.currentWritableStore();
	require(secondStore != firstStore, "writable root refreshes after profile switch");
	firstStore->create("still-live.dat")->writeExact("old", 3);
	require(firstStore->exists("still-live.dat"), "refreshed writable stores keep borrowers alive");
}

void testRouterNormalizesLegacySeparators()
{
	TemporaryDirectory temporary;
	PhysicalResourceStore resources(temporary.path() / "resources");
	StoreRouter router(resources, [&] { return temporary.path() / "writable"; });

	require(StoreRouter::normalizeLogicalPath("Temp\\\\i_A9.DAT") == "Temp/i_A9.DAT",
		"legacy doubled backslashes are collapsed");
	require(StoreRouter::normalizeLogicalPath("Temp//i_A9.DAT") == "Temp/i_A9.DAT",
		"legacy doubled forward slashes are collapsed");
	{
		auto file = router.create("Temp\\\\i_A9.DAT");
		file->writeExact("item", 4);
	}
	require(readAll(*router.openRead("Temp/i_A9.DAT")) == "item",
		"normalized legacy path addresses the created file");
}

void testRouterFallbackWithoutWritableProfile()
{
	TemporaryDirectory temporary;
	PhysicalResourceStore resources(temporary.path() / "resources");
	resources.writable().create("Data/Fallback.TXT")->writeExact("fallback", 8);
	StoreRouter router(resources, []() -> fs::path
	{
		throw Error(ErrorCode::permissionDenied, "no writable profile");
	}, {"SavedGames"});

	require(router.exists("Data/Fallback.TXT"), "resource exists fallback");
	require(readAll(*router.openRead("Data/Fallback.TXT")) == "fallback",
		"resource read fallback");
	const auto entries = router.list("Data/*.TXT");
	require(entries.size() == 1 && entries[0].name == "Fallback.TXT",
		"resource list fallback");
	require(router.metadata("Data/Fallback.TXT").size == 8, "resource metadata fallback");

	requireError(ErrorCode::permissionDenied, [&] { router.openWrite("Data/new.dat"); },
		"write without writable profile");
	requireError(ErrorCode::permissionDenied, [&] { router.openRead("SavedGames/slot.sav"); },
		"exclusive read without writable profile");
	requireError(ErrorCode::permissionDenied, [&] { router.exists("SavedGames/slot.sav"); },
		"exclusive exists without writable profile");
	requireError(ErrorCode::permissionDenied, [&] { router.list("SavedGames/*.sav"); },
		"exclusive list without writable profile");
}
}

int main()
{
	try
	{
		TemporaryDirectory temporaryDirectory;
		PhysicalWritableStore store(temporaryDirectory.path() / "store");
		testModesAndIo(store);
		testZeroPartialAndExactReads(store);
		testSeekTellAndSize(store);
		testEnumerationAndMetadata(store);
		testExclusiveAndRemove(store);
		testTraversalRejection(store);
		testSymlinkEscape(store, temporaryDirectory.path());
		testReplaceAndSync(store);
		testRouterOverlayAndRefresh();
		testRouterNormalizesLegacySeparators();
		testRouterFallbackWithoutWritableProfile();
		std::cout << "portable file I/O tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "portable file I/O tests failed: " << error.what() << '\n';
		return 1;
	}
}
