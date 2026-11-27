/// @file
/// @brief The assistant add-on (examples/shared/addons/assistant), through the rendering check's screen that includes
/// it: the launcher shows by default (an app that hides it shows it again for an agent, a conversation or a request),
/// the panel opens on the composer and says when no agent is connected, Escape gives the focus back to the launcher, a
/// message sent is kept, a request to a connected agent comes back as a proposal card the person accepts or rejects,
/// the person points at elements of the app for their next message, and the past conversations are kept, listed, gone
/// on with and deleted.

#include "AudioWav.hpp"
#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <string>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A folder of its own for one test's "persist" state, removed after it.
struct DataFolder {
	/// @brief Creates it.
	DataFolder() { std::filesystem::create_directories(path); }
	DataFolder(const DataFolder&) = delete;
	DataFolder& operator=(const DataFolder&) = delete;
	/// @brief Removes it and what is in it.
	~DataFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}

	/// The folder: unique to this process, so parallel runs don't meet.
	std::filesystem::path path = std::filesystem::temp_directory_path() /
#ifdef _WIN32
	                             ("affrescoui-assistant-" + std::to_string(_getpid()));
#else
	                             ("affrescoui-assistant-" + std::to_string(getpid()));
#endif
};

/// @brief The rendering check's screen that includes the add-on.
/// @return Its path.
std::filesystem::path assistantScreen() {
	return std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / ".." / "tests" / "render" / "screens" /
	       "assistant.screen.json";
}

/// @brief The add-on's screen built in a headless window of its size, with its data in a given folder.
struct Built {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Loads and builds the screen.
	/// @param data The folder its "persist" state is kept in.
	/// @param before What to do with the screen before it is built (the host's calls, which its onShow steps use), or
	/// nothing.
	explicit Built(const std::filesystem::path& data, const std::function<void(affresco::Screen&)>& before = {})
	    : window(createHeadlessWindow(800, 700, clock)),
	      screen(affresco::Screen::fromFile(assistantScreen(), data, affresco::Screen::History::InMemory)) {
		if(before) before(screen);
		screen.build(*window);
		window->render();
	}

	/// @brief Runs a document action, then lets its animation and its deferred steps finish.
	/// @param name The action.
	void run(const std::string& name) {
		screen.runAction(name, window->root(), "{}");
		settle();
	}

	/// @brief Renders frames a quarter of a second apart, so animations and deferred steps finish.
	void settle() {
		for(int frame = 0; frame < 4; ++frame) {
			clock.advance(250.0);
			window->render();
		}
	}

	/// @brief Types characters into the focused panel.
	/// @param text The characters (ASCII).
	void type(const std::string& text) {
		for(const char c : text) window->routeChar({static_cast<char32_t>(c)});
		window->render();
	}

	/// @brief Presses and releases a key, as a keyboard does before the next character.
	/// @param key The key.
	/// @param mods The modifier bits held.
	void press(affresco::Key key, int mods = 0) {
		window->routeKey({key, 0, affresco::Action::Press, {mods}});
		window->routeKey({key, 0, affresco::Action::Release, {mods}});
		settle();
	}

	/// @brief Whether a panel with this id exists and is shown.
	/// @param id The id.
	/// @return Whether it is.
	[[nodiscard]] bool shown(const std::string& id) {
		const affresco::Panel* panel = screen.find(id);
		return panel != nullptr && panel->isVisible();
	}

	/// @brief Whether the focus is on the panel with this id or inside it.
	/// @param id The id.
	/// @return Whether it is.
	[[nodiscard]] bool focusIn(const std::string& id) const {
		for(const affresco::Panel* panel = window->focusedPanel(); panel != nullptr; panel = panel->parent())
			if(panel->id() == id) return true;
		return false;
	}

