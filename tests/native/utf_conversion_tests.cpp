#include "UtfConversion.h"

#include <iostream>
#include <string>

namespace
{
int failures = 0;

void expect( bool condition, const char *message )
{
	if ( condition )
		return;
	std::cerr << message << '\n';
	++failures;
}

template<typename Function>
void expectConversionError( Function function, ja2::text::Encoding encoding, const char *message )
{
	try
	{
		function();
		expect( false, message );
	}
	catch ( const ja2::text::ConversionError& error )
	{
		expect( error.encoding() == encoding, "conversion error identified the wrong input encoding" );
	}
	catch ( ... )
	{
		expect( false, "conversion leaked a third-party exception" );
	}
}
}

int main()
{
	using namespace ja2::text;

	expect( utf8ToUtf16( "" ).empty(), "empty UTF-8 did not produce empty UTF-16" );
	expect( utf16ToUtf8( Utf16View{} ).empty(), "empty UTF-16 did not produce empty UTF-8" );
	expect( utf16ToUtf8( static_cast<const CHAR16 *>( nullptr ) ).empty(), "null legacy string was not empty" );

	const std::string multilingual = u8"Zażółć gęślą jaźń — Привет — 日本語 — 😀";
	const Utf16String wide = utf8ToUtf16( multilingual );
	expect( utf16ToUtf8( wide ) == multilingual, "multilingual text did not round-trip" );
	expect( wide.size() >= 2 && wide[wide.size() - 2] == static_cast<CHAR16>( 0xd83d ) &&
		wide.back() == static_cast<CHAR16>( 0xde00 ), "supplementary character was not encoded as a surrogate pair" );

	const std::string withNull( "A\0B", 3 );
	const Utf16String wideWithNull = utf8ToUtf16( withNull );
	expect( wideWithNull.size() == 3, "embedded NUL changed UTF-16 length" );
	expect( utf16ToUtf8( wideWithNull ) == withNull, "embedded NUL did not round-trip" );

	const Utf16String emoji{ static_cast<CHAR16>( 0xd83d ), static_cast<CHAR16>( 0xde00 ) };
	expect( utf16ToUtf8( emoji ) == "\xf0\x9f\x98\x80", "surrogate pair produced the wrong UTF-8" );

	const std::string invalidUtf8( "\xc0\xaf", 2 );
	expect( !isValidUtf8( invalidUtf8 ), "overlong UTF-8 sequence was accepted" );
	expectConversionError( [&] { (void) utf8ToUtf16( invalidUtf8 ); }, Encoding::utf8,
		"invalid UTF-8 did not produce ConversionError" );
	const Utf16String replaced = utf8ToUtf16ReplacingInvalid( invalidUtf8 );
	expect( utf16ToUtf8( replaced ) == "\xef\xbf\xbd", "invalid UTF-8 was not replaced predictably" );

	const Utf16String loneSurrogate{ static_cast<CHAR16>( 0xd800 ) };
	expectConversionError( [&] { (void) utf16ToUtf8( loneSurrogate ); }, Encoding::utf16,
		"invalid UTF-16 did not produce ConversionError" );
	expect( utf16ToUtf8ReplacingInvalid( loneSurrogate ) == "\xef\xbf\xbd",
		"invalid UTF-16 was not replaced predictably" );

	CHAR16 exactBuffer[4]{};
	const BufferConversionResult exactResult = copyUtf8ToUtf16( "ABC", exactBuffer );
	expect( exactResult.codeUnitsWritten == 3 && exactResult.inputWasValid && !exactResult.truncated,
		"exact-fit buffer conversion reported the wrong result" );
	expect( exactBuffer[0] == static_cast<CHAR16>( 'A' ) && exactBuffer[2] == static_cast<CHAR16>( 'C' ) &&
		exactBuffer[3] == 0, "exact-fit buffer conversion wrote the wrong text" );

	CHAR16 shortBuffer[3]{};
	const BufferConversionResult shortResult = copyUtf8ToUtf16( "ABCD", shortBuffer );
	expect( shortResult.codeUnitsWritten == 2 && shortResult.truncated,
		"short buffer did not report truncation" );
	expect( shortBuffer[0] == static_cast<CHAR16>( 'A' ) && shortBuffer[1] == static_cast<CHAR16>( 'B' ) &&
		shortBuffer[2] == 0, "short buffer was not safely terminated" );

	CHAR16 explicitlyLimitedBuffer[4]{ static_cast<CHAR16>( 'X' ), static_cast<CHAR16>( 'X' ),
		static_cast<CHAR16>( 'X' ), static_cast<CHAR16>( 'X' ) };
	const BufferConversionResult limitedResult =
		copyUtf8ToUtf16( "ABC", explicitlyLimitedBuffer, 3 );
	expect( limitedResult.codeUnitsWritten == 2 && limitedResult.truncated,
		"explicit buffer capacity was not respected" );
	expect( explicitlyLimitedBuffer[0] == static_cast<CHAR16>( 'A' ) &&
		explicitlyLimitedBuffer[1] == static_cast<CHAR16>( 'B' ) && explicitlyLimitedBuffer[2] == 0 &&
		explicitlyLimitedBuffer[3] == static_cast<CHAR16>( 'X' ),
		"explicit buffer capacity wrote beyond its declared range" );

	CHAR16 surrogateBuffer[2]{ static_cast<CHAR16>( 'X' ), static_cast<CHAR16>( 'X' ) };
	const BufferConversionResult surrogateResult = copyUtf8ToUtf16( "\xf0\x9f\x98\x80", surrogateBuffer );
	expect( surrogateResult.codeUnitsWritten == 0 && surrogateResult.truncated && surrogateBuffer[0] == 0,
		"buffer truncation split a surrogate pair" );

	CHAR16 replacementBuffer[2]{};
	const BufferConversionResult replacementResult = copyUtf8ToUtf16( invalidUtf8, replacementBuffer );
	expect( replacementResult.codeUnitsWritten == 1 && !replacementResult.inputWasValid &&
		!replacementResult.truncated && replacementBuffer[0] == static_cast<CHAR16>( 0xfffd ),
		"buffer conversion did not report and replace invalid UTF-8" );

	const BufferConversionResult zeroCapacityResult = copyUtf8ToUtf16( "A", nullptr, 0 );
	expect( zeroCapacityResult.codeUnitsWritten == 0 && zeroCapacityResult.truncated,
		"zero-capacity buffer reported the wrong result" );
	expectConversionError( [] { (void) utf8ToUtf16( std::string( "\xed\xa0\x80", 3 ) ); }, Encoding::utf8,
		"UTF-8 encoding of a surrogate was accepted" );
	try
	{
		(void) copyUtf8ToUtf16( "A", nullptr, 1 );
		expect( false, "null non-empty output buffer was accepted" );
	}
	catch ( const std::invalid_argument& )
	{
	}

	return failures == 0 ? 0 : 1;
}
