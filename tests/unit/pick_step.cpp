/// @file
/// @brief The "pick" and "unpick" steps, "picks" on "ask", and Runtime::source() (SCREEN_FORMAT.md, "Pointing at a
/// node"): the person points at an element after a press of their own, the engine keeps a record of where it is
/// written, never of the app's data, and the agents get it with a request or read it with "getPicks" and
/// "getSource".

#include "Headless.hpp"
#include "Helpers.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using affresco::Action;
using affresco::json;
using affresco::Key;
using affresco::Mods;
using affresco::MouseButton;
using affresco::Panel;
using affresco::detail::createHeadlessWindow;
using affresco::detail::ManualClock;

namespace {

/// @brief A screen whose person points at its nodes: a title, a repeat, a component of its own, a kit's component,
/// a node without an id, and a label showing a value no record may carry.
constexpr const char* pointingScreen = R"json({
	"format": "affrescoui-screen",
	"version": 2,
	"state": { "pointed": null, "last": null, "cancelled": 0, "asked": "", "items": ["a", "b", "c"],
	           "secret": "TOP-SECRET-42", "query": "" },
	"permissions": { "allow": ["studio"] },
	"components": {
		"chip": { "params": { "label": "" },
		          "node": { "type": "row", "height": 20, "width": 120, "children": [
		              { "type": "label", "id": "chipText", "text": "{{label}}", "width": 80, "height": 20 }] } }
	},
	"actions": {
		"point": [{ "pick": "node", "hint": "Point at what to change", "into": "state.pointed",
		            "onDone": [{ "set": "state.last", "to": "{{result}}" }],
		            "onCancel": [{ "increment": "state.cancelled" }] }],
		"send": [{ "ask": "Make it blue", "picks": ["{{state.pointed.id}}"], "into": "state.asked" }],
		"drop": [{ "unpick": "{{state.pointed.id}}" }]
	},
	"root": { "id": "root", "children": [{ "type": "column", "id": "main", "width": 200, "height": 300, "children": [
		{ "type": "label", "id": "pointButton", "text": "Point", "height": 24, "width": 120, "onClick": [{ "run": "point" }] },
		{ "type": "label", "id": "title", "text": "{{state.secret}}", "height": 20, "width": 120, "onClick": [{ "run": "send" }] },
		{ "type": "label", "id": "item", "text": "{{item}}", "height": 20, "width": 120, "repeat": { "for": "item", "in": "state.items" } },
		{ "use": "chip", "id": "chip", "with": { "label": "Chip" } },
		{ "type": "row", "id": "box", "height": 20, "width": 120, "children": [{ "type": "label", "text": "inside", "width": 60 }] },
		{ "type": "label", "id": "later", "text": "Later", "height": 24, "width": 120,
		  "onClick": [{ "after": 10, "do": [{ "pick": "node" }] }] },
		{ "id": "find", "use": "searchField", "with": { "name": "query", "width": 180 } }
	] }] }
})json";

/// @brief A screen in a headless window that the person points at, the agent's channel first: it outlives the screen.
struct Pointing {
	affresco::test::MemoryChannel agent;      ///< What the agent is sent.
	ManualClock clock;                        ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.

	/// @brief Builds a screen.
	/// @param made The screen, loaded.
	explicit Pointing(affresco::Screen made) : window(createHeadlessWindow(400, 400, clock)), screen(std::move(made)) {
		screen.build(*window);
		settle();
	}

	/// @brief Lets what an event deferred happen.
	void settle() {
		for(int i = 0; i < 3; ++i) {
			clock.advance(20.0);
			window->runDeferredTasks();
			window->render();
		}
	}

	/// @brief The n-th panel with an id, in the order of the tree.
	/// @param id The id.
	/// @param n Which, from 0.
	/// @return It, or nullptr.
	[[nodiscard]] Panel* panel(const std::string& id, std::size_t n = 0) const {
		std::vector<Panel*> found;
		collect(&window->root(), id, found);
		return n < found.size() ? found[n] : nullptr;
	}

	/// @brief Clicks a panel's middle, the person's own press.
	/// @param target The panel.
	void click(const Panel* target) {
		REQUIRE(target);
		const affresco::Rect& frame = target->frame();
		const affresco::Vec2 at{(frame.left + frame.right) / 2.0f, (frame.top + frame.bottom) / 2.0f};
		window->routeMouseButton({MouseButton::Left, Action::Press, Mods{}, at});
		window->routeMouseButton({MouseButton::Left, Action::Release, Mods{}, at});
		settle();
	}

