/// @file
/// @brief Unit tests of the whole editor loop (Screen::listenEditor(), SECURITY.md, "Editors (nREPL)"): a screen on a
/// small app written as a program, in a headless window, the server on port 0, and a client that speaks bencode over
/// real loopback TCP. Nothing of the app is read or changed before the person's own press on the pairing question;
/// after it, each evaluation is the person's change.

#include "Headless.hpp"
#include "HistoryFiles.hpp"
#include "NetServer.hpp"
#include "nrepl/Bencode.hpp"

#include <affresco/affresco.hpp>
#include <affresco/net/EditorServer.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/TextEditor.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using affresco::Panel;
using affresco::Vec2;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;
using affresco::net::bencode::Value;
namespace tcp = affresco::net::tcp;
using namespace std::chrono_literals;

namespace {

constexpr double answerDelayMs = 600.0; ///< More than the question waits before a yes counts (WindowConfirm.cpp).

constexpr const char* kManifest =
    R"lisp({:format affrescoui-app :version 1 :id nrepl-loop :name "Loop" :entry index.screen.lisp})lisp";

constexpr const char* kEntry = R"lisp(;;; index.screen.lisp --- a screen for the editor's loop

(include "views.lisp")

(defstate title "Loop" "The window's title.")

(defscreen main
  (panel#root ~(greeting)))
)lisp";

constexpr const char* kViews = R"lisp(;;; views.lisp --- the views

(defview greeting "Says hello." []
  `(label#hello :text "Hello"))

(defn helper "Gives one." [] 1)
)lisp";

constexpr const char* kTitle = "Let this editor evaluate as you?"; ///< The pairing question's title.

/// @brief A folder of its own for the app and its history, removed after the test.
struct Folder {
	/// @brief Creates it, with the app in it.
	Folder() {
		std::filesystem::remove_all(path);
		write("app/app.lisp", kManifest);
		write("app/index.screen.lisp", kEntry);
		write("app/views.lisp", kViews);
	}
	Folder(const Folder&) = delete;
	Folder& operator=(const Folder&) = delete;
	/// @brief Removes it.
	~Folder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
	/// @brief Writes a file in it. @param name Its path in the folder. @param text What it holds.
	void write(const std::string& name, const std::string& text) const {
		std::filesystem::create_directories((path / name).parent_path());
		std::ofstream(path / name, std::ios::binary) << text;
	}
	/// The folder.
	std::filesystem::path path = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-nrepl-loop-" + std::to_string(_getpid()));
#else
	                             ("affrescoui-nrepl-loop-" + std::to_string(getpid()));
#endif
};

/// @brief The app on a headless window, its history in the folder, and the editor server on 127.0.0.1, port 0.
struct Loop {
	Folder folder;                            ///< The app's files and history.
	ManualClock clock;                        ///< The window's clock.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::test::ThreadExecutor executor;  ///< The main thread's tasks (the server's messages).
	std::optional<affresco::Screen> screen;   ///< The screen; reset to destroy it first.
	std::uint16_t port = 0;                   ///< The server's port.

	/// @brief Opens the app, builds it and listens.
	Loop() : window(createHeadlessWindow(500, 400, clock)) {
		affresco::Screen& opened = screen.emplace(
		    affresco::Screen::fromApp(folder.path / "app", folder.path / "data", affresco::Screen::History::InMemory));
		opened.setHistoryDirectory(folder.path / "history");
		opened.build(*window);
		window->render();
		port = opened.listenEditor(executor);
	}
	~Loop() { screen.reset(); } // before the window
	Loop(const Loop&) = delete;
	Loop& operator=(const Loop&) = delete;

	/// @brief The screen, while it lives.
	/// @return It.
	[[nodiscard]] affresco::Screen& shown() {
		if(!screen.has_value()) throw std::logic_error("the screen is gone");
		return *screen;
	}

	/// @brief Runs the main thread a little: the server's tasks, and a frame.
	void step() {
		executor.runFor(2ms);
		if(window) window->render();
	}

	/// @brief The absolute path of one of the app's files, as an editor sends it.
	/// @param name The file. @return Its path.
	[[nodiscard]] std::string pathOf(const std::string& name) const {
		return std::filesystem::absolute(folder.path / "app" / name).lexically_normal().string();
	}

	/// @brief The text of the label with the id `hello`. @return It; "" if there is none.
	[[nodiscard]] std::string hello() const {
		const auto* label = dynamic_cast<affresco::Label*>(window->root().findById("hello"));
		return label != nullptr ? label->text() : std::string();
	}
};

/// @brief An nREPL client over loopback TCP.
struct Client {
	/// @brief Connects. @param loop The app.
	explicit Client(Loop& loop) : loop_(loop), connection_(tcp::Connection::open("127.0.0.1", loop.port)) {
		REQUIRE(connection_ != nullptr);
	}

	/// @brief Sends a request. @param request Its entries; an `id` is added. @return The id.
	std::string send(Value::Dictionary request) {
		const std::string id = std::to_string(++lastId_);
		request.emplace("id", Value(id));
		REQUIRE(connection_->send(affresco::net::bencode::encode(Value(std::move(request))), 5s));
		return id;
	}

	/// @brief Runs the main thread and reads what arrives, until a condition holds.
	/// @param done The condition. @param timeout The longest wait. @return Whether it holds.
	bool until(const std::function<bool()>& done, std::chrono::milliseconds timeout = 10s) {
		const auto deadline = std::chrono::steady_clock::now() + timeout;
		while(!done()) {
			if(std::chrono::steady_clock::now() > deadline) return false;
			loop_.step();
			read();
		}
		return true;
	}

	/// @brief Waits for a request's answer to end (a `done` status). @param id The request. @return Its messages.
	std::vector<Value> answer(const std::string& id) {
		CHECK(until([&] { return ended(id); }));
		return messagesOf(id);
	}

	/// @brief The messages received for a request. @param id The request. @return They.
	[[nodiscard]] std::vector<Value> messagesOf(const std::string& id) const {
		std::vector<Value> out;
		for(const Value& message : received_)
			if(const Value* found = message.find("id"); found != nullptr && found->isString() && found->string() == id)
				out.push_back(message);
		return out;
	}

	/// @brief Whether a request's answer ended. @param id The request. @return true if a `done` came.
	[[nodiscard]] bool ended(const std::string& id) const { return hasStatus(messagesOf(id), "done"); }

	/// @brief Whether the server closed the connection. @return true if it did.
	[[nodiscard]] bool closed() const noexcept { return closed_; }

private:
	/// @brief Reads what has arrived, without waiting long.
	void read() {
		if(closed_) return;
		const tcp::Received got = connection_->receive(buffer_, 1ms);
		if(got == tcp::Received::Closed || got == tcp::Received::Failed) closed_ = true;
		for(;;) {
			affresco::net::bencode::Decoded decoded = affresco::net::bencode::decode(buffer_, {});
			if(decoded.status != affresco::net::bencode::Read::Value) break;
			received_.push_back(std::move(decoded.value));
			buffer_.erase(0, decoded.used);
		}
	}

public:
	/// @brief Whether one of some messages has a status word. @param messages They. @param word It. @return true if so.
	static bool hasStatus(const std::vector<Value>& messages, std::string_view word) {
		return std::ranges::any_of(messages, [&](const Value& message) {
			const Value* status = message.find("status");
			return status != nullptr && status->isList() && std::ranges::any_of(status->list(), [&](const Value& item) {
				       return item.isString() && item.string() == word;
			       });
		});
	}

	/// @brief The first text of a key in some messages. @param messages They. @param key It. @return It, or "".
	static std::string first(const std::vector<Value>& messages, std::string_view key) {
		for(const Value& message : messages)
			if(const Value* found = message.find(key); found != nullptr && found->isString()) return found->string();
		return "";
	}

private:
	Loop& loop_;                                  ///< The app.
	std::unique_ptr<tcp::Connection> connection_; ///< The socket.
	std::string buffer_;                          ///< Bytes not decoded yet.
	std::vector<Value> received_;                 ///< Every message, in order.
	int lastId_ = 0;                              ///< The last request's id.
	bool closed_ = false;                         ///< The server closed it.
};

/// @brief A text value. @param text It. @return The value.
Value text(std::string text) { return Value(std::move(text)); }

/// @brief An `eval` request. @param code The code. @param file The editor's file, or "". @return The request.
Value::Dictionary evalOf(const std::string& code, const std::string& file = "") {
	Value::Dictionary request{{"op", text("eval")}, {"code", text(code)}};
	if(!file.empty()) request.emplace("file", text(file));
	return request;
}

/// @brief A panel of the question that is a button with a name.
/// @param panel Where to look. @param name The name. @return It, or nullptr.
Panel* button(Panel* panel, const std::string& name) {
	if(!panel) return nullptr;
	if(panel->accessibleRole() == "button" && panel->accessibleLabel() == name) return panel;
	for(const auto& child : panel->children())
		if(Panel* found = button(child.get(), name)) return found;
	return nullptr;
}

/// @brief Whether a label under a panel shows a text. @param panel Where to look. @param shown It. @return true if so.
bool showsLabel(Panel* panel, const std::string& shown) {
	if(!panel) return false;
	if(const auto* label = dynamic_cast<affresco::Label*>(panel); label != nullptr && label->text() == shown)
		return true;
	return std::ranges::any_of(panel->children(), [&](const auto& child) { return showsLabel(child.get(), shown); });
}

/// @brief The text the question shows in its box. @param panel Where to look. @return It; "" if none.
std::string shownText(Panel* panel) {
	if(!panel) return "";
	if(const auto* editor = dynamic_cast<affresco::TextEditor*>(panel)) return editor->text();
	for(const auto& child : panel->children())
		if(std::string found = shownText(child.get()); !found.empty()) return found;
	return "";
}

/// @brief Clicks one of the question's buttons with the person's own mouse, once the question has been shown long
/// enough for a press to count.
/// @param loop The app. @param name The button's name.
void press(Loop& loop, const std::string& name) {
	loop.clock.advance(answerDelayMs);
	loop.window->render();
	static_cast<void>(loop.window->panelAt({})); // laid out
	Panel* found = button(loop.window->confirmation(), name);
	REQUIRE(found != nullptr);
	const affresco::Rect& frame = found->frame();
	const Vec2 at{(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
	loop.window->routeMouseMove(at);
	loop.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
	loop.window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
	loop.window->render();
}

/// @brief Pairs a client: an `eval` that asks, and the person's yes. @param loop The app. @param client The client.
void pair(Loop& loop, Client& client) {
	const std::string id = client.send(evalOf("(+ 1 2)"));
	REQUIRE(client.until([&] { return loop.window->confirmation() != nullptr; }));
	press(loop, "Let It Evaluate");
	const std::vector<Value> answer = client.answer(id);
	REQUIRE(Client::first(answer, "value") == "3");
}

/// @brief The text of a commit object in the history folder. @param loop The app. @param id The commit. @return It.
std::string commitObject(const Loop& loop, const std::string& id) {
	return affresco::test::readText(affresco::test::objectFile(loop.folder.path / "history", id));
}

} // namespace

TEST_CASE("nrepl_loop: before the person's yes, nothing of the app is read or changed") {
	Loop loop;
	const std::string document = loop.shown().documentJson();
	const std::string state = loop.shown().stateJson();
	const std::size_t commits = loop.shown().log().size();
	Client client(loop);
	const std::vector<Value> described = client.answer(client.send({{"op", text("describe")}}));
	CHECK(described.front().find("ops") != nullptr);
	const std::vector<Value> cloned = client.answer(client.send({{"op", text("clone")}}));
	CHECK_FALSE(Client::first(cloned, "new-session").empty());
	const std::string id = client.send(evalOf("state.title"));
	REQUIRE(client.until([&] { return loop.window->confirmation() != nullptr; }));
	client.until([] { return false; }, 100ms);
	CHECK(client.messagesOf(id).empty()); // no value: it waits for the person
	CHECK(showsLabel(loop.window->confirmation(), kTitle));
	CHECK(shownText(loop.window->confirmation()) == "state.title");
	CHECK(loop.shown().documentJson() == document);
	CHECK(loop.shown().stateJson() == state);
	CHECK(loop.shown().log().size() == commits);
}

TEST_CASE("nrepl_loop: a yes answers what waited, and an evaluation is the person's change") {
	Loop loop;
	Client client(loop);
	const std::string waiting = client.send(evalOf("state.title"));
	REQUIRE(client.until([&] { return loop.window->confirmation() != nullptr; }));
	press(loop, "Let It Evaluate");
	CHECK(Client::first(client.answer(waiting), "value") == "\"Loop\"");
	CHECK(loop.window->confirmation() == nullptr);
	CHECK(loop.hello() == "Hello");
	const std::size_t commits = loop.shown().log().size();
	const std::vector<Value> changed = client.answer(client.send(evalOf(
	    "(defview greeting \"Says hello.\" []\n  `(label#hello :text \"Changed\"))", loop.pathOf("views.lisp"))));
	CHECK(Client::first(changed, "err").empty());
	CHECK(Client::first(changed, "value") == "greeting");
	loop.window->render();
	CHECK(loop.hello() == "Changed");
	const std::vector<affresco::CommitInfo> log = loop.shown().log();
	REQUIRE(log.size() == commits + 1U);
	CHECK(log.back().author == "user");
	CHECK(log.back().message.ends_with("(editor)"));
	CHECK(commitObject(loop, log.back().id).contains("\"ownInput\""));
	CHECK(loop.window->confirmation() == nullptr); // a paired connection isn't asked again
}

TEST_CASE("nrepl_loop: a no ends the request with not-paired, closes the connection, and lands nothing") {
	Loop loop;
	const std::size_t commits = loop.shown().log().size();
	Client client(loop);
	const std::string id = client.send(evalOf(R"((set "state.title" "Mine"))"));
	REQUIRE(client.until([&] { return loop.window->confirmation() != nullptr; }));
	press(loop, "Refuse");
	const std::vector<Value> answer = client.answer(id);
	CHECK(Client::hasStatus(answer, "not-paired"));
	CHECK(client.until([&] { return client.closed(); }));
	CHECK(loop.shown().log().size() == commits);
	CHECK(loop.shown().stateJson().find("Mine") == std::string::npos);
}

TEST_CASE("nrepl_loop: every connection is asked with its own text, and three refusals end the asking") {
	Loop loop;
	for(int refusal = 0; refusal < 3; ++refusal) {
		Client client(loop);
		const std::string code = "(+ " + std::to_string(refusal) + " 1)";
		const std::string id = client.send(evalOf(code));
		REQUIRE(client.until([&] { return loop.window->confirmation() != nullptr; }));
		CHECK(shownText(loop.window->confirmation()) == code);
		press(loop, "Refuse");
		CHECK(Client::hasStatus(client.answer(id), "not-paired"));
	}
	Client fourth(loop);
	const std::string id = fourth.send(evalOf("(+ 1 1)"));
	const std::vector<Value> answer = fourth.answer(id);
	CHECK(Client::hasStatus(answer, "not-paired"));
	CHECK(loop.window->confirmation() == nullptr);
}

TEST_CASE("nrepl_loop: a first text with a character that draws nothing asks nothing") {
	Loop loop;
	Client client(loop);
	const std::vector<Value> answer = client.answer(client.send(evalOf("(+ 1 1)\xE2\x80\x8B")));
	CHECK(Client::hasStatus(answer, "error"));
	CHECK(loop.window->confirmation() == nullptr);
	client.until([] { return false; }, 100ms);
	CHECK_FALSE(client.closed());
	const std::vector<Value> described = client.answer(client.send({{"op", text("describe")}}));
	CHECK(described.front().find("ops") != nullptr);
}

TEST_CASE("nrepl_loop: while a proposal is previewed, an evaluation lands nothing") {
	Loop loop;
	Client client(loop);
	pair(loop, client);
	const affresco::RevisionResult proposed = loop.shown().versions().propose(
	    {{"author", "agent/x"}, {"message", "a title"}, {"set", {{"/state/title", "Proposed"}}}});
	REQUIRE(proposed.proposal != 0);
	REQUIRE_FALSE(loop.shown().versions().preview(proposed.proposal).has_value());
	const std::size_t commits = loop.shown().log().size();
	const std::vector<Value> answer = client.answer(client.send(evalOf(R"((set "state.title" "Mine"))")));
	CHECK_FALSE(Client::first(answer, "err").empty());
	CHECK(loop.shown().log().size() == commits);
	CHECK(loop.shown().stateJson().find("Mine") == std::string::npos);
}

TEST_CASE("nrepl_loop: an error names its line in the editor's buffer") {
	Loop loop;
	Client client(loop);
	pair(loop, client);
	Value::Dictionary request = evalOf("(+ 1 1)\n(helper 1 2 3)");
	request.emplace("line", Value(std::int64_t{40}));
	request.emplace("column", Value(std::int64_t{1}));
	const std::vector<Value> answer = client.answer(client.send(std::move(request)));
	const std::string error = Client::first(answer, "err");
	INFO(error);
	CHECK(error.contains(":41:"));
}

TEST_CASE("nrepl_loop: load-file lands what changed, in the file the editor named") {
	Loop loop;
	Client client(loop);
	pair(loop, client);
	const std::size_t commits = loop.shown().log().size();
	std::string changed = kViews;
	changed.replace(changed.find("[] 1)"), 5, "[] 2)");
	const std::vector<Value> answer = client.answer(client.send({{"op", text("load-file")},
	                                                             {"file", text(changed)},
	                                                             {"file-path", text(loop.pathOf("views.lisp"))},
	                                                             {"file-name", text("views.lisp")}}));
	CHECK(Client::first(answer, "err").empty());
	REQUIRE(loop.shown().log().size() == commits + 1U);
	const affresco::json document = affresco::json::parse(loop.shown().documentJson());
	CHECK(loop.shown().versions().programAuthors()["views.lisp"]["helper"] == "user");
	CHECK(loop.shown().versions().programAuthors()["views.lisp"]["greeting"] == "file");
	static_cast<void>(document);
}

TEST_CASE("nrepl_loop: completions and lookup") {
	Loop loop;
	Client client(loop);
	pair(loop, client);
	const std::vector<Value> completions =
	    client.answer(client.send({{"op", text("completions")}, {"prefix", text("ma")}}));
	bool hasMap = false;
	for(const Value& message : completions)
		if(const Value* list = message.find("completions"); list != nullptr && list->isList())
			for(const Value& item : list->list())
				if(const Value* candidate = item.find("candidate");
				   candidate != nullptr && candidate->string() == "map")
					hasMap = true;
	CHECK(hasMap);
	const std::vector<Value> lookup = client.answer(client.send({{"op", text("lookup")}, {"sym", text("helper")}}));
	const Value* info = lookup.front().find("info");
	REQUIRE(info != nullptr);
	REQUIRE(info->find("file") != nullptr);
	CHECK(std::filesystem::equivalent(info->find("file")->string(), loop.pathOf("views.lisp")));
	CHECK(info->find("line")->integer() == 6);
}

TEST_CASE("nrepl_loop: the screen destroyed with a connection open and a question waiting") {
	Loop loop;
	Client paired(loop);
	pair(loop, paired);
	Client asking(loop);
	static_cast<void>(asking.send(evalOf("(+ 1 1)")));
	REQUIRE(asking.until([&] { return loop.window->confirmation() != nullptr; }));
	loop.screen.reset();
	CHECK(loop.window->confirmation() == nullptr); // the question went with its server
	loop.executor.runFor(50ms);                    // what the server's threads posted finds nothing to reach
	loop.window->render();
	CHECK(asking.until([&] { return asking.closed(); }, 5s));
}

TEST_CASE("nrepl_loop: a screen written as JSON takes no editor") {
	const Folder folder;
	folder.write("plain.screen.json",
	             R"({"format": "affrescoui-screen", "version": 2, "root": {"type": "label", "id": "x", "text": "x"}})");
	ManualClock clock;
	const std::unique_ptr<affresco::Window> window = createHeadlessWindow(200, 100, clock);
	affresco::test::ThreadExecutor executor;
	{
		affresco::Screen screen = affresco::Screen::fromFile(folder.path / "plain.screen.json", folder.path / "data",
		                                                     affresco::Screen::History::InMemory);
		screen.build(*window);
		CHECK_THROWS_WITH_AS(static_cast<void>(screen.listenEditor(executor)), doctest::Contains("this one is JSON"),
		                     std::runtime_error);
	}
}
