#include "PhysicalWritableStore.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <fstream>
#include <limits>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace ja2::fileio
{
namespace
{
namespace fs = std::filesystem;

ErrorCode errorCode(const std::error_code& error)
{
	if (error == std::errc::no_such_file_or_directory) return ErrorCode::notFound;
	if (error == std::errc::file_exists) return ErrorCode::alreadyExists;
	if (error == std::errc::permission_denied) return ErrorCode::permissionDenied;
	return ErrorCode::io;
}

std::string displayPath(const fs::path& path)
{
	return path.u8string();
}

[[noreturn]] void throwFilesystemError(
	const char* operation, const fs::path& path, const std::error_code& error)
{
	throw Error(errorCode(error),
		std::string(operation) + " failed for '" + displayPath(path) + "': " + error.message());
}

[[noreturn]] void throwInvalidPath(std::string_view name)
{
	throw Error(ErrorCode::invalidPath, "invalid writable-store path: '" + std::string(name) + "'");
}

bool isReservedDeviceName(std::string component)
{
	const auto dot = component.find('.');
	component.resize(dot == std::string::npos ? component.size() : dot);
	std::transform(component.begin(), component.end(), component.begin(),
		[](unsigned char value) { return static_cast<char>(std::toupper(value)); });

	if (component == "CON" || component == "PRN" || component == "AUX" ||
		component == "NUL" || component == "CLOCK$")
	{
		return true;
	}
	return component.size() == 4 &&
		(component.compare(0, 3, "COM") == 0 || component.compare(0, 3, "LPT") == 0) &&
		component[3] >= '1' && component[3] <= '9';
}

void validateName(std::string_view name, bool allowLeafWildcards)
{
	if (name.empty() || name.front() == '/' || name.front() == '\\') throwInvalidPath(name);

	std::size_t componentStart = 0;
	while (componentStart < name.size())
	{
		const std::size_t slash = name.find('/', componentStart);
		const bool leaf = slash == std::string_view::npos;
		const std::string_view component = name.substr(
			componentStart, leaf ? name.size() - componentStart : slash - componentStart);
		if (component.empty() || component == "." || component == ".." ||
			component.back() == '.' || component.back() == ' ')
		{
			throwInvalidPath(name);
		}

		for (const unsigned char value : component)
		{
			const bool wildcard = value == '*' || value == '?';
			if (value < 32 || value == '\\' || value == ':' || value == '"' ||
				value == '<' || value == '>' || value == '|' || (wildcard && !(allowLeafWildcards && leaf)))
			{
				throwInvalidPath(name);
			}
		}

		if (isReservedDeviceName(std::string(component))) throwInvalidPath(name);
		if (leaf) break;
		componentStart = slash + 1;
		if (componentStart == name.size()) throwInvalidPath(name);
	}
}

[[noreturn]] void throwUnsafeTraversal(std::string_view name, const fs::path& path)
{
	throw Error(ErrorCode::invalidPath,
		"writable-store path '" + std::string(name) + "' traverses symbolic link or reparse point '" +
		displayPath(path) + "'");
}

bool isWithin(const fs::path& root, const fs::path& path)
{
	const auto mismatch = std::mismatch(root.begin(), root.end(), path.begin(), path.end());
	return mismatch.first == root.end();
}

void verifyExistingComponents(
	const fs::path& root, const fs::path& path, std::string_view requestedName)
{
	fs::path current = root;
	const fs::path relative = path.lexically_relative(root);
	if (relative.empty() && path != root) throwInvalidPath(requestedName);

	for (const fs::path& component : relative)
	{
		current /= component;
		std::error_code error;
		const fs::file_status status = fs::symlink_status(current, error);
		if (error)
		{
			if (error == std::errc::no_such_file_or_directory) break;
			throwFilesystemError("inspect path component", current, error);
		}
		if (status.type() == fs::file_type::not_found) break;
		if (fs::is_symlink(status)) throwUnsafeTraversal(requestedName, current);

#ifdef _WIN32
		const DWORD attributes = GetFileAttributesW(current.c_str());
		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			throwFilesystemError("inspect path component", current,
				std::error_code(static_cast<int>(GetLastError()), std::system_category()));
		}
		if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
			throwUnsafeTraversal(requestedName, current);
#endif

		const fs::path canonical = fs::canonical(current, error);
		if (error) throwFilesystemError("canonicalize path component", current, error);
		if (!isWithin(root, canonical)) throwUnsafeTraversal(requestedName, current);
		current = canonical;
	}
}

bool wildcardMatch(std::string_view pattern, std::string_view value)
{
	auto equalAsciiCaseInsensitive = [](char left, char right)
	{
		auto foldAscii = [](unsigned char value)
		{
			return value >= 'A' && value <= 'Z' ?
				static_cast<unsigned char>(value + ('a' - 'A')) : value;
		};
		return foldAscii(static_cast<unsigned char>(left)) ==
			foldAscii(static_cast<unsigned char>(right));
	};
	std::size_t patternIndex = 0;
	std::size_t valueIndex = 0;
	std::size_t star = std::string_view::npos;
	std::size_t retryValue = 0;

	while (valueIndex < value.size())
	{
		if (patternIndex < pattern.size() &&
			(pattern[patternIndex] == '?' ||
				equalAsciiCaseInsensitive(pattern[patternIndex], value[valueIndex])))
		{
			++patternIndex;
			++valueIndex;
		}
		else if (patternIndex < pattern.size() && pattern[patternIndex] == '*')
		{
			star = patternIndex++;
			retryValue = valueIndex;
		}
		else if (star != std::string_view::npos)
		{
			patternIndex = star + 1;
			valueIndex = ++retryValue;
		}
		else
		{
			return false;
		}
	}

	while (patternIndex < pattern.size() && pattern[patternIndex] == '*') ++patternIndex;
	return patternIndex == pattern.size();
}

Metadata readMetadata(const fs::path& path)
{
	std::error_code error;
	const fs::file_status status = fs::status(path, error);
	if (error) throwFilesystemError("metadata", path, error);
	if (!fs::exists(status)) throw Error(ErrorCode::notFound, "path does not exist: '" + displayPath(path) + "'");

	Metadata result;
	result.directory = fs::is_directory(status);
	if (fs::is_regular_file(status))
	{
		result.size = fs::file_size(path, error);
		if (error) throwFilesystemError("file size", path, error);
	}
	const fs::perms writable = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
	result.readOnly = (status.permissions() & writable) == fs::perms::none;
	result.modifiedUnixNanoseconds = std::nullopt;
#ifdef _WIN32
	WIN32_FILE_ATTRIBUTE_DATA attributes{};
	if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes))
	{
		throwFilesystemError("read modification time", path,
			std::error_code(static_cast<int>(GetLastError()), std::system_category()));
	}
	const std::uint64_t windowsTicks =
		(static_cast<std::uint64_t>(attributes.ftLastWriteTime.dwHighDateTime) << 32) |
		attributes.ftLastWriteTime.dwLowDateTime;
	constexpr std::uint64_t unixEpochWindowsTicks = 116444736000000000ULL;
	constexpr std::uint64_t nanosecondsPerTick = 100;
	const std::uint64_t maximumTicks =
		static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)()) / nanosecondsPerTick;
	if (windowsTicks >= unixEpochWindowsTicks)
	{
		const std::uint64_t ticks = windowsTicks - unixEpochWindowsTicks;
		if (ticks > maximumTicks)
			throw Error(ErrorCode::unsupported, "modification time is outside the Unix-nanosecond range");
		result.modifiedUnixNanoseconds = static_cast<std::int64_t>(ticks * nanosecondsPerTick);
	}
	else
	{
		const std::uint64_t ticks = unixEpochWindowsTicks - windowsTicks;
		if (ticks > maximumTicks)
			throw Error(ErrorCode::unsupported, "modification time is outside the Unix-nanosecond range");
		result.modifiedUnixNanoseconds = -static_cast<std::int64_t>(ticks * nanosecondsPerTick);
	}
