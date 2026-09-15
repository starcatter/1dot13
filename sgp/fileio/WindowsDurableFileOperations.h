#ifndef JA2_FILEIO_WINDOWSDURABLEFILEOPERATIONS_H
#define JA2_FILEIO_WINDOWSDURABLEFILEOPERATIONS_H

#include "DurableFileOperations.h"

#include <filesystem>

namespace ja2::fileio
{
class PhysicalWritableStore;

class WindowsDurableFileOperations final : public DurableFileOperations
{
public:
	explicit WindowsDurableFileOperations(const PhysicalWritableStore& store);

	void syncFile(std::string_view name) override;
	void replace(std::string_view source, std::string_view destination) override;
	void syncDirectory(std::string_view directory) override;

private:
	std::filesystem::path resolve(std::string_view name, bool allowEmpty) const;

	std::filesystem::path root_;
};
}

#endif
