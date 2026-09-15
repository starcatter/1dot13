#ifndef JA2_FILEIO_STOREROUTER_H
#define JA2_FILEIO_STOREROUTER_H

#include "FileIO.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace ja2::fileio
{
class PhysicalWritableStore;

// Overlays newly-created profile files on the read-only resource catalogue.
// Logical names always use '/', while the physical store alone translates them
// to native paths and enforces confinement beneath the current writable root.
class StoreRouter final : public ResourceStore
{
public:
	using WritableRootProvider = std::function<std::filesystem::path()>;
	using WritableRootCallback = WritableRootProvider;
	using ExclusivePrefixes = std::vector<std::string>;

	StoreRouter(ResourceStore& resources, WritableRootProvider writableRoot,
		ExclusivePrefixes exclusivePrefixes = defaultExclusivePrefixes());
	~StoreRouter() override;

	StoreRouter(const StoreRouter&) = delete;
	StoreRouter& operator=(const StoreRouter&) = delete;

	std::unique_ptr<File> open(std::string_view name) override;
	std::unique_ptr<File> openFromProfile(
		std::string_view name, std::string_view profile) override;
	std::vector<ResourceVersion> openAll(std::string_view name) override;
	bool exists(std::string_view name) const override;
	std::vector<DirectoryEntry> list(std::string_view pattern) const override;

	std::unique_ptr<File> openRead(std::string_view name);
	std::unique_ptr<File> openRead(
		std::string_view name, std::string_view profile);
	std::unique_ptr<File> openReadFromProfile(
		std::string_view name, std::string_view profile);

	// Legacy write-open creates a missing file but never truncates an existing one.
	std::unique_ptr<File> openWrite(std::string_view name);
	std::unique_ptr<File> create(std::string_view name);
	std::unique_ptr<File> createExclusive(std::string_view name);
	std::unique_ptr<File> openReadWrite(std::string_view name);

	Metadata metadata(std::string_view name) const;
	std::vector<DirectoryEntry> listRecursive(std::string_view directory) const;
	void ensureDirectory(std::string_view name);
	void clearDirectory(std::string_view name, bool recursive);
	void remove(std::string_view name);
	void replace(std::string_view source, std::string_view destination);

	std::shared_ptr<PhysicalWritableStore> currentWritableStore() const;
	std::shared_ptr<PhysicalWritableStore> writableStore() const { return currentWritableStore(); }
	ResourceStore& resourceStore() noexcept { return resources_; }
	const ResourceStore& resourceStore() const noexcept { return resources_; }

	void setExclusivePrefixes(ExclusivePrefixes prefixes);
	const ExclusivePrefixes& exclusivePrefixes() const noexcept { return exclusivePrefixes_; }
	bool isExclusive(std::string_view name) const;

	static ExclusivePrefixes defaultExclusivePrefixes();
	static std::string normalizeLogicalPath(std::string_view name);

private:
	std::shared_ptr<PhysicalWritableStore> refreshWritableStore() const;
	bool hasWritableOverride(std::string_view name) const;
	bool hasWritableTombstone(std::string_view name) const;
	void recordWritableOverride(std::string_view name);
	void recordWritableRemoval(std::string_view name);
	static ExclusivePrefixes normalizeExclusivePrefixes(ExclusivePrefixes prefixes);

	ResourceStore& resources_;
	WritableRootProvider writableRoot_;
	ExclusivePrefixes exclusivePrefixes_;
	mutable std::mutex writableStoreMutex_;
	mutable std::filesystem::path writableRootRequest_;
	mutable std::shared_ptr<PhysicalWritableStore> writableStore_;
	mutable std::mutex writableOverlayMutex_;
	mutable std::unordered_set<std::string> writableOverrides_;
	mutable std::unordered_set<std::string> writableTombstones_;
};
}

#endif