#else
	struct stat nativeStatus{};
	if (::stat(path.c_str(), &nativeStatus) != 0)
		throwFilesystemError("read modification time", path,
			std::error_code(errno, std::generic_category()));
#ifdef __APPLE__
	const auto seconds = nativeStatus.st_mtimespec.tv_sec;
	const auto nanoseconds = nativeStatus.st_mtimespec.tv_nsec;
#else
	const auto seconds = nativeStatus.st_mtim.tv_sec;
	const auto nanoseconds = nativeStatus.st_mtim.tv_nsec;
#endif
	constexpr std::int64_t nanosecondsPerSecond = 1000000000LL;
	constexpr std::int64_t maximum = (std::numeric_limits<std::int64_t>::max)();
	if (seconds > maximum / nanosecondsPerSecond ||
		(seconds == maximum / nanosecondsPerSecond && nanoseconds > maximum % nanosecondsPerSecond) ||
		seconds < (std::numeric_limits<std::int64_t>::min)() / nanosecondsPerSecond)
	{
		throw Error(ErrorCode::unsupported, "modification time is outside the Unix-nanosecond range");
	}
	result.modifiedUnixNanoseconds =
		static_cast<std::int64_t>(seconds) * nanosecondsPerSecond +
		static_cast<std::int64_t>(nanoseconds);
