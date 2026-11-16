/// @file
/// @brief The assistant add-on's input takes Lisp (examples/shared/addons/assistant, "The input takes Lisp"), through
/// tests/render/screens/assistant-lisp.screen.lisp, a small program that includes its Lisp version: a draft that
/// reads as forms the program knows shows what evaluating it would do, Enter evaluates it as the person's and its
/// value or error joins the conversation, a draft the person didn't type waits for their yes to the window's own
/// question, and anything else, or Cmd+Enter (Ctrl+Enter), stays a request to the agents.

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>
#include <affresco/render/TextEditor.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using affresco::json;
using affresco::Key;
using affresco::Panel;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

constexpr int shiftKey = 0x0001;   ///< GLFW_MOD_SHIFT.
constexpr int controlKey = 0x0002; ///< GLFW_MOD_CONTROL.
constexpr int superKey = 0x0008;   ///< GLFW_MOD_SUPER (Cmd).

/// The modifier of "mod" keys: Cmd on macOS, Ctrl elsewhere.
#ifdef __APPLE__
constexpr int modKey = superKey;
#else
constexpr int modKey = controlKey;
#endif

constexpr double answerDelayMs = 600.0; ///< More than the window's question waits before a yes counts.

/// @brief The screen: a program that includes the add-on's Lisp version.
/// @return Its path.
std::filesystem::path screenFile() {
	return std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / ".." / "tests" / "render" / "screens" /
	       "assistant-lisp.screen.lisp";
}

/// @brief The screen in a headless window, with an agent connected in the process, its panel open.
struct Built {
	affresco::test::MemoryChannel agent; ///< What the agent is sent; it outlives the screen.
	/// The folder of its "persist" state: unique to this process, so parallel runs don't meet.
	std::filesystem::path data = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-assistant-lisp-" + std::to_string(_getpid()));
#else
	                             ("affrescoui-assistant-lisp-" + std::to_string(getpid()));
#endif
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	std::unique_ptr<affresco::Screen> screen; ///< The screen.

	/// @brief Loads and builds the screen, connects the agent and opens the panel.
	Built() : window(createHeadlessWindow(800, 700, clock)) {
		std::filesystem::remove_all(data);
		std::filesystem::create_directories(data);
		screen = std::make_unique<affresco::Screen>(
		    affresco::Screen::fromFile(screenFile(), data, affresco::Screen::History::InMemory));
		screen->build(*window);
		window->render();
		screen->connect(agent, "agent");
		run("assistant.open");
	}
	Built(const Built&) = delete;
	Built& operator=(const Built&) = delete;
	/// @brief Takes the screen down before its folder goes.
	~Built() {
		screen.reset();
		std::error_code ignored;
		std::filesystem::remove_all(data, ignored);
	}

	/// @brief Runs a document action, then lets its deferred steps finish.
	/// @param name The action.
	void run(const std::string& name) {
		screen->runAction(name, window->root(), "{}");
		settle();
	}

	/// @brief Renders frames a quarter of a second apart, so animations and deferred steps finish.
	void settle() {
		for(int frame = 0; frame < 4; ++frame) {
			clock.advance(250.0);
			window->render();
		}
	}

	/// @brief Types characters into the focused field, as the person's keys do.
	/// @param text The characters (ASCII).
	void type(const std::string& text) const {
		for(const char c : text) window->routeChar({static_cast<char32_t>(c)});
		window->render();
	}

	/// @brief Presses and releases a key.
	/// @param key The key.
	/// @param mods The modifier bits held.
	void press(Key key, int mods = 0) {
		window->routeKey({key, 0, affresco::Action::Press, {mods}});
		window->routeKey({key, 0, affresco::Action::Release, {mods}});
		settle();
	}

	/// @brief Puts a text on the clipboard and pastes it into the focused field with the person's own key.
	/// @param text The text.
	void paste(const std::string& text) {
		window->setClipboardText(text);
		press(Key::V, controlKey);
	}

	/// @brief Whether a panel with this id exists and is shown.
	/// @param id The id.
	/// @return Whether it is.
	[[nodiscard]] bool shown(const std::string& id) const {
		const Panel* panel = screen->find(id);
		return panel != nullptr && panel->isVisible();
	}

	/// @brief The text of the label with this id.
	/// @param id The id.
	/// @return Its text, its runs' when it has runs; "" if there is none.
	[[nodiscard]] std::string text(const std::string& id) const {
		const auto* label = dynamic_cast<const affresco::Label*>(screen->find(id));
		return label == nullptr ? std::string() : label->text();
	}

	/// @brief Reads the add-on's state.
	/// @return state.assistant.
	[[nodiscard]] json assistant() const { return json::parse(screen->stateJson())["assistant"]; }

	/// @brief The requests the agent was sent.
	/// @return Their texts, oldest first.
	[[nodiscard]] std::vector<std::string> requests() const {
		std::vector<std::string> found;
		for(const json& message : agent.sent)
			if(message.value("type", "") == "request") found.push_back(message.value("text", ""));
		return found;
	}

