#ifndef JA2_LEGACY_UTF16_H
#define JA2_LEGACY_UTF16_H

#include "types.h"

#include <cstdarg>
#include <cstddef>

#ifndef _WIN32

std::size_t wcslen(const CHAR16* string) noexcept;
CHAR16* wcscpy(CHAR16* destination, const CHAR16* source) noexcept;
CHAR16* wcsncpy(CHAR16* destination, const CHAR16* source,
	std::size_t count) noexcept;
CHAR16* wcscat(CHAR16* destination, const CHAR16* source) noexcept;
CHAR16* wcsncat(CHAR16* destination, const CHAR16* source,
	std::size_t count) noexcept;
int wcscmp(const CHAR16* left, const CHAR16* right) noexcept;
int wcsncmp(const CHAR16* left, const CHAR16* right,
	std::size_t count) noexcept;
int _wcsicmp(const CHAR16* left, const CHAR16* right) noexcept;
CHAR16* wcsstr(CHAR16* string, const CHAR16* sought) noexcept;
const CHAR16* wcsstr(const CHAR16* string, const CHAR16* sought) noexcept;
CHAR16* wcschr(CHAR16* string, CHAR16 sought) noexcept;
const CHAR16* wcschr(const CHAR16* string, CHAR16 sought) noexcept;
CHAR16* wcsrchr(CHAR16* string, CHAR16 sought) noexcept;
const CHAR16* wcsrchr(const CHAR16* string, CHAR16 sought) noexcept;
std::size_t wcscspn(const CHAR16* string, const CHAR16* rejected) noexcept;
CHAR16* wcstok(CHAR16* string, const CHAR16* delimiters) noexcept;

int vswprintf(CHAR16* destination, const CHAR16* format,
	std::va_list arguments);
int vswprintf(CHAR16* destination, std::size_t capacity,
	const CHAR16* format, std::va_list arguments);
int swprintf(CHAR16* destination, const CHAR16* format, ...);
int swprintf(CHAR16* destination, std::size_t capacity,
	const CHAR16* format, ...);
int swscanf(const CHAR16* input, const CHAR16* format, ...);
int _wtoi(const CHAR16* input) noexcept;
CHAR16* _itow(int value, CHAR16* output, int radix) noexcept;

#endif

#endif
