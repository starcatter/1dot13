#include "application/ShutdownOnce.h"

namespace ja2
{
namespace application
{
bool ShutdownOnce::begin() noexcept
{
	State expected = State::ready;
	return state_.compare_exchange_strong(
		expected, State::inProgress,
		std::memory_order_acq_rel, std::memory_order_acquire);
}

void ShutdownOnce::complete() noexcept
{
	State expected = State::inProgress;
	state_.compare_exchange_strong(
		expected, State::complete,
		std::memory_order_release, std::memory_order_relaxed);
}

ShutdownOnce::State ShutdownOnce::state() const noexcept
{
	return state_.load(std::memory_order_acquire);
}
}
}
