/// @file
/// @brief PseudoTerminal, once for Windows (a pseudo console, ConPTY, and a job object) and once for POSIX
/// (forkpty). Neither goes through a shell.

#include "process/Pty.hpp"

#include <algorithm>
#include <array>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace affresco::services {

namespace {

/// @brief An environment with TERM set, unless it sets TERM itself.
/// @param entries "NAME=value" entries.
/// @return Them, and TERM=xterm-256color if none was TERM.
std::vector<std::string> withTerm(std::vector<std::string> entries) {
	const bool named =
	    std::ranges::any_of(entries, [](const std::string& entry) { return entry.starts_with("TERM="); });
	if(!named) entries.emplace_back("TERM=xterm-256color");
	return entries;
}

} // namespace

} // namespace affresco::services

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <cstddef>
#include <cwchar>

namespace affresco::services {

namespace {

/// @brief Closes a handle if it is open, and forgets it.
/// @param handle The handle; nullptr afterwards.
void closeHandle(HANDLE& handle) noexcept {
	if(handle && handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
	handle = nullptr;
}

/// @brief Converts UTF-8 to the UTF-16 Windows takes.
/// @param text UTF-8 text.
/// @return The text in UTF-16.
/// @throws std::runtime_error If @p text isn't valid UTF-8.
std::wstring widen(std::string_view text) {
	if(text.empty()) return {};
	const int size = static_cast<int>(text.size());
	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
	if(length <= 0) throw std::runtime_error("an argument isn't valid UTF-8");
	std::wstring wide(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, wide.data(), length);
	return wide;
}

/// @brief Quotes one argument the way CommandLineToArgvW and the C runtime read it back (as Process.cpp does).
/// @param argument The argument, in UTF-16.
/// @return It, quoted.
std::wstring quote(const std::wstring& argument) {
	std::wstring quoted = L"\"";
	std::size_t backslashes = 0;
	for(const wchar_t character : argument) {
		if(character == L'\\') {
			++backslashes;
			continue;
		}
		quoted.append(character == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
		backslashes = 0;
		quoted += character;
	}
	quoted.append(backslashes * 2, L'\\');
	return quoted + L"\"";
}

/// @brief The environment block CreateProcessW takes, with TERM set.
/// @param launch The launch: its environment, or the host's.
/// @return The block.
std::wstring environmentBlock(const Launch& launch) {
	std::vector<std::string> entries;
	if(launch.environment) entries = *launch.environment;
	else if(wchar_t* host = GetEnvironmentStringsW()) {
		std::wstring block;
		for(const wchar_t* entry = host; *entry; entry += std::wcslen(entry) + 1) block += std::wstring(entry) + L'\0';
		FreeEnvironmentStringsW(host);
		bool named = false;
		for(std::size_t at = 0; at < block.size(); at = block.find(L'\0', at) + 1)
			named |= block.compare(at, 5, L"TERM=") == 0;
		return named ? block + L'\0' : block + L"TERM=xterm-256color" + L'\0' + L'\0';
	}
	std::wstring block;
	for(const std::string& entry : withTerm(std::move(entries))) block += widen(entry) + L'\0';
	return block + L'\0';
}

/// @brief A pseudo console's size.
/// @param size The size.
/// @return It, as ConPTY takes it.
COORD coordinates(TerminalSize size) noexcept {
	return {static_cast<SHORT>(size.columns), static_cast<SHORT>(size.rows)};
}

} // namespace

/// @brief The program, the job holding it, the pseudo console and the parent's ends of its pipes.
struct PseudoTerminal::Platform {
	/// @brief Closes every handle still open; closing the job ends anything still in it.
	~Platform() {
		if(console) ClosePseudoConsole(console);
		for(HANDLE* handle : {&toConsole, &fromConsole, &process, &job}) closeHandle(*handle);
	}
	Platform() = default;
	Platform(const Platform&) = delete;
	Platform& operator=(const Platform&) = delete;

	HPCON console = nullptr;      ///< The pseudo console.
	HANDLE job = nullptr;         ///< Ends every process in it when terminated or closed.
	HANDLE process = nullptr;     ///< The program.
	HANDLE toConsole = nullptr;   ///< What is typed.
	HANDLE fromConsole = nullptr; ///< What it shows.
	std::mutex lock;              ///< Guards console against end() while a resize runs.
	std::once_flag ended;         ///< end() runs once.
};

namespace {

/// @brief Starts the program on a pseudo console, suspended until it is in the job.
/// @param launch What to start.
/// @param platform The console and the job; receives the process.
/// @return Whether it started.
bool startOnConsole(const Launch& launch, PseudoTerminal::Platform& platform);

} // namespace

std::unique_ptr<PseudoTerminal> PseudoTerminal::start(const Launch& launch, TerminalSize size) {
	auto platform = std::make_unique<Platform>();
	HANDLE consoleReads = nullptr, consoleWrites = nullptr;
	const auto fail = [&](const std::string& what) {
		closeHandle(consoleReads);
		closeHandle(consoleWrites);
		return std::runtime_error(what);
	};
	if(!CreatePipe(&consoleReads, &platform->toConsole, nullptr, 0) ||
	   !CreatePipe(&platform->fromConsole, &consoleWrites, nullptr, 0))
		throw fail("could not create pipes");
	if(FAILED(CreatePseudoConsole(coordinates(size), consoleReads, consoleWrites, 0, &platform->console)))
		throw fail("could not create a pseudo console");
	platform->job = CreateJobObjectW(nullptr, nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if(!platform->job ||
	   !SetInformationJobObject(platform->job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
		throw fail("could not create a job object");
	if(!startOnConsole(launch, *platform)) throw fail("could not start '" + launch.program.filename().string() + "'");
	closeHandle(consoleReads);
	closeHandle(consoleWrites);
	return std::unique_ptr<PseudoTerminal>(new PseudoTerminal(std::move(platform)));
}

namespace {

bool startOnConsole(const Launch& launch, PseudoTerminal::Platform& platform) {
	std::wstring line = quote(launch.program.wstring());
	for(const std::string& argument : launch.arguments) line += L" " + quote(widen(argument));
	std::wstring environment = environmentBlock(launch);
	const std::wstring directory = launch.directory.wstring();
	SIZE_T size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
	std::vector<std::byte> storage(size);
	const auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
	if(!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) return false;
	const bool listed = UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, platform.console,
	                                              sizeof(platform.console), nullptr, nullptr) != 0;
	STARTUPINFOEXW startup{};
	startup.StartupInfo.cb = sizeof(startup);
	startup.lpAttributeList = attributes;
	const DWORD flags = CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT;
	PROCESS_INFORMATION started{};
	const bool created =
	    listed && CreateProcessW(launch.program.wstring().c_str(), line.data(), nullptr, nullptr, FALSE, flags,
		                         environment.data(), directory.empty() ? nullptr : directory.c_str(),
		                         &startup.StartupInfo, &started) != 0;
	DeleteProcThreadAttributeList(attributes);
	if(!created) return false;
	if(!AssignProcessToJobObject(platform.job, started.hProcess)) {
		TerminateProcess(started.hProcess, 1);
		WaitForSingleObject(started.hProcess, INFINITE);
		closeHandle(started.hProcess);
	} else {
		ResumeThread(started.hThread);
	}
	closeHandle(started.hThread);
	platform.process = started.hProcess;
	return platform.process != nullptr;
}

} // namespace

bool PseudoTerminal::write(std::string_view bytes) {
	for(std::size_t written = 0; written < bytes.size();) {
		const auto size = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - written, 1u << 20));
		DWORD count = 0;
		if(!WriteFile(platform_->toConsole, bytes.data() + written, size, &count, nullptr) || count == 0) return false;
		written += count;
	}
	return true;
}

std::size_t PseudoTerminal::read(char* buffer, std::size_t size) {
	DWORD count = 0;
	const auto chunk = static_cast<DWORD>(std::min<std::size_t>(size, 1u << 20));
	if(!ReadFile(platform_->fromConsole, buffer, chunk, &count, nullptr)) return 0;
	return count;
}

void PseudoTerminal::resize(TerminalSize size) noexcept {
	const std::scoped_lock guard(platform_->lock);
	if(platform_->console) ResizePseudoConsole(platform_->console, coordinates(size));
}

void PseudoTerminal::signal(TerminalSignal signal) noexcept {
	if(signal == TerminalSignal::Interrupt) (void)write("\x03");
	else TerminateJobObject(platform_->job, 1);
}

int PseudoTerminal::end() noexcept {
	std::call_once(platform_->ended, [this] {
		TerminateJobObject(platform_->job, 1);
		const std::scoped_lock guard(platform_->lock);
		ClosePseudoConsole(platform_->console); // its output pipe closes, so a blocked read returns
		platform_->console = nullptr;
	});
	return wait();
}

int PseudoTerminal::wait() noexcept {
	WaitForSingleObject(platform_->process, INFINITE);
	DWORD code = 0;
	return GetExitCodeProcess(platform_->process, &code) ? static_cast<int>(code) : -1;
}

} // namespace affresco::services

#else

#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <thread>
#include <unistd.h>
#ifdef __APPLE__
#include <util.h>
#else
#include <pty.h>
#endif

// The environment the program gets by default: POSIX's own variable, which not every C library's unistd.h declares
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,readability-redundant-declaration)
extern char** environ;

namespace affresco::services {

namespace {

/// @brief Closes a descriptor if it is open, and forgets it.
/// @param fd The descriptor; -1 afterwards.
void closeDescriptor(int& fd) noexcept {
	if(fd >= 0) close(fd);
	fd = -1;
}

/// @brief The exit code waitpid reported.
/// @param status Its status.
/// @return The code, or -1 if a signal ended the program.
int exitCode(int status) noexcept { return WIFEXITED(status) ? WEXITSTATUS(status) : -1; }

/// @brief The size as the kernel takes it.
/// @param size The size.
/// @return It.
winsize windowSize(TerminalSize size) noexcept { return {size.rows, size.columns, 0, 0}; }

/// @brief The words and environment entries a program is started with, and the pointers execve takes, made before
/// fork (the child may only call what is safe in a signal handler).
struct ExecArguments {
	std::string program;                  ///< The program's path.
	std::string directory;                ///< Where it starts, or empty.
	std::vector<std::string> words;       ///< Its name, then its arguments.
	std::vector<std::string> entries;     ///< Its environment.
	std::vector<char*> argv, environment; ///< Pointers into the two, each ended by nullptr.

