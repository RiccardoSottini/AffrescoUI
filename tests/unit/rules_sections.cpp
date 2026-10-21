/// @file
/// @brief Unit tests for format::Rules on the sections that grant capabilities (permissions, channels, persist, and
/// include, which exists only while a file loads): what validate accepts and refuses in each, and that checkChange
/// refuses every revision that changes one, as tables.

#include <affresco/format/Rules.hpp>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using affresco::json;

namespace {

/// @brief A valid screen that uses every capability section.
/// @return The document.
json screenWithCapabilities() {
	return json::parse(R"json({
		"format": "affrescoui-screen",
		"version": 1,
		"state": { "habits": [], "theme": "dark" },
		"channels": { "host": { "socket": "host.sock", "reconnect": 1000, "note": "the app's backend" } },
		"persist": ["state.habits"],
		"permissions": { "allow": ["studio", "agent(#board)"], "ask": ["agent(/actions)"], "deny": ["*(#footer)"],
		                 "reviewers": ["studio"] },
		"actions": { "ping": [{ "send": "host", "data": { "type": "ping" } }] },
		"root": { "type": "panel", "id": "main", "children": [
			{ "type": "panel", "id": "board" },
			{ "type": "panel", "id": "footer" }
		] }
	})json");
}

/// @brief A change to a screen, and what validate or checkChange says about it.
struct Case {
	const char* what;       ///< What the change does.
	const char* pointer;    ///< Where it sets a value ("" for the whole document).
	json value;             ///< The value set there; null removes what is there.
	std::string error;      ///< Text the error holds; "" if it is accepted.
};

/// @brief The screen with one value set or removed.
/// @param change The change.
/// @return The document after it.
json changed(const Case& change) {
	json document = screenWithCapabilities();
	const json::json_pointer pointer(change.pointer);
	if(change.value.is_null()) document[pointer.parent_pointer()].erase(pointer.back());
	else document[pointer] = change.value;
	return document;
}

/// @brief What a call says: "" if it returns, else its DocumentError's text.
/// @param call The call.
/// @return The error text.
template <typename Call> std::string errorOf(Call call) {
	try {
		call();
		return "";
	} catch(const affresco::DocumentError& error) {
		return error.what();
	}
}

} // namespace

TEST_CASE("format: validate accepts the capability sections when well formed, and names the place when not") {
	const affresco::format::Rules rules;
	const std::vector<Case> cases = {
	    {"the screen as it is", "/note", "unchanged", ""},
	    // channels
	    {"channels: not an object", "/channels", json::array(), "channels: expected an object"},
	    {"a channel: not an object", "/channels/host", "host.sock", "channels.host: expected an object"},
	    {"a channel without a socket", "/channels/host/socket", nullptr, "channels.host: a channel needs a \"socket\""},
	    {"a channel with an unknown property", "/channels/host/command", "rm -rf /",
	     "channels.host.command: unknown property (expected socket or reconnect)"},
	    {"a second channel", "/channels/log", {{"socket", "/tmp/log.sock"}}, ""},
	    // persist
	    {"persist: not a list", "/persist", "state.habits", "persist: expected a list of state paths"},
	    {"persist: not a text", "/persist/0", 3, "persist[0]: expected a string"},
	    {"persist: a path outside state", "/persist/0", "document.root", "persist[0]: expected a path under state"},
	    {"persist: another state path", "/persist/1", "state.theme", ""},
	    // permissions
	    {"permissions: not an object", "/permissions", json::array(), "permissions: expected an object"},
	    {"permissions: an unknown key", "/permissions/grant", json::array({"agent"}),
	     "permissions.grant: unknown property (expected allow, ask, deny or reviewers)"},
	    {"permissions: rules not in a list", "/permissions/allow", "studio", "permissions.allow: expected a list of rules"},
	    {"permissions: reviewers not in a list", "/permissions/reviewers", "studio",
	     "permissions.reviewers: expected a list of author names"},
	    {"permissions: a rule that isn't text", "/permissions/deny/0", 1, "permissions.deny[0]: expected a string"},
	    {"permissions: a malformed rule", "/permissions/ask/0", "agent(board)",
	     "permissions.ask[0]: expected \"author\" or \"author(pattern)\""},
	    {"permissions: a rule with a space", "/permissions/allow/0", "my agent", "permissions.allow[0]: expected"},
	    {"permissions: a rule with \"/**\"", "/permissions/allow/1", "agent(/state/**)", ""},
	    // include exists only while a file loads: a document never holds it
	    {"include", "/include", json::array({"parts.json"}), "include: unknown section"},
	    {"a section the format doesn't know", "/gadgets", json::object(), "gadgets: unknown section"},
	};
	for(const Case& change : cases) {
		CAPTURE(change.what);
		const std::string error = errorOf([&] { rules.validate(changed(change)); });
		if(change.error.empty()) CHECK(error == "");
		else CHECK(error.find(change.error) != std::string::npos);
	}
}

TEST_CASE("format: checkChange refuses every revision that changes a capability section") {
	const affresco::format::Rules rules;
	const std::string denied = "permission denied: revisions can't change \"";
	const std::vector<Case> cases = {
	    {"a change elsewhere", "/root/children/0/color", "#ff0000", ""},
	    {"a change to the live state", "/state/theme", "light", ""},
	    // permissions
	    {"a rule added", "/permissions/allow/2", "agent", denied + "permissions\""},
	    {"a rule removed", "/permissions/deny", json::array(), denied + "permissions\""},
	    {"a reviewer added", "/permissions/reviewers/1", "agent", denied + "permissions\""},
	    {"the rules reordered", "/permissions/allow", json::array({"agent(#board)", "studio"}),
	     denied + "permissions\""},
	    {"the section removed", "/permissions", nullptr, denied + "permissions\""},
	    // channels
	    {"a channel added", "/channels/shell", {{"socket", "/tmp/shell.sock"}}, denied + "channels\""},
	    {"a channel's socket changed", "/channels/host/socket", "other.sock", denied + "channels\""},
	    {"a channel's reconnect changed", "/channels/host/reconnect", 1, denied + "channels\""},
	    {"a channel's note changed", "/channels/host/note", "edited", denied + "channels\""},
	    {"the section removed", "/channels", nullptr, denied + "channels\""},
	    // persist
	    {"a path added", "/persist/1", "state.theme", denied + "persist\""},
	    {"the paths replaced", "/persist", json::array({"state.theme"}), denied + "persist\""},
	    {"the section removed", "/persist", nullptr, denied + "persist\""},
	    // steps that reach the outside world or control changes, wherever a revision puts them
	    {"a send in a new action", "/actions/leak", json::array({{{"send", "host"}, {"data", "{{state}}"}}}),
	     "permission denied: revisions can't add, change or move"},
	    {"a connect on load", "/onLoad", json::array({{{"connect", "host"}}}),
	     "permission denied: revisions can't add, change or move"},
	    {"a call in a watch", "/watch", {{"state.theme", json::array({{{"call", "save"}}})}},
	     "permission denied: revisions can't add, change or move"},
	    {"an existing send changed", "/actions/ping/0/data", {{"type", "shutdown"}},
	     "permission denied: revisions can't add, change or move"},
	};
	for(const Case& change : cases) {
		CAPTURE(change.what);
		const std::string error = errorOf([&] {
			// Both as rules see them: the live state at "state"
			rules.checkChange(screenWithCapabilities(), changed(change));
		});
		if(change.error.empty()) CHECK(error == "");
		else CHECK(error.find(change.error) != std::string::npos);
	}
}