	/// @brief Clicks one of the window's question's buttons, once it has been shown long enough, as the person does.
	/// @param name The button's accessible name.
	void answer(const std::string& name) {
		clock.advance(answerDelayMs);
		window->render();
		const Panel* button = labelled(window->confirmation(), name);
		REQUIRE(button != nullptr);
		const affresco::Rect& frame = button->frame();
		const affresco::Vec2 at{(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
		window->routeMouseMove(at);
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		settle();
	}

	/// @brief Finds a panel by its accessible label.
	/// @param in Where to look (it and its descendants), or nullptr.
	/// @param label The label.
	/// @return The first found, depth first, or nullptr.
	/// @brief Finds the first text editor in a panel and its descendants.
	/// @param in Where to look, or nullptr.
	/// @return The editor, depth first, or nullptr.
	[[nodiscard]] static const affresco::TextEditor* editorIn(const Panel* in) {
		if(in == nullptr) return nullptr;
		if(const auto* editor = dynamic_cast<const affresco::TextEditor*>(in)) return editor;
		for(const std::unique_ptr<Panel>& child : in->children())
			if(const affresco::TextEditor* found = editorIn(child.get())) return found;
		return nullptr;
	}

	[[nodiscard]] static const Panel* labelled(const Panel* in, const std::string& label) {
		if(in == nullptr || in->accessibleLabel() == label) return in;
		for(const std::unique_ptr<Panel>& child : in->children())
			if(const Panel* found = labelled(child.get(), label)) return found;
		return nullptr;
	}
};

} // namespace

TEST_CASE("assistant_lisp_input: a typed definition is previewed, and Enter lands it as the person's") {
	Built built;
	const int head = built.screen->revision();
	built.type(R"((defn greet [] "hi"))");
	REQUIRE(built.assistant()["draft"] == R"((defn greet [] "hi"))");
	REQUIRE(built.shown("assistantFormPreview"));
	const std::string preview = built.text("assistantFormText");
	CHECK(preview.contains("defines"));
	CHECK(preview.contains("greet"));
	CHECK(preview.contains("evaluated.lisp"));
	CHECK(preview.contains("Ask instead"));
	CHECK_FALSE(preview.contains("asked to confirm"));
	const affresco::TextEditor* field = Built::editorIn(built.screen->find("assistantComposer"));
	REQUIRE(field != nullptr);
	CHECK(field->caret().line == 0); // the preview appearing leaves the caret after what was typed
	CHECK(field->caret().column == std::string(R"((defn greet [] "hi"))").size());
	CHECK(Built::labelled(built.screen->find("assistantComposer"), "Evaluate") != nullptr);

	built.press(Key::Enter);
	CHECK(built.window->confirmation() == nullptr); // typed here: no question
	CHECK(built.screen->revision() == head + 1);
	REQUIRE_FALSE(built.screen->log().empty());
	CHECK(built.screen->log().back().author == "user");
	const json assistant = built.assistant();
	CHECK(assistant["draft"] == "");
	REQUIRE(assistant["messages"].size() == 2);
	CHECK(assistant["messages"][0]["kind"] == "code");
	CHECK(assistant["messages"][0]["mine"] == true);
	CHECK(assistant["messages"][0]["text"] == R"((defn greet [] "hi"))");
	CHECK(assistant["messages"][1]["kind"] == "result");
	CHECK(assistant["messages"][1]["error"] == false);
	CHECK(assistant["messages"][1]["text"].get<std::string>().starts_with("=> "));
	CHECK(built.requests().empty()); // no request went to any agent
	CHECK(built.shown("assistantCode0"));
	CHECK(built.shown("assistantCode1"));
	CHECK(built.shown("assistantCodeCopy0")); // Copy on both kinds
	CHECK(built.shown("assistantCodeCopy1"));
	CHECK_FALSE(built.shown("assistantRetry1")); // never Retry
	CHECK_FALSE(built.shown("assistantFormPreview"));
	CHECK(Built::labelled(built.screen->find("assistantComposer"), "Send") != nullptr);
}

TEST_CASE("assistant_lisp_input: an expression shows its value and moves no history") {
	Built built;
	const int head = built.screen->revision();
	built.type("(+ 1 2)");
	CHECK(built.text("assistantFormText").contains("runs 1 form"));
	built.press(Key::Enter);
	const json assistant = built.assistant();
	REQUIRE(assistant["messages"].size() == 2);
	CHECK(assistant["messages"][1]["text"] == "=> 3");
	CHECK(assistant["draft"] == "");
	CHECK(built.screen->revision() == head);
	CHECK(built.requests().empty());
}

TEST_CASE("assistant_lisp_input: words, in brackets or not, stay a request") {
	Built built;
	built.type("(please make it blue)");
	CHECK_FALSE(built.shown("assistantFormPreview"));
	CHECK(Built::labelled(built.screen->find("assistantComposer"), "Send") != nullptr);
	built.press(Key::Enter);
	REQUIRE(built.requests().size() == 1);
	CHECK(built.requests()[0] == "(please make it blue)");
	CHECK(built.assistant()["draft"] == "");

	built.type("make the title blue");
	CHECK_FALSE(built.shown("assistantFormPreview"));
	built.press(Key::Enter);
	REQUIRE(built.requests().size() == 2);
	CHECK(built.requests()[1] == "make the title blue");
	const json assistant = built.assistant(); // held: a loop over a temporary's member reads it after it is freed
	for(const json& message : assistant["messages"]) CHECK(message.value("kind", "message") == "message");
}

TEST_CASE("assistant_lisp_input: a pasted form says the person will be asked, and runs only on their yes") {
	Built built;
	built.paste("(+ 1 2)");
	REQUIRE(built.assistant()["draft"] == "(+ 1 2)");
	REQUIRE(built.shown("assistantFormPreview"));
	CHECK(built.text("assistantFormText").contains("You will be asked to confirm"));

	built.press(Key::Enter);
	REQUIRE(built.window->confirmation() != nullptr);
	built.answer("Don't Run");
	CHECK(built.window->confirmation() == nullptr);
	json assistant = built.assistant();
	CHECK(assistant["draft"] == "(+ 1 2)"); // kept, to fix or run
	REQUIRE(assistant["messages"].size() == 2);
	CHECK(assistant["messages"][1]["kind"] == "result");
	CHECK(assistant["messages"][1]["error"] == true);
	CHECK(assistant["messages"][1]["text"].get<std::string>().contains("you said no"));

	built.press(Key::Enter);
	REQUIRE(built.window->confirmation() != nullptr);
	built.answer("Run as Me");
	assistant = built.assistant();
	REQUIRE(assistant["messages"].size() == 4);
	CHECK(assistant["messages"][3]["text"] == "=> 3");
	CHECK(assistant["draft"] == "");
	CHECK(built.requests().empty());
}

TEST_CASE("assistant_lisp_input: a form set by a step, not typed, is asked about too") {
	Built built;
	built.run("test.draft");
	REQUIRE(built.assistant()["draft"] == "(+ 2 2)");
	CHECK(built.text("assistantFormText").contains("You will be asked to confirm"));
	built.press(Key::Enter);
	REQUIRE(built.window->confirmation() != nullptr);
	built.answer("Run as Me");
	const json assistant = built.assistant();
	REQUIRE(assistant["messages"].size() == 2);
	CHECK(assistant["messages"][1]["text"] == "=> 4");
}

TEST_CASE("assistant_lisp_input: Cmd+Enter (Ctrl+Enter) and Ask instead send a form as a request") {
	Built built;
	built.type("(+ 1 2)");
	REQUIRE(built.shown("assistantFormPreview"));
	built.press(Key::Enter, modKey);
	REQUIRE(built.requests().size() == 1);
	CHECK(built.requests()[0] == "(+ 1 2)");
	CHECK(built.assistant()["draft"] == "");
	CHECK(built.window->confirmation() == nullptr);

	built.type("(+ 3 4)");
	built.run("assistant.askDraft"); // what the preview's Ask instead link runs
	REQUIRE(built.requests().size() == 2);
	CHECK(built.requests()[1] == "(+ 3 4)");
	const json assistant = built.assistant(); // held: a loop over a temporary's member reads it after it is freed
	for(const json& message : assistant["messages"]) CHECK(message.value("kind", "message") == "message");

	built.type("(+ 5"); // Shift+Enter still starts a line in a form
	built.press(Key::Enter, shiftKey);
	CHECK(built.assistant()["draft"] == "(+ 5\n");
}

TEST_CASE("assistant_lisp_input: an error keeps the draft and shows the error") {
	Built built;
	const int head = built.screen->revision();
	built.type("(page-title 1 2 3)"); // a name the program knows, called wrongly
	REQUIRE(built.shown("assistantFormPreview"));
	built.press(Key::Enter);
	const json assistant = built.assistant();
	REQUIRE(assistant["messages"].size() == 2);
	CHECK(assistant["messages"][1]["kind"] == "result");
	CHECK(assistant["messages"][1]["error"] == true);
	CHECK_FALSE(assistant["messages"][1]["text"].get<std::string>().empty());
	CHECK(assistant["draft"] == "(page-title 1 2 3)");
	CHECK(built.screen->revision() == head);
	CHECK(built.requests().empty());
}

TEST_CASE("assistant_lisp_input: with an empty draft there is no preview, and the panel is as before") {
	Built built;
	CHECK(built.assistant()["draft"] == "");
	CHECK_FALSE(built.shown("assistantFormPreview"));
	const Panel* input = built.screen->find("assistantInput");
	REQUIRE(input != nullptr);
	CHECK(input->frame().bottom - input->frame().top == doctest::Approx(112.0));
	CHECK(Built::labelled(built.screen->find("assistantComposer"), "Send") != nullptr);
}
