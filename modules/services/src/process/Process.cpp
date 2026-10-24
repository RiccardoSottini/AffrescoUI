/// @file
/// @brief ChildProcess, once for Windows (a job object, CreateProcessW with a quoted command line) and once for POSIX
/// (a process group, posix_spawn). Neither goes through a shell.

#include "process/Process.hpp"

#include <mutex>
#include <stdexcept>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>

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

/// @brief Quotes one argument the way CommandLineToArgvW and the C runtime read it back: in quotes, with the
/// backslashes before a quote doubled.
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

/// @brief The command line for a launch: the program, then each argument, quoted.
/// @param launch The launch.
/// @return The command line.
std::wstring commandLine(const Launch& launch) {
	std::wstring line = quote(launch.program.wstring());
	for(const std::string& argument : launch.arguments) line += L" " + quote(widen(argument));
	return line;
}

/// @brief The environment block CreateProcessW takes: "NAME=value" entries, each ended by a NUL, then one more.
/// @param entries The entries.
/// @return The block.
std::wstring environmentBlock(const std::vector<std::string>& entries) {
	std::wstring block;
	for(const std::string& entry : entries) block += widen(entry) + L'\0';
	return block + L'\0';
}

/// @brief Creates a pipe whose @p childEnd the child inherits, while the parent's end stays private.
/// @param childEnd Receives the child's end.
/// @param parentEnd Receives the parent's end.
/// @param childReads Whether the child reads the pipe (its stdin) or writes it (its stdout).
/// @return false if the pipe couldn't be created.
bool makePipe(HANDLE& childEnd, HANDLE& parentEnd, bool childReads) noexcept {
	constexpr DWORD bufferSize = 1u << 16; // 64 KiB, as on macOS and Linux
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

/// @brief Starts the program inside @p job, suspended until it is in the job, with only @p standard inherited.
/// @param launch What to start.
/// @param job The job.
/// @param standard The child's stdin, stdout and stderr (stderr may be nullptr); all inheritable.
/// @return The process, or nullptr if it couldn't be started.
HANDLE startInJob(const Launch& launch, HANDLE job, std::array<HANDLE, 3> standard) {
	std::wstring line = commandLine(launch);
	std::wstring environment = launch.environment ? environmentBlock(*launch.environment) : std::wstring{};
	const std::wstring directory = launch.directory.wstring();
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
	const DWORD flags = CREATE_SUSPENDED | EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW;
	PROCESS_INFORMATION started{};
	const bool created =
	    listed && CreateProcessW(launch.program.wstring().c_str(), line.data(), nullptr, nullptr, TRUE, flags,
	                             launch.environment ? environment.data() : nullptr,
	                             directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo, &started) != 0;
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

} // namespace

/// @brief The child, the job holding it and everything it starts, and the parent's ends of its pipes.
struct ChildProcess::Platform {
	/// @brief Closes every handle still open; closing the job ends anything still in it.
	~Platform() {
		for(HANDLE* handle : {&toChild, &fromChild, &process, &job}) closeHandle(*handle);
	}
	Platform() = default;
	Platform(const Platform&) = delete;
	Platform& operator=(const Platform&) = delete;

	HANDLE job = nullptr;       ///< Ends every process in it when terminated or closed.
	HANDLE process = nullptr;   ///< The child.
	HANDLE toChild = nullptr;   ///< Its stdin.
	HANDLE fromChild = nullptr; ///< Its stdout.
	std::mutex lock;            ///< Guards toChild against closeInput() while a write runs.
	std::once_flag ended;       ///< end() runs once.
};

std::unique_ptr<ChildProcess> ChildProcess::start(const Launch& launch) {
	auto platform = std::make_unique<Platform>();
	std::array<HANDLE, 3> standard{};
	const auto fail = [&](const std::string& what) {
		for(HANDLE& handle : standard) closeHandle(handle);
		return std::runtime_error(what);
	};
	if(!makePipe(standard[0], platform->toChild, true) || !makePipe(standard[1], platform->fromChild, false))
		throw fail("could not create pipes");
	standard[2] = inheritableStandardError();
	platform->job = makeJob();
	if(!platform->job) throw fail("could not create a job object");
	platform->process = startInJob(launch, platform->job, standard);
	if(!platform->process) throw fail("could not start '" + launch.program.filename().string() + "'");
	for(HANDLE& handle : standard) closeHandle(handle);
	return std::unique_ptr<ChildProcess>(new ChildProcess(std::move(platform)));
}

bool ChildProcess::write(std::string_view bytes) {
	for(std::size_t written = 0; written < bytes.size();) {
		const auto size = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - written, 1u << 20));
		DWORD count = 0;
		if(!WriteFile(platform_->toChild, bytes.data() + written, size, &count, nullptr) || count == 0) return false;
		written += count;
	}
	return true;
}

