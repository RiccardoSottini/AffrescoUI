/// @file
/// @brief ProcessChannel and StdioChannel. The platform layer (the Process struct, line reading and writing, the
/// standard handles) comes first, once for Windows and once for POSIX; the channels themselves are shared.

#include "affresco/channel/Channels.hpp"

#include <stdexcept>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "transports/Lines.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <vector>

namespace affresco::channel {

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
std::wstring widen(const std::string& text) {
	if(text.empty()) return {};
	const int size = static_cast<int>(text.size());
	const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, nullptr, 0);
	if(length <= 0) throw std::runtime_error("AffrescoUI: the command is not valid UTF-8");
	std::wstring wide(static_cast<std::size_t>(length), L'\0');
	MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, wide.data(), length);
	return wide;
}

/// @brief Creates a pipe whose @p childEnd the child inherits, while the parent's end stays private to this process.
/// @param childEnd Receives the child's end.
/// @param parentEnd Receives the parent's end.
/// @param childReads Whether the child reads the pipe (its stdin) or writes it (its stdout).
/// @return false if the pipe couldn't be created.
bool makePipe(HANDLE& childEnd, HANDLE& parentEnd, bool childReads) noexcept {
	// 64 KiB, as a pipe has on macOS and Linux: the default is a few KiB, and send() would then block the main
	// thread on any larger message until the child reads it
	constexpr DWORD bufferSize = 1u << 16;
	SECURITY_ATTRIBUTES inheritable{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
	HANDLE readEnd = nullptr, writeEnd = nullptr;
	if(!CreatePipe(&readEnd, &writeEnd, &inheritable, bufferSize)) return false;
	childEnd = childReads ? readEnd : writeEnd;
	parentEnd = childReads ? writeEnd : readEnd;
	return SetHandleInformation(parentEnd, HANDLE_FLAG_INHERIT, 0) != 0;
}

/// @brief An inheritable copy of this process's stderr, so the child logs where the app does.
/// @return The copy, or nullptr if this process has no stderr.
HANDLE inheritableStandardError() noexcept {
	const HANDLE error = GetStdHandle(STD_ERROR_HANDLE);
	if(!error || error == INVALID_HANDLE_VALUE) return nullptr;
	HANDLE copy = nullptr;
	const HANDLE self = GetCurrentProcess();
	return DuplicateHandle(self, error, self, &copy, 0, TRUE, DUPLICATE_SAME_ACCESS) ? copy : nullptr;
}

/// @brief Creates a job object that ends every process in it when it is closed or terminated.
/// @return The job, or nullptr on failure.
HANDLE makeJob() noexcept {
	HANDLE job = CreateJobObjectW(nullptr, nullptr);
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
	limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
	if(job && !SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits)))
		closeHandle(job);
	return job;
}

/// @brief The command line that runs @p command through cmd.exe, which /s makes take the quoted text as it is.
/// @param shell The full path of cmd.exe.
/// @param command The command, in UTF-8.
/// @return The command line.
std::wstring shellCommandLine(const std::wstring& shell, const std::string& command) {
	return L"\"" + shell + L"\" /d /s /c \"" + widen(command) + L"\"";
}

/// @brief The full path of cmd.exe, taken from the system directory so a cmd.exe in the working folder can't
/// stand in for it.
/// @return The path.
std::wstring shellPath() {
	std::array<wchar_t, MAX_PATH> directory{};
	const UINT length = GetSystemDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
	if(length == 0 || length >= directory.size()) throw std::runtime_error("AffrescoUI: no system directory");
	return std::wstring(directory.data(), length) + L"\\cmd.exe";
}

/// @brief Starts cmd.exe inside @p job, with stdin, stdout and stderr set to @p standard and no other handle
/// inherited.
/// @param shell The full path of cmd.exe.
/// @param commandLine Its command line (shellCommandLine()); CreateProcessW may write to it.
/// @param job The job the process joins before it runs.
/// @param standard The child's stdin, stdout and stderr (stderr may be nullptr); all inheritable.
/// @return The process, or nullptr if it couldn't be started.
HANDLE startShell(const std::wstring& shell, std::wstring commandLine, HANDLE job, std::array<HANDLE, 3> standard) {
	// Only these handles are inherited, even if another thread creates an inheritable handle meanwhile
	const DWORD inherited = standard[2] ? 3 : 2;
	SIZE_T size = 0;
	InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
	std::vector<std::byte> storage(size);
	const auto attributes = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
	if(!InitializeProcThreadAttributeList(attributes, 1, 0, &size)) return nullptr;
	const bool listed = UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, standard.data(),
	                                              inherited * sizeof(HANDLE), nullptr, nullptr) != 0;

	STARTUPINFOEXW startup{};
	startup.StartupInfo.cb = sizeof(startup);
	startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
	startup.StartupInfo.hStdInput = standard[0];
	startup.StartupInfo.hStdOutput = standard[1];
	startup.StartupInfo.hStdError = standard[2];
	startup.lpAttributeList = attributes;

	// Suspended until it is in the job, so nothing it starts can escape the job
	PROCESS_INFORMATION started{};
	const bool created = listed && CreateProcessW(shell.c_str(), commandLine.data(), nullptr, nullptr, TRUE,
	                                              CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr,
	                                              &startup.StartupInfo, &started) != 0;
	DeleteProcThreadAttributeList(attributes);
	if(!created) return nullptr;

	if(!AssignProcessToJobObject(job, started.hProcess)) {
		TerminateProcess(started.hProcess, 1);
		WaitForSingleObject(started.hProcess, INFINITE);
		closeHandle(started.hProcess);
	} else {
		ResumeThread(started.hThread);
	}
	closeHandle(started.hThread);
	return started.hProcess;
}

