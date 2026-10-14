#pragma once

#include "sgl/Signal.hpp"

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace sgl {

class Application;

// A two-way stream of text messages (one JSON object per line) between the app and something outside it,
// e.g. a program that turns chat messages into screen revisions. onMessage always fires on the main thread.
class Channel {
public:
	virtual ~Channel() = default;

	virtual void send(std::string_view message) = 0;

	Signal<const std::string&> onMessage;
	Signal<> onClosed; // the other end went away (main thread)
};

// Runs `command` (through /bin/sh) as a child process: messages go to its stdin, its stdout lines come back.
// Its stderr is inherited, so it can log. POSIX only for now.
class ProcessChannel final : public Channel {
public:
	ProcessChannel(Application& app, const std::string& command); // throws std::runtime_error
	~ProcessChannel() override;                                   // closes its stdin and waits for it to exit

	ProcessChannel(const ProcessChannel&) = delete;
	ProcessChannel& operator=(const ProcessChannel&) = delete;

	void send(std::string_view message) override;
	[[nodiscard]] bool isRunning() const noexcept { return running_; }

private:
	Application& app_;
	int pid_ = -1;
	int toChild_ = -1;
	int fromChild_ = -1;
	std::atomic<bool> running_ = false;
	std::thread reader_;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true); // guards tasks still queued after destruction
};

// This process's own stdin/stdout: the app *is* the other end's child process (how MCP clients run servers).
// Nothing else may write to stdout while it is in use. POSIX only for now.
class StdioChannel final : public Channel {
public:
	explicit StdioChannel(Application& app);
	~StdioChannel() override;

	StdioChannel(const StdioChannel&) = delete;
	StdioChannel& operator=(const StdioChannel&) = delete;

	void send(std::string_view message) override;

private:
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace sgl
