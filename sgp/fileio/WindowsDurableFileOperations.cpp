#include "WindowsDurableFileOperations.h"

#include "FileIO.h"
#include "PhysicalWritableStore.h"

#include <algorithm>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace ja2::fileio
{
namespace
{
namespace fs = std::filesystem;

[[noreturn]] void unsupported()
{
	throw Error(ErrorCode::unsupported, "Windows durable file operations require a Windows target");
}

#ifdef _WIN32
ErrorCode mapError(DWORD error)
{
	if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return ErrorCode::notFound;
	if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) return ErrorCode::alreadyExists;
	if (error == ERROR_ACCESS_DENIED || error == ERROR_SHARING_VIOLATION) return ErrorCode::permissionDenied;
	return ErrorCode::io;
}

[[noreturn]] void throwWindowsError(const char* operation, const fs::path& path, DWORD error)
{
	throw Error(mapError(error), std::string(operation) + " failed for '" + path.u8string() +
		"' (Windows error " + std::to_string(error) + ")");
}

class Handle
{
public:
	explicit Handle(HANDLE value) : value_(value) {}
	~Handle() { if (value_ != INVALID_HANDLE_VALUE) CloseHandle(value_); }
	Handle(const Handle&) = delete;
	Handle& operator=(const Handle&) = delete;
	HANDLE get() const { return value_; }

private:
	HANDLE value_;
};
#endif
}

WindowsDurableFileOperations::WindowsDurableFileOperations(const PhysicalWritableStore& store) :
	root_(store.root())
{
}

fs::path WindowsDurableFileOperations::resolve(std::string_view name, bool allowEmpty) const
{
	if (name.empty())
	{
		if (allowEmpty) return root_;
		throw Error(ErrorCode::invalidPath, "empty durable file name");
	}
	if (name.front() == '/' || name.front() == '\\' || name.find('\\') != std::string_view::npos)
		throw Error(ErrorCode::invalidPath, "invalid durable file name");

	const fs::path relative = fs::u8path(name.begin(), name.end());
	if (relative.is_absolute() || relative.has_root_name() || relative.has_root_directory())
		throw Error(ErrorCode::invalidPath, "absolute durable file name");
	for (const fs::path& component : relative)
	{
		if (component == "." || component == "..")
			throw Error(ErrorCode::invalidPath, "unconfined durable file name");
	}

	const fs::path result = (root_ / relative).lexically_normal();
	const auto mismatch = std::mismatch(root_.begin(), root_.end(), result.begin(), result.end());
	if (mismatch.first != root_.end())
		throw Error(ErrorCode::invalidPath, "unconfined durable file name");
	return result;
}

void WindowsDurableFileOperations::syncFile(std::string_view name)
{
#ifdef _WIN32
	const fs::path path = resolve(name, false);
	Handle handle(CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, nullptr));
	if (handle.get() == INVALID_HANDLE_VALUE) throwWindowsError("open for durable sync", path, GetLastError());
	if (!FlushFileBuffers(handle.get())) throwWindowsError("durable sync", path, GetLastError());
#else
	(void)name;
	unsupported();
#endif
}

void WindowsDurableFileOperations::replace(std::string_view source, std::string_view destination)
{
#ifdef _WIN32
	const fs::path sourcePath = resolve(source, false);
	const fs::path destinationPath = resolve(destination, false);
	if (sourcePath.parent_path() != destinationPath.parent_path())
		throw Error(ErrorCode::invalidPath, "durable replacement must remain in one directory");

	// Omitting MOVEFILE_REPLACE_EXISTING makes destination absence an atomic
	// precondition of the move instead of a racy inspection.
	if (!MoveFileExW(sourcePath.c_str(), destinationPath.c_str(), MOVEFILE_WRITE_THROUGH))
	{
		throwWindowsError("durable move", destinationPath, GetLastError());
	}
#else
	(void)source;
	(void)destination;
	unsupported();
#endif
}

void WindowsDurableFileOperations::syncDirectory(std::string_view directory)
{
#ifdef _WIN32
	const fs::path path = resolve(directory, true);
	Handle handle(CreateFileW(path.c_str(), GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS, nullptr));
	if (handle.get() == INVALID_HANDLE_VALUE)
	{
		const DWORD error = GetLastError();
		// Windows does not expose a universally available directory fsync.
		if (error == ERROR_ACCESS_DENIED || error == ERROR_INVALID_FUNCTION) return;
		throwWindowsError("open directory for sync", path, error);
	}
	if (FlushFileBuffers(handle.get())) return;

	const DWORD error = GetLastError();
	// A failed directory flush is explicitly best effort for filesystems that do
	// not support it; MoveFileExW itself uses MOVEFILE_WRITE_THROUGH.
	if (error != ERROR_INVALID_HANDLE && error != ERROR_INVALID_FUNCTION && error != ERROR_ACCESS_DENIED)
		throwWindowsError("directory sync", path, error);
#else
	(void)directory;
	unsupported();
#endif
}
}