	/// @brief Prepares a launch.
	/// @param launch The launch.
	explicit ExecArguments(const Launch& launch)
	    : program(launch.program.string()), directory(launch.directory.string()), words{program} {
		words.insert(words.end(), launch.arguments.begin(), launch.arguments.end());
		if(launch.environment) entries = *launch.environment;
		else
			for(char** entry = environ; *entry; ++entry) entries.emplace_back(*entry);
		entries = withTerm(std::move(entries));
		for(std::string& word : words) argv.push_back(word.data());
		argv.push_back(nullptr);
		for(std::string& entry : entries) environment.push_back(entry.data());
		environment.push_back(nullptr);
	}
};

/// @brief Closes every descriptor from one on, in a child after fork: closefrom where the C library has it, else one
/// by one.
/// @param first The first to close.
/// @param count How many a process may have open (sysconf(_SC_OPEN_MAX), read before fork), or -1 if unknown.
void closeFrom(int first, long count) noexcept {
#if defined(__GLIBC__) && (__GLIBC__ > 2 || __GLIBC_MINOR__ >= 34)
	(void)count;
	closefrom(first);
#else
	for(long fd = first; fd < (count > 0 ? count : 1024); ++fd) close(static_cast<int>(fd));
#endif
}

} // namespace

/// @brief The program, the leader of a session of its own, the terminal's parent end, and a pipe that wakes a read.
struct PseudoTerminal::Platform {
	/// @brief Closes the descriptors still open.
	~Platform() {
		for(int* fd : {&terminal, &wakeRead, &wakeWrite}) closeDescriptor(*fd);
	}
	Platform() = default;
	Platform(const Platform&) = delete;
	Platform& operator=(const Platform&) = delete;