std::ptrdiff_t ChildProcess::read(char* buffer, std::size_t size) {
	DWORD count = 0;
	const auto chunk = static_cast<DWORD>(std::min<std::size_t>(size, 1u << 20));
	if(!ReadFile(platform_->fromChild, buffer, chunk, &count, nullptr))
		return GetLastError() == ERROR_BROKEN_PIPE ? 0 : -1;
	return static_cast<std::ptrdiff_t>(count);
}

bool ChildProcess::readable(std::chrono::milliseconds wait) {
	const auto deadline = std::chrono::steady_clock::now() + wait;
	while(true) {
		DWORD available = 0;
		if(!PeekNamedPipe(platform_->fromChild, nullptr, 0, nullptr, &available, nullptr) || available > 0)
			return true; // bytes, or a broken pipe that read() reports
		if(std::chrono::steady_clock::now() >= deadline) return false;
		Sleep(10);
	}
}

void ChildProcess::closeInput() noexcept { closeHandle(platform_->toChild); }

int ChildProcess::end() noexcept {
	std::call_once(platform_->ended, [this] { TerminateJobObject(platform_->job, 1); });
	return wait();
}

int ChildProcess::wait() noexcept {
	WaitForSingleObject(platform_->process, INFINITE);
	DWORD code = 0;
	return GetExitCodeProcess(platform_->process, &code) ? static_cast<int>(code) : -1;
}

long startDetached(const Launch& launch) {
	std::wstring line = commandLine(launch);
	std::wstring environment = launch.environment ? environmentBlock(*launch.environment) : std::wstring{};
	const std::wstring directory = launch.directory.wstring();
	const std::wstring program = launch.program.wstring();
	STARTUPINFOW startup{};
	startup.cb = sizeof(startup);
	PROCESS_INFORMATION started{};
	DWORD flags = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP | CREATE_UNICODE_ENVIRONMENT | CREATE_BREAKAWAY_FROM_JOB;
	for(int attempt = 0; attempt < 2; ++attempt) {
		if(CreateProcessW(program.c_str(), line.data(), nullptr, nullptr, FALSE, flags,
		                  launch.environment ? environment.data() : nullptr,
		                  directory.empty() ? nullptr : directory.c_str(), &startup, &started)) {
			closeHandle(started.hThread);
			closeHandle(started.hProcess);
			return static_cast<long>(started.dwProcessId);
		}
		flags &= ~static_cast<DWORD>(CREATE_BREAKAWAY_FROM_JOB); // a job that forbids breaking away: start inside it
	}
	throw std::runtime_error("could not start '" + launch.program.filename().string() + "'");
}

} // namespace affresco::services

#else

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

// The environment that posix_spawn passes on: POSIX's own variable, which not every C library's unistd.h declares
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables,readability-redundant-declaration)
extern char** environ;

namespace affresco::services {

namespace {

/// @brief Creates a pipe whose ends programs started later don't inherit.
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

/// @brief The detached children not yet reaped: one thread polls them all (never a thread per run) and ends once none
/// is left, so a child never stays a zombie while the host runs.
class Reaper {
public:
	/// The most detached children running at once: past it, startDetached() refuses.
	static constexpr std::size_t mostChildren = 64;

	/// @brief The process's reaper. Never destroyed: its thread may still run while the statics are.
	/// @return It.
	static Reaper& instance() {
		// NOLINTNEXTLINE(cppcoreguidelines-owning-memory): it lives as long as the process
		static Reaper* const reaper = new Reaper();
		return *reaper;
	}

	/// @brief Whether another child may start.
	/// @return Whether fewer than mostChildren are running.
	bool roomForOne() {
		const std::scoped_lock lock(mutex_);
		reap();
		return children_.size() < mostChildren;
	}

	/// @brief Watches a child, starting the thread if it isn't running.
	/// @param pid The child.
	void add(pid_t pid) {
		const std::scoped_lock lock(mutex_);
		children_.push_back(pid);
		if(running_) return;
		running_ = true;
		std::thread([this] { run(); }).detach();
	}

private:
	/// @brief Collects the children that ended. The mutex is held.
	void reap() {
		std::erase_if(children_, [](pid_t pid) {
			int status = 0;
			const pid_t reaped = waitpid(pid, &status, WNOHANG);
			return reaped == pid || (reaped < 0 && errno == ECHILD);
		});
	}

