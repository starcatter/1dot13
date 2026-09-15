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
#else
	return currentDirectory();
#endif
}
}
