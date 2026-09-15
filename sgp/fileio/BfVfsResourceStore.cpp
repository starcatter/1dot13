#include "BfVfsResourceStore.h"

#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_init.h>
#include <vfs/Core/vfs_profile.h>

#include <filesystem>
#include <limits>
#include <map>
#include <mutex>
#include <utility>

namespace ja2::fileio
{
namespace
{
struct BfVfsState
{
	explicit BfVfsState(vfs::CVirtualFileSystem* fileSystem) : fileSystem(fileSystem) {}

	vfs::CVirtualFileSystem* fileSystem;
	std::recursive_mutex mutex;
	std::map<std::string, std::size_t> liveLeases;
};

std::shared_ptr<BfVfsState> sharedBfVfsState()
{
	static std::mutex stateMutex;
	static std::weak_ptr<BfVfsState> weakState;
	std::lock_guard<std::mutex> lock(stateMutex);
	std::shared_ptr<BfVfsState> state = weakState.lock();
	if (!state)
	{
		state = std::make_shared<BfVfsState>(vfs::CVirtualFileSystem::getVFS());
		weakState = state;
	}
	return state;
}

[[noreturn]] void throwInvalidPath(std::string_view kind, std::string_view value)
{
	throw Error(ErrorCode::invalidPath,
		"invalid bfVFS " + std::string(kind) + ": '" + std::string(value) + "'");
}

void validateText(std::string_view kind, std::string_view value)
{
	if (value.empty() || value.find('\0') != std::string_view::npos)
	{
		throwInvalidPath(kind, value);
	}
}

std::string leafName(std::string_view name)
{
	const std::size_t separator = name.find_last_of("/\\");
	return std::string(separator == std::string_view::npos ? name : name.substr(separator + 1));
}

vfs::Path makePath(std::string_view value, std::string_view kind)
{
	validateText(kind, value);
	try
	{
		return vfs::Path(std::string(value));
	}
	catch (const std::exception&)
	{
		throwInvalidPath(kind, value);
	}
}

vfs::String makeProfileName(std::string_view value)
{
	validateText("profile name", value);
	try
	{
		return vfs::String(std::string(value));
	}
	catch (const std::exception&)
	{
		throwInvalidPath("profile name", value);
	}
}

template<typename Function>
decltype(auto) translateBfVfsError(
	const char* operation, const std::string& subject, Function&& function)
{
	try
	{
		return std::forward<Function>(function)();
	}
	catch (const Error&)
	{
		throw;
	}
	catch (const std::exception& error)
	{
		throw Error(ErrorCode::io,
			std::string("bfVFS ") + operation + " failed for '" + subject + "': " + error.what());
	}
	catch (...)
	{
		throw Error(ErrorCode::io,
			std::string("bfVFS ") + operation + " failed for '" + subject + "'");
	}
}

std::string profileForFile(
	vfs::CVirtualFileSystem& fileSystem, const vfs::Path& path, vfs::IBaseFile* file)
{
	vfs::CProfileStack::Iterator profiles = fileSystem.getProfileStack()->begin();
	for (; !profiles.end(); profiles.next())
	{
		vfs::CVirtualProfile* profile = profiles.value();
		if (profile != nullptr && profile->getFile(path) == file)
		{
			return profile->cName.utf8();
		}
	}
	return {};
}

vfs::IBaseFile::ESeekDir seekDirection(SeekOrigin origin)
{
	switch (origin)
	{
		case SeekOrigin::begin: return vfs::IBaseFile::SD_BEGIN;
		case SeekOrigin::current: return vfs::IBaseFile::SD_CURRENT;
		case SeekOrigin::end: return vfs::IBaseFile::SD_END;
	}
	throw Error(ErrorCode::unsupported, "unsupported seek origin");
}
}

struct BfVfsFileLease::Impl
{
	Impl(std::shared_ptr<BfVfsState> state, vfs::tReadableFile* file,
		std::string resource, std::string profile) :
		state(std::move(state)), file(file), resource(std::move(resource)), profile(std::move(profile))
	{
		const bool wasOpen = this->file->isOpenRead();
		auto insertion = this->state->liveLeases.emplace(this->profile, 0);
		try
		{
			if (!this->file->openRead())
			{
				throw Error(ErrorCode::io, "bfVFS could not open resource '" + this->resource + "'");
			}
			++insertion.first->second;
			active = true;
		}
		catch (...)
		{
			if (insertion.second) this->state->liveLeases.erase(insertion.first);
			if (!wasOpen && this->file->isOpenRead())
			{
				try { this->file->close(); } catch (...) {}
			}
			throw;
		}
	}

