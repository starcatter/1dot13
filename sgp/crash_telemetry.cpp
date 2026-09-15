// Crash telemetry: upload the crash_report_*.txt files the crash handler left
// behind, on the next launch.
//
// Deliberately a separate translation unit from crash_report.cpp. Everything in
// there runs inside a faulting thread and may not allocate; everything here runs at
// startup with a healthy heap and is ordinary code. Keeping the two apart keeps the
// no-heap rule easy to see and easy to hold.

#if defined(_MSC_VER)

#include "crash_report.h"
#include "fileio/FileIO.h"
#include "fileio/PhysicalWritableStore.h"
#include "platform/Thread.h"

#include <windows.h>
#include <winhttp.h>

#include <cstring> // strstr
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace {

using TelemetryStore = ja2::fileio::PhysicalWritableStore;

// Persisted consent: 1 = yes, 0 = no, -1 = not asked yet.
int readConsent(TelemetryStore& store) {
	try {
		std::unique_ptr<ja2::fileio::File> file = store.openRead("telemetry.consent");
		char c = 0;
		return (file->read(&c, 1) == 1 && c == '1') ? 1 : 0;
	} catch (...) {
		return -1;
	}
}

void writeConsent(TelemetryStore& store, bool yes) {
	try {
		std::unique_ptr<ja2::fileio::File> file = store.create("telemetry.consent");
		file->writeExact(yes ? "1" : "0", 1);
	} catch (...) {
	}
}

// A report bigger than this is not one of ours; never put it on the wire. Kept
// equal to MAX_BYTES in the sink, which answers a settling 400 above it: sending
// what the sink will not take is how an upload destroys the report it carries.
const DWORD kMaxReportBytes = 32 * 1024;

// POST one report file to url. Returns the HTTP status, or 0 if the request never
// completed (no connection, DNS failure, timeout) — see reportIsSettled().
DWORD postReport(TelemetryStore& store, const wchar_t* url, const char* path) {
	std::vector<char> body;
	DWORD size = 0;
	try {
		std::unique_ptr<ja2::fileio::File> file = store.openRead(path);
		if (file->size() > kMaxReportBytes) return 413;
		size = static_cast<DWORD>(file->size());
		body.resize(size ? size : 1);
		file->readExact(body.data(), size);
	} catch (...) {
		return 0;
	}

	URL_COMPONENTS uc = {}; uc.dwStructSize = sizeof(uc);
	wchar_t host[256] = {}, urlpath[1024] = {};
	uc.lpszHostName = host;    uc.dwHostNameLength = _countof(host);
	uc.lpszUrlPath = urlpath;  uc.dwUrlPathLength  = _countof(urlpath);
	if (!WinHttpCrackUrl(url, 0, 0, &uc)) return 400;

	HINTERNET hSession = WinHttpOpen(L"JA2-1.13-crash-telemetry",
		WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) return 0;
	// Bound every phase. Even off the main thread these must not hang forever: the
	// thread holds the report files open-ish and we want the queue drained or given
	// up on within seconds, not to leave a socket parked for the whole session.
	WinHttpSetTimeouts(hSession, 5000, 5000, 10000, 15000);

	DWORD status = 0;
	if (HINTERNET hConnect = WinHttpConnect(hSession, host, uc.nPort, 0)) {
		DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
		if (HINTERNET hReq = WinHttpOpenRequest(hConnect, L"POST", urlpath, NULL,
				WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags)) {
			if (WinHttpSendRequest(hReq, L"Content-Type: text/plain\r\n", (DWORD)-1,
					body.data(), size, size, 0) &&
				WinHttpReceiveResponse(hReq, NULL)) {
				DWORD len = sizeof(status);
				WinHttpQueryHeaders(hReq,
					WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
					WINHTTP_HEADER_NAME_BY_INDEX, &status, &len, WINHTTP_NO_HEADER_INDEX);
			}
			WinHttpCloseHandle(hReq);
		}
		WinHttpCloseHandle(hConnect);
	}
	WinHttpCloseHandle(hSession);
	return status;
}

// Whether a report is done with, i.e. safe to delete. Uploaded (2xx), or rejected
// as content the server will never take (a malformed or oversized body). Anything
// else — no connection, 5xx, and notably 404/403 from a mistyped or misconfigured
// CRASH_TELEMETRY_URL — keeps the file, so a bad setting loses nobody's report.
bool reportIsSettled(DWORD status) {
	return (status >= 200 && status < 300) ||
		status == 400 || status == 413 || status == 415;
}

// A report stamped "build local" comes from a developer build with no released
// PDB: nobody at the receiving end can symbolize it, so it never goes on the
// wire — and never gets reaped either, it is the developer's to delete.
bool isFromLocalBuild(TelemetryStore& store, const char* path) {
	try {
		std::unique_ptr<ja2::fileio::File> file = store.openRead(path);
		char head[160] = {};
		(void)file->read(head, sizeof(head) - 1);
		return strstr(head, "  build local") != NULL;
	} catch (...) {
		return false;
	}
}

// Reports older than this are stale: the crash they describe is long since shipped
// past, and a player who was offline for a season should not upload a season of them.
const DWORD kMaxReportAgeDays = 30;

bool olderThan(const ja2::fileio::Metadata& metadata, DWORD days) {
	if (!metadata.modifiedUnixNanoseconds) return false;
	const std::int64_t now = std::chrono::duration_cast<std::chrono::nanoseconds>(
		std::chrono::system_clock::now().time_since_epoch()).count();
	if (now <= *metadata.modifiedUnixNanoseconds) return false; // clock skew: treat as fresh
	const std::int64_t maximumAge = static_cast<std::int64_t>(days) * 24 * 60 * 60 * 1000000000LL;
	return now - *metadata.modifiedUnixNanoseconds > maximumAge;
}

// One launch drains at most this many, so a crash-looping build cannot turn startup
// into a long upload session. The rest wait for the next launch.
const int kMaxUploadsPerRun = 20;

// Drains the pending reports. Runs detached: if the player quits first the process
// exits from under it, which costs nothing — an interrupted upload leaves the file
// on disk and it goes out next launch.
void drainTelemetryReports(const std::filesystem::path& root, const std::wstring& url) {
	int sent = 0;
	try {
		TelemetryStore store(root);
		const std::vector<ja2::fileio::DirectoryEntry> reports =
			store.list("crash_report_*.txt");
		for (const ja2::fileio::DirectoryEntry& report : reports) {
			if (isFromLocalBuild(store, report.name.c_str())) continue;
			if (olderThan(report.metadata, kMaxReportAgeDays)) {
				store.remove(report.name);
				continue;
			}
			if (sent++ >= kMaxUploadsPerRun) break;
			if (reportIsSettled(postReport(store, url.c_str(), report.name.c_str())))
				store.remove(report.name);
		}
	} catch (...) {
		// Preserve every unsettled report and retry on the next launch.
	}
}

} // anonymous namespace

