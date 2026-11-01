/// @file
/// @brief format::Reconciler: building panels and keeping them in line with the document.

#include "Reconciler.hpp"
#include "Engine.hpp"
#include "Contributions.hpp"
#include "PropertyValues.hpp"

#include "affresco/core/Assets.hpp"

#include <algorithm>
#include <chrono>
#include <format>

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

/// @brief Whether two scopes hold the same variables with the same values, all the way out.
/// @param a A scope.
/// @param b Another.
/// @return true if every variable reads the same in both.
bool sameScope(const Scope* a, const Scope* b) {
	for(; a && b; a = a->parent, b = b->parent) {
		if(a == b) return true;
		if(a->vars != b->vars) return false;
	}
	return a == b;
}

/// @brief Gives the window the ring of "window.focusRing" (a colour, { "color", "width", "offset" }, or false; the
/// default ring when it is left out), if it changed. Errors are reported, and leave the ring as it was.
/// @param engine The runtime, with a window.
void showFocusRing(Engine& engine) {
	if(!engine.window) return;
	const json& document = engine.versions.document();
	std::optional<FocusRing> ring = FocusRing{}; // on unless the document says otherwise
	try {
		if(document.contains("window") && document["window"].is_object() && document["window"].contains("focusRing")) {
			ring.reset();
			const json& value = document["window"]["focusRing"];
			const std::string at = "window.focusRing";
			if(value.is_object()) {
				ring.emplace();
				for(const auto& [key, item] : value.items()) {
					if(key == "color") ring->color = engine.document.color(item, at + ".color");
					else if(key == "width") ring->width = pixels(item, at + ".width");
					else if(key == "offset") ring->offset = pixels(item, at + ".offset");
					else fail(std::format("{}.{}", at, key), "unknown property");
				}
			} else if(!value.is_boolean() || value.get<bool>())
				ring = FocusRing{.color = engine.document.color(value, at)};
		}
	} catch(const DocumentError& error) {
		engine.report(error.what());
		return;
	}
	if(ring != engine.window->focusRing()) engine.window->setFocusRing(ring);
}

/// @brief Gives the window the icon of "window.icon" (none when it is left out), if it changed: a file under the rules
/// of an image node's (decoded on the worker thread; one that can't be read shows none), or an asset.
/// @param engine The runtime, with a window.
/// @throws DocumentError If it isn't text, or names a file outside the allowed folders or an asset that isn't a
/// picture: the icon stays as it was.
void showIcon(Engine& engine) {
	const json& document = engine.versions.document();
	const auto settings = document.find("window");
	json value = settings != document.end() && settings->is_object() ? settings->value("icon", json()) : json();
	if(value == engine.icon) return;
	const std::string at = "window.icon";
	if(value.is_null()) {
		engine.iconFile.cancel();
		engine.window->setIcon(nullptr);
	} else if(assetReference(value)) { // decoded once, by its bytes
		std::shared_ptr<const Bitmap> image = engine.document.images().named(engine.document.assetImage(value, at));
		engine.iconFile.cancel();
		engine.window->setIcon(std::move(image));
	} else {
		const std::filesystem::path file = engine.document.imageFile(value, at);
		engine.iconFile = engine.document.images().load(file, [&engine](const std::shared_ptr<const Bitmap>& image) {
			if(engine.window) engine.window->setIcon(image);
		});
	}
	engine.icon = std::move(value);
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
	std::optional<std::vector<Item>> items;
	if(node.children) items = engine_.builder.expand(*node.children, record.locals, path);
	else if(record.managed > 0) items.emplace(); // else nothing built here before: leave the panel's own children alone
	if(record.type == "root" && path == "root") addOverlays(record, items);
	return items;
}

// After the root's children, so above the overlays inside them; keyed, so they stay when the root loses a child
void Reconciler::addOverlays(const Record& record, std::optional<std::vector<Item>>& items) const {
	const json& document = engine_.versions.document();
	const auto overlays = document.find("overlays");
	if(overlays == document.end() || !overlays->is_object() || overlays->empty()) return;
	if(!items) items.emplace();
	for(const auto& [name, node] : overlays->items()) {
		std::string at = "overlays." + name;
		SharedScope scope = record.locals;
		CompiledRef scoped = engine_.builder.scoped(engine_.builder.compiledOverlay(node, at), scope, at);
		if(scoped->when && // as a child's: not built while it doesn't hold
		   !format::truthy(scoped->when->evaluate(engine_.document.context(*scope, emptyObject()), at, "when")))
			continue;
		items->push_back({std::move(scoped), std::move(scope), at, json(at)});
	}
}

