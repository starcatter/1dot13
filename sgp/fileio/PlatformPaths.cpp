#include "PlatformPaths.h"

#include "FileIO.h"

#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <cerrno>
#include <unistd.h>
#endif

namespace ja2::fileio
{
namespace
{
namespace fs = std::filesystem;

std::string displayPath(const fs::path& path)
{
	return path.u8string();
}
}

bool setCurrentDirectory(std::string_view path) noexcept
{
	try
	{
		std::error_code error;
		fs::current_path(fs::u8path(path.begin(), path.end()), error);
		return !error;
	}
	catch (...)
	{
		return false;
	}
}

std::string currentDirectory()
{
	std::error_code error;
	const fs::path path = fs::current_path(error);
	if (error) throw Error(ErrorCode::io, "read current directory failed: " + error.message());
	return displayPath(path);
}

std::string executableDirectory()
{
#ifdef _WIN32
	std::vector<wchar_t> buffer(32768);
	const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
	if (length == 0 || length >= static_cast<DWORD>(buffer.size()))
		throw Error(ErrorCode::io, "read executable path failed");
	return displayPath(fs::path(std::wstring(buffer.data(), length)).parent_path());
#elif defined(__linux__)
	std::vector<char> buffer(256);
	for (;;)
	{
		const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
		if (length < 0)
		{
			// Procfs may be unavailable in a restricted container. Retain the
			// historical working-directory fallback rather than failing startup.
			return currentDirectory();
		}
		if (static_cast<std::size_t>(length) < buffer.size())
		{
			const fs::path executable = fs::u8path(buffer.begin(), buffer.begin() + length);
			std::error_code error;
			const fs::path canonical = fs::weakly_canonical(executable, error);
			return displayPath((error ? executable : canonical).parent_path());
		}
		if (buffer.size() >= 1024 * 1024)
			throw Error(ErrorCode::unsupported, "executable path exceeds one MiB");
		buffer.resize(buffer.size() * 2);
	}
#else
	return currentDirectory();
#endif
}
}