/// @brief Reads @p input until it closes, posting each line to the channel's onMessage, then onClosed. The framing
/// is the one every channel shares (detail::readLines): a line past maxMessageBytes ends the stream.
/// @param input The handle to read.
/// @param to Where the lines go.
/// @param running Set to false when the stream ends (may be nullptr).
void readLines(HANDLE input, const detail::Delivery& to, std::atomic<bool>* running) {
	const auto read = [input](char* data, std::size_t size) -> std::ptrdiff_t {
		DWORD count = 0;
		if(!ReadFile(input, data, static_cast<DWORD>(size), &count, nullptr)) return -1; // also when the writer closed
		return static_cast<std::ptrdiff_t>(count);
	};
	// The console and cmd.exe end lines with CRLF
	detail::readLines(read, to, running, true);
}

/// @brief Writes a message followed by a line break.
/// @param output The handle.
/// @param message The message.
/// @return false if the write failed (the other end went away).
bool writeAll(HANDLE output, std::string_view message) {
	std::string line(message);
	line += '\n';
	for(std::size_t written = 0; written < line.size();) {
		const auto size = static_cast<DWORD>(std::min<std::size_t>(line.size() - written, 1u << 20));
		DWORD count = 0;
		if(!WriteFile(output, line.data() + written, size, &count, nullptr) || count == 0) return false;
		written += count;
	}
	return true;
}

/// @brief Nothing to do on Windows: a write to a closed pipe fails instead of raising a signal.
void ignoreBrokenPipes() noexcept {}

/// @brief This process's stdin.
HANDLE standardInput() noexcept { return GetStdHandle(STD_INPUT_HANDLE); }

/// @brief This process's stdout.
HANDLE standardOutput() noexcept { return GetStdHandle(STD_OUTPUT_HANDLE); }

} // namespace

/// @brief The child (cmd.exe), the job holding it and everything it starts, and the parent's ends of its pipes.
struct ProcessChannel::Process {
	/// @brief Starts @p command.
	/// @param command A shell command.
	/// @throws std::runtime_error If it can't be started.
	explicit Process(const std::string& command);
	/// @brief Closes every handle still open; closing the job ends anything still in it.
	~Process() { closeAll(); }

	Process(const Process&) = delete;
	Process& operator=(const Process&) = delete;

	/// @brief Closes the child's stdin, ends the job, and waits for the child.
	void end() noexcept;
	/// @brief Closes every handle still open.
	void closeAll() noexcept {
		for(HANDLE* handle : {&toChild, &fromChild, &process, &job}) closeHandle(*handle);
	}

	HANDLE job = nullptr;       ///< Ends every process in it when terminated or closed.
	HANDLE process = nullptr;   ///< The child.
	HANDLE toChild = nullptr;   ///< Its stdin.
	HANDLE fromChild = nullptr; ///< Its stdout.
};

ProcessChannel::Process::Process(const std::string& command) {
	const std::wstring shell = shellPath(); // these two may throw, so they come before any handle is open
	const std::wstring commandLine = shellCommandLine(shell, command);

	std::array<HANDLE, 3> standard{}; // the child's ends: stdin, stdout, stderr
	const auto fail = [&](const std::string& what) {
		for(HANDLE& handle : standard) closeHandle(handle);
		closeAll();
		return std::runtime_error("AffrescoUI: " + what);
	};

	if(!makePipe(standard[0], toChild, true) || !makePipe(standard[1], fromChild, false))
		throw fail("could not create pipes");
	standard[2] = inheritableStandardError();
	job = makeJob();
	if(!job) throw fail("could not create a job object");
	process = startShell(shell, commandLine, job, standard);
	if(!process) throw fail("could not start '" + command + "'");
	for(HANDLE& handle : standard) closeHandle(handle); // the child has them; reads end when it closes stdout
}

void ProcessChannel::Process::end() noexcept {
	closeHandle(toChild);       // EOF on its stdin
	TerminateJobObject(job, 1); // cmd.exe runs the program as its own child: end both
	WaitForSingleObject(process, INFINITE);
}

} // namespace affresco::channel

#else

#include "transports/Lines.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

// The environment that posix_spawn passes on: POSIX's own variable, which not every C library's unistd.h declares
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,readability-redundant-declaration)
extern char** environ;

