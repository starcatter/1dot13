#ifndef JA2_LEGACY_COMPILER_COMPATIBILITY_H
#define JA2_LEGACY_COMPILER_COMPATIBILITY_H

#include "LegacyUtf16.h"

#include <cstdio>

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <climits>
#include <cmath>
#include <cstring>
#include <strings.h>
#include <type_traits>

// Temporary source-compatibility helpers for gameplay translation units that
// historically relied on windows.h macros. New portable code must use std::min,
// std::max, and std::size directly.
using std::max;
using std::min;

template <typename Left, typename Right,
	std::enable_if_t<!std::is_same_v<Left, Right>, int> = 0>
constexpr std::common_type_t<Left, Right> min(Left left, Right right)
{
	using Result = std::common_type_t<Left, Right>;
	return static_cast<Result>(right) < static_cast<Result>(left)
		? static_cast<Result>(right) : static_cast<Result>(left);
}

template <typename Left, typename Right,
	std::enable_if_t<!std::is_same_v<Left, Right>, int> = 0>
constexpr std::common_type_t<Left, Right> max(Left left, Right right)
{
	using Result = std::common_type_t<Left, Right>;
	return static_cast<Result>(left) < static_cast<Result>(right)
		? static_cast<Result>(right) : static_cast<Result>(left);
}

#ifndef _WIN32
inline void OutputDebugString(const char* message)
{
	if (message) std::fputs(message, stderr);
}

#define __debugbreak() __builtin_trap()

inline CHAR16* _wcsupr(CHAR16* text)
{
	if (!text) return text;
	for (CHAR16* character = text; *character; ++character)
		if (*character >= u'a' && *character <= u'z')
			*character = static_cast<CHAR16>(*character - u'a' + u'A');
	return text;
}

inline std::size_t wcsnlen(const CHAR16* text, std::size_t maximum)
{
	if (!text) return 0;
	std::size_t length = 0;
	while (length < maximum && text[length]) ++length;
	return length;
}

#ifndef MAXUINT8
constexpr UINT8 MAXUINT8 = UINT8_MAX;
#endif

using INT = int;
using LPCSTR = const char*;
using std::fabs;
using std::pow;
using std::sqrt;

struct RECT
{
	long left;
	long top;
	long right;
	long bottom;
};

inline CHAR16* _ltow(long value, CHAR16* output, int radix)
{
	return _itow(static_cast<int>(value), output, radix);
}

#define ZeroMemory(destination, size) std::memset((destination), 0, (size))

#ifndef MAX_PATH
constexpr std::size_t MAX_PATH = 260;
#endif

#define _stricmp strcasecmp
#define _strnicmp strncasecmp

inline int sprintf_s(char* destination, std::size_t capacity,
	const char* format, ...)
{
	std::va_list arguments;
	va_start(arguments, format);
	const int result = std::vsnprintf(destination, capacity, format, arguments);
	va_end(arguments);
	return result;
}

template <std::size_t Capacity>
int sprintf_s(char (&destination)[Capacity], const char* format, ...)
{
	std::va_list arguments;
	va_start(arguments, format);
	const int result = std::vsnprintf(destination, Capacity, format, arguments);
	va_end(arguments);
	return result;
}

inline int wcscpy_s(CHAR16* destination, std::size_t capacity,
	const CHAR16* source)
{
	const std::size_t length = wcslen(source);
	if (capacity == 0 || length >= capacity)
	{
		if (capacity != 0) destination[0] = 0;
		return 1;
	}
	wcscpy(destination, source);
	return 0;
}

inline int wcscat_s(CHAR16* destination, std::size_t capacity,
	const CHAR16* source)
{
	const std::size_t destinationLength = wcslen(destination);
	const std::size_t sourceLength = wcslen(source);
	if (destinationLength >= capacity || sourceLength >= capacity - destinationLength)
	{
		if (capacity != 0) destination[0] = 0;
		return 1;
	}
	wcscat(destination, source);
	return 0;
}

template <typename Left, typename Right>
constexpr std::common_type_t<Left, Right> __min(Left left, Right right)
{
	using Result = std::common_type_t<Left, Right>;
	return static_cast<Result>(right) < static_cast<Result>(left)
		? static_cast<Result>(right) : static_cast<Result>(left);
}

template <typename Left, typename Right>
constexpr std::common_type_t<Left, Right> __max(Left left, Right right)
{
	using Result = std::common_type_t<Left, Right>;
	return static_cast<Result>(left) < static_cast<Result>(right)
		? static_cast<Result>(right) : static_cast<Result>(left);
}
#endif

template <typename T, std::size_t Size>
constexpr std::size_t _countof(T (&)[Size]) noexcept
{
	return Size;
}

#endif