namespace sgp {
void processCrashTelemetry(const wchar_t* url) {
	if (url == NULL || url[0] == L'\0') return; // no endpoint configured: feature off

	try {
		const std::filesystem::path root = std::filesystem::current_path();
		TelemetryStore store(root);
		int consent = readConsent(store);
		if (consent < 0) { // first run: ask once, remember the answer
			int r = MessageBoxW(NULL,
				L"This build can send crash reports to the developers to help fix bugs.\n"
				L"A report contains where the game crashed, the names of the loaded\n"
				L"modules, and the HANDLE from your Ja2.ini if you set one. No file\n"
				L"paths, no save games, nothing else about your machine.\n\n"
				L"Send crash reports automatically?",
				L"Jagged Alliance 2 v1.13 \x2014 Crash Reporting",
				MB_YESNO | MB_ICONQUESTION);
			writeConsent(store, r == IDYES);
			consent = (r == IDYES) ? 1 : 0;
		}
		if (consent != 1) return; // declined: leave reports on disk, accumulating

		// Hand the draining to a detached thread. The uploads are synchronous WinHttp
		// calls with seconds-long timeouts, and this runs on the startup path: on the
		// main thread a slow or unreachable endpoint is a stall the player sees before
		// the splash screen. Nothing waits on the result, so it can take as long as it
		// takes. The consent prompt above stays here, on purpose — that one is a
		// question, and a question has to be asked before anything is sent.
		const std::wstring telemetryUrl(url);
		Platform::RunDetached([root, telemetryUrl]() {
			drainTelemetryReports(root, telemetryUrl);
		});
	} catch (...) {
		// Telemetry is best-effort and must never prevent game startup.
	}
}
} // namespace sgp

#endif // _MSC_VER
