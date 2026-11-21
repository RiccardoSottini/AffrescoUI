/// @file
/// @brief format::Reconciler: building panels and keeping them in line with the document.

#include "Reconciler.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <string_view>
#include <utility>

namespace affresco::format {

namespace {

/// @brief Writes what panels read of the items of the rows they lie in under the rows' keys (rowKey), for the index:
/// an insert or a removal before a row numbers those reads again in place (Tracker::shift()). A row's number is read
/// under its row's key too, so that only the rows numbered again run.
/// @param locals The panels' variables, the rows' scopes among them (innermost first).
/// @param reads What they read: places of the state, rewritten.
void rowRelative(const Scope* locals, ReadSet& reads) {
	for(const Scope* scope = locals; scope; scope = scope->parent) {
		if(scope->place.row == ItemPlace::noRow) continue;
		const std::string& origin = scope->origin();
		const std::string list = scope->listPlace();
		const std::string number = std::to_string(scope->place.row);
		for(std::string& path : reads.paths) {
			if(path.size() == list.size() + 1U + indexKey.size() && path.starts_with(list) &&
			   std::string_view(path).substr(list.size() + 1U) == indexKey)
				path.insert(list.size() + 1U, std::string(rowKey) + number + '/');
			else if(path.starts_with(origin) && (path.size() == origin.size() || path[origin.size()] == '/'))
				path.insert(list.size() + 1U, rowKey);
		}
	}
}

} // namespace

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
	addReaders(panel, record);
	engine_.events.hookUp(panel, record);
	return record;
}

void Reconciler::addReaders(Panel& panel, Record& record) {
	for(PanelReader* reader : {&record.itemsReader, &record.valuesReader, &record.listReader}) {
		reader->reconciler = this;
		reader->panel = &panel;
	}
	record.valuesReader.id = engine_.tracker.add(record.valuesReader);
	record.listReader.id = engine_.tracker.add(record.listReader);
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
	readItems(panel, record);
	{
		const DocumentContext::ReadScope scope(engine_.document, &reads); // the bound value, and what behaviour reads
		engine_.properties.apply(panel, record, item.node, std::move(values), item.path, isRoot, created);
	}
	// A node whose behaviour reads more than its values (an overlay's "open" path, a bound path) is applied whole each
	// time it runs (Properties::apply()), which records those reads: it runs when they change, not on every update
	if(reads.always) ++statistics_.alwaysReaders;
	if(const std::string& dropped = testing::droppedReads(); !dropped.empty() && panel.id() == dropped)
		reads.clear(); // a read dropped, as a bug would (test-only)
	rowRelative(item.locals.get(), reads);
	engine_.tracker.setReads(record.valuesReader.id, std::move(reads));
}

void Reconciler::listReconciled(Panel& /*panel*/, Record& record, ReadSet reads) {
	rowRelative(record.locals.get(), reads);
	if(record.node) // its node's children in the document: a revision that changes them runs it again
		if(const std::string place = childrenPlace(record.node->serial); !place.empty()) reads.addPath(place);
	engine_.tracker.setReads(record.listReader.id, std::move(reads));
}

const Scope* Reconciler::parentLocals(const Panel& panel) {
	const Record* parent = panel.parent() ? recordOf(panel.parent()) : nullptr;
	return parent ? parent->locals.get() : nullptr;
}

// A change inside the item a panel's variables copy reaches its handlers too (refreshItems()): the copy is read
void Reconciler::readItems(const Panel& panel, Record& record) {
	if(record.itemsOf == record.locals.get()) return;
	record.itemsOf = record.locals.get();
	ReadSet reads;
	const Scope* stop = parentLocals(panel);
	for(const Scope* scope = record.locals.get(); scope && scope != stop; scope = scope->parent)
		if(!scope->fromState.empty()) reads.addPath(scope->origin());
	Tracker& tracker = engine_.tracker;
	if(reads.paths.empty()) {
		tracker.remove(std::exchange(record.itemsReader.id, 0));
		return;
	}
	rowRelative(record.locals.get(), reads);
	if(record.itemsReader.id == 0) record.itemsReader.id = tracker.add(record.itemsReader);
	tracker.setReads(record.itemsReader.id, std::move(reads)); // copied with these variables: up to date
}

// A kept panel's variables keep a copy of the items they lie in: a change inside one copies it again in place, so the
// panel's handlers, timers and those below it see it as it is now (bindings read it live while updating)
void Reconciler::refreshItems(const Panel& panel, const Record& record, const StateChanges& changes) {
	const Scope* stop = parentLocals(panel);
	for(const Scope* scope = record.locals.get(); scope && scope != stop; scope = scope->parent) {
		if(scope->fromState.empty()) continue;
		for(const std::string& written : changes.paths)
			if(overlaps(scope->origin(), written) && !refreshItem(*scope, written)) break;
	}
}

