/// @file
/// @brief Whole flows of an undo that keeps later changes (VERSION_CONTROL.md, "Undo that keeps what came later"),
/// through a channel::Session over a MemoryChannel as a peer drives them: a program's definitions, a definition another
/// uses, a keyword edit, a model with records, a feature's point, an agent's undo and its review, a stale choice, a
/// limited reader, the changes rate and a forged undo mark. Written as a second look at the engine: each case asks
/// for the plan first, then the undo, and checks what the person was shown against what landed.

#include "Headless.hpp"
#include "Helpers.hpp"

#include "FeatureManifest.hpp"

#include <affresco/affresco.hpp>
#include <affresco/channel/Session.hpp>
#include <affresco/data/Store.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/lisp/Document.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

using affresco::json;
using affresco::channel::Session;
using affresco::test::MemoryChannel;
using affresco::vcs::Repository;

namespace {

/// @brief A peer: its channel and its session on a history. The channel comes first: it outlives the session.
struct Peer {
	MemoryChannel channel; ///< What the peer is sent.
	Session session;       ///< Its session, whose author is the channel's.

	/// @brief Opens a session on a history.
	/// @param versions The history; it must outlive this.
	/// @param author The channel's author.
	/// @param limits Its limits.
	Peer(affresco::VersionControl& versions, const std::string& author,
	     const affresco::channel::PeerLimits& limits = {})
	    : session(channel, versions, author, {}, nullptr, limits) {}

	/// @brief Sends a message and gives the answer.
	/// @param message The message.
	/// @return The last message the peer was sent.
	json send(const json& message) {
		channel.receive(message.dump());
		REQUIRE_FALSE(channel.sent.empty());
		return channel.sent.back();
	}

	/// @brief Asks for the plan of an undo.
	/// @param revision The commit to undo.
	/// @param choices More keys of the message ("drop", "keep", "then").
	/// @return The plan.
	json plan(const json& revision, json choices = json::object()) {
		choices["type"] = "undoPlan";
		choices["revision"] = revision;
		const json answer = send(choices);
		INFO(answer.dump());
		REQUIRE(answer["type"] == "undoPlan");
		return answer["plan"];
	}

	/// @brief Asks for an undo.
	/// @param revision The commit to undo.
	/// @param choices More keys of the message ("drop", "keep", "then", "head").
	/// @return The answer.
	json revert(const json& revision, json choices = json::object()) {
		choices["type"] = "revert";
		choices["revision"] = revision;
		return send(choices);
	}

