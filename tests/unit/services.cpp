/// @file
/// @brief Services (RFC 0002): a screen invokes the prototype JSON-RPC server end to end, reads a granted folder and
/// nothing outside it, frames are bounded, and revisions may add uses of declared, revisable methods only, with the
/// ones that write reviewed.

#include "rpc/Framing.hpp"
#include "Headless.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/services/Schema.hpp>
#include <affresco/services/ServiceRegistry.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
using affresco::RevisionStatus;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief An executor whose posted tasks the test runs by hand, on the main thread.
class PumpedExecutor final : public affresco::Executor {
public:
	void post(std::function<void()> task) override {
		const std::scoped_lock lock(mutex_);
		tasks_.push_back(std::move(task));
		posted_.notify_all();
	}
	/// @brief Runs posted tasks until a condition holds.
	/// @param done The condition, checked after each batch.
	/// @param most How long at most.
	/// @return Whether it held in time.
	bool pumpUntil(const std::function<bool()>& done, std::chrono::milliseconds most = std::chrono::seconds(10)) {
		const auto end = std::chrono::steady_clock::now() + most;
		while(!done()) {
			std::vector<std::function<void()>> tasks;
			{
				std::unique_lock lock(mutex_);
				if(!posted_.wait_until(lock, end, [this] { return !tasks_.empty(); })) return done();
				tasks.swap(tasks_);
			}
			for(const auto& task : tasks) task();
		}
		return true;
	}

private:
	std::mutex mutex_;
	std::condition_variable posted_;
	std::vector<std::function<void()>> tasks_;
};

/// @brief The value at a path of a screen's state.
/// @param screen The screen.
/// @param pointer The path.
/// @return The value.
json stateAt(const affresco::Screen& screen, const char* pointer) {
	return json::parse(screen.stateJson()).at(json::json_pointer(pointer));
}

/// @brief A screen whose "read" action reads README.md from the prototype service, and "escape" a file outside the
/// granted folder.
constexpr const char* serviceScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"services": {
		"proto": { "provider": "jsonrpc", "process": "proto" },
		"files": { "provider": "fs", "root": "work", "readOnly": true }
	},
	"state": { "text": "", "busy": false, "error": "", "local": "", "denied": "" },
	"actions": {
		"read": [{ "invoke": "proto.files/read", "with": { "path": "README.md" }, "into": "state.text",
		           "loading": "state.busy", "onError": [{ "set": "state.error", "to": "{{error.message}}" }] }],
		"local": [{ "invoke": "files.read", "with": { "path": "note.txt" },
		            "onDone": [{ "set": "state.local", "to": "{{result.text}}" }] }],
		"escape": [{ "invoke": "files.read", "with": { "path": "../outside.txt" },
		             "onError": [{ "set": "state.denied", "to": "{{error.code}}" }] }]
	},
	"root": { "type": "label", "text": "{{state.text}}" }
})json";

/// @brief A folder of its own under the system's temporary folder, removed at the end.
struct TemporaryFolder {
	std::filesystem::path path = std::filesystem::temp_directory_path() / "affrescoui-services-test"; ///< The folder.
	TemporaryFolder() {
		std::filesystem::remove_all(path);
		std::filesystem::create_directories(path / "work");
	}
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	TemporaryFolder(TemporaryFolder&&) = delete;
	TemporaryFolder& operator=(TemporaryFolder&&) = delete;
};

/// @brief Writes a text file.
/// @param path Where.
/// @param text What.
void writeText(const std::filesystem::path& path, const std::string& text) { std::ofstream(path) << text; }

} // namespace

