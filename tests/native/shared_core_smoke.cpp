#include "fileio/DurableFileOperationsFactory.h"
#include "fileio/PhysicalWritableStore.h"
#include "fileio/PlatformPaths.h"
#include "platform/Clock.h"
#include "platform/Sleep.h"
#include "platform/Thread.h"
#include "timing/MainLoopScheduler.h"

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{
	using namespace std::chrono_literals;
	namespace fs = std::filesystem;

	class TemporaryDirectory
	{
	public:
		TemporaryDirectory()
		{
			const std::string leaf = "ja2-shared-core-" +
				std::to_string(Platform::GetClockMicroseconds());
			path_ = fs::temp_directory_path() / leaf;
			if (!fs::create_directory(path_))
				throw std::runtime_error("could not create unique smoke-test directory");
		}

		~TemporaryDirectory()
		{
			std::error_code error;
			fs::remove_all(path_, error);
		}

		const fs::path& path() const { return path_; }

	private:
		fs::path path_;
	};

	bool check(bool condition, const char* message)
	{
		if (!condition)
			std::cerr << "FAIL: " << message << '\n';
		return condition;
	}
}

int main()
{
	const std::uint64_t beforeSleep = Platform::GetClockMilliseconds64();
	Platform::Sleep(2);
	if (!check(Platform::GetClockMilliseconds64() >= beforeSleep,
		"monotonic clock moved backwards")) return 1;

	ja2::timing::MainLoopScheduler scheduler;
	scheduler.reset(Platform::GetClockMicroseconds(), 10'000);

	auto completed = std::make_shared<std::promise<void>>();
	std::future<void> completion = completed->get_future();
	if (!check(Platform::RunDetached([completed]() { completed->set_value(); }),
		"detached task did not start") ||
		!check(completion.wait_for(2s) == std::future_status::ready,
			"detached task did not finish")) return 1;

	TemporaryDirectory temporary;
	ja2::fileio::PhysicalWritableStore store(temporary.path());
	{
		std::unique_ptr<ja2::fileio::File> file = store.create("probe.bin");
		file->writeExact("JA2", 3);
		file->flush();
	}
	auto durable = ja2::fileio::makeDurableFileOperations(store);
	durable->syncFile("probe.bin");
	if (!check(store.metadata("probe.bin").size == 3,
		"physical writable store returned incorrect metadata")) return 1;
	if (!check(!ja2::fileio::executableDirectory().empty(),
		"native executable directory is empty")) return 1;

	std::cout << "native shared-core smoke test passed\n";
	return 0;
}