	/// @brief Sends a revision that must land.
	/// @param revision The revision's keys ("set", "patch", ...).
	void land(json revision) {
		revision["type"] = "revision";
		const json answer = send(revision);
		INFO(answer.dump());
		REQUIRE(answer["type"] == "applied");
	}
};

/// @brief The text of every definition of a program, by file and name.
/// @param versions The history.
/// @return `{file: {name: text}}`.
json programOf(const affresco::VersionControl& versions) { return versions.sourceDocument()["program"]; }

/// A program of three definitions, a command with keywords, and a screen.
constexpr const char* program = R"lisp((def greeting "The greeting." "Hello")
(def farewell "The farewell." "Bye")
(defcommand save "Saves." :title "Save" :category "File" [] nil)
(defscreen main (column#list (label#title :text greeting) (label#end :text farewell)))
)lisp";

/// @brief The pointer of one of program's definitions.
/// @param name Its name.
/// @return `/program/app.lisp/<name>`.
std::string at(const std::string& name) { return "/program/app.lisp/" + name; }

/// @brief A history of program, and a peer for each of the agent, the studio and the person.
struct ProgramFlow {
	affresco::format::Rules rules;                                                   ///< The format's rules.
	Repository versions{rules, affresco::lisp::sourceDocument(program, "app.lisp")}; ///< The history.
	Peer agent{versions, "agent"};                                                   ///< An agent.
	Peer studio{versions, "studio"};                                                 ///< The studio.
	Peer person{versions, "user"};                                                   ///< The person.
	const json original = programOf(versions);                                       ///< The texts as loaded.
	std::vector<json> greetings{original["app.lisp"]["greeting"]}; ///< Every text "greeting" held on main.

	/// @brief Lands a revision of a peer, and keeps the text "greeting" holds after it.
	/// @param peer Who sends it.
	/// @param revision The revision's keys.
	void land(Peer& peer, const json& revision) {
		peer.land(revision);
		greetings.push_back(programOf(versions)["app.lisp"]["greeting"]);
	}
};

} // namespace

TEST_CASE("undo flows: a program: another definition is clean; the same one is a choice, and drop gives the text "
          "before, its first writer's") {
	ProgramFlow f;
	f.land(f.agent, {{"set", {{at("greeting"), R"lisp((def greeting "The greeting." "Hi"))lisp"}}}});    // A
	f.land(f.studio, {{"set", {{at("farewell"), R"lisp((def farewell "The farewell." "Ciao"))lisp"}}}}); // B
	CHECK(f.person.plan(1)["outcome"] == "clean");

	f.land(f.studio, {{"set", {{at("greeting"), R"lisp((def greeting "The greeting." "Hey"))lisp"}}}}); // C, on A
	const json plan = f.person.plan(1);
	REQUIRE(plan["outcome"] == "choose");
	REQUIRE(plan["conflicts"].size() == 1U);
	const json& conflict = plan["conflicts"][0];
	CHECK(conflict["path"] == at("greeting"));
	CHECK(conflict["then"].get<std::string>().contains("\"Hi\""));   // what A wrote
	CHECK(conflict["now"].get<std::string>().contains("\"Hey\""));   // what C wrote on it
	CHECK(conflict["before"] == f.original["app.lisp"]["greeting"]); // what was there before A
	CHECK(conflict["commits"] == json::array({3}));
	CHECK(f.versions.head().number == 3); // a plan changes nothing

	const json done = f.person.revert(1, {{"drop", {at("greeting")}}, {"head", plan["head"]}});
	INFO(done.dump());
	REQUIRE(done["type"] == "applied");
	const json now = programOf(f.versions);
	CHECK(now["app.lisp"]["greeting"] == f.original["app.lisp"]["greeting"]); // byte for byte
	CHECK(now["app.lisp"]["farewell"].get<std::string>().contains("Ciao"));   // B stays
	// A definition is one value: the undo wrote a text main had held before, never one of its own
	CHECK(std::ranges::find(f.greetings, now["app.lisp"]["greeting"]) != f.greetings.end());
	const json authors = f.versions.programAuthors();
	CHECK(authors["app.lisp"]["greeting"] == "file"); // who first wrote that text, never who undid
	CHECK(authors["app.lisp"]["farewell"] == "studio");
	CHECK(f.versions.head().undo["of"] == f.versions.log()[1].id);
	CHECK(f.versions.head().undo["drop"] == json::array({at("greeting")}));
}

TEST_CASE("undo flows: a definition another uses: undoing what added it is blocked, naming it") {
	ProgramFlow f;
	f.studio.land({{"set", {{at("shout"), R"lisp((defn shout "Loud." [s] (str s "!")))lisp"}}}});        // A
	f.studio.land({{"set", {{at("welcome"), R"lisp((defn welcome "Welcome." [] (shout "Hi")))lisp"}}}}); // B
	const json plan = f.person.plan(1);
	CHECK(plan["outcome"] == "blocked");
	INFO(plan["blocked"].dump());
	CHECK(plan["blocked"].get<std::string>().contains("shout"));
	CHECK(plan["blocked"].get<std::string>().contains("welcome"));
	const json refused = f.person.revert(1);
	CHECK(refused["type"] == "rejected");
	CHECK(f.versions.head().number == 2);
	CHECK(programOf(f.versions)["app.lisp"].contains("shout"));
}

TEST_CASE("undo flows: a keyword edit nothing edited later comes back byte for byte") {
	ProgramFlow f;
	f.agent.land({{"set", {{at("save") + "/:title", "Store"}}}}); // A: one keyword of a definition
	CHECK(f.versions.document()["actions"]["save"]["title"] == "Store");
	f.studio.land({{"set", {{at("farewell"), R"lisp((def farewell "The farewell." "Ciao"))lisp"}}}}); // B
	const json plan = f.person.plan(1);
	CHECK(plan["outcome"] == "clean");
	REQUIRE(f.person.revert(1, {{"head", plan["head"]}})["type"] == "applied");
	CHECK(programOf(f.versions)["app.lisp"]["save"] == f.original["app.lisp"]["save"]);
	CHECK(programOf(f.versions)["app.lisp"]["farewell"].get<std::string>().contains("Ciao"));
	CHECK(f.versions.document()["actions"]["save"]["title"] == "Save");
}

namespace {

/// @brief A notes screen with a model and a label that counts its records: made-up test data only.
/// @return The document.
json notesScreen() {
	return json::parse(R"json({
	"format": "affrescoui-screen", "version": 2,
	"models": { "notes": { "version": 1, "fields": {
		"title": { "type": "string", "required": true },
		"done": { "type": "boolean", "default": false } },
		"seed": [{ "id": "n1", "title": "First" }, { "id": "n2", "title": "Second", "done": true }] } },
	"root": { "type": "column", "id": "main", "children": [
		{ "type": "label", "id": "count", "text": "{{length(data.notes)}}" }] }
})json");
}

/// @brief The revision that renames a field of the notes, with its migration: "title" to "name" for version 2, then
/// another for version 3.
/// @param from The field's name now.
/// @param to Its new name.
/// @param version The model's version after it: 2 or 3.
/// @return The revision's keys.
json renameField(const std::string& from, const std::string& to, int version) {
	json migrations =
	    json::array({{{"from", 1}, {"to", 2}, {"steps", json::array({{{"rename", "title"}, {"to", "name"}}})}}});
	if(version == 3)
		migrations.push_back({{"from", 2}, {"to", 3}, {"steps", json::array({{{"rename", from}, {"to", to}}})}});
	return {{"set",
	         {{"/models/notes/fields/" + to, {{"type", "string"}, {"required", true}}},
	          {"/models/notes/version", version},
	          {"/models/notes/migrations", migrations}}},
	        {"remove", {"/models/notes/fields/" + from}}};
}

/// @brief A screen built in a headless window, and the person's session on its history. The window comes first: the
/// screen is built into it; the peer comes last, after the screen whose history it holds.
struct ScreenFlow {
	affresco::detail::ManualClock clock;      ///< The window's time.
	std::unique_ptr<affresco::Window> window; ///< The window.
	affresco::Screen screen;                  ///< The screen.
	std::unique_ptr<Peer> person;             ///< The person's session.
	std::unique_ptr<Peer> studio;             ///< The studio's.

