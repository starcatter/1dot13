#pragma once

#include <string>

// Compatibility for the remaining diagnostic code that stores host-wide
// strings. New engine code should use ja2::text and fixed-width CHAR16 text.
void convert_string( std::wstring const& input, std::string& output );
void convert_string( std::string const& input, std::wstring& output );
