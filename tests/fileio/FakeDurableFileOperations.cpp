#include "FakeDurableFileOperations.h"

#include <stdexcept>

namespace ja2::fileio::test
{
namespace
{
bool endsWith(std::string_view value, std::string_view suffix)
{
	return value.size() >= suffix.size() &&
		value.substr(value.size() - suffix.size()) == suffix;
}
}

FakeDurableFileOperations::FakeDurableFileOperations(WritableStore& store) : store_(store) {}

void FakeDurableFileOperations::syncFile(std::string_view name)
{
	++fileSyncCount_;
	if (fileSyncObserver_) fileSyncObserver_();
	if (!syncFailureSuffix_.empty() && endsWith(name, syncFailureSuffix_))
	{
		syncFailureSuffix_.clear();
		throw std::runtime_error("injected durable file sync failure");
	}
}

std::optional<PublicationPhase> FakeDurableFileOperations::phaseOf(
	std::string_view source, std::string_view destination)
{
	if (endsWith(source, ".journal.stage") && endsWith(destination, ".journal"))
		return PublicationPhase::publishJournal;
	if (endsWith(destination, ".save.backup")) return PublicationPhase::backupSave;
	if (endsWith(destination, ".sidecar.backup")) return PublicationPhase::backupSidecar;
	if (endsWith(source, ".sidecar.stage")) return PublicationPhase::publishSidecar;
	if (endsWith(source, ".save.stage")) return PublicationPhase::publishSave;
	return std::nullopt;
}

void FakeDurableFileOperations::maybeFail(PublicationPhase phase, FailureTiming timing)
{
	if (failure_ && failure_->phase == phase && failure_->timing == timing)
	{
		failure_.reset();
		throw std::runtime_error("injected transaction publication failure");
	}
}

void FakeDurableFileOperations::replace(std::string_view source, std::string_view destination)
{
	const std::optional<PublicationPhase> phase = phaseOf(source, destination);
	if (phase)
	{
		publications_.push_back(*phase);
		maybeFail(*phase, FailureTiming::beforeReplace);
	}
	if (store_.exists(destination))
		throw Error(ErrorCode::alreadyExists, "durable move destination already exists");
	store_.replace(source, destination);
	if (phase) maybeFail(*phase, FailureTiming::afterReplace);
}

void FakeDurableFileOperations::syncDirectory(std::string_view directory)
{
	(void)directory;
	++directorySyncCount_;
}
}
