#ifndef JA2_FILEIO_PLATFORMPATHS_H
#define JA2_FILEIO_PLATFORMPATHS_H

#include <string>
#include <string_view>

namespace ja2::fileio
{
bool setCurrentDirectory(std::string_view path) noexcept;
std::string currentDirectory();
std::string executableDirectory();
}

#endif
