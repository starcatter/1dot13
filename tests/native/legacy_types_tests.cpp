#include "sgp.h"

#include <cstdint>
#include <type_traits>

static_assert(sizeof(INT8) == 1);
static_assert(sizeof(UINT8) == 1);
static_assert(sizeof(INT16) == 2);
static_assert(sizeof(UINT16) == 2);
static_assert(sizeof(INT32) == 4);
static_assert(sizeof(UINT32) == 4);
static_assert(sizeof(INT64) == 8);
static_assert(sizeof(UINT64) == 8);
static_assert(sizeof(FLAGS8) == 1);
static_assert(sizeof(FLAGS16) == 2);
static_assert(sizeof(FLAGS32) == 4);
static_assert(sizeof(FLAGS64) == 8);
static_assert(sizeof(CHAR16) == 2);
static_assert(std::is_unsigned_v<FLAGS32>);

#ifndef _WIN32
static_assert(std::is_same_v<CHAR16, char16_t>);
static_assert(!std::is_same_v<CHAR16, wchar_t>);
#endif

int main()
{
	const CHAR16 text[] = u"JA2";
	return text[0] == static_cast<CHAR16>('J') && text[3] == 0 ? 0 : 1;
}