namespace affresco::channel {

namespace {

using detail::readLines;
using detail::writeAll;

/// @brief Creates a pipe whose ends programs started later don't inherit (posix_spawn's dup2 gives the child its
/// own copies), so one child never holds another channel's pipes open.
/// @param ends Receives the read end, then the write end.
/// @return false if the pipe couldn't be created.
bool makePipe(std::array<int, 2>& ends) noexcept {
	if(pipe(ends.data()) != 0) return false;
	for(const int end : ends) fcntl(end, F_SETFD, FD_CLOEXEC);
	return true;
}

/// @brief Closes a descriptor if it is open, and forgets it.
/// @param fd The descriptor; -1 afterwards.
void closeDescriptor(int& fd) noexcept {
	if(fd >= 0) close(fd);
	fd = -1;
}

/// @brief Makes a write to a closed pipe fail instead of ending the app (a child that exited must not kill it).
void ignoreBrokenPipes() noexcept { std::signal(SIGPIPE, SIG_IGN); }

/// @brief This process's stdin.
int standardInput() noexcept { return STDIN_FILENO; }

/// @brief This process's stdout.
int standardOutput() noexcept { return STDOUT_FILENO; }

} // namespace

/// @brief The child (/bin/sh), the leader of a process group holding everything it starts, and the parent's ends of
/// its pipes.
struct ProcessChannel::Process {
	/// @brief Starts @p command.
	/// @param command A shell command.
	/// @throws std::runtime_error If it can't be started.
	explicit Process(const std::string& command);
	/// @brief Closes the descriptors still open.
	~Process() {
		closeDescriptor(toChild);
		closeDescriptor(fromChild);
	}

	Process(const Process&) = delete;
	Process& operator=(const Process&) = delete;

	/// @brief Closes the child's stdin, ends its process group, and waits for the child.
	void end() noexcept;

	pid_t pid = -1;     ///< The child; also the id of its process group.
	int toChild = -1;   ///< Its stdin.
	int fromChild = -1; ///< Its stdout.
};

ProcessChannel::Process::Process(const std::string& command) {
	ignoreBrokenPipes();

	std::array<int, 2> input = {-1, -1}, output = {-1, -1};
	if(!makePipe(input) || !makePipe(output)) {
		for(std::array<int, 2>* ends : {&input, &output})
			for(int& fd : *ends) closeDescriptor(fd);
		throw std::runtime_error("AffrescoUI: could not create pipes");
	}

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, input[0], STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);

	// Its own process group, so closing the channel also ends the programs the shell starts
	posix_spawnattr_t attributes;
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setflags(&attributes, POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&attributes, 0);

	std::string shell = "/bin/sh", flag = "-c", script = command;
	const std::array<char*, 4> argv = {shell.data(), flag.data(), script.data(), nullptr};
	const int spawned = posix_spawn(&pid, "/bin/sh", &actions, &attributes, argv.data(), environ);
	posix_spawnattr_destroy(&attributes);
	posix_spawn_file_actions_destroy(&actions);
	closeDescriptor(input[0]);
	closeDescriptor(output[1]);
	toChild = input[1];
	fromChild = output[0];
	if(spawned != 0) {
		closeDescriptor(toChild); // the destructor doesn't run when a constructor throws
		closeDescriptor(fromChild);
		throw std::runtime_error("AffrescoUI: could not start '" + command + "'");
	}
}

void ProcessChannel::Process::end() noexcept {
	closeDescriptor(toChild); // EOF on its stdin
	kill(-pid, SIGTERM);      // the whole group; the child is still unreaped, so its id can't have been reused
	int status = 0;
	while(waitpid(pid, &status, 0) < 0 && errno == EINTR) {
	}
}

} // namespace affresco::channel

#endif

namespace affresco::channel {

ProcessChannel::ProcessChannel(Executor& executor, const std::string& command)
    : executor_(executor), process_(std::make_unique<Process>(command)), running_(true) {
	try {
		reader_ = std::thread([this] { readLines(process_->fromChild, {executor_, *this, alive_}, &running_); });
	} catch(...) {
		process_->end(); // the destructor doesn't run when a constructor throws, and ~Process alone leaves it running
		throw;
	}
}

ProcessChannel::~ProcessChannel() {
	*alive_ = false;
	process_->end();
	if(reader_.joinable()) reader_.join(); // the read returns once every process that held its stdout has ended
}

void ProcessChannel::send(std::string_view message) {
	if(running_ && !writeAll(process_->toChild, message)) running_ = false;
}

StdioChannel::StdioChannel(Executor& executor) {
	ignoreBrokenPipes();
	// Detached: a read of stdin can't be interrupted, and the thread ends with the process anyway
	std::thread([this, &executor, alive = alive_] {
		readLines(standardInput(), {executor, *this, alive}, nullptr);
	}).detach();
}

StdioChannel::~StdioChannel() { *alive_ = false; }

void StdioChannel::send(std::string_view message) { writeAll(standardOutput(), message); }

} // namespace affresco::channel
