/// @file
/// @brief The built-in chat components (modules/themes/components/chat.json): the composer's keys, the launcher's name
/// and badge, the proposal card's buttons, and the bubble's and the typing indicator's names.

#include "Headless.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <string>
#include <vector>

using affresco::json;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen built in a headless window.
struct Built {
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param text The screen.
	explicit Built(const std::string& text)
	    : window(createHeadlessWindow(400, 300, clock)), screen(affresco::Screen::fromString(text)) {
		screen.build(*window);
		window->render();
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
		window->render();
	}

	/// @brief Clicks a point.
	/// @param x Its x, in pixels.
	/// @param y Its y, in pixels.
	void click(float x, float y) {
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Press, {}, {x, y}});
		window->routeMouseButton({affresco::MouseButton::Left, affresco::Action::Release, {}, {x, y}});
		window->render();
	}

	/// @brief Reads a state key.
	/// @param key Its name.
	/// @return Its value.
	[[nodiscard]] json state(const char* key) const { return json::parse(screen.stateJson())[key]; }
};

/// @brief A screen with a composer that sends into state.sent and closes into state.closed.
constexpr const char* composerScreen = R"json({ "format": "affrescoui-screen", "version": 2,
	"state": { "draft": "", "sent": [], "closed": false },
	"actions": { "send": [{ "push": "state.sent", "value": "{{state.draft}}" }, { "set": "state.draft", "to": "" }],
	             "close": [{ "set": "state.closed", "to": true }] },
	"root": { "id": "root", "children": [{ "id": "composer", "use": "chatComposer", "with": {
		"bind": "state.draft", "value": "{{state.draft}}", "send": "send", "close": "close", "autofocus": true,
		"placeholder": "Ask anything" }, "width": 300 }] } })json";

/// @brief The texts of a panel's shown labels, depth first.
/// @param panel The panel.
/// @return Them.
std::vector<std::string> shownTexts(const affresco::Panel& panel) {
	std::vector<std::string> result;
	if(!panel.isVisible()) return result;
	if(const auto* label = dynamic_cast<const affresco::Label*>(&panel)) result.push_back(label->text());
	for(const auto& child : panel.children())
		for(std::string& text : shownTexts(*child)) result.push_back(std::move(text));
	return result;
}

} // namespace

TEST_CASE("chat: Enter in the composer sends the draft, Shift+Enter breaks the line, and an empty one isn't sent") {
	Built built(composerScreen);
	built.press(affresco::Key::Enter);
	CHECK(built.state("sent").empty());
	built.type("hello");
	built.press(affresco::Key::Enter, 0x0001);
	built.type("there");
	CHECK(built.state("draft") == "hello\nthere");
	built.press(affresco::Key::Enter);
	CHECK(built.state("sent") == json::array({"hello\nthere"}));
	CHECK(built.state("draft") == "");
	built.type("   ");
	built.press(affresco::Key::Enter);
	CHECK(built.state("sent").size() == 1); // only spaces: nothing to send
}

TEST_CASE("chat: Escape in the composer runs its close action") {
	Built built(composerScreen);
	built.press(affresco::Key::Escape);
	CHECK(built.state("closed") == true);
}

TEST_CASE("chat: the launcher toggles its place, names the unread count and shows a badge") {
	Built built(R"json({ "format": "affrescoui-screen", "version": 2, "state": { "chatOpen": false },
		"root": { "id": "root", "children": [{ "id": "launcher", "use": "chatLauncher", "with": {
		"open": "state.chatOpen", "isOpen": "{{state.chatOpen}}", "unread": 3, "label": "Assistant" } }] } })json");
	affresco::Panel* launcher = built.screen.find("launcher");
	REQUIRE(launcher);
	CHECK(affresco::accessibleName(*launcher) == "Assistant (3)");
	CHECK(affresco::accessibilityTree(*launcher).role == "button");
	built.click(28.0f, 28.0f);
	CHECK(built.state("chatOpen") == true);
	built.click(28.0f, 28.0f);
	CHECK(built.state("chatOpen") == false);
}

TEST_CASE("chat: a proposal card's buttons run their actions with its value, and a decided one shows the outcome") {
	const std::string screen = R"json({ "format": "affrescoui-screen", "version": 2,
		"state": { "log": [], "status": "pending" },
		"actions": { "preview": [{ "push": "state.log", "value": "{{'preview ' + id}}" }],
		             "accept": [{ "push": "state.log", "value": "{{'accept ' + id}}" }],
		             "reject": [{ "push": "state.log", "value": "{{'reject ' + id}}" }] },
		"root": { "id": "root", "children": [{ "id": "card", "use": "proposalCard", "with": {
		"title": "Add a column", "summary": "Adds Review between Doing and Done.", "status": "{{state.status}}",
		"preview": "preview", "accept": "accept", "reject": "reject", "with": { "id": "p1" } } }] } })json";
	Built built(screen);
	CHECK(affresco::accessibleName(*built.screen.find("card")) == "Add a column");
	built.click(50.0f, 76.0f);  // Preview
	built.click(140.0f, 76.0f); // Accept
	built.click(230.0f, 76.0f); // Reject
	CHECK(built.state("log") == json::array({"preview p1", "accept p1", "reject p1"}));

	Built decided(std::string(screen).replace(screen.find("\"pending\""), 9, "\"accepted\""));
	decided.click(140.0f, 76.0f);
	CHECK(decided.state("log").empty()); // no buttons once decided
}

TEST_CASE("chat: a bubble is named by its author and text, and the typing indicator by its label") {
	Built built(R"json({ "format": "affrescoui-screen", "version": 2, "state": {},
		"root": { "id": "root", "type": "column", "children": [
		{ "id": "bubble", "use": "chatBubble", "with": { "text": "Done.", "from": "agent" } },
		{ "id": "typing", "use": "typingIndicator", "with": { "label": "The assistant is typing", "phase": 1 } }
		] } })json");
	CHECK(affresco::accessibleName(*built.screen.find("bubble")) == "agent: Done.");
	CHECK(affresco::accessibleName(*built.screen.find("typing")) == "The assistant is typing");
}

TEST_CASE("chat: a proposal card whose proposal is gone says so, and an unknown status shows as it is") {
	const std::string screen = R"json({ "format": "affrescoui-screen", "version": 2, "state": {},
		"root": { "id": "root", "children": [{ "id": "card", "use": "proposalCard", "with": {
		"title": "Add a column", "summary": "Adds Review.", "status": "closed", "accept": "accept" } }] } })json";
	Built closed(screen);
	const std::vector<std::string> texts = shownTexts(*closed.screen.find("card"));
	CHECK(std::find(texts.begin(), texts.end(), "Closed") != texts.end());
	CHECK(std::find(texts.begin(), texts.end(), "Accept") == texts.end()); // no buttons

	Built other(std::string(screen).replace(screen.find("\"closed\""), 8, "\"merged\""));
	const std::vector<std::string> otherTexts = shownTexts(*other.screen.find("card"));
	CHECK(std::find(otherTexts.begin(), otherTexts.end(), "merged") != otherTexts.end());
}
