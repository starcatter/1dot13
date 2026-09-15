#ifndef JA2_FILEIO_FILESERVICES_H
#define JA2_FILEIO_FILESERVICES_H

#include <string>
#include <string_view>
#include <vector>

namespace ja2::fileio
{
class BfVfsResourceStore;
class StoreRouter;

void initializeFileServices(std::vector<std::string> exclusivePrefixes);
void shutdownFileServices();
bool fileServicesInitialized() noexcept;
void popResourceProfile(std::string_view profile);
void replaceWritableResourceProfile(std::string_view profile, std::string_view root);
void refreshWritableStore();

BfVfsResourceStore& resourceStore();
StoreRouter& storeRouter();
}

#endif
