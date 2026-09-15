#include "FakeDurableFileOperations.h"

#include "fileio/PhysicalWritableStore.h"
#include "fileio/SaveTransaction.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
namespace fs = std::filesystem;
using ja2::fileio::AbandonedStagePolicy;
using ja2::fileio::PhysicalWritableStore;
using ja2::fileio::SaveTransaction;
using ja2::fileio::StagedBytes;
using ja2::fileio::test::FailureTiming;
using ja2::fileio::test::FakeDurableFileOperations;
using ja2::fileio::test::InjectedFailure;
using ja2::fileio::test::PublicationPhase;

void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory
{
public:
	TemporaryDirectory()
	{
		const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
		for (unsigned int index = 0; index < 100; ++index)
		{
			path_ = fs::temp_directory_path() /
				("ja2-save-transaction-tests-" + std::to_string(stamp) + "-" + std::to_string(index));
			std::error_code error;
			if (fs::create_directory(path_, error)) return;
			if (error && error != std::errc::file_exists)
				throw fs::filesystem_error("create test directory", error);
		}
		throw std::runtime_error("could not create a unique test directory");
	}

	~TemporaryDirectory()
	{
		std::error_code ignored;
		fs::remove_all(path_, ignored);
	}

	const fs::path& path() const { return path_; }

private:
	fs::path path_;
};

void write(PhysicalWritableStore& store, std::string_view name, std::string_view bytes)
{
	auto file = store.create(name);
	file->writeExact(bytes.data(), bytes.size());
}

std::string read(PhysicalWritableStore& store, std::string_view name)
{
	auto file = store.openRead(name);
	std::string result(static_cast<std::size_t>(file->size()), '\0');
	file->readExact(result.data(), result.size());
	return result;
}

std::vector<std::uint8_t> readBytes(PhysicalWritableStore& store, std::string_view name)
{
	auto file = store.openRead(name);
	std::vector<std::uint8_t> result(static_cast<std::size_t>(file->size()));
	file->readExact(result.data(), result.size());
	return result;
}

void writeBytes(PhysicalWritableStore& store, std::string_view name,
	const std::vector<std::uint8_t>& bytes)
{
	auto file = store.create(name);
	file->writeExact(bytes.data(), bytes.size());
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset)
{
	require(bytes.size() - offset >= 4, "test journal has a truncated integer");
	std::uint32_t result = 0;
	for (unsigned int shift = 0; shift < 32; shift += 8)
		result |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
	return result;
}

void writeU32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value)
{
	require(bytes.size() - offset >= 4, "test journal has no checksum field");
	for (unsigned int shift = 0; shift < 32; shift += 8)
		bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
}

std::uint32_t crc32(const std::uint8_t* data, std::size_t size)
{
	std::uint32_t crc = 0xffffffffU;
	for (std::size_t index = 0; index < size; ++index)
	{
		crc ^= data[index];
		for (unsigned int bit = 0; bit < 8; ++bit)
			crc = (crc >> 1U) ^ (0xedb88320U & (0U - (crc & 1U)));
	}
	return ~crc;
}

void refreshFrameChecksum(std::vector<std::uint8_t>& bytes, std::size_t frameOffset)
{
	const std::uint32_t length = readU32(bytes, frameOffset);
	const std::size_t payloadOffset = frameOffset + 4;
	require(length <= bytes.size() - payloadOffset - 4, "test journal frame is truncated");
	writeU32(bytes, payloadOffset + length, crc32(bytes.data() + payloadOffset, length));
}

std::string onlyJournal(PhysicalWritableStore& store)
{
	const std::vector<ja2::fileio::DirectoryEntry> journals = store.list("*.ja2txn-*.journal");
	require(journals.size() == 1, "expected exactly one transaction journal");
	return journals[0].name;
}

void requireNoArtifacts(PhysicalWritableStore& store)
{
	require(store.list("*.ja2txn-*").empty(), "transaction artifacts remain");
}

void prepareOldGeneration(PhysicalWritableStore& store)
{
	write(store, "slot.sav", "old-save");
	write(store, "slot.IPQ", "old-sidecar");
}