	/// @brief Presses the button that starts a pick, then clicks a panel.
	/// @param target The panel.
	void point(const Panel* target) {
		click(panel("pointButton"));
		REQUIRE(window->picking());
		click(target);
	}

	/// @brief Presses and lets go of a key.
	/// @param key The key.
	void type(Key key) {
		window->routeKey({key, 0, Action::Press, Mods{}});
		window->routeKey({key, 0, Action::Release, Mods{}});
		settle();
	}

	/// @brief The screen's state.
	/// @return It.
	[[nodiscard]] const json& state() { return screen.versions().state(); }

	/// @brief The agent's answer to a message.
	/// @param message The message.
	/// @return The last message the agent was sent.
	json answer(const json& message) {
		agent.receive(message.dump());
		settle();
		return agent.sent.empty() ? json() : agent.sent.back();
	}

private:
	/// @brief Collects the panels with an id, depth first.
	/// @param at Where to look.
	/// @param id The id.
	/// @param found What was found.
	static void collect(Panel* at, const std::string& id, std::vector<Panel*>& found) {
		if(at->id() == id) found.push_back(at);
		for(const auto& child : at->children()) collect(child.get(), id, found);
	}
};

/// @brief A folder of files for a screen that includes others, removed when it goes.
struct Folder {
	Folder() = default;                        ///< A folder named after this object, made by the first write.
	Folder(const Folder&) = delete;            ///< One folder, removed once.
	Folder& operator=(const Folder&) = delete; ///< One folder, removed once.
	Folder(Folder&&) = delete;                 ///< It stays where its path names it.
	Folder& operator=(Folder&&) = delete;      ///< It stays where its path names it.

	std::filesystem::path path = std::filesystem::temp_directory_path() /
	                             ("affrescoui-pick-" + std::to_string(reinterpret_cast<std::uintptr_t>(this))); ///< It.

	/// @brief Writes a file.
	/// @param name Its path inside the folder.
	/// @param text Its text.
	/// @return Its path.
	std::filesystem::path write(const std::string& name, const std::string& text) const {
		const std::filesystem::path file = path / name;
		std::filesystem::create_directories(file.parent_path());
		std::ofstream(file) << text;
		return file;
	}

	/// @brief Removes it.
	~Folder() {
		std::error_code ignored;
		std::filesystem::remove_all(path, ignored);
	}
};

/// @brief A program for a screen: a view in its own file, a button that points, and a definition an agent may change.
constexpr const char* pointingProgram = R"lisp((include "parts.lisp" "../shared/bits.lisp")

(defstate pointed nil)
(defstate last nil)
(defstate x 0)

(defn point "Lets the person point." [] (pick "node" :hint "Point" :into "state.pointed" :onDone pointed))
(defn pointed "Keeps what was pointed at." [result] (set "state.last" result))
(defn other "Does something else." [] (set "state.x" 1))

(defscreen main
  :permissions {:allow [studio agent]}
  (panel#root (column#main :width 200 :height 300
    (label#pointButton :text "Point" :width 120 :height 24 :onClick point)
    (label#otherButton :text "Other" :width 120 :height 24 :onClick other)
    ~(cards)
    ~(badge))))
)lisp";

