/// @file
/// @brief Unit tests for the depth limit on what revisions and steps build (maxDocumentNesting): each message is
/// limited, and so is the screen that many of them build up.

#include "Helpers.hpp"

#include <affresco/affresco.hpp>
#include <affresco/format/Rules.hpp>
#include <affresco/vcs/Repository.hpp>

#include <doctest/doctest.h>

#include <string>

using affresco::json;
using affresco::maxDocumentNesting;

namespace {

/// @brief Lists inside lists.
/// @param depth How many levels (0: the number 0).
/// @return E.g. [[0]] for 2.
json listsNested(int depth) {
	json value = 0;
	for(int i = 0; i < depth; ++i) { // moved, not copied: a copy recurses, as deep as the value
		json list = json::array();
		list.push_back(std::move(value));
		value = std::move(list);
	}
	return value;
}

/// @brief The JSON Pointer of the innermost item of listsNested(depth), below a key.
/// @param key The key the lists are under.
/// @param depth How many levels of lists.
/// @return E.g. "/state/a/0/0" for "/state/a", 2.
std::string innermost(const std::string& key, int depth) {
	std::string pointer = key;
	for(int i = 0; i < depth; ++i) pointer += "/0";
	return pointer;
}

/// @brief A screen whose "wrap" action puts the value of state.box inside a new object, one level deeper each time.
constexpr const char* wrappingScreen = R"json({
	"format": "affrescoui-screen",
	"version": 1,
	"state": { "box": 0, "list": [] },
	"actions": {
		"wrap": [{ "set": "state.box", "to": { "inside": "{{state.box}}" } }],
		"pushBox": [{ "push": "state.list", "value": "{{state.box}}" }]
	},
	"root": { "type": "panel", "id": "main" }
})json";

} // namespace

TEST_CASE("core: nestingDepth counts levels as parseUntrusted does, and pointerDepth counts segments") {
	CHECK(affresco::nestingDepth(json(3)) == 0);
	CHECK(affresco::nestingDepth(json::array()) == 1);
	CHECK(affresco::nestingDepth(json::parse(R"({"a": [1, {"b": []}], "c": 2})")) == 4);
	CHECK(affresco::nestingDepth(listsNested(100'000)) == 100'000); // no recursion: deeper than any stack

	CHECK(affresco::pointerDepth(json::json_pointer("")) == 0);
	CHECK(affresco::pointerDepth(json::json_pointer("/a/0")) == 2);
	CHECK(affresco::pointerDepth(json::json_pointer("/a~1b/c")) == 2); // a '/' in a key is one segment

	CHECK_NOTHROW(affresco::expect::shallowEnough(listsNested(maxDocumentNesting - 1), 1, "here"));
	CHECK_THROWS_WITH_AS(affresco::expect::shallowEnough(listsNested(maxDocumentNesting), 1, "here"),
	                     "AffrescoUI screen: here: would nest the state more than 200 levels deep",
	                     affresco::DocumentError);
}

// Before, the 256 levels held for each message only: revisions could each add a value inside the last one's, and
// the screen grew deeper with every change, beyond what a history file or a peer can read back
TEST_CASE("vcs: a revision that would make the screen deeper than maxDocumentNesting is refused") {
	affresco::format::Rules rules;
	affresco::vcs::Repository versions(rules, affresco::test::permissionsScreen());
	const auto add = [&](const std::string& path, const json& value) {
		return versions.submit({{"author", "studio"}, {"patch", {{{"op", "add"}, {"path", path}, {"value", value}}}}});
	};

	// The screen with its state is one level, "state" a second: a value at /state/a can be 198 deep
	CHECK(add("/state/a", listsNested(maxDocumentNesting - 2)).status == affresco::RevisionStatus::Applied);
	CHECK(affresco::nestingDepth(versions.state()) == maxDocumentNesting - 1);

	// Each message is shallow, but this one would put the value one level deeper than the limit
	const affresco::RevisionResult deeper = add(innermost("/state/a", maxDocumentNesting - 2), json::array());
	CHECK(deeper.status == affresco::RevisionStatus::Rejected);
	CHECK(deeper.error == "AffrescoUI screen: revision: the screen would nest 201 levels deep (the live state one "
	                      "level inside it), more than 200");
	CHECK(affresco::nestingDepth(versions.state()) == maxDocumentNesting - 1);

	// The same holds for the document: two shallow messages that build too deep a node
	CHECK(add("/root/children/0/note", listsNested(150)).status == affresco::RevisionStatus::Applied);
	CHECK(add(innermost("/root/children/0/note", 150), listsNested(60)).status == affresco::RevisionStatus::Rejected);
	CHECK(versions.head().number == 2);
}

TEST_CASE("format: a step that would make the state deeper than maxDocumentNesting is stopped") {
	affresco::Screen screen = affresco::Screen::fromString(wrappingScreen);
	affresco::test::MemoryChannel peer;
	screen.connect(peer, "watcher");
	affresco::Panel sender;

	// state.box gets one level deeper each time: 199 wraps, inside the state's own level, reach the limit
	affresco::setLogSink([](affresco::LogLevel, std::string_view) {});
	for(int i = 0; i < maxDocumentNesting + 20; ++i) screen.runAction("wrap", sender);
	CHECK(affresco::nestingDepth(json::parse(screen.stateJson())) == maxDocumentNesting);
	const auto errors = std::ranges::count_if(peer.sent, [](const json& m) { return m.value("type", "") == "error"; });
	CHECK(errors == 21);
	CHECK(peer.sent.back()["message"] ==
	      "AffrescoUI screen: actions.wrap[0].to: would nest the state more than 200 levels deep");

	// An item pushed onto a list is one level deeper than the list
	screen.runAction("pushBox", sender);
	CHECK(json::parse(screen.stateJson())["list"].empty());

	// A place deeper than the limit is refused before anything is built there
	std::string place = "state";
	for(int i = 0; i <= maxDocumentNesting; ++i) place += ".a";
	json deepScreen = json::parse(wrappingScreen);
	deepScreen["actions"]["setDeep"] = json::array({{{"set", place}, {"to", 1}}});
	affresco::Screen deep = affresco::Screen::fromString(deepScreen.dump());
	affresco::test::MemoryChannel deepPeer;
	deep.connect(deepPeer, "watcher");
	deep.runAction("setDeep", sender);
	CHECK_FALSE(json::parse(deep.stateJson()).contains("a"));
	CHECK(deepPeer.sent.back()["message"] ==
	      "AffrescoUI screen: actions.setDeep[0].set: a place in the state more than 200 levels deep");
	affresco::setLogSink(nullptr);
}