	/// @brief Loads and builds a screen.
	/// @param document The screen.
	explicit ScreenFlow(const json& document)
	    : window(affresco::detail::createHeadlessWindow(400, 300, clock)),
	      screen(affresco::Screen::fromString(document.dump())) {
		screen.build(*window);
		window->render();
		person = std::make_unique<Peer>(screen.versions(), "user");
		studio = std::make_unique<Peer>(screen.versions(), "studio");
	}

	/// @brief Every record of the notes, as a query gives them, sorted by id.
	/// @return The records.
	json notes() { return json(screen.data().run({{"from", "notes"}, {"sort", "id"}}, "q")); }

	/// @brief Has one peer send a message whose change is always reviewed (a migration, or its undo), and the other
	/// accept it on the head it was proposed with.
	/// @param author Who sends it.
	/// @param reviewer Who accepts it.
	/// @param message The message.
	/// @return The reviewer's answer, or the author's when the change was not proposed.
	static json reviewed(Peer& author, Peer& reviewer, const json& message) {
		const json proposed = author.send(message);
		INFO(proposed.dump());
		REQUIRE(proposed["type"] == "proposed");
		return reviewer.send({{"type", "accept"}, {"proposal", proposed["proposal"]}, {"head", proposed["commit"]}});
	}
};

} // namespace

