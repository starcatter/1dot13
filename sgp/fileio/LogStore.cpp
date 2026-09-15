#include "LogStore.h"

#include "PhysicalWritableStore.h"
#include "StoreRouter.h"

#include <string>

namespace ja2::fileio
{
namespace
{
std::string logicalName(std::string_view name)
{
	return "Logs/" + StoreRouter::normalizeLogicalPath(name);
}

std::string crlfText(std::string_view text)
{
	std::string result;
	result.reserve(text.size() + 2);
	for (std::size_t index = 0; index < text.size(); ++index)
	{
		if (text[index] == '\r' && index + 1 < text.size() && text[index + 1] == '\n')
		{
			result.append("\r\n");
			++index;
		}
		else if (text[index] == '\n')
		{
			result.append("\r\n");
		}
		else
		{
			result.push_back(text[index]);
		}
	}
	result.append("\r\n");
	return result;
}
}

bool LogStore::appendBytes(
	std::string_view name, const void* data, std::size_t size) noexcept
{
	try
	{
		std::lock_guard<std::mutex> lock(mutex_);
		auto file = router_.openWrite(logicalName(name));
		file->seek(0, SeekOrigin::end);
		file->writeExact(data, size);
		file->flush();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool LogStore::appendLine(std::string_view name, std::string_view line) noexcept
{
	try
	{
		const std::string bytes = crlfText(line);
		std::lock_guard<std::mutex> lock(mutex_);
		auto file = router_.openWrite(logicalName(name));
		file->seek(0, SeekOrigin::end);
		file->writeExact(bytes.data(), bytes.size());
		file->flush();
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool LogStore::remove(std::string_view name) noexcept
{
	try
	{
		std::lock_guard<std::mutex> lock(mutex_);
		router_.remove(logicalName(name));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool LogStore::truncate(std::string_view name) noexcept
{
	try
	{
		std::lock_guard<std::mutex> lock(mutex_);
		router_.currentWritableStore()->create(logicalName(name));
		return true;
	}
	catch (...)
	{
		return false;
	}
}
}
