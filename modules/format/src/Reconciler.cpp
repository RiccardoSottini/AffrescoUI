/// @file
/// @brief format::Reconciler: building panels and keeping them in line with the document.

#include "Reconciler.hpp"
#include "Engine.hpp"

namespace affresco::format {

namespace {

/// @brief A node's type, which must be text.
/// @param node The compiled node.
/// @param path Its place, for errors.
/// @return The type.
/// @throws DocumentError If it isn't text.
const std::string& typeOf(const CompiledNode& node, const std::string& path) {
	if(!node.typeIsText) static_cast<void>(string(node.own["type"], path + ".type"));
	return node.type;
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

std::optional<std::vector<Item>> Reconciler::childrenOf(const CompiledNode& node, const Record& record,
                                                        const std::string& path) const {
	if(record.type == "outlet") {
		// The current screen, filling the outlet unless it places itself
		std::vector<Item> items;
		const json& document = engine_.versions.document();
		const std::string& screen = engine_.document.screen();
		if(document.contains("screens") && document["screens"].contains(screen)) {
			std::string at = "screens." + screen;
			SharedScope scope = record.locals;
			CompiledRef scopedNode =
			    engine_.builder.scoped(engine_.builder.compiled(document["screens"][screen], at, true), scope, at);
			items.push_back({std::move(scopedNode), std::move(scope), std::move(at), nullptr});
		}
		return items;
	}
	if(const json* children = node.children()) return engine_.builder.expand(*children, record.locals, path);
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

void Reconciler::reconcileChildren(const CompiledNode& node, Panel& panel, Record& record, const std::string& path,
                                   int depth) {
	if(depth >= maxNodeDepth)
		fail(path, "nodes nested too deeply (more than " + std::to_string(maxNodeDepth) +
		               " levels: a component or a screen inside itself?)");
	std::optional<std::vector<Item>> items = childrenOf(node, record, path);
	if(!items) return;

	std::size_t index = record.firstManaged;
	for(Item& item : *items) {
		std::vector<json> values = engine_.builder.values(*item.node, *item.locals, item.path);
		const std::string& type = typeOf(*item.node, item.path);
		const auto [child, created] = place(panel, index, item, type);

		Record& childRecord = created ? adopt(*child, type) : records_.at(child);
		childRecord.locals = std::move(item.locals);
		childRecord.key = std::move(item.key);
		engine_.properties.apply(*child, childRecord, item.node, std::move(values), item.path, false, created);
		reconcileChildren(*item.node, *child, childRecord, item.path, depth + 1);
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
	const CompiledRef node = engine_.builder.scoped(engine_.builder.compiled(document["root"], "root"), scope, "root");
	record.locals = std::move(scope);
	engine_.properties.apply(root, record, node, engine_.builder.values(*node, *record.locals, "root"), "root", true,
	                         false);
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
	const CompiledRef scopedNode = engine_.builder.scoped(engine_.builder.compiled(node, path), scope, path);
	std::vector<json> values = engine_.builder.values(*scopedNode, *scope, path);
	const std::string& type = typeOf(*scopedNode, path);

	Panel& panel = parent.addChild(create(type, path));
	Record& record = adopt(panel, type);
	record.locals = std::move(scope);
	engine_.properties.apply(panel, record, scopedNode, std::move(values), path, false, true);
	reconcileChildren(*scopedNode, panel, record, path);
	return panel;
}

} // namespace affresco::format