	pid_t pid = -1;      ///< The program; also its session's and process group's id.
	int terminal = -1;   ///< The terminal's parent end.
	int wakeRead = -1;   ///< Readable once end() was called.
	int wakeWrite = -1;  ///< Written by end().
	std::mutex lock;     ///< Guards reaped: a reaped program's id may be reused, so it is never signalled after.
	bool reaped = false; ///< Whether waitpid returned the program.
	int code = -1;       ///< Its exit code, once reaped.
};

std::unique_ptr<PseudoTerminal> PseudoTerminal::start(const Launch& launch, TerminalSize size) {
	std::signal(SIGPIPE, SIG_IGN); // a write to a terminal that closed must fail, not end the app
	ExecArguments exec(launch);
	auto platform = std::make_unique<Platform>();
	std::array<int, 2> wake{-1, -1};
	if(pipe(wake.data()) != 0) throw std::runtime_error("could not create a pipe");
	platform->wakeRead = wake[0];
	platform->wakeWrite = wake[1];
	for(const int fd : wake) fcntl(fd, F_SETFD, FD_CLOEXEC);
	winsize window = windowSize(size);
	const long descriptors = sysconf(_SC_OPEN_MAX);
	platform->pid = forkpty(&platform->terminal, nullptr, nullptr, &window);
	if(platform->pid == 0) { // the child: only what is safe after fork
		std::signal(SIGPIPE, SIG_DFL);
		closeFrom(STDERR_FILENO + 1, descriptors); // none of the host's descriptors (its peers' sockets) reach it
		if(!exec.directory.empty() && chdir(exec.directory.c_str()) != 0) _exit(127);
		execve(exec.program.c_str(), exec.argv.data(), exec.environment.data());
		_exit(127);
	}
	if(platform->pid < 0) throw std::runtime_error("could not start '" + launch.program.filename().string() + "'");
	fcntl(platform->terminal, F_SETFD, FD_CLOEXEC);
	return std::unique_ptr<PseudoTerminal>(new PseudoTerminal(std::move(platform)));
}

bool PseudoTerminal::write(std::string_view bytes) {
	for(std::size_t written = 0; written < bytes.size();) {
		const ssize_t count = ::write(platform_->terminal, bytes.data() + written, bytes.size() - written);
		if(count < 0 && errno == EINTR) continue;
		if(count <= 0) return false;
		written += static_cast<std::size_t>(count);
	}
	return true;
}

std::size_t PseudoTerminal::read(char* buffer, std::size_t size) {
	while(true) {
		std::array<pollfd, 2> watched{{{platform_->terminal, POLLIN, 0}, {platform_->wakeRead, POLLIN, 0}}};
		if(poll(watched.data(), 2, -1) < 0) {
			if(errno == EINTR) continue;
			return 0;
		}
		if(watched[1].revents != 0) return 0;
		const ssize_t count = ::read(platform_->terminal, buffer, size);
		if(count < 0 && (errno == EINTR || errno == EAGAIN)) continue;
		return count > 0 ? static_cast<std::size_t>(count) : 0; // EIO: the program's side closed
	}
}

void PseudoTerminal::resize(TerminalSize size) noexcept {
	const winsize window = windowSize(size);
	ioctl(platform_->terminal, TIOCSWINSZ, &window);
}

void PseudoTerminal::signal(TerminalSignal signal) noexcept {
	const std::scoped_lock guard(platform_->lock);
	if(platform_->reaped) return;
	const pid_t foreground = tcgetpgrp(platform_->terminal);
	const pid_t group = foreground > 0 ? foreground : platform_->pid;
	switch(signal) {
	case TerminalSignal::Interrupt:
		kill(-group, SIGINT);
		break;
	case TerminalSignal::Terminate:
		kill(-group, SIGTERM);
		break;
	case TerminalSignal::Hangup:
		kill(-platform_->pid, SIGHUP);
		break;
	case TerminalSignal::Kill:
		kill(-platform_->pid, SIGKILL);
		break;
	}
}

int PseudoTerminal::end() noexcept {
	const char wake = 0;
	[[maybe_unused]] const ssize_t woken = ::write(platform_->wakeWrite, &wake, 1); // a full pipe wakes it already
	const std::scoped_lock guard(platform_->lock);
	if(platform_->reaped) return platform_->code;
	kill(-platform_->pid, SIGHUP); // what closing a terminal sends; whatever ignores it is killed below
	int status = 0;
	for(int tries = 0; tries < 20; ++tries) {
		if(waitpid(platform_->pid, &status, WNOHANG) == platform_->pid) {
			platform_->reaped = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
	kill(-platform_->pid, SIGKILL); // the group's id is still the program's while it is unreaped or just reaped
	while(!platform_->reaped && waitpid(platform_->pid, &status, 0) < 0 && errno == EINTR) {
	}
	platform_->reaped = true;
	platform_->code = exitCode(status);
	return platform_->code;
}

int PseudoTerminal::wait() noexcept {
	// Polled, not a blocking waitpid: end() must never wait behind this thread for a program that keeps running
	while(true) {
		{
			const std::scoped_lock guard(platform_->lock);
			if(platform_->reaped) return platform_->code;
			int status = 0;
			if(waitpid(platform_->pid, &status, WNOHANG) == platform_->pid) {
				kill(-platform_->pid, SIGKILL); // what it left in its session
				platform_->reaped = true;
				platform_->code = exitCode(status);
				return platform_->code;
			}
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
}

} // namespace affresco::services

#endif

namespace affresco::services {

PseudoTerminal::PseudoTerminal(std::unique_ptr<Platform> platform) noexcept : platform_(std::move(platform)) {}

PseudoTerminal::~PseudoTerminal() { end(); }

} // namespace affresco::services
