#include "UtfConversion.h"

#include <utf8.h>

#include <algorithm>
#include <iterator>

namespace ja2::text
{
ConversionError::ConversionError( Encoding encoding )
	: std::runtime_error( encoding == Encoding::utf8 ? "Invalid UTF-8" : "Invalid UTF-16" ),
	  encoding_( encoding )
{
}

Encoding ConversionError::encoding() const noexcept
{
	return encoding_;
}

bool isValidUtf8( std::string_view input ) noexcept
{
	return utf8::is_valid( input.begin(), input.end() );
}

bool isValidUtf16( Utf16View input ) noexcept
{
	for ( std::size_t index = 0; index < input.size(); ++index )
	{
		const CHAR16 codeUnit = input[index];
		if ( codeUnit >= static_cast<CHAR16>( 0xd800 ) && codeUnit <= static_cast<CHAR16>( 0xdbff ) )
		{
			if ( index + 1 >= input.size() || input[index + 1] < static_cast<CHAR16>( 0xdc00 ) ||
				input[index + 1] > static_cast<CHAR16>( 0xdfff ) )
			{
				return false;
			}
			++index;
		}
		else if ( codeUnit >= static_cast<CHAR16>( 0xdc00 ) && codeUnit <= static_cast<CHAR16>( 0xdfff ) )
		{
			return false;
		}
	}
	return true;
}

Utf16String utf8ToUtf16( std::string_view input )
{
	Utf16String result;
	try
	{
		utf8::utf8to16( input.begin(), input.end(), std::back_inserter( result ) );
	}
	catch ( const utf8::exception& )
	{
		throw ConversionError( Encoding::utf8 );
	}
	return result;
}

Utf16String utf8ToUtf16ReplacingInvalid( std::string_view input )
{
	std::string validUtf8;
	utf8::replace_invalid( input.begin(), input.end(), std::back_inserter( validUtf8 ) );
	return utf8ToUtf16( validUtf8 );
}

std::string utf16ToUtf8( Utf16View input )
{
	std::string result;
	try
	{
		utf8::utf16to8( input.begin(), input.end(), std::back_inserter( result ) );
	}
	catch ( const utf8::exception& )
	{
		throw ConversionError( Encoding::utf16 );
	}
	return result;
}

std::string utf16ToUtf8ReplacingInvalid( Utf16View input )
{
	Utf16String validUtf16;
	validUtf16.reserve( input.size() );
	for ( std::size_t index = 0; index < input.size(); ++index )
	{
		const CHAR16 codeUnit = input[index];
		if ( codeUnit >= static_cast<CHAR16>( 0xd800 ) && codeUnit <= static_cast<CHAR16>( 0xdbff ) )
		{
			if ( index + 1 < input.size() && input[index + 1] >= static_cast<CHAR16>( 0xdc00 ) &&
				input[index + 1] <= static_cast<CHAR16>( 0xdfff ) )
			{
				validUtf16.push_back( codeUnit );
				validUtf16.push_back( input[++index] );
			}
			else
			{
				validUtf16.push_back( static_cast<CHAR16>( 0xfffd ) );
			}
		}
		else if ( codeUnit >= static_cast<CHAR16>( 0xdc00 ) && codeUnit <= static_cast<CHAR16>( 0xdfff ) )
		{
			validUtf16.push_back( static_cast<CHAR16>( 0xfffd ) );
		}
		else
		{
			validUtf16.push_back( codeUnit );
		}
	}
	return utf16ToUtf8( validUtf16 );
}

std::string utf16ToUtf8( const CHAR16 *input )
{
	if ( input == nullptr )
		return {};
	return utf16ToUtf8( Utf16View( input, std::char_traits<CHAR16>::length( input ) ) );
}

std::string utf16ToUtf8ReplacingInvalid( const CHAR16 *input )
{
	if ( input == nullptr )
		return {};
	return utf16ToUtf8ReplacingInvalid(
		Utf16View( input, std::char_traits<CHAR16>::length( input ) ) );
}

BufferConversionResult copyUtf8ToUtf16(
	std::string_view input, CHAR16 *output, std::size_t capacity )
{
	if ( output == nullptr && capacity != 0 )
		throw std::invalid_argument( "UTF-16 output buffer is null" );

	const bool inputWasValid = isValidUtf8( input );
	const Utf16String converted = utf8ToUtf16ReplacingInvalid( input );
	if ( capacity == 0 )
		return { 0, inputWasValid, !converted.empty() };

	const std::size_t maximumCodeUnits = capacity - 1;
	std::size_t codeUnitsWritten = std::min( maximumCodeUnits, converted.size() );
	if ( codeUnitsWritten < converted.size() && codeUnitsWritten != 0 &&
		converted[codeUnitsWritten - 1] >= static_cast<CHAR16>( 0xd800 ) &&
		converted[codeUnitsWritten - 1] <= static_cast<CHAR16>( 0xdbff ) )
	{
		--codeUnitsWritten;
	}

	std::copy_n( converted.begin(), codeUnitsWritten, output );
	output[codeUnitsWritten] = 0;
	return { codeUnitsWritten, inputWasValid, codeUnitsWritten < converted.size() };
}

ByteBufferConversionResult copyUtf16ToUtf8(
	Utf16View input, char *output, std::size_t capacity )
{
	if ( output == nullptr && capacity != 0 )
		throw std::invalid_argument( "UTF-8 output buffer is null" );

	const bool inputWasValid = isValidUtf16( input );
	const std::string converted = utf16ToUtf8ReplacingInvalid( input );
	if ( capacity == 0 )
		return { 0, inputWasValid, !converted.empty() };

	const std::size_t maximumBytes = capacity - 1;
	std::size_t bytesWritten = std::min( maximumBytes, converted.size() );
	if ( bytesWritten < converted.size() )
	{
		while ( bytesWritten != 0 &&
			(static_cast<unsigned char>( converted[bytesWritten] ) & 0xc0) == 0x80 )
		{
			--bytesWritten;
		}
	}

	std::copy_n( converted.begin(), bytesWritten, output );
	output[bytesWritten] = '\0';
	return { bytesWritten, inputWasValid, bytesWritten < converted.size() };
}

ByteBufferConversionResult copyUtf16ToUtf8(
	const CHAR16 *input, char *output, std::size_t capacity )
{
	const Utf16View view = input == nullptr
		? Utf16View{}
		: Utf16View( input, std::char_traits<CHAR16>::length( input ) );
	return copyUtf16ToUtf8( view, output, capacity );
}
}
