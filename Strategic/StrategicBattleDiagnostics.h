#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>

namespace ja2::strategic::diagnostics
{
	inline bool enabled()
	{
		static const bool value = std::getenv("JA2_STRATEGIC_BATTLE_DIAGNOSTICS") != nullptr;
		return value;
	}

	inline void log(const char* format, ...)
	{
		if (!enabled())
			return;

		std::fputs("strategic battle diagnostic: ", stderr);
		va_list arguments;
		va_start(arguments, format);
		std::vfprintf(stderr, format, arguments);
		va_end(arguments);
		std::fputc('\n', stderr);
		std::fflush(stderr);
	}
}
