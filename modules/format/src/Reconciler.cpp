/// @file
/// @brief format::Reconciler: building panels and keeping them in line with the document.

#include "Reconciler.hpp"
#include "Engine.hpp"

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

std::unique_ptr<Panel> Reconciler::create(const std::string& type, const std::string& path) const {
	return engine_.types.create(type, path, engine_.placeholderTypes);
}

// Starts tracking a newly created panel, remembering its defaults so they can be restored later
Record& Reconciler::adopt(Panel& panel, const std::string& type) {
	Record& record = records_[&panel];
	record.type = type;
	record.sets = engine_.types.setsFor(engine_.types.find(type), panel);
	record.firstManaged = panel.children().size();
	record.defaultColor = panel.color();
	record.defaultCursor = panel.cursor();
	engine_.events.hookUp(panel, record);
	return record;
}

std::optional<std::vector<Item>> Reconciler::childrenOf(const json& node, const Record& record,
                                                        const std::string& path) const {
	if(record.type == "outlet") {
		// The current screen, filling the outlet unless it places itself
		std::vector<Item> items;
		const json& document = engine_.versions.document();
		const std::string& screen = engine_.document.screen();
		if(document.contains("screens") && document["screens"].contains(screen)) {
			json screenNode = engine_.builder.withStyles(document["screens"][screen], "screens." + screen);
			if(!hasLayout(screenNode)) screenNode["fill"] = 0;
			SharedScope scope = record.locals;
			NodeRef scopedNode =
			    engine_.builder.scoped(std::make_shared<const json>(std::move(screenNode)), scope, "screens." + screen);
			items.push_back({std::move(scopedNode), std::move(scope), "screens." + screen, nullptr});
		}
		return items;
	}
	if(node.contains("children")) return engine_.builder.expand(node["children"], record.locals, path);
	if(record.managed == 0) return std::nullopt; // nothing built here before: leave the panel's own children alone
	return std::vector<Item>{};
}

std::pair<Panel*, bool> Reconciler::place(Panel& panel, std::size_t index, const Item& item, const std::string& type) {
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
	if(index < panel.children().size()) {
		Panel& existing = *panel.children()[index];
		const Record* r = recordOf(&existing);
		if(r && r->type == type && r->key == item.key) return {&existing, false};
		forgetSubtree(existing);
		return {&panel.replaceChild(index, create(type, item.path)), true};
	}
	return {&panel.addChild(create(type, item.path)), true};
}

void Reconciler::reconcileChildren(const json& node, Panel& panel, Record& record, const std::string& path) {
	std::optional<std::vector<Item>> items = childrenOf(node, record, path);
	if(!items) return;

	std::size_t index = record.firstManaged;
	for(Item& item : *items) {
		const json props = engine_.builder.evaluateNode(*item.node, *item.locals, item.path);
		const std::string type = props.contains("type") ? string(props["type"], item.path + ".type") : "panel";
		const auto [child, created] = place(panel, index, item, type);

		Record& childRecord = created ? adopt(*child, type) : records_.at(child);
		childRecord.locals = std::move(item.locals);
		childRecord.key = std::move(item.key);
		engine_.properties.apply(*child, childRecord, props, item.path, false, created);
		reconcileChildren(*item.node, *child, childRecord, item.path);
		if(created && !engine_.handlersSuspended && childRecord.handlers.contains("onShow"))
			engine_.scheduler.defer(engine_.runner.later(childRecord.handlers["onShow"], *child, {{"type", "show"}},
			                                             childRecord.locals, childRecord.path + ".onShow"));
		++index;
	}

	for(std::size_t i = index; i < panel.children().size(); ++i) forgetSubtree(*panel.children()[i]);
	panel.removeChildren(index);
	record.managed = index - record.firstManaged;
}

void Reconciler::forgetSubtree(const Panel& panel) {
	if(Record* record = recordOf(&panel)) {
		if(Window* window = engine_.window) {
			engine_.scheduler.cancel(*record);
			if(!engine_.handlersSuspended && record->handlers.contains("onHide"))
				engine_.scheduler.defer(engine_.runner.later(record->handlers["onHide"], window->root(),
				                                             {{"type", "hide"}}, record->locals,
				                                             record->path + ".onHide"));
		}
		engine_.events.forget(panel);
		records_.erase(&panel);
	}
	for(const auto& child : panel.children()) forgetSubtree(*child);
}

// Brings the UI in line with the document and the state
void Reconciler::reconcile() {
	Window* window = engine_.window;
	if(!window) return;
	reconcileQueued_ = false;
	DocumentContext& context = engine_.document;
	context.clearStateChanged();
	const json& document = engine_.versions.document();

	// The screen an outlet shows: the first one, unless navigation chose another that still exists
	context.keepScreen(document);
	context.globalsChanged(); // window size, history, proposals, screen: refreshed once per update
	context.updateTheme();

	// Revisions can change the fonts: the default one is the window's
	if(const auto font = context.findFont("default"); font && window->defaultFont() != font.get())
		window->setDefaultFont(font);

	Panel& root = window->root();
	if(!records_.contains(&root)) adopt(root, "root");
	Record& record = records_.at(&root);

	SharedScope scope = noVariables();
	const NodeRef node = engine_.builder.scoped(
	    std::make_shared<const json>(engine_.builder.withStyles(document["root"], "root")), scope, "root");
	record.locals = std::move(scope);
	const json properties = engine_.builder.evaluateNode(*node, *record.locals, "root");
	engine_.properties.apply(root, record, properties, "root", true, false);
	reconcileChildren(*node, root, record, "root");

	if(!engine_.handlersSuspended) watch(root);
	engine_.owner.onUpdated();
}

// "watch": a handler runs after the update in which its value changed. Not while handlers are suspended: the values
// seen then are never compared, so a document shown for a moment changes nothing.
void Reconciler::watch(Panel& root) {
	const json& document = engine_.versions.document();
	if(!document.contains("watch")) return;
	for(const auto& [path, handler] : object(document["watch"], "watch").items()) {
		const json value =
		    format::lookup(path, engine_.document.context(*noVariables(), json::object()), "watch." + path);
		if(watched_.contains(path) && watched_[path] != value)
			engine_.scheduler.defer(engine_.runner.later(
			    handler, root, {{"type", "change"}, {"value", value}, {"previous", watched_[path]}}, noVariables(),
			    "watch." + path));
		watched_[path] = value;
	}
}

// Updates are batched: many state changes during one event become one reconcile, after the event is delivered
void Reconciler::requestReconcile() {
	engine_.document.requestUpdate();
	if(reconcileQueued_ || !engine_.window) return;
	reconcileQueued_ = true;
	engine_.scheduler.defer([this] {
		if(!reconcileQueued_) return;
		try {
			reconcile();
		} catch(const DocumentError& error) {
			engine_.report(error.what());
		}
	});
}

// A template built outside reconciliation (Runtime::instantiate)
Panel& Reconciler::buildDetached(const json& node, Panel& parent, const std::string& path) {
	SharedScope scope = noVariables();
	const NodeRef scopedNode =
	    engine_.builder.scoped(std::make_shared<const json>(engine_.builder.withStyles(node, path)), scope, path);
	const json props = engine_.builder.evaluateNode(*scopedNode, *scope, path);
	const std::string type = props.contains("type") ? string(props["type"], path + ".type") : "panel";

	Panel& panel = parent.addChild(create(type, path));
	Record& record = adopt(panel, type);
	record.locals = std::move(scope);
	engine_.properties.apply(panel, record, props, path, false, true);
	reconcileChildren(*scopedNode, panel, record, path);
	return panel;
}

} // namespace affresco::format
