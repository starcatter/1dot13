#pragma once

#include "UtfConversion.h"

#include <vfs/Aspects/vfs_settings.h>
#include <vfs/Core/vfs_string.h>

#include <string>

// Compatibility for the remaining diagnostic code that stores host-wide
// strings. New engine code should use ja2::text and fixed-width CHAR16 text.
inline void convert_string( std::wstring const& input, std::string& output )
{
	if ( vfs::Settings::getUseUnicode() )
	{
#ifdef _WIN32
		output = ja2::text::utf16ToUtf8( ja2::text::Utf16View( input.data(), input.size() ) );
#else
		output = vfs::String::as_utf8( input );
#endif
	}
	else
	{
		vfs::String::narrow( input, output );
	}
}

inline void convert_string( std::string const& input, std::wstring& output )
{
	if ( vfs::Settings::getUseUnicode() )
	{
#ifdef _WIN32
		const ja2::text::Utf16String converted = ja2::text::utf8ToUtf16( input );
		output.assign( converted.begin(), converted.end() );
#else
		vfs::String::as_utf16( input, output );
#endif
	}
	else
	{
		vfs::String::widen( input, output );
	}
}
