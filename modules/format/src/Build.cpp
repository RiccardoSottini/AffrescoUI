#include "RuntimeImpl.hpp"

namespace affresco::format {

namespace {

/// @brief Tells whether a node sets its own position or size (otherwise a screen fills its outlet).
/// @param node The node.
/// @return true if it has layout properties.
bool hasLayout(const json& node) {
	for(const char* key : {"x", "y", "width", "height", "anchors", "fill"})
		if(node.contains(key)) return true;
	return false;
}

} // namespace

/* Building */

std::unique_ptr<Panel> Runtime::Impl::create(const std::string& type, const std::string& path) const {
	if(type == "panel") return std::make_unique<Panel>();
	if(type == "label") return std::make_unique<Label>();
	if(type == "textarea") return std::make_unique<TextArea>();
	if(type == "editor") return std::make_unique<TextEditor>();
	if(type == "stack" || type == "column") return std::make_unique<Stack>();
	if(type == "row") {
		auto row = std::make_unique<Stack>();
		row->setDirection(Direction::Row);
		return row;
	}
	if(type == "grid") return std::make_unique<Grid>();
	if(type == "toggle") return std::make_unique<Toggle>(Toggle::Look::Switch);
	if(type == "checkbox") return std::make_unique<Toggle>(Toggle::Look::Checkbox);
	if(type == "slider") return std::make_unique<Slider>();
	if(type == "outlet") {
		auto outlet = std::make_unique<Panel>();
		outlet->setColor(colors::transparent);
		return outlet;
	}

	if(const auto it = types.find(type); it != types.end()) {
		auto panel = it->second.factory();
		if(!panel) fail(path, "the factory of type '" + type + "' returned nothing");
		return panel;
	}

	if(placeholderTypes) {
		log(LogLevel::Warning, "AffrescoUI screen: " + path + ": unknown type '" + type + "', shown as a plain panel");
		return std::make_unique<Panel>();
	}
	fail(path, "unknown type '" + type +
	               "' (built in: panel, label, textarea, editor, stack, row, column, grid, toggle, checkbox, slider, outlet; "
	               "others must be registered)");
}

// Starts tracking a newly created panel, remembering its defaults so they can be restored later
Record& Runtime::Impl::adopt(Panel& panel, const std::string& type) {
	Record& record = records[&panel];
	record.type = type;
	record.firstManaged = panel.children().size();
	record.defaultColor = panel.color();
	record.defaultCursor = panel.cursor();
	hookUp(panel, record);
	return record;
}

// Signals are connected once; handlers read the panel's current record, so later updates need no reconnecting
void Runtime::Impl::hookUp(Panel& panel, Record& adopted) {
	Record::Hooks& hooks = adopted.hooks;
	hooks.hover = panel.onHover.connect([this, &panel](bool hovered) {
		Record* record = recordOf(&panel);
		if(!record) return;
		record->hovered = hovered;
		refresh(panel, *record);
		fire(panel, hovered ? "onHover" : "onLeave", {{"type", hovered ? "hover" : "leave"}});
	});
	hooks.focus = panel.onFocusChanged.connect([this, &panel](bool focused) {
		Record* record = recordOf(&panel);
		if(!record) return;
		record->focused = focused;
		refresh(panel, *record);
		fire(panel, focused ? "onFocus" : "onBlur", {{"type", focused ? "focus" : "blur"}});
	});
	hooks.button =
	    panel.onMouseButton.connect([this, &panel](const MouseButtonEvent& event) { mouseButton(panel, event); });
	hooks.move = panel.onMouseMove.connect([this, &panel](const MouseMoveEvent& event) { mouseMove(panel, event); });
	hooks.scroll = panel.onScroll.connect([this, &panel](const ScrollEvent& event) {
		bubble(panel, "onScroll", {{"type", "scroll"}, {"dx", event.offset.x}, {"dy", event.offset.y}});
	});
	hooks.key = panel.onKey.connect([this, &panel](const KeyEvent& event) { key(panel, event); });

	if(auto* textArea = dynamic_cast<TextArea*>(&panel))
		hooks.value =
		    textArea->onTextChanged.connect([this, &panel](const std::string& value) { valueChanged(panel, value); });
	if(auto* editor = dynamic_cast<TextEditor*>(&panel)) {
		hooks.value =
		    editor->onTextChanged.connect([this, &panel](const std::string& value) { valueChanged(panel, value); });
		hooks.caret = editor->onCaretMoved.connect([this, &panel, editor](TextEditor::Position caret) {
			Record* record = recordOf(&panel);
			if(!record || !record->caret) return;
			try {
				versions.state()[format::statePointer(*record->caret, context(*record->locals, json::object()),
				                                      record->path + ".caret")] = {
				    {"line", caret.line + 1}, {"column", caret.column + 1}, {"selected", editor->selectedLength()}};
				changed();
				requestReconcile();
			} catch(const DocumentError& error) {
				report(error.what());
			}
		});
	}
	if(auto* toggle = dynamic_cast<Toggle*>(&panel))
		hooks.value = toggle->onToggled.connect([this, &panel](bool checked) { valueChanged(panel, checked); });
	if(auto* slider = dynamic_cast<Slider*>(&panel))
		hooks.value = slider->onValueChanged.connect([this, &panel](double value) {
			valueChanged(panel, std::trunc(value) == value ? json(static_cast<std::int64_t>(value)) : json(value));
		});
}

void Runtime::Impl::unhook(Panel& panel, const Record& record) {
	const Record::Hooks& hooks = record.hooks;
	panel.onHover.disconnect(hooks.hover);
	panel.onFocusChanged.disconnect(hooks.focus);
	panel.onMouseButton.disconnect(hooks.button);
	panel.onMouseMove.disconnect(hooks.move);
	panel.onScroll.disconnect(hooks.scroll);
	panel.onKey.disconnect(hooks.key);
	if(auto* textArea = dynamic_cast<TextArea*>(&panel)) textArea->onTextChanged.disconnect(hooks.value);
	if(auto* editor = dynamic_cast<TextEditor*>(&panel)) {
		editor->onTextChanged.disconnect(hooks.value);
		editor->onCaretMoved.disconnect(hooks.caret);
	}
	if(auto* toggle = dynamic_cast<Toggle*>(&panel)) toggle->onToggled.disconnect(hooks.value);
	if(auto* slider = dynamic_cast<Slider*>(&panel)) slider->onValueChanged.disconnect(hooks.value);
}

// Only the panels still in the window are reached: a panel the application removed itself is gone, and its record
// is never read again
void Runtime::Impl::disconnectAll() {
	if(!window) return;
	window->onResize.disconnect(windowHooks.resized);
	window->onKey.disconnect(windowHooks.key);
	window->onMouseButton.disconnect(windowHooks.button);
	std::vector<Panel*> pending{&window->root()};
	while(!pending.empty()) {
		Panel* panel = pending.back();
		pending.pop_back();
		if(const Record* record = recordOf(panel)) unhook(*panel, *record);
		for(const auto& child : panel->children()) pending.push_back(child.get());
	}
}

void Runtime::Impl::valueChanged(Panel& panel, const json& value) {
	Record* record = recordOf(&panel);
	if(!record) return;
	if(record->bind) {
		try {
			const json none = json::object();
			versions.state()[format::statePointer(*record->bind, context(*record->locals, none), record->path + ".bind")] =
			    value;
			changed();
			requestReconcile();
		} catch(const DocumentError& error) {
			report(error.what());
		}
	}
	fire(panel, "onChange", {{"type", "change"}, {"value", value}});
}

json Runtime::Impl::evaluateNode(const json& merged, const Scope& locals, const std::string& path) const {
	json result = json::object();
	for(const auto& [key, value] : merged.items()) {
		if(key == "children") continue; // built from the node itself (reconcileChildren), never copied
		result[key] =
		    structural(key) || key == "type" ? value : evaluate(value, locals, json::object(), path + "." + key);
	}
	return result;
}

json Runtime::Impl::instantiateComponent(const json& use, const std::string& path) const {
	const std::string name = string(use["use"], path + ".use");
	const json& document = versions.document();
	if(!document.contains("components") || !document["components"].contains(name))
		fail(path + ".use", "unknown component '" + name + "'");
	const json& component = object(document["components"][name], "components." + name);
	if(!component.contains("node")) fail("components." + name, "a component needs a \"node\"");

	json result = object(component["node"], "components." + name + ".node");
	for(const auto& [key, value] : use.items())
		if(key != "use" && key != "with" && key != "children") result[key] = value;

	// Parameters: the component's defaults, then what the use passes (evaluated in the caller's scope)
	json with = component.contains("params") ? object(component["params"], "components." + name + ".params")
	                                         : json::object();
	if(use.contains("with")) with.update(object(use["with"], path + ".with"));
	result["with"] = std::move(with);

	// The use's children go where the component has {"slot": true}
	if(use.contains("children")) {
		std::function<bool(json&)> fill = [&](json& node) {
			if(!node.is_object() || !node.contains("children") || !node["children"].is_array()) return false;
			json& children = node["children"];
			for(std::size_t i = 0; i < children.size(); ++i) {
				if(children[i].is_object() && children[i].value("slot", json(false)) == true) {
					json replaced = json::array();
					for(std::size_t j = 0; j < children.size(); ++j) {
						if(j == i) replaced.insert(replaced.end(), use["children"].begin(), use["children"].end());
						else replaced.push_back(children[j]);
					}
					children = std::move(replaced);
					return true;
				}
				if(!children[i].contains("use") && fill(children[i])) return true;
			}
			return false;
		};
		if(!fill(result)) result["children"] = use["children"];
	}
	return result;
}

// The new variables form one layer over the parent's scope; with and let see the ones before them
NodeRef Runtime::Impl::scoped(const NodeRef& shared, SharedScope& scope, const std::string& at) const {
	const json& node = *shared;
	const json none = json::object();
	Scope layer{scope.get(), nullptr, json::object()};
	if(node.contains("with"))
		for(const auto& [name, value] : object(node["with"], at + ".with").items())
			layer.vars[name] = evaluate(value, layer, none, at + ".with." + name);
	if(node.contains("let")) // in order: each sees the ones before it
		for(const auto& [name, value] : object(node["let"], at + ".let").items())
			layer.vars[name] = evaluate(value, layer, none, at + ".let." + name);
	if(node.contains("check")) {
		const json& check = object(node["check"], at + ".check");
		// The value checked: "value", else the node's bound value
		const json value = check.contains("value") ? evaluate(check["value"], layer, none, at + ".check.value")
		                   : node.contains("bind") && node["bind"].is_string()
		                       ? format::lookup(text(evaluate(node["bind"], layer, none, at + ".bind")),
		                                        context(layer, none), at + ".bind")
		                       : json();
		layer.vars["value"] = value;
		const bool valid = !check.contains("rule") || truthy(evaluate(check["rule"], layer, none, at + ".check.rule"));
		layer.vars["valid"] = valid;
		layer.vars["error"] =
		    valid ? json("") : evaluate(check.value("message", json("Invalid")), layer, none, at + ".check");
	}
	NodeRef result = shared;
	if(node.contains("style") && isDynamic(node["style"])) { // this instance's own copy, with its style
		json chosen = node;
		chosen["style"] = evaluate(node["style"], layer, none, at + ".style");
		if(chosen["style"].is_null() || chosen["style"] == "") chosen.erase("style");
		result = std::make_shared<const json>(withStyles(chosen, at));
	}
	if(!layer.vars.empty()) scope = extend(std::move(scope), std::move(layer.vars));
	return result;
}

// The children to build, after "use", "repeat", "with", "let", "check" and "when"
std::vector<Item> Runtime::Impl::expand(const json& children, const SharedScope& locals, const std::string& path) const {
	std::vector<Item> items;
	if(!children.is_array()) fail(path + ".children", "expected an array");

	for(std::size_t i = 0; i < children.size(); ++i) {
		const std::string at = path + ".children[" + std::to_string(i) + "]";
		const json& child = object(children[i], at);
		NodeRef shared;
		if(child.contains("use")) {
			json raw = child;
			for(int depth = 0; raw.contains("use"); ++depth) {
				if(depth >= maxComponentDepth) fail(at, "components nested too deeply (a component uses itself?)");
				raw = instantiateComponent(raw, at);
			}
			shared = std::make_shared<const json>(withStyles(raw, at));
		} else if(child.contains("style") && !isDynamic(child["style"])) {
			shared = std::make_shared<const json>(withStyles(child, at));
		} else {
			shared = borrow(child);
		}
		const json& merged = *shared;

		auto add = [&](SharedScope scope, const std::string& where, json itemKey) {
			NodeRef node = scoped(shared, scope, where);
			if(node->contains("when") &&
			   !format::truthy(evaluate((*node)["when"], *scope, json::object(), where + ".when")))
				return;
			items.push_back({std::move(node), std::move(scope), where, std::move(itemKey)});
		};

		if(!merged.contains("repeat")) {
			add(locals, at, nullptr);
			continue;
		}

		const json& repeat = object(merged["repeat"], at + ".repeat");
		const std::string name = repeat.contains("for") ? string(repeat["for"], at + ".repeat.for") : "item";
		const std::string indexName =
		    repeat.contains("index") ? string(repeat["index"], at + ".repeat.index") : "index";
		if(!repeat.contains("in")) fail(at + ".repeat", "needs \"in\": the list to repeat over");

		// "in" is a path ("state.messages") or any value/expression
		const json& source = repeat["in"];
		const json list = source.is_string() && source.get<std::string>().find("{{") == std::string::npos
		                      ? evaluate("{{" + source.get<std::string>() + "}}", *locals, json::object(), at + ".repeat.in")
		                      : evaluate(source, *locals, json::object(), at + ".repeat.in");
		if(list.is_null()) continue;
		if(!list.is_array()) fail(at + ".repeat.in", "expected a list, got " + list.dump());

		for(std::size_t k = 0; k < list.size(); ++k) {
			SharedScope scope = extend(locals, {{name, list[k]}, {indexName, k}});
			const std::string where = at + "[" + std::to_string(k) + "]";
			json itemKey = repeat.contains("key") ? evaluate(repeat["key"], *scope, json::object(), where + ".repeat.key")
			                                      : json();
			add(std::move(scope), where, std::move(itemKey));
		}
	}

	return items;
}

void Runtime::Impl::reconcileChildren(const json& node, Panel& panel, Record& record, const std::string& path) {
	std::vector<Item> items;
	if(record.type == "outlet") {
		// The current screen, filling the outlet unless it places itself
		const json& document = versions.document();
		if(document.contains("screens") && document["screens"].contains(screen)) {
			json screenNode = withStyles(document["screens"][screen], "screens." + screen);
			if(!hasLayout(screenNode)) screenNode["fill"] = 0;
			SharedScope scope = record.locals;
			NodeRef scopedNode = scoped(std::make_shared<const json>(std::move(screenNode)), scope, "screens." + screen);
			items.push_back({std::move(scopedNode), std::move(scope), "screens." + screen, nullptr});
		}
	} else if(node.contains("children")) {
		items = expand(node["children"], record.locals, path);
	} else if(record.managed == 0) {
		return; // nothing built here before: leave the panel's own children alone
	}

	std::size_t index = record.firstManaged;
	for(Item& item : items) {
		const json props = evaluateNode(*item.node, *item.locals, item.path);
		const std::string type = props.contains("type") ? string(props["type"], item.path + ".type") : "panel";

		// A keyed item finds its panel wherever it is now, so it keeps its focus, scroll and drag when lists reorder
		if(!item.key.is_null()) {
			for(std::size_t j = index; j < panel.children().size(); ++j) {
				const Record* r = recordOf(panel.children()[j].get());
				if(r && r->type == type && r->key == item.key) {
					if(j != index) panel.moveChild(j, index);
					break;
				}
			}
		}

		// Reuse the panel already in this place if it has the same type (and key): it keeps focus, hover and drag
		Panel* child = nullptr;
		bool created = false;
		if(index < panel.children().size()) {
			Panel& existing = *panel.children()[index];
			const Record* r = recordOf(&existing);
			if(r && r->type == type && r->key == item.key) {
				child = &existing;
			} else {
				forgetSubtree(existing);
				child = &panel.replaceChild(index, create(type, item.path));
				created = true;
			}
		} else {
			child = &panel.addChild(create(type, item.path));
			created = true;
		}

		Record& childRecord = created ? adopt(*child, type) : records.at(child);
		childRecord.locals = std::move(item.locals);
		childRecord.key = std::move(item.key);
		apply(*child, childRecord, props, item.path, false, created);
		reconcileChildren(*item.node, *child, childRecord, item.path);
		if(created && !handlersSuspended && childRecord.handlers.contains("onShow"))
			defer(later(childRecord.handlers["onShow"], *child, {{"type", "show"}}, childRecord.locals,
			            childRecord.path + ".onShow"));
		++index;
	}

	for(std::size_t i = index; i < panel.children().size(); ++i) forgetSubtree(*panel.children()[i]);
	panel.removeChildren(index);
	record.managed = index - record.firstManaged;
}

void Runtime::Impl::forgetSubtree(const Panel& panel) {
	if(Record* record = recordOf(&panel)) {
		if(window) {
			if(record->everyTimer) window->cancelTimer(record->everyTimer);
			for(const Tween* tween : record->tweens())
				if(tween->timer) window->cancelTimer(tween->timer);
			if(!handlersSuspended && record->handlers.contains("onHide"))
				defer(later(record->handlers["onHide"], window->root(), {{"type", "hide"}}, record->locals,
				            record->path + ".onHide"));
		}
		if(press.panel == &panel) press = {};
		if(press.source == &panel) press.source = nullptr;
		if(press.over == &panel) press.over = nullptr;
		if(lastClick.owner == &panel) lastClick.owner = nullptr;
		records.erase(&panel);
	}
	for(const auto& child : panel.children()) forgetSubtree(*child);
}

// Brings the UI in line with the document and the state
void Runtime::Impl::reconcile() {
	if(!window) return;
	reconcileQueued = false;
	stateChanged = false;
	const json& document = versions.document();

	// The screen an outlet shows: the first one, unless navigation chose another that still exists
	if(document.contains("screens") && document["screens"].is_object() && !document["screens"].empty() &&
	   !document["screens"].contains(screen)) {
		screen = document["screens"].begin().key();
		screenHistory.clear();
	}
	globalsChanged(); // window size, history, proposals, screen: refreshed once per update
	if(document.contains("theme")) {
		theme = text(evaluate(document["theme"], *noVariables(), json::object(), "theme"));
		globalsChanged();
	} else {
		theme.clear();
	}

	// Revisions can change the fonts: the default one is the window's
	if(const auto it = fonts.find("default"); it != fonts.end() && window->defaultFont() != it->second.get())
		window->setDefaultFont(it->second);

	Panel& root = window->root();
	if(!records.contains(&root)) adopt(root, "root");
	Record& record = records.at(&root);

	SharedScope scope = noVariables();
	const NodeRef node = scoped(std::make_shared<const json>(withStyles(document["root"], "root")), scope, "root");
	record.locals = std::move(scope);
	const json properties = evaluateNode(*node, *record.locals, "root");
	apply(root, record, properties, "root", true, false);
	reconcileChildren(*node, root, record, "root");

	// "watch": a handler runs after the update in which its value changed (not while handlers are suspended: the
	// values seen then are never compared, so a document shown for a moment changes nothing)
	if(document.contains("watch") && !handlersSuspended) {
		for(const auto& [path, handler] : object(document["watch"], "watch").items()) {
			const json value = format::lookup(path, context(*noVariables(), json::object()), "watch." + path);
			if(watched.contains(path) && watched[path] != value)
				defer(later(handler, root, {{"type", "change"}, {"value", value}, {"previous", watched[path]}},
				            noVariables(), "watch." + path));
			watched[path] = value;
		}
	}
	owner.onUpdated();
}

// Updates are batched: many state changes during one event become one reconcile, after the event is delivered
void Runtime::Impl::requestReconcile() {
	stateChanged = true;
	if(reconcileQueued || !window) return;
	reconcileQueued = true;
	defer([this] {
		if(!reconcileQueued) return;
		try {
			reconcile();
		} catch(const DocumentError& error) {
			report(error.what());
		}
	});
}

// A template built outside reconciliation (Runtime::instantiate)
Panel& Runtime::Impl::buildDetached(const json& node, Panel& parent, const std::string& path) {
	SharedScope scope = noVariables();
	const NodeRef scopedNode = scoped(std::make_shared<const json>(withStyles(node, path)), scope, path);
	const json props = evaluateNode(*scopedNode, *scope, path);
	const std::string type = props.contains("type") ? string(props["type"], path + ".type") : "panel";

	Panel& panel = parent.addChild(create(type, path));
	Record& record = adopt(panel, type);
	record.locals = std::move(scope);
	apply(panel, record, props, path, false, true);
	reconcileChildren(*scopedNode, panel, record, path);
	return panel;
}

} // namespace affresco::format