	~Impl()
	{
		if (!active) return;
		std::lock_guard<std::recursive_mutex> lock(state->mutex);
		try
		{
			file->close();
		}
		catch (...)
		{
			// Destructors cannot report bfVFS close failures.
		}

		auto lease = state->liveLeases.find(profile);
		if (lease != state->liveLeases.end() && --lease->second == 0)
		{
			state->liveLeases.erase(lease);
		}
	}

	std::shared_ptr<BfVfsState> state;
	vfs::tReadableFile* file;
	std::string resource;
	std::string profile;
	bool active = false;
};

BfVfsFileLease::BfVfsFileLease(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

BfVfsFileLease::~BfVfsFileLease() = default;

std::size_t BfVfsFileLease::read(void* destination, std::size_t requested)
{
	if (requested == 0) return 0;
	if (destination == nullptr) throw Error(ErrorCode::io, "null bfVFS read destination");

	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("read", impl_->resource, [&]() {
		return static_cast<std::size_t>(
			impl_->file->read(static_cast<vfs::Byte*>(destination), requested));
	});
}

void BfVfsFileLease::readExact(void* destination, std::size_t requested)
{
	if (requested == 0) return;
	if (destination == nullptr) throw Error(ErrorCode::io, "null bfVFS read destination");

	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	translateBfVfsError("read", impl_->resource, [&]() {
		auto* output = static_cast<vfs::Byte*>(destination);
		std::size_t total = 0;
		while (total < requested)
		{
			const std::size_t count = impl_->file->read(output + total, requested - total);
			if (count == 0) break;
			total += count;
		}
		if (total != requested)
		{
			throw Error(ErrorCode::shortRead, "short read from bfVFS resource '" + impl_->resource + "'");
		}
	});
}

void BfVfsFileLease::writeExact(const void*, std::size_t)
{
	throw Error(ErrorCode::permissionDenied, "bfVFS resource leases are read-only");
}

std::uint64_t BfVfsFileLease::seek(std::int64_t offset, SeekOrigin origin)
{
	if (offset < static_cast<std::int64_t>((std::numeric_limits<vfs::offset_t>::min)()) ||
		offset > static_cast<std::int64_t>((std::numeric_limits<vfs::offset_t>::max)()))
	{
		throw Error(ErrorCode::unsupported, "bfVFS seek offset is not representable");
	}

	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("seek", impl_->resource, [&]() -> std::uint64_t {
		impl_->file->setReadPosition(static_cast<vfs::offset_t>(offset), seekDirection(origin));
		return static_cast<std::uint64_t>(impl_->file->getReadPosition());
	});
}

std::uint64_t BfVfsFileLease::position() const
{
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("tell", impl_->resource, [&]() {
		return static_cast<std::uint64_t>(impl_->file->getReadPosition());
	});
}

std::uint64_t BfVfsFileLease::size() const
{
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("size", impl_->resource, [&]() {
		return static_cast<std::uint64_t>(impl_->file->getSize());
	});
}

void BfVfsFileLease::flush()
{
}

void BfVfsFileLease::sync()
{
	throw Error(ErrorCode::unsupported, "cannot sync a read-only bfVFS resource lease");
}

struct BfVfsResourceStore::Impl
{
	Impl() : state(sharedBfVfsState()) {}

	std::unique_ptr<File> lease(
		vfs::tReadableFile* file, const std::string& resource, std::string profile) const
	{
		auto leaseImpl = std::make_unique<BfVfsFileLease::Impl>(state, file, resource, std::move(profile));
		return std::unique_ptr<File>(new BfVfsFileLease(std::move(leaseImpl)));
	}

	std::shared_ptr<BfVfsState> state;
};

BfVfsResourceStore::BfVfsResourceStore() : impl_(std::make_unique<Impl>()) {}

BfVfsResourceStore::~BfVfsResourceStore() = default;

void BfVfsResourceStore::popProfile(std::string_view profile)
{
	const std::string profileName(profile);
	const vfs::String bfProfile = makeProfileName(profile);
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	translateBfVfsError("profile removal", profileName, [&]() {
		vfs::CProfileStack* profiles = impl_->state->fileSystem->getProfileStack();
		vfs::CVirtualProfile* selectedProfile = profiles->getProfile(bfProfile);
		if (selectedProfile == nullptr)
		{
			throw Error(ErrorCode::notFound, "bfVFS profile not found: '" + profileName + "'");
		}
		if (selectedProfile != profiles->topProfile())
		{
			throw Error(ErrorCode::unsupported,
				"cannot remove bfVFS profile '" + profileName + "': it is not the top profile");
		}

		const std::string selectedName = selectedProfile->cName.utf8();
		const auto leases = impl_->state->liveLeases.find(selectedName);
		if (leases != impl_->state->liveLeases.end() && leases->second != 0)
		{
			throw Error(ErrorCode::io, "cannot remove bfVFS profile '" + selectedName + "': " +
				std::to_string(leases->second) + " adapter lease(s) are still live");
		}
		if (!profiles->popProfile())
		{
			throw Error(ErrorCode::io, "could not remove bfVFS profile '" + selectedName + "'");
		}
	});
}

void BfVfsResourceStore::replaceWritableProfile(
	std::string_view profile, std::string_view root)
{
	const std::string profileName(profile);
	const std::string rootName(root);
	if (rootName.empty()) throw Error(ErrorCode::invalidPath, "writable profile root is empty");
	std::error_code directoryError;
	std::filesystem::create_directories(std::filesystem::u8path(rootName), directoryError);
	if (directoryError)
		throw Error(ErrorCode::io, "could not create writable profile root '" + rootName +
			"': " + directoryError.message());
	const vfs::String bfProfile = makeProfileName(profile);
	const vfs::Path bfRoot = makePath(root, "profile root");
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	translateBfVfsError("writable profile replacement", profileName, [&]() {
		std::unique_ptr<vfs::CVirtualProfile> replacement(
			new vfs::CVirtualProfile(bfProfile, bfRoot, true));
		if (!vfs_init::initWriteProfile(*replacement))
			throw Error(ErrorCode::io, "could not initialize writable bfVFS profile '" +
				profileName + "' at '" + rootName + "'");

		vfs::CProfileStack* profiles = impl_->state->fileSystem->getProfileStack();
		vfs::CVirtualProfile* previous = profiles->getProfile(bfProfile);
		if (previous != nullptr)
		{
			if (previous != profiles->topProfile())
				throw Error(ErrorCode::unsupported,
					"cannot replace bfVFS profile '" + profileName + "': it is not the top profile");
			const auto leases = impl_->state->liveLeases.find(previous->cName.utf8());
			if (leases != impl_->state->liveLeases.end() && leases->second != 0)
				throw Error(ErrorCode::io, "cannot replace bfVFS profile '" + profileName +
					"': adapter leases are still live");
			if (!profiles->popProfile())
				throw Error(ErrorCode::io, "could not remove previous bfVFS profile '" +
					profileName + "'");
		}
		profiles->pushProfile(replacement.release());
	});
}

std::unique_ptr<File> BfVfsResourceStore::open(std::string_view resource)
{
	const std::string resourceName(resource);
	const vfs::Path path = makePath(resource, "resource path");
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("open", resourceName, [&]() {
		vfs::tReadableFile* file = impl_->state->fileSystem->getReadFile(path);
		if (file == nullptr)
		{
			throw Error(ErrorCode::notFound, "bfVFS resource not found: '" + resourceName + "'");
		}
		return impl_->lease(file, resourceName,
			profileForFile(*impl_->state->fileSystem, path, file));
	});
}

std::unique_ptr<File> BfVfsResourceStore::openFromProfile(
	std::string_view resource, std::string_view profile)
{
	const std::string resourceName(resource);
	const std::string profileName(profile);
	const vfs::Path path = makePath(resource, "resource path");
	const vfs::String bfProfile = makeProfileName(profile);
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("profile open", resourceName, [&]() {
		vfs::CProfileStack* profiles = impl_->state->fileSystem->getProfileStack();
		vfs::CVirtualProfile* selectedProfile = profiles->getProfile(bfProfile);
		if (selectedProfile == nullptr)
		{
			throw Error(ErrorCode::notFound, "bfVFS profile not found: '" + profileName + "'");
		}

		vfs::tReadableFile* file = impl_->state->fileSystem->getReadFile(path, bfProfile);
		if (file == nullptr)
		{
			throw Error(ErrorCode::notFound,
				"bfVFS resource '" + resourceName + "' not found in profile '" + profileName + "'");
		}
		return impl_->lease(file, resourceName, selectedProfile->cName.utf8());
	});
}

std::vector<ResourceVersion> BfVfsResourceStore::openAll(std::string_view resource)
{
	const std::string resourceName(resource);
	const vfs::Path path = makePath(resource, "resource path");
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("open all", resourceName, [&]() {
		std::vector<ResourceVersion> versions;
		vfs::CProfileStack::Iterator profiles = impl_->state->fileSystem->getProfileStack()->begin();
		for (; !profiles.end(); profiles.next())
		{
			vfs::CVirtualProfile* profile = profiles.value();
			if (profile == nullptr) continue;
			vfs::tReadableFile* file = impl_->state->fileSystem->getReadFile(path, profile->cName);
			if (file == nullptr) continue;

			std::string profileName = profile->cName.utf8();
			versions.push_back({profileName, impl_->lease(file, resourceName, profileName)});
		}
		return versions;
	});
}

bool BfVfsResourceStore::exists(std::string_view resource) const
{
	const std::string resourceName(resource);
	const vfs::Path path = makePath(resource, "resource path");
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("existence check", resourceName,
		[&]() { return impl_->state->fileSystem->fileExists(path); });
}

std::vector<DirectoryEntry> BfVfsResourceStore::list(std::string_view pattern) const
{
	const std::string patternName(pattern);
	const vfs::Path path = makePath(pattern, "resource pattern");
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("list", patternName, [&]() {
		std::vector<DirectoryEntry> entries;
		vfs::CVirtualFileSystem::Iterator files = impl_->state->fileSystem->begin(path);
		for (; !files.end(); files.next())
		{
			vfs::tReadableFile* file = files.value();
			if (file == nullptr) continue;

			Metadata metadata;
			metadata.size = static_cast<std::uint64_t>(file->getSize());
			metadata.directory = false;
			metadata.readOnly = !file->implementsWritable();
			entries.push_back({leafName(file->getName().to_string()), metadata});
		}
		return entries;
	});
}

std::vector<ResourceProfile> BfVfsResourceStore::profiles() const
{
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("profile list", "profiles", [&]() {
		std::vector<ResourceProfile> result;
		vfs::CProfileStack::Iterator profiles = impl_->state->fileSystem->getProfileStack()->begin();
		for (; !profiles.end(); profiles.next())
		{
			vfs::CVirtualProfile* profile = profiles.value();
			if (profile != nullptr)
				result.push_back({profile->cName.utf8(), profile->cWritable});
		}
		return result;
	});
}

std::vector<DirectoryEntry> BfVfsResourceStore::listFromProfile(
	std::string_view pattern, std::string_view profile) const
{
	const std::string patternName(pattern);
	const std::string profileName(profile);
	const vfs::Path path = makePath(pattern, "resource pattern");
	const vfs::String bfProfile = makeProfileName(profile);
	std::lock_guard<std::recursive_mutex> lock(impl_->state->mutex);
	return translateBfVfsError("profile list", patternName, [&]() {
		vfs::CVirtualProfile* selected =
			impl_->state->fileSystem->getProfileStack()->getProfile(bfProfile);
		if (selected == nullptr)
			throw Error(ErrorCode::notFound, "bfVFS profile not found: '" + profileName + "'");

		std::vector<DirectoryEntry> entries;
		vfs::CVirtualProfile::FileIterator files = selected->files(path);
		for (; !files.end(); files.next())
		{
			vfs::IBaseFile* file = files.value();
			if (file == nullptr) continue;
			Metadata metadata;
			metadata.size = static_cast<std::uint64_t>(file->getSize());
			metadata.directory = false;
			metadata.readOnly = !file->implementsWritable();
			entries.push_back({leafName(file->getName().to_string()), metadata});
		}
		return entries;
	});
}
}
