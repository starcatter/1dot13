#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace Platform
{
	using ProcessArguments = std::vector<std::string>;

	struct CommandLineProperty
	{
		std::string key;
		std::string value;
	};

	enum class WindowShowMode
	{
		hidden,
		normal,
		minimized,
		maximized
	};

	enum class RelaunchResult
	{
		launched,
		failed,
		unavailable
	};

	enum class ExistingInstanceResult
	{
		notFound,
		activated,
		unavailable
	};

	// UTF-8 arguments including argv[0].
	ProcessArguments GetProcessArguments();

	// Extracts -KEY=value, /KEY:value, and -KEY value properties. Bare
	// switches are ignored, matching the legacy VFS property overlay.
	std::vector<CommandLineProperty> ParseCommandLineProperties(
		const ProcessArguments& arguments);

	// Relaunches the current executable with its current arguments. Some native
	// hosts do not need these compatibility policies and report unavailable.
	RelaunchResult RelaunchCurrentProcess(WindowShowMode showMode) noexcept;
	ExistingInstanceResult ActivateExistingInstance(
		std::string_view applicationNameUtf8) noexcept;
}