#endif
	return result;
}

class StandardFile final : public File
{
public:
	StandardFile(fs::path path, std::ios::openmode mode, bool readable, bool writable,
		const char* operation) :
		path_(std::move(path)), readable_(readable), writable_(writable)
	{
		errno = 0;
		stream_.open(path_, mode | std::ios::binary);
		if (!stream_.is_open())
		{
			if (errno != 0)
				throwFilesystemError(operation, path_, std::error_code(errno, std::generic_category()));
			throw Error(ErrorCode::io, std::string(operation) + " failed for '" + displayPath(path_) + "'");
		}
	}

	std::size_t read(void* destination, std::size_t requested) override
	{
		if (!readable_) throw Error(ErrorCode::permissionDenied, "file is not readable");
		if (requested == 0) return 0;
		if (destination == nullptr) throw Error(ErrorCode::io, "null read destination");

		prepareRead();
		auto* output = static_cast<char*>(destination);
		std::size_t total = 0;
		const auto maximum = static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)());
		while (total < requested)
		{
			const std::size_t amount = (std::min)(requested - total, maximum);
			stream_.read(output + total, static_cast<std::streamsize>(amount));
			const std::streamsize count = stream_.gcount();
			total += static_cast<std::size_t>(count);
			position_ += static_cast<std::uint64_t>(count);
			if (static_cast<std::size_t>(count) != amount)
			{
				if (stream_.bad()) throw Error(ErrorCode::io, "read failed for '" + displayPath(path_) + "'");
				stream_.clear();
				break;
			}
		}
		return total;
	}

	void readExact(void* destination, std::size_t requested) override
	{
		const std::size_t count = read(destination, requested);
		if (count != requested)
		{
			throw Error(ErrorCode::shortRead, "short read from '" + displayPath(path_) + "'");
		}
	}

	void writeExact(const void* source, std::size_t requested) override
	{
		if (!writable_) throw Error(ErrorCode::permissionDenied, "file is not writable");
		if (requested == 0) return;
		if (source == nullptr) throw Error(ErrorCode::io, "null write source");

		prepareWrite();
		const auto* input = static_cast<const char*>(source);
		std::size_t total = 0;
		const auto maximum = static_cast<std::size_t>((std::numeric_limits<std::streamsize>::max)());
		while (total < requested)
		{
			const std::size_t amount = (std::min)(requested - total, maximum);
			stream_.write(input + total, static_cast<std::streamsize>(amount));
			if (!stream_)
			{
				stream_.clear();
				throw Error(ErrorCode::shortWrite, "short write to '" + displayPath(path_) + "'");
			}
			total += amount;
			position_ += static_cast<std::uint64_t>(amount);
		}
	}

	std::uint64_t seek(std::int64_t offset, SeekOrigin origin) override
	{
		std::uint64_t base = 0;
		if (origin == SeekOrigin::current) base = position_;
		if (origin == SeekOrigin::end) base = size();

		std::uint64_t target;
		if (offset >= 0)
		{
			const auto increment = static_cast<std::uint64_t>(offset);
			if (increment > (std::numeric_limits<std::uint64_t>::max)() - base)
				throw Error(ErrorCode::io, "seek position overflow");
			target = base + increment;
		}
		else
		{
			const auto decrement = static_cast<std::uint64_t>(-(offset + 1)) + 1;
			if (decrement > base) throw Error(ErrorCode::io, "seek before beginning of file");
			target = base - decrement;
		}

		if (target > static_cast<std::uint64_t>((std::numeric_limits<std::streamoff>::max)()))
			throw Error(ErrorCode::unsupported, "seek position is not representable by this standard library");

		seekStreams(static_cast<std::streamoff>(target));
		position_ = target;
		return position_;
	}

	std::uint64_t position() const override { return position_; }

	std::uint64_t size() const override
	{
		if (writable_)
		{
			stream_.flush();
			if (!stream_) throw Error(ErrorCode::io, "flush failed for '" + displayPath(path_) + "'");
		}
		std::error_code error;
		const std::uintmax_t value = fs::file_size(path_, error);
		if (error) throwFilesystemError("file size", path_, error);
		if (value > (std::numeric_limits<std::uint64_t>::max)())
			throw Error(ErrorCode::unsupported, "file size is not representable");
		return static_cast<std::uint64_t>(value);
	}

	void flush() override
	{
		if (!writable_) return;
		stream_.flush();
		if (!stream_) throw Error(ErrorCode::io, "flush failed for '" + displayPath(path_) + "'");
	}

	void sync() override
	{
		flush();
		throw Error(ErrorCode::unsupported,
			"durable sync requires a platform-specific file backend");
	}

