#include "DurableFileOperationsFactory.h"

#ifdef _WIN32
#include "WindowsDurableFileOperations.h"
#else
#include "PosixDurableFileOperations.h"
#endif

namespace ja2::fileio
{
std::unique_ptr<DurableFileOperations> makeDurableFileOperations(
	const PhysicalWritableStore& store)
{
#ifdef _WIN32
	return std::make_unique<WindowsDurableFileOperations>(store);
#else
	return std::make_unique<PosixDurableFileOperations>(store);
#endif
}
}
