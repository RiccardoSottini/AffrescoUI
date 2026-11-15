/// @file
/// @brief The host's window on a change (apps/host/ChangesWindow.cpp): opened on a live proposal, it tells the truth
/// whatever that proposal does to the app, and a review it saw is the only one it can accept. Its Source page shows
/// where an element is written, in either notation, with the line asked for marked and the commit that last changed it.

#include "../../../apps/host/ChangesWindow.hpp"

#include <affresco/affresco.hpp>
#include <affresco/lisp/Document.hpp>

#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

using affresco::json;
using affresco::Screen;

namespace {

/// A screen with one title, whose agent's changes wait for review.
constexpr const char* appScreen = R"json({
	"format": "affrescoui-screen", "version": 2,
	"permissions": { "reviewers": ["studio"], "ask": ["agent"] },
	"window": { "title": "app", "width": 400, "height": 300 },
	"root": { "id": "root", "children": [{ "id": "title", "type": "label", "text": "One" }] } })json";

/// @brief A proposal changing the title.
/// @param screen The app.
/// @param text The title.
/// @return The proposal's id.
int proposeTitle(Screen& screen, const std::string& text) {
	const json patch = {{{"op", "replace"}, {"path", "/root/children/0/text"}, {"value", text}}};
	const affresco::RevisionResult result = screen.versions().submit({{"author", "agent"}, {"patch", patch}});
	REQUIRE(result.proposal > 0);
	return result.proposal;
}

/// @brief Presses the shortcut that opens the window.
/// @param window The app's window.
void pressShortcut(affresco::Window& window) {
	affresco::KeyEvent press;
	press.key = affresco::Key::D;
	press.action = affresco::Action::Press;
#ifdef __APPLE__
	press.mods.bits = 0x0008 | 0x0001; // Super and Shift
#else
	press.mods.bits = 0x0002 | 0x0001; // Control and Shift
#endif
	window.routeKey(press);
}

/// @brief The window's screen's state.
/// @param screen The window's screen.
/// @return It.
json stateOf(const Screen& screen) { return json::parse(screen.stateJson()); }

/// @brief The host's screen file.
/// @param notation "json" or "lisp".
/// @return Its path.
std::filesystem::path changesScreen(const std::string& notation) {
	return std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "shared" / "host" / ("changes.screen." + notation);
}

/// @brief A new folder for one test, removed with what it holds when the test ends.
class TemporaryFolder {
public:
	/// @brief Creates the folder.
	TemporaryFolder() {
		static std::atomic<int> count{0};
		path_ =
		    std::filesystem::temp_directory_path() /
		    ("affrescoui-changes-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
			 "-" + std::to_string(count++));
		std::filesystem::create_directories(path_);
	}
	/// @brief Removes the folder.
	~TemporaryFolder() {
		std::error_code ignored;
		std::filesystem::remove_all(path_, ignored);
	}
	TemporaryFolder(const TemporaryFolder&) = delete;
	TemporaryFolder& operator=(const TemporaryFolder&) = delete;
	/// @brief The folder.
	/// @return Its path.
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
	std::filesystem::path path_; ///< The folder.
};

/// @brief Writes a text file.
/// @param file The file.
/// @param text What it holds.
void writeText(const std::filesystem::path& file, const std::string& text) {
	std::ofstream(file, std::ios::binary) << text;
}