TEST_CASE("undo flows: a model with records: the migration's undo takes the records back, as the plan said") {
	ScreenFlow f(notesScreen());
	const json records = f.notes();
	json migration = renameField("title", "name", 2);
	migration["type"] = "revision";
	REQUIRE(ScreenFlow::reviewed(*f.person, *f.studio, migration)["type"] == "done"); // A: a migration, reviewed
	CHECK(f.notes()[0]["name"] == "First");
	static_cast<void>(f.screen.data().insert("notes", {{"name", "Third"}}, "user")); // records written after it
	f.studio->land({{"set", {{"#count/text", "{{length(data.notes)}} notes"}}}});    // B: a layout
	const json withThird = f.notes();
	REQUIRE(withThird.size() == 3U);

	const json plan = f.person->plan(1);
	REQUIRE(plan["outcome"] == "clean");
	INFO(plan["impact"].dump());
	REQUIRE(plan["impact"]["data"].is_string());
	CHECK(plan["impact"]["data"].get<std::string>().contains("3 records")); // said before anything changed
	CHECK(f.notes() == withThird);

	SUBCASE("undone: every record, the later one too, back through the migration's inverse") {
		const json undo = {{"type", "revert"}, {"revision", 1}, {"head", plan["head"]}};
		const json done = ScreenFlow::reviewed(*f.person, *f.studio, undo); // it transforms records: reviewed too
		INFO(done.dump());
		REQUIRE(done["type"] == "done");
		const json now = f.notes();
		REQUIRE(now.size() == 3U);
		for(const json& record : now) {
			CHECK(record.contains("title"));
			CHECK_FALSE(record.contains("name"));
		}
		std::vector<std::string> titles;
		for(const json& record : now) titles.push_back(record.value("title", std::string()));
		std::ranges::sort(titles);
		CHECK(titles == std::vector<std::string>{"First", "Second", "Third"});
		CHECK(json(f.screen.data().run({{"from", "notes"}, {"where", {{"id", "n1"}}}}, "q"))[0]["title"] ==
		      records[0]["title"]);
		CHECK(f.screen.versions().document()["root"]["children"][0]["text"] == "{{length(data.notes)}} notes"); // B
	}
	SUBCASE("a later migration of the same collection blocks it, naming that revision") {
		json again = renameField("name", "label", 3);
		again["type"] = "revision";
		REQUIRE(ScreenFlow::reviewed(*f.studio, *f.person, again)["type"] == "done");
		const json blocked = f.person->plan(1);
		CHECK(blocked["outcome"] == "blocked");
		CHECK(blocked["blocked"] == "revert: revision 3 changed the model 'notes' after it: undo that first");
		CHECK(f.person->revert(1)["type"] == "rejected");
		CHECK(f.notes().size() == 3U);
		for(const json& record : f.notes()) CHECK(record.contains("label")); // nothing changed
	}
	SUBCASE("a layout change undone leaves every record as it was") {
		REQUIRE(f.person->revert(2)["type"] == "applied");
		CHECK(f.notes() == withThird);
	}
}

namespace {

/// @brief A feature that puts a label in the "footer.right" point, as an installed record.
/// @return The record.
json clockFeature() {
	const json manifest = json::parse(R"json({
		"format": "affrescoui-feature",
		"version": 2,
		"name": "clock",
		"release": "1.0.0",
		"title": "Clock",
		"description": "Shows the time in the footer.",
		"license": "Apache-2.0",
		"requires": { "format": 2, "points": ["footer.right"] },
		"provides": { "views": { "footer.right": { "clock-label": { "type": "label", "text": "12:00" } } } }
	})json");
	return affresco::format::recordFor(manifest, "revision", "", "", 10);
}

} // namespace

TEST_CASE("undo flows: a feature on a point the undo takes away: the plan refuses it, naming the feature; the screen "
          "still builds") {
	ScreenFlow f(json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"root": { "type": "column", "id": "main", "children": [{ "type": "label", "id": "title", "text": "Tasks" }] }
	})json"));
	f.studio->land(
	    {{"patch", json::array({{{"op", "add"},
		                         {"path", "/root/children/-"},
		                         {"value", {{"type", "row"}, {"id", "footer"}, {"point", "footer.right"}}}}})}}); // A
	f.studio->land(
	    {{"patch",
		  json::array({{{"op", "add"}, {"path", "/features"}, {"value", {{"clock", clockFeature()}}}}})}}); // B
	f.window->runDeferredTasks();
	f.window->render();
	REQUIRE(f.screen.find("clock-label") != nullptr);

	// Every commit must compose, so an enabled feature left without its point blocks the undo: never a screen
	// that drops it unannounced (the warnings line is for what composition lets through)
	const json plan = f.person->plan(1);
	INFO(plan.dump());
	REQUIRE(plan["outcome"] == "blocked");
	CHECK(plan["blocked"].get<std::string>().contains("features.clock.requires.points"));
	CHECK(plan["blocked"].get<std::string>().contains("footer.right"));
	CHECK(f.person->revert(1)["type"] == "rejected");
	f.window->runDeferredTasks();
	f.window->render();
	CHECK(f.screen.find("footer") != nullptr); // it still builds, as it was
	CHECK(f.screen.find("clock-label") != nullptr);

	// Disabled first, the feature asks for no point: the undo is clean, and the screen builds without the footer
	f.studio->land({{"set", {{"/features/clock/enabled", false}}}});
	const json clean = f.person->plan(1);
	INFO(clean.dump());
	REQUIRE(clean["outcome"] == "clean");
	REQUIRE(f.person->revert(1, {{"head", clean["head"]}})["type"] == "applied");
	f.window->runDeferredTasks();
	f.window->render();
	CHECK(f.screen.find("title") != nullptr);
	CHECK(f.screen.find("footer") == nullptr);
	CHECK(f.screen.versions().document()["features"].contains("clock")); // B stays installed
}

