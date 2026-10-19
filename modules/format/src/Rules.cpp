#include "affresco/format/Rules.hpp"

#include "Checker.hpp"

#include <algorithm>
#include <functional>

namespace affresco::format {

using expect::fail;

namespace {

/// Format versions this library reads. A future version gets its own entry, and its differences branch on the
/// document's "version" where they apply.
constexpr int supportedVersions[] = {1};

/// Steps that control changes: a revision may never add or alter them (it could otherwise approve itself).
constexpr std::string_view controlSteps[] = {"accept", "reject", "preview", "endPreview", "restore", "revert",
                                             "undo",   "patch",  "call",    "send",       "connect"};

/// Sections that grant capabilities (talking to programs, writing to disk): revisions may never change them.
constexpr std::string_view capabilities[] = {"permissions", "channels", "persist"};

/// The most findings a rejected revision lists; it says how many more there are.
constexpr std::size_t maxListed = 10;

/// @brief Collects every control step in a document, to compare before and after a revision. The state is data,
/// never steps, so it is left out.
/// @param document The document.
/// @return The steps as JSON text, sorted.
std::vector<std::string> controlsIn(const json& document) {
	std::vector<std::string> found;
	std::function<void(const json&)> walk = [&](const json& v) {
		if(v.is_object()) {
			if(std::ranges::any_of(controlSteps, [&](std::string_view key) { return v.contains(std::string(key)); }))
				found.push_back(v.dump());
			for(const auto& [key, item] : v.items()) walk(item);
		} else if(v.is_array()) {
			for(const json& item : v) walk(item);
		}
	};
	for(const auto& [key, section] : document.items())
		if(key != "state") walk(section);
	std::ranges::sort(found);
	return found;
}

/// @brief The state a document is checked against.
/// @param document The document, with the live state as its "state".
/// @return Its state, or an empty object.
const json& stateOf(const json& document) {
	static const json none = json::object();
	const auto it = document.find("state");
	return it != document.end() ? *it : none;
}

} // namespace

void Rules::validate(const json& doc) const {
	expect::object(doc, "");

	static constexpr std::string_view sections[] = {
	    "$schema", "format",   "version", "note",    "window", "fonts",    "colors",   "theme",
	    "styles",  "components", "templates", "screens", "state", "computed", "watch",    "actions",
	    "keys",    "onLoad",   "channels", "persist", "permissions", "root"};
	for(const auto& [key, value] : doc.items()) {
		if(std::ranges::find(sections, key) == std::end(sections)) fail(key, "unknown section");
	}

	if(doc.contains("format") && expect::string(doc["format"], "format") != "affrescoui-screen")
		fail("format", "expected \"affrescoui-screen\"");
	if(!doc.contains("version")) fail("", "missing \"version\" (this library reads version 1)");
	const int v = expect::integer(doc["version"], "version");
	if(std::ranges::find(supportedVersions, v) == std::end(supportedVersions))
		fail("version", "unsupported version " + std::to_string(v) + " (this library reads version 1)");
	if(!doc.contains("root")) fail("", "missing \"root\"");
	for(const char* section : {"actions", "styles", "components", "templates", "screens", "computed", "watch", "keys",
	                           "channels"})
		if(doc.contains(section)) expect::object(doc[section], section);
	if(doc.contains("components"))
		for(const auto& [name, component] : doc["components"].items()) {
			expect::object(component, "components." + name);
			if(!component.contains("node")) fail("components." + name, "a component needs a \"node\"");
			for(const auto& [key, value] : component.items())
				if(key != "node" && key != "params" && key != "note")
					fail("components." + name + "." + key, "unknown property (expected node, params or note)");
		}
	if(doc.contains("channels"))
		for(const auto& [name, channel] : doc["channels"].items()) {
			const std::string at = "channels." + name;
			expect::object(channel, at);
			if(!channel.contains("socket")) fail(at, "a channel needs a \"socket\"");
			for(const auto& [key, value] : channel.items())
				if(key != "socket" && key != "reconnect" && key != "note")
					fail(at + "." + key, "unknown property (expected socket or reconnect)");
		}
	if(doc.contains("persist")) {
		const json& persist = doc["persist"];
		if(!persist.is_array()) fail("persist", "expected a list of state paths, e.g. [\"state.habits\"]");
		for(std::size_t i = 0; i < persist.size(); ++i)
			if(!expect::string(persist[i], "persist[" + std::to_string(i) + "]").starts_with("state."))
				fail("persist[" + std::to_string(i) + "]", "expected a path under state, e.g. \"state.habits\"");
	}

	if(doc.contains("permissions")) {
		for(const auto& [key, value] : expect::object(doc["permissions"], "permissions").items()) {
			const std::string at = "permissions." + key;
			if(key != "allow" && key != "ask" && key != "deny" && key != "reviewers")
				fail(at, "unknown property (expected allow, ask, deny or reviewers)");
			if(!value.is_array())
				fail(at, key == "reviewers" ? "expected a list of author names" : "expected a list of rules");
			for(std::size_t i = 0; i < value.size(); ++i) {
				const std::string item = expect::string(value[i], at + "[" + std::to_string(i) + "]");
				if(key != "reviewers" && !parsePermissionRule(item))
					fail(at + "[" + std::to_string(i) + "]",
					     "expected \"author\" or \"author(pattern)\", the pattern a JSON Pointer or \"#id\" path, e.g. "
					     "\"agent(#board)\"");
			}
		}
	}
}

// In root or templates
std::optional<std::string> Rules::findNode(const json& doc, std::string_view nodeId) const {
	const std::string id(nodeId);
	std::function<std::optional<std::string>(const json&, const std::string&)> search =
	    [&](const json& node, const std::string& pointer) -> std::optional<std::string> {
		if(!node.is_object()) return std::nullopt;
		if(node.contains("id") && node["id"] == id) return pointer;
		if(node.contains("children") && node["children"].is_array()) {
			for(std::size_t i = 0; i < node["children"].size(); ++i)
				if(auto found = search(node["children"][i], pointer + "/children/" + std::to_string(i))) return found;
		}
		return std::nullopt;
	};

	if(doc.contains("root"))
		if(auto found = search(doc["root"], "/root")) return found;
	for(const char* section : {"screens", "templates"}) {
		if(!doc.contains(section) || !doc[section].is_object()) continue;
		for(const auto& [name, node] : doc[section].items())
			if(auto found = search(node, "/" + std::string(section) + "/" + name)) return found;
	}
	if(doc.contains("components") && doc["components"].is_object()) {
		for(const auto& [name, component] : doc["components"].items())
			if(component.is_object() && component.contains("node"))
				if(auto found = search(component["node"], "/components/" + name + "/node")) return found;
	}
	return std::nullopt;
}

// "permissions" as rules, plus every node with "locked": true denied to everyone
Permissions Rules::permissions(const json& doc) const {
	Permissions result;
	if(doc.contains("permissions") && doc["permissions"].is_object()) {
		const json& section = doc["permissions"];
		result.configured = true;
		auto read = [&](const char* key, std::vector<PermissionRule>& into) {
			for(const json& text : section.value(key, json::array()))
				if(const auto rule = parsePermissionRule(text.get<std::string>())) into.push_back(*rule);
		};
		read("allow", result.allow);
		read("ask", result.ask);
		read("deny", result.deny);
		for(const json& name : section.value("reviewers", json::array()))
			result.reviewers.push_back(name.get<std::string>());
	}

	std::function<void(const json&)> collect = [&](const json& node) {
		if(!node.is_object()) return;
		if(node.contains("locked") && node["locked"] == true && node.contains("id") && node["id"].is_string())
			if(auto rule = parsePermissionRule("*(#" + node["id"].get<std::string>() + ")"))
				result.deny.push_back(std::move(*rule));
		if(node.contains("children") && node["children"].is_array())
			for(const json& child : node["children"]) collect(child);
	};
	if(doc.contains("root")) collect(doc["root"]);
	for(const char* section : {"screens", "templates"})
		if(doc.contains(section) && doc[section].is_object())
			for(const auto& [name, node] : doc[section].items()) collect(node);
	return result;
}

void Rules::checkChange(const json& before, const json& after) const {
	for(std::string_view section : capabilities) {
		const std::string key(section);
		if(before.value(key, json()) != after.value(key, json()))
			fail("revision", "permission denied: revisions can't change \"" + key + "\"");
	}

	if(controlsIn(before) != controlsIn(after))
		fail("revision", "permission denied: revisions can't add or change accept, reject, preview, endPreview, "
		                 "restore, revert, undo, patch, call, send or connect steps");

	// What a revision refers to must exist, so a typo is rejected before a reviewer sees it. Findings that were there
	// before don't block it: the revision didn't bring them. (Most screens have none: then one check is enough.)
	const auto now = check(after, stateOf(after), nativeActions_);
	if(!now || now->empty()) return; // nothing found, or too large to check
	const auto old = check(before, stateOf(before), nativeActions_);
	if(!old) return;
	const std::vector<Finding> added = introduced(*old, *now);
	if(added.empty()) return;
	std::string message = "the checker found:";
	for(std::size_t i = 0; i < added.size() && i < maxListed; ++i) message += (i ? "; " : " ") + added[i].text();
	if(added.size() > maxListed) message += "; and " + std::to_string(added.size() - maxListed) + " more";
	fail("revision", message);
}

std::vector<std::string> Rules::findings(const json& document, const json& state) const {
	const auto found = check(document, state, nativeActions_);
	if(!found) return {"not checked: the screen is larger than the checker's limit"};
	std::vector<std::string> lines;
	for(const Finding& finding : *found) lines.push_back(finding.text());
	return lines;
}

void Rules::addNativeAction(std::string name) { nativeActions_.insert(std::move(name)); }

} // namespace affresco::format
