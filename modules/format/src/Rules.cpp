#include "sgl/format/Rules.hpp"

#include <algorithm>
#include <functional>

namespace sgl::format {

using expect::fail;

namespace {

/// Format versions this library reads. A future version gets its own entry, and its differences branch on the
/// document's "version" where they apply.
constexpr int supportedVersions[] = {1};

/// Steps that control changes: a revision may never add or alter them (it could otherwise approve itself).
constexpr std::string_view controlSteps[] = {"accept", "reject", "preview", "endPreview", "restore",
                                             "revert", "undo",   "patch",   "call"};

/// @brief Collects every control step in a document, to compare before and after a revision.
/// @param value The document, or part of it.
/// @return The steps as JSON text, sorted.
std::vector<std::string> controlsIn(const json& value) {
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
	walk(value);
	std::ranges::sort(found);
	return found;
}

} // namespace

void Rules::validate(const json& doc) const {
	expect::object(doc, "");

	static constexpr std::string_view sections[] = {"$schema", "format", "version", "window",
	                                                "fonts",   "colors", "styles",  "templates",
	                                                "root",    "state",  "actions", "permissions"};
	for(const auto& [key, value] : doc.items()) {
		if(std::ranges::find(sections, key) == std::end(sections)) fail(key, "unknown section");
	}

	if(doc.contains("format") && expect::string(doc["format"], "format") != "sglgui-screen")
		fail("format", "expected \"sglgui-screen\"");
	if(!doc.contains("version")) fail("", "missing \"version\" (this library reads version 1)");
	const int v = expect::integer(doc["version"], "version");
	if(std::ranges::find(supportedVersions, v) == std::end(supportedVersions))
		fail("version", "unsupported version " + std::to_string(v) + " (this library reads version 1)");
	if(!doc.contains("root")) fail("", "missing \"root\"");
	if(doc.contains("actions")) expect::object(doc["actions"], "actions");

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
	if(doc.contains("templates")) {
		for(const auto& [name, node] : doc["templates"].items())
			if(auto found = search(node, "/templates/" + name)) return found;
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
	return result;
}

void Rules::checkChange(const json& before, const json& after) const {
	if(before.value("permissions", json()) != after.value("permissions", json()))
		fail("revision", "permission denied: revisions can't change \"permissions\"");

	if(controlsIn(before) != controlsIn(after))
		fail("revision", "permission denied: revisions can't add or change accept, reject, preview, endPreview, "
		                 "restore, revert, undo, patch or call steps");
}

} // namespace sgl::format