/// A program whose card is a definition of its own, and an action that asks the host for a source.
constexpr const char* lispApp = R"lisp((defview card "A card." []
  `(column#card :width 120 :height 40
     (label#cardTitle :text "Card" :width 120 :height 20)))

(defpart calls "What the tests run."
  :actions {:showSource [{:call "changes.open" :with "{{event}}"}]})

(defscreen main
  :permissions {:allow [studio] :ask [agent]}
  :window {:title "app" :width 400 :height 300}
  (panel#root ~(card)))
)lisp";

/// @brief A JSON app in two files: the entry, and a part whose component holds the node shown.
/// @param folder The folder.
/// @return The entry.
std::filesystem::path writeJsonApp(const std::filesystem::path& folder) {
	writeText(folder / "part.json", R"json({ "components": {
		"card": { "node": { "id": "cardFace", "color": "#111111", "width": 120, "height": 40 } } } })json");
	writeText(folder / "index.screen.json", R"json({
		"format": "affrescoui-screen", "version": 2, "include": ["part.json"],
		"permissions": { "allow": ["studio"], "ask": ["agent"] },
		"window": { "title": "app", "width": 400, "height": 300 },
		"actions": { "showSource": [{ "call": "changes.open", "with": "{{event}}" }] },
		"root": { "id": "root", "children": [{ "use": "card" }] } })json");
	return folder / "index.screen.json";
}

/// @brief Applies a revision as the person's studio.
/// @param screen The app.
/// @param revision What it changes: `patch` or `set`.
void applyAsStudio(Screen& screen, json revision) {
	revision["author"] = "studio";
	revision["message"] = "Make the card larger";
	const affresco::RevisionResult result = screen.submitRevision(revision.dump());
	CAPTURE(result.error);
	REQUIRE(result.status == affresco::RevisionStatus::Applied);
}

/// @brief The app asks for its Source page (`changes.open` with `source`), and the window follows its host.
/// @param screen The app.
/// @param window The app's window.
/// @param changes The host's window on it.
/// @param asked The call's arguments.
/// @return The window's state once it has followed.
json askForSource(Screen& screen, affresco::Window& window, const host::ChangesWindow& changes, const json& asked) {
	screen.runAction("showSource", window.root(), asked.dump());
	REQUIRE(changes.opened() != nullptr);
	changes.opened()->runAction("changes.refresh", window.root());
	return stateOf(*changes.opened());
}

} // namespace

TEST_CASE("changes window: it opens on the proposal, and tells the truth while that proposal is previewed") {
	for(const std::string notation : {"json", "lisp"}) { // the window in either notation
		CAPTURE(notation);
		affresco::Application app({nullptr, true});
		Screen screen = Screen::fromString(appScreen, {});
		affresco::Window& window = screen.createWindow(app);
		const host::ChangesWindow changes(app, screen, window, changesScreen(notation));
		CHECK(changes.opened() == nullptr);
		const int id = proposeTitle(screen, "Nothing to see here");
		REQUIRE(screen.previewProposal(id)); // the running app is now the proposal
		pressShortcut(window);
		REQUIRE(changes.opened() != nullptr);
		changes.opened()->runAction("changes.refresh", window.root());
		const json state = stateOf(*changes.opened());
		REQUIRE(state["view"].is_object());
		CHECK(state["view"]["proposal"] == id);
		CHECK(state["list"]["previewing"] == id);
		REQUIRE(state["text"].is_object());
		CHECK(state["text"]["before"].get<std::string>().contains("\"One\""));
		CHECK(state["text"]["after"].get<std::string>().contains("Nothing to see here"));
		screen.endPreview();
	}
}

TEST_CASE("changes window: only the change the person saw can be accepted, and then it can be undone") {
	for(const std::string notation : {"json", "lisp"}) { // the window in either notation
		CAPTURE(notation);
		affresco::Application app({nullptr, true});
		Screen screen = Screen::fromString(appScreen, {});
		affresco::Window& window = screen.createWindow(app);
		const host::ChangesWindow changes(app, screen, window, changesScreen(notation));
		const int id = proposeTitle(screen, "Two");
		pressShortcut(window);
		Screen* own = changes.opened();
		REQUIRE(own != nullptr);
		affresco::Panel& sender = window.root();
		own->runAction("changes.refresh", sender);

		// The author pushes more after the person read it: the window says so, and shows what is there now
		const std::string seen = stateOf(*own)["view"]["head"].get<std::string>();
		const json more = {{{"op", "replace"}, {"path", "/root/children/0/text"}, {"value", "Three"}}};
		REQUIRE(screen.versions().submit({{"author", "agent"}, {"proposal", id}, {"patch", more}}).proposal == id);
		own->runAction("changes.refresh", sender);
		json state = stateOf(*own);
		CHECK(state["stale"] == true);
		CHECK(state["view"]["head"] != seen);
		CHECK(state["text"]["after"].get<std::string>().contains("Three"));

		// What was reviewed is what is accepted: an older head is refused
		std::string refused;
		CHECK_FALSE(screen.acceptProposal(id, seen, &refused));
		CHECK_FALSE(refused.empty());
		CHECK(screen.proposals().size() == 1);

		own->runAction("changes.accept", sender);
		state = stateOf(*own);
		CHECK(state["message"] == "");
		CHECK(screen.proposals().empty());
		CHECK(state["closed"] == true);
		CHECK(json::parse(screen.documentJson())["root"]["children"][0]["text"] == "Three");

		pressShortcut(window); // nothing is waiting: the newest applied change
		own->runAction("changes.refresh", sender);
		state = stateOf(*own);
		REQUIRE(state["view"]["kind"] == "commit");
		own->runAction("changes.undo", sender);
		CHECK(stateOf(*own)["message"] == "");
		CHECK(json::parse(screen.documentJson())["root"]["children"][0]["text"] == "One");
	}
}

TEST_CASE("changes window: a person's rejection reaches the author, with the reason") {
	for(const std::string notation : {"json", "lisp"}) { // the window in either notation
		CAPTURE(notation);
		affresco::Application app({nullptr, true});
		Screen screen = Screen::fromString(appScreen, {});
		affresco::Window& window = screen.createWindow(app);
		const host::ChangesWindow changes(app, screen, window, changesScreen(notation));
		proposeTitle(screen, "Two");
		pressShortcut(window);
		REQUIRE(changes.opened() != nullptr);
		std::string told;
		screen.versions().onDecision.connect([&](const affresco::Decision& decision) {
			if(decision.error) told = *decision.error;
		});
		changes.opened()->runAction("changes.refresh", window.root());
		changes.opened()->runAction("changes.reject", window.root());
		CHECK(screen.proposals().empty());
		CHECK(json::parse(screen.documentJson())["root"]["children"][0]["text"] == "One");
		CHECK_FALSE(told.empty());
	}
}

TEST_CASE("changes window: the Source page shows a program's definition, the line asked for, and who changed it") {
	for(const std::string notation : {"json", "lisp"}) { // the window in either notation
		CAPTURE(notation);
		affresco::Application app({nullptr, true});
		Screen screen = Screen::fromString(affresco::lisp::jsonText(lispApp, "index.screen.lisp"), {});
		affresco::Window& window = screen.createWindow(app);
		const host::ChangesWindow changes(app, screen, window, changesScreen(notation));
		const std::string address = screen.source("#cardTitle")["address"].get<std::string>();
		const std::string larger = "(defview card \"A card.\" []\n  `(column#card :width 160 :height 40\n"
		                           "     (label#cardTitle :text \"Card\" :width 160 :height 20)))";
		applyAsStudio(screen, {{"set", {{address, larger}}}});

		const json state = askForSource(screen, window, changes, {{"source", address}, {"line", 2}});
		CHECK(state["view"].is_null()); // the Source page, not a change
		const json& source = state["source"];
		REQUIRE(source.is_object());
		CHECK(source["notation"] == "lisp");
		CHECK(source["kind"] == "defview");
		CHECK(source["text"] == larger);
		CHECK(source["marked"] == 2);
		REQUIRE(source["blame"].is_object());
		CHECK(source["blame"]["number"] == 1);
		CHECK(source["blame"]["author"] == "studio");
		CHECK(source["blame"]["message"] == "Make the card larger");
		CHECK(state["message"] == "");

		// Without a line, the definition's own is marked
		CHECK(askForSource(screen, window, changes, {{"source", address}})["source"]["marked"] == 1);
	}
}

