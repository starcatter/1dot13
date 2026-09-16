#include "platform/Process.h"
#include "platform/Dialog.h"
#include "crash_telemetry.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
[[noreturn]] void Fail(const std::string& message)
{
	std::cerr << "FAIL: " << message << '\n';
	std::exit(1);
}

void Require(bool condition, const std::string& message)
{
	if (!condition)
		Fail(message);
}
}

int main()
{
	const Platform::ProcessArguments input{
		"ja2", "-LANGUAGE=Polish", "/VFS_CONFIG", "portable.ini",
		"-BARE", "/NEXT:value", "loose", "-EMPTY=", "", "-UNICODE", "zażółć"};
	const auto properties = Platform::ParseCommandLineProperties(input);
	Require(properties.size() == 5, "property count");
	Require(properties[0].key == "LANGUAGE" && properties[0].value == "Polish",
		"equals property");
	Require(properties[1].key == "VFS_CONFIG" && properties[1].value == "portable.ini",
		"separate property value");
	Require(properties[2].key == "NEXT" && properties[2].value == "value",
		"colon property");
	Require(properties[3].key == "EMPTY" && properties[3].value.empty(),
		"explicit empty property");
	Require(properties[4].key == "UNICODE" && properties[4].value == "zażółć",
		"UTF-8 property");

	const Platform::ProcessArguments current = Platform::GetProcessArguments();
	Require(!current.empty(), "current process has argv[0]");
	Require(!current.front().empty(), "current process argv[0] is non-empty");
	for (const std::string& argument : current)
		Require(argument.find('\0') == std::string::npos, "argument contains no embedded NUL");

	Require(Platform::RelaunchCurrentProcess(Platform::WindowShowMode::normal) ==
		Platform::RelaunchResult::unavailable, "portable restart policy is explicit");
	Require(Platform::ActivateExistingInstance("Jagged Alliance 2 v1.13") ==
		Platform::ExistingInstanceResult::unavailable,
		"portable single-instance policy is explicit");
	Require(!Platform::AskYesNo("JA2 test", "portable question policy"),
		"portable questions default to no");
	Require(!sgp::crashTelemetryAvailable(), "portable telemetry policy is explicit");
	sgp::processCrashTelemetry("https://example.invalid/crashes");

	std::cout << "process service tests passed\n";
	return 0;
}