TEST_CASE("services: a screen invokes the prototype JSON-RPC server, and reads a granted folder only") {
	const TemporaryFolder folder;
	writeText(folder.path / "work" / "note.txt", "inside");
	writeText(folder.path / "outside.txt", "outside");
	PumpedExecutor executor;
	const ManualClock clock;
	const auto window = createHeadlessWindow(200, 100, clock);
	affresco::Screen screen = affresco::Screen::fromString(serviceScreen);
	screen.services().allowProcess("proto", "/usr/bin/env",
	                               {"python3", AFFRESCOUI_PROTOTYPES_DIR "/0002-services/service.py"});
	screen.services().addRoot("work", folder.path / "work");
	screen.setExecutor(&executor);
	screen.build(*window);

	screen.runAction("read", window->root());
	CHECK(stateAt(screen, "/busy") == true);
	REQUIRE(executor.pumpUntil([&screen] { return stateAt(screen, "/busy") == false; }));
	CHECK(stateAt(screen, "/error") == "");
	CHECK(stateAt(screen, "/text") == json{{"text", "# Example\nA small project to search in.\n"}});
	CHECK(json::parse(screen.stateJson()).is_object());

	screen.runAction("local", window->root());
	REQUIRE(executor.pumpUntil([&screen] { return stateAt(screen, "/local") == "inside"; }));
	screen.runAction("escape", window->root());
	REQUIRE(executor.pumpUntil([&screen] { return stateAt(screen, "/denied") != ""; }));
	CHECK(stateAt(screen, "/local") == "inside");
}

TEST_CASE("services: Content-Length frames are read in pieces, and bounded") {
	using affresco::services::FrameReader;
	using affresco::services::Framing;
	std::vector<std::string> bodies;
	FrameReader reader(Framing::ContentLength, {64, 16});
	CHECK_FALSE(reader.feed("Content-Length: 2\r\n", bodies));
	CHECK_FALSE(reader.feed("\r\n{}Content-Le", bodies));
	CHECK(bodies == std::vector<std::string>{"{}"});
	CHECK(reader.feed("ngth: 17\r\n\r\n", bodies)); // over 16 bytes: the stream is broken from now on
	CHECK(reader.feed("Content-Length: 1\r\n\r\nx", bodies));

	FrameReader headers(Framing::ContentLength, {64, 16});
	CHECK(headers.feed(std::string(65, 'a'), bodies)); // headers over 64 bytes with no end
	FrameReader missing(Framing::ContentLength);
	CHECK(missing.feed("Content-Type: x\r\n\r\n", bodies));
	CHECK_THROWS_AS((void)affresco::services::contentLength("Content-Length: -1", 10), std::runtime_error);
	CHECK_THROWS_AS((void)affresco::services::contentLength("Content-Length: 99999999999999999999999", 10),
	                std::runtime_error);
	CHECK(affresco::services::contentLength("content-length:  7 ", 10) == 7);
	CHECK(affresco::services::frameMessage(Framing::ContentLength, "{}") == "Content-Length: 2\r\n\r\n{}");

	FrameReader lines(Framing::Lines, {64, 4});
	bodies.clear();
	CHECK_FALSE(lines.feed("ab\r\ncd\n", bodies));
	CHECK(bodies == std::vector<std::string>{"ab", "cd"});
	CHECK(lines.feed("toolong", bodies));

	// A line is within the limit without its line break: 4 bytes and "\r\n" pass, 5 bytes and "\n" don't
	FrameReader exact(Framing::Lines, {64, 4});
	bodies.clear();
	CHECK_FALSE(exact.feed("abcd\r", bodies));
	CHECK_FALSE(exact.feed("\n", bodies));
	CHECK(bodies == std::vector<std::string>{"abcd"});
	FrameReader over(Framing::Lines, {64, 4});
	bodies.clear();
	CHECK(over.feed("abcde\n", bodies));
	CHECK(bodies.empty());

	// Lines in pieces: the bytes searched before aren't searched again, and the next line starts afresh
	FrameReader pieces(Framing::Lines, {64, 8});
	bodies.clear();
	for(const char* piece : {"ab", "c", "de\r", "\nf", "g", "\n", "h"}) CHECK_FALSE(pieces.feed(piece, bodies));
	CHECK(bodies == std::vector<std::string>{"abcde", "fg"});
	CHECK_FALSE(pieces.feed("12345678", bodies)); // "h" and 7 bytes: the limit and room for a '\r'
	CHECK(pieces.feed("9", bodies));
}

TEST_CASE("services: a registry refuses what the host didn't grant") {
	affresco::services::ServiceRegistry registry;
	CHECK_THROWS_AS(registry.allowProcess("relative", "bin/tool"), std::runtime_error);
	CHECK_THROWS_AS(registry.addRoot("none", std::filesystem::temp_directory_path() / "affrescoui-no-such-folder"),
	                std::runtime_error);
	const std::vector<std::string> refused = registry.load(
	    {{"a", {{"provider", "process"}, {"process", "unknown"}}}, {"b", {{"provider", "fs"}, {"root", "unknown"}}},
	     {"c", {{"provider", "nothing"}}}});
	CHECK(refused.size() == 3);
	const json status = registry.status();
	CHECK(status["a"]["state"] == "refused");
	CHECK(status["b"]["state"] == "refused");
	CHECK(status["c"]["state"] == "refused");
	CHECK(registry.catalog().contains("jsonrpc"));
}

