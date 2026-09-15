#include "SaveTransaction.h"

#include "DurableFileOperations.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace ja2::fileio
{
namespace
{
constexpr std::uint16_t journalVersion = 1;
constexpr std::uint32_t maximumRecordSize = 64 * 1024;
constexpr std::uint64_t maximumJournalSize = 1024 * 1024;
constexpr std::string_view journalMarker = ".ja2txn-";

enum class RecordKind : std::uint8_t
{
	header = 1,
	intent = 2,
	completion = 3
};

enum class Operation : std::uint8_t
{
	backupSave = 0,
	backupSidecar = 1,
	publishSidecar = 2,
	publishSave = 3,
	count = 4
};

struct JournalHeader
{
	std::string transactionId;
	std::string finalSave;
	std::string stagedSave;
	std::string backupSave;
	std::optional<std::string> finalSidecar;
	std::string stagedSidecar;
	std::string backupSidecar;
	bool hadOldSave = false;
	bool hadOldSidecar = false;
	bool hasNewSidecar = false;
};

struct Journal
{
	JournalHeader header;
	std::array<bool, static_cast<std::size_t>(Operation::count)> intents{};
	std::array<bool, static_cast<std::size_t>(Operation::count)> completions{};
};

[[noreturn]] void journalError(const std::string& message)
{
	throw Error(ErrorCode::io, "invalid save transaction journal: " + message);
}

std::string directoryOf(std::string_view name)
{
	const std::size_t slash = name.rfind('/');
	return slash == std::string_view::npos ? std::string() : std::string(name.substr(0, slash));
}

std::string join(std::string_view directory, std::string_view leaf)
{
	if (directory.empty()) return std::string(leaf);
	return std::string(directory) + "/" + std::string(leaf);
}

bool endsWith(std::string_view value, std::string_view suffix)
{
	return value.size() >= suffix.size() &&
		value.substr(value.size() - suffix.size()) == suffix;
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

void appendU16(std::vector<std::uint8_t>& output, std::uint16_t value)
{
	output.push_back(static_cast<std::uint8_t>(value));
	output.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void appendU32(std::vector<std::uint8_t>& output, std::uint32_t value)
{
	for (unsigned int shift = 0; shift < 32; shift += 8)
		output.push_back(static_cast<std::uint8_t>(value >> shift));
}

void appendString(std::vector<std::uint8_t>& output, std::string_view value)
{
	if (value.size() > (std::numeric_limits<std::uint32_t>::max)())
		throw Error(ErrorCode::unsupported, "save transaction name is too long");
	appendU32(output, static_cast<std::uint32_t>(value.size()));
	output.insert(output.end(), value.begin(), value.end());
}

std::uint16_t readU16(const std::vector<std::uint8_t>& input, std::size_t& offset)
{
	if (input.size() - offset < 2) journalError("truncated integer");
	const std::uint16_t value = static_cast<std::uint16_t>(input[offset]) |
		(static_cast<std::uint16_t>(input[offset + 1]) << 8U);
	offset += 2;
	return value;
}

std::uint32_t readU32(const std::vector<std::uint8_t>& input, std::size_t& offset)
{
	if (input.size() - offset < 4) journalError("truncated integer");
	std::uint32_t value = 0;
	for (unsigned int shift = 0; shift < 32; shift += 8)
		value |= static_cast<std::uint32_t>(input[offset++]) << shift;
	return value;
}

std::string readString(const std::vector<std::uint8_t>& input, std::size_t& offset)
{
	const std::uint32_t length = readU32(input, offset);
	if (length > input.size() - offset) journalError("truncated string");
	std::string result(input.begin() + static_cast<std::ptrdiff_t>(offset),
		input.begin() + static_cast<std::ptrdiff_t>(offset + length));
	offset += length;
	return result;
}

std::vector<std::uint8_t> headerRecord(const JournalHeader& header)
{
	std::vector<std::uint8_t> result;
	result.push_back(static_cast<std::uint8_t>(RecordKind::header));
	result.insert(result.end(), {'J', 'A', '2', 'T', 'X', 'N'});
	appendU16(result, journalVersion);
	appendString(result, header.transactionId);
	appendString(result, header.finalSave);
	appendString(result, header.stagedSave);
	appendString(result, header.backupSave);
	appendString(result, header.finalSidecar.value_or(std::string()));
	appendString(result, header.stagedSidecar);
	appendString(result, header.backupSidecar);
	std::uint8_t flags = 0;
	if (header.hadOldSave) flags |= 1U;
	if (header.hadOldSidecar) flags |= 2U;
	if (header.hasNewSidecar) flags |= 4U;
	if (header.finalSidecar) flags |= 8U;
	result.push_back(flags);
	return result;
}

std::vector<std::uint8_t> operationRecord(RecordKind kind, Operation operation)
{
	return {static_cast<std::uint8_t>(kind), static_cast<std::uint8_t>(operation)};
}

std::vector<Operation> expectedOperations(const JournalHeader& header)
{
	std::vector<Operation> result;
	if (header.hadOldSave) result.push_back(Operation::backupSave);
	if (header.hadOldSidecar) result.push_back(Operation::backupSidecar);
	if (header.hasNewSidecar) result.push_back(Operation::publishSidecar);
	result.push_back(Operation::publishSave);
	return result;
}

std::vector<std::uint8_t> frame(const std::vector<std::uint8_t>& payload)
{
	if (payload.empty() || payload.size() > maximumRecordSize)
		throw Error(ErrorCode::unsupported, "save transaction journal record is too large");
	std::vector<std::uint8_t> result;
	result.reserve(payload.size() + 8);
	appendU32(result, static_cast<std::uint32_t>(payload.size()));
	result.insert(result.end(), payload.begin(), payload.end());
	appendU32(result, crc32(payload.data(), payload.size()));
	return result;
}

void writeNewJournal(WritableStore& store, DurableFileOperations& durable,
	std::string_view journalStageName, std::string_view journalName,
	std::string_view directory, const JournalHeader& header)
{
	const std::vector<std::uint8_t> bytes = frame(headerRecord(header));
	{
		auto file = store.createExclusive(journalStageName);
		file->writeExact(bytes.data(), bytes.size());
		file->flush();
	}
	durable.syncFile(journalStageName);
	durable.replace(journalStageName, journalName);
	durable.syncDirectory(directory);
}

void appendJournal(WritableStore& store, DurableFileOperations& durable,
	std::string_view journalName, RecordKind kind, Operation operation)
{
	const std::vector<std::uint8_t> bytes = frame(operationRecord(kind, operation));
	{
		auto file = store.openReadWrite(journalName);
		file->seek(0, SeekOrigin::end);
		file->writeExact(bytes.data(), bytes.size());
		file->flush();
	}
	durable.syncFile(journalName);
}

JournalHeader decodeHeader(const std::vector<std::uint8_t>& payload)
{
	std::size_t offset = 0;
	if (payload.empty() || payload[offset++] != static_cast<std::uint8_t>(RecordKind::header))
		journalError("first record is not a header");
	constexpr std::array<std::uint8_t, 6> magic{{'J', 'A', '2', 'T', 'X', 'N'}};
	if (payload.size() - offset < magic.size() ||
		!std::equal(magic.begin(), magic.end(), payload.begin() + static_cast<std::ptrdiff_t>(offset)))
		journalError("bad magic");
	offset += magic.size();
	if (readU16(payload, offset) != journalVersion) journalError("unsupported version");

	JournalHeader header;
	header.transactionId = readString(payload, offset);
	header.finalSave = readString(payload, offset);
	header.stagedSave = readString(payload, offset);
	header.backupSave = readString(payload, offset);
	const std::string finalSidecar = readString(payload, offset);
	header.stagedSidecar = readString(payload, offset);
	header.backupSidecar = readString(payload, offset);
	if (offset == payload.size()) journalError("missing state flags");
	const std::uint8_t flags = payload[offset++];
	if (offset != payload.size() || (flags & 0xf0U) != 0) journalError("invalid header fields");
	header.hadOldSave = (flags & 1U) != 0;
	header.hadOldSidecar = (flags & 2U) != 0;
	header.hasNewSidecar = (flags & 4U) != 0;
	if ((flags & 8U) != 0)
	{
		if (finalSidecar.empty()) journalError("empty sidecar target");
		header.finalSidecar = finalSidecar;
	}
	else if (!finalSidecar.empty()) journalError("sidecar state mismatch");
	if (header.transactionId.empty() || header.finalSave.empty() || header.stagedSave.empty() ||
		header.backupSave.empty()) journalError("empty required name");
	if (!header.finalSidecar && (header.hadOldSidecar || header.hasNewSidecar))
		journalError("sidecar state without a sidecar target");
	return header;
}

Journal readJournal(WritableStore& store, std::string_view journalName)
{
	auto file = store.openRead(journalName);
	const std::uint64_t size = file->size();
	if (size > maximumJournalSize) journalError("file is too large");
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	file->readExact(bytes.data(), bytes.size());

	Journal result;
	bool haveHeader = false;
	std::vector<Operation> operations;
	std::size_t operationIndex = 0;
	bool awaitingCompletion = false;
	std::size_t offset = 0;
	while (offset < bytes.size())
	{
		if (bytes.size() - offset < 4) break;
		std::size_t lengthOffset = offset;
		const std::uint32_t length = readU32(bytes, lengthOffset);
		if (length == 0 || length > maximumRecordSize)
		{
			if (haveHeader) break;
			journalError("invalid record length");
		}
		const std::size_t frameSize = static_cast<std::size_t>(length) + 8;
		if (frameSize > bytes.size() - offset) break;
		const std::size_t payloadOffset = offset + 4;
		std::size_t checksumOffset = payloadOffset + length;
		const std::uint32_t storedChecksum = readU32(bytes, checksumOffset);
		const std::uint32_t actualChecksum = crc32(bytes.data() + payloadOffset, length);
		if (storedChecksum != actualChecksum)
		{
			if (offset + frameSize == bytes.size()) break;
			journalError("checksum mismatch before final record");
		}

		std::vector<std::uint8_t> payload(bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset),
			bytes.begin() + static_cast<std::ptrdiff_t>(payloadOffset + length));
		if (!haveHeader)
		{
			result.header = decodeHeader(payload);
			operations = expectedOperations(result.header);
			haveHeader = true;
		}
		else
		{
			if (payload.size() != 2) journalError("invalid operation record");
			const auto kind = static_cast<RecordKind>(payload[0]);
			const auto operation = static_cast<Operation>(payload[1]);
			const std::size_t index = static_cast<std::size_t>(operation);
			if (index >= result.intents.size() ||
				(kind != RecordKind::intent && kind != RecordKind::completion))
				journalError("unknown operation record");
			if (kind == RecordKind::intent)
			{
				if (awaitingCompletion || operationIndex >= operations.size() ||
					operation != operations[operationIndex])
					journalError("operation intent is out of order");
				result.intents[index] = true;
				awaitingCompletion = true;
			}
			else
			{
				if (!awaitingCompletion || operationIndex >= operations.size() ||
					operation != operations[operationIndex])
					journalError("operation completion is out of order");
				result.completions[index] = true;
				awaitingCompletion = false;
				++operationIndex;
			}
		}
		offset += frameSize;
	}
	if (!haveHeader) journalError("no complete header");
	return result;
}

void removeIfPresent(WritableStore& store, std::string_view name)
{
	if (!name.empty() && store.exists(name)) store.remove(name);
}

void cleanup(WritableStore& store, DurableFileOperations& durable,
	const JournalHeader& header, std::string_view journalName)
{
	const std::string directory = directoryOf(header.finalSave);
	durable.syncDirectory(directory);
	removeIfPresent(store, header.stagedSave);
	removeIfPresent(store, header.backupSave);
	removeIfPresent(store, header.stagedSidecar);
	removeIfPresent(store, header.backupSidecar);
	durable.syncDirectory(directory);
	removeIfPresent(store, journalName);
	durable.syncDirectory(directory);
}

void replaceAndSync(DurableFileOperations& durable, std::string_view source,
	std::string_view destination, std::string_view directory)
{
	durable.replace(source, destination);
	durable.syncDirectory(directory);
}

void perform(WritableStore& store, DurableFileOperations& durable,
	std::string_view journalName, std::string_view directory, Operation operation,
	std::string_view source, std::string_view destination)
{
	appendJournal(store, durable, journalName, RecordKind::intent, operation);
	replaceAndSync(durable, source, destination, directory);
	appendJournal(store, durable, journalName, RecordKind::completion, operation);
}

std::string uniqueId()
{
	static std::atomic<std::uint64_t> sequence{0};
	const auto ticks = std::chrono::high_resolution_clock::now().time_since_epoch().count();
	return std::to_string(ticks) + "-" + std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
}

JournalHeader allocateNames(WritableStore& store, const std::string& finalSave,
	const std::optional<std::string>& finalSidecar, std::string& journalStageName,
	std::string& journalName)
{
	for (unsigned int attempt = 0; attempt < 100; ++attempt)
	{
		JournalHeader header;
		header.transactionId = uniqueId();
		header.finalSave = finalSave;
		header.finalSidecar = finalSidecar;
		const std::string prefix = finalSave + std::string(journalMarker) + header.transactionId;
		header.stagedSave = prefix + ".save.stage";
		header.backupSave = prefix + ".save.backup";
		header.stagedSidecar = prefix + ".sidecar.stage";
		header.backupSidecar = prefix + ".sidecar.backup";
		journalStageName = prefix + ".journal.stage";
		journalName = prefix + ".journal";
		if (!store.exists(header.stagedSave) && !store.exists(header.backupSave) &&
			!store.exists(header.stagedSidecar) && !store.exists(header.backupSidecar) &&
			!store.exists(journalStageName) && !store.exists(journalName)) return header;
	}
	throw Error(ErrorCode::alreadyExists, "could not allocate unique save transaction names");
}

void stage(WritableStore& store, DurableFileOperations& durable,
	std::string_view name, const SaveTransaction::StageWriter& writer)
{
	{
		auto file = store.createExclusive(name);
		writer(*file);
		file->flush();
	}
	durable.syncFile(name);
}

void ensureBackedUp(WritableStore& store, DurableFileOperations& durable,
	bool hadOld, std::string_view finalName, std::string_view backupName,
	std::string_view directory)
{
	if (hadOld && store.exists(finalName) && !store.exists(backupName))
		replaceAndSync(durable, finalName, backupName, directory);
}

void recoverCommit(WritableStore& store, DurableFileOperations& durable,
	const JournalHeader& header, std::string_view journalName)
{
	const std::string directory = directoryOf(header.finalSave);
	if (header.finalSidecar)
	{
		if (header.hasNewSidecar)
		{
			if (store.exists(header.stagedSidecar))
			{
				ensureBackedUp(store, durable, header.hadOldSidecar, *header.finalSidecar,
					header.backupSidecar, directory);
				replaceAndSync(durable, header.stagedSidecar, *header.finalSidecar, directory);
			}
			else if (!store.exists(*header.finalSidecar))
			{
				journalError("committed sidecar is missing");
			}
		}
		else if (store.exists(*header.finalSidecar))
		{
			ensureBackedUp(store, durable, header.hadOldSidecar, *header.finalSidecar,
				header.backupSidecar, directory);
			removeIfPresent(store, *header.finalSidecar);
			durable.syncDirectory(directory);
		}
	}

	if (store.exists(header.stagedSave))
	{
		ensureBackedUp(store, durable, header.hadOldSave, header.finalSave,
			header.backupSave, directory);
		replaceAndSync(durable, header.stagedSave, header.finalSave, directory);
	}
	else if (!store.exists(header.finalSave))
	{
		journalError("committed save is missing");
	}
	cleanup(store, durable, header, journalName);
}

void recoverRollback(WritableStore& store, DurableFileOperations& durable,
	const JournalHeader& header, std::string_view journalName)
{
	const std::string directory = directoryOf(header.finalSave);
	if (header.hadOldSave && !store.exists(header.backupSave))
	{
		if (!store.exists(header.finalSave)) journalError("old save and its backup are missing");
		cleanup(store, durable, header, journalName);
		return;
	}

	if (header.finalSidecar)
	{
		if (store.exists(header.backupSidecar))
		{
			removeIfPresent(store, *header.finalSidecar);
			durable.syncDirectory(directory);
			replaceAndSync(durable, header.backupSidecar, *header.finalSidecar, directory);
		}
		else if (header.hadOldSidecar)
		{
			if (!store.exists(*header.finalSidecar)) journalError("old sidecar and its backup are missing");
		}
		else if (store.exists(*header.finalSidecar))
		{
			store.remove(*header.finalSidecar);
			durable.syncDirectory(directory);
		}
	}

	if (store.exists(header.backupSave))
	{
		removeIfPresent(store, header.finalSave);
		durable.syncDirectory(directory);
		replaceAndSync(durable, header.backupSave, header.finalSave, directory);
	}
	else if (!header.hadOldSave && store.exists(header.finalSave))
	{
		store.remove(header.finalSave);
		durable.syncDirectory(directory);
	}
	cleanup(store, durable, header, journalName);
}

void recoverOne(WritableStore& store, DurableFileOperations& durable, std::string_view journalName)
{
	const Journal journal = readJournal(store, journalName);
	const JournalHeader& header = journal.header;
	const std::string directory = directoryOf(header.finalSave);
	const std::string prefix = header.finalSave + std::string(journalMarker) + header.transactionId;
	if (directoryOf(journalName) != directory || directoryOf(header.stagedSave) != directory ||
		directoryOf(header.backupSave) != directory ||
		(header.finalSidecar && directoryOf(*header.finalSidecar) != directory) ||
		directoryOf(header.stagedSidecar) != directory || directoryOf(header.backupSidecar) != directory)
		journalError("transaction files are not siblings");
	if (journalName != prefix + ".journal" || header.stagedSave != prefix + ".save.stage" ||
		header.backupSave != prefix + ".save.backup" ||
		header.stagedSidecar != prefix + ".sidecar.stage" ||
		header.backupSidecar != prefix + ".sidecar.backup")
		journalError("transaction artifact names do not match the transaction ID");

	const std::size_t publishSave = static_cast<std::size_t>(Operation::publishSave);
	const bool physicallyPublished = store.exists(header.finalSave) && !store.exists(header.stagedSave) &&
		(store.exists(header.backupSave) || !header.hadOldSave);
	if (journal.intents[publishSave] || journal.completions[publishSave] || physicallyPublished)
		recoverCommit(store, durable, header, journalName);
	else
		recoverRollback(store, durable, header, journalName);
}
}

SaveTransaction::SaveTransaction(WritableStore& store, DurableFileOperations& durable,
	std::string saveName, std::optional<std::string> sidecarName) :
	store_(store), durable_(durable), saveName_(std::move(saveName)), sidecarName_(std::move(sidecarName))
{
	if (saveName_.empty()) throw Error(ErrorCode::invalidPath, "empty save transaction target");
	if (sidecarName_ && sidecarName_->empty())
		throw Error(ErrorCode::invalidPath, "empty save sidecar target");
	if (sidecarName_ && directoryOf(*sidecarName_) != directoryOf(saveName_))
		throw Error(ErrorCode::invalidPath, "save and sidecar must be siblings");
	// Exercise the store's path validation before any transaction artifacts exist.
	(void)store_.exists(saveName_);
	if (sidecarName_) (void)store_.exists(*sidecarName_);
}

void SaveTransaction::commit(const StageWriter& saveWriter,
	const std::optional<StageWriter>& sidecarWriter)
{
	if (!saveWriter) throw Error(ErrorCode::io, "save transaction has no save writer");
	if (sidecarWriter && !sidecarName_)
		throw Error(ErrorCode::invalidPath, "sidecar bytes supplied without a sidecar target");
	if (sidecarWriter && !*sidecarWriter)
		throw Error(ErrorCode::io, "save transaction has an empty sidecar writer");

	std::string journalStageName;
	std::string journalName;
	JournalHeader header = allocateNames(store_, saveName_, sidecarName_, journalStageName, journalName);
	header.hadOldSave = store_.exists(saveName_);
	header.hadOldSidecar = sidecarName_ && store_.exists(*sidecarName_);
	header.hasNewSidecar = sidecarWriter.has_value();
	bool saveStaged = false;
	bool sidecarStaged = false;
	try
	{
		if (sidecarWriter)
		{
			stage(store_, durable_, header.stagedSidecar, *sidecarWriter);
			sidecarStaged = true;
		}
		stage(store_, durable_, header.stagedSave, saveWriter);
		saveStaged = true;
	}
	catch (...)
	{
		const bool removeSidecar = sidecarStaged || store_.exists(header.stagedSidecar);
		const bool removeSave = saveStaged || store_.exists(header.stagedSave);
		if (removeSidecar) removeIfPresent(store_, header.stagedSidecar);
		if (removeSave) removeIfPresent(store_, header.stagedSave);
		if (removeSave || removeSidecar) durable_.syncDirectory(directoryOf(saveName_));
		throw;
	}

	const std::string directory = directoryOf(saveName_);
	writeNewJournal(store_, durable_, journalStageName, journalName, directory, header);
	if (header.hadOldSave)
		perform(store_, durable_, journalName, directory, Operation::backupSave,
			header.finalSave, header.backupSave);
	if (header.finalSidecar && header.hadOldSidecar)
		perform(store_, durable_, journalName, directory, Operation::backupSidecar,
			*header.finalSidecar, header.backupSidecar);
	if (header.finalSidecar && header.hasNewSidecar)
		perform(store_, durable_, journalName, directory, Operation::publishSidecar,
			header.stagedSidecar, *header.finalSidecar);
	perform(store_, durable_, journalName, directory, Operation::publishSave,
		header.stagedSave, header.finalSave);
	cleanup(store_, durable_, header, journalName);
}

void SaveTransaction::commit(StagedBytes saveBytes, std::optional<StagedBytes> sidecarBytes)
{
	if (saveBytes.size != 0 && saveBytes.data == nullptr)
		throw Error(ErrorCode::io, "null save byte range");
	if (sidecarBytes && sidecarBytes->size != 0 && sidecarBytes->data == nullptr)
		throw Error(ErrorCode::io, "null sidecar byte range");
	const StageWriter saveWriter = [saveBytes](File& file)
	{
		file.writeExact(saveBytes.data, saveBytes.size);
	};
	std::optional<StageWriter> sidecarWriter;
	if (sidecarBytes)
	{
		const StagedBytes bytes = *sidecarBytes;
		sidecarWriter = [bytes](File& file) { file.writeExact(bytes.data, bytes.size); };
	}
	commit(saveWriter, sidecarWriter);
}

void SaveTransaction::recoverDirectory(WritableStore& store, DurableFileOperations& durable,
	std::string_view directory, AbandonedStagePolicy abandonedPolicy)
{
	const std::string journalPattern = join(directory, "*.ja2txn-*.journal");
	const std::vector<DirectoryEntry> journals = store.list(journalPattern);
	for (const DirectoryEntry& entry : journals)
	{
		if (!entry.metadata.directory && endsWith(entry.name, ".journal"))
			recoverOne(store, durable, join(directory, entry.name));
	}

	if (abandonedPolicy == AbandonedStagePolicy::remove)
	{
		bool removed = false;
		const std::vector<DirectoryEntry> stages = store.list(join(directory, "*.ja2txn-*.stage"));
		for (const DirectoryEntry& entry : stages)
		{
			if (!entry.metadata.directory && entry.name.find(journalMarker) != std::string::npos &&
				endsWith(entry.name, ".stage"))
			{
				store.remove(join(directory, entry.name));
				removed = true;
			}
		}
		if (removed) durable.syncDirectory(directory);
	}
}
}
