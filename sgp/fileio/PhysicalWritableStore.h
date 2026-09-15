#ifndef JA2_FILEIO_PHYSICALWRITABLESTORE_H
#define JA2_FILEIO_PHYSICALWRITABLESTORE_H

#include "FileIO.h"

#include <filesystem>

namespace ja2::fileio
{
// This standard-library backend rejects existing symbolic-link/reparse-point path
// components and canonicalizes existing parents beneath a canonical root. The
// checks and subsequent filesystem operations are separate, so hostile concurrent
// replacement still has an unavoidable TOCTOU window. Race-free confinement,
// atomic create/replace, and durable sync belong in handle-relative platform backends.
class PhysicalWritableStore final : public WritableStore
{
public:
	explicit PhysicalWritableStore(std::filesystem::path root);

	std::unique_ptr<File> openRead(std::string_view name) override;
	std::unique_ptr<File> create(std::string_view name) override;
	std::unique_ptr<File> createExclusive(std::string_view name) override;
	std::unique_ptr<File> openReadWrite(std::string_view name) override;
	bool exists(std::string_view name) const override;
	Metadata metadata(std::string_view name) const override;
	std::vector<DirectoryEntry> list(std::string_view pattern) const override;
	std::vector<DirectoryEntry> listRecursive(std::string_view directory) const override;
	void ensureDirectory(std::string_view name) override;
	void clearDirectory(std::string_view name, bool recursive);
	void remove(std::string_view name) override;
	void replace(std::string_view source, std::string_view destination) override;

	const std::filesystem::path& root() const noexcept { return root_; }

private:
	std::filesystem::path resolveLexicallyConfined(
		std::string_view name, bool allowLeafWildcards = false) const;
	void createParentDirectories(const std::filesystem::path& path) const;

	std::filesystem::path root_;
};
}

#endif
