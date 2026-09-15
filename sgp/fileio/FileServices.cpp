#include "FileServices.h"

#include "BfVfsResourceStore.h"
#include "StoreRouter.h"

#include <vfs/Core/vfs.h>
#include <vfs/Core/vfs_profile.h>

#include <filesystem>
#include <memory>
#include <utility>

namespace ja2::fileio
{
namespace
{
std::unique_ptr<BfVfsResourceStore> resources;
std::unique_ptr<StoreRouter> router;

std::filesystem::path currentWritableRoot()
{
	vfs::CProfileStack* profiles = getVFS()->getProfileStack();
	vfs::CVirtualProfile* profile = profiles == nullptr ? nullptr : profiles->getWriteProfile();
	if (profile == nullptr || !profile->cWritable)
	{
		throw Error(ErrorCode::permissionDenied, "bfVFS has no active writable profile");
	}
	const std::string root = profile->cRoot.to_string();
	if (root.empty())
	{
		throw Error(ErrorCode::invalidPath, "active bfVFS writable profile has no physical root");
	}
	return std::filesystem::u8path(root);
}

[[noreturn]] void throwNotInitialized()
{
	throw Error(ErrorCode::io, "portable file services are not initialized");
}
}

void initializeFileServices(std::vector<std::string> exclusivePrefixes)
{
	shutdownFileServices();
	resources = std::make_unique<BfVfsResourceStore>();
	try
	{
		router = std::make_unique<StoreRouter>(*resources, currentWritableRoot,
			std::move(exclusivePrefixes));
	}
	catch (...)
	{
		router.reset();
		resources.reset();
		throw;
	}
}

void shutdownFileServices()
{
	router.reset();
	resources.reset();
}

bool fileServicesInitialized() noexcept
{
	return router != nullptr;
}

void popResourceProfile(std::string_view profile)
{
	if (!resources || !router) throwNotInitialized();
	resources->popProfile(profile);
}

void replaceWritableResourceProfile(std::string_view profile, std::string_view root)
{
	if (!resources || !router) throwNotInitialized();
	resources->replaceWritableProfile(profile, root);
	(void)router->currentWritableStore();
}

void refreshWritableStore()
{
	if (!router) throwNotInitialized();
	(void)router->currentWritableStore();
}

BfVfsResourceStore& resourceStore()
{
	if (!resources) throwNotInitialized();
	return *resources;
}

StoreRouter& storeRouter()
{
	if (!router) throwNotInitialized();
	return *router;
}
}
