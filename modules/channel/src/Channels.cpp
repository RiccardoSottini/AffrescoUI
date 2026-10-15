#include "sgl/channel/Channels.hpp"

#include <stdexcept>

#if defined(_WIN32)

namespace sgl::channel {

ProcessChannel::ProcessChannel(Executor& executor, const std::string&) : executor_(executor) {
	throw std::runtime_error("sGLGUI: ProcessChannel is not available on Windows yet");
}
ProcessChannel::~ProcessChannel() = default;
void ProcessChannel::send(std::string_view) {}

StdioChannel::StdioChannel(Executor&) {
	throw std::runtime_error("sGLGUI: StdioChannel is not available on Windows yet");
}
StdioChannel::~StdioChannel() = default;
void StdioChannel::send(std::string_view) {}

} // namespace sgl::channel

#else

#include <csignal>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace sgl::channel {

namespace {

// Splits what arrives on `fd` into lines and hands each one to the main thread, then reports the end of the stream
void readLines(int fd, Executor& executor, Channel& channel, std::shared_ptr<bool> alive, std::atomic<bool>* running) {
	std::string buffer;
	char chunk[4096];

	while(true) {
		const ssize_t n = read(fd, chunk, sizeof(chunk));
		if(n <= 0) break;
		buffer.append(chunk, static_cast<std::size_t>(n));

		for(std::size_t end; (end = buffer.find('\n')) != std::string::npos;) {
			std::string line = buffer.substr(0, end);
			buffer.erase(0, end + 1);
			if(line.empty()) continue;
			executor.post([&channel, alive, line = std::move(line)] {
				if(*alive) channel.onMessage(line);
			});
		}
	}

	if(running) *running = false;
	executor.post([&channel, alive] {
		if(*alive) channel.onClosed();
	});
}

bool writeAll(int fd, std::string_view message) {
	std::string line(message);
	line += '\n';
	for(std::size_t written = 0; written < line.size();) {
		const ssize_t n = write(fd, line.data() + written, line.size() - written);
		if(n <= 0) return false;
		written += static_cast<std::size_t>(n);
	}
	return true;
}

} // namespace

ProcessChannel::ProcessChannel(Executor& executor, const std::string& command) : executor_(executor) {
	std::signal(SIGPIPE, SIG_IGN); // writing to a brain that exited must not kill the app

	int input[2], output[2];
	if(pipe(input) != 0 || pipe(output) != 0) throw std::runtime_error("sGLGUI: could not create pipes");

	posix_spawn_file_actions_t actions;
	posix_spawn_file_actions_init(&actions);
	posix_spawn_file_actions_adddup2(&actions, input[0], STDIN_FILENO);
	posix_spawn_file_actions_adddup2(&actions, output[1], STDOUT_FILENO);
	for(int fd : {input[0], input[1], output[0], output[1]}) posix_spawn_file_actions_addclose(&actions, fd);

	std::string shell = "/bin/sh", flag = "-c", script = command;
	char* argv[] = {shell.data(), flag.data(), script.data(), nullptr};

	pid_t pid = 0;
	const int spawned = posix_spawn(&pid, "/bin/sh", &actions, nullptr, argv, environ);
	posix_spawn_file_actions_destroy(&actions);
	close(input[0]);
	close(output[1]);
	if(spawned != 0) {
		close(input[1]);
		close(output[0]);
		throw std::runtime_error("sGLGUI: could not start '" + command + "'");
	}

	pid_ = pid;
	toChild_ = input[1];
	fromChild_ = output[0];
	running_ = true;
	reader_ = std::thread([this] { readLines(fromChild_, executor_, *this, alive_, &running_); });
}

ProcessChannel::~ProcessChannel() {
	*alive_ = false;
	if(toChild_ >= 0) close(toChild_); // EOF on its stdin: a well-behaved child exits
	if(pid_ > 0) {
		int status = 0;
		if(waitpid(pid_, &status, WNOHANG) == 0) {
			kill(pid_, SIGTERM);
			waitpid(pid_, &status, 0);
		}
	}
	if(reader_.joinable()) reader_.join(); // read() returns 0 once the child is gone
	if(fromChild_ >= 0) close(fromChild_);
}

void ProcessChannel::send(std::string_view message) {
	if(running_ && !writeAll(toChild_, message)) running_ = false;
}

StdioChannel::StdioChannel(Executor& executor) {
	std::signal(SIGPIPE, SIG_IGN);
	// Detached: a read() on stdin can't be interrupted, and the thread ends with the process anyway
	std::thread([this, &executor, alive = alive_] { readLines(STDIN_FILENO, executor, *this, alive, nullptr); }).detach();
}

StdioChannel::~StdioChannel() { *alive_ = false; }

void StdioChannel::send(std::string_view message) { writeAll(STDOUT_FILENO, message); }

} // namespace sgl::channel

#endif