private:
	enum class LastOperation
	{
		none,
		read,
		write
	};

	void prepareRead()
	{
		if (lastOperation_ == LastOperation::read) return;
		if (lastOperation_ == LastOperation::write) stream_.flush();
		stream_.clear();
		stream_.seekg(static_cast<std::streamoff>(position_), std::ios::beg);
		if (!stream_) throw Error(ErrorCode::io, "read seek failed for '" + displayPath(path_) + "'");
		lastOperation_ = LastOperation::read;
	}

	void prepareWrite()
	{
		if (lastOperation_ == LastOperation::write) return;
		stream_.clear();
		stream_.seekp(static_cast<std::streamoff>(position_), std::ios::beg);
		if (!stream_) throw Error(ErrorCode::io, "write seek failed for '" + displayPath(path_) + "'");
		lastOperation_ = LastOperation::write;
	}

	void seekStreams(std::streamoff target)
	{
		if (writable_) stream_.flush();
		stream_.clear();
		if (readable_) stream_.seekg(target, std::ios::beg);
		if (!stream_) throw Error(ErrorCode::io, "seek failed for '" + displayPath(path_) + "'");
		if (writable_) stream_.seekp(target, std::ios::beg);
		if (!stream_) throw Error(ErrorCode::io, "seek failed for '" + displayPath(path_) + "'");
		lastOperation_ = LastOperation::none;
	}

	fs::path path_;
	bool readable_;
	bool writable_;
	std::uint64_t position_ = 0;
	LastOperation lastOperation_ = LastOperation::none;
	mutable std::fstream stream_;
};
}

