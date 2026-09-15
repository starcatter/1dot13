#include "platform/Sleep.h"

#include <chrono>
#include <thread>

void Platform::Sleep(std::uint32_t milliseconds)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}
