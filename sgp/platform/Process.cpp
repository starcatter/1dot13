#include "platform/Process.h"

#include <utility>

namespace Platform
{
namespace
{
bool IsOption(const std::string& argument)
{
	return !argument.empty() && (argument.front() == '-' || argument.front() == '/');
}
}

std::vector<CommandLineProperty> ParseCommandLineProperties(
	const ProcessArguments& arguments)
{
	std::vector<CommandLineProperty> properties;
	for (std::size_t index = 1; index < arguments.size(); ++index)
	{
		const std::string& argument = arguments[index];
		if (!IsOption(argument))
			continue;

		const std::size_t separator = argument.find_first_of(":=", 1);
		const bool hasSeparator = separator != std::string::npos;
		const std::string key = argument.substr(1, hasSeparator ? separator - 1 : std::string::npos);
		std::string value = hasSeparator ? argument.substr(separator + 1) : std::string{};

		if ((!hasSeparator || value.empty()) && index + 1 < arguments.size() &&
			!IsOption(arguments[index + 1]))
		{
			value = arguments[++index];
			properties.push_back({key, std::move(value)});
		}
		else if (hasSeparator)
		{
			properties.push_back({key, std::move(value)});
		}
	}
	return properties;
}
}
