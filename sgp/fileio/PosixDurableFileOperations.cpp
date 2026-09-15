#include "PosixDurableFileOperations.h"

#include "FileIO.h"
#include "PhysicalWritableStore.h"

#include <algorithm>
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __linux__
#include <sys/syscall.h>
#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1U << 0)
#endif
#endif

namespace ja2::fileio
{
namespace
{
namespace fs = std::filesystem;

ErrorCode mapError(int error)
{
	if (error == ENOENT || error == ENOTDIR) return ErrorCode::notFound;
	if (error == EEXIST) return ErrorCode::alreadyExists;
	if (error == EACCES || error == EPERM || error == EROFS) return ErrorCode::permissionDenied;
	return ErrorCode::io;
}

[[noreturn]] void throwPosixError(const char* operation, const fs::path& path, int error)
{
	throw Error(mapError(error), std::string(operation) + " failed for '" + path.u8string() +
		"': " + std::generic_category().message(error));
}

class FileDescriptor
{
public:
	explicit FileDescriptor(int value) : value_(value) {}
	~FileDescriptor() { if (value_ >= 0) ::close(value_); }
	FileDescriptor(const FileDescriptor&) = delete;
	FileDescriptor& operator=(const FileDescriptor&) = delete;
	int get() const { return value_; }

private:
	int value_;
};

int openFlags(int base)
{
#ifdef O_CLOEXEC
	base |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
	base |= O_NOFOLLOW;
#endif
	return base;
}

void syncDescriptor(int descriptor, const fs::path& path, const char* operation)
{
	int result;
	do
	{
		result = ::fsync(descriptor);
	}
	while (result != 0 && errno == EINTR);
	if (result != 0) throwPosixError(operation, path, errno);
}
}

PosixDurableFileOperations::PosixDurableFileOperations(const PhysicalWritableStore& store) :
	root_(store.root())
{
}

fs::path PosixDurableFileOperations::resolve(std::string_view name, bool allowEmpty) const
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

void PosixDurableFileOperations::syncFile(std::string_view name)
{
	const fs::path path = resolve(name, false);
	FileDescriptor descriptor(::open(path.c_str(), openFlags(O_RDONLY)));
	if (descriptor.get() < 0) throwPosixError("open for durable sync", path, errno);

	struct stat status{};
	if (::fstat(descriptor.get(), &status) != 0)
		throwPosixError("inspect durable file", path, errno);
	if (!S_ISREG(status.st_mode))
		throw Error(ErrorCode::invalidPath, "durable file is not a regular file: '" + path.u8string() + "'");
	syncDescriptor(descriptor.get(), path, "durable sync");
}

void PosixDurableFileOperations::replace(
	std::string_view source, std::string_view destination)
{
	const fs::path sourcePath = resolve(source, false);
	const fs::path destinationPath = resolve(destination, false);
	if (sourcePath.parent_path() != destinationPath.parent_path())
		throw Error(ErrorCode::invalidPath, "durable replacement must remain in one directory");

#if defined(__linux__) && defined(SYS_renameat2)
	int result;
	do
	{
		result = static_cast<int>(::syscall(SYS_renameat2, AT_FDCWD, sourcePath.c_str(),
			AT_FDCWD, destinationPath.c_str(), RENAME_NOREPLACE));
	}
	while (result != 0 && errno == EINTR);
	if (result == 0) return;
	if (errno != ENOSYS && errno != EINVAL)
		throwPosixError("durable move", destinationPath, errno);
#endif

	// link/unlink preserves the no-clobber precondition on POSIX systems without
	// renameat2. Save recovery tolerates both names if the process stops between them.
	if (::link(sourcePath.c_str(), destinationPath.c_str()) != 0)
		throwPosixError("durable move", destinationPath, errno);
	if (::unlink(sourcePath.c_str()) == 0) return;

	const int unlinkError = errno;
	if (::unlink(destinationPath.c_str()) != 0)
		throw Error(ErrorCode::io, "durable move and rollback both failed for '" +
			destinationPath.u8string() + "'");
	throwPosixError("remove durable move source", sourcePath, unlinkError);
}

void PosixDurableFileOperations::syncDirectory(std::string_view directory)
{
	const fs::path path = resolve(directory, true);
	int flags = openFlags(O_RDONLY);
#ifdef O_DIRECTORY
	flags |= O_DIRECTORY;
#endif
	FileDescriptor descriptor(::open(path.c_str(), flags));
	if (descriptor.get() < 0) throwPosixError("open directory for sync", path, errno);

	int result;
	do
	{
		result = ::fsync(descriptor.get());
	}
	while (result != 0 && errno == EINTR);
	if (result == 0) return;
	if (errno == EINVAL || errno == ENOTSUP) return;
	throwPosixError("directory sync", path, errno);
}
}