namespace {

/// @brief A screen whose root has three labels, where the agent may change anything and only the studio reviews.
/// @return The document.
json agentScreen() {
	return json::parse(R"json({
		"format": "affrescoui-screen", "version": 2,
		"permissions": { "allow": ["studio", "agent"], "reviewers": ["studio"] },
		"root": { "type": "panel", "id": "main", "children": [
			{ "type": "label", "id": "first", "text": "One" },
			{ "type": "label", "id": "second", "text": "Two" },
			{ "type": "label", "id": "third", "text": "Three" }
		] }
	})json");
}

/// @brief A history of agentScreen() with three commits: A appends #fourth, B renames it, C renames #first; a peer
/// for the agent and one for the studio.
struct AgentFlow {
	affresco::format::Rules rules;             ///< The format's rules.
	Repository versions{rules, agentScreen()}; ///< The history.
	Peer agent{versions, "agent"};             ///< The agent: changes anything, reviews nothing.
	Peer studio{versions, "studio"};           ///< The reviewer.

	/// @brief Makes the three commits, as the agent.
	AgentFlow() {
		agent.land({{"patch", json::array({{{"op", "add"},
		                                    {"path", "/root/children/-"},
		                                    {"value", {{"type", "label"}, {"id", "fourth"}, {"text", "Four"}}}}})}});
		agent.land({{"set", {{"#fourth/text", "Vier"}}}});
		agent.land({{"set", {{"#first/text", "Uno"}}}});
	}

	/// @brief The text of a child of the root.
	/// @param id Its id.
	/// @return The text, or null if there is no such child.
	[[nodiscard]] json text(const std::string& id) const {
		for(const json& child : versions.document()["root"]["children"])
			if(child["id"] == id) return child.value("text", json());
		return nullptr;
	}

	/// @brief Has the studio accept a proposal on the head it saw.
	/// @param proposed The "proposed" answer.
	void accept(const json& proposed) {
		const json done =
		    studio.send({{"type", "accept"}, {"proposal", proposed["proposal"]}, {"head", proposed["commit"]}});
		INFO(done.dump());
		REQUIRE(done["type"] == "done");
	}
};

} // namespace

TEST_CASE("undo flows: an agent's clean undo lands as its rules allow; with drop it is proposed, also in acceptEdits") {
	AgentFlow f;
	REQUIRE(f.agent.revert(3)["type"] == "applied"); // clean: as any change the rules allow
	CHECK(f.text("first") == "One");
	CHECK(f.versions.head().undo["drop"] == json::array());

	f.versions.setMode(affresco::Mode::AcceptEdits);
	const json plan = f.agent.plan(1);
	REQUIRE(plan["outcome"] == "choose");
	const json proposed = f.agent.revert(1, {{"drop", "all"}, {"head", plan["head"]}});
	INFO(proposed.dump());
	REQUIRE(proposed["type"] == "proposed");
	CHECK(f.text("fourth") == "Vier"); // nothing changed yet
	REQUIRE(f.versions.proposals().size() == 1U);
	CHECK(f.versions.proposals()[0].remember.empty()); // never remembered

	// The agent can't accept its own undo
	const json own =
	    f.agent.send({{"type", "accept"}, {"proposal", proposed["proposal"]}, {"head", proposed["commit"]}});
	CHECK(own["type"] == "rejected");
	CHECK(f.text("fourth") == "Vier");

	const std::string undone = f.versions.log()[1].id;
	f.accept(proposed);
	CHECK(f.text("fourth").is_null());
	CHECK(f.text("first") == "One");
	CHECK(f.versions.head().undo["of"] == undone); // the merge commit carries the mark
	CHECK(f.versions.head().undo["drop"] == json::array({"#fourth"}));
	CHECK(f.versions.head().author == "agent");
	CHECK(f.versions.head().committer == "studio");
}