	/// @brief Clicks the button with this accessible label inside a panel, at its middle.
	/// @param id The panel's id.
	/// @param label The button's label.
	void click(const std::string& id, const std::string& label) {
		const affresco::Panel* button = labelled(screen.find(id), label);
		REQUIRE(button != nullptr);
		const affresco::Rect& bounds = button->bounds();
		const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		settle();
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

	/// @brief Reads the add-on's state.
	/// @return state.assistant.
	[[nodiscard]] json assistant() const { return json::parse(screen.stateJson())["assistant"]; }

	/// @brief Clicks the middle of the panel with this id, as the mouse does.
	/// @param id The id.
	void clickAt(const std::string& id) {
		const affresco::Panel* panel = screen.find(id);
		REQUIRE(panel != nullptr);
		const affresco::Rect& bounds = panel->bounds();
		const affresco::Vec2 at{(bounds.left + bounds.right) / 2.0f, (bounds.top + bounds.bottom) / 2.0f};
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, at});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, at});
		settle();
	}

	/// @brief Presses the add-on's point shortcut, mod+alt+a ("mod" is Cmd on macOS, Ctrl elsewhere).
	void pressPoint() {
#ifdef __APPLE__
		press(affresco::Key::A, 0x0008 | 0x0004); // Super and Alt
#else
		press(affresco::Key::A, 0x0002 | 0x0004); // Control and Alt
#endif
	}
};

} // namespace

TEST_CASE("assistant add-on: the launcher shows by default, and once hidden waits for an agent or a request") {
	const DataFolder data;
	Built built(data.path);
	CHECK(built.shown("assistantLauncher"));
	built.run("test.hide");
	CHECK_FALSE(built.shown("assistantLauncher"));
	built.run("test.online");
	CHECK(built.shown("assistantLauncher"));
	built.run("test.offline");
	CHECK_FALSE(built.shown("assistantLauncher"));
	built.run("test.show");
	CHECK(built.shown("assistantLauncher"));
}

TEST_CASE("assistant add-on: the panel opens on the composer, says it is offline, and Escape closes it") {
	const DataFolder data;
	Built built(data.path);
	built.run("test.offline");
	built.run("test.show");
	CHECK_FALSE(built.shown("assistantPanel"));

	built.run("assistant.open");
	CHECK(built.assistant()["open"] == true);
	CHECK(built.shown("assistantPanel"));
	CHECK(built.shown("assistantOffline"));
	CHECK(built.shown("assistantEmpty"));
	CHECK(built.focusIn("assistantComposer"));

	built.press(affresco::Key::Escape);
	CHECK(built.assistant()["open"] == false);
	CHECK_FALSE(built.shown("assistantPanel"));
	CHECK(built.focusIn("assistantLauncher"));
}

TEST_CASE("assistant add-on: Enter sends the draft, Shift+Enter starts a line, and the conversation is kept") {
	const DataFolder data;
	{
		Built built(data.path);
		built.run("test.online");
		built.run("assistant.open");
		built.type("Bigger");
		built.press(affresco::Key::Enter, 0x0001); // Shift
		built.type("titles");
		CHECK(built.assistant()["draft"] == "Bigger\ntitles");

		built.press(affresco::Key::Enter);
		const json assistant = built.assistant();
		CHECK(assistant["draft"] == "");
		REQUIRE(assistant["messages"].size() == 2); // no agent really answers here: the request fails
		CHECK(assistant["messages"][0]["text"] == "Bigger\ntitles");
		CHECK(assistant["messages"][0]["mine"] == true);
		CHECK(assistant["messages"][1]["mine"] == false);
		CHECK(assistant["messages"][1]["error"] == true);
		CHECK_FALSE(built.shown("assistantEmpty"));
		CHECK(built.shown("assistantMessage0"));
		CHECK(built.shown("assistantMessage1"));

		built.press(affresco::Key::Enter); // an empty draft sends nothing
		CHECK(built.assistant()["messages"].size() == 2);

		// The failed reply remembers what it answered: retry asks it again
		CHECK(built.assistant()["messages"][1]["asked"] == "Bigger\ntitles");
		CHECK(built.shown("assistantRetry1"));
		CHECK(built.shown("assistantCopy1"));
		built.click("assistantRetry1", "Retry");
		const json retried = built.assistant();
		REQUIRE(retried["messages"].size() == 4);
		CHECK(retried["messages"][2]["text"] == "Bigger\ntitles");
		CHECK(retried["messages"][2]["mine"] == true);
		built.click("assistantCopy3", "Copy");
		CHECK(built.assistant()["copied"] == 3);
	}
	Built again(data.path);
	again.run("test.hide");
	const json assistant = again.assistant();
	REQUIRE(assistant["messages"].size() == 4);
	CHECK(assistant["messages"][0]["text"] == "Bigger\ntitles");
	CHECK(assistant["open"] == false);       // only the conversation is kept, not whether the panel was open
	CHECK(again.shown("assistantLauncher")); // a conversation shows the launcher
}

