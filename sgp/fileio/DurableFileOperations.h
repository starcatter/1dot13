#ifndef JA2_FILEIO_DURABLEFILEOPERATIONS_H
#define JA2_FILEIO_DURABLEFILEOPERATIONS_H

#include <string_view>

namespace ja2::fileio
{
// Platform boundary for durability and same-directory atomic moves.
// syncFile is a persistence barrier. syncDirectory requests a metadata barrier,
// but is best effort on platforms/filesystems without a supported primitive.
class DurableFileOperations
{
public:
	virtual ~DurableFileOperations() = default;

	virtual void syncFile(std::string_view name) = 0;
	// source and destination must be siblings and destination must not exist.
	virtual void replace(std::string_view source, std::string_view destination) = 0;
	virtual void syncDirectory(std::string_view directory) = 0;
};
}

#endif
