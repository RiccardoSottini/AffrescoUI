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
constexpr std::string_view controlSteps[] = {"accept",  "reject", "preview", "endPreview",
                                             "restore", "undo",   "patch",   "call"};

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
			if(key == "authors") {
				for(const auto& [author, policy] : expect::object(value, at).items()) {
					const std::string name = expect::string(policy, at + "." + author);
					if(name != "allow" && name != "review" && name != "deny")
						fail(at + "." + author, "expected \"allow\", \"review\" or \"deny\"");
				}
			} else if(key == "locked") {
				if(!value.is_array()) fail(at, "expected a list of \"#id\" or JSON Pointer paths");
				for(const json& path : value) expect::string(path, at);
			} else if(key == "reviewers") {
				if(!value.is_array()) fail(at, "expected a list of author names");
				for(const json& name : value) expect::string(name, at);
			} else {
				fail(at, "unknown property (expected authors, reviewers or locked)");
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

// From "permissions.authors": the exact name, else the channel's identity ("mcp" for "mcp/Claude"), else "*"
Policy Rules::policyFor(const json& doc, std::string_view author) const {
	auto policy = [&]() -> std::string {
		if(!doc.contains("permissions") || !doc["permissions"].contains("authors")) return "allow";
		const json& authors = doc["permissions"]["authors"];
		const std::string name(author);
		if(authors.contains(name)) return authors[name].get<std::string>();
		if(const auto slash = name.find('/'); slash != std::string::npos && authors.contains(name.substr(0, slash)))
			return authors[name.substr(0, slash)].get<std::string>();
		return authors.value("*", "allow");
	}();
	if(policy == "deny") return Policy::Deny;
	if(policy == "review") return Policy::Review;
	return Policy::Allow;
}

// Without a "permissions" section anyone may; otherwise the authors in "permissions.reviewers" (exact name or channel
// name: "studio" covers "studio/riccardo")
bool Rules::mayReview(const json& doc, std::string_view author) const {
	if(!doc.contains("permissions")) return true;
	const json reviewers = doc["permissions"].value("reviewers", json::array());
	const std::string name(author);
	const std::string channel = name.substr(0, name.find('/'));
	return std::ranges::any_of(reviewers, [&](const json& r) { return r == name || r == channel; });
}

// Listed in "permissions.locked", or nodes with "locked": true
std::vector<std::string> Rules::lockedPaths(const json& doc) const {
	std::vector<std::string> locked;
	if(doc.contains("permissions") && doc["permissions"].contains("locked"))
		for(const json& path : doc["permissions"]["locked"]) locked.push_back(path.get<std::string>());

	std::function<void(const json&)> collect = [&](const json& node) {
		if(!node.is_object()) return;
		if(node.value("locked", false) && node.contains("id")) locked.push_back("#" + node["id"].get<std::string>());
		if(node.contains("children") && node["children"].is_array())
			for(const json& child : node["children"]) collect(child);
	};
	if(doc.contains("root")) collect(doc["root"]);
	return locked;
}

void Rules::checkChange(const json& before, const json& after) const {
	if(before.value("permissions", json()) != after.value("permissions", json()))
		fail("revision", "permission denied: revisions can't change \"permissions\"");

	if(controlsIn(before) != controlsIn(after))
		fail("revision", "permission denied: revisions can't add or change accept, reject, preview, endPreview, "
		                 "restore, undo, patch or call steps");
}

} // namespace sgl::format