/// @brief A Lisp screen in a folder, with a file of its own and a shared one.
/// @param folder The folder.
/// @return The screen, loaded.
affresco::Screen lispScreen(const Folder& folder) {
	folder.write("app/parts.lisp", R"lisp(
(defview cards "The cards." []
  `(column#cards :width 120 :height 20
     (label#cardTitle :text "Card" :width 120 :height 20)))
)lisp");
	folder.write("shared/bits.lisp",
	             "(defview badge \"A badge.\" [] `(label#badge :text \"1\" :width 120 :height 20))\n");
	return affresco::Screen::fromFile(folder.write("app/index.screen.lisp", pointingProgram));
}

} // namespace

TEST_CASE("pick step: a JSON node's record says where it is written, with its bindings, and never its value") {
	Pointing p(affresco::Screen::fromString(pointingScreen));
	p.point(p.panel("title"));
	CHECK_FALSE(p.window->picking());

	const json& record = p.state()["pointed"];
	REQUIRE(record.is_object());
	CHECK(record["id"] == "pick-1");
	CHECK(record["revision"] == 0);
	CHECK(record["node"] == "#title");
	CHECK(record["type"] == "label");
	CHECK(record["pointer"] == "/root/children/0/children/1");
	CHECK(record["exact"] == true);
	CHECK(record["bindings"] == json{{"text", "{{state.secret}}"}});
	CHECK(record["handlers"] == json::array({"onClick"}));
	CHECK(record["source"]["address"] == "#title");
	REQUIRE(record["bounds"].size() == 4);
	CHECK(record["bounds"][3] == 20);
	CHECK_FALSE(record.contains("repeat"));
	CHECK_FALSE(record.contains("component"));
	CHECK(record.dump().find("TOP-SECRET-42") == std::string::npos); // the binding as written, not what it shows
	CHECK(p.state()["last"] == record);                              // onDone's result
	CHECK(p.screen.picks() == json::array({record}));
}

TEST_CASE("pick step: a repeat's item, a component's node and a kit's, and a node without an id") {
	Pointing p(affresco::Screen::fromString(pointingScreen));
	p.point(p.panel("item", 1));
	json record = p.state()["pointed"];
	CHECK(record["node"] == "#item");
	REQUIRE(record["repeat"].size() == 1);
	CHECK(record["repeat"][0] == json{{"for", "item"}, {"in", "state.items"}, {"index", 1}});

	p.point(p.panel("chipText"));
	record = p.state()["pointed"];
	CHECK(record["node"] == "#chipText");
	CHECK(record["pointer"] == "/components/chip/node/children/0");
	CHECK(record["component"] == json{{"name", "chip"}, {"kit", false}, {"usedAt", "#chip"}});

	const Panel* box = p.panel("box");
	REQUIRE(box);
	REQUIRE_FALSE(box->children().empty());
	p.point(box->children().front().get());
	record = p.state()["pointed"];
	CHECK(record["node"] == "#box");
	CHECK(record["exact"] == false);
	CHECK(record["hitType"] == "label");
	CHECK(record["id"] == "pick-3");

	const Panel* find = p.panel("find");
	REQUIRE(find);
	REQUIRE_FALSE(find->children().empty());
	p.point(find->children().front().get()); // the kit's field inside the use
	record = p.state()["pointed"];
	CHECK(record["node"] == "#find");
	CHECK(record["component"] == json{{"name", "searchField"}, {"kit", true}, {"usedAt", "#find"}});
}

TEST_CASE("pick step: Escape runs onCancel; a second pick, a timer and a missing window start none") {
	Pointing p(affresco::Screen::fromString(pointingScreen));
	p.click(p.panel("pointButton"));
	REQUIRE(p.window->picking());
	p.type(Key::Escape);
	CHECK_FALSE(p.window->picking());
	CHECK(p.state()["cancelled"] == 1);
	CHECK(p.state()["pointed"].is_null());
	CHECK(p.screen.picks().empty());

	p.click(p.panel("later")); // the press runs a timer, and the timer can't start a pick
	p.clock.advance(50.0);
	p.settle();
	CHECK_FALSE(p.window->picking());

	// Without a window, nothing to point at: nothing happens, and no error
	affresco::Screen headless = affresco::Screen::fromString(pointingScreen);
	CHECK_NOTHROW(headless.runAction("point", p.window->root(), "{}"));
	CHECK(headless.picks().empty());
}

TEST_CASE("pick step: a pick without onDone or onCancel keeps its record, and a cancel says nothing") {
	json screen = json::parse(pointingScreen);
	screen["actions"]["point"] = json::array({{{"pick", "node"}, {"into", "state.pointed"}}});
	Pointing p(affresco::Screen::fromString(screen.dump()));
	std::string errors;
	(void)p.screen.onError([&errors](const std::string& message) { errors += message + "\n"; });
	p.point(p.panel("title"));
	CHECK(p.state()["pointed"]["node"] == "#title");
	p.click(p.panel("pointButton"));
	p.type(Key::Escape);
	CHECK_FALSE(p.window->picking());
	CHECK(errors.empty());
}

TEST_CASE("pick step: the step's shape is checked") {
	const auto refused = [](const char* step) {
		json screen = json::parse(pointingScreen);
		screen["actions"]["bad"] = json::array({json::parse(step)});
		Pointing p(affresco::Screen::fromString(screen.dump()));
		std::string error;
		(void)p.screen.onError([&error](const std::string& message) { error = message; });
		p.screen.runAction("bad", p.window->root(), "{}");
		p.settle();
		return error;
	};
	CHECK(refused(R"({"pick": "panel"})").find(R"(expected "node")") != std::string::npos);
	CHECK(refused(R"({"pick": "node", "hint": 3})").find(".hint") != std::string::npos);
	const json longHint = {{"pick", "node"}, {"hint", std::string(201, 'h')}};
	CHECK(refused(longHint.dump().c_str()).find("longer than 200 bytes") != std::string::npos);
	// Run by itself, not from a press of the person's own
	CHECK(refused(R"({"pick": "node"})").find("a pick starts from the person's own input") != std::string::npos);
}

TEST_CASE("pick step: unpick forgets, the table keeps sixteen, and ask sends the records once") {
	Pointing p(affresco::Screen::fromString(pointingScreen));
	p.screen.connect(p.agent, "agent");
	p.point(p.panel("title"));
	const json record = p.state()["pointed"];
	p.screen.runAction("send", p.window->root(), "{}");
	p.settle();
	std::vector<json> requests;
	for(const json& message : p.agent.sent)
		if(message.value("type", "") == "request") requests.push_back(message);
	REQUIRE(requests.size() == 1);
	CHECK(requests[0]["picks"] == json::array({record}));
	CHECK(p.screen.picks().empty()); // sent once

	p.point(p.panel("title"));
	CHECK(p.screen.picks().size() == 1);
	p.screen.runAction("drop", p.window->root(), "{}");
	CHECK(p.screen.picks().empty());

	for(int i = 0; i < 17; ++i) p.point(p.panel("title"));
	const json kept = p.screen.picks();
	REQUIRE(kept.size() == 16);
	CHECK(kept[0]["id"] == "pick-4"); // the oldest went
	CHECK(kept[15]["id"] == "pick-19");
}

TEST_CASE("pick step: ask refuses an unknown pick, a made-up object, and more than four") {
	const auto askWith = [](const json& picks) {
		json screen = json::parse(pointingScreen);
		screen["actions"]["send"] = json::array({{{"ask", "Change it"}, {"picks", picks}}});
		Pointing p(affresco::Screen::fromString(screen.dump()));
		p.screen.connect(p.agent, "agent");
		p.point(p.panel("title"));
		std::string error;
		(void)p.screen.onError([&error](const std::string& message) { error = message; });
		p.screen.runAction("send", p.window->root(), "{}");
		p.settle();
		for(const json& message : p.agent.sent)
			if(message.value("type", "") == "request") return message.dump();
		return error;
	};
	CHECK(askWith(json::array({"pick-9"})).find("no pick 'pick-9'") != std::string::npos);
	// An object written by the document: only its id counts, and the engine's record goes
	const std::string sent = askWith(json::array({{{"id", "pick-1"}, {"node", "#fake"}, {"secret", "x"}}}));
	CHECK(sent.find("\"picks\"") != std::string::npos);
	CHECK(sent.find("#fake") == std::string::npos);
	CHECK(askWith(json::array({{{"node", "#fake"}}})).find("expected pick ids") != std::string::npos);
	CHECK(askWith(json::array({"pick-1", "pick-1", "pick-1", "pick-1", "pick-1"})).find("at most 4 picks") !=
	      std::string::npos);
}

TEST_CASE("pick step: a peer whose reads are limited gets the request without picks, and can't read them") {
	json screen = json::parse(pointingScreen);
	screen["permissions"]["data"] = json::array({{{"author", "agent"}, {"read", json::array({"/state/asked"})}}});
	Pointing p(affresco::Screen::fromString(screen.dump()));
	p.screen.connect(p.agent, "agent");
	p.point(p.panel("title"));
	p.screen.runAction("send", p.window->root(), "{}");
	p.settle();
	bool found = false;
	for(const json& message : p.agent.sent)
		if(message.value("type", "") == "request") {
			found = true;
			CHECK_FALSE(message.contains("picks"));
		}
	CHECK(found);
	p.point(p.panel("title"));
	CHECK(p.answer({{"type", "getPicks"}, {"id", 1}})["type"] == "rejected");
	CHECK(p.answer({{"type", "getSource"}, {"at", "#title"}, {"id", 2}})["type"] == "rejected");
}

TEST_CASE("pick step: an agent reads what is pointed at and where a node is written, through its channel") {
	Pointing p(affresco::Screen::fromString(pointingScreen));
	p.screen.connect(p.agent, "agent");
	p.point(p.panel("title"));
	const json picks = p.answer({{"type", "getPicks"}, {"id", 1}});
	CHECK(picks["type"] == "picks");
	CHECK(picks["picks"] == p.screen.picks());
	const json source = p.answer({{"type", "getSource"}, {"at", "#title"}, {"id", 2}});
	REQUIRE(source["type"] == "source");
	CHECK(source["source"]["notation"] == "json");
	CHECK(source["source"]["pointer"] == "/root/children/0/children/1");
	CHECK(p.answer({{"type", "getSource"}, {"at", "#nothing"}, {"id", 3}})["type"] == "rejected");
}

TEST_CASE("pick step: Runtime::source in JSON: a node by id or pointer, cut at 16 KiB, and nothing there throws") {
	json screen = json::parse(pointingScreen);
	screen["root"]["children"].push_back({{"type", "label"}, {"id", "long"}, {"text", std::string(20000, 'x')}});
	const affresco::Screen loaded = affresco::Screen::fromString(screen.dump());
	const json byId = loaded.source("#title");
	CHECK(byId["node"] == "#title");
	CHECK(byId["notation"] == "json");
	CHECK(json::parse(byId["text"].get<std::string>())["id"] == "title");
	CHECK_FALSE(byId.contains("cut"));
	CHECK(loaded.source("/root/children/0/children/1")["node"] == "#title"); // a pointer to a node with an id names it
	const json cut = loaded.source("#long");
	CHECK(cut["cut"] == true);
	CHECK(cut["text"].get<std::string>().size() <= 16384);
	CHECK_THROWS_WITH_AS(static_cast<void>(loaded.source("#nothing")), doctest::Contains("#nothing"),
	                     affresco::DocumentError);
	CHECK_THROWS_AS(static_cast<void>(loaded.source("/root/children/0/children/99")), affresco::DocumentError);
	CHECK_THROWS_AS(static_cast<void>(loaded.source("nothing")), affresco::DocumentError);
	CHECK_THROWS_AS(static_cast<void>(loaded.source("#" + std::string(600, 'x'))), affresco::DocumentError);
}

TEST_CASE("pick step: a program's node: its definition, an included file and a shared one, and its source") {
	const Folder folder;
	Pointing p(lispScreen(folder));
	p.point(p.panel("cardTitle"));
	json record = p.state()["pointed"];
	REQUIRE(record.is_object());
	CHECK(record["node"] == "#cardTitle");
	CHECK(record["exact"] == true);
	CHECK(record["source"]["address"] == "/program/parts.lisp/cards");
	CHECK(record["source"]["kind"] == "defview");
	CHECK(record["source"]["file"] == "parts.lisp");
	CHECK(record["source"]["line"] == 3);
	CHECK(record["source"]["shared"] == false);

	p.point(p.panel("badge"));
	record = p.state()["pointed"];
	CHECK(record["source"]["file"] == "../shared/bits.lisp");
	CHECK(record["source"]["shared"] == true);
	CHECK(record["source"]["address"] == "/program/..~1shared~1bits.lisp/badge");

	const json source = p.screen.source("#cardTitle");
	CHECK(source["notation"] == "lisp");
	CHECK(source["text"].get<std::string>().find("(defview cards") != std::string::npos);
	const json definition = p.screen.source("/program/parts.lisp/cards");
	CHECK(definition["kind"] == "defview");
	CHECK_FALSE(definition.contains("node"));
	CHECK_THROWS_AS(static_cast<void>(p.screen.source("/program/parts.lisp/nothing")), affresco::DocumentError);
}

TEST_CASE("pick step: code a revision wrote can't start a pick") {
	const Folder folder;
	Pointing p(lispScreen(folder));
	std::string error;
	(void)p.screen.onError([&error](const std::string& message) { error += message + "\n"; });
	const json revision = {
	    {"author", "agent"},
	    {"message", "Point from the other button"},
	    {"set", {{"/program/index.screen.lisp/other", R"((defn other "Points." [] (pick "node")))"}}}};
	const affresco::RevisionResult result = p.screen.submitRevision(revision.dump());
	p.settle();
	p.click(p.panel("otherButton"));
	CHECK_FALSE(p.window->picking());
	// Whether the revision is refused or lands, the step it added is a control step it can't run
	if(result.status == affresco::RevisionStatus::Applied) CHECK(error.find("pick") != std::string::npos);
	CHECK(p.screen.picks().empty());
}
