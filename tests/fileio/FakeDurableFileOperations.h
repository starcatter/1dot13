#ifndef JA2_TESTS_FILEIO_FAKEDURABLEFILEOPERATIONS_H
#define JA2_TESTS_FILEIO_FAKEDURABLEFILEOPERATIONS_H

#include "fileio/DurableFileOperations.h"
#include "fileio/FileIO.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ja2::fileio::test
{
enum class PublicationPhase
{
	publishJournal,
	backupSave,
	backupSidecar,
	publishSidecar,
	publishSave
};

enum class FailureTiming
{
	beforeReplace,
	afterReplace
};

struct InjectedFailure
{
	PublicationPhase phase;
	FailureTiming timing;
};

class FakeDurableFileOperations final : public DurableFileOperations
{
public:
	explicit FakeDurableFileOperations(WritableStore& store);

	void syncFile(std::string_view name) override;
	void replace(std::string_view source, std::string_view destination) override;
	void syncDirectory(std::string_view directory) override;

	void failAt(InjectedFailure failure) { failure_ = failure; }
	void failFileSyncWithSuffix(std::string suffix) { syncFailureSuffix_ = std::move(suffix); }
	void observeFileSync(std::function<void()> observer) { fileSyncObserver_ = std::move(observer); }

	const std::vector<PublicationPhase>& publications() const { return publications_; }
	std::size_t fileSyncCount() const { return fileSyncCount_; }
	std::size_t directorySyncCount() const { return directorySyncCount_; }

private:
	static std::optional<PublicationPhase> phaseOf(
		std::string_view source, std::string_view destination);
	void maybeFail(PublicationPhase phase, FailureTiming timing);

	WritableStore& store_;
	std::optional<InjectedFailure> failure_;
	std::string syncFailureSuffix_;
	std::function<void()> fileSyncObserver_;
	std::vector<PublicationPhase> publications_;
	std::size_t fileSyncCount_ = 0;
	std::size_t directorySyncCount_ = 0;
};
}

#endif
