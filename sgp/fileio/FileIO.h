#ifndef JA2_FILEIO_FILEIO_H
#define JA2_FILEIO_FILEIO_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ja2::fileio
{
enum class ErrorCode
{
	notFound,
	alreadyExists,
	permissionDenied,
	invalidPath,
	shortRead,
	shortWrite,
	unsupported,
	io
};

class Error : public std::runtime_error
{
public:
	Error(ErrorCode code, const std::string& message) : std::runtime_error(message), code_(code) {}

	ErrorCode code() const noexcept { return code_; }

private:
	ErrorCode code_;
};

enum class SeekOrigin
{
	begin,
	current,
	end
};

struct Metadata
{
	std::uint64_t size = 0;
	std::optional<std::int64_t> modifiedUnixNanoseconds;
	bool directory = false;
	bool readOnly = false;
};

struct DirectoryEntry
{
	// The leaf name relative to the directory selected by list(); never a path.
	std::string name;
	Metadata metadata;
};

class File
{
public:
	File() = default;
	File(const File&) = delete;
	File& operator=(const File&) = delete;
	virtual ~File() = default;

	virtual std::size_t read(void* destination, std::size_t size) = 0;
	virtual void readExact(void* destination, std::size_t size) = 0;
	virtual void writeExact(const void* source, std::size_t size) = 0;
	virtual std::uint64_t seek(std::int64_t offset, SeekOrigin origin) = 0;
	virtual std::uint64_t position() const = 0;
	virtual std::uint64_t size() const = 0;
	virtual void flush() = 0;
	virtual void sync() = 0;
};

struct ResourceVersion
{
	std::string profile;
	std::unique_ptr<File> file;
};

class ResourceStore
{
public:
	virtual ~ResourceStore() = default;
	virtual std::unique_ptr<File> open(std::string_view resource) = 0;
	virtual std::unique_ptr<File> openFromProfile(
		std::string_view resource, std::string_view profile) = 0;
	virtual std::vector<ResourceVersion> openAll(std::string_view resource) = 0;
	virtual bool exists(std::string_view resource) const = 0;
	virtual std::vector<DirectoryEntry> list(std::string_view pattern) const = 0;
};

class WritableStore
{
public:
	virtual ~WritableStore() = default;
	virtual std::unique_ptr<File> openRead(std::string_view name) = 0;
	virtual std::unique_ptr<File> create(std::string_view name) = 0;
	virtual std::unique_ptr<File> createExclusive(std::string_view name) = 0;
	virtual std::unique_ptr<File> openReadWrite(std::string_view name) = 0;
	virtual bool exists(std::string_view name) const = 0;
	virtual Metadata metadata(std::string_view name) const = 0;
	virtual std::vector<DirectoryEntry> list(std::string_view pattern) const = 0;
	virtual std::vector<DirectoryEntry> listRecursive(std::string_view directory) const = 0;
	virtual void ensureDirectory(std::string_view name) = 0;
	virtual void remove(std::string_view name) = 0;
	virtual void replace(std::string_view source, std::string_view destination) = 0;
};
}

#endif
