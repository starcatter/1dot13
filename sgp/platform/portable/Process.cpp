#include "platform/Process.h"

#include <fstream>
#include <iterator>

namespace Platform
{
ProcessArguments GetProcessArguments()
{
#ifdef __linux__
	std::ifstream commandLine("/proc/self/cmdline", std::ios::binary);
	const std::string bytes(
		(std::istreambuf_iterator<char>(commandLine)), std::istreambuf_iterator<char>());

	ProcessArguments arguments;
	std::size_t begin = 0;
	while (begin < bytes.size())
	{
		const std::size_t end = bytes.find('\0', begin);
		if (end == std::string::npos)
		{
			arguments.emplace_back(bytes.substr(begin));
			break;
		}
		arguments.emplace_back(bytes.substr(begin, end - begin));
		begin = end + 1;
	}
	return arguments;
#else
	return {};
#endif
}

RelaunchResult RelaunchCurrentProcess(WindowShowMode) noexcept
{
	return RelaunchResult::unavailable;
}

ExistingInstanceResult ActivateExistingInstance(std::string_view) noexcept
{
	return ExistingInstanceResult::unavailable;
}
}