// A number too large for a 64-bit integer was converted to one to test whether it was whole: undefined behaviour
TEST_CASE("services: a schema's integer type takes whole numbers that fit, and refuses huge ones safely") {
	using affresco::services::checkSchema;
	const json integer = {{"type", "integer"}};
	CHECK_FALSE(checkSchema(integer, 3.0, "with"));
	CHECK_FALSE(checkSchema(integer, -9223372036854775808.0, "with"));
	CHECK(checkSchema(integer, 3.5, "with") == "with: must be a value of type integer");
	CHECK(checkSchema(integer, 1e300, "with") == "with: must be a value of type integer");
	CHECK(checkSchema(integer, -1e300, "with") == "with: must be a value of type integer");
	CHECK(checkSchema(integer, 9223372036854775808.0, "with") == "with: must be a value of type integer");
	CHECK(checkSchema({{"type", "string"}, {"maxLength", 1e300}}, "abc", "with") == std::nullopt);
	CHECK(checkSchema({{"type", "string"}, {"maxLength", 2}}, "abc", "with") == "with: at most 2 bytes");
	CHECK(checkSchema({{"type", "array"}, {"maxItems", 1}}, json::array({1, 2}), "with") == "with: at most 1 items");
}

// A debug adapter can run code (debugpy's `evaluate`): a screen attached to any program listening on a loopback port
TEST_CASE("services: a dap service attaches only to a port the host allows") {
	affresco::services::ServiceRegistry registry;
	const json section = {{"debug", {{"provider", "dap"}, {"attach", {{"port", 5678}}}}}};
	const std::vector<std::string> refused = registry.load(section);
	REQUIRE(refused.size() == 1);
	CHECK(refused[0] == "services.debug.attach.port: 5678 isn't a port the host allows attaching to");
	CHECK(registry.status()["debug"]["state"] == "refused");

	registry.allowAttach(5679);
	CHECK(registry.load(section).size() == 1);
	registry.allowAttach(5678);
	CHECK(registry.load(section).empty());
	CHECK(registry.status()["debug"]["state"] == "stopped");
}

TEST_CASE("services: revisions use declared, revisable methods only; writes are always reviewed") {
	const TemporaryFolder folder;
	affresco::services::ServiceRegistry registry;
	registry.addRoot("work", folder.path / "work");
	const json section = {{"files", {{"provider", "fs"}, {"root", "work"}}}};
	CHECK(registry.load(section).empty());
	affresco::format::Rules rules;
	rules.setServices(&registry);
	affresco::vcs::Repository repository(
	    rules, {{"format", "affrescoui-screen"},
	            {"version", 2},
	            {"services", section},
	            {"state", {{"text", ""}}},
	            {"actions", json::object()},
	            {"root", {{"type", "label"}, {"text", "{{state.text}}"}}}});
	const auto add = [&repository](const json& steps) {
		return repository.submit({{"author", "agent"}, {"add", {{"/actions/go", steps}}}});
	};

	CHECK(add(json::array({{{"invoke", "files.read"}, {"with", {{"path", "a"}}}, {"into", "state.text"}}})).status ==
	      RevisionStatus::Applied);
	const affresco::RevisionResult write =
	    add(json::array({{{"invoke", "files.write"}, {"with", {{"path", "a"}, {"text", "b"}}}}}));
	CHECK(write.status == RevisionStatus::Proposed);
	CHECK(add(json::array({{{"invoke", "other.read"}}})).status == RevisionStatus::Rejected);
	CHECK(add(json::array({{{"invoke", "files.nothing"}}})).status == RevisionStatus::Rejected);
	CHECK(add(json::array({{{"invoke", "{{state.text}}"}}})).status == RevisionStatus::Rejected);
	CHECK(add(json::array({{{"respond", "yes"}}})).status == RevisionStatus::Rejected);
	CHECK(repository.submit({{"author", "agent"}, {"set", {{"/services/files/root", "elsewhere"}}}}).status ==
	      RevisionStatus::Rejected);
}
