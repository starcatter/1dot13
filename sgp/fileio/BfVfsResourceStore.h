#ifndef JA2_FILEIO_BFVFSRESOURCESTORE_H
#define JA2_FILEIO_BFVFSRESOURCESTORE_H

#include "FileIO.h"

#include <memory>

namespace ja2::fileio
{
struct ResourceProfile
{
	std::string name;
	bool writable = false;
};

class BfVfsFileLease final : public File
{
public:
	~BfVfsFileLease() override;

	std::size_t read(void* destination, std::size_t size) override;
	void readExact(void* destination, std::size_t size) override;
	void writeExact(const void* source, std::size_t size) override;
	std::uint64_t seek(std::int64_t offset, SeekOrigin origin) override;
	std::uint64_t position() const override;
	std::uint64_t size() const override;
	void flush() override;
	void sync() override;

private:
	struct Impl;
	explicit BfVfsFileLease(std::unique_ptr<Impl> impl);

	std::unique_ptr<Impl> impl_;

	friend class BfVfsResourceStore;
};

// Adapts the process-wide, configured bfVFS without taking ownership of it.
// The bfVFS instance and its profiles must outlive this store and every lease.
// Profile removal is coordinated with adapter-owned leases. Leases opened
// directly through bfVFS remain outside this adapter's accounting.
class BfVfsResourceStore final : public ResourceStore
{
public:
	BfVfsResourceStore();
	~BfVfsResourceStore() override;

	BfVfsResourceStore(const BfVfsResourceStore&) = delete;
	BfVfsResourceStore& operator=(const BfVfsResourceStore&) = delete;

	std::unique_ptr<File> open(std::string_view resource) override;
	std::unique_ptr<File> openFromProfile(
		std::string_view resource, std::string_view profile) override;
	std::vector<ResourceVersion> openAll(std::string_view resource) override;
	bool exists(std::string_view resource) const override;
	std::vector<DirectoryEntry> list(std::string_view pattern) const override;
	std::vector<ResourceProfile> profiles() const;
	std::vector<DirectoryEntry> listFromProfile(
		std::string_view pattern, std::string_view profile) const;
	void popProfile(std::string_view profile);
	void replaceWritableProfile(std::string_view profile, std::string_view root);

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};
}

#endif
