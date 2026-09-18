#include "LegacyUtf16.h"

#ifndef _WIN32

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <cwctype>
#include <limits>
#include <string>
#include <type_traits>

std::size_t wcslen(const CHAR16* string) noexcept
{
	const CHAR16* end = string;
	while (*end != 0)
	{
		++end;
	}
	return static_cast<std::size_t>(end - string);
}

CHAR16* wcscpy(CHAR16* destination, const CHAR16* source) noexcept
{
	CHAR16* result = destination;
	while ((*destination++ = *source++) != 0)
	{
	}
	return result;
}

CHAR16* wcsncpy(
	CHAR16* destination, const CHAR16* source, std::size_t count) noexcept
{
	std::size_t index = 0;
	for (; index < count && source[index] != 0; ++index)
	{
		destination[index] = source[index];
	}
	for (; index < count; ++index)
	{
		destination[index] = 0;
	}
	return destination;
}

CHAR16* wcscat(CHAR16* destination, const CHAR16* source) noexcept
{
	wcscpy(destination + wcslen(destination), source);
	return destination;
}

CHAR16* wcsncat(
	CHAR16* destination, const CHAR16* source, std::size_t count) noexcept
{
	CHAR16* output = destination + wcslen(destination);
	std::size_t index = 0;
	for (; index < count && source[index] != 0; ++index)
	{
		output[index] = source[index];
	}
	output[index] = 0;
	return destination;
}

int wcscmp(const CHAR16* left, const CHAR16* right) noexcept
{
	while (*left != 0 && *left == *right)
	{
		++left;
		++right;
	}
	return *left < *right ? -1 : (*left > *right ? 1 : 0);
}

int wcsncmp(
	const CHAR16* left, const CHAR16* right, std::size_t count) noexcept
{
	for (std::size_t index = 0; index < count; ++index)
	{
		if (left[index] != right[index])
		{
			return left[index] < right[index] ? -1 : 1;
		}
		if (left[index] == 0)
		{
			return 0;
		}
	}
	return 0;
}

int _wcsicmp(const CHAR16* left, const CHAR16* right) noexcept
{
	while (*left != 0 && *right != 0)
	{
		const auto foldedLeft = static_cast<CHAR16>(
			std::towlower(static_cast<wint_t>(*left)));
		const auto foldedRight = static_cast<CHAR16>(
			std::towlower(static_cast<wint_t>(*right)));
		if (foldedLeft != foldedRight)
		{
			return foldedLeft < foldedRight ? -1 : 1;
		}
		++left;
		++right;
	}
	return *left < *right ? -1 : (*left > *right ? 1 : 0);
}

const CHAR16* wcsstr(const CHAR16* string, const CHAR16* sought) noexcept
{
	if (*sought == 0)
	{
		return string;
	}
	for (; *string != 0; ++string)
	{
		if (*string == *sought &&
			wcsncmp(string, sought, wcslen(sought)) == 0)
		{
			return string;
		}
	}
	return nullptr;
}

CHAR16* wcsstr(CHAR16* string, const CHAR16* sought) noexcept
{
	return const_cast<CHAR16*>(wcsstr(
		static_cast<const CHAR16*>(string), sought));
}

const CHAR16* wcschr(const CHAR16* string, CHAR16 sought) noexcept
{
	for (;; ++string)
	{
		if (*string == sought)
		{
			return string;
		}
		if (*string == 0)
		{
			return nullptr;
		}
	}
}

CHAR16* wcschr(CHAR16* string, CHAR16 sought) noexcept
{
	return const_cast<CHAR16*>(wcschr(
		static_cast<const CHAR16*>(string), sought));
}

const CHAR16* wcsrchr(const CHAR16* string, CHAR16 sought) noexcept
{
	const CHAR16* result = nullptr;
	do
	{
		if (*string == sought)
		{
			result = string;
		}
	}
	while (*string++ != 0);
	return result;
}

CHAR16* wcsrchr(CHAR16* string, CHAR16 sought) noexcept
{
	return const_cast<CHAR16*>(wcsrchr(
		static_cast<const CHAR16*>(string), sought));
}

std::size_t wcscspn(const CHAR16* string, const CHAR16* rejected) noexcept
{
	const CHAR16* cursor = string;
	while (*cursor != 0 && wcschr(rejected, *cursor) == nullptr)
	{
		++cursor;
	}
	return static_cast<std::size_t>(cursor - string);
}

CHAR16* wcstok(CHAR16* string, const CHAR16* delimiters) noexcept
{
	static thread_local CHAR16* next = nullptr;
	CHAR16* cursor = string != nullptr ? string : next;
	if (cursor == nullptr)
	{
		return nullptr;
	}
	while (*cursor != 0 && wcschr(delimiters, *cursor) != nullptr)
	{
		++cursor;
	}
	if (*cursor == 0)
	{
		next = nullptr;
		return nullptr;
	}
	CHAR16* token = cursor;
	while (*cursor != 0 && wcschr(delimiters, *cursor) == nullptr)
	{
		++cursor;
	}
	if (*cursor != 0)
	{
		*cursor++ = 0;
	}
	next = cursor;
	return token;
}