PhysicalWritableStore::PhysicalWritableStore(fs::path root)
{
	if (root.empty()) throw Error(ErrorCode::invalidPath, "writable-store root is empty");
	std::error_code error;
	root_ = fs::absolute(std::move(root), error).lexically_normal();
	if (error) throwFilesystemError("resolve root", root_, error);
	fs::create_directories(root_, error);
	if (error) throwFilesystemError("create root", root_, error);
	if (!fs::is_directory(root_, error))
	{
		if (error) throwFilesystemError("inspect root", root_, error);
		throw Error(ErrorCode::invalidPath, "writable-store root is not a directory");
	}
	root_ = fs::canonical(root_, error);
	if (error) throwFilesystemError("canonicalize root", root_, error);
}

fs::path PhysicalWritableStore::resolveLexicallyConfined(
	std::string_view name, bool allowLeafWildcards) const
{
	validateName(name, allowLeafWildcards);
	const fs::path relative = fs::u8path(name.begin(), name.end());
	if (relative.has_root_name() || relative.has_root_directory() || relative.is_absolute())
		throwInvalidPath(name);

	const fs::path result = (root_ / relative).lexically_normal();
	const auto mismatch = std::mismatch(root_.begin(), root_.end(), result.begin(), result.end());
	if (mismatch.first != root_.end()) throwInvalidPath(name);
	verifyExistingComponents(root_, result, name);
	return result;
}

void PhysicalWritableStore::createParentDirectories(const fs::path& path) const
{
	std::error_code error;
	fs::create_directories(path.parent_path(), error);
	if (error) throwFilesystemError("create parent directories", path.parent_path(), error);
	// This post-check catches links already present or observed after creation, but
	// std::filesystem cannot make the check and create operation atomic.
	verifyExistingComponents(root_, path.parent_path(), displayPath(path.parent_path()));
}

std::unique_ptr<File> PhysicalWritableStore::openRead(std::string_view name)
{
	const fs::path path = resolveLexicallyConfined(name);
	std::error_code error;
	if (!fs::exists(path, error))
	{
		if (error) throwFilesystemError("inspect", path, error);
		throw Error(ErrorCode::notFound, "file does not exist: '" + displayPath(path) + "'");
	}
	if (!fs::is_regular_file(path, error))
	{
		if (error) throwFilesystemError("inspect", path, error);
		throw Error(ErrorCode::io, "path is not a regular file: '" + displayPath(path) + "'");
	}
	return std::make_unique<StandardFile>(path, std::ios::in, true, false, "open for reading");
}

std::unique_ptr<File> PhysicalWritableStore::create(std::string_view name)
{
	const fs::path path = resolveLexicallyConfined(name);
	createParentDirectories(path);
	return std::make_unique<StandardFile>(
		path, std::ios::in | std::ios::out | std::ios::trunc, true, true, "create file");
}

std::unique_ptr<File> PhysicalWritableStore::createExclusive(std::string_view name)
{
	const fs::path path = resolveLexicallyConfined(name);
	createParentDirectories(path);
	std::error_code error;
	if (fs::exists(path, error)) throw Error(ErrorCode::alreadyExists, "file already exists: '" + displayPath(path) + "'");
	if (error) throwFilesystemError("inspect", path, error);
	// The standard library has no atomic create-new stream mode; platform backends
	// must close the check/open race where hostile concurrent writers are possible.
	return std::make_unique<StandardFile>(
		path, std::ios::in | std::ios::out | std::ios::trunc, true, true, "create exclusive file");
}

std::unique_ptr<File> PhysicalWritableStore::openReadWrite(std::string_view name)
{
	const fs::path path = resolveLexicallyConfined(name);
	std::error_code error;
	if (!fs::is_regular_file(path, error))
	{
		if (error) throwFilesystemError("inspect", path, error);
		if (!fs::exists(path, error))
		{
			if (error) throwFilesystemError("inspect", path, error);
			throw Error(ErrorCode::notFound, "file does not exist: '" + displayPath(path) + "'");
		}
		throw Error(ErrorCode::io, "path is not a regular file: '" + displayPath(path) + "'");
	}
	return std::make_unique<StandardFile>(
		path, std::ios::in | std::ios::out, true, true, "open for reading and writing");
}

