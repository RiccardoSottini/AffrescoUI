/// @file
/// @brief The assistant add-on's run cards (examples/shared/addons/assistant, "Runs"), through the rendering check's
/// screens that include its JSON and its Lisp version: a command a connected agent asks to run waits on a card, under
/// the agent's name; Run runs it, Always allow also lets the agent run it again without asking, Don't run refuses it,
/// and Undo puts back what it changed. What the agent wrote (arguments with braces and line breaks) shows as written,
/// on one line, and a run only the window may allow has no buttons.

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/render/Label.hpp>

#include <doctest/doctest.h>

#include <algorithm>
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
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief The rendering check's screens that include the add-on: its JSON version, then its Lisp one.
/// @return Their file names, in tests/render/screens.
std::vector<std::string> screens() { return {"assistant.screen.json", "assistant-lisp.screen.lisp"}; }

/// @brief A screen that includes the add-on in a headless window, with an agent connected in the process and the
/// panel open.
struct Built {
	affresco::test::MemoryChannel agent; ///< What the agent is sent; it outlives the screen.
	/// The folder of its "persist" state: unique to this process, so parallel runs don't meet.
	std::filesystem::path data = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-assistant-runs-" + std::to_string(_getpid()));
#else
	                             ("affrescoui-assistant-runs-" + std::to_string(getpid()));
#endif
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	std::unique_ptr<affresco::Screen> screen; ///< The screen.

	/// @brief Loads and builds a screen, connects the agent and opens the panel.
	/// @param file The screen's file name, in tests/render/screens.
	explicit Built(const std::string& file) : window(createHeadlessWindow(800, 700, clock)) {
		std::filesystem::remove_all(data);
		std::filesystem::create_directories(data);
		screen = std::make_unique<affresco::Screen>(affresco::Screen::fromFile(
		    std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / ".." / "tests" / "render" / "screens" / file, data,
		    affresco::Screen::History::InMemory));
		screen->build(*window);
		window->render();
		screen->connect(agent, "agent");
		run("assistant.open");
	}
	Built(const Built&) = delete;
	Built& operator=(const Built&) = delete;
	/// @brief Closes the screen, then removes its folder.
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

	/// @brief The agent asks to run the page's command, test.note, which writes the page's note.
	/// @param id The message's id.
	/// @param text The note.
	/// @return The screen's answer.
	json ask(int id, const std::string& text) {
		agent.receive(
		    json{{"type", "runCommand"}, {"id", id}, {"command", "test.note"}, {"arguments", {{"text", text}}}}.dump());
		settle();
		REQUIRE_FALSE(agent.sent.empty());
		return agent.sent.back();
	}

	/// @brief Finds a panel by its accessible label.
	/// @param in Where to look (it and its descendants), or nullptr.
	/// @param label The label.
	/// @return The first found, depth first, or nullptr.
	[[nodiscard]] static const affresco::Panel* labelled(const affresco::Panel* in, const std::string& label) {
		if(in == nullptr || in->accessibleLabel() == label) return in;
		for(const std::unique_ptr<affresco::Panel>& child : in->children())
			if(const affresco::Panel* found = labelled(child.get(), label)) return found;
		return nullptr;
	}

	/// @brief Whether a run's card has a button with this label.
	/// @param run The run's number.
	/// @param label The label.
	/// @return Whether it has.
	[[nodiscard]] bool has(int run, const std::string& label) const {
		return labelled(screen->find("assistantRun" + std::to_string(run)), label) != nullptr;
	}

	/// @brief Clicks the button with this label on a run's card, at its middle.
	/// @param run The run's number.
	/// @param label The button's label.
	void click(int run, const std::string& label) {
		const affresco::Panel* button = labelled(screen->find("assistantRun" + std::to_string(run)), label);
		REQUIRE(button != nullptr);
		const affresco::Rect& bounds = button->bounds();
		const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		settle();
	}

	/// @brief The texts of the labels on a run's card, depth first.
	/// @param run The run's number.
	/// @return Them.
	[[nodiscard]] std::vector<std::string> texts(int run) const {
		std::vector<std::string> found;
		collect(screen->find("assistantRun" + std::to_string(run)), found);
		return found;
	}