TEST_CASE("changes window: the Source page shows a JSON node, from the part that holds it") {
	for(const std::string notation : {"json", "lisp"}) { // the window in either notation
		CAPTURE(notation);
		const TemporaryFolder folder;
		affresco::Application app({nullptr, true});
		Screen screen =
		    Screen::fromFile(writeJsonApp(folder.path()), folder.path() / "data", Screen::History::InMemory);
		affresco::Window& window = screen.createWindow(app);
		const host::ChangesWindow changes(app, screen, window, changesScreen(notation));
		applyAsStudio(screen,
		              {{"patch", {{{"op", "replace"}, {"path", "/components/card/node/width"}, {"value", 160}}}}});

		const json state = askForSource(screen, window, changes, {{"source", "#cardFace"}, {"line", 3}});
		const json& source = state["source"];
		REQUIRE(source.is_object());
		CHECK(source["notation"] == "json");
		CHECK(source["address"] == "#cardFace");
		CHECK(source["file"] == "part.json");
		CHECK(source["text"].get<std::string>().contains("160"));
		CHECK(source["marked"] == 3);
		REQUIRE(source["blame"].is_object());
		CHECK(source["blame"]["number"] == 1);
	}
}

TEST_CASE("changes window: a source that names nothing shows the error, and the page stays") {
	for(const std::string notation : {"json", "lisp"}) { // the window in either notation
		CAPTURE(notation);
		affresco::Application app({nullptr, true});
		Screen screen = Screen::fromString(affresco::lisp::jsonText(lispApp, "index.screen.lisp"), {});
		affresco::Window& window = screen.createWindow(app);
		const host::ChangesWindow changes(app, screen, window, changesScreen(notation));
		const json state = askForSource(screen, window, changes, {{"source", "#nothing"}});
		CHECK(state["message"].get<std::string>().contains("#nothing"));
		REQUIRE(state["source"].is_object()); // still on the Source page, the error under it
		CHECK(state["source"].contains("error"));
		CHECK_FALSE(state["source"].contains("text"));
	}
}

TEST_CASE("changes window: a source too long, or not a text, is ignored: the window opens on the change") {
	for(const std::string notation : {"json", "lisp"}) { // the window in either notation
		CAPTURE(notation);
		affresco::Application app({nullptr, true});
		Screen screen = Screen::fromString(affresco::lisp::jsonText(lispApp, "index.screen.lisp"), {});
		affresco::Window& window = screen.createWindow(app);
		const host::ChangesWindow changes(app, screen, window, changesScreen(notation));
		for(const json& asked : {json{{"source", "#" + std::string(512, 'x')}}, json{{"source", 5}, {"line", 2}}}) {
			CAPTURE(asked.dump().size());
			const json state = askForSource(screen, window, changes, asked);
			CHECK(state["list"]["sourceCount"] == 0);
			CHECK(state["source"].is_null());
			REQUIRE(state["view"].is_object());
			CHECK(state["view"]["kind"] == "commit"); // nothing waits: the newest applied change, as without source
		}
	}
}