	/// @brief The thread: reaps four times a second until no child is left.
	void run() {
		while(true) {
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			const std::scoped_lock lock(mutex_);
			reap();
			if(children_.empty()) {
				running_ = false;
				return;
			}
		}
	}

	std::mutex mutex_;              ///< Guards children_ and running_.
	std::vector<pid_t> children_{}; ///< Not reaped yet.
	bool running_ = false;          ///< Whether the thread runs.
};

/// @brief Sets the child's working directory. posix_spawn_file_actions_addchdir is POSIX 2024's name; macOS has it
/// (and deprecates the _np one), glibc has the _np one since 2.29.
/// @param actions The file actions.
/// @param directory The directory.
/// @return 0 or an error number.
int changeDirectory(posix_spawn_file_actions_t& actions, const char* directory) noexcept {
#ifdef __APPLE__
	return posix_spawn_file_actions_addchdir(&actions, directory);
#else
	return posix_spawn_file_actions_addchdir_np(&actions, directory);
#endif
}

/// @brief Gives the child only its stdin, stdout and stderr: none of the host's other descriptors (its peers' sockets
/// among them), close-on-exec or not.
/// @param actions The file actions, after the dup2s.
/// @param flags The spawn flags, to which it may add one.
/// @return 0 or an error number.
int closeTheRest(posix_spawn_file_actions_t& actions, short& flags) noexcept {
#ifdef __APPLE__
	flags |= POSIX_SPAWN_CLOEXEC_DEFAULT; // all but the dup2 targets and those inherited explicitly
	return posix_spawn_file_actions_addinherit_np(&actions, STDERR_FILENO);
#elif defined(__GLIBC__) && (__GLIBC__ > 2 || __GLIBC_MINOR__ >= 34)
	(void)flags;
	return posix_spawn_file_actions_addclosefrom_np(&actions, STDERR_FILENO + 1);
#else
	(void)actions;
	(void)flags;
	return 0; // the host's own descriptors are close-on-exec
#endif
}

/// @brief The exit code waitpid reported.
/// @param status Its status.
/// @return The code, or -1 if a signal ended the child.
int exitCode(int status) noexcept { return WIFEXITED(status) ? WEXITSTATUS(status) : -1; }

} // namespace

/// @brief The child, the leader of a process group holding everything it starts, and the parent's ends of its pipes.
struct ChildProcess::Platform {
	/// @brief Closes the descriptors still open.
	~Platform() {
		closeDescriptor(toChild);
		closeDescriptor(fromChild);
	}
	Platform() = default;
	Platform(const Platform&) = delete;
	Platform& operator=(const Platform&) = delete;

