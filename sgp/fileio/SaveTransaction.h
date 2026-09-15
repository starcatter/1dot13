#ifndef JA2_FILEIO_SAVETRANSACTION_H
#define JA2_FILEIO_SAVETRANSACTION_H

#include "FileIO.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace ja2::fileio
{
class DurableFileOperations;

struct StagedBytes
{
	const void* data = nullptr;
	std::size_t size = 0;
};

enum class AbandonedStagePolicy
{
	remove,
	retain
};

class SaveTransaction
{
public:
	using StageWriter = std::function<void(File&)>;

	SaveTransaction(WritableStore& store, DurableFileOperations& durable,
		std::string saveName, std::optional<std::string> sidecarName = std::nullopt);

	void commit(const StageWriter& saveWriter,
		const std::optional<StageWriter>& sidecarWriter = std::nullopt);
	void commit(StagedBytes saveBytes,
		std::optional<StagedBytes> sidecarBytes = std::nullopt);

	static void recoverDirectory(WritableStore& store, DurableFileOperations& durable,
		std::string_view directory,
		AbandonedStagePolicy abandonedPolicy = AbandonedStagePolicy::remove);

private:
	WritableStore& store_;
	DurableFileOperations& durable_;
	std::string saveName_;
	std::optional<std::string> sidecarName_;
};
}

#endif