void recover(PhysicalWritableStore& store)
{
	FakeDurableFileOperations durable(store);
	SaveTransaction::recoverDirectory(store, durable, "");
}

void testSuccessfulBytesCommit()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	prepareOldGeneration(store);
	FakeDurableFileOperations durable(store);
	SaveTransaction transaction(store, durable, "slot.sav", "slot.IPQ");
	const std::string save = "new-save";
	const std::string sidecar = "new-sidecar";
	transaction.commit(StagedBytes{save.data(), save.size()},
		StagedBytes{sidecar.data(), sidecar.size()});

	require(read(store, "slot.sav") == save, "new save was not published");
	require(read(store, "slot.IPQ") == sidecar, "new sidecar was not published");
	const std::vector<PublicationPhase>& phases = durable.publications();
	require(phases.size() == 5, "unexpected publication count");
	require(phases[0] == PublicationPhase::publishJournal, "journal was not published first");
	require(phases[1] == PublicationPhase::backupSave, "save was not hidden first");
	require(phases[2] == PublicationPhase::backupSidecar, "sidecar backup order");
	require(phases[3] == PublicationPhase::publishSidecar, "sidecar publication order");
	require(phases[4] == PublicationPhase::publishSave, "save was not published last");
	require(durable.fileSyncCount() >= 10, "journal and stages were not durably synced");
	require(durable.directorySyncCount() >= 7, "directory barriers were not issued");
	requireNoArtifacts(store);
}

void testSidecarSerializerRunsFirst()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	FakeDurableFileOperations durable(store);
	SaveTransaction transaction(store, durable, "slot.sav", "slot.sav.IPQ");
	std::vector<std::string> order;
	SaveTransaction::StageWriter saveWriter = [&](ja2::fileio::File& file)
	{
		order.push_back("save");
		file.writeExact("save", 4);
	};
	SaveTransaction::StageWriter sidecarWriter = [&](ja2::fileio::File& file)
	{
		order.push_back("sidecar");
		file.writeExact("sidecar", 7);
	};
	transaction.commit(saveWriter, sidecarWriter);

	require(order == std::vector<std::string>({"sidecar", "save"}),
		"sidecar serializer did not run before save serializer");
}

void testStageWriterReleasesCallbackStateBeforeSync()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	FakeDurableFileOperations durable(store);
	SaveTransaction transaction(store, durable, "slot.sav");
	bool callbackActive = false;
	durable.observeFileSync([&]
	{
		require(!callbackActive, "stage writer state escaped into durable file sync");
	});

	SaveTransaction::StageWriter writer = [&](ja2::fileio::File& file)
	{
		struct CallbackScope
		{
			bool& active;
			~CallbackScope() { active = false; }
		};
		callbackActive = true;
		CallbackScope scope{callbackActive};
		file.writeExact("save", 4);
	};
	transaction.commit(writer);
}

void testCallbackAndExplicitMissingSidecar()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	prepareOldGeneration(store);
	FakeDurableFileOperations durable(store);
	SaveTransaction transaction(store, durable, "slot.sav", "slot.IPQ");
	SaveTransaction::StageWriter writer = [](ja2::fileio::File& file)
	{
		file.writeExact("callback-save", 13);
	};
	transaction.commit(writer);

	require(read(store, "slot.sav") == "callback-save", "callback bytes were not staged");
	require(!store.exists("slot.IPQ"), "explicitly absent sidecar survived");
	requireNoArtifacts(store);
}

void testExplicitMissingSaveNamedSidecar()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	write(store, "slot.sav", "old-save");
	write(store, "slot.sav.IPQ", "stale-sidecar");
	FakeDurableFileOperations durable(store);
	SaveTransaction transaction(store, durable, "slot.sav", "slot.sav.IPQ");
	SaveTransaction::StageWriter writer = [](ja2::fileio::File& file)
	{
		file.writeExact("new-save", 8);
	};
	transaction.commit(writer);

	require(read(store, "slot.sav") == "new-save", "save replacement failed");
	require(!store.exists("slot.sav.IPQ"), "stale save-named sidecar survived");
	requireNoArtifacts(store);
}

