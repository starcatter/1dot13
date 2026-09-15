#include "StoreRouter.h"

#include "PhysicalWritableStore.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <system_error>
#include <utility>

namespace ja2::fileio
{
namespace
{
namespace fs = std::filesystem;

std::string folded(std::string_view value)
{
	std::string result(value);
	std::transform(result.begin(), result.end(), result.begin(), [](unsigned char character)
	{
		return static_cast<char>(std::tolower(character));
	});
	return result;
}

bool sameLogicalName(std::string_view left, std::string_view right)
{
	return folded(left) == folded(right);
}

bool entryLess(const DirectoryEntry& left, const DirectoryEntry& right)
{
	const std::string leftFolded = folded(left.name);
	const std::string rightFolded = folded(right.name);
	if (leftFolded != rightFolded) return leftFolded < rightFolded;
	return left.name < right.name;
}

std::string leafName(std::string_view name)
{
	const std::size_t slash = name.rfind('/');
	return std::string(slash == std::string_view::npos ? name : name.substr(slash + 1));
}

fs::path normalizedRoot(fs::path root)
{
	if (root.empty())
		throw Error(ErrorCode::invalidPath, "writable-store root is empty");
	std::error_code error;
	fs::path absolute = fs::absolute(std::move(root), error);
	if (error)
	{
		throw Error(ErrorCode::io,
			"resolve writable-store root failed: " + error.message());
	}
	fs::path result = fs::weakly_canonical(absolute, error);
	if (error)
	{
		throw Error(ErrorCode::io,
			"canonicalize writable-store root failed: " + error.message());
	}
	return result;
}

[[noreturn]] void throwInvalidPrefix(std::string_view prefix)
{
	throw Error(ErrorCode::invalidPath,
		"invalid exclusive logical prefix: '" + std::string(prefix) + "'");
}

std::string normalizePrefix(std::string_view input)
{
	std::string prefix = StoreRouter::normalizeLogicalPath(input);
	while (!prefix.empty() && prefix.back() == '/') prefix.pop_back();
	if (prefix.empty() || prefix.front() == '/') throwInvalidPrefix(input);

	std::size_t start = 0;
	while (start < prefix.size())
	{
		const std::size_t slash = prefix.find('/', start);
		const std::string_view component(prefix.data() + start,
			(slash == std::string::npos ? prefix.size() : slash) - start);
		if (component.empty() || component == "." || component == ".." ||
			component.find_first_of(":*?\"<>|") != std::string_view::npos)
		{
			throwInvalidPrefix(input);
		}
		if (slash == std::string::npos) break;
		start = slash + 1;
	}
	return prefix;
}

void normalizeEntryNames(std::vector<DirectoryEntry>& entries)
{
	for (DirectoryEntry& entry : entries)
		entry.name = StoreRouter::normalizeLogicalPath(entry.name);
}
}

StoreRouter::StoreRouter(ResourceStore& resources, WritableRootProvider writableRoot,
	ExclusivePrefixes exclusivePrefixes) :
	resources_(resources),
	writableRoot_(std::move(writableRoot)),
	exclusivePrefixes_(normalizeExclusivePrefixes(std::move(exclusivePrefixes)))
{
	if (!writableRoot_)
		throw Error(ErrorCode::invalidPath, "writable-root provider is empty");
}

StoreRouter::~StoreRouter() = default;

StoreRouter::ExclusivePrefixes StoreRouter::defaultExclusivePrefixes()
{
	return {"Temp", "ShadeTables", "SavedGames", "MP_SavedGames"};
}

std::string StoreRouter::normalizeLogicalPath(std::string_view name)
{
	std::string result;
	result.reserve(name.size());
	bool previousWasSeparator = false;
	for (const char character : name)
	{
		const bool isSeparator = character == '/' || character == '\\';
		if (isSeparator)
		{
			if (!previousWasSeparator) result.push_back('/');
		}
		else
		{
			result.push_back(character);
		}
		previousWasSeparator = isSeparator;
	}
	return result;
}

StoreRouter::ExclusivePrefixes StoreRouter::normalizeExclusivePrefixes(
	ExclusivePrefixes prefixes)
{
	for (std::string& prefix : prefixes) prefix = normalizePrefix(prefix);
	std::sort(prefixes.begin(), prefixes.end(), [](const std::string& left, const std::string& right)
	{
		const std::string leftFolded = folded(left);
		const std::string rightFolded = folded(right);
		if (leftFolded != rightFolded) return leftFolded < rightFolded;
		return left < right;
	});
	prefixes.erase(std::unique(prefixes.begin(), prefixes.end(), [](const std::string& left,
		const std::string& right) { return sameLogicalName(left, right); }), prefixes.end());
	return prefixes;
}

void StoreRouter::setExclusivePrefixes(ExclusivePrefixes prefixes)
{
	exclusivePrefixes_ = normalizeExclusivePrefixes(std::move(prefixes));
}

bool StoreRouter::isExclusive(std::string_view rawName) const
{
	const std::string name = normalizeLogicalPath(rawName);
	for (const std::string& prefix : exclusivePrefixes_)
	{
		if (name.size() < prefix.size()) continue;
		if (!sameLogicalName(std::string_view(name).substr(0, prefix.size()), prefix)) continue;
		if (name.size() == prefix.size() || name[prefix.size()] == '/') return true;
	}
	return false;
}

std::shared_ptr<PhysicalWritableStore> StoreRouter::refreshWritableStore() const
{
	std::lock_guard<std::mutex> lock(writableStoreMutex_);
	const fs::path rootRequest = writableRoot_();
	if (writableStore_ && rootRequest == writableRootRequest_)
		return writableStore_;

	const fs::path requestedRoot = normalizedRoot(rootRequest);
	if (!writableStore_ || writableStore_->root() != requestedRoot)
	{
		writableStore_ = std::make_shared<PhysicalWritableStore>(requestedRoot);
		std::lock_guard<std::mutex> overlayLock(writableOverlayMutex_);
		writableOverrides_.clear();
		writableTombstones_.clear();
	}
	writableRootRequest_ = rootRequest;
	return writableStore_;
}

std::shared_ptr<PhysicalWritableStore> StoreRouter::currentWritableStore() const
{
	return refreshWritableStore();
}

bool StoreRouter::hasWritableOverride(std::string_view name) const
{
	std::lock_guard<std::mutex> lock(writableOverlayMutex_);
	return writableOverrides_.find(folded(normalizeLogicalPath(name))) != writableOverrides_.end();
}

bool StoreRouter::hasWritableTombstone(std::string_view name) const
{
	std::lock_guard<std::mutex> lock(writableOverlayMutex_);
	return writableTombstones_.find(folded(normalizeLogicalPath(name))) != writableTombstones_.end();
}

void StoreRouter::recordWritableOverride(std::string_view name)
{
	const std::string key = folded(normalizeLogicalPath(name));
	std::lock_guard<std::mutex> lock(writableOverlayMutex_);
	writableTombstones_.erase(key);
	writableOverrides_.insert(key);
}

void StoreRouter::recordWritableRemoval(std::string_view name)
{
	const std::string key = folded(normalizeLogicalPath(name));
	std::lock_guard<std::mutex> lock(writableOverlayMutex_);
	writableOverrides_.erase(key);
	writableTombstones_.insert(key);
}

std::unique_ptr<File> StoreRouter::open(std::string_view rawName)
{
	const std::string name = normalizeLogicalPath(rawName);
	std::shared_ptr<PhysicalWritableStore> store;
	try
	{
		store = refreshWritableStore();
	}
	catch (const Error&)
	{
		if (isExclusive(name)) throw;
		return resources_.open(name);
	}
	if (isExclusive(name) || hasWritableOverride(name))
	{
		return store->openRead(name);
	}
	if (hasWritableTombstone(name))
		throw Error(ErrorCode::notFound, "file does not exist: '" + name + "'");
	try
	{
		return resources_.open(name);
	}
	catch (const Error& error)
	{
		if (error.code() != ErrorCode::notFound) throw;
	}
	return store->openRead(name);
}

std::unique_ptr<File> StoreRouter::openRead(std::string_view name)
{
	return open(name);
}

std::unique_ptr<File> StoreRouter::openFromProfile(
	std::string_view rawName, std::string_view profile)
{
	return resources_.openFromProfile(normalizeLogicalPath(rawName), profile);
}

std::unique_ptr<File> StoreRouter::openRead(
	std::string_view name, std::string_view profile)
{
	return openFromProfile(name, profile);
}

std::unique_ptr<File> StoreRouter::openReadFromProfile(
	std::string_view name, std::string_view profile)
{
	return openFromProfile(name, profile);
}

std::vector<ResourceVersion> StoreRouter::openAll(std::string_view rawName)
{
	return resources_.openAll(normalizeLogicalPath(rawName));
}

std::unique_ptr<File> StoreRouter::openWrite(std::string_view rawName)
{
	const std::string name = normalizeLogicalPath(rawName);
	std::shared_ptr<PhysicalWritableStore> store = refreshWritableStore();
	std::unique_ptr<File> file;
	try
	{
		file = store->openReadWrite(name);
	}
	catch (const Error& error)
	{
		if (error.code() != ErrorCode::notFound) throw;
		file = store->create(name);
	}
	recordWritableOverride(name);
	return file;
}

std::unique_ptr<File> StoreRouter::create(std::string_view name)
{
	const std::string normalized = normalizeLogicalPath(name);
	auto file = refreshWritableStore()->create(normalized);
	recordWritableOverride(normalized);
	return file;
}

std::unique_ptr<File> StoreRouter::createExclusive(std::string_view name)
{
	const std::string normalized = normalizeLogicalPath(name);
	auto file = refreshWritableStore()->createExclusive(normalized);
	recordWritableOverride(normalized);
	return file;
}

std::unique_ptr<File> StoreRouter::openReadWrite(std::string_view name)
{
	return openWrite(name);
}

bool StoreRouter::exists(std::string_view rawName) const
{
	const std::string name = normalizeLogicalPath(rawName);
	std::shared_ptr<PhysicalWritableStore> store;
	try
	{
		store = refreshWritableStore();
	}
	catch (const Error&)
	{
		if (isExclusive(name)) throw;
		return resources_.exists(name);
	}
	if (isExclusive(name) || hasWritableOverride(name)) return store->exists(name);
	if (hasWritableTombstone(name)) return false;
	if (resources_.exists(name)) return true;
	return store->exists(name);
}

Metadata StoreRouter::metadata(std::string_view rawName) const
{
	const std::string name = normalizeLogicalPath(rawName);
	std::shared_ptr<PhysicalWritableStore> store;
	try
	{
		store = refreshWritableStore();
	}
	catch (const Error&)
	{
		if (isExclusive(name)) throw;
		store.reset();
	}
	const bool preferWritable = store && (isExclusive(name) || hasWritableOverride(name));
	try
	{
		if (preferWritable) return store->metadata(name);
	}
	catch (const Error& error)
	{
		if (error.code() != ErrorCode::notFound || isExclusive(name)) throw;
	}
	if (hasWritableTombstone(name))
		throw Error(ErrorCode::notFound, "file does not exist: '" + name + "'");

	std::vector<DirectoryEntry> entries = resources_.list(name);
	normalizeEntryNames(entries);
	std::sort(entries.begin(), entries.end(), entryLess);
	const std::string leaf = leafName(name);
	for (const DirectoryEntry& entry : entries)
	{
		if (sameLogicalName(entry.name, name) || sameLogicalName(entry.name, leaf))
			return entry.metadata;
	}

	// Some resource providers cannot enumerate an exact name. Opening still gives
	// portable file metadata and distinguishes a missing resource.
	try
	{
		auto file = resources_.open(name);
		Metadata result;
		result.size = file->size();
		result.readOnly = true;
		return result;
	}
	catch (const Error& error)
	{
		if (error.code() != ErrorCode::notFound || !store || preferWritable) throw;
	}
	return store->metadata(name);
}

std::vector<DirectoryEntry> StoreRouter::list(std::string_view rawPattern) const
{
	const std::string pattern = normalizeLogicalPath(rawPattern);
	std::vector<DirectoryEntry> writable;
	std::shared_ptr<PhysicalWritableStore> store;
	try
	{
		store = refreshWritableStore();
	}
	catch (const Error&)
	{
		if (isExclusive(pattern)) throw;
		std::vector<DirectoryEntry> resource = resources_.list(pattern);
		normalizeEntryNames(resource);
		std::sort(resource.begin(), resource.end(), entryLess);
		return resource;
	}
	writable = store->list(pattern);
	normalizeEntryNames(writable);
	std::sort(writable.begin(), writable.end(), entryLess);
	if (isExclusive(pattern)) return writable;

	std::vector<DirectoryEntry> resource = resources_.list(pattern);
	normalizeEntryNames(resource);
	std::sort(resource.begin(), resource.end(), entryLess);

	std::map<std::string, DirectoryEntry> merged;
	for (DirectoryEntry& entry : resource)
		merged.emplace(folded(entry.name), std::move(entry));
	for (DirectoryEntry& entry : writable)
		merged[folded(entry.name)] = std::move(entry);

	std::vector<DirectoryEntry> result;
	result.reserve(merged.size());
	for (auto& item : merged) result.push_back(std::move(item.second));
	std::sort(result.begin(), result.end(), entryLess);
	return result;
}

std::vector<DirectoryEntry> StoreRouter::listRecursive(std::string_view directory) const
{
	return refreshWritableStore()->listRecursive(normalizeLogicalPath(directory));
}

void StoreRouter::ensureDirectory(std::string_view name)
{
	refreshWritableStore()->ensureDirectory(normalizeLogicalPath(name));
}

void StoreRouter::clearDirectory(std::string_view rawName, bool recursive)
{
	std::string name = normalizeLogicalPath(rawName);
	while (!name.empty() && name.back() == '/') name.pop_back();
	refreshWritableStore()->clearDirectory(name, recursive);
}

void StoreRouter::remove(std::string_view name)
{
	const std::string normalized = normalizeLogicalPath(name);
	refreshWritableStore()->remove(normalized);
	recordWritableRemoval(normalized);
}

void StoreRouter::replace(std::string_view source, std::string_view destination)
{
	const std::string normalizedSource = normalizeLogicalPath(source);
	const std::string normalizedDestination = normalizeLogicalPath(destination);
	refreshWritableStore()->replace(normalizedSource, normalizedDestination);
	recordWritableRemoval(normalizedSource);
	recordWritableOverride(normalizedDestination);
}
}
