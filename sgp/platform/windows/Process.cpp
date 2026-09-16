#include "platform/Process.h"

#include "UtfConversion.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <cwchar>
#include <vector>

namespace Platform
{
namespace
{
std::vector<wchar_t> ExecutablePath()
{
	std::vector<wchar_t> path(256);
	for (;;)
	{
		const DWORD length = GetModuleFileNameW(
			nullptr, path.data(), static_cast<DWORD>(path.size()));
		if (length == 0)
			return {};
		if (length < path.size() - 1)
		{
			path.resize(length + 1);
			return path;
		}
		if (path.size() >= 32768)
			return {};
		path.resize(path.size() * 2);
	}
}

WORD ToNativeShowMode(WindowShowMode mode)
{
	switch (mode)
	{
		case WindowShowMode::hidden: return SW_HIDE;
		case WindowShowMode::minimized: return SW_SHOWMINIMIZED;
		case WindowShowMode::maximized: return SW_SHOWMAXIMIZED;
		case WindowShowMode::normal: return SW_SHOWNORMAL;
	}
	return SW_SHOWNORMAL;
}
}

ProcessArguments GetProcessArguments()
{
	int argumentCount = 0;
	wchar_t** nativeArguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount);
	if (nativeArguments == nullptr)
		return {};

	ProcessArguments arguments;
	try
	{
		arguments.reserve(static_cast<std::size_t>(argumentCount));
		for (int index = 0; index < argumentCount; ++index)
		{
			arguments.push_back(ja2::text::utf16ToUtf8(
				ja2::text::Utf16View(nativeArguments[index], std::wcslen(nativeArguments[index]))));
		}
	}
	catch (...)
	{
		LocalFree(nativeArguments);
		throw;
	}
	LocalFree(nativeArguments);
	return arguments;
}

RelaunchResult RelaunchCurrentProcess(WindowShowMode showMode) noexcept
{
	try
	{
		std::vector<wchar_t> executable = ExecutablePath();
		if (executable.empty())
			return RelaunchResult::failed;

		const wchar_t* currentCommandLine = GetCommandLineW();
		std::vector<wchar_t> commandLine(
			currentCommandLine, currentCommandLine + std::wcslen(currentCommandLine) + 1);

		STARTUPINFOW startup{};
		startup.cb = sizeof(startup);
		startup.dwFlags = STARTF_USESHOWWINDOW;
		startup.wShowWindow = ToNativeShowMode(showMode);
		PROCESS_INFORMATION process{};
		if (!CreateProcessW(executable.data(), commandLine.data(), nullptr, nullptr, FALSE,
			0, nullptr, nullptr, &startup, &process))
		{
			return RelaunchResult::failed;
		}
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
		return RelaunchResult::launched;
	}
	catch (...)
	{
		return RelaunchResult::failed;
	}
}

ExistingInstanceResult ActivateExistingInstance(
	std::string_view applicationNameUtf8) noexcept
{
	try
	{
		const ja2::text::Utf16String applicationName =
			ja2::text::utf8ToUtf16(applicationNameUtf8);
		HWND window = FindWindowExW(
			nullptr, nullptr, applicationName.c_str(), applicationName.c_str());
		if (window == nullptr)
			return ExistingInstanceResult::notFound;

		SetForegroundWindow(window);
		ShowWindow(window, SW_RESTORE);
		return ExistingInstanceResult::activated;
	}
	catch (...)
	{
		return ExistingInstanceResult::notFound;
	}
}
}
