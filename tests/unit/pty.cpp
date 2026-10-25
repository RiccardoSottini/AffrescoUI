/// @file
/// @brief The pseudo terminal and the `pty` provider: a program's output read back, what is typed, a resize the
/// program hears, an interrupt it traps, and output split mid-character kept as valid text.

#include "Providers.hpp"
#include "process/Pty.hpp"

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#endif

using affresco::json;
using affresco::services::ChildProcess;
using affresco::services::Launch;
using affresco::services::PseudoTerminal;
using affresco::services::takeTerminalText;

TEST_CASE("pty: output split inside a character waits for the rest, and bytes that aren't UTF-8 become U+FFFD") {
	std::string carry;
	CHECK(takeTerminalText(carry, "caf\xC3") == "caf");
	CHECK(carry == "\xC3");
	CHECK(takeTerminalText(carry, "\xA9!") == "\xC3\xA9!");
	CHECK(carry.empty());
	CHECK(takeTerminalText(carry, "a\xFF"
	                              "b") == "a\xEF\xBF\xBD"
	                                      "b");
	CHECK(takeTerminalText(carry, "\xE2\x82") == "");
	CHECK(takeTerminalText(carry, "x") == "\xEF\xBF\xBD\xEF\xBF\xBDx");
}

#ifndef _WIN32

namespace {

/// @brief An `open` call the test reads: its arguments, its events' text, and how it ended.
class TerminalCall final : public affresco::services::Call {
public:
	/// @brief A call with arguments.
	/// @param arguments Its `with`.
	explicit TerminalCall(json arguments = json::object()) : arguments_(std::move(arguments)) {}
	const json& arguments() const noexcept override { return arguments_; }
	std::stop_token stopToken() const noexcept override { return stop_.get_token(); }
	void progress(json) override {}
	void event(json event) override {
		const std::scoped_lock lock(mutex_);
		if(event.contains("session")) session_ = event.at("session").get<std::uint64_t>();
		else output_ += event.at("data").get<std::string>();
		changed_.notify_all();
	}
	void finish(json result) override {
		const std::scoped_lock lock(mutex_);
		code_ = result.value("code", -2);
		changed_.notify_all();
	}
	void fail(std::string code, std::string, json) override {
		const std::scoped_lock lock(mutex_);
		failure_ = std::move(code);
		changed_.notify_all();
	}
	/// @brief Waits until the output holds some text.
	/// @param text The text.
	/// @return Whether it did within 10 seconds.
	bool waitForOutput(const std::string& text) {
		std::unique_lock lock(mutex_);
		return changed_.wait_for(lock, std::chrono::seconds(10), [&] { return output_.contains(text); });
	}
	/// @brief Waits for the call to finish or fail.
	/// @return Its exit code, or std::nullopt if it failed or didn't end within 10 seconds.
	std::optional<int> waitForEnd() {
		std::unique_lock lock(mutex_);
		changed_.wait_for(lock, std::chrono::seconds(10), [&] { return code_ || failure_; });
		return code_;
	}
	/// @brief The session's id, once the first event came.
	/// @return The id, or 0.
	std::uint64_t session() {
		std::unique_lock lock(mutex_);
		changed_.wait_for(lock, std::chrono::seconds(10), [&] { return session_ != 0; });
		return session_;
	}

private:
	json arguments_;                     ///< See the constructor.
	std::stop_source stop_;              ///< Never asked.
	std::mutex mutex_;                   ///< Guards the rest.
	std::condition_variable changed_;    ///< Signalled at each event and at the end.
	std::string output_;                 ///< The output so far.
	std::uint64_t session_ = 0;          ///< From the first event.
	std::optional<int> code_;            ///< Once finished.
	std::optional<std::string> failure_; ///< Once failed.
};

/// @brief An executor that never runs anything: the pty service posts nothing.
class UnusedExecutor final : public affresco::Executor {
public:
	void post(std::function<void()>) override {}
};

/// @brief A `pty` service whose program is `/bin/sh -c script`.
struct ShellService {
	UnusedExecutor executor; ///< The main thread, unused.
	std::unique_ptr<affresco::services::Provider> provider = affresco::services::makePtyProvider(); ///< The provider.
	std::unique_ptr<affresco::services::Service> service;                                           ///< The service.

	/// @brief Creates it.
	/// @param script What the shell runs.
	/// @param root The service's root (canonical), or empty.
	explicit ShellService(const std::string& script, const std::filesystem::path& root = {}) {
		const affresco::services::Grant grant{root, root.empty() ? "" : "root",
		                                      affresco::services::ProcessGrant{"/bin/sh", {"-c", script}, {}, {}},
		                                      "sh"};
		service = provider->create({{"process", "sh"}}, {executor, grant, {}, {}});
	}
	/// @brief Invokes a method.
	/// @param name The method.
	/// @param call The call.
	/// @return The refusal, or std::nullopt.
	std::optional<std::string> invoke(const std::string& name, std::shared_ptr<TerminalCall> call) {
		for(const affresco::services::MethodSpec& method : provider->methods(json::object()))
			if(method.name == name) return service->invoke(method, std::move(call));
		return "no such method";
	}
};

} // namespace

TEST_CASE("pty: a shell command runs on a terminal of the size asked, and its output is read back") {
	auto terminal = PseudoTerminal::start(Launch{"/bin/sh", {"-c", "stty size; printf hello"}, {}, {}}, {100, 30});
	std::string output;
	char buffer[256];
	for(std::size_t count; (count = terminal->read(buffer, sizeof buffer)) > 0;) output.append(buffer, count);
	CHECK(output.contains("30 100"));
	CHECK(output.contains("hello"));
	CHECK(terminal->wait() == 0);
}