namespace
{

enum class LengthModifier
{
	none,
	hh,
	h,
	l,
	ll,
	z,
	t,
	j,
	longDouble
};

void appendNarrow(std::u16string& output, const char* text)
{
	if (text == nullptr)
	{
		text = "(null)";
	}
	while (*text != 0)
	{
		output.push_back(static_cast<unsigned char>(*text++));
	}
}

template <typename Value>
void appendFormatted(std::u16string& output, const std::string& format,
	Value value)
{
	const int required = std::snprintf(nullptr, 0, format.c_str(), value);
	if (required < 0)
	{
		return;
	}
	std::string buffer(static_cast<std::size_t>(required) + 1, '\0');
	std::snprintf(buffer.data(), buffer.size(), format.c_str(), value);
	appendNarrow(output, buffer.c_str());
}

void appendPadding(std::u16string& output, std::size_t count, CHAR16 value)
{
	output.append(count, value);
}

void appendString(std::u16string& output, std::u16string value, int width,
	int precision, bool leftAligned)
{
	if (precision >= 0 && value.size() > static_cast<std::size_t>(precision))
	{
		value.resize(static_cast<std::size_t>(precision));
	}
	const std::size_t padding = width > 0 &&
		static_cast<std::size_t>(width) > value.size()
		? static_cast<std::size_t>(width) - value.size() : 0;
	if (!leftAligned)
	{
		appendPadding(output, padding, u' ');
	}
	output += value;
	if (leftAligned)
	{
		appendPadding(output, padding, u' ');
	}
}

}

int vswprintf(
	CHAR16* destination, const CHAR16* format, std::va_list arguments)
{
	std::u16string output;
	for (const CHAR16* cursor = format; *cursor != 0; ++cursor)
	{
		if (*cursor != u'%')
		{
			output.push_back(*cursor);
			continue;
		}
		++cursor;
		if (*cursor == u'%')
		{
			output.push_back(u'%');
			continue;
		}

		std::string narrowFormat{"%"};
		bool leftAligned = false;
		while (*cursor == u'-' || *cursor == u'+' || *cursor == u' ' ||
			*cursor == u'#' || *cursor == u'0')
		{
			leftAligned = leftAligned || *cursor == u'-';
			narrowFormat.push_back(static_cast<char>(*cursor++));
		}
		int width = -1;
		if (*cursor == u'*')
		{
			width = va_arg(arguments, int);
			if (width < 0)
			{
				leftAligned = true;
				width = -width;
				narrowFormat.push_back('-');
			}
			narrowFormat += std::to_string(width);
			++cursor;
		}
		else
		{
			width = 0;
			while (*cursor >= u'0' && *cursor <= u'9')
			{
				width = width * 10 + static_cast<int>(*cursor - u'0');
				narrowFormat.push_back(static_cast<char>(*cursor++));
			}
		}

		int precision = -1;
		if (*cursor == u'.')
		{
			narrowFormat.push_back('.');
			++cursor;
			if (*cursor == u'*')
			{
				precision = va_arg(arguments, int);
				narrowFormat += std::to_string(std::max(precision, 0));
				++cursor;
			}
			else
			{
				precision = 0;
				while (*cursor >= u'0' && *cursor <= u'9')
				{
					precision = precision * 10 +
						static_cast<int>(*cursor - u'0');
					narrowFormat.push_back(static_cast<char>(*cursor++));
				}
			}
		}

		LengthModifier length = LengthModifier::none;
		if (*cursor == u'h')
		{
			length = *++cursor == u'h' ? LengthModifier::hh : LengthModifier::h;
			if (length == LengthModifier::hh) ++cursor;
		}
		else if (*cursor == u'l')
		{
			length = *++cursor == u'l' ? LengthModifier::ll : LengthModifier::l;
			if (length == LengthModifier::ll) ++cursor;
		}
		else if (*cursor == u'I' && cursor[1] == u'6' && cursor[2] == u'4')
		{
			length = LengthModifier::ll;
			cursor += 3;
		}
		else if (*cursor == u'z') { length = LengthModifier::z; ++cursor; }
		else if (*cursor == u't') { length = LengthModifier::t; ++cursor; }
		else if (*cursor == u'j') { length = LengthModifier::j; ++cursor; }
		else if (*cursor == u'L') { length = LengthModifier::longDouble; ++cursor; }

		const char conversion = static_cast<char>(*cursor);
		if (conversion == 's' || conversion == 'S')
		{
			std::u16string value;
			const bool narrow = conversion == 'S' || length == LengthModifier::h;
			if (narrow)
			{
				const char* text = va_arg(arguments, const char*);
				if (text == nullptr) text = "(null)";
				while (*text != 0)
					value.push_back(static_cast<unsigned char>(*text++));
			}
			else
			{
				const CHAR16* text = va_arg(arguments, const CHAR16*);
				value = text != nullptr ? text : u"(null)";
			}
			appendString(output, std::move(value), width, precision, leftAligned);
			continue;
		}
		if (conversion == 'c' || conversion == 'C')
		{
			std::u16string value(1,
				static_cast<CHAR16>(va_arg(arguments, int)));
			appendString(output, std::move(value), width, precision, leftAligned);
			continue;
		}

		if (length == LengthModifier::hh) narrowFormat += "hh";
		else if (length == LengthModifier::h) narrowFormat += 'h';
		else if (length == LengthModifier::l) narrowFormat += 'l';
		else if (length == LengthModifier::ll) narrowFormat += "ll";
		else if (length == LengthModifier::z) narrowFormat += 'z';
		else if (length == LengthModifier::t) narrowFormat += 't';
		else if (length == LengthModifier::j) narrowFormat += 'j';
		else if (length == LengthModifier::longDouble) narrowFormat += 'L';
		narrowFormat.push_back(conversion);

		switch (conversion)
		{
			case 'd': case 'i':
				if (length == LengthModifier::ll)
					appendFormatted(output, narrowFormat, va_arg(arguments, long long));
				else if (length == LengthModifier::l)
					appendFormatted(output, narrowFormat, va_arg(arguments, long));
				else if (length == LengthModifier::z || length == LengthModifier::t)
					appendFormatted(output, narrowFormat, va_arg(arguments, std::ptrdiff_t));
				else
					appendFormatted(output, narrowFormat, va_arg(arguments, int));
				break;
			case 'u': case 'o': case 'x': case 'X':
				if (length == LengthModifier::ll)
					appendFormatted(output, narrowFormat, va_arg(arguments, unsigned long long));
				else if (length == LengthModifier::l)
					appendFormatted(output, narrowFormat, va_arg(arguments, unsigned long));
				else if (length == LengthModifier::z)
					appendFormatted(output, narrowFormat, va_arg(arguments, std::size_t));
				else
					appendFormatted(output, narrowFormat, va_arg(arguments, unsigned int));
				break;
			case 'f': case 'F': case 'e': case 'E': case 'g': case 'G':
			case 'a': case 'A':
				if (length == LengthModifier::longDouble)
					appendFormatted(output, narrowFormat, va_arg(arguments, long double));
				else
					appendFormatted(output, narrowFormat, va_arg(arguments, double));
				break;
			case 'p':
				appendFormatted(output, narrowFormat, va_arg(arguments, void*));
				break;
			case 'n':
				*va_arg(arguments, int*) = static_cast<int>(output.size());
				break;
			default:
				output.push_back(u'%');
				output.push_back(static_cast<CHAR16>(conversion));
				break;
		}
	}
	std::copy(output.begin(), output.end(), destination);
	destination[output.size()] = 0;
	return output.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())
		? -1 : static_cast<int>(output.size());
}