void testFailureMatrix()
{
	const std::array<PublicationPhase, 5> phases{{
		PublicationPhase::publishJournal,
		PublicationPhase::backupSave,
		PublicationPhase::backupSidecar,
		PublicationPhase::publishSidecar,
		PublicationPhase::publishSave
	}};
	const std::array<FailureTiming, 2> timings{{
		FailureTiming::beforeReplace,
		FailureTiming::afterReplace
	}};

	for (PublicationPhase phase : phases)
	{
		for (FailureTiming timing : timings)
		{
			TemporaryDirectory temporary;
			PhysicalWritableStore store(temporary.path());
			prepareOldGeneration(store);
			FakeDurableFileOperations durable(store);
			durable.failAt(InjectedFailure{phase, timing});
			SaveTransaction transaction(store, durable, "slot.sav", "slot.IPQ");
			const std::string save = "new-save";
			const std::string sidecar = "new-sidecar";
			bool failed = false;
			try
			{
				transaction.commit(StagedBytes{save.data(), save.size()},
					StagedBytes{sidecar.data(), sidecar.size()});
			}
			catch (const std::runtime_error&)
			{
				failed = true;
			}
			require(failed, "publication failure was not injected");

			recover(store);
			const bool committed = phase == PublicationPhase::publishSave;
			require(read(store, "slot.sav") == (committed ? save : "old-save"),
				"recovery selected the wrong save generation");
			require(read(store, "slot.IPQ") == (committed ? sidecar : "old-sidecar"),
				"recovery exposed a mixed generation");
			requireNoArtifacts(store);
		}
	}
}

void testExplicitMissingSidecarRecovery()
{
	const std::array<FailureTiming, 2> timings{{
		FailureTiming::beforeReplace,
		FailureTiming::afterReplace
	}};
	for (FailureTiming timing : timings)
	{
		TemporaryDirectory temporary;
		PhysicalWritableStore store(temporary.path());
		prepareOldGeneration(store);
		FakeDurableFileOperations durable(store);
		durable.failAt({PublicationPhase::publishSave, timing});
		SaveTransaction transaction(store, durable, "slot.sav", "slot.IPQ");
		const std::string save = "new-save";
		try
		{
			transaction.commit(StagedBytes{save.data(), save.size()});
		}
		catch (const std::runtime_error&)
		{
		}

		recover(store);
		require(read(store, "slot.sav") == save, "commit intent did not publish the save");
		require(!store.exists("slot.IPQ"), "recovery did not preserve explicit sidecar absence");
		requireNoArtifacts(store);
	}
}

void testTornJournalUsesValidPrefix()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	prepareOldGeneration(store);
	FakeDurableFileOperations durable(store);
	durable.failAt({PublicationPhase::publishSave, FailureTiming::beforeReplace});
	SaveTransaction transaction(store, durable, "slot.sav", "slot.IPQ");
	const std::string save = "new-save";
	const std::string sidecar = "new-sidecar";
	try
	{
		transaction.commit(StagedBytes{save.data(), save.size()},
			StagedBytes{sidecar.data(), sidecar.size()});
	}
	catch (const std::runtime_error&)
	{
	}

	const std::vector<ja2::fileio::DirectoryEntry> journals = store.list("*.ja2txn-*.journal");
	require(journals.size() == 1, "failed transaction did not retain one journal");
	const fs::path journalPath = temporary.path() / journals[0].name;
	const std::uintmax_t size = fs::file_size(journalPath);
	require(size > 3, "journal is unexpectedly short");
	fs::resize_file(journalPath, size - 3);

	recover(store);
	require(read(store, "slot.sav") == "old-save", "torn commit intent was accepted");
	require(read(store, "slot.IPQ") == "old-sidecar", "torn journal produced a mixed pair");
	requireNoArtifacts(store);
}