Panel* Reconciler::reusable(Panel& panel, std::size_t index, const Item& item, const std::string& type) {
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
		const Record* r = recordOf(panel.children()[index].get());
		if(r && r->type == type && r->key == item.key) return panel.children()[index].get();
	}
	return nullptr;
}

Panel& Reconciler::make(Panel& panel, std::size_t index, const Item& item, const std::string& type) {
	if(records_.size() >= maxPanels)
		fail(item.path,
		     std::format("more than {} panels (components or repeats inside each other multiply)", maxPanels));
	if(index < panel.children().size()) {
		forgetSubtree(*panel.children()[index]);
		++statistics_.panelsCreated;
		return panel.replaceChild(index, create(type, item.path));
	}
	++statistics_.panelsCreated;
	return panel.addChild(create(type, item.path));
}

std::vector<json> Reconciler::valuesOf(const Item& item, ReadSet& reads) const {
	const DocumentContext::ReadScope scope(engine_.document, &reads);
	return engine_.builder.values(*item.node, *item.locals, item.path);
}

void Reconciler::applyValues(Panel& panel, Record& record, const Item& item, std::vector<json> values, ReadSet reads,
                             bool created) {
	const bool isRoot = engine_.window && &panel == &engine_.window->root();
	++statistics_.panelsApplied;
	{
		const DocumentContext::ReadScope scope(engine_.document, &reads); // the bound value, and what behaviour reads
		engine_.properties.apply(panel, record, item.node, std::move(values), item.path, isRoot, created);
	}
	if(item.node->alwaysApply) reads.always = true; // its behaviour reads more than its values
	record.valueReads = std::move(reads);
}

void Reconciler::gatherReads(const Panel& panel, Record& record) const {
	record.subtreeReads = record.valueReads;
	record.subtreeReads.add(record.expandReads);
	// A change inside the item it lies in reaches its handlers too (refreshItems())
	for(const Scope* scope = record.locals.get(); scope; scope = scope->parent) {
		if(scope->fromState.empty()) continue;
		record.subtreeReads.addPath(scope->origin);
		break;
	}
	const auto& children = panel.children();
	if(record.managed >= ItemIndex::minChildren) record.items.start(children.size());
	else record.items.drop();
	for(std::size_t i = record.firstManaged; i < record.firstManaged + record.managed && i < children.size(); ++i) {
		const auto it = records_.find(children[i].get());
		if(it == records_.end()) continue;
		record.subtreeReads.add(it->second.subtreeReads);
		record.items.add(i, it->second.locals.get(), it->second.subtreeReads);
	}
}

bool Reconciler::updateKept(Panel& kept, const Item& item, const StateChanges& changes, int depth) {
	Record& record = records_.at(&kept);
	if(record.node != item.node || record.path != item.path || !sameScope(record.locals.get(), item.locals.get()))
		return false; // other values may be read: built again
	updatePanel(kept, record, changes, depth);
	return true;
}

// A kept panel's variables keep a copy of the items they lie in: a change inside one copies it again in place, so the
// panel's handlers, timers and those below it see it as it is now (bindings read it live while updating)
void Reconciler::refreshItems(const Scope& locals, const StateChanges& changes) const {
	for(const Scope* scope = &locals; scope; scope = scope->parent) {
		if(scope->fromState.empty() ||
		   std::ranges::none_of(changes.paths, [&](const std::string& path) { return overlaps(scope->origin, path); }))
			continue;
		json value;
		if(scope->origin.starts_with(modelPlaces)) value = engine_.models.at(scope->origin);
		else if(const json* lying = findInState(engine_.versions.state(), json::json_pointer(scope->origin)))
			value = *lying;
		if(value.is_null()) continue;                     // gone: the list that shows it is built again
		scope->vars[scope->fromState] = std::move(value); // vars is mutable for this refresh
	}
}

void Reconciler::updatePanel(Panel& panel, Record& record, const StateChanges& changes, int depth) {
	if(!record.subtreeReads.touchedBy(changes)) return;
	refreshItems(*record.locals, changes);
	if(record.valueReads.touchedBy(changes)) {
		const Item own{record.node, record.locals, record.path, nullptr};
		ReadSet reads;
		std::vector<json> values = valuesOf(own, reads);
		applyValues(panel, record, own, std::move(values), std::move(reads), false);
		record.subtreeReads.add(record.valueReads);
	}
	updateChildren(panel, record, changes, depth);
}