int swprintf(CHAR16* destination, const CHAR16* format, ...)
{
	std::va_list arguments;
	va_start(arguments, format);
	const int result = vswprintf(destination, format, arguments);
	va_end(arguments);
	return result;
}

int swscanf(const CHAR16* input, const CHAR16* format, ...)
{
	std::string narrowInput;
	std::string narrowFormat;
	while (*input != 0)
	{
		narrowInput.push_back(static_cast<char>(*input++));
	}
	while (*format != 0)
	{
		narrowFormat.push_back(static_cast<char>(*format++));
	}
	std::va_list arguments;
	va_start(arguments, format);
	const int result = std::vsscanf(
		narrowInput.c_str(), narrowFormat.c_str(), arguments);
	va_end(arguments);
	return result;
}

int _wtoi(const CHAR16* input) noexcept
{
	while (*input == u' ' || (*input >= u'\t' && *input <= u'\r')) ++input;
	int sign = 1;
	if (*input == u'-')
	{
		sign = -1;
		++input;
	}
	else if (*input == u'+')
	{
		++input;
	}

	int value = 0;
	while (*input >= u'0' && *input <= u'9')
	{
		value = value * 10 + static_cast<int>(*input - u'0');
		++input;
	}
	return sign * value;
}

CHAR16* _itow(int value, CHAR16* output, int radix) noexcept
{
	if (radix < 2 || radix > 36)
	{
		output[0] = 0;
		return output;
	}

	CHAR16 reversed[35];
	std::size_t length = 0;
	const bool negative = value < 0 && radix == 10;
	unsigned int magnitude = negative
		? 0u - static_cast<unsigned int>(value)
		: static_cast<unsigned int>(value);
	do
	{
		const unsigned int digit = magnitude % static_cast<unsigned int>(radix);
		reversed[length++] = static_cast<CHAR16>(digit < 10 ? u'0' + digit : u'a' + digit - 10);
		magnitude /= static_cast<unsigned int>(radix);
	} while (magnitude != 0);
	if (negative) reversed[length++] = u'-';

	for (std::size_t i = 0; i < length; ++i) output[i] = reversed[length - i - 1];
	output[length] = 0;
	return output;
}

#endif