TEST_CASE("assistant add-on: New conversation keeps the one shown with the past ones, kept across a restart") {
	const DataFolder data;
	{
		Built built(data.path);
		built.run("test.online");
		built.run("assistant.open");
		built.type("Bigger");
		built.press(affresco::Key::Enter, 0x0001); // Shift: a new line
		built.type("titles");
		built.press(affresco::Key::Enter);
		REQUIRE(built.assistant()["messages"].size() == 2); // the request, and the failed reply

		built.click("assistantNew", "New conversation");
		json assistant = built.assistant();
		CHECK(assistant["messages"].empty());
		CHECK(assistant["conversation"] == "");
		REQUIRE(assistant["conversations"].size() == 1);
		CHECK(assistant["conversations"][0]["title"] == "Bigger titles"); // on one line
		CHECK(assistant["conversations"][0]["count"] == 2);
		CHECK(assistant["conversations"][0]["messages"].size() == 2);
		CHECK(built.shown("assistantEmpty"));
		CHECK(built.focusIn("assistantComposer"));

		built.click("assistantNew", "New conversation"); // an empty conversation isn't kept
		CHECK(built.assistant()["conversations"].size() == 1);

		// What the host was asked to ask (--ask) starts a conversation of its own
		built.type("Draft");
		built.press(affresco::Key::Enter);
		built.screen.runAction("assistant.start", built.window->root(), R"({"text": "Add a chart"})");
		built.settle();
		assistant = built.assistant();
		REQUIRE(assistant["conversations"].size() == 2);
		CHECK(assistant["conversations"][0]["title"] == "Draft");
		REQUIRE_FALSE(assistant["messages"].empty());
		CHECK(assistant["messages"][0]["text"] == "Add a chart");

		built.click("assistantHistoryButton", "Past conversations");
		CHECK(built.shown("assistantHistory"));
		CHECK_FALSE(built.shown("assistantMessages"));
		CHECK(built.shown("assistantPast1"));
	}
	Built again(data.path);
	again.run("test.online");
	again.run("assistant.open");
	json assistant = again.assistant();
	CHECK(assistant["history"] == false); // whether the list showed isn't kept
	REQUIRE(assistant["conversations"].size() == 2);
	CHECK(assistant["messages"][0]["text"] == "Add a chart");

	again.click("assistantHistoryButton", "Past conversations");
	again.click("assistantPast1", "Bigger titles"); // goes on with it: the one shown takes its place in the list
	assistant = again.assistant();
	CHECK(assistant["history"] == false);
	CHECK(assistant["messages"][0]["text"] == "Bigger\ntitles");
	REQUIRE(assistant["conversations"].size() == 2);
	CHECK(assistant["conversations"][0]["title"] == "Add a chart");
	CHECK(assistant["conversations"][1]["title"] == "Draft");
	CHECK(assistant["conversation"] != "");

	again.click("assistantHistoryButton", "Past conversations");
	again.click("assistantPast0", "Delete Add a chart");
	CHECK(again.assistant()["conversations"].size() == 1);
	again.click("assistantHistory", "Delete all");
	CHECK(again.assistant()["conversations"].empty());
	CHECK(again.shown("assistantHistoryEmpty"));
}

/// @brief The add-on with an agent connected: the person asks, the agent proposes and replies.
struct Conversation {
	affresco::test::MemoryChannel agent; ///< What the agent is sent; it outlives the screen.
	DataFolder data;                     ///< The persisted conversation.
	Built built{data.path};              ///< The screen.

	/// @brief Connects the agent, then the person types a request and sends it with Enter.
	Conversation() {
		built.screen.connect(agent, "agent");
		built.run("assistant.open");
		built.type("Rename the page");
		built.press(affresco::Key::Enter);
	}

	/// @brief The requests the agent was sent.
	/// @return Them, oldest first.
	[[nodiscard]] std::vector<json> requests() const {
		std::vector<json> found;
		for(const json& message : agent.sent)
			if(message.value("type", "") == "request") found.push_back(message);
		return found;
	}