	pid_t pid = -1;      ///< The child; also its process group's id.
	int toChild = -1;    ///< Its stdin.
	int fromChild = -1;  ///< Its stdout.
	std::mutex lock;     ///< Guards reaped: a reaped child's id may be reused, so it is never signalled after.
	bool reaped = false; ///< Whether waitpid returned the child.
	int code = -1;       ///< Its exit code, once reaped.
};

std::unique_ptr<ChildProcess> ChildProcess::start(const Launch& launch) {
	std::signal(SIGPIPE, SIG_IGN); // a write to a child that exited must fail, not end the app
	std::array<int, 2> input = {-1, -1}, output = {-1, -1};
	if(!makePipe(input) || !makePipe(output)) {
		for(std::array<int, 2>* ends : {&input, &output})
			for(int& fd : *ends) closeDescriptor(fd);
		throw std::runtime_error("could not create pipes");
	}
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, input[0], STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
	const std::string directory = launch.directory.string();
	int problem = directory.empty() ? 0 : changeDirectory(actions, directory.c_str());
	short flags = POSIX_SPAWN_SETPGROUP;
	if(problem == 0) problem = closeTheRest(actions, flags);
	posix_spawnattr_t attributes;
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setflags(&attributes, flags);
	posix_spawnattr_setpgroup(&attributes, 0);

	const std::string program = launch.program.string();
	std::vector<std::string> words{program};
	words.insert(words.end(), launch.arguments.begin(), launch.arguments.end());
	std::vector<char*> argv, environment;
	for(std::string& word : words) argv.push_back(word.data());
	argv.push_back(nullptr);
	std::vector<std::string> entries = launch.environment.value_or(std::vector<std::string>{});
	for(std::string& entry : entries) environment.push_back(entry.data());
	environment.push_back(nullptr);

	auto platform = std::make_unique<Platform>();
	if(problem == 0)
		problem = posix_spawn(&platform->pid, program.c_str(), &actions, &attributes, argv.data(),
		                      launch.environment ? environment.data() : environ);
	posix_spawnattr_destroy(&attributes);
	posix_spawn_file_actions_destroy(&actions);
	closeDescriptor(input[0]);
	closeDescriptor(output[1]);
	platform->toChild = input[1];
	platform->fromChild = output[0];
	if(problem != 0) throw std::runtime_error("could not start '" + launch.program.filename().string() + "'");
	return std::unique_ptr<ChildProcess>(new ChildProcess(std::move(platform)));
}

bool ChildProcess::write(std::string_view bytes) {
	for(std::size_t written = 0; written < bytes.size();) {
		const ssize_t count = ::write(platform_->toChild, bytes.data() + written, bytes.size() - written);
		if(count < 0 && errno == EINTR) continue;
		if(count <= 0) return false;
		written += static_cast<std::size_t>(count);
	}
	return true;
}

std::ptrdiff_t ChildProcess::read(char* buffer, std::size_t size) {
	while(true) {
		const ssize_t count = ::read(platform_->fromChild, buffer, size);
		if(count < 0 && errno == EINTR) continue;
		return count;
	}
}

bool ChildProcess::readable(std::chrono::milliseconds wait) {
	pollfd watched{platform_->fromChild, POLLIN, 0};
	const int ready = ::poll(&watched, 1, static_cast<int>(wait.count()));
	return ready > 0; // POLLIN, POLLHUP or POLLERR: read() returns at once
}

void ChildProcess::closeInput() noexcept { closeDescriptor(platform_->toChild); }

int ChildProcess::end() noexcept {
	const std::scoped_lock guard(platform_->lock);
	if(platform_->reaped) return platform_->code;
	kill(-platform_->pid, SIGKILL); // the whole group; the child is unreaped, so its id can't have been reused
	int status = 0;
	while(waitpid(platform_->pid, &status, 0) < 0 && errno == EINTR) {
	}
	platform_->reaped = true;
	platform_->code = exitCode(status);
	return platform_->code;
}

long startDetached(const Launch& launch) {
	Reaper& reaper = Reaper::instance();
	if(!reaper.roomForOne())
		throw std::runtime_error("could not start '" + launch.program.filename().string() +
		                         "': " + std::to_string(Reaper::mostChildren) + " programs started are still running");
	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	int problem = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
	const std::string directory = launch.directory.string();
	if(problem == 0 && !directory.empty()) problem = changeDirectory(actions, directory.c_str());
#ifdef POSIX_SPAWN_SETSID
	short flags = POSIX_SPAWN_SETSID; // a session of its own: the host's terminal closing doesn't end it
#else
	short flags = POSIX_SPAWN_SETPGROUP;
#endif
	if(problem == 0) problem = closeTheRest(actions, flags);
#ifdef __APPLE__
	if(problem == 0) problem = posix_spawn_file_actions_addinherit_np(&actions, STDOUT_FILENO); // as stderr
#endif
	posix_spawnattr_t attributes;
	posix_spawnattr_init(&attributes);
	posix_spawnattr_setflags(&attributes, flags);
	posix_spawnattr_setpgroup(&attributes, 0);

	const std::string program = launch.program.string();
	std::vector<std::string> words{program};
	words.insert(words.end(), launch.arguments.begin(), launch.arguments.end());
	std::vector<char*> argv, environment;
	for(std::string& word : words) argv.push_back(word.data());
	argv.push_back(nullptr);
	std::vector<std::string> entries = launch.environment.value_or(std::vector<std::string>{});
	for(std::string& entry : entries) environment.push_back(entry.data());
	environment.push_back(nullptr);

	pid_t pid = -1;
	if(problem == 0)
		problem = posix_spawn(&pid, program.c_str(), &actions, &attributes, argv.data(),
		                      launch.environment ? environment.data() : environ);
	posix_spawnattr_destroy(&attributes);
	posix_spawn_file_actions_destroy(&actions);
	if(problem != 0) throw std::runtime_error("could not start '" + launch.program.filename().string() + "'");
	reaper.add(pid); // collects its exit, so it doesn't stay a zombie while the host runs
	return static_cast<long>(pid);
}

int ChildProcess::wait() noexcept {
	// Polled, not a blocking waitpid: end() must never wait behind this thread for a child that keeps running
	while(true) {
		{
			const std::scoped_lock guard(platform_->lock);
			if(platform_->reaped) return platform_->code;
			int status = 0;
			if(waitpid(platform_->pid, &status, WNOHANG) == platform_->pid) {
				kill(-platform_->pid, SIGKILL); // what it left in its group; the group's id is still its own
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

ChildProcess::ChildProcess(std::unique_ptr<Platform> platform) noexcept : platform_(std::move(platform)) {}

ChildProcess::~ChildProcess() { end(); }

} // namespace affresco::services
