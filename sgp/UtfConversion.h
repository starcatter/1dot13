#pragma once

#include "types.h"

#include <stdexcept>
#include <string>
#include <string_view>

namespace ja2::text
{
using Utf16String = std::basic_string<CHAR16>;
using Utf16View = std::basic_string_view<CHAR16>;

enum class Encoding
{
	utf8,
	utf16
};

class ConversionError final : public std::runtime_error
{
public:
	explicit ConversionError( Encoding encoding );
	Encoding encoding() const noexcept;

private:
	Encoding encoding_;
};

bool isValidUtf8( std::string_view input ) noexcept;
Utf16String utf8ToUtf16( std::string_view input );
Utf16String utf8ToUtf16ReplacingInvalid( std::string_view input );
std::string utf16ToUtf8( Utf16View input );
std::string utf16ToUtf8ReplacingInvalid( Utf16View input );
std::string utf16ToUtf8( const CHAR16 *input );
}
