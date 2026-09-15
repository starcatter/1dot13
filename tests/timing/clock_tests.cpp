#include "platform/Clock.h"
#include "timer.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
void require(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

std::uint64_t distance(std::uint64_t left, std::uint64_t right)
{
	return left > right ? left - right : right - left;
}

void testMonotonicAndCoherentUnits()
{
	std::uint64_t previous = Platform::GetClockMicroseconds();
	for (int sample = 0; sample < 10000; ++sample)
	{
		const std::uint64_t current = Platform::GetClockMicroseconds();
		require(current >= previous, "portable clock moved backward");
		previous = current;
	}

	const std::uint64_t microseconds = Platform::GetClockMicroseconds();
	const std::uint64_t milliseconds = Platform::GetClockMilliseconds64();
	require(distance(microseconds / 1000, milliseconds) <= 1,
		"microsecond and millisecond clocks disagree");
	require(Platform::GetClockMilliseconds() ==
		static_cast<std::uint32_t>(Platform::GetClockMilliseconds64()),
		"32-bit clock is not the wrapped 64-bit clock");
}

void testElapsedTimeMatchesSteadyClock()
{
	const auto referenceStart = std::chrono::steady_clock::now();
	const std::uint64_t portableStart = Platform::GetClockMicroseconds();
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	const std::uint64_t portableElapsed =
		Platform::GetClockMicroseconds() - portableStart;
	const auto referenceElapsed = std::chrono::duration_cast<std::chrono::microseconds>(
		std::chrono::steady_clock::now() - referenceStart).count();

	require(portableElapsed >= 20000 && portableElapsed <= 1000000,
		"portable elapsed time is outside scheduler tolerance");
	require(distance(portableElapsed, static_cast<std::uint64_t>(referenceElapsed)) <= 3000,
		"portable elapsed time differs from steady_clock");
}

void testLegacyDeadlineWraparound()
{
	require(Platform::ClockMillisecondsRemaining(100, 150) == 50,
		"ordinary deadline duration is wrong");
	require(Platform::ClockMillisecondsRemaining(150, 150) == 0,
		"deadline did not expire at equality");
	require(Platform::ClockMillisecondsRemaining(151, 150) == 0,
		"expired deadline retained time");
	require(Platform::ClockMillisecondsRemaining(0xfffffff0U, 0x10U) == 32,
		"deadline failed across 32-bit wraparound");
	require(Platform::ClockMillisecondsRemaining(0x20U, 0x10U) == 0,
		"wrapped deadline did not expire");
	require(Platform::ClockMillisecondsRemaining(0, 0x80000000U) == 0,
		"ambiguous half-range deadline was not treated as expired");
}

void testLegacyClockManagerContract()
{
	require(InitializeClockManager(), "clock manager initialization failed");
	const TIMER start = GetClock();
	const TIMER deadline = SetCountdownClock(25);
	const std::uint32_t initialRemaining = ClockIsTicking(deadline);
	require(initialRemaining > 0 && initialRemaining <= 25,
		"countdown clock did not start with the requested delay");
	std::this_thread::sleep_for(std::chrono::milliseconds(35));
	require(GetClock() - start >= 25, "legacy clock manager advanced too slowly");
	require(ClockIsTicking(deadline) == 0, "legacy countdown did not expire");
	ShutdownClockManager();
}

#ifdef _WIN32
void testWindowsReferenceClocks()
{
	LARGE_INTEGER frequency{};
	LARGE_INTEGER qpcStart{};
	require(QueryPerformanceFrequency(&frequency) != 0, "QPC frequency unavailable");
	require(QueryPerformanceCounter(&qpcStart) != 0, "QPC unavailable");
	const ULONGLONG tickStart = GetTickCount64();
	const std::uint64_t portableStart = Platform::GetClockMicroseconds();
	std::this_thread::sleep_for(std::chrono::milliseconds(30));
	LARGE_INTEGER qpcEnd{};
	require(QueryPerformanceCounter(&qpcEnd) != 0, "QPC unavailable after delay");
	const std::uint64_t portableElapsed =
		Platform::GetClockMicroseconds() - portableStart;
	const std::uint64_t qpcElapsed = static_cast<std::uint64_t>(
		(qpcEnd.QuadPart - qpcStart.QuadPart) * 1000000LL / frequency.QuadPart);
	const std::uint64_t tickElapsed = (GetTickCount64() - tickStart) * 1000ULL;
	require(distance(portableElapsed, qpcElapsed) <= 3000,
		"portable clock differs from QueryPerformanceCounter");
	require(distance(portableElapsed, tickElapsed) <= 5000,
		"portable clock differs from GetTickCount64");
}
#endif
}

int main()
{
	try
	{
		testMonotonicAndCoherentUnits();
		testElapsedTimeMatchesSteadyClock();
		testLegacyDeadlineWraparound();
		testLegacyClockManagerContract();
#ifdef _WIN32
		testWindowsReferenceClocks();
#endif
		std::cout << "portable clock tests passed\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "portable clock tests failed: " << error.what() << '\n';
		return 1;
	}
}