bool Reconciler::refreshItem(const Scope& scope, const std::string& written) const {
	const std::string& origin = scope.origin();
	const json& state = engine_.versions.state();
	json& copy = scope.vars[scope.fromState]; // vars is mutable for this refresh
	const bool inside = !origin.starts_with(modelPlaces) && contains(origin, written) && written.size() > origin.size();
	if(inside && !std::string_view(written).substr(origin.size()).contains('~')) { // only what changed
		try {
			const json::json_pointer below(written.substr(origin.size()));
			if(const json* lying = findInState(state, json::json_pointer(written))) {
				copy[below] = *lying;
				return true;
			}
		} catch(const json::exception&) { // NOLINT(bugprone-empty-catch): the copy differs in shape: copied whole
		}
	}
	json value;
	if(origin.starts_with(modelPlaces)) value = engine_.models.at(origin);
	else if(const json* lying = findInState(state, json::json_pointer(origin))) value = *lying;
	if(!value.is_null()) copy = std::move(value); // else gone: the list that shows it is built again
	return false;
}

void Reconciler::updatePanel(Panel& /*panel*/, Record& /*record*/, const StateChanges& /*changes*/, int /*depth*/) {}

std::uint32_t Reconciler::rankOf(const Panel& panel) {
	const auto children = panel.parent()->children();
	const Record* record = recordOf(&panel);
	if(record && record->rank < children.size() && children[record->rank].get() == &panel) return record->rank;
	// moved: number all its siblings again, so that theirs (moved too, after a reorder) are found at once
	std::uint32_t rank = 0;
	for(std::uint32_t i = 0; i < children.size(); ++i) {
		if(children[i].get() == &panel) rank = i;
		if(Record* sibling = recordOf(children[i].get())) sibling->rank = i;
	}
	return rank;
}

void Reconciler::positionOf(const PanelReader& reader, std::vector<std::uint32_t>& out) {
	out.push_back(1);
	const std::size_t start = out.size();
	for(const Panel* panel = reader.panel; panel->parent(); panel = panel->parent()) out.push_back(3 + rankOf(*panel));
	std::reverse(out.begin() + static_cast<std::ptrdiff_t>(start), out.end());
	out.push_back(static_cast<std::uint32_t>(reader.kind));
}

void Reconciler::runReader(const PanelReader& reader, const StateChanges& changes) {
	Panel& panel = *reader.panel;
	Record* record = recordOf(&panel);
	if(!record) return;
	switch(reader.kind) {
	case PanelReader::Kind::Items:
		refreshItems(panel, *record, changes);
		return;
	case PanelReader::Kind::Values: {
		++statistics_.panelsVisited;
		const Item own{record->node, record->locals, record->path, nullptr};
		ReadSet reads;
		std::vector<json> values = valuesOf(own, reads);
		applyValues(panel, *record, own, std::move(values), std::move(reads), false);
		return;
	}
	case PanelReader::Kind::List: {
		++statistics_.panelsVisited;
		int depth = 0;
		for(const Panel* above = panel.parent(); above; above = above->parent()) ++depth;
		if(const std::string place = childrenPlace(record->node->serial);
		   !place.empty() && std::ranges::find(changes.paths, place) != changes.paths.end())
			record->list.node = nullptr; // a revision changed its children: matched again, not followed by the state
		children_.reconcileChildren(*record->node, panel, *record, record->path, depth, &changes);
		return;
	}
	}
}

void PanelReader::update(const StateChanges& changes) { reconciler->runReader(*this, changes); }

std::string PanelReader::name() const {
	static constexpr std::array<std::string_view, 3> kinds{"items", "values", "children"};
	const Record* record = reconciler->recordOf(panel);
	return std::string(kinds[static_cast<std::size_t>(kind)]) + " of a " + (record ? record->type : "panel");
}

void PanelReader::position(std::vector<std::uint32_t>& out) const { reconciler->positionOf(*this, out); }

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
		for(const PanelReader* reader : {&record->itemsReader, &record->valuesReader, &record->listReader})
			engine_.tracker.remove(reader->id);
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

void Reconciler::updateRoot(Panel& root, Record& record) {
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

bool Reconciler::runReaders(Panel& root, Record& record, StateChanges& changes, bool part) {
	Tracker& tracker = engine_.tracker;
	if(part) {
		const bool ran = tracker.update(changes);
		const bool restyled = engine_.document.appearance() != syncedAppearance_; // the theme reader ran: every token
		if(ran && !restyled && !rootScopeReads_.touchedBy(changes)) return true;
		statistics_.fullReason = restyled      ? "appearance"
		                         : changes.all ? "too many changes"
		                                       : "requested"; // or the root's own variables
		statistics_.full = true;
	} else {
		tracker.update(changes, false); // the readers outside the panels: what they mark joins this update
	}
	updateRoot(root, record);
	return false;
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

	statistics_.fullReason = full ? reason : fullReason(changes, resized, requested);
	statistics_.full = !statistics_.fullReason.empty();
	synced_ = false; // until the update ends without an error
	const bool part = runReaders(root, record, changes, !statistics_.full);
	synced_ = true;
	syncedAppearance_ = context.appearance();
	syncedScreen_ = context.screen();

	++statistics_.updates;
	statistics_.milliseconds =
	    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
	if(part && engine_.verify.on()) engine_.verify.check(root, [&] { updateRoot(root, record); });
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
	synced_ = false; // its parent's list doesn't build it: the next update builds everything
	return panel;
}

} // namespace affresco::format