void Reconciler::reconcileChild(Panel& panel, std::size_t index, Item& item, int depth, const StateChanges* changes) {
	const std::string& type = typeOf(*item.node, item.path);
	Panel* kept = reusable(panel, index, item, type);
	if(kept && changes && updateKept(*kept, item, *changes, depth)) return;

	ReadSet reads;
	std::vector<json> values = valuesOf(item, reads); // before a panel is made: an error leaves none half built
	const bool created = !kept;
	Panel& child = created ? make(panel, index, item, type) : *kept;
	Record& childRecord = created ? adopt(child, type) : records_.at(&child);
	childRecord.locals = std::move(item.locals);
	childRecord.key = std::move(item.key);
	applyValues(child, childRecord, item, std::move(values), std::move(reads), created);
	reconcileChildren(*item.node, child, childRecord, item.path, depth); // other variables: everything below
	if(created && !engine_.handlersSuspended && childRecord.handlers.contains("onShow"))
		engine_.scheduler.defer(engine_.runner.later(childRecord.handlers["onShow"], child, {{"type", "show"}},
		                                             childRecord.locals, childRecord.path + ".onShow"));
}

void Reconciler::reconcileChildren(const CompiledNode& node, Panel& panel, Record& record, const std::string& path,
                                   int depth, const StateChanges* changes) {
	if(depth >= maxNodeDepth)
		fail(path, "nodes nested too deeply (more than " + std::to_string(maxNodeDepth) +
		               " levels: a component or a screen inside itself?)");
	record.expandReads.clear();
	std::optional<std::vector<Item>> items;
	{
		const DocumentContext::ReadScope scope(engine_.document, &record.expandReads);
		items = childrenOf(node, record, path);
	}
	if(items) {
		std::size_t index = record.firstManaged;
		for(Item& item : *items) reconcileChild(panel, index++, item, depth + 1, changes);
		for(std::size_t i = index; i < panel.children().size(); ++i) forgetSubtree(*panel.children()[i]);
		panel.removeChildren(index);
		record.managed = index - record.firstManaged;
	}
	gatherReads(panel, record);
}

void Reconciler::updateChildren(Panel& panel, Record& record, const StateChanges& changes, int depth) {
	if(record.expandReads.touchedBy(changes)) { // the list of children may differ: built again, keeping what it can
		reconcileChildren(*record.node, panel, record, record.path, depth, &changes);
		return;
	}
	const auto& children = panel.children();
	const std::size_t end = std::min(record.firstManaged + record.managed, children.size());
	const auto visit = [&](std::size_t i) {
		Panel& child = *children[i];
		Record* childRecord = recordOf(&child);
		if(!childRecord || !childRecord->subtreeReads.touchedBy(changes)) return;
		updatePanel(child, *childRecord, changes, depth + 1);
		record.subtreeReads.add(childRecord->subtreeReads); // what it reads now: a set that only grows stays right
		if(!record.items.keeps(childRecord->locals.get(), childRecord->subtreeReads)) record.items.drop();
	};
	if(const auto chosen = record.items.visits(changes, children.size())) {
		for(const std::size_t i : *chosen)
			if(i >= record.firstManaged && i < end) visit(i);
		return;
	}
	for(std::size_t i = record.firstManaged; i < end; ++i) visit(i);
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
		engine_.serviceCalls.forgetSender(panel);
		if(!record->snapshotImage.empty()) engine_.document.images().setNamed(record->snapshotImage, nullptr);
		records_.erase(&panel);
	}
	for(const auto& child : panel.children()) forgetSubtree(*child);
}

bool Reconciler::canUpdatePart(const StateChanges& changes) const {
	const DocumentContext& context = engine_.document;
	return synced_ && !changes.all && context.appearance() == syncedAppearance_ && context.screen() == syncedScreen_ &&
	       !rootScopeReads_.touchedBy(changes);
}

