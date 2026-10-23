/// @file
/// @brief The control scan (RulesControl.hpp): what in a document can control changes or reach the outside world.

#include "RulesControl.hpp"

#include "CheckerShapes.hpp"
#include "Limits.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <string_view>
#include <utility>

namespace affresco::format {

namespace {

/// Steps that control changes: a revision may never add or alter them (it could otherwise approve itself).
constexpr std::array<std::string_view, 14> controlSteps{"accept",  "reject",  "preview", "endPreview", "restore",
                                                        "revert",  "undo",    "patch",   "call",       "send",
                                                        "connect", "respond", "signIn",  "signOut"};

/// The most uses of the document's own components one use is expanded through to be scanned; past them, it may run a
/// control step.
constexpr std::size_t maxExpansions = 64;

/// The most bytes one scan expands the document's components to, over all their uses: as many as the largest document
/// has. Past them, a use may run a control step. Each use copies its component, and a few MiB used a few thousand times
/// would be GiBs.
constexpr std::size_t maxExpandedBytes = maxDocumentBytes;

/// @brief Whether a key's string value names an action to run: an event handler ("onClick"), "do", "onLoad", "run".
/// @param key The key.
/// @return true if it does.
bool namesAction(std::string_view key) {
	const bool handler = key.size() > 2 && key.starts_with("on") && std::isupper(static_cast<unsigned char>(key[2]));
	return handler || key == "do" || key == "run";
}

/// The built-in components that run the actions a parameter names, through a computed "run" the document never
/// shows, each with that parameter: the "run" of a menu's items, a dialog's buttons, a toast's actions, a toolbar's
/// and a status bar's items. The quick pick has none ("") as it runs its recent items, kept in the state, as well as
/// its providers' items, and so do the toasts, whose actions are kept in the state.
constexpr std::array<std::pair<std::string_view, std::string_view>, 10> runningBuiltIns{{{"menu", "items"},
                                                                                         {"submenu", "items"},
                                                                                         {"contextMenu", "items"},
                                                                                         {"menuBar", "menus"},
                                                                                         {"dialog", "buttons"},
                                                                                         {"toast", "actions"},
                                                                                         {"toolbar", "items"},
                                                                                         {"statusBar", "items"},
                                                                                         {"quickPick", ""},
                                                                                         {"toasts", ""}}};

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
/// @param room How many more bytes the parameters put in may take; lowered by theirs.
/// @return false if they would take more (the value is then only partly substituted).
bool substitute(json& value, const json& parameters, std::size_t& room) {
	if(value.is_structured()) {
		for(json& item : value)
			if(!substitute(item, parameters, room)) return false;
		return true;
	}
	if(!value.is_string()) return true;
	std::string_view name = value.get_ref<const std::string&>();
	if(name.size() < 4 || !name.starts_with("{{") || !name.ends_with("}}")) return true;
	name = name.substr(2, name.size() - 4);
	name.remove_prefix(std::min(name.find_first_not_of(" \t"), name.size()));
	name.remove_suffix(name.size() - std::min(name.find_last_not_of(" \t") + 1, name.size()));
	const auto it = parameters.find(std::string(name));
	if(it == parameters.end()) return true;
	const std::size_t size = approximateBytes(*it, room);
	if(size > room) return false;
	room -= size;
	value = *it;
	return true;
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
	    : actions_(actions), control_(control), runners_(runners), actionsAt_(actions), componentsAt_(components) {}

	/// @brief An action of the document.
	/// @param name Its name.
	/// @return Its steps, or nullptr.
	[[nodiscard]] const json* action(std::string_view name) const { return actionsAt_.find(name); }

	/// @brief A component of the document.
	/// @param name Its name.
	/// @return It, or nullptr.
	[[nodiscard]] const json* component(std::string_view name) const { return componentsAt_.find(name); }

	/// @brief Whether running a target may run a control step: a computed one, an action the document doesn't have
	/// (a native action, which is the application's code), or one in the control set.
	/// @param target The action name, or what computes it.
	/// @return true if it may.
	[[nodiscard]] bool mayControl(const json& target) const {
		if(!target.is_string()) return true;
		const auto& name = target.get_ref<const std::string&>();
		if(name.contains("{{") || !actions_.is_object() || !action(name) || control_.contains(name)) return true;
		if(askedActions_) askedActions_->push_back(name);
		return false;
	}

	/// @brief Has the scan note the names it found not to run control steps, whose joining would change what it finds:
	/// the actions it runs, and the document's components it uses.
	/// @param actions Gets those actions; nullptr for none.
	/// @param components Gets those components; nullptr for none.
	void note(std::vector<std::string>* actions, std::vector<std::string>* components) const {
		askedActions_ = actions;
		askedComponents_ = components;
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
			// One text per outermost control step: the ones inside it are in its text, so theirs would only repeat
			// it, once per level (a document of a few MiB, nested, would make GiBs)
			if(control && !dumped_) found.push_back(trigger + ": " + value.dump());
			if(uses && runsControl(value))
				found.push_back(trigger + ": use " + value["use"].dump() + " with parameters that may control");
			const bool outer = std::exchange(dumped_, dumped_ || control);
			for(const auto& [key, item] : value.items()) member(key, item, trigger, part, found);
			dumped_ = outer;
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
		if(!runners_.contains(used)) {
			if(askedComponents_ && component(used)) askedComponents_->push_back(used);
			return false;
		}
		const auto with = node.find("with");
		const json& parameters = with != node.end() ? *with : json();
		if(const json* own = component(used)) {
			if(depth_ == 0) expansions_ = maxExpansions; // each use outside a component has its own budget
			++depth_;
			const bool controls = expandsToControl(*own, parameters);
			--depth_;
			return controls;
		}
		for(const auto& [name, parameter] : runningBuiltIns) {
			if(used != name) continue;
			if(parameter.empty()) return true;
			const auto value = parameters.is_object() ? parameters.find(parameter) : parameters.end();
			return parameters.is_object() && value != parameters.end() && computes(*value);
		}
		return computes(parameters);
	}

	/// @brief Whether a use of the document's own component may run a control step: its parameters compute anything,
	/// or the component's node, with its defaults and the use's literal parameters in place of the strings that name
	/// them, holds a way to run one (a parameter naming an action that controls, say). Past maxExpansions nested
	/// uses, or once the scan has expanded maxExpandedBytes, it may.
	/// @param component The component.
	/// @param with The use's "with".
	/// @return true if it may.
	[[nodiscard]] bool expandsToControl(const json& component, const json& with) const {
		if(computes(with) || !component.is_object() || expansions_ == 0 || room_ == 0) return true;
		--expansions_;
		const auto node = component.find("node");
		if(node == component.end()) return false;
		json parameters = json::object();
		if(const auto defaults = component.find("params"); defaults != component.end() && defaults->is_object())
			parameters = *defaults;
		if(with.is_object()) parameters.update(with);
		// Once spent, the budget stays spent (0), so the uses after it aren't measured again
		const std::size_t size = approximateBytes(*node, room_);
		if(size > room_) {
			room_ = 0;
			return true;
		}
		room_ -= size;
		json value = *node;
		if(!substitute(value, parameters, room_)) {
			room_ = 0;
			return true;
		}
		std::vector<std::string> found;
		const bool outer = std::exchange(dumped_, false); // the expansion's own control steps count
		walk(value, "use", Part::Steps, found);
		dumped_ = outer;
		return !found.empty();
	}

	const json& actions_;                         ///< The document's actions.
	const std::set<std::string>& control_;        ///< Those that may run control steps.
	const std::set<std::string>& runners_;        ///< The components that run the actions their parameters name.
	const checker::Members actionsAt_;            ///< The document's actions by name.
	const checker::Members componentsAt_;         ///< The document's own components by name.
	mutable std::size_t depth_ = 0;               ///< How many uses of the document's components are being expanded.
	mutable std::size_t expansions_ = 0;          ///< How many more the use being scanned may be expanded through.
	mutable std::size_t room_ = maxExpandedBytes; ///< How many more bytes the scan may expand components to.
	mutable bool dumped_ = false;                 ///< Whether the value being scanned is inside a control step's text.
	mutable std::vector<std::string>* askedActions_ = nullptr;    ///< Gets the actions found not to control.
	mutable std::vector<std::string>* askedComponents_ = nullptr; ///< Gets the components found not to run actions.
};

/// @brief Grows a set to the names that join it: those that do on their own, and those that do once others have.
/// Each is scanned once, then again only when a name its scan depended on joins, so a chain of N costs N scans, not
/// N rounds of N (it took seconds for a few thousand actions).
/// @param candidates The names that may join.
/// @param members The set, which the scans read: it gets the names that join.
/// @param joins Scans a name against the set: whether it joins, noting in its second parameter the names not in the
/// set it depended on.
template <typename Joins>
void grow(const std::vector<std::string>& candidates, std::set<std::string>& members, Joins joins) {
	std::map<std::string, std::set<std::string>> waiting; // a name, and the candidates whose scan depended on it
	std::vector<std::string> joined;
	const auto consider = [&](const std::string& name) {
		std::vector<std::string> asked;
		if(!joins(name, asked)) {
			for(std::string& other : asked) waiting[std::move(other)].insert(name);
		} else if(members.insert(name).second) {
			joined.push_back(name);
		}
	};
	for(const std::string& name : candidates)
		if(!members.contains(name)) consider(name);
	while(!joined.empty()) {
		const auto it = waiting.find(joined.back());
		joined.pop_back();
		if(it == waiting.end()) continue;
		const std::set<std::string> waiters = std::move(it->second);
		waiting.erase(it);
		for(const std::string& waiter : waiters)
			if(!members.contains(waiter)) consider(waiter);
	}
}

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
	const ControlScan scan(actions != doc.end() ? *actions : none, control, runners, *components);
	grow(checker::keysOf(*components), runners, [&](const std::string& name, std::vector<std::string>& asked) {
		scan.note(nullptr, &asked);
		std::vector<std::string> found;
		scan.walk(*scan.component(name), name, Part::Steps, found);
		return !found.empty();
	});
	return runners;
}

/// @brief The document's actions that may run control steps: those with one, and those that run one of them, a
/// native action or a computed one.
/// @param doc The document.
/// @param granted The host's actions its points pass on: they don't count (grantedActions()).
/// @return Their names.
std::set<std::string> controlActions(const json& doc, const std::set<std::string, std::less<>>& granted) {
	std::set<std::string> control;
	if(!doc.contains("actions") || !doc["actions"].is_object()) return control;
	const json& actions = doc["actions"];
	static const json none; // actions use no component
	const ControlScan scan(actions, control, runningBuiltInNames(), none);
	std::vector<std::string> candidates = checker::keysOf(actions);
	std::erase_if(candidates, [&](const std::string& name) { return granted.contains(name); });
	grow(candidates, control, [&](const std::string& name, std::vector<std::string>& asked) {
		scan.note(&asked, nullptr);
		std::vector<std::string> found;
		scan.walk(*scan.action(name), name, Part::Steps, found);
		return !found.empty();
	});
	return control;
}

} // namespace

std::vector<std::string> controlsIn(const json& doc, const std::set<std::string, std::less<>>& granted) {
	std::vector<std::string> found;
	if(!doc.is_object()) return found;
	static const json none; // both branches are references: the scan keeps one
	const std::set<std::string> control = controlActions(doc, granted);
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

} // namespace affresco::format
