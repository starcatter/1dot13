#pragma once

#include <string_view>

namespace sgp
{
	// Startup-only telemetry. The current transport is available on Windows;
	// native hosts explicitly report unavailable and perform no network access.
	bool crashTelemetryAvailable() noexcept;
	void processCrashTelemetry(std::string_view urlUtf8);
}
