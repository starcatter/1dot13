#pragma once

#include "types.h"

#include <cstddef>
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

struct BufferConversionResult
{
	std::size_t codeUnitsWritten;
	bool inputWasValid;
	bool truncated;
};

bool isValidUtf8( std::string_view input ) noexcept;
Utf16String utf8ToUtf16( std::string_view input );
Utf16String utf8ToUtf16ReplacingInvalid( std::string_view input );
std::string utf16ToUtf8( Utf16View input );
std::string utf16ToUtf8ReplacingInvalid( Utf16View input );
std::string utf16ToUtf8( const CHAR16 *input );

// Compatibility boundary for legacy fixed-size engine strings. Invalid UTF-8
// is replaced with U+FFFD, output is NUL-terminated when capacity is non-zero,
// and truncation never writes half of a UTF-16 surrogate pair.
BufferConversionResult copyUtf8ToUtf16(
	std::string_view input, CHAR16 *output, std::size_t capacity );

template<std::size_t Size>
BufferConversionResult copyUtf8ToUtf16( std::string_view input, CHAR16 (&output)[Size] )
{
	return copyUtf8ToUtf16( input, output, Size );
}
}
