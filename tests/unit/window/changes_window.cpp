/// @file
/// @brief The host's window on a change (apps/host/ChangesWindow.cpp): opened on a live proposal, it tells the truth
/// whatever that proposal does to the app, and a review it saw is the only one it can accept.

#include "../../../apps/host/ChangesWindow.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <filesystem>
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
/// @return Its path.
std::filesystem::path changesScreen() {
	return std::filesystem::path(AFFRESCOUI_EXAMPLES_DIRECTORY) / "shared" / "host" / "changes.screen.json";
}

} // namespace

TEST_CASE("changes window: it opens on the proposal, and tells the truth while that proposal is previewed") {
	affresco::Application app({nullptr, true});
	Screen screen = Screen::fromString(appScreen, {});
	affresco::Window& window = screen.createWindow(app);
	const host::ChangesWindow changes(app, screen, window, changesScreen());
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

TEST_CASE("changes window: only the change the person saw can be accepted, and then it can be undone") {
	affresco::Application app({nullptr, true});
	Screen screen = Screen::fromString(appScreen, {});
	affresco::Window& window = screen.createWindow(app);
	const host::ChangesWindow changes(app, screen, window, changesScreen());
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

TEST_CASE("changes window: a person's rejection reaches the author, with the reason") {
	affresco::Application app({nullptr, true});
	Screen screen = Screen::fromString(appScreen, {});
	affresco::Window& window = screen.createWindow(app);
	const host::ChangesWindow changes(app, screen, window, changesScreen());
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
