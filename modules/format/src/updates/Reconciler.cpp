/// @file
/// @brief format::Reconciler: building panels and keeping them in line with the document.

#include "Reconciler.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <chrono>

namespace affresco::format {

std::unique_ptr<Panel> Reconciler::create(const std::string& type, const std::string& path) const {
	engine_.verify.created(path); // a full pass after a partial update creates none
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

std::vector<json> Reconciler::valuesOf(const Item& item, ReadSet& reads) const {
	const DocumentContext::ReadScope scope(engine_.document, &reads);
	++statistics_.nodesEvaluated;
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
	if(item.node->alwaysApply) reads.addAlways(AlwaysReason::Behaviour); // its behaviour reads more than its values
	if(reads.always) ++statistics_.alwaysReaders;
	if(const std::string& dropped = testing::droppedReads(); !dropped.empty() && panel.id() == dropped)
		reads.clear(); // a read dropped, as a bug would (test-only)
	record.valueReads = std::move(reads);
}

void Reconciler::listReconciled(Panel& panel, Record& record, ReadSet reads) {
	record.expandReads = std::move(reads);
	gatherReads(panel, record);
}

void Reconciler::gatherReads(const Panel& panel, Record& record) const {
	record.subtreeReads = record.valueReads;
	record.subtreeReads.add(record.expandReads);
	// A change inside the item it lies in reaches its handlers too (refreshItems())
	for(const Scope* scope = record.locals.get(); scope; scope = scope->parent) {
		if(scope->fromState.empty()) continue;
		record.subtreeReads.addPath(scope->origin());
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

// A kept panel's variables keep a copy of the items they lie in: a change inside one copies it again in place, so the
// panel's handlers, timers and those below it see it as it is now (bindings read it live while updating)
void Reconciler::refreshItems(const Scope& locals, const StateChanges& changes) const {
	for(const Scope* scope = &locals; scope; scope = scope->parent) {
		if(scope->fromState.empty() || std::ranges::none_of(changes.paths, [&](const std::string& path) {
			   return overlaps(scope->origin(), path);
		   }))
			continue;
		json value;
		if(scope->origin().starts_with(modelPlaces)) value = engine_.models.at(scope->origin());
		else if(const json* lying = findInState(engine_.versions.state(), json::json_pointer(scope->origin())))
			value = *lying;
		if(value.is_null()) continue;                     // gone: the list that shows it is built again
		scope->vars[scope->fromState] = std::move(value); // vars is mutable for this refresh
	}
}

void Reconciler::updatePanel(Panel& panel, Record& record, const StateChanges& changes, int depth) {
	++statistics_.panelsVisited;
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

void Reconciler::updateChildren(Panel& panel, Record& record, const StateChanges& changes, int depth) {
	if(record.expandReads.touchedBy(changes)) { // the list of children may differ: built again, keeping what it can
		children_.reconcileChildren(*record.node, panel, record, record.path, depth, &changes);
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

std::string_view Reconciler::fullReason(const StateChanges& changes, bool resized, std::string_view requested) const {
	const DocumentContext& context = engine_.document;
	if(!synced_) return "first";
	if(context.appearance() != syncedAppearance_) return "appearance";
	if(context.screen() != syncedScreen_) return "screen";
	if(changes.all) return resized ? "resize" : !requested.empty() ? requested : "too many changes";
	if(rootScopeReads_.touchedBy(changes)) return "requested"; // the root's own variables: everything below
	return {};
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
		++statistics_.panelsVisited;
		updateChildren(root, record, *changes, 0);
		return;
	}
	SharedScope scope = noVariables();
	rootScopeReads_.clear();
	CompiledRef node;
	{
		const DocumentContext::ReadScope reading(engine_.document, &rootScopeReads_);
		node = engine_.builder.scoped(engine_.builder.compiler().compiled(engine_.versions.document()["root"], "root"),
		                              scope, "root");
	}
	const Item item{node, scope, "root", nullptr};
	++statistics_.panelsVisited;
	record.locals = std::move(scope);
	ReadSet reads;
	std::vector<json> values = valuesOf(item, reads);
	applyValues(root, record, item, std::move(values), std::move(reads), false);
	children_.reconcileChildren(*node, root, record, "root");
}

// Brings the UI in line with the document and the state: everything, or only what reads the state changed
void Reconciler::update(bool full, std::string_view reason) {
	Window* window = engine_.window;
	if(!window) return;
	reconcileQueued_ = false;
	const auto start = std::chrono::steady_clock::now();
	statistics_ = {.updates = statistics_.updates, .milliseconds = statistics_.milliseconds};
	DocumentContext& context = engine_.document;
	const DocumentContext::BudgetScope budget(context); // an update has its own, unless an event runs it
	StateChanges changes = context.takeChanges();
	const std::string_view requested = context.takeFullReason();
	const bool resized = engine_.frame.begin(changes);

	Panel& root = window->root();
	if(!records_.contains(&root)) adopt(root, "root");
	Record& record = records_.at(&root);

	engine_.tracker.update(changes); // the readers outside the panels first: what they mark joins this update
	statistics_.fullReason = full ? reason : fullReason(changes, resized, requested);
	statistics_.full = !statistics_.fullReason.empty();
	const bool part = !statistics_.full;
	synced_ = false; // until the update ends without an error
	updateRoot(root, record, part ? &changes : nullptr);
	synced_ = true;
	syncedAppearance_ = context.appearance();
	syncedScreen_ = context.screen();

	++statistics_.updates;
	statistics_.milliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	if(part && engine_.verify.on()) engine_.verify.check(root, [&] { updateRoot(root, record, nullptr); });
	engine_.frame.end(changes, !part);
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

void Reconciler::requestReconcile(std::string_view reason) {
	engine_.document.requestUpdate(reason);
	queue();
}

void Reconciler::requestStateUpdate() { queue(); }

// A template built outside reconciliation (Runtime::instantiate)
Panel& Reconciler::buildDetached(const json& node, Panel& parent, const std::string& path) {
	SharedScope scope = noVariables();
	const CompiledRef scopedNode = engine_.builder.scoped(engine_.builder.compiler().compiled(node, path), scope, path);
	const Item item{scopedNode, scope, path, nullptr};
	ReadSet reads;
	std::vector<json> values = valuesOf(item, reads);
	const std::string& type = ChildList::typeOf(*scopedNode, path);

	Panel& panel = parent.addChild(create(type, path));
	Record& record = adopt(panel, type);
	record.locals = std::move(scope);
	applyValues(panel, record, item, std::move(values), std::move(reads), true);
	children_.reconcileChildren(*scopedNode, panel, record, path);
	synced_ = false; // its parent's reads don't include it: the next update visits everything
	return panel;
}

} // namespace affresco::format
