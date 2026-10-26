/// @file
/// @brief Adapters: a feature's needs bound to a host, directly or through a declarative mapping, and the bindings
/// applied to what the feature provides before Composition merges it.

#include "AdapterMapping.hpp"
#include "ContributionPoints.hpp"
#include "Expression.hpp"
#include "ExpressionParser.hpp"
#include "FeatureManifest.hpp"
#include "NodeKeys.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <set>
#include <tuple>

namespace affresco::format {

namespace {

using expect::fail;

/// The forms of a binding, by the key that names what it binds to.
constexpr auto bindingForms = std::to_array<std::string_view>({"export", "value", "action", "point", "capability"});

/// The keys of an adapter.
constexpr auto adapterKeys = std::to_array<std::string_view>({"format", "version", "feature", "host", "note", "bind"});

/// The names an adapter's expression may start with: the host's exports, and a list function's item and index.
constexpr auto valueRoots = std::to_array<std::string_view>({"app", "it", "index"});

/// A set of names.
using Names = std::set<std::string, std::less<>>;

/// @brief The form of a binding: the one key of bindingForms it has.
/// @param binding The binding.
/// @return The key, or empty if it has none.
std::string formOf(const json& binding) {
	for(const std::string_view form : bindingForms)
		if(binding.is_object() && binding.contains(form)) return std::string(form);
	return {};
}

/// @brief Calls a function on every text in a value.
/// @param value The value.
/// @param each The function, given the text.
/// @param depth How deep the walk is, bounded by maxAdapterDepth.
template <typename Each> void eachText(const json& value, const Each& each, int depth = 0) {
	if(depth > maxAdapterDepth) return;
	if(value.is_string()) each(value.get_ref<const std::string&>());
	else if(value.is_structured())
		for(const json& item : value) eachText(item, each, depth + 1);
}

/// @brief Checks a text of a binding: its length and each of its expressions.
/// @param text The text.
/// @param place Its place.
/// @throws DocumentError If it is too long or an expression doesn't parse.
void checkText(const std::string& text, const std::string& place) {
	if(text.size() > maxBindingExpression) fail(place, std::format("longer than {} characters", maxBindingExpression));
	const Placeholders found = placeholders(text);
	if(found.unclosed) fail(place, "a '{{' has no '}}'");
	for(const std::string_view expression : found.expressions) (void)detail::parse(expression, place);
}

/// @brief Checks one binding's shape.
/// @param binding The binding.
/// @param place Its place.
/// @throws DocumentError If it is malformed.
void checkBinding(const json& binding, const std::string& place) {
	expect::object(binding, place);
	const std::string form = formOf(binding);
	if(form.empty()) fail(place, R"(expected one of "export", "value", "action", "point" or "capability")");
	for(const auto& [key, value] : binding.items())
		if(key != form && !(key == "with" && form == "action") && key != "note")
			fail(concatenate(place, ".", key),
			     form == "action" ? "unknown key" : "only an action binding has more than one key");
	checkText(expect::string(binding[form], place + "." + form), place + "." + form);
	if(form == "value" && !placeholders(binding["value"].get<std::string>()).whole)
		fail(place + ".value", "expected one expression, \"{{...}}\"");
	if(binding.contains("with")) {
		expect::object(binding["with"], place + ".with");
		if(nestingDepth(binding["with"]) > maxAdapterDepth) fail(place + ".with", "nested too deeply");
		eachText(binding["with"], [&](const std::string& text) { checkText(text, place + ".with"); });
	}
	if(binding.contains("note")) expect::string(binding["note"], place + ".note");
}

/// @brief Collects the paths an operator tree reads: the names each `$get` starts with.
/// @param tree The tree.
/// @param paths Receives each path's leading names.
/// @param depth How deep the walk is, bounded by maxNesting.
void pathsIn(const json& tree, std::vector<std::vector<std::string>>& paths, int depth = 0) {
	if(depth > maxNesting || !tree.is_structured()) return;
	if(tree.is_object())
		if(const auto get = tree.find("$get"); get != tree.end() && get->is_array() && !get->empty()) {
			std::vector<std::string> names;
			for(const json& key : *get) {
				if(!key.is_string()) break;
				names.push_back(key.get<std::string>());
			}
			if(!names.empty()) paths.push_back(std::move(names));
		}
	for(const json& item : tree) pathsIn(item, paths, depth + 1);
}

/// @brief Tells whether `app.<names...>` reads a host export: some leading names, joined by dots, name one.
/// @param names The names after "app".
/// @param exports The host's exports.
/// @return true if it does.
bool readsExport(const std::vector<std::string>& names, const json& exports) {
	std::string name;
	for(std::size_t index = 1; index < names.size(); ++index) {
		name += (name.empty() ? "" : ".") + names[index];
		if(exports.contains(name)) return true;
	}
	return false;
}

/// @brief Why the expressions of a text read something an adapter can't give.
/// @param text The text.
/// @param roots The names an expression may start with.
/// @param exports The host's exports.
/// @return Why, or empty if every path is allowed.
std::string readsWhy(const std::string& text, const Names& roots, const json& exports) {
	const Placeholders found = placeholders(text); // named: the loop would outlive a temporary
	for(const std::string_view expression : found.expressions) {
		std::vector<std::vector<std::string>> paths;
		try {
			pathsIn(detail::parse(expression, "adapter"), paths);
		} catch(const DocumentError& error) {
			return error.what();
		}
		for(const auto& names : paths) {
			if(!roots.contains(names.front()))
				return std::format("reads '{}', and an adapter reads only app.<export>, it, index and the arguments",
				                   names.front());
			if(names.front() == "app" && !readsExport(names, exports))
				return std::format("reads app.{}, which the host doesn't export", names.size() > 1 ? names[1] : "");
		}
	}
	return {};
}

/// @brief Why a host export doesn't meet a need's contract: kind, type, item fields and arguments.
/// @param need The need.
/// @param entry The export.
/// @return Why, or empty if it meets it.
std::string contractMismatch(const json& need, const json& entry) {
	const std::string wanted = need.value("kind", "");
	const std::string kind = exportKind(entry);
	if(wanted != kind && !(wanted == "value" && kind == "state"))
		return std::format("needs a {}, the host's is a {}", wanted, kind);
	if(need.contains("type") && need["type"] != entry.value("type", json()))
		return std::format("needs a {}, the host's is a {}", need["type"].dump(), entry.value("type", json()).dump());
	const json have = entry.value("item", json::object());
	for(const json entries = need.value("item", json::object()); const auto& [field, type] : entries.items()) {
		if(!have.contains(field)) return std::format("its items have no field '{}'", field);
		if(have[field] != type)
			return std::format("its items' '{}' is a {}, needs a {}", field, have[field].dump(), type.dump());
	}
	if(need.contains("arguments") && need["arguments"] != entry.value("arguments", json::object()))
		return std::format("takes {}, needs {}", entry.value("arguments", json::object()).dump(),
		                   need["arguments"].dump());
	return {};
}

/// @brief Why an action export can't be run by a feature: it isn't an action, names no action, or no point passes it.
/// @param name The export's name.
/// @param host The host document.
/// @return Why, or empty if it can.
std::string actionWhy(const std::string& name, const json& host) {
	const json exports = host.value("exports", json::object());
	if(!exports.contains(name)) return std::format("the host exports no '{}'", name);
	if(exportKind(exports[name]) != "action") return std::format("the host's '{}' isn't an action", name);
	const std::string target = actionTarget(exports[name]);
	if(target.empty()) return std::format("the host's '{}' computes its action", name);
	if(!grantedActions(host).contains(target)) return std::format("no point of the host receives '{}'", name);
	return {};
}

/// @brief The points of a host whose slot is one.
/// @param host The host document.
/// @param slot The slot.
/// @return Their names.
std::vector<std::string> pointsOfSlot(const json& host, const std::string& slot) {
	std::vector<std::string> found;
	for(const json entries = host.value("points", json::object()); const auto& [point, entry] : entries.items())
		if(entry.is_object() && entry.value("slot", "") == slot) found.push_back(point);
	return found;
}

/// @brief Tells whether a host declares a capability: "services.storage" in its "services".
/// @param host The host document.
/// @param capability The capability.
/// @return true if it does.
bool hasCapability(const json& host, const std::string& capability) {
	const auto dot = capability.find('.');
	if(dot == std::string::npos) return false;
	const std::string section = capability.substr(0, dot);
	return host.contains(section) && host[section].is_object() && host[section].contains(capability.substr(dot + 1));
}

/// @brief Binds a need directly, by its name, slot or capability.
/// @param name The need.
/// @param need What it is.
/// @param host The host document.
/// @return The binding, or null with why.
std::pair<json, std::string> bindDirectly(const std::string& name, const json& need, const json& host) {
	const std::string kind = need.value("kind", "");
	if(kind == "slot") {
		const auto points = pointsOfSlot(host, need.value("slot", ""));
		if(points.size() == 1) return {{{"point", points.front()}}, {}};
		return {nullptr, points.empty() ? std::format("the host has no {} point", need.value("slot", ""))
		                                : std::format("the host has {} {} points: an adapter must pick one",
		                                              points.size(), need.value("slot", ""))};
	}
	if(kind == "capability") {
		const std::string capability = need.value("capability", "");
		if(hasCapability(host, capability)) return {{{"capability", capability}}, {}};
		return {nullptr, std::format("the host has no {} (only a host grants it)", capability)};
	}
	const json exports = host.value("exports", json::object());
	if(!exports.contains(name))
		return {nullptr, std::format("the host exports no '{}', and no adapter binds it", name)};
	std::string why = contractMismatch(need, exports[name]);
	if(why.empty() && kind == "action") why = actionWhy(name, host);
	if(!why.empty()) return {nullptr, std::format("the host's '{}' {}", name, why)};
	return {{{"export", name}}, {}};
}

/// @brief The names an action binding's arguments may read: valueRoots and the need's arguments.
/// @param need The need.
/// @return The names.
Names rootsFor(const json& need) {
	Names roots(valueRoots.begin(), valueRoots.end());
	for(const json entries = need.value("arguments", json::object()); const auto& [argument, type] : entries.items())
		roots.insert(argument);
	return roots;
}

/// @brief Tells whether a binding's form can bind a need's kind.
/// @param kind The need's kind.
/// @param form The binding's form.
/// @return true if it can.
bool formFits(const std::string& kind, const std::string& form) {
	if(kind == "slot") return form == "point";
	if(kind == "capability") return form == "capability";
	if(kind == "action") return form == "export" || form == "action";
	return form == "export" || form == "value";
}

/// @brief Why a binding's expressions read something an adapter can't give: a value reads app.<export>, it and
/// index; an action's arguments read those and the need's arguments.
/// @param need The need.
/// @param binding The binding (checked).
/// @param exports The host's exports.
/// @return Why, or empty if they read only what they may.
std::string readsOf(const json& need, const json& binding, const json& exports) {
	if(binding.contains("value"))
		return readsWhy(binding.value("value", ""), Names(valueRoots.begin(), valueRoots.end()), exports);
	std::string why;
	eachText(binding.value("with", json::object()), [&](const std::string& text) {
		if(why.empty()) why = readsWhy(text, rootsFor(need), exports);
	});
	return why;
}

/// @brief Why an adapter's binding doesn't fit a need on a host.
/// @param need The need.
/// @param binding The binding (checked).
/// @param host The host document.
/// @return Why, or empty if it fits.
std::string bindingWhy(const json& need, const json& binding, const json& host) {
	const std::string kind = need.value("kind", "");
	const std::string form = formOf(binding);
	if(!formFits(kind, form)) return std::format("a {} need can't bind to a \"{}\"", kind, form);
	const json exports = host.value("exports", json::object());
	const std::string target = binding[form].get<std::string>();
	if(form == "export") {
		if(!exports.contains(target)) return std::format("the host exports no '{}'", target);
		if(const std::string why = contractMismatch(need, exports[target]); !why.empty())
			return "the host's '" + target + "' " + why;
		return kind == "action" ? actionWhy(target, host) : std::string();
	}
	if(form == "action")
		if(std::string why = actionWhy(target, host); !why.empty()) return why;
	if(form == "value" || form == "action") return readsOf(need, binding, exports);
	if(form == "capability")
		return std::format("a capability is never an adapter's to give (the host must declare {})", target);
	const json points = host.value("points", json::object());
	if(!points.contains(target)) return std::format("the host has no point '{}'", target);
	if(points[target].value("slot", "") != need.value("slot", ""))
		return std::format("the point '{}' is a {}, needs a {}", target, points[target].value("slot", ""),
		                   need.value("slot", ""));
	return {};
}

/// @brief Applies an action need's binding: the action `adapted.<need>` running the host's.
/// @param need The need.
/// @param binding Its binding.
/// @param exports The host's exports.
/// @param actions The feature's actions, which receive it.
/// @param key The need's name.
/// @return The host action's name, or empty if the binding names none.
std::string adaptAction(const json& need, const json& binding, const json& exports, json& actions,
                        const std::string& key) {
	const std::string name = binding.value(binding.contains("action") ? "action" : "export", "");
	std::string target = exports.contains(name) ? actionTarget(exports[name]) : std::string();
	if(target.empty()) return {};
	json with = binding.value("with", json::object());
	if(!binding.contains("action"))
		for(const json entries = need.value("arguments", json::object());
		    const auto& [argument, type] : entries.items())
			with[argument] = "{{" + argument + "}}";
	json step = {{"run", target}};
	if(!with.empty()) step["with"] = std::move(with);
	actions["adapted." + key] = json::array({std::move(step)});
	return target;
}

/// @brief Moves a slot need's views to the point it binds to.
/// @param provides What the feature provides, changed in place.
/// @param need The need's name (the views' key in the manifest).
/// @param point The point, or empty to drop them (an optional need left unbound).
void moveViews(json& provides, const std::string& need, const std::string& point) {
	if(!provides.contains("views") || !provides["views"].contains(need)) return;
	json views = std::move(provides["views"][need]);
	provides["views"].erase(need);
	if(point.empty()) return;
	for(auto& [id, node] : views.items()) provides["views"][point][id] = std::move(node);
}

/// @brief One need of a feature being adapted.
struct NeedOf {
	const std::string& feature; ///< The feature.
	const std::string& name;    ///< The need's name.
	const json& wanted;         ///< The need.
	std::string kind;           ///< Its kind.
	std::string place;          ///< Its place, for problems: "features.<feature>.needs.<name>: ".
};

/// @brief Adapts a feature to a need the app leaves unbound: an optional slot shows nothing, an optional value is
/// null, and a need that isn't optional is a problem.
/// @param need The need.
/// @param adapted The feature being adapted, changed in place.
void adaptUnbound(const NeedOf& need, AdaptedFeature& adapted) {
	if(!need.wanted.value("optional", false)) adapted.problems.push_back(need.place + "not bound to the app");
	else if(need.kind == "slot") moveViews(adapted.provides, need.name, "");
	else if(need.kind != "action" && need.kind != "capability") adapted.replacements.emplace_back(need.name, "null");
}

/// @brief Adapts a feature to a need the app binds: its views move to the point, its action runs the app's, and its
/// value is the app's export or value.
/// @param need The need.
/// @param binding Its binding.
/// @param exports The host's exports.
/// @param adapted The feature being adapted, changed in place.
void adaptBound(const NeedOf& need, const json& binding, const json& exports, AdaptedFeature& adapted) {
	std::string why = readsOf(need.wanted, binding, exports);
	if(why.empty() && binding.contains("export") && !exports.contains(binding.value("export", "")))
		why = "the app exports no '" + binding.value("export", "") + "'";
	if(!why.empty()) adapted.problems.push_back(need.place + why);
	else if(need.kind == "slot") moveViews(adapted.provides, need.name, binding.value("point", ""));
	else if(need.kind == "action") {
		if(adaptAction(need.wanted, binding, exports, adapted.provides["actions"], need.name).empty())
			adapted.problems.push_back(need.place + "the app exports no action for it");
		else adapted.replacements.emplace_back(need.name, need.feature + ".adapted." + need.name);
	} else if(need.kind != "capability") {
		const std::string text =
		    binding.contains("export") ? "app." + binding.value("export", "") : binding.value("value", "");
		const Placeholders found = placeholders(text);
		adapted.replacements.emplace_back(need.name, found.whole ? std::string(found.expressions.front()) : text);
	}
}

} // namespace

void checkAdapter(const json& adapter) {
	expect::object(adapter, "adapter");
	if(nestingDepth(adapter) > maxAdapterDepth) fail("adapter", std::format("nested deeper than {}", maxAdapterDepth));
	if(adapter.dump().size() > maxAdapterBytes) fail("adapter", std::format("larger than {} bytes", maxAdapterBytes));
	for(const auto& [key, value] : adapter.items())
		if(std::ranges::find(adapterKeys, key) == adapterKeys.end()) fail("adapter." + key, "unknown key");
	if(adapter.value("format", json()) != json(adapterFormat))
		fail("adapter.format", "expected \"affrescoui-adapter\"");
	if(adapter.value("version", json()) != 1) fail("adapter.version", "expected 1");
	for(const char* key : {"feature", "host", "note"})
		if(adapter.contains(key)) expect::string(adapter[key], std::string("adapter.") + key);
	checkBindings(adapter.value("bind", json::object()), "adapter.bind");
}

void checkBindings(const json& bindings, const std::string& place) {
	expect::object(bindings, place);
	if(bindings.size() > maxBindings) fail(place, std::format("more than {} bindings", maxBindings));
	for(const auto& [need, binding] : bindings.items()) {
		if(need.empty() || need.size() > 64) fail(place, "a need's name is empty or too long");
		checkBinding(binding, concatenate(place, ".", need));
	}
}

BoundNeeds bindNeeds(const json& needs, const json& host, const json& adapter) {
	BoundNeeds bound;
	const json bind = adapter.is_object() ? adapter.value("bind", json::object()) : json::object();
	for(const auto& [name, need] : needs.items()) {
		json binding;
		std::string why;
		const bool adapted = bind.contains(name);
		if(adapted) {
			binding = bind[name];
			why = bindingWhy(need, binding, host);
		} else std::tie(binding, why) = bindDirectly(name, need, host);
		if(!why.empty()) {
			if(!need.value("optional", false)) bound.unmet.push_back({{"need", name}, {"why", why}});
			continue;
		}
		bound.bindings[name] = binding;
		bound.records.push_back(
		    {{"need", name}, {"kind", need.value("kind", "")}, {"by", adapted ? "adapter" : "host"}, {"to", binding}});
	}
	for(const auto& [name, binding] : bind.items())
		if(!needs.contains(name))
			bound.unmet.push_back({{"need", name}, {"why", "the adapter binds a need the feature doesn't have"}});
	return bound;
}

AdaptedFeature adaptFeature(const std::string& name, const json& record, const json& exports) {
	AdaptedFeature adapted{record.value("provides", json::object()), {}, {}};
	const json bindings = record.value("bindings", json::object());
	if(adapted.provides.contains("actions"))
		for(const auto& [key, steps] : adapted.provides["actions"].items())
			if(key.starts_with("adapted."))
				adapted.problems.push_back(
				    std::format("features.{}.provides.actions.{}: \"adapted.\" names are the adapter's", name, key));
	for(const json entries = record.value("needs", json::object()); const auto& [need, wanted] : entries.items()) {
		const NeedOf of{name, need, wanted, wanted.value("kind", ""),
		                std::format("features.{}.needs.{}: ", name, need)};
		if(bindings.contains(need)) adaptBound(of, bindings[need], exports, adapted);
		else adaptUnbound(of, adapted);
	}
	std::ranges::stable_sort(adapted.replacements, std::ranges::greater{},
	                         [](const auto& item) { return item.first.size(); });
	return adapted;
}

} // namespace affresco::format
