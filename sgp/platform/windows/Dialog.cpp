#include "platform/Dialog.h"

#include "UtfConversion.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace Platform
{
namespace
{
UINT IconFlags(DialogKind kind)
{
	switch (kind)
	{
		case DialogKind::information: return MB_ICONINFORMATION;
		case DialogKind::warning: return MB_ICONWARNING;
		case DialogKind::error: return MB_ICONERROR;
	}
	return 0;
}
}

void ShowDialog(
	std::string_view titleUtf8, std::string_view messageUtf8, DialogKind kind) noexcept
{
	try
	{
		const ja2::text::Utf16String title =
			ja2::text::utf8ToUtf16ReplacingInvalid(titleUtf8);
		const ja2::text::Utf16String message =
			ja2::text::utf8ToUtf16ReplacingInvalid(messageUtf8);
		MessageBoxW(GetActiveWindow(), message.c_str(), title.c_str(),
			MB_OK | MB_TASKMODAL | IconFlags(kind));
	}
	catch (...)
	{
	}
}

bool AskYesNo(std::string_view titleUtf8, std::string_view messageUtf8) noexcept
{
	try
	{
		const ja2::text::Utf16String title =
			ja2::text::utf8ToUtf16ReplacingInvalid(titleUtf8);
		const ja2::text::Utf16String message =
			ja2::text::utf8ToUtf16ReplacingInvalid(messageUtf8);
		return MessageBoxW(GetActiveWindow(), message.c_str(), title.c_str(),
			MB_YESNO | MB_TASKMODAL | MB_ICONQUESTION) == IDYES;
	}
	catch (...)
	{
		return false;
	}
}
}