TEST_CASE("undo flows: an agent's undo with then is proposed, and the rebuilt node is there once accepted") {
	AgentFlow f;
	const json rebuild = json::array({{{"op", "add"},
	                                   {"path", "/root/children/-"},
	                                   {"value", {{"type", "label"}, {"id", "rebuilt"}, {"text", "Vier"}}}}});
	const json plan = f.agent.plan(1, {{"drop", "all"}, {"then", rebuild}});
	REQUIRE(plan["outcome"] == "ready");
	const json proposed = f.agent.revert(1, {{"drop", "all"}, {"then", rebuild}, {"head", plan["head"]}});
	INFO(proposed.dump());
	REQUIRE(proposed["type"] == "proposed");
	CHECK(f.text("rebuilt").is_null());
	f.accept(proposed);
	CHECK(f.text("fourth").is_null());
	CHECK(f.text("rebuilt") == "Vier");
	CHECK(f.versions.head().undo["then"] == 1);

	// then alone, with no later change dropped, is reviewed too
	const json again =
	    f.agent.revert(3, {{"then", json::array({{{"op", "replace"}, {"path", "#second/text"}, {"value", "Dos"}}})}});
	CHECK(again["type"] == "proposed");
}

TEST_CASE("undo flows: two parts traced back through the same commits: each names only the commits that built on it, "
          "past a node added before both") {
	const affresco::format::Rules rules;
	Repository versions(rules, agentScreen());
	Peer studio(versions, "studio");
	studio.land({{"set", {{"#second/text", "Dos"}, {"#third/text", "Tres"}}}}); // 1: both parts
	studio.land({{"patch", json::array({{{"op", "add"},                         // 2: moves both one place on
	                                     {"path", "/root/children/0"},
	                                     {"value", {{"type", "label"}, {"id", "zeroth"}, {"text", "Zero"}}}}})}});
	studio.land({{"set", {{"#third/text", "Drei"}}}});  // 3
	studio.land({{"set", {{"#second/text", "Zwei"}}}}); // 4
	studio.land({{"set", {{"#third/text", "Trois"}}}}); // 5
	const json plan = studio.plan(1);
	INFO(plan.dump());
	REQUIRE(plan["outcome"] == "choose");
	REQUIRE(plan["conflicts"].size() == 2U);
	for(const json& conflict : plan["conflicts"]) {
		const bool second = conflict["path"].get<std::string>().contains("second");
		CHECK(conflict["commits"] == (second ? json::array({4}) : json::array({3, 5})));
	}
	REQUIRE(plan["later"].size() == 3U); // 2 moved them, and changed neither
	CHECK(plan["later"][0]["number"] == 3);
	CHECK(plan["later"][1]["number"] == 4);
	CHECK(plan["later"][2]["number"] == 5);

	const json done = studio.revert(1, {{"drop", "all"}, {"head", plan["head"]}});
	INFO(done.dump());
	REQUIRE(done["type"] == "applied");
	const json& children = versions.document()["root"]["children"];
	CHECK(children[0]["id"] == "zeroth"); // what 2 built stays
	CHECK(children[2]["text"] == "Two");
	CHECK(children[3]["text"] == "Three");
}

TEST_CASE("undo flows: without permissions, an agent's undo with drop or then is still proposed; the person's lands") {
	json screen = agentScreen();
	screen.erase("permissions"); // anyone may change it, and anyone but the author may accept
	const affresco::format::Rules rules;
	Repository versions(rules, screen);
	Peer agent(versions, "agent");
	Peer person(versions, "user");
	agent.land({{"patch", json::array({{{"op", "add"},
	                                    {"path", "/root/children/-"},
	                                    {"value", {{"type", "label"}, {"id", "fourth"}, {"text", "Four"}}}}})}});
	agent.land({{"set", {{"#fourth/text", "Vier"}}}});
	agent.land({{"set", {{"#first/text", "Uno"}}}});

	CHECK(agent.revert(3)["type"] == "applied"); // clean: as any change
	const json plan = agent.plan(1);
	REQUIRE(plan["outcome"] == "choose");
	const json dropped = agent.revert(1, {{"drop", "all"}, {"head", plan["head"]}});
	INFO(dropped.dump());
	REQUIRE(dropped["type"] == "proposed");
	CHECK(versions.document()["root"]["children"].size() == 4U); // nothing taken out yet
	CHECK(versions.proposals()[0].remember.empty());
	const json rebuilt =
	    agent.revert(2, {{"then", json::array({{{"op", "replace"}, {"path", "#second/text"}, {"value", "Dos"}}})}});
	CHECK(rebuilt["type"] == "proposed"); // then alone is reviewed too
	CHECK(agent.send({{"type", "accept"}, {"proposal", dropped["proposal"]}, {"head", dropped["commit"]}})["type"] ==
	      "rejected"); // never its own

	// The person's own undo with the same choice lands at once
	const json again = person.plan(1);
	const json landed = person.revert(1, {{"drop", "all"}, {"head", again["head"]}});
	INFO(landed.dump());
	CHECK(landed["type"] == "applied");
	CHECK(versions.document()["root"]["children"].size() == 3U);
}

