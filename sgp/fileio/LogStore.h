#ifndef JA2_FILEIO_LOGSTORE_H
#define JA2_FILEIO_LOGSTORE_H

#include <cstddef>
#include <mutex>
#include <string_view>

namespace ja2::fileio
{
class StoreRouter;
StoreRouter& storeRouter();

// Stores log names relative to the active writable root's Logs directory.
class LogStore final
{
public:
	explicit LogStore(StoreRouter& router) noexcept : router_(router) {}

	LogStore(const LogStore&) = delete;
	LogStore& operator=(const LogStore&) = delete;

	bool appendBytes(std::string_view name, const void* data, std::size_t size) noexcept;
	bool appendLine(std::string_view name, std::string_view line) noexcept;
	bool remove(std::string_view name) noexcept;
	bool truncate(std::string_view name) noexcept;

private:
	StoreRouter& router_;
	std::mutex mutex_;
};

inline LogStore& logStore()
{
	static LogStore instance(storeRouter());
	return instance;
}
}

#endif