void testRecoveryCanBeRepeatedAfterAnotherCrash()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	prepareOldGeneration(store);
	FakeDurableFileOperations initialFailure(store);
	initialFailure.failAt({PublicationPhase::publishSave, FailureTiming::beforeReplace});
	SaveTransaction transaction(store, initialFailure, "slot.sav", "slot.IPQ");
	const std::string save = "new-save";
	const std::string sidecar = "new-sidecar";
	try
	{
		transaction.commit(StagedBytes{save.data(), save.size()},
			StagedBytes{sidecar.data(), sidecar.size()});
	}
	catch (const std::runtime_error&)
	{
	}

	FakeDurableFileOperations recoveryFailure(store);
	recoveryFailure.failAt({PublicationPhase::publishSave, FailureTiming::afterReplace});
	bool failed = false;
	try
	{
		SaveTransaction::recoverDirectory(store, recoveryFailure, "");
	}
	catch (const std::runtime_error&)
	{
		failed = true;
	}
	require(failed, "second simulated crash did not occur during recovery");

	recover(store);
	require(read(store, "slot.sav") == save, "repeated recovery lost committed save");
	require(read(store, "slot.IPQ") == sidecar, "repeated recovery mixed sidecar generations");
	requireNoArtifacts(store);
}

void testStagingFailuresDoNotJournal()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	prepareOldGeneration(store);
	FakeDurableFileOperations durable(store);
	SaveTransaction transaction(store, durable, "slot.sav", "slot.IPQ");
	SaveTransaction::StageWriter failingWriter = [](ja2::fileio::File& file)
	{
		file.writeExact("partial", 7);
		throw std::runtime_error("serialization failure");
	};
	SaveTransaction::StageWriter sidecarWriter = [](ja2::fileio::File& file)
	{
		file.writeExact("staged-sidecar", 14);
	};
	bool failed = false;
	try
	{
		transaction.commit(failingWriter, sidecarWriter);
	}
	catch (const std::runtime_error&)
	{
		failed = true;
	}
	require(failed, "serialization failure was not returned");
	require(read(store, "slot.sav") == "old-save", "serialization damaged old save");
	require(read(store, "slot.IPQ") == "old-sidecar", "serialization damaged old sidecar");
	requireNoArtifacts(store);

	FakeDurableFileOperations journalSyncFailure(store);
	journalSyncFailure.failFileSyncWithSuffix(".journal.stage");
	SaveTransaction journalSyncTransaction(store, journalSyncFailure, "slot.sav", "slot.IPQ");
	const std::string journalBytes = "new-journal-sync";
	failed = false;
	try
	{
		journalSyncTransaction.commit(StagedBytes{journalBytes.data(), journalBytes.size()});
	}
	catch (const std::runtime_error&)
	{
		failed = true;
	}
	require(failed, "journal stage sync failure was not returned");
	require(read(store, "slot.sav") == "old-save", "journal sync failure damaged old save");
	require(read(store, "slot.IPQ") == "old-sidecar", "journal sync failure damaged old sidecar");
	recover(store);
	requireNoArtifacts(store);

	FakeDurableFileOperations syncFailure(store);
	syncFailure.failFileSyncWithSuffix(".save.stage");
	SaveTransaction syncTransaction(store, syncFailure, "slot.sav", "slot.IPQ");
	const std::string bytes = "new";
	failed = false;
	try
	{
		syncTransaction.commit(StagedBytes{bytes.data(), bytes.size()});
	}
	catch (const std::runtime_error&)
	{
		failed = true;
	}
	require(failed, "stage sync failure was not returned");
	require(read(store, "slot.sav") == "old-save", "sync failure damaged old save");
	requireNoArtifacts(store);
}

void testAbandonedStagePolicy()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	write(store, "slot.sav.ja2txn-orphan.save.stage", "orphan");
	write(store, "slot.sav.ja2txn-orphan.journal.stage", "incomplete journal");
	FakeDurableFileOperations durable(store);
	SaveTransaction::recoverDirectory(store, durable, "", AbandonedStagePolicy::retain);
	require(store.exists("slot.sav.ja2txn-orphan.save.stage"), "retain policy removed orphan");
	require(store.exists("slot.sav.ja2txn-orphan.journal.stage"),
		"retain policy removed pre-publication journal");
	SaveTransaction::recoverDirectory(store, durable, "", AbandonedStagePolicy::remove);
	require(!store.exists("slot.sav.ja2txn-orphan.save.stage"), "remove policy retained orphan");
	require(!store.exists("slot.sav.ja2txn-orphan.journal.stage"),
		"remove policy retained pre-publication journal");
}

