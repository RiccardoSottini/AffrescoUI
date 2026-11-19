/// @file
/// @brief Verify mode: after every partial update a full pass over the same panels must give the same panels, or the
/// first difference is reported, naming the node and the property, without the values.

#include "Headless.hpp"
#include "Helpers.hpp"
#include "Verify.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <cstdlib>
#include <string>
#include <vector>

using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A counter with a label that reads it, and one that doesn't.
constexpr const char* counter = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "count": 0, "secret": "s3cr3t" },
	"actions": { "count": [{ "increment": "state.count" }], "secret": [{ "set": "state.secret", "to": "{{state.count}}x" }] },
	"root": { "type": "panel", "children": [
		{ "type": "label", "id": "count", "text": "{{state.count}}" },
		{ "type": "label", "id": "secret", "text": "{{state.secret}}" },
		{ "type": "label", "id": "fixed", "text": "fixed" }
	] }
})json";

/// @brief A screen made in verify mode (AFFRESCOUI_VERIFY_UPDATES=1 while it is made), with its errors collected.
struct Verified {
	ManualClock clock;                                                                ///< The window's clock.
	std::unique_ptr<affresco::Window> window = createHeadlessWindow(320, 240, clock); ///< The window.
	affresco::Screen screen;                                                          ///< The screen.
	std::vector<std::string> errors;                                                  ///< What it reported.
	affresco::Connection listening;                                                   ///< The errors' listener.

	/// @brief Makes and builds the screen.
	/// @param document The document.
	explicit Verified(const char* document) : screen(made(document)) {
		listening = screen.onError([this](const std::string& message) { errors.push_back(message); });
		screen.build(*window);
		window->render();
	}

	/// @brief A screen made with the environment variable set.
	/// @param document The document.
	/// @return It.
	static affresco::Screen made(const char* document) {
		affresco::test::verifyNextRuntimes(true);
		affresco::Screen screen = affresco::Screen::fromString(document);
		affresco::test::verifyNextRuntimes(false);
		return screen;
	}

	/// @brief The errors that are update checks.
	/// @return They.
	[[nodiscard]] std::vector<std::string> checks() const {
		std::vector<std::string> found;
		for(const std::string& error : errors)
			if(error.contains("update check")) found.push_back(error);
		return found;
	}
};

} // namespace

TEST_CASE("update check: partial updates that read what they should report nothing") {
	Verified verified(counter);
	for(int i = 0; i < 5; ++i) {
		verified.screen.runAction(i % 2 ? "secret" : "count", verified.window->root());
		verified.window->render();
	}
	CHECK(verified.checks().empty());
}

TEST_CASE("update check: a read dropped is reported once, naming the node and the property, not the values") {
	// The difference made on purpose stays out of the test's output, where CI's verify mode looks for real ones
	affresco::setLogSink([](affresco::LogLevel, std::string_view) {});
	affresco::format::testing::dropReadsOf("secret");
	Verified verified(counter);
	verified.screen.runAction("count", verified.window->root()); // nothing reads count but #count
	verified.window->render();
	CHECK(verified.checks().empty());

	verified.screen.runAction("secret", verified.window->root()); // #secret dropped its read: the update misses it
	verified.window->render();
	affresco::format::testing::dropReadsOf({});
	affresco::setLogSink(nullptr);
	const std::vector<std::string> checks = verified.checks();
	REQUIRE(checks.size() == 1);
	CHECK(checks.front() == "update check: root.children[1].values[0]: a partial update and a full update differ");
	CHECK_FALSE(checks.front().contains("s3cr3t"));
	CHECK_FALSE(checks.front().contains("1x"));
}

TEST_CASE("update check: a difference in a secret field logs where it is, and not what it holds") {
	constexpr const char* form = R"json({
		"format": "affrescoui-screen",
		"version": 2,
		"state": { "word": "first-word" },
		"actions": { "change": [{ "set": "state.word", "to": "second-word" }] },
		"root": { "type": "panel", "children": [
			{ "type": "textarea", "id": "word", "text": "{{state.word}}", "secret": true }
		] }
	})json";
	std::string logged;
	affresco::setLogSink([&](affresco::LogLevel, std::string_view message) { logged += message; });
	affresco::format::testing::dropReadsOf("word");
	Verified verified(form);
	verified.screen.runAction("change", verified.window->root()); // #word dropped its read: the update misses it
	verified.window->render();
	affresco::format::testing::dropReadsOf({});
	affresco::setLogSink(nullptr);
	CHECK(verified.checks().size() == 1);
	CHECK(logged.contains("update check: root.children[0]"));
	CHECK_FALSE(logged.contains("first-word"));
	CHECK_FALSE(logged.contains("second-word"));
}
