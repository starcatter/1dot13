#ifndef JA2_FILEIO_DURABLEFILEOPERATIONSFACTORY_H
#define JA2_FILEIO_DURABLEFILEOPERATIONSFACTORY_H

#include "DurableFileOperations.h"

#include <memory>

namespace ja2::fileio
{
class PhysicalWritableStore;

std::unique_ptr<DurableFileOperations> makeDurableFileOperations(
	const PhysicalWritableStore& store);
}

#endif
