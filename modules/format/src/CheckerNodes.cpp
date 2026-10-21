/// @file
/// @brief The checker's walk of nodes (components, styles, repeat, variables, properties, references) and of
/// behaviour (handlers, steps, actions run with their callers' variables).

#include "CheckerImpl.hpp"
#include "Models.hpp"

#include <format>

namespace affresco::format::checker {

namespace {

/// Visual states whose properties are shown on top of the node's own (SCREEN_FORMAT.md, "Every type").
constexpr auto visualStates = std::to_array<std::string_view>({"hover", "focused", "dragging", "dragOver", "pressed"});

/// Node keys walkNode() reads before the properties, or that are no values.
constexpr auto nodeKeys = std::to_array<std::string_view>(
    {"type", "id", "note", "slot", "repeat", "with", "let", "check", "use", "children"});

/// The steps the runtime knows, by the key that names each, in the order the runtime looks for them (Steps.cpp).
constexpr auto stepNames = std::to_array<std::string_view>(
    {"set",     "push",     "remove",  "toggle", "increment",  "if",    "forEach",  "while",
	 "run",     "call",     "emit",    "send",   "connect",    "patch", "accept",   "reject",
	 "preview", "restore",  "revert",  "undo",   "endPreview", "focus", "scrollTo", "navigate",
	 "after",   "debounce", "animate", "copy",   "log",        "stop",  "note"});

/// Steps that change the state at the path their key names.
constexpr auto stateSteps =
    std::to_array<std::string_view>({"set", "push", "remove", "toggle", "increment", "animate"});

/// Step keys that hold steps, checked after the step's values.
constexpr auto nestedSteps = std::to_array<std::string_view>({"then", "else", "do", "onError"});

/// @brief Reads a name given as text, with a default.
/// @param object An object.
/// @param key The key.
/// @param fallback Used when the key is missing or not text.
/// @return The name.
std::string nameOr(const json& object, const char* key, const char* fallback) {
	const auto it = object.find(key);
	return it != object.end() && it->is_string() ? it->get<std::string>() : fallback;
}

/// @brief Collects the text literals an expression may give as its result: `on ? 'chipOn' : 'chip'` gives both.
/// @param tree The compiled expression.
/// @param names Receives them.
void resultLiterals(const json& tree, std::vector<std::string>& names) {
	if(!tree.is_object() || tree.size() != 1) return;
	const auto only = tree.begin(); // kept: GCC sees a reference through a temporary iterator as dangling
	const std::string& name = only.key();
	const json& raw = only.value();
	if(name == "$value" && raw.is_string()) {
		names.push_back(raw.get<std::string>());
	} else if(name == "$if" && raw.is_array() && raw.size() == 3) {
		resultLiterals(raw[1], names);
		resultLiterals(raw[2], names);
	} else if(among(choiceOperators, name) && raw.is_array()) { // $and, $or and $coalesce give an argument
		for(const json& item : raw) resultLiterals(item, names);
	}
}

/// @brief The names a reference may give: the name itself, the names in a list, or those an expression may choose.
/// @param value The reference.
/// @return The names; none for text built around an expression (`"@{{card.tag}}Soft"`), which can't be known.
std::vector<std::string> candidateNames(const json& value) {
	std::vector<std::string> names;
	if(value.is_array()) {
		for(const json& item : value)
			if(item.is_string() && !isDynamic(item)) names.push_back(item.get<std::string>());
	} else if(value.is_string() && !isDynamic(value)) {
		names.push_back(value.get<std::string>());
	} else if(value.is_string()) {
		const Placeholders found = placeholders(value.get_ref<const std::string&>());
		try {
			if(found.whole) resultLiterals(compile(found.expressions.front(), ""), names);
		} catch(const DocumentError&) { // NOLINT(bugprone-empty-catch): shapeOf() reports it, with its place
		}
	}
	return names;
}

/// @brief What a property refers to by name.
struct Reference {
	std::string_view kind;   ///< For messages: "color".
	const char* section;     ///< Where the names are defined: "colors".
	std::string_view prefix; ///< Written before the name: "@" (a colour without it is "#rrggbb" or "transparent").
};

/// @brief Tells what a property refers to.
/// @param key The property.
/// @return Colours for "color" and "...Color", fonts for "font", styles for "style"; else nothing.
std::optional<Reference> referenceOf(std::string_view key) {
	if(key == "color" || key.ends_with("Color")) return Reference{"color", "colors", "@"};
	if(key == "font") return Reference{"font", "fonts", ""};
	if(key == "style") return Reference{"style", "styles", ""};
	return std::nullopt;
}

/// @brief Tells whether a property may refer to anything: plain text and numbers don't, unless they name something.
/// @param key The property.
/// @param value Its value.
/// @return true for objects, lists, expressions, and names of actions, paths, styles, fonts and colours.
bool mayReferTo(const std::string& key, const json& value) {
	if(value.is_structured()) return true;
	if(!value.is_string()) return false;
	return isDynamic(value) || handlerKey(key) || key == "bind" || key == "caret" || referenceOf(key).has_value();
}

/// @brief Tells whether a step's key is what the step writes, or where: those are read by walkWrite().
/// @param kind The step.
/// @param key A key of it.
/// @return true for the target of a state step, "to" of set, "value" of push and "into" of call.
bool writtenKey(std::string_view kind, std::string_view key) {
	return (among(stateSteps, kind) && key == kind) || (kind == "set" && key == "to") ||
	       (kind == "push" && key == "value") || (kind == "call" && key == "into");
}

/// @brief Tells whether a component's node has a `{"slot": true}` for the use's children, as the runtime looks for
/// it (not inside nodes that use components themselves).
/// @param node The node.
/// @param depth Nodes inside each other so far.
/// @return true if it has one.
bool hasSlot(const json& node, int depth = 0) {
	const auto children = node.find("children");
	if(depth > maxDepth || children == node.end() || !children->is_array()) return false;
	return std::ranges::any_of(*children, [&](const json& child) {
		if(!child.is_object()) return false;
		if(const auto slot = child.find("slot"); slot != child.end() && *slot == true) return true;
		return !child.contains("use") && hasSlot(child, depth + 1);
	});
}

/// @brief Finds which step a step object is.
/// @param step The step.
/// @return The key that names it, the first the runtime looks for; std::nullopt if it has none.
std::optional<std::string_view> stepKind(const json& step) {
	for(const std::string_view name : stepNames)
		if(step.contains(name)) return name;
	return std::nullopt;
}

} // namespace

Prepared prepare(const json& node, const std::string& place) {
	Prepared result;
	result.place = place;
	result.layers.push_back({&node, place, false});
	if(const auto children = node.find("children"); children != node.end()) {
		result.children = &*children;
		result.childrenPlace = place;
	}
	return result;
}

/* Nodes */

void Checker::walkNode(const json& node, const std::string& place, const Scope& scope, const Slot* slot, bool child) {
	if(!node.is_object() || nodeDepth_ >= maxDepth || !visit()) return;
	++nodeDepth_;
	Prepared prepared = prepare(node, place);
	Slot own; // a slot inside a component is its use's, never an enclosing one's
	const bool used = child && node.contains("use");
	if(!used || expandUse(prepared, own)) walkPrepared(prepared, scope, used ? &own : slot, child);
	--nodeDepth_;
}

void Checker::walkPrepared(Prepared& node, const Scope& scope, const Slot* slot, bool child) {
	applyStyles(node, 0);
	Scope repeated{&scope, nullptr, json(), {}, {}}; // variables start as null, which costs nothing until one is added
	if(child && node.value("repeat")) walkRepeat(node, repeated);
	Scope layer{&repeated, nullptr, json(), {}, {}};
	walkLayer(node, layer);
	walkProperties(node, layer);

	// A component's body is checked once for each set of variable names it is used with
	if(node.component.empty() || walked_.insert("component " + node.component + "|" + variableNames(layer)).second)
		walkChildren(node, layer, slot);
	else if(slot && slot->children) walkSlot(*slot, layer);
}

bool Checker::expandUse(Prepared& node, Slot& slot) {
	for(int depth = 0; const json* use = node.value("use"); ++depth) {
		if(!use->is_string() || depth >= maxComponentDepth) return false;
		const std::string name = use->get<std::string>();
		const json* component = componentNamed(name, node.where("use"));
		if(!component) return false;

		Prepared next = prepare((*component)["node"], "components." + name + ".node");
		next.component = name;
		json with = takeParameters(node, *component, name, next.parameterPlaces, depth == 0);
		for(Layer& layer : node.layers) { // the use's properties win over the component's
			layer.use = true;
			next.layers.push_back(std::move(layer));
		}
		next.made = std::move(node.made);
		next.made.push_back(std::make_unique<json>(json{{"with", std::move(with)}}));
		next.layers.push_back({next.made.back().get(), node.place, false});
		if(node.children && hasSlot((*component)["node"])) {
			slot = {node.children, node.childrenPlace};
		} else if(node.children) {
			next.children = node.children;
			next.childrenPlace = node.childrenPlace;
		}
		node = std::move(next);
	}
	return true;
}

const json* Checker::componentNamed(const std::string& name, const std::string& place) {
	const json* component = definition("components", name);
	const bool valid =
	    component && component->is_object() && component->contains("node") && (*component)["node"].is_object();
	if(!valid) {
		report(place, "unknown component '" + name + "'",
		       hint([&] { return hintFor(name, definedNames("components"), "", "defined:"); }));
		return nullptr;
	}
	componentsReached_.insert(name);
	return component;
}

json Checker::takeParameters(const Prepared& use, const json& component, const std::string& name,
                             std::map<std::string, std::string>& places, bool written) {
	const auto declared = component.find("params");
	json with = declared != component.end() && declared->is_object() ? *declared : json::object();
	for(const auto& [key, value] : with.items()) places[key] = std::format("components.{}.params.{}", name, key);

	if(const json* given = use.value("with"); given && given->is_object())
		for(const auto& [key, value] : given->items()) {
			const auto place = use.parameterPlaces.find(key);
			const std::string at =
			    place != use.parameterPlaces.end() ? place->second : std::format("{}.{}", use.where("with"), key);
			if(written && !with.contains(key))
				report(at, std::format("unknown parameter '{}' of component '{}'", key, name),
				       hint([&] { return hintFor(key, keysOf(with), "", "it takes"); }));
			with[key] = value;
			places[key] = at;
		}
	return with;
}

void Checker::applyStyles(Prepared& node, int depth) {
	const json* style = node.value("style");
	if(!style || isDynamic(*style)) return; // chosen per instance: walkDynamicStyle()
	std::vector<Layer> styles;
	mergeStyles(*style, node.where("style"), depth, styles);
	node.layers.insert(node.layers.begin(), styles.begin(), styles.end()); // the node's own properties win
}

void Checker::mergeStyles(const json& names, const std::string& place, int depth, std::vector<Layer>& styles) {
	for(const json& name : names.is_array() ? names : json::array({names})) {
		if(!name.is_string()) continue;
		const json* style = definition("styles", name.get<std::string>());
		if(!style || !style->is_object()) {
			refer("style", "styles", name.get<std::string>(), "", place);
			continue;
		}
		const std::string at = "styles." + name.get<std::string>();
		// A style's own styles come first, so it wins over them; deeper, the runtime reports a style that uses itself
		if(const auto inner = style->find("style"); inner != style->end() && !isDynamic(*inner) && depth < 16)
			mergeStyles(*inner, at + ".style", depth + 1, styles);
		styles.push_back({style, at, false});
	}
}

void Checker::walkRepeat(const Prepared& node, Scope& scope) {
	const json& repeat = *node.value("repeat");
	if(!repeat.is_object()) return;
	const std::string at = node.where("repeat");
	Shape list = anything;
	const auto source = repeat.find("in");
	if(source != repeat.end() && source->is_string() && modelName(source->get_ref<const std::string&>())) {
		// a data model's rows: the application gives them, so their shape is known only while it runs
	} else if(source != repeat.end()) { // a path, or any value
		list = source->is_string() && !isDynamic(*source)
		           ? shapeOfExpression(source->get_ref<const std::string&>(), *scope.parent, at + ".in")
		           : shapeOf(*source, *scope.parent, at + ".in");
	}
	scope.vars[nameOr(repeat, "for", "item")] = itemOf(list);
	scope.vars[nameOr(repeat, "index", "index")] = single;
	if(const auto key = repeat.find("key"); key != repeat.end()) read(*key, scope, at + ".key");
}

void Checker::walkLayer(const Prepared& node, Scope& layer) {
	if(const json* with = node.value("with"); with && with->is_object())
		for(const auto& [name, value] : with->items()) {
			const auto place = node.parameterPlaces.find(name);
			layer.vars[name] = shapeOf(
			    value, layer, place != node.parameterPlaces.end() ? place->second : node.where("with") + "." + name);
		}
	if(const json* let = node.value("let"); let && let->is_object())
		for(const auto& [name, value] : let->items())
			layer.vars[name] = shapeOf(value, layer, node.where("let") + "." + name);
	if(const json* check = node.value("check"); check && check->is_object()) walkCheck(node, *check, layer);
}

void Checker::walkCheck(const Prepared& node, const json& check, Scope& layer) {
	const std::string at = node.where("check");
	const json* bind = node.value("bind"); // the value checked, unless "value" says
	const bool bound = bind && bind->is_string() && !isDynamic(*bind);
	layer.vars["value"] = check.contains("value") ? shapeOf(check["value"], layer, at + ".value")
	                      : bound ? shapeOfPath(bind->get_ref<const std::string&>(), layer, node.where("bind"))
	                              : anything;
	layer.vars["valid"] = single;
	layer.vars["error"] = single;
	for(const char* key : {"rule", "message"})
		if(check.contains(key)) read(check[key], layer, at + "." + key);
}

void Checker::walkProperties(const Prepared& node, const Scope& scope) {
	node.forEach([&](const std::string& key, const json& value, const Layer& layer) {
		if(!among(nodeKeys, key)) walkProperty(node, key, value, layer, scope);
	});
}

void Checker::walkProperty(const Prepared& node, const std::string& key, const json& value, const Layer& layer,
                           const Scope& scope) {
	if(!mayReferTo(key, value)) return; // most properties: no place is built for them
	const std::string at = layer.place + "." + key;
	if(walkBehaviour(key, value, scope, at)) return;
	if(key == "bind" || key == "caret") {
		walkBinding(key, value, scope, at);
		if(key == "bind") checkBind(node, value, at);
	} else if(key == "overlay") {
		walkOverlay(value, scope, at);
	} else if(key == "style") { // static styles are layers already: this one is chosen by an expression
		if(isDynamic(value)) walkDynamicStyle(node, value, at, scope);
	} else {
		read(value, scope, at);
		references(key, value, at);
	}
}

bool Checker::walkBehaviour(const std::string& key, const json& value, const Scope& scope, const std::string& place) {
	if(handlerKey(key)) {
		walkHandler(value, scope, place);
	} else if(key == "keys" && value.is_object()) {
		for(const auto& [shortcut, handler] : value.items())
			walkHandler(handler, scope, std::format("{}.{}", place, shortcut));
	} else if(key == "every" && value.is_object()) {
		if(value.contains("ms")) read(value["ms"], scope, place + ".ms");
		if(value.contains("do")) walkHandler(value["do"], scope, place + ".do");
	} else {
		return false;
	}
	return true;
}

void Checker::walkDynamicStyle(const Prepared& node, const json& style, const std::string& place, const Scope& scope) {
	read(style, scope, place);
	if(styleDepth_ >= 16) return; // a style that chooses itself
	++styleDepth_;
	for(const std::string& name : candidateNames(style)) {
		if(name.empty()) continue; // no style
		Prepared styled;
		styled.place = node.place;
		mergeStyles(name, place, 0, styled.layers);
		walkProperties(styled, scope);
	}
	--styleDepth_;
}

void Checker::references(const std::string& key, const json& value, const std::string& place) {
	if(among(visualStates, key) && value.is_object()) {
		for(const auto& [property, shown] : value.items())
			references(property, shown, std::format("{}.{}", place, property));
		return;
	}
	const std::optional<Reference> reference = referenceOf(key);
	if(!reference) return;
	for(const std::string& name : candidateNames(value))
		if(!name.empty() && name.starts_with(reference->prefix)) // "" is no style; "#fff" is no colour name
			refer(reference->kind, reference->section, name.substr(reference->prefix.size()), reference->prefix, place);
}

void Checker::walkBinding(const std::string& key, const json& value, const Scope& scope, const std::string& place) {
	if(!value.is_string() || isDynamic(value)) { // "{{...}}" gives the path: it can't be followed
		read(value, scope, place);
		return;
	}
	if(key == "bind") (void)shapeOfPath(value.get_ref<const std::string&>(), scope, place);
	write(value, key == "caret" ? caretShape() : single, Change::set, scope, place);
}

void Checker::walkOverlay(const json& value, const Scope& scope, const std::string& place) {
	read(value, scope, place);
	if(!value.is_object()) return;
	const auto open = value.find("open");
	if(open == value.end() || !open->is_string() || isDynamic(*open)) return;
	(void)shapeOfPath(open->get_ref<const std::string&>(), scope, place + ".open");
	if(value.contains("dismiss"))
		write(*open, single, Change::set, scope, place + ".open"); // a click outside closes it
}

void Checker::walkChildren(const Prepared& node, const Scope& scope, const Slot* slot) {
	if(!node.children || !node.children->is_array()) return;
	const json& children = *node.children;
	for(std::size_t i = 0; i < children.size(); ++i) {
		const json& child = children[i];
		if(slot && slot->children && child.is_object() && child.contains("slot") && child["slot"] == true) {
			walkSlot(*slot, scope);
			continue;
		}
		walkNode(child, node.childrenPlace + ".children[" + std::to_string(i) + "]", scope, slot, true);
	}
}

void Checker::walkSlot(const Slot& slot, const Scope& scope) {
	if(!slot.children->is_array()) return;
	for(std::size_t i = 0; i < slot.children->size(); ++i)
		walkNode((*slot.children)[i], slot.place + ".children[" + std::to_string(i) + "]", scope, nullptr, true);
}

/* Behaviour */

void Checker::walkHandler(const json& handler, const Scope& scope, const std::string& place) {
	if(handler.is_string()) callAction(handler.get<std::string>(), scope, place);
	else walkSteps(handler, scope, place);
}

void Checker::walkSteps(const json& steps, const Scope& scope, const std::string& place) {
	if(steps.is_array()) {
		for(std::size_t i = 0; i < steps.size(); ++i) walkStep(steps[i], scope, place + "[" + std::to_string(i) + "]");
	} else if(steps.is_string()) {
		callAction(steps.get<std::string>(), scope, place);
	} else {
		walkStep(steps, scope, place);
	}
}

void Checker::walkStep(const json& step, const Scope& scope, const std::string& place) {
	if(!step.is_object() || !visit()) return;
	const std::optional<std::string_view> kind = stepKind(step);
	if(!kind) {
		report(place, "unknown step",
		       "expected set, push, remove, toggle, increment, if, forEach, while, run, call, emit, send, connect, "
		       "patch, focus, scrollTo, navigate, after, debounce, animate, copy, log, stop, note, accept, reject, "
		       "preview, endPreview, restore, revert or undo");
		return;
	}
	if(*kind == "run") return walkRun(step, scope, place);
	if(*kind == "forEach") return walkForEach(step, scope, place);

	walkWrite(*kind, step, scope, place);
	walkStepValues(*kind, step, scope, place);
	walkNestedSteps(step, scope, place);
}

void Checker::walkStepValues(std::string_view kind, const json& step, const Scope& scope, const std::string& place) {
	for(const auto& [key, value] : step.items()) // walkWrite() reads the target, and what is written there
		if(!writtenKey(kind, key) && !among(nestedSteps, key) && key != "note")
			read(value, scope, std::format("{}.{}", place, key));
	if(kind == "navigate")
		for(const std::string& name : candidateNames(step["navigate"]))
			if(name != "back") refer("screen", "screens", name, "", place + ".navigate");
}

void Checker::walkNestedSteps(const json& step, const Scope& scope, const std::string& place) {
	for(const std::string_view key : nestedSteps) {
		const auto steps = step.find(key);
		if(steps == step.end()) continue;
		Scope layer{&scope, nullptr, json(), {}, {}};
		if(key == "onError") layer.vars["error"] = single; // a native call's error message
		walkHandler(*steps, layer, place + "." + std::string(key));
	}
}

void Checker::walkWrite(std::string_view kind, const json& step, const Scope& scope, const std::string& place) {
	const std::string key(kind);
	if(among(stateSteps, kind)) checkWrite(kind, step, place);
	const auto valueOf = [&](const char* name) {
		return step.contains(name) ? shapeOf(step[name], scope, place + "." + name) : anything;
	};
	if(kind == "set") write(step[key], valueOf("to"), Change::set, scope, place + "." + key);
	else if(kind == "push") write(step[key], valueOf("value"), Change::push, scope, place + "." + key);
	else if(kind == "remove") write(step[key], anything, Change::none, scope, place + "." + key);
	else if(among(stateSteps, kind)) write(step[key], single, Change::set, scope, place + "." + key); // numbers, flags
	else if(kind == "call" && step.contains("into")) write(step["into"], anything, Change::set, scope, place + ".into");
}

void Checker::walkRun(const json& step, const Scope& scope, const std::string& place) {
	// "with": the action's own variables, evaluated where the step is
	Scope layer{&scope, nullptr, json(), {}, {}};
	if(const auto with = step.find("with"); with != step.end() && with->is_object())
		for(const auto& [name, value] : with->items())
			layer.vars[name] = shapeOf(value, scope, std::format("{}.with.{}", place, name));
	const json& name = step["run"];
	if(name.is_string() && !isDynamic(name)) callAction(name.get<std::string>(), layer, place + ".run");
	else read(name, scope, place + ".run"); // a name an expression computes: not checked
}

void Checker::walkForEach(const json& step, const Scope& scope, const std::string& place) {
	const json& source = step["forEach"];
	const Shape list = source.is_string() && !isDynamic(source)
	                       ? shapeOfPath(source.get_ref<const std::string&>(), scope, place + ".forEach")
	                       : shapeOf(source, scope, place + ".forEach");
	Scope layer{&scope, nullptr, json(), {}, {}};
	layer.vars[nameOr(step, "as", "item")] = itemOf(list);
	layer.vars[nameOr(step, "index", "index")] = single;
	if(const auto steps = step.find("do"); steps != step.end()) walkSteps(*steps, layer, place + ".do");
}

void Checker::callAction(const std::string& name, const Scope& scope, const std::string& place) {
	if(const json* steps = definition("actions", name)) {
		actionsReached_.insert(name);
		// An action sees its caller's variables: it is checked once for each set of names it is run with
		if(actionDepth_ >= maxActionDepth || !walked_.insert("action " + name + "|" + variableNames(scope)).second)
			return;
		++actionDepth_;
		walkSteps(*steps, scope, "actions." + name);
		--actionDepth_;
		return;
	}
	if(nativeActions_.contains(name)) return;
	report(place, "unknown action '" + name + "'", hint([&] {
		       std::vector<std::string> names = definedNames("actions");
		       names.insert(names.end(), nativeActions_.begin(), nativeActions_.end());
		       return hintFor(name, names, "", "defined:");
	       }));
}

} // namespace affresco::format::checker