TEST_CASE("undo flows: a choice made on a plan main has moved past is rejected: look again") {
	AgentFlow f;
	const json plan = f.studio.plan(1);
	REQUIRE(plan["outcome"] == "choose");
	f.agent.land({{"set", {{"#second/text", "Dos"}}}}); // main moves
	const json stale = f.studio.revert(1, {{"drop", "all"}, {"head", plan["head"]}});
	CHECK(stale["type"] == "rejected");
	CHECK(stale["error"].get<std::string>().contains("look again"));
	CHECK(f.text("fourth") == "Vier");
	CHECK(f.versions.head().number == 4);
	// A head too short to name a commit is refused too
	CHECK(f.studio.revert(1, {{"drop", "all"}, {"head", "abc"}})["type"] == "rejected");
}

TEST_CASE("undo flows: a peer whose data rules limit its reads is refused the plan") {
	json screen = affresco::test::permissionsScreen();
	screen["permissions"]["data"] = json::array({{{"author", "agent"}, {"read", json::array({"/state/count"})}}});
	const affresco::format::Rules rules;
	Repository versions(rules, screen);
	Peer studio(versions, "studio");
	studio.land({{"set", {{"#title/text", "Done"}}}});
	Peer agent(versions, "agent");
	const json refused = agent.send({{"type", "undoPlan"}, {"revision", 1}, {"id", 1}});
	CHECK(refused["type"] == "rejected");
	CHECK_FALSE(refused.contains("plan"));
	CHECK(studio.plan(1)["outcome"] == "clean"); // a reader with every read gets it
}

TEST_CASE("undo flows: undoPlan counts against the changes rate, which revisions share") {
	AgentFlow f;
	affresco::channel::PeerLimits limits;
	limits.revisionBurst = 2;
	Peer limited(f.versions, "studio", limits);
	CHECK(limited.plan(3)["outcome"] == "clean");
	CHECK(limited.plan(3)["outcome"] == "clean");
	const json over = limited.send({{"type", "undoPlan"}, {"revision", 3}});
	CHECK(over["type"] == "rejected");
	CHECK(over["error"].get<std::string>().starts_with("too many changes"));
	const json revision = limited.send({{"type", "revision"}, {"set", {{"#second/text", "Dos"}}}});
	CHECK(revision["type"] == "rejected"); // the same rate
	CHECK(f.versions.head().number == 3);
}

TEST_CASE("undo flows: a revision whose undo mark names a commit it does not undo is rejected") {
	AgentFlow f;
	const std::string head = f.versions.head().id;
	const json forged = {
	    {"type", "revision"},
	    {"parent", head},
	    {"patch", json::array({{{"op", "replace"}, {"path", "/root/children/1/text"}, {"value", "Forged"}}})},
	    {"undo", {{"of", f.versions.log()[3].id}, {"drop", json::array()}, {"then", 0}}}};
	const json refused = f.agent.send(forged);
	CHECK(refused["type"] == "rejected");
	CHECK(refused["error"].get<std::string>().contains("is not the undo of"));

	json unknown = forged;
	unknown["undo"]["of"] = std::string(64, 'a');
	CHECK(f.agent.send(unknown)["type"] == "rejected");

	// The real undo, as the engine works it out, with a drop the agent added to look reviewed: refused as well
	json real = f.versions.undoRevision(3, "agent", {});
	real["type"] = "revision";
	real["undo"]["drop"] = json::array({"#first"});
	CHECK(f.agent.send(real)["type"] == "rejected");
	CHECK(f.versions.head().id == head);
	CHECK(f.versions.proposals().empty());
}
