#include "affresco/format/Rules.hpp"

#include "Checker.hpp"
#include "SectionRegistry.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <set>

namespace affresco::format {

using expect::fail;

namespace {

/// Steps that control changes: a revision may never add or alter them (it could otherwise approve itself).
constexpr std::string_view controlSteps[] = {"accept", "reject", "preview", "endPreview", "restore", "revert",
                                             "undo",   "patch",  "call",    "send",       "connect"};

/// The most findings a rejected revision lists; it says how many more there are.
constexpr std::size_t maxListed = 10;

/// @brief Whether a key's string value names an action to run: an event handler ("onClick"), "do", "onLoad", "run".
/// @param key The key.
/// @return true if it does.
bool namesAction(std::string_view key) {
	const bool handler = key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
	return handler || key == "do" || key == "run";
}

/// @brief Where a scan is in the document: what its strings mean.
enum class Part {
	Values,   ///< "state": values, never run.
	Steps,    ///< Nodes, steps and the other sections: strings under action keys name actions.
	Handlers, ///< A "keys" or "watch" map: every string value names an action.
};

/// @brief Finds what in a document can control changes or reach the outside world: control steps, and references to
/// actions that may run them.
///
/// Each finding is prefixed with the trigger it runs from ("onClick", "every", "keys", "onLoad", "watch", or the
/// action it belongs to), so moving an "accept" from a button into a timer is a change too.
class ControlScan {
public:
	/// @brief A scan of a document whose actions in @p control are known to run control steps.
	/// @param actions The document's "actions" (an object, or null).
	/// @param control Those of them that may run control steps.
	ControlScan(const json& actions, const std::set<std::string>& control) : actions_(actions), control_(control) {}

	/// @brief Whether running a target may run a control step: a computed one, an action the document doesn't have
	/// (a native action, which is the application's code), or one in the control set.
	/// @param target The action name, or what computes it.
	/// @return true if it may.
	[[nodiscard]] bool mayControl(const json& target) const {
		if(!target.is_string()) return true;
		const auto& name = target.get_ref<const std::string&>();
		return name.contains("{{") || !actions_.is_object() || !actions_.contains(name) || control_.contains(name);
	}

	/// @brief Scans a value.
	/// @param value The value.
	/// @param trigger What runs it.
	/// @param part What its strings mean.
	/// @param found Receives the findings.
	void walk(const json& value, const std::string& trigger, Part part, std::vector<std::string>& found) const {
		if(value.is_object()) {
			if(std::ranges::any_of(controlSteps,
			                       [&](std::string_view key) { return value.contains(std::string(key)); }))
				found.push_back(trigger + ": " + value.dump());
			for(const auto& [key, item] : value.items()) member(key, item, trigger, part, found);
		} else if(value.is_array()) {
			for(const json& item : value) walk(item, trigger, part == Part::Values ? Part::Values : Part::Steps, found);
		}
	}

	/// @brief Scans an object's member.
	/// @param key Its key.
	/// @param item Its value.
	/// @param trigger What runs the object.
	/// @param part What the object's strings mean.
	/// @param found Receives the findings.
	void member(const std::string& key, const json& item, const std::string& trigger, Part part,
	            std::vector<std::string>& found) const {
		if(part == Part::Values) return walk(item, trigger, part, found);
		const bool triggers = namesAction(key) && key != "do" && key != "run";
		const std::string inner = triggers || key == "every" || key == "keys" || key == "watch" ? key : trigger;
		const bool names = part == Part::Handlers || namesAction(key);
		if(names && (item.is_string() || key == "run") && mayControl(item))
			found.push_back(inner + ": run " + item.dump());
		walk(item, inner, key == "keys" || key == "watch" ? Part::Handlers : Part::Steps, found);
	}

private:
	const json& actions_;                  ///< The document's actions.
	const std::set<std::string>& control_; ///< Those that may run control steps.
};

/// @brief The document's actions that may run control steps: those with one, and those that run one of them, a
/// native action or a computed one.
/// @param doc The document.
/// @return Their names.
std::set<std::string> controlActions(const json& doc) {
	std::set<std::string> control;
	if(!doc.contains("actions") || !doc["actions"].is_object()) return control;
	const json& actions = doc["actions"];
	for(bool grew = true; grew;) { // until no action joins: a chain of runs takes one round per link
		grew = false;
		const ControlScan scan(actions, control);
		for(const auto& [name, steps] : actions.items()) {
			if(control.contains(name)) continue;
			std::vector<std::string> found;
			scan.walk(steps, name, Part::Steps, found);
			if(found.empty()) continue;
			control.insert(name);
			grew = true;
		}
	}
	return control;
}

/// @brief Collects what in a document can control changes or reach the outside world, to compare before and after a
/// revision: every control step, and every reference to an action that may run one, each with its trigger.
/// @param doc The document.
/// @return The findings as text, sorted.
std::vector<std::string> controlsIn(const json& doc) {
	std::vector<std::string> found;
	if(!doc.is_object()) return found;
	static const json none; // both branches are references: the scan keeps one
	const std::set<std::string> control = controlActions(doc);
	const ControlScan scan(doc.contains("actions") ? doc["actions"] : none, control);
	for(const auto& [key, value] : doc.items()) {
		if(key == "actions" && value.is_object()) {
			for(const auto& [name, steps] : value.items()) scan.walk(steps, "actions." + name, Part::Steps, found);
		} else {
			scan.member(key, value, key, key == "state" ? Part::Values : Part::Steps, found);
		}
	}
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

Rules::Rules() : sections_(builtInSections()) {}

void Rules::validate(const json& doc) const { sections_->validate(doc); }

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
	for(const SectionType& section : sections_->sections()) {
		if(!section.capability) continue;
		if(before.value(section.name, json()) != after.value(section.name, json()))
			fail("revision", "permission denied: revisions can't change \"" + section.name + "\"");
	}

	if(controlsIn(before) != controlsIn(after))
		fail("revision",
		     "permission denied: revisions can't add, change or move accept, reject, preview, endPreview, "
		     "restore, revert, undo, patch, call, send or connect steps, nor run an action that has them, a "
		     "native action or a computed one");

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
