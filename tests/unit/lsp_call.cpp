/// @file
/// @brief What Forge's language server client sends with (examples/forge/features/lsp/LspCall.hpp): the screen's
/// arguments read within their limits, columns in UTF-16 units, and a server's answer handled once, on the main
/// thread, only while the client is alive.

#include "features/lsp/LspCall.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using affresco::json;
namespace lsp = forge::lsp;

namespace {

/// @brief An executor that keeps what is posted until the test runs it, as the main thread would later.
class QueuedExecutor final : public affresco::Executor {
public:
	void post(std::function<void()> task) override { tasks_.push_back(std::move(task)); }

	/// @brief Runs the tasks posted until now.
	/// @return How many ran.
	std::size_t drain() {
		const std::vector<std::function<void()>> tasks = std::exchange(tasks_, {});
		for(const auto& task : tasks) task();
		return tasks.size();
	}

private:
	std::vector<std::function<void()>> tasks_; ///< Posted, not yet run.
};

/// @brief What a call's done callback received.
struct Answer {
	int calls = 0;              ///< How many times it ran.
	std::optional<json> result; ///< The last result.
	std::string error;          ///< The last error.
};

/// @brief A done callback that records into an answer.
/// @param answer Where it records.
/// @return The callback.
lsp::ServerCall::Done recordInto(Answer& answer) {
	return [&answer](std::optional<json> result, const std::string& error) {
		++answer.calls;
		answer.result = std::move(result);
		answer.error = error;
	};
}

} // namespace

TEST_SUITE("lsp call") {
	TEST_CASE("text arguments: a string within its limit, else empty or refused") {
		const json with = {{"query", "main"}, {"count", 3}};
		CHECK(lsp::textArgument(with, "query", 4) == "main");
		CHECK(lsp::textArgument(with, "count", 4).empty());
		CHECK(lsp::textArgument(with, "missing", 4).empty());
		CHECK(lsp::textArgument(json::array({"main"}), "query", 4).empty());
		CHECK_THROWS_AS((void)lsp::textArgument(with, "query", 3), std::runtime_error);
	}

	TEST_CASE("position arguments: a positive whole number, capped, else 1") { // unsigned, as parsed from text
		CHECK(lsp::positionArgument({{"line", 12U}}, "line") == 12);
		CHECK(lsp::positionArgument({{"line", 0U}}, "line") == 1);
		CHECK(lsp::positionArgument({{"line", -4}}, "line") == 1);
		CHECK(lsp::positionArgument({{"line", 2.5}}, "line") == 1);
		CHECK(lsp::positionArgument({{"line", "7"}}, "line") == 1);
		CHECK(lsp::positionArgument({{"line", 1ULL << 40}}, "line") == std::size_t{1} << 30);
		CHECK(lsp::positionArgument(json::array({5U}), "line") == 1);
	}

	TEST_CASE("columns: code points to UTF-16 units and back, past the end too") {
		const std::u32string line = U"a\U0001F600b"; // a, an emoji (two units), b
		CHECK(lsp::utf16Units(line, 0) == 0);
		CHECK(lsp::utf16Units(line, 2) == 3);
		CHECK(lsp::utf16Units(line, 3) == 4);
		CHECK(lsp::utf16Units(line, 5) == 6);
		CHECK(lsp::codePointsOf(line, 1) == 1);
		CHECK(lsp::codePointsOf(line, 2) == 2); // inside the pair: the whole emoji
		CHECK(lsp::codePointsOf(line, 3) == 2);
		CHECK(lsp::codePointsOf(line, 4) == 3);
		CHECK(lsp::codePointsOf(line, 6) == 5);
	}
}

TEST_SUITE("lsp call") { // the calls themselves
	TEST_CASE("a call's answer arrives once, on the main thread") {
		QueuedExecutor executor;
		const auto alive = std::make_shared<bool>(true);
		Answer answer;
		lsp::ServerCall call({{"uri", "file:///a.cpp"}}, &executor, alive, recordInto(answer));
		CHECK(call.arguments() == json{{"uri", "file:///a.cpp"}});
		call.progress(json{{"done", 1}});
		call.finish(json{{"items", 2}});
		call.fail("busy", "too late", nullptr);
		call.finish(json{{"items", 3}});
		CHECK(answer.calls == 0);
		CHECK(executor.drain() == 1);
		CHECK(answer.calls == 1);
		CHECK(answer.result == json{{"items", 2}});
		CHECK(answer.error.empty());
	}

	TEST_CASE("a failed call gives no result and the message") {
		QueuedExecutor executor;
		const auto alive = std::make_shared<bool>(true);
		Answer answer;
		lsp::ServerCall call(json::object(), &executor, alive, recordInto(answer));
		call.fail("unavailable", "no server", nullptr);
		executor.drain();
		CHECK(answer.calls == 1);
		CHECK_FALSE(answer.result.has_value());
		CHECK(answer.error == "no server");
	}

	TEST_CASE("a subscription's events stop with stop() and with its end") {
		QueuedExecutor executor;
		const auto alive = std::make_shared<bool>(true);
		std::vector<json> events;
		lsp::ServerCall call(json::object(), &executor, alive, {},
		                     [&events](const json& event) { events.push_back(event); });
		call.event(json{{"n", 1}});
		executor.drain();
		CHECK(events == std::vector<json>{json{{"n", 1}}});
		CHECK_FALSE(call.stopToken().stop_requested());
		call.stop();
		CHECK(call.stopToken().stop_requested());
		call.event(json{{"n", 2}});
		call.finish(json{}); // no done callback: nothing to post
		CHECK(executor.drain() == 0);
		CHECK(events.size() == 1);
	}

	TEST_CASE("an answer for a client that is gone, or with no main thread, is dropped") {
		QueuedExecutor executor;
		auto alive = std::make_shared<bool>(true);
		Answer answer;
		lsp::ServerCall call(json::object(), &executor, alive, recordInto(answer));
		call.finish(json{{"late", true}});
		alive.reset();
		CHECK(executor.drain() == 1);
		CHECK(answer.calls == 0);

		Answer unposted;
		lsp::ServerCall headless(json::object(), nullptr, std::make_shared<bool>(true), recordInto(unposted));
		headless.finish(json{});
		headless.event(json{});
		CHECK(unposted.calls == 0);
	}
}