TEST_CASE("pty: a program started in a terminal or as a child gets none of the host's other descriptors") {
	// The host's sockets (its peers' connections among them) must not reach a shell or a debug adapter it starts.
	const int open = ::open("/dev/null", O_RDONLY); // without O_CLOEXEC, like a library's descriptor
	REQUIRE(open > 2);
	const std::string probe = "test -e /dev/fd/" + std::to_string(open) + " && echo inherited || echo closed";
	auto terminal = PseudoTerminal::start(Launch{"/bin/sh", {"-c", probe}, {}, {}}, {80, 24});
	std::string output;
	char buffer[256];
	for(std::size_t count; (count = terminal->read(buffer, sizeof buffer)) > 0;) output.append(buffer, count);
	CHECK(output.contains("closed"));
	CHECK(terminal->wait() == 0);
	const auto child = ChildProcess::start(Launch{"/bin/sh", {"-c", probe}, {}, std::nullopt});
	output.clear();
	for(std::ptrdiff_t count; (count = child->read(buffer, sizeof buffer)) > 0;)
		output.append(buffer, static_cast<std::size_t>(count));
	CHECK(output == "closed\n");
	CHECK(child->wait() == 0);
	::close(open);
}

TEST_CASE("pty: the provider streams a session's output and takes what is typed") {
	ShellService shell("read line; echo \"got $line\"; exit 4");
	const auto open = std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}});
	REQUIRE_FALSE(shell.invoke("open", open));
	const std::uint64_t session = open->session();
	REQUIRE(session != 0);
	REQUIRE_FALSE(shell.invoke("write", std::make_shared<TerminalCall>(json{{"session", session}, {"data", "abc\n"}})));
	CHECK(open->waitForOutput("got abc"));
	CHECK(open->waitForEnd() == 4);
	CHECK(shell.invoke("write", std::make_shared<TerminalCall>(json{{"session", 99}, {"data", "x"}})));
}

TEST_CASE("pty: a resize reaches the program, and an interrupt reaches its foreground group") {
	ShellService shell("trap 'stty size' WINCH; trap 'echo caught; exit 3' INT; echo ready; "
	                   "while :; do sleep 0.05; done");
	const auto open = std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}});
	REQUIRE_FALSE(shell.invoke("open", open));
	const std::uint64_t session = open->session();
	REQUIRE(open->waitForOutput("ready"));
	REQUIRE_FALSE(shell.invoke(
	    "resize", std::make_shared<TerminalCall>(json{{"session", session}, {"cols", 120}, {"rows", 40}})));
	CHECK(open->waitForOutput("40 120"));
	REQUIRE_FALSE(
	    shell.invoke("signal", std::make_shared<TerminalCall>(json{{"session", session}, {"name", "interrupt"}})));
	CHECK(open->waitForOutput("caught"));
	CHECK(open->waitForEnd() == 3);
}

TEST_CASE("pty: a terminal opens in a folder inside the service's root, and nowhere else") {
	const std::filesystem::path root = std::filesystem::canonical(std::filesystem::temp_directory_path()) /
	                                   ("affresco-pty-" + std::to_string(::getpid()));
	std::filesystem::create_directories(root / "sub");
	std::filesystem::create_directories(root.parent_path() / (root.filename().string() + "-other"));
	ShellService shell("pwd", root);
	const auto inside = std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}, {"folder", "sub"}});
	REQUIRE_FALSE(shell.invoke("open", inside));
	CHECK(inside->waitForOutput((root / "sub").string()));
	const auto absolute =
	    std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}, {"folder", (root / "sub").string()}});
	REQUIRE_FALSE(shell.invoke("open", absolute));
	CHECK(absolute->waitForOutput((root / "sub").string()));
	for(const std::string& folder : std::vector<std::string>{"..", "../" + root.filename().string() + "-other", "/",
	                                                         "missing", root.string() + "-other"})
		CHECK(
		    shell.invoke("open", std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}, {"folder", folder}})));
	CHECK(ShellService("pwd").invoke(
	    "open", std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}, {"folder", "sub"}})));
	CHECK(inside->waitForEnd() == 0);
	CHECK(absolute->waitForEnd() == 0);
	std::filesystem::remove_all(root);
	std::filesystem::remove_all(root.string() + "-other");
}

TEST_CASE("pty: a service ends its terminals when it ends, and opens at most 8 at once") {
	auto shell = std::make_unique<ShellService>("echo up; while :; do sleep 1; done");
	std::vector<std::shared_ptr<TerminalCall>> calls;
	for(int index = 0; index < 8; ++index) {
		calls.push_back(std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}}));
		REQUIRE_FALSE(shell->invoke("open", calls.back()));
	}
	CHECK(shell->invoke("open", std::make_shared<TerminalCall>(json{{"cols", 80}, {"rows", 24}})));
	for(const auto& call : calls) REQUIRE(call->waitForOutput("up"));
	const auto started = std::chrono::steady_clock::now();
	shell.reset();
	CHECK(std::chrono::steady_clock::now() - started < std::chrono::seconds(5));
	for(const auto& call : calls) CHECK_FALSE(call->waitForEnd()); // cancelled, not finished
}

#endif