bool PhysicalWritableStore::exists(std::string_view name) const
{
	const fs::path path = resolveLexicallyConfined(name);
	std::error_code error;
	const bool result = fs::exists(path, error);
	if (error) throwFilesystemError("inspect", path, error);
	return result;
}

Metadata PhysicalWritableStore::metadata(std::string_view name) const
{
	return readMetadata(resolveLexicallyConfined(name));
}

std::vector<DirectoryEntry> PhysicalWritableStore::list(std::string_view pattern) const
{
	const fs::path resolved = resolveLexicallyConfined(pattern, true);
	const fs::path directory = resolved.parent_path();
	const std::string leafPattern = resolved.filename().u8string();
	std::error_code error;
	if (!fs::exists(directory, error))
	{
		if (error) throwFilesystemError("inspect directory", directory, error);
		return {};
	}

	std::vector<DirectoryEntry> result;
	fs::directory_iterator iterator(directory, error);
	if (error) throwFilesystemError("list directory", directory, error);
	const fs::directory_iterator end;
	while (iterator != end)
	{
		const fs::directory_entry& entry = *iterator;
		const std::string name = entry.path().filename().u8string();
		if (wildcardMatch(leafPattern, name))
		{
			verifyExistingComponents(root_, entry.path(), name);
			result.push_back({name, readMetadata(entry.path())});
		}
		iterator.increment(error);
		if (error) throwFilesystemError("list directory", directory, error);
	}
	std::sort(result.begin(), result.end(),
		[](const DirectoryEntry& left, const DirectoryEntry& right) { return left.name < right.name; });
	return result;
}

std::vector<DirectoryEntry> PhysicalWritableStore::listRecursive(std::string_view name) const
{
	const fs::path directory = name.empty() ? root_ : resolveLexicallyConfined(name);
	std::error_code error;
	if (!fs::exists(directory, error))
	{
		if (error) throwFilesystemError("inspect directory", directory, error);
		return {};
	}
	if (!fs::is_directory(directory, error))
	{
		if (error) throwFilesystemError("inspect directory", directory, error);
		throw Error(ErrorCode::invalidPath, "recursive-list root is not a directory");
	}

	std::vector<DirectoryEntry> result;
	fs::recursive_directory_iterator iterator(directory, error);
	if (error) throwFilesystemError("list directory recursively", directory, error);
	const fs::recursive_directory_iterator end;
	while (iterator != end)
	{
		const fs::directory_entry& entry = *iterator;
		verifyExistingComponents(root_, entry.path(), entry.path().u8string());
		if (entry.is_regular_file(error))
		{
			DirectoryEntry output;
			output.name = entry.path().lexically_relative(directory).generic_u8string();
			output.metadata = readMetadata(entry.path());
			result.push_back(std::move(output));
		}
		else if (error)
		{
			throwFilesystemError("inspect recursive directory entry", entry.path(), error);
		}
		iterator.increment(error);
		if (error) throwFilesystemError("list directory recursively", directory, error);
	}
	std::sort(result.begin(), result.end(),
		[](const DirectoryEntry& left, const DirectoryEntry& right) { return left.name < right.name; });
	return result;
}

void PhysicalWritableStore::ensureDirectory(std::string_view name)
{
	const fs::path directory = resolveLexicallyConfined(name);
	std::error_code error;
	fs::create_directories(directory, error);
	if (error) throwFilesystemError("create directory", directory, error);
	verifyExistingComponents(root_, directory, name);
}