void testJournalOrderingValidation()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	prepareOldGeneration(store);
	FakeDurableFileOperations durable(store);
	durable.failAt({PublicationPhase::backupSave, FailureTiming::beforeReplace});
	SaveTransaction transaction(store, durable, "slot.sav", "slot.IPQ");
	const std::string save = "new-save";
	const std::string sidecar = "new-sidecar";
	try
	{
		transaction.commit(StagedBytes{save.data(), save.size()},
			StagedBytes{sidecar.data(), sidecar.size()});
	}
	catch (const std::runtime_error&)
	{
	}

	const std::string journal = onlyJournal(store);
	std::vector<std::uint8_t> bytes = readBytes(store, journal);
	const std::size_t secondFrame = static_cast<std::size_t>(readU32(bytes, 0)) + 8;
	require(readU32(bytes, secondFrame) == 2, "unexpected operation record size");
	bytes[secondFrame + 5] = 1; // backupSidecar before backupSave
	refreshFrameChecksum(bytes, secondFrame);
	writeBytes(store, journal, bytes);

	bool rejected = false;
	try
	{
		recover(store);
	}
	catch (const ja2::fileio::Error&)
	{
		rejected = true;
	}
	require(rejected, "out-of-order journal operation was accepted");
	require(read(store, "slot.sav") == "old-save", "invalid journal changed the save");
	require(read(store, "slot.IPQ") == "old-sidecar", "invalid journal changed the sidecar");
}

void testJournalSidecarStateValidation()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	write(store, "slot.sav", "old-save");
	FakeDurableFileOperations durable(store);
	durable.failAt({PublicationPhase::backupSave, FailureTiming::beforeReplace});
	SaveTransaction transaction(store, durable, "slot.sav");
	const std::string save = "new-save";
	try
	{
		transaction.commit(StagedBytes{save.data(), save.size()});
	}
	catch (const std::runtime_error&)
	{
	}

	const std::string journal = onlyJournal(store);
	std::vector<std::uint8_t> bytes = readBytes(store, journal);
	const std::uint32_t headerLength = readU32(bytes, 0);
	bytes[4 + headerLength - 1] |= 8U; // Claim an empty serialized sidecar target is present.
	refreshFrameChecksum(bytes, 0);
	writeBytes(store, journal, bytes);

	bool rejected = false;
	try
	{
		recover(store);
	}
	catch (const ja2::fileio::Error&)
	{
		rejected = true;
	}
	require(rejected, "empty present sidecar target was accepted");
	require(read(store, "slot.sav") == "old-save", "invalid sidecar state changed the save");
}

void testDurableMoveRequiresAbsentDestination()
{
	TemporaryDirectory temporary;
	PhysicalWritableStore store(temporary.path());
	write(store, "source.dat", "source");
	write(store, "destination.dat", "destination");
	FakeDurableFileOperations durable(store);
	bool rejected = false;
	try
	{
		durable.replace("source.dat", "destination.dat");
	}
	catch (const ja2::fileio::Error& error)
	{
		rejected = error.code() == ja2::fileio::ErrorCode::alreadyExists;
	}
	require(rejected, "durable move replaced an existing destination");
	require(read(store, "source.dat") == "source", "failed durable move removed its source");
	require(read(store, "destination.dat") == "destination",
		"failed durable move changed its destination");
}
}

int main()
{
	try
	{
		testSuccessfulBytesCommit();
		testSidecarSerializerRunsFirst();
		testStageWriterReleasesCallbackStateBeforeSync();
		testCallbackAndExplicitMissingSidecar();
		testExplicitMissingSaveNamedSidecar();
		testFailureMatrix();
		testExplicitMissingSidecarRecovery();
		testTornJournalUsesValidPrefix();
		testRecoveryCanBeRepeatedAfterAnotherCrash();
		testStagingFailuresDoNotJournal();
		testAbandonedStagePolicy();
		testJournalOrderingValidation();
		testJournalSidecarStateValidation();
		testDurableMoveRequiresAbsentDestination();
		std::cout << "save transaction tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "save transaction tests failed: " << error.what() << '\n';
		return 1;
	}
}