	/// @brief The agent proposes a new page title and replies to the request.
	/// @return The proposal's id.
	int propose() {
		const std::vector<json> asked = requests();
		REQUIRE(asked.size() == 1);
		const affresco::RevisionResult proposed = built.screen.submitRevision(
		    R"({"author": "agent/helper", "message": "Rename the page", "set": {"#pageTitle/text": "Tasks"}})");
		REQUIRE(proposed.status == affresco::RevisionStatus::Proposed);
		agent.receive(json{{"type", "reply"}, {"request", asked[0]["id"]}, {"text", "Renamed it."}}.dump());
		built.settle();
		return proposed.proposal;
	}
};

TEST_CASE("assistant add-on: a request to a connected agent comes back as a proposal the person accepts") {
	Conversation chat;
	const std::vector<json> asked = chat.requests();
	REQUIRE(asked.size() == 1);
	CHECK(asked[0]["text"] == "Rename the page");
	CHECK(asked[0]["from"] == "user");
	CHECK(chat.built.shown("assistantTyping")); // the agent is working on it

	const int proposal = chat.propose();
	const std::string id = std::to_string(proposal);
	const json messages = chat.built.assistant()["messages"];
	REQUIRE(messages.size() == 3);
	CHECK(messages[1]["text"] == "Renamed it.");
	CHECK(messages[2]["kind"] == "card");
	CHECK(messages[2]["id"] == proposal);
	CHECK(messages[2]["title"] == "Rename the page");
	REQUIRE(messages[2]["lines"].size() == 1);
	CHECK(chat.built.shown("assistantCard" + id));
	CHECK_FALSE(chat.built.shown("assistantTyping"));
	CHECK(chat.built.screen.revision() == 0); // a reply is never the person's decision

	chat.built.click("assistantCard" + id, "Accept");
	CHECK(chat.built.screen.proposals().empty());
	CHECK(chat.built.screen.revision() == 1);
	CHECK(json::parse(chat.built.screen.documentJson())["root"]["children"][0]["text"] == "Tasks");
	CHECK(chat.built.assistant()["decided"][id] == "accepted");
	CHECK(chat.built.shown("assistantCard" + id)); // the card stays, saying it was accepted
}

TEST_CASE("assistant add-on: files attached show as chips, go with the request, and a proposal shows its assets") {
	affresco::test::MemoryChannel agent;
	const DataFolder data;
	Built built(data.path);
	built.screen.connect(agent, "agent");
	const std::filesystem::path file = data.path / "Clip One.wav";
	const std::vector<std::uint8_t> wav =
	    affresco::test::wavFile(affresco::test::toneSamples(22050, 22050, 440), 22050, 1); // a second of a tone
	std::ofstream(file, std::ios::binary)
	    .write(reinterpret_cast<const char*>(wav.data()), static_cast<std::streamsize>(wav.size()));
	int dialogs = 0;
	built.window->setFileDialogHandler([&](const affresco::FileDialog& dialog) {
		++dialogs;
		CHECK(dialog.multiple);
		return std::vector<std::string>{file.string()};
	});
	built.run("assistant.open");

	built.click("assistantPanel", "Attach files"); // the paperclip: the dialog, then a chip
	CHECK(dialogs == 1);
	json assistant = built.assistant();
	REQUIRE(assistant["attachments"].size() == 1);
	CHECK(assistant["attachments"][0]["name"] == "clip-one.wav");
	CHECK(assistant["attachments"][0]["duration"] == doctest::Approx(1.0));
	CHECK(built.shown("assistantAttached0"));
	built.click("assistantAttached0", "Remove clip-one.wav");
	CHECK(built.assistant()["attachments"].empty());
	CHECK_FALSE(built.shown("assistantAttachments"));

	built.run("assistant.attach"); // again, and this time it goes with the message
	built.type("Use this file");
	built.press(affresco::Key::Enter);
	assistant = built.assistant();
	CHECK(assistant["attachments"].empty());
	REQUIRE(assistant["messages"].size() == 2);
	CHECK(assistant["messages"][0]["kind"] == "files");
	CHECK(assistant["messages"][0]["files"][0]["name"] == "clip-one.wav");
	CHECK(built.shown("assistantFiles0"));
	std::vector<json> asked;
	for(const json& message : agent.sent)
		if(message.value("type", "") == "request") asked.push_back(message);
	REQUIRE(asked.size() == 1);
	REQUIRE(asked[0]["attachments"].size() == 1);
	const json attached = asked[0]["attachments"][0];
	CHECK(attached["name"] == "clip-one.wav");
	CHECK(attached["type"] == "audio/wav");
	CHECK(attached.dump().find(data.path.string()) == std::string::npos); // never where the file is

	const json revision = {{"author", "agent/helper"},
	                       {"message", "Keep the file"},
	                       {"assets", {{{"name", "clip.wav"}, {"attachment", attached["id"]}}}},
	                       {"run", {{{"set", "state.assistant.draft"}, {"to", "done"}}}}};
	const affresco::RevisionResult proposed = built.screen.submitRevision(revision.dump());
	REQUIRE_MESSAGE(proposed.status == affresco::RevisionStatus::Proposed, proposed.error);
	agent.receive(json{{"type", "reply"}, {"request", asked[0]["id"]}, {"text", "It waits for you."}}.dump());
	built.settle();
	const json messages = built.assistant()["messages"];
	REQUIRE(messages.size() == 5); // the files, the request, the reply, the card and what it brings
	CHECK(messages[3]["kind"] == "card");
	CHECK(messages[4]["kind"] == "extras");
	CHECK(messages[4]["assets"][0]["name"] == "clip.wav");
	CHECK(messages[4]["run"].size() == 1);
	CHECK(built.shown("assistantAdds" + std::to_string(proposed.proposal)));

	built.click("assistantCard" + std::to_string(proposed.proposal), "Accept");
	CHECK(built.screen.proposals().empty());
	CHECK(built.assistant()["draft"] == "done"); // its run step ran once, on accept
}

TEST_CASE("assistant add-on: the person rejects a proposal, and main stays as it was") {
	Conversation chat;
	const std::string id = std::to_string(chat.propose());
	chat.built.click("assistantCard" + id, "Reject");
	CHECK(chat.built.screen.proposals().empty());
	CHECK(chat.built.screen.revision() == 0);
	CHECK(chat.built.assistant()["decided"][id] == "rejected");
}

TEST_CASE("assistant add-on: Point steps the panel aside, a click picks without running the app, and the chip waits") {
	const DataFolder data;
	Built built(data.path);
	built.run("test.online");
	built.run("assistant.open");

	built.click("assistantPanel", "Point at something");
	CHECK(built.window->picking());
	CHECK(built.assistant()["open"] == false); // the panel is modal: it steps aside
	CHECK(built.assistant()["pointing"] == true);
	CHECK_FALSE(built.shown("assistantPanel"));

	built.clickAt("pageTitle");
	CHECK_FALSE(built.window->picking());
	const json assistant = built.assistant();
	CHECK_FALSE(json::parse(built.screen.stateJson()).contains("titleClicks")); // the click picked, it didn't click
	CHECK(assistant["open"] == true);
	CHECK(assistant["pointing"] == false);
	CHECK(assistant["justPointed"].is_null());
	REQUIRE(assistant["pointed"].size() == 1);
	CHECK(assistant["pointed"][0]["node"] == "#pageTitle");
	CHECK(assistant["pointed"][0]["source"]["address"] == "#pageTitle");
	CHECK(built.shown("assistantPointedList"));
	CHECK(built.shown("assistantPointed0"));
	CHECK(built.shown("assistantPointedNote"));
	CHECK(Built::labelled(built.screen.find("assistantPointed0"), "label · #pageTitle") != nullptr);
	CHECK(built.focusIn("assistantComposer"));
	CHECK(built.screen.picks().size() == 1);

	built.click("assistantPanel", "Point at something"); // the same element again is kept once
	built.clickAt("pageTitle");
	CHECK(built.assistant()["pointed"].size() == 1);
	CHECK(built.screen.picks().size() == 1);

	built.click("assistantPointed0", "Remove label · #pageTitle");
	CHECK(built.assistant()["pointed"].empty());
	CHECK(built.screen.picks().empty()); // the engine forgot it too
	CHECK_FALSE(built.shown("assistantPointedRow"));
	CHECK(built.focusIn("assistantComposer"));
}

TEST_CASE("assistant add-on: Escape cancels pointing: the panel comes back only if it was open") {
	const DataFolder data;
	Built built(data.path);
	built.run("test.online");
	built.run("assistant.open");
	built.click("assistantPanel", "Point at something");
	REQUIRE(built.window->picking());
	built.press(affresco::Key::Escape);
	CHECK_FALSE(built.window->picking());
	CHECK(built.assistant()["pointing"] == false);
	CHECK(built.assistant()["open"] == true);
	CHECK(built.assistant()["pointed"].empty());

	built.press(affresco::Key::Escape); // closes the panel
	REQUIRE(built.assistant()["open"] == false);
	built.pressPoint(); // the shortcut works with the panel closed
	REQUIRE(built.window->picking());
	built.press(affresco::Key::Escape);
	CHECK_FALSE(built.window->picking());
	CHECK(built.assistant()["open"] == false);
	CHECK(built.assistant()["pointing"] == false);
}

TEST_CASE("assistant add-on: by keyboard alone the person points, types and sends, and the request carries it") {
	affresco::test::MemoryChannel agent;
	const DataFolder data;
	Built built(data.path);
	built.screen.connect(agent, "agent");

	built.pressPoint();
	REQUIRE(built.window->picking());
	for(int step = 0; step < 3; ++step) built.press(affresco::Key::Down);
	built.press(affresco::Key::Enter);
	CHECK_FALSE(built.window->picking());
	json assistant = built.assistant();
	REQUIRE(assistant["pointed"].size() == 1);
	CHECK(assistant["open"] == true);
	CHECK(built.focusIn("assistantComposer"));
	const json record = assistant["pointed"][0];

	built.type("Make this larger");
	built.press(affresco::Key::Enter);
	assistant = built.assistant();
	CHECK(assistant["pointed"].empty());
	REQUIRE(assistant["messages"].size() == 2);
	CHECK(assistant["messages"][0]["kind"] == "pointed");
	CHECK(assistant["messages"][0]["items"] == json::array({record}));
	CHECK(assistant["messages"][1]["text"] == "Make this larger");
	CHECK(built.shown("assistantPointedSent0"));
	CHECK(Built::labelled(built.screen.find("assistantPointedSent0"), "Show source") == nullptr); // sent: no buttons
	CHECK(built.screen.picks().empty()); // sent once: the request has it now

	std::vector<json> asked;
	for(const json& message : agent.sent)
		if(message.value("type", "") == "request") asked.push_back(message);
	REQUIRE(asked.size() == 1);
	REQUIRE(asked[0]["picks"].size() == 1);
	CHECK(asked[0]["picks"][0]["id"] == record["id"]);
	CHECK(asked[0]["picks"][0]["node"] == record["node"]);
}

TEST_CASE("assistant add-on: four elements at most, and Show source asks the host for where one is written") {
	const DataFolder data;
	json opened;
	Built built(data.path, [&opened](affresco::Screen& screen) { // a host with a changes window
		screen.onCall("changes.available", [](const std::string&) { return std::string("true"); });
		screen.onCall("changes.open", [&opened](const std::string& text) {
			opened = json::parse(text);
			return std::string("null");
		});
	});
	built.run("test.pointed");
	built.run("assistant.open");
	REQUIRE(built.assistant()["pointed"].size() == 4);
	CHECK(built.shown("assistantPointed3"));
	const affresco::Panel* point = Built::labelled(built.screen.find("assistantPanel"), "Point at something");
	REQUIRE(point != nullptr);
	CHECK(point->accessibleState().disabled);

	built.pressPoint(); // a fifth is refused: no pick starts
	CHECK_FALSE(built.window->picking());
	CHECK(built.assistant()["open"] == true);

	built.click("assistantPointed1", "Show source");
	CHECK(opened.dump().find("/program/board.lisp/card-title") != std::string::npos);
	CHECK(opened.dump().find("12") != std::string::npos); // and its line
}

TEST_CASE("assistant add-on: a commit since an element was pointed at drops it quietly, where it was having moved") {
	const DataFolder data;
	Built built(data.path);
	built.run("test.online");
	built.run("assistant.open");
	built.click("assistantPanel", "Point at something");
	built.clickAt("pageTitle");
	REQUIRE(built.assistant()["pointed"].size() == 1);
	built.press(affresco::Key::Escape);

	const affresco::RevisionResult committed =
	    built.screen.submitRevision(R"({"author": "studio", "message": "Rename", "set": {"#pageTitle/text": "Tasks"}})");
	REQUIRE_MESSAGE(committed.status == affresco::RevisionStatus::Applied, committed.error);
	built.settle();
	built.run("assistant.open");
	CHECK(built.assistant()["pointed"].empty());
	CHECK(built.screen.picks().empty());
	CHECK_FALSE(built.shown("assistantPointedRow"));
}