	/// @brief Adds the texts of the labels in a panel and its descendants.
	/// @param in The panel, or nullptr.
	/// @param found Where they go.
	static void collect(const affresco::Panel* in, std::vector<std::string>& found) {
		if(in == nullptr) return;
		if(const auto* label = dynamic_cast<const affresco::Label*>(in)) found.push_back(label->text());
		for(const std::unique_ptr<affresco::Panel>& child : in->children()) collect(child.get(), found);
	}

	/// @brief The page's state.
	/// @return It.
	[[nodiscard]] json state() const { return json::parse(screen->stateJson()); }

	/// @brief The runs and run grants, as the person sees them.
	/// @return `{"commands", "runs", "runGrants"}`.
	[[nodiscard]] json commands() const { return screen->commandsJson(); }
};

/// @brief Whether a list of texts holds one.
/// @param texts The list.
/// @param text The one.
/// @return Whether it does.
bool holds(const std::vector<std::string>& texts, const std::string& text) {
	return std::ranges::find(texts, text) != texts.end();
}

} // namespace

TEST_CASE("assistant: a run an agent asks for waits on its card, and Run runs it under the agent's name") {
	for(const std::string& file : screens()) {
		CAPTURE(file);
		Built built(file);
		const json waiting = built.ask(1, "Buy milk");
		REQUIRE(waiting["type"] == "waiting");
		const int run = waiting["run"].get<int>();
		CHECK(built.state()["note"] != "Buy milk"); // nothing ran yet
		CHECK(built.has(run, "Run"));
		CHECK(built.has(run, "Always allow"));
		CHECK(built.has(run, "Don't run"));
		CHECK(holds(built.texts(run), "Add a note"));
		CHECK(holds(built.texts(run), "text: Buy milk"));

		built.click(run, "Run");
		CHECK(built.state()["note"] == "Buy milk");
		const json runs = built.commands()["runs"];
		REQUIRE(runs.size() == 1);
		CHECK(runs[0]["state"] == "ran");
		CHECK(runs[0]["author"].get<std::string>().starts_with("agent")); // the agent's run, never the person's
		CHECK_FALSE(built.has(run, "Run"));
		CHECK(built.commands()["runGrants"] == json::array()); // Run allows this run only

		built.click(run, "Undo");
		CHECK(built.state()["note"] != "Buy milk");
		CHECK(built.commands()["runs"][0]["state"] == "undone");
		CHECK_FALSE(built.has(run, "Undo"));
	}
}

TEST_CASE("assistant: a run allowed always adds a grant, and the agent's next run of it doesn't wait") {
	for(const std::string& file : screens()) {
		CAPTURE(file);
		Built built(file);
		const int run = built.ask(1, "First")["run"].get<int>();
		built.click(run, "Always allow");
		CHECK(built.state()["note"] == "First");
		CHECK(built.commands()["runGrants"].size() == 1);
		CHECK(built.ask(2, "Second")["type"] == "ran");
		CHECK(built.state()["note"] == "Second");
	}
}

TEST_CASE("assistant: a run the person doesn't allow changes nothing") {
	for(const std::string& file : screens()) {
		CAPTURE(file);
		Built built(file);
		const int run = built.ask(1, "Nope")["run"].get<int>();
		built.click(run, "Don't run");
		CHECK(built.state()["note"] != "Nope");
		CHECK(built.commands()["runs"][0]["state"] == "denied");
		CHECK_FALSE(built.has(run, "Run"));
		CHECK_FALSE(built.has(run, "Undo"));
	}
}

TEST_CASE("assistant: a run's arguments show as the agent wrote them, on one line, never evaluated") {
	for(const std::string& file : screens()) {
		CAPTURE(file);
		Built built(file);
		const int run = built.ask(1, "a {{state.locale}}\nb")["run"].get<int>();
		CHECK(holds(built.texts(run), "text: a {{state.locale}} b"));
		const std::string longer(80, 'x');
		const int other = built.ask(2, longer)["run"].get<int>();
		CHECK(holds(built.texts(other), "text: " + std::string(60, 'x') + "…"));
	}
}

TEST_CASE("assistant: a run only the window may allow has no buttons on its card") {
	for(const std::string& file : screens()) {
		CAPTURE(file);
		Built built(file);
		built.run("test.runs");
		CHECK(built.has(3, "Run"));
		CHECK_FALSE(built.has(4, "Run"));
		CHECK_FALSE(built.has(4, "Always allow"));
		CHECK_FALSE(built.has(4, "Don't run"));
		CHECK(built.has(1, "Undo"));
		CHECK_FALSE(built.has(2, "Undo"));
	}
}
