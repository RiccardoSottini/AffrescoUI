#include "affresco/format/Rules.hpp"

#include "affresco/format/NodeIds.hpp"

#include "Checker.hpp"
#include "SectionRegistry.hpp"
#include "ServiceRules.hpp"
#include "Limits.hpp"
#include "StateTypes.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <functional>
#include <set>

namespace affresco::format {

using expect::fail;

namespace {

/// Steps that control changes: a revision may never add or alter them (it could otherwise approve itself).
constexpr std::string_view controlSteps[] = {"accept", "reject", "preview", "endPreview", "restore", "revert",
                                             "undo",   "patch",  "call",    "send",       "connect",
                                             "respond"};

/// The most findings a rejected revision lists; it says how many more there are.
constexpr std::size_t maxListed = 10;

/// The most uses of the document's own components one use is expanded through to be scanned; past them, it may run a
/// control step.
constexpr std::size_t maxExpansions = 64;

/// @brief Whether a key's string value names an action to run: an event handler ("onClick"), "do", "onLoad", "run".
/// @param key The key.
/// @return true if it does.
bool namesAction(std::string_view key) {
	const bool handler = key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
	return handler || key == "do" || key == "run";
}

/// The built-in components that run the actions a parameter names, through a computed "run" the document never
/// shows, each with that parameter: the "run" of a menu's items, a dialog's buttons, a toast's actions. The quick
/// pick has none ("") as it runs its recent items, kept in the state, as well as its providers' items.
constexpr std::array<std::pair<std::string_view, std::string_view>, 7> runningBuiltIns{{
    {"menu", "items"},     {"submenu", "items"}, {"contextMenu", "items"}, {"menuBar", "menus"},
    {"dialog", "buttons"}, {"toast", "actions"}, {"quickPick", ""}}};

/// @brief The names of the built-in components that run the actions a parameter names.
/// @return Them.
const std::set<std::string>& runningBuiltInNames() {
	static const std::set<std::string> names = [] {
		std::set<std::string> made;
		for(const auto& [name, parameter] : runningBuiltIns) made.emplace(name);
		return made;
	}();
	return names;
}

/// @brief Whether a value computes anything: a string with "{{" in it, at any depth.
/// @param value The value.
/// @return true if it does.
bool computes(const json& value) {
	if(value.is_string()) return value.get_ref<const std::string&>().contains("{{");
	if(value.is_structured()) return std::ranges::any_of(value, computes);
	return false;
}

/// @brief Puts a component's parameters in place of the strings that only name one ("{{label}}"), as using it does.
/// @param value The component's node, or a value in it.
/// @param parameters The parameters: the component's defaults, then the use's "with".
void substitute(json& value, const json& parameters) {
	if(value.is_structured()) {
		for(json& item : value) substitute(item, parameters);
		return;
	}
	if(!value.is_string()) return;
	std::string_view name = value.get_ref<const std::string&>();
	if(name.size() < 4 || !name.starts_with("{{") || !name.ends_with("}}")) return;
	name = name.substr(2, name.size() - 4);
	name.remove_prefix(std::min(name.find_first_not_of(" \t"), name.size()));
	name.remove_suffix(name.size() - std::min(name.find_last_not_of(" \t") + 1, name.size()));
	if(const auto it = parameters.find(std::string(name)); it != parameters.end()) value = *it;
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
	/// @param runners The components that run the actions their parameters name.
	/// @param components The document's own "components" (an object, or null).
	ControlScan(const json& actions, const std::set<std::string>& control, const std::set<std::string>& runners,
	            const json& components)
	    : actions_(actions), control_(control), runners_(runners), components_(components) {}

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
			bool control = false; // one look at each key, not one search of them per control step
			bool uses = false;
			for(const auto& entry : value.items()) {
				control = control || std::ranges::find(controlSteps, entry.key()) != std::ranges::end(controlSteps);
				uses = uses || entry.key() == "use";
			}
			if(control) found.push_back(trigger + ": " + value.dump());
			if(uses && runsControl(value))
				found.push_back(trigger + ": use " + value["use"].dump() + " with parameters that may control");
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
	/// @brief Whether a node uses a component that runs the actions its parameters name, with parameters that may
	/// name an action that controls: for a built-in, computed ones (from the state, say) in the parameter that names
	/// them, or any for the quick pick; for the document's own component, which wins over a built-in of its name, see
	/// expandsToControl. Literal parameters are also scanned as the node's own members.
	/// @param node The node.
	/// @return true if it does.
	[[nodiscard]] bool runsControl(const json& node) const {
		const auto use = node.find("use");
		if(use == node.end() || !use->is_string()) return false;
		const auto& used = use->get_ref<const std::string&>();
		if(!runners_.contains(used)) return false;
		const auto with = node.find("with");
		const json& parameters = with != node.end() ? *with : json();
		if(const auto own = components_.find(used); own != components_.end()) {
			if(depth_ == 0) expansions_ = maxExpansions; // each use outside a component has its own budget
			++depth_;
			const bool controls = expandsToControl(*own, parameters);
			--depth_;
			return controls;
		}
		for(const auto& [name, parameter] : runningBuiltIns) {
			if(*use != name) continue;
			if(parameter.empty()) return true;
			const auto value = parameters.is_object() ? parameters.find(parameter) : parameters.end();
			return parameters.is_object() && value != parameters.end() && computes(*value);
		}
		return computes(parameters);
	}

	/// @brief Whether a use of the document's own component may run a control step: its parameters compute anything,
	/// or the component's node, with its defaults and the use's literal parameters in place of the strings that name
	/// them, holds a way to run one (a parameter naming an action that controls, say). Past maxExpansions nested
	/// uses, it may.
	/// @param component The component.
	/// @param with The use's "with".
	/// @return true if it may.
	[[nodiscard]] bool expandsToControl(const json& component, const json& with) const {
		if(computes(with) || !component.is_object() || expansions_ == 0) return true;
		--expansions_;
		const auto node = component.find("node");
		if(node == component.end()) return false;
		json parameters = json::object();
		if(const auto defaults = component.find("params"); defaults != component.end() && defaults->is_object())
			parameters = *defaults;
		if(with.is_object()) parameters.update(with);
		json value = *node;
		substitute(value, parameters);
		std::vector<std::string> found;
		walk(value, "use", Part::Steps, found);
		return !found.empty();
	}

	const json& actions_;                  ///< The document's actions.
	const std::set<std::string>& control_; ///< Those that may run control steps.
	const std::set<std::string>& runners_; ///< The components that run the actions their parameters name.
	const json& components_;               ///< The document's own components.
	mutable std::size_t depth_ = 0;        ///< How many uses of the document's components are being expanded.
	mutable std::size_t expansions_ = 0;   ///< How many more the use being scanned may be expanded through.
};

/// @brief The components that run the actions their parameters name: the built-in ones, and the document's own that
/// hold a way to run a control step (a computed "run", say) or use such a component with computed parameters.
/// @param doc The document.
/// @param control The document's actions that may run control steps.
/// @return Their names.
std::set<std::string> runningComponents(const json& doc, const std::set<std::string>& control) {
	std::set<std::string> runners = runningBuiltInNames();
	const auto components = doc.find("components");
	if(components == doc.end() || !components->is_object()) return runners;
	static const json none;
	const auto actions = doc.find("actions");
	for(bool grew = true; grew;) { // until no component joins: a chain of uses takes one round per link
		grew = false;
		const ControlScan scan(actions != doc.end() ? *actions : none, control, runners, *components);
		for(const auto& [name, component] : components->items()) {
			if(runners.contains(name)) continue;
			std::vector<std::string> found;
			scan.walk(component, name, Part::Steps, found);
			if(found.empty()) continue;
			runners.insert(name);
			grew = true;
		}
	}
	return runners;
}

/// @brief The document's actions that may run control steps: those with one, and those that run one of them, a
/// native action or a computed one.
/// @param doc The document.
/// @return Their names.
std::set<std::string> controlActions(const json& doc) {
	std::set<std::string> control;
	if(!doc.contains("actions") || !doc["actions"].is_object()) return control;
	const json& actions = doc["actions"];
	static const json none; // actions use no component
	for(bool grew = true; grew;) { // until no action joins: a chain of runs takes one round per link
		grew = false;
		const ControlScan scan(actions, control, runningBuiltInNames(), none);
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
	const std::set<std::string> runners = runningComponents(doc, control);
	const ControlScan scan(doc.contains("actions") ? doc["actions"] : none, control, runners,
	                       doc.contains("components") ? doc["components"] : none);
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

/// @brief Says which type a mismatch breaks, for its message.
/// @param mismatch The mismatch, at "state.<key>..." .
/// @return " (stateTypes.<key>)".
std::string typeNote(const Mismatch& mismatch) {
	const std::string_view place = std::string_view(mismatch.place).substr(std::string_view("state.").size());
	return " (stateTypes." + std::string(place.substr(0, place.find_first_of(".["))) + ")";
}

/// @brief Refuses a document larger than maxDocumentBytes without its "state", or whose "state" is larger than
/// maxStateBytes.
/// @param document The document.
/// @throws DocumentError If it is.
void checkDocumentSize(const json& document) {
	std::size_t bytes = 0;
	for(const auto& [key, value] : document.items())
		if(key != "state") bytes += key.size() + approximateBytes(value, maxDocumentBytes);
	if(bytes > maxDocumentBytes)
		fail("", "the screen is larger than " + std::to_string(maxDocumentBytes >> 20) + " MiB of JSON");
	if(approximateBytes(stateOf(document), maxStateBytes) > maxStateBytes)
		fail("state", "larger than " + std::to_string(maxStateBytes >> 20) + " MiB of JSON");
}

/// @brief Refuses a revision after which the live state is larger than maxStateBytes, and larger than before.
/// @param before The document with the live state in "state", before the revision.
/// @param after The same, after it.
/// @throws DocumentError If it would be.
void checkStateSize(const json& before, const json& after) {
	const std::size_t now = approximateBytes(stateOf(after), maxStateBytes);
	if(now > maxStateBytes && now > approximateBytes(stateOf(before)))
		fail("revision", "the state would be larger than " + std::to_string(maxStateBytes >> 20) + " MiB of JSON");
}

/// @brief A state key's value and declared type before a revision.
/// @param before The document with the live state in "state", before the revision.
/// @param key The key.
/// @return Its value and its type; nullptr for one it didn't have.
std::pair<const json*, const json*> typedBefore(const json& before, const std::string& key) {
	const json& state = stateOf(before);
	const auto value = state.find(key);
	const auto types = before.find("stateTypes");
	const json* type = nullptr;
	if(types != before.end() && types->is_object())
		if(const auto it = types->find(key); it != types->end()) type = &*it;
	return {value == state.end() ? nullptr : &*value, type};
}

/// @brief Refuses a revision after which the live state matches its types less than before: a value that no longer
/// matches, or a type the state doesn't match. Mismatches that were there before don't block it.
/// @param before The document with the live state in "state", before the revision.
/// @param after The same, after it.
/// @throws DocumentError If it brings a mismatch.
void checkTypedState(const json& before, const json& after) {
	const auto types = after.find("stateTypes");
	if(types == after.end() || !types->is_object()) return;
	const json& state = stateOf(after);
	// Cost follows the change: a key whose value and type are as before matches as it did
	for(const auto& [key, type] : types->items()) {
		const auto value = state.find(key);
		if(value == state.end()) continue;
		const auto [oldValue, oldType] = typedBefore(before, key);
		if(oldValue && oldType && *oldType == type && *oldValue == *value) continue;
		const std::optional<Mismatch> now = typeMismatch(*value, type, "state." + key);
		if(now && !(oldValue && oldType && typeMismatch(*oldValue, *oldType, "state." + key)))
			fail("revision", "the state wouldn't match its types: " + now->text() + typeNote(*now));
	}
}

} // namespace

Rules::Rules() : sections_(builtInSections()) {}

void Rules::checkRevision(const json& patch) const {
	if(patch.is_array() && patch.size() > maxRevisionOperations)
		fail("revision.patch", std::to_string(patch.size()) + " operations, more than the " +
		                           std::to_string(maxRevisionOperations) + " a revision may have");
	if(approximateBytes(patch, maxRevisionBytes) > maxRevisionBytes)
		fail("revision.patch", "more than " + std::to_string(maxRevisionBytes >> 20) +
		                           " MiB of JSON, more than a revision may have");
}

// The document's own "state" must match its types; the live state is checked by checkChange() and findings()
void Rules::validate(const json& doc) const {
	sections_->validate(doc);
	checkDocumentSize(doc);
	const std::vector<Mismatch> mismatches = stateMismatches(doc, stateOf(doc));
	if(!mismatches.empty()) fail(mismatches.front().place, mismatches.front().problem + typeNote(mismatches.front()));
}

// In root, then screens, templates and components (NodeIds.hpp)
std::optional<std::string> Rules::findNode(const json& doc, std::string_view nodeId) const {
	return findNodeWithId(doc, nodeId);
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

	checkTypedState(before, after);
	checkStateSize(before, after);

	if(controlsIn(before) != controlsIn(after))
		fail("revision",
		     "permission denied: revisions can't add, change or move accept, reject, preview, endPreview, "
		     "restore, revert, undo, patch, call, send, connect or respond steps, nor run an action that has them, "
		     "a "
		     "native action or a computed one");

	checkServiceUses(before, after, services_);

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
	std::vector<std::string> lines;
	for(const Mismatch& mismatch : stateMismatches(document, state)) // a state kept on disk may predate its types
		lines.push_back(mismatch.text() + typeNote(mismatch));
	const auto found = check(document, state, nativeActions_);
	if(!found) {
		lines.emplace_back("not checked: the screen is larger than the checker's limit");
		return lines;
	}
	for(const Finding& finding : *found) lines.push_back(finding.text());
	return lines;
}

void Rules::addNativeAction(std::string name) { nativeActions_.insert(std::move(name)); }

std::vector<std::string> Rules::reviewLines(const json& before, const json& after) const {
	return serviceReviewLines(before, after, services_);
}

} // namespace affresco::format