void PhysicalWritableStore::clearDirectory(std::string_view name, bool recursive)
{
	const fs::path directory = resolveLexicallyConfined(name);
	std::error_code error;
	if (!fs::exists(directory, error))
	{
		if (error) throwFilesystemError("inspect directory", directory, error);
		return;
	}
	if (!fs::is_directory(directory, error))
	{
		if (error) throwFilesystemError("inspect directory", directory, error);
		throw Error(ErrorCode::invalidPath, "clear-directory target is not a directory");
	}

	if (recursive)
	{
		fs::recursive_directory_iterator iterator(directory, error);
		if (error) throwFilesystemError("enumerate directory", directory, error);
		const fs::recursive_directory_iterator end;
		while (iterator != end)
		{
			const fs::directory_entry entry = *iterator;
			verifyExistingComponents(root_, entry.path(), entry.path().u8string());
			if (entry.is_regular_file(error))
			{
				if (!fs::remove(entry.path(), error) && !error)
					throw Error(ErrorCode::io, "failed to remove directory file");
				if (error) throwFilesystemError("remove directory file", entry.path(), error);
			}
			else if (error)
			{
				throwFilesystemError("inspect directory entry", entry.path(), error);
			}
			iterator.increment(error);
			if (error) throwFilesystemError("enumerate directory", directory, error);
		}
		return;
	}

	fs::directory_iterator iterator(directory, error);
	if (error) throwFilesystemError("enumerate directory", directory, error);
	const fs::directory_iterator end;
	while (iterator != end)
	{
		const fs::directory_entry entry = *iterator;
		verifyExistingComponents(root_, entry.path(), entry.path().u8string());
		if (entry.is_regular_file(error))
		{
			if (!fs::remove(entry.path(), error) && !error)
				throw Error(ErrorCode::io, "failed to remove directory file");
			if (error) throwFilesystemError("remove directory file", entry.path(), error);
		}
		else if (error)
		{
			throwFilesystemError("inspect directory entry", entry.path(), error);
		}
		iterator.increment(error);
		if (error) throwFilesystemError("enumerate directory", directory, error);
	}
}

void PhysicalWritableStore::remove(std::string_view name)
{
	const fs::path path = resolveLexicallyConfined(name);
	std::error_code error;
	if (!fs::remove(path, error))
	{
		if (error) throwFilesystemError("remove", path, error);
		throw Error(ErrorCode::notFound, "path does not exist: '" + displayPath(path) + "'");
	}
}

void PhysicalWritableStore::replace(std::string_view source, std::string_view destination)
{
	const fs::path sourcePath = resolveLexicallyConfined(source);
	const fs::path destinationPath = resolveLexicallyConfined(destination);
	if (sourcePath == destinationPath) throw Error(ErrorCode::invalidPath, "replace source and destination are identical");
	createParentDirectories(destinationPath);

	std::error_code error;
	if (!fs::exists(sourcePath, error))
	{
		if (error) throwFilesystemError("inspect replace source", sourcePath, error);
		throw Error(ErrorCode::notFound, "replace source does not exist: '" + displayPath(sourcePath) + "'");
	}

	// rename() replaces an existing file on some hosts but not all. Try that best
	// standard operation first, then use a recoverable but non-atomic fallback.
	fs::rename(sourcePath, destinationPath, error);
	if (!error) return;
	std::error_code inspectError;
	if (!fs::exists(destinationPath, inspectError))
	{
		if (inspectError) throwFilesystemError("inspect replace destination", destinationPath, inspectError);
		throwFilesystemError("replace", destinationPath, error);
	}

	fs::path backup;
	for (unsigned int index = 0; index < 1000; ++index)
	{
		backup = destinationPath;
		backup += ".ja2-replace-backup-" + std::to_string(index);
		inspectError.clear();
		if (!fs::exists(backup, inspectError))
		{
			if (inspectError) throwFilesystemError("inspect replace backup", backup, inspectError);
			break;
		}
		backup.clear();
	}
	if (backup.empty()) throw Error(ErrorCode::io, "could not allocate a replace backup name");

	error.clear();
	fs::rename(destinationPath, backup, error);
	if (error) throwFilesystemError("backup replace destination", destinationPath, error);
	fs::rename(sourcePath, destinationPath, error);
	if (error)
	{
		std::error_code rollbackError;
		fs::rename(backup, destinationPath, rollbackError);
		throwFilesystemError("publish replacement", destinationPath, error);
	}
	fs::remove(backup, error);
	if (error) throwFilesystemError("remove replace backup", backup, error);
}
}
