#include "platform/Dialog.h"

#include <cstdio>

namespace Platform
{
void ShowDialog(
	std::string_view titleUtf8, std::string_view messageUtf8, DialogKind) noexcept
{
	std::fprintf(stderr, "%.*s: %.*s\n",
		static_cast<int>(titleUtf8.size()), titleUtf8.empty() ? "" : titleUtf8.data(),
		static_cast<int>(messageUtf8.size()), messageUtf8.empty() ? "" : messageUtf8.data());
}

bool AskYesNo(std::string_view titleUtf8, std::string_view messageUtf8) noexcept
{
	std::fprintf(stderr, "%.*s: %.*s [default: no]\n",
		static_cast<int>(titleUtf8.size()), titleUtf8.empty() ? "" : titleUtf8.data(),
		static_cast<int>(messageUtf8.size()), messageUtf8.empty() ? "" : messageUtf8.data());
	return false;
}
}