void Reconciler::updateRoot(Panel& root, Record& record, const StateChanges* changes) {
	if(changes) { // the root's node and variables are the same: only what reads the changes
		if(record.valueReads.touchedBy(*changes)) {
			const Item item{record.node, record.locals, "root", nullptr};
			ReadSet reads;
			std::vector<json> values = valuesOf(item, reads);
			applyValues(root, record, item, std::move(values), std::move(reads), false);
			record.subtreeReads.add(record.valueReads);
		}
		updateChildren(root, record, *changes, 0);
		return;
	}
	SharedScope scope = noVariables();
	rootScopeReads_.clear();
	CompiledRef node;
	{
		const DocumentContext::ReadScope reading(engine_.document, &rootScopeReads_);
		node = engine_.builder.scoped(engine_.builder.compiled(engine_.versions.document()["root"], "root"), scope,
		                              "root");
	}
	const Item item{node, scope, "root", nullptr};
	record.locals = std::move(scope);
	ReadSet reads;
	std::vector<json> values = valuesOf(item, reads);
	applyValues(root, record, item, std::move(values), std::move(reads), false);
	reconcileChildren(*node, root, record, "root");
}

// Brings the UI in line with the document and the state: everything, or only what reads the state changed
void Reconciler::update(bool full) {
	Window* window = engine_.window;
	if(!window) return;
	reconcileQueued_ = false;
	const auto start = std::chrono::steady_clock::now();
	statistics_.panelsApplied = statistics_.panelsCreated = 0;
	DocumentContext& context = engine_.document;
	const DocumentContext::BudgetScope budget(context); // an update has its own, unless an event runs it
	const StateChanges changes = context.takeChanges();
	const json& document = engine_.versions.document();

	// The screen an outlet shows: the first one, unless navigation chose another that still exists
	context.keepScreen(document);
	context.globalsChanged(); // window size, history, proposals, screen: refreshed once per update
	// Computed values are cached per state version, and may read the window: a resize makes them stale too
	std::string windowNow = std::to_string(window->width()) + "x" + std::to_string(window->height()) + "@" +
	                        std::to_string(window->contentScale()) +
	                        (window->colorScheme() == ColorScheme::Dark ? "d" : "l");
	if(windowNow != syncedWindow_) {
		if(!syncedWindow_.empty()) context.bumpStateVersion();
		syncedWindow_ = std::move(windowNow);
	}
	context.updateTheme();
	context.updateLocale();

	// Revisions can change the fonts: the default one is the window's
	if(const auto font = context.findFont("default"); font && window->defaultFont() != font.get())
		window->setDefaultFont(font);
	showIcon(engine_); // and the icon: a file outside the allowed folders refuses the revision, as an image node's does

	Panel& root = window->root();
	if(!records_.contains(&root)) adopt(root, "root");
	Record& record = records_.at(&root);

	const bool part = !full && canUpdatePart(changes);
	synced_ = false; // until the update ends without an error
	updateRoot(root, record, part ? &changes : nullptr);
	synced_ = true;
	syncedAppearance_ = context.appearance();
	syncedScreen_ = context.screen();

	++statistics_.updates;
	statistics_.milliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

	if(!engine_.handlersSuspended) watch(root);
	showWindowTitle(engine_);
	showMenuBar(engine_);
	showFocusRing(engine_);
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

// Updates are batched: many state changes during one event become one update, after the event is delivered
void Reconciler::queue() {
	if(reconcileQueued_ || !engine_.window) return;
	reconcileQueued_ = true;
	engine_.scheduler.defer([this] {
		if(!reconcileQueued_) return;
		try {
			update(false);
		} catch(const DocumentError& error) {
			engine_.report(error.what());
		}
	});
}

void Reconciler::requestReconcile() {
	engine_.document.requestUpdate();
	queue();
}

void Reconciler::requestStateUpdate() { queue(); }

// A template built outside reconciliation (Runtime::instantiate)
Panel& Reconciler::buildDetached(const json& node, Panel& parent, const std::string& path) {
	SharedScope scope = noVariables();
	const CompiledRef scopedNode = engine_.builder.scoped(engine_.builder.compiled(node, path), scope, path);
	const Item item{scopedNode, scope, path, nullptr};
	ReadSet reads;
	std::vector<json> values = valuesOf(item, reads);
	const std::string& type = typeOf(*scopedNode, path);

	Panel& panel = parent.addChild(create(type, path));
	Record& record = adopt(panel, type);
	record.locals = std::move(scope);
	applyValues(panel, record, item, std::move(values), std::move(reads), true);
	reconcileChildren(*scopedNode, panel, record, path);
	synced_ = false; // its parent's reads don't include it: the next update visits everything
	return panel;
}

} // namespace affresco::format
