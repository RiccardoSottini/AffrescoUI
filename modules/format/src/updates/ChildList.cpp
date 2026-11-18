/// @file
/// @brief format::ChildList: building a panel's children and keeping them in line with the document.

#include "ChildList.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <charconv>
#include <format>
#include <unordered_set>

namespace affresco::format {

namespace {

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

/// @brief The scope of a kept panel's variables that a row's scope, built again the same, stands for: its repeat's
/// item and number, so that numbering the row again reaches the panel.
/// @param old The kept panel's variables.
/// @param fresh The same variables, built again (sameScope()).
/// @return The old row's scope, or null if the row has none of its own.
SharedScope counterpart(const SharedScope& old, const SharedScope& fresh) {
	SharedScope a = old;
	for(const Scope* b = fresh.get(); a && b; b = b->parent, a = a->keep) {
		if(a.get() == b) return nullptr; // shared from here: the parent's
		if(b->place.row != ItemPlace::noRow) return a;
	}
	return nullptr;
}

/// @brief A key as a list keeps it: after its panel's type, since a key matches only a panel of the same type.
/// @param type The type.
/// @param key The key.
/// @return The name.
std::string keyName(const std::string& type, const json& key) { return type + '\x1f' + key.dump(); }

/// @brief The number of the row a place inside a list lies in.
/// @param list The list's place.
/// @param path A place inside it.
/// @return The number, or std::nullopt if the key isn't one.
std::optional<std::size_t> rowIn(std::string_view list, std::string_view path) {
	path.remove_prefix(list.size() + 1U);
	const std::string_view key = path.substr(0, path.find('/'));
	if(key.empty() || (key.size() > 1U && key.front() == '0')) return std::nullopt;
	std::size_t number = 0;
	const auto [end, error] = std::from_chars(key.data(), key.data() + key.size(), number);
	if(error != std::errc{} || end != key.data() + key.size()) return std::nullopt;
	return number;
}

/// @brief Whether a change of one place may change what a set read.
/// @param reads What was read.
/// @param path The place.
/// @return true if it may.
bool touches(const ReadSet& reads, const std::string& path) {
	StateChanges one;
	one.paths.push_back(path);
	return reads.touchedBy(one);
}

/// @brief What a kept row's panels read of a list beyond their own item.
struct Reach {
	bool list = false;   ///< The list, a place around it, or another row.
	bool number = false; ///< The row's number (indexKey).
};

/// @brief What panels read of a list beyond one item.
/// @param reads What they read.
/// @param list The list's place.
/// @param recorded The number of their row when they read it (ItemPlace::noRow: none).
/// @return It.
Reach reachOf(const ReadSet& reads, std::string_view list, std::size_t recorded) {
	if(reads.always) return {.list = true, .number = true};
	Reach reach;
	const std::string own = std::to_string(recorded);
	for(const std::string& read : reads.paths) {
		if(!contains(list, read)) {
			reach.list = reach.list || contains(read, list, true);
			continue;
		}
		std::string_view key = std::string_view(read).substr(std::min(read.size(), list.size() + 1U));
		key = key.substr(0, key.find('/'));
		if(key == indexKey) reach.number = true;
		else if(read.size() == list.size() || key != own) reach.list = true;
	}
	return reach;
}

/// @brief A row's item, as its variables hold it.
/// @param entry Its repeat.
/// @param row The row.
/// @return The item.
const json& itemOf(const ListEntry& entry, const ListRow& row) {
	return row.scope->vars.at(entry.compiledRepeat().item);
}

/// @brief Whether a list is a repeat's rows' items with some inserted or removed at one place.
/// @param entry The repeat.
/// @param list The list now.
/// @param at Where.
/// @param inserted How many inserted.
/// @param removed How many removed.
/// @param from The rows before it already compared.
/// @return true if it is.
bool shiftedBy(const ListEntry& entry, const json& list, std::size_t at, std::size_t inserted, std::size_t removed,
               std::size_t from = 0) {
	const std::size_t count = entry.rows.size();
	if(at + removed > count || list.size() + removed != count + inserted) return false;
	for(std::size_t k = from; k < at; ++k)
		if(itemOf(entry, entry.rows[k]) != list[k]) return false;
	for(std::size_t k = at + removed; k < count; ++k)
		if(itemOf(entry, entry.rows[k]) != list[k - removed + inserted]) return false;
	return true;
}

/// @brief How a repeat's list changed shape: as the step said (StateChanges::lists), else one run of items inserted
/// or removed, found by comparing the items.
/// @param entry The repeat.
/// @param list The list now.
/// @param said How the steps said lists changed.
/// @param plan Gets where, and how many inserted or removed.
/// @return false if it isn't one run.
bool shiftOf(const ListEntry& entry, const json& list, const std::vector<ListChange>& said, std::size_t& at,
             std::size_t& inserted, std::size_t& removed) {
	const std::string place = entry.place();
	if(std::ranges::count(said, place, &ListChange::place) == 1) {
		const ListChange& change = *std::ranges::find(said, place, &ListChange::place);
		const bool insert = change.kind == ListChange::Kind::Inserted;
		at = change.index;
		inserted = insert ? change.count : 0U;
		removed = insert ? 0U : change.count;
		if(shiftedBy(entry, list, at, inserted, removed)) return true;
	}
	const std::size_t count = entry.rows.size();
	at = 0;
	while(at < count && at < list.size() && itemOf(entry, entry.rows[at]) == list[at]) ++at;
	inserted = list.size() > count ? list.size() - count : 0U;
	removed = count > list.size() ? count - list.size() : 0U;
	return shiftedBy(entry, list, at, inserted, removed, at);
}

/// @brief Whether a row may get or lose its panel in place, as a full update would match panels (ChildList::match()):
/// its key is unique, or nothing matched by place follows it.
/// @param state The rows.
/// @param key Its key.
/// @param type Its panel's type.
/// @param adding Whether it gets one (else it loses one).
/// @param last Whether no panel follows it.
/// @return true if it may.
bool movable(const ListState& state, const json& key, const std::string& type, bool adding, bool last) {
	if(last) return true;
	if(key.is_null()) return false;
	const auto found = state.keys.find(keyName(type, key));
	const std::size_t have = found == state.keys.end() ? 0U : found->second;
	return adding ? have == 0U : have == 1U;
}

/// @brief Counts a shown row's key in or out.
/// @param state The rows.
/// @param type Its panel's type.
/// @param key Its key.
/// @param adding Whether it gets a panel (else it loses one).
void countKey(ListState& state, const std::string& type, const json& key, bool adding) {
	if(key.is_null()) return;
	const std::string name = keyName(type, key);
	if(adding) {
		++state.keys[name];
		return;
	}
	const auto found = state.keys.find(name);
	if(found != state.keys.end() && --found->second == 0U) state.keys.erase(found);
}

} // namespace

const std::string& ChildList::typeOf(const CompiledNode& node, const std::string& path) {
	if(!node.typeIsText) static_cast<void>(string(node.own["type"], path + ".type"));
	return node.type;
}

std::optional<std::vector<Item>> ChildList::childrenOf(const CompiledNode& node, const Record& record,
                                                       const std::string& path, ListState& state) const {
	if(record.type == "outlet") {
		// The current screen, filling the outlet unless it places itself
		std::vector<Item> items;
		const json& document = engine_.versions.document();
		const std::string& screen = engine_.document.screen();
		if(document.contains("screens") && document["screens"].contains(screen)) {
			std::string at = "screens." + screen;
			SharedScope scope = record.locals;
			CompiledRef scopedNode = engine_.builder.scoped(
			    engine_.builder.compiler().compiled(document["screens"][screen], at, true), scope, at);
			items.push_back({std::move(scopedNode), std::move(scope), std::move(at), nullptr});
		}
		return items;
	}
	std::optional<std::vector<Item>> items;
	if(node.children) {
		items = engine_.builder.expand(*node.children, record.locals, path, &state);
		state.tracked = true;
		state.node = &node;
	} else if(record.managed > 0)
		items.emplace(); // else nothing built here before: leave the panel's own children alone
	if(record.type == "root" && path == "root" && addOverlays(record, items)) state.tracked = false;
	return items;
}

// After the root's children, so above the overlays inside them; keyed, so they stay when the root loses a child
bool ChildList::addOverlays(const Record& record, std::optional<std::vector<Item>>& items) const {
	const json& document = engine_.versions.document();
	const auto overlays = document.find("overlays");
	if(overlays == document.end() || !overlays->is_object() || overlays->empty()) return false;
	if(!items) items.emplace();
	for(const auto& [name, node] : overlays->items()) {
		const std::string at = "overlays." + name;
		SharedScope scope = record.locals;
		CompiledRef scoped = engine_.builder.scoped(engine_.builder.compiler().compiledOverlay(node, at), scope, at);
		if(scoped->when && // as a child's: not built while it doesn't hold
		   !format::truthy(scoped->when->evaluate(engine_.document.context(*scope, emptyObject()), at, "when")))
			continue;
		items->push_back({std::move(scoped), std::move(scope), at, json(at)});
	}
	return true; // what their "when"s read isn't any list's: the root's children are built again when it changes
}

Panel& ChildList::build(Panel& panel, std::size_t index, Item& item, int depth) {
	++reconciler_.counters().panelsVisited;
	const std::string& type = typeOf(*item.node, item.path);
	// Its values before a panel is made: an error leaves none half built
	ReadSet reads;
	std::vector<json> values = reconciler_.valuesOf(item, reads);
	if(reconciler_.records().size() >= maxPanels)
		fail(item.path,
		     std::format("more than {} panels (components or repeats inside each other multiply)", maxPanels));
	++reconciler_.counters().panelsCreated;
	Panel& child = panel.insertChild(index, reconciler_.create(type, item.path));
	Record& record = reconciler_.adopt(child, type);
	record.locals = item.locals;
	record.key = item.key;
	reconciler_.applyValues(child, record, item, std::move(values), std::move(reads), true);
	reconcileChildren(*item.node, child, record, item.path, depth); // everything below
	if(!engine_.handlersSuspended && record.handlers.contains("onShow"))
		engine_.scheduler.defer(engine_.runner.later(record.handlers["onShow"], child, {{"type", "show"}},
		                                             record.locals, record.path + ".onShow"));
	return child;
}

void ChildList::rebuild(Panel& child, Item& item, int depth) {
	++reconciler_.counters().panelsVisited;
	Record& record = *reconciler_.recordOf(&child);
	ReadSet reads;
	std::vector<json> values = reconciler_.valuesOf(item, reads);
	record.locals = item.locals;
	record.key = item.key;
	reconciler_.applyValues(child, record, item, std::move(values), std::move(reads), false);
	reconcileChildren(*item.node, child, record, item.path, depth); // other variables: everything below
}

Panel& ChildList::place(Panel& panel, Panel* kept, Item& item, int depth, const StateChanges* changes,
                        SharedScope& reused) {
	if(!kept) return build(panel, panel.children().size(), item, depth);
	Record& record = *reconciler_.recordOf(kept);
	if(changes && record.node == item.node && record.path == item.path &&
	   sameScope(record.locals.get(), item.locals.get())) {
		// The same node, place and variables: only what reads the changes; the row keeps its old variables
		++reconciler_.counters().panelsVisited;
		reused = counterpart(record.locals, item.locals);
		reconciler_.updatePanel(*kept, record, *changes, depth);
		return *kept;
	}
	rebuild(*kept, item, depth);
	return *kept;
}

// Keyed children find their panels wherever they are (the first of a key, if two share it); the others take the
// panels of those without a key in order, if their type and key agree
std::vector<std::optional<std::size_t>> ChildList::match(const Panel& panel, const Record& record,
                                                         const std::vector<Item>& items) const {
	const auto& children = panel.children();
	const std::size_t first = std::min(record.firstManaged, children.size());
	std::unordered_map<std::string, std::size_t> byKey;
	byKey.reserve(children.size() - first);
	std::vector<std::size_t> byPlace;
	for(std::size_t i = first; i < children.size(); ++i) {
		const Record* old = reconciler_.recordOf(children[i].get());
		if(!old) continue;
		if(old->key.is_null() || !byKey.try_emplace(keyName(old->type, old->key), i - first).second)
			byPlace.push_back(i - first);
	}
	std::vector<std::optional<std::size_t>> matched(items.size());
	std::unordered_set<std::string> seen;
	std::size_t next = 0;
	for(std::size_t j = 0; j < items.size(); ++j) {
		const Item& item = items[j];
		const std::string& type = typeOf(*item.node, item.path);
		if(!item.key.is_null()) {
			std::string name = keyName(type, item.key);
			const auto found = byKey.find(name);
			if(seen.insert(std::move(name)).second) { // the first of its key: its own panel, or a new one
				if(found != byKey.end()) matched[j] = found->second;
				continue;
			}
		}
		const std::size_t at = next++;
		if(at >= byPlace.size()) continue;
		const Record* old = reconciler_.recordOf(children[first + byPlace[at]].get());
		if(old->type == type && old->key == item.key) matched[j] = byPlace[at];
	}
	return matched;
}

void ChildList::fill(ListState& state, const std::vector<Item>& items, const std::vector<Panel*>& panels,
                     std::vector<SharedScope>& reused) {
	std::size_t j = 0;
	for(ListEntry& entry : state.entries)
		for(ListRow& row : entry.rows) {
			if(!row.shown) continue;
			if(j >= items.size()) {
				state.tracked = false;
				return;
			}
			row.panel = panels[j];
			if(reused[j]) row.scope = std::move(reused[j]);
			countKey(state, typeOf(*items[j].node, items[j].path), row.key, true);
			++j;
		}
	if(j != items.size()) state.tracked = false;
}

void ChildList::arrange(Panel& panel, Record& record, std::vector<Item>& items, int depth, const StateChanges* changes,
                        ListState& state) {
	const std::vector<std::optional<std::size_t>> matched = match(panel, record, items);
	const std::size_t first = std::min(record.firstManaged, panel.children().size());
	const std::size_t old = panel.children().size() - first;
	std::vector<std::size_t> order;
	order.reserve(old + items.size());
	std::vector<bool> used(old);
	std::vector<Panel*> panels(items.size());
	std::vector<SharedScope> reused(items.size());
	for(std::size_t j = 0; j < items.size(); ++j) {
		const std::optional<std::size_t> at = matched[j];
		Panel* kept = nullptr;
		order.push_back(panel.children().size()); // a new one goes at the end
		if(at) {
			kept = panel.children()[first + *at].get();
			order.back() = first + *at;
			used[*at] = true;
		}
		panels[j] = &place(panel, kept, items[j], depth, changes, reused[j]);
	}
	for(std::size_t i = 0; i < old; ++i)
		if(!used[i]) order.push_back(first + i);
	bool moved = false;
	for(std::size_t i = 0; i < order.size() && !moved; ++i) moved = order[i] != first + i;
	if(moved) panel.reorderChildren(first, std::move(order)); // one move for the whole list
	const std::size_t end = first + items.size();
	for(std::size_t i = end; i < panel.children().size(); ++i) reconciler_.forgetSubtree(*panel.children()[i]);
	panel.removeChildren(end);
	record.managed = items.size();
	fill(state, items, panels, reused);
}

void ChildList::reconcileAll(const CompiledNode& node, Panel& panel, Record& record, const std::string& path, int depth,
                             const StateChanges* changes) {
	ReadSet reads;
	ListState state;
	std::optional<std::vector<Item>> items;
	{
		const DocumentContext::ReadScope scope(engine_.document, &reads);
		items = childrenOf(node, record, path, state);
	}
	if(items) arrange(panel, record, *items, depth + 1, changes, state);
	else state.tracked = false;
	if(state.tracked)
		for(const ListEntry& entry : state.entries)
			if(entry.placed) reads.addPath(entry.place()); // a change inside an item comes here (partial())
	record.list = std::move(state);
	reconciler_.listReconciled(panel, record, std::move(reads));
}

void ChildList::reconcileChildren(const CompiledNode& node, Panel& panel, Record& record, const std::string& path,
                                  int depth, const StateChanges* changes) {
	if(depth >= maxNodeDepth)
		fail(path, "nodes nested too deeply (more than " + std::to_string(maxNodeDepth) +
		               " levels: a component or a screen inside itself?)");
	if(changes && record.list.tracked && record.list.node == &node && partial(panel, record, depth, *changes)) return;
	reconcileAll(node, panel, record, path, depth, changes);
}

// --- Partial updates: an insert, a removal, or changes inside items ---

std::size_t ChildList::positionOf(const ListState& state, std::size_t first, std::size_t index, std::size_t number) {
	std::size_t position = first;
	for(std::size_t i = 0; i <= index && i < state.entries.size(); ++i) {
		const std::vector<ListRow>& rows = state.entries[i].rows;
		const std::size_t end = i == index ? std::min(number, rows.size()) : rows.size();
		for(std::size_t k = 0; k < end; ++k) position += rows[k].panel ? 1U : 0U;
	}
	return position;
}

bool ChildList::inStep(const Panel& panel, const Record& record) {
	const auto& children = panel.children();
	if(children.size() != record.firstManaged + record.managed) return false;
	std::size_t i = record.firstManaged;
	for(const ListEntry& entry : record.list.entries)
		for(const ListRow& row : entry.rows) {
			if(!row.panel) continue;
			if(i >= children.size() || children[i].get() != row.panel) return false;
			++i;
		}
	return i == children.size();
}

bool ChildList::planPath(const ListEntry& entry, const std::string& list, const std::string& path, EntryPlan& plan,
                         bool& shape) {
	if(entry.placed && path == list) {
		plan.shape = shape = true;
		return true;
	}
	if(entry.placed && contains(path, list)) return false; // around the list: every row may differ
	if(!entry.placed || !contains(list, path)) return !touches(entry.reads, path); // what rows read outside items
	const std::optional<std::size_t> row = rowIn(list, path);
	if(!row || *row >= entry.rows.size()) return false; // past its end: it grows
	plan.inside.push_back(*row);
	return true;
}

bool ChildList::classify(const ListState& state, const StateChanges& changes, Plan& plan) {
	plan.entries.resize(state.entries.size());
	std::vector<std::string> places(state.entries.size());
	for(std::size_t i = 0; i < state.entries.size(); ++i) {
		if(state.entries[i].reads.always) return false;
		if(state.entries[i].placed) places[i] = state.entries[i].place();
	}
	for(const std::string& path : changes.paths) {
		bool shape = false;
		for(std::size_t i = 0; i < state.entries.size(); ++i)
			if(!planPath(state.entries[i], places[i], path, plan.entries[i], shape)) return false;
		if(shape) plan.shapes = true;
		else plan.others.paths.push_back(path);
	}
	for(const ListChange& change : changes.lists)
		if(std::ranges::find(plan.others.paths, change.place) != plan.others.paths.end())
			plan.others.lists.push_back(change);
	return true;
}

bool ChildList::prepare(ListState& state, std::size_t index, const StateChanges& changes, EntryPlan& plan,
                        const Panel& panel, std::size_t first) {
	if(!plan.shape) return prepareRows(state, index, changes, plan, panel, first);
	ListEntry& entry = state.entries[index];
	if(!plan.inside.empty() || entry.otherRows) return false; // rows read their numbers or each other
	const json* list = findInState(engine_.versions.state(), json::json_pointer(entry.place()));
	if(!list || !list->is_array()) return false;
	if(!shiftOf(entry, *list, changes.lists, plan.at, plan.inserted, plan.removed)) return false;
	const bool last = positionOf(state, first, index, plan.at + plan.removed) == panel.children().size();
	for(std::size_t k = plan.at; k < plan.at + plan.removed; ++k)
		if(const ListRow& row = entry.rows[k];
		   row.panel && !movable(state, row.key, reconciler_.recordOf(row.panel)->type, false, last))
			return false;
	return insertRows(state, entry, *list, plan, last);
}

bool ChildList::insertRows(const ListState& state, const ListEntry& entry, const json& list, EntryPlan& plan,
                           bool last) const {
	const DocumentContext::ReadScope reading(engine_.document, &plan.reads);
	std::unordered_set<std::string> added;
	for(std::size_t k = plan.at; k < plan.at + plan.inserted; ++k) {
		ListRow& row = plan.rows.emplace_back();
		const std::optional<Item>& item = plan.items.emplace_back(engine_.builder.row(entry, k, list[k], row));
		plan.numbers.push_back(k);
		if(!item) continue;
		const std::string& type = typeOf(*item->node, item->path);
		if(!movable(state, row.key, type, true, last)) return false;
		if(!row.key.is_null() && !added.insert(keyName(type, row.key)).second && !last) return false;
	}
	return true;
}

bool ChildList::prepareRows(ListState& state, std::size_t index, const StateChanges& changes, EntryPlan& plan,
                            const Panel& panel, std::size_t first) {
	if(plan.inside.empty()) return true;
	ListEntry& entry = state.entries[index];
	if(entry.otherRows) return false; // a row's own reads may reach another's item
	std::ranges::sort(plan.inside);
	plan.inside.erase(std::ranges::unique(plan.inside).begin(), plan.inside.end());
	const DocumentContext::ReadScope reading(engine_.document, &plan.reads);
	const std::string& name = entry.compiledRepeat().item;
	std::size_t flips = 0;
	for(const std::size_t k : plan.inside) {
		const ListRow& row = entry.rows[k];
		if(const json* item = findInState(engine_.versions.state(), json::json_pointer(row.scope->origin())))
			row.scope->vars[name] = *item; // as refreshItems() does for a panel: a row without one too
		if(!row.reads.touchedBy(changes)) continue;
		ListRow fresh;
		std::optional<Item> item = engine_.builder.row(entry, k, itemOf(entry, row), fresh);
		if(fresh.key != row.key) return false; // its panel would move
		if(item.has_value() != (row.panel != nullptr)) {
			const std::size_t position = positionOf(state, first, index, k);
			const bool last = position + (row.panel ? 1U : 0U) == panel.children().size();
			const std::string& type = item ? typeOf(*item->node, item->path) : reconciler_.recordOf(row.panel)->type;
			const bool alone = flips == 0U; // a second flip at the end would find the first's panel there
			++flips;
			if(!movable(state, row.key, type, !row.panel, last && alone) || (last && !alone)) return false;
		}
		plan.rows.push_back(std::move(fresh));
		plan.items.push_back(std::move(item));
		plan.numbers.push_back(k);
	}
	return true;
}

std::vector<std::pair<std::size_t, int>> ChildList::chosenOf(const Panel& panel, const Record& record, const Plan& plan,
                                                             bool& allChanges, bool& allShapes) {
	const std::size_t count = panel.children().size();
	std::vector<std::pair<std::size_t, int>> chosen;
	if(!plan.others.empty()) {
		const auto found = record.items.visits(plan.others, count);
		allChanges = !found;
		if(found)
			for(const std::size_t i : *found) chosen.emplace_back(i, 1);
	}
	if(plan.shapes) { // those that read more than their item: their numbers, the list, another row
		const auto found = record.items.visits(StateChanges{}, count);
		allShapes = !found;
		if(found)
			for(const std::size_t i : *found) chosen.emplace_back(i, 2);
	}
	std::ranges::sort(chosen);
	return chosen;
}

std::vector<ChildList::Visit> ChildList::visitsOf(const Panel& panel, const Record& record, const Plan& plan) {
	bool allChanges = false;
	bool allShapes = false;
	const std::vector<std::pair<std::size_t, int>> chosen = chosenOf(panel, record, plan, allChanges, allShapes);
	std::vector<Visit> visits;
	std::size_t child = record.firstManaged;
	std::size_t next = 0;
	for(std::size_t i = 0; i < record.list.entries.size(); ++i)
		for(std::size_t k = 0; k < record.list.entries[i].rows.size(); ++k) {
			if(!record.list.entries[i].rows[k].panel) continue;
			Visit visit{.entry = i, .row = k, .changes = allChanges, .shape = allShapes};
			for(; next < chosen.size() && chosen[next].first <= child; ++next)
				if(chosen[next].first == child) (chosen[next].second == 1 ? visit.changes : visit.shape) = true;
			if(visit.changes || visit.shape) visits.push_back(visit);
			++child;
		}
	return visits;
}

void ChildList::replaceRow(Panel& panel, ListState& state, std::size_t index, std::size_t number, ListRow row,
                           std::optional<Item>& item, int depth, const StateChanges& changes) {
	ListRow& old = state.entries[index].rows[number];
	const std::size_t position = positionOf(state, reconciler_.recordOf(&panel)->firstManaged, index, number);
	if(old.panel && !item) { // its "when" no longer holds
		countKey(state, reconciler_.recordOf(old.panel)->type, old.key, false);
		reconciler_.forgetSubtree(*old.panel);
		static_cast<void>(panel.takeChild(position));
	} else if(!old.panel && item) { // it holds now
		row.panel = &build(panel, position, *item, depth);
		countKey(state, typeOf(*item->node, item->path), row.key, true);
	} else if(item) {
		row.panel = old.panel;
		Record& record = *reconciler_.recordOf(old.panel);
		if(record.node == item->node && record.path == item->path &&
		   sameScope(record.locals.get(), item->locals.get())) {
			row.scope = old.scope; // the same variables: the panel's are kept
			reconciler_.updatePanel(*old.panel, record, changes, depth);
		} else rebuild(*old.panel, *item, depth);
	}
	old = std::move(row);
}

void ChildList::apply(Panel& panel, Record& record, int depth, Plan& plan, const StateChanges& changes) {
	for(std::size_t i = 0; i < plan.entries.size(); ++i) {
		EntryPlan& part = plan.entries[i];
		ListState& state = record.list;
		ListEntry& entry = state.entries[i];
		entry.reads.add(part.reads);
		if(!part.shape) {
			for(std::size_t j = 0; j < part.rows.size(); ++j)
				replaceRow(panel, state, i, part.numbers[j], std::move(part.rows[j]), part.items[j], depth, changes);
			continue;
		}
		const std::size_t position = positionOf(state, record.firstManaged, i, part.at);
		for(std::size_t k = part.at; k < part.at + part.removed; ++k) {
			const ListRow& row = entry.rows[k];
			if(!row.panel) continue;
			countKey(state, reconciler_.recordOf(row.panel)->type, row.key, false);
			reconciler_.forgetSubtree(*row.panel);
			static_cast<void>(panel.takeChild(position));
		}
		const auto from = entry.rows.begin() + static_cast<std::ptrdiff_t>(part.at);
		entry.rows.erase(from, from + static_cast<std::ptrdiff_t>(part.removed));
		entry.rows.insert(entry.rows.begin() + static_cast<std::ptrdiff_t>(part.at),
		                  std::make_move_iterator(part.rows.begin()), std::make_move_iterator(part.rows.end()));
		for(std::size_t j = 0, at = position; j < part.items.size(); ++j) {
			std::optional<Item>& item = part.items[j];
			if(!item) continue;
			ListRow& row = entry.rows[part.at + j];
			row.panel = &build(panel, at++, *item, depth);
			countKey(state, typeOf(*item->node, item->path), row.key, true);
		}
		for(std::size_t k = part.at + part.inserted; k < entry.rows.size(); ++k) entry.rows[k].scope->renumber(k);
	}
	record.managed = panel.children().size() - record.firstManaged;
}

StateChanges ChildList::changesFor(const ListState& state, const Plan& plan, const Visit& visit, std::size_t number,
                                   const ReadSet& reads) {
	StateChanges changes;
	if(visit.changes) changes = plan.others;
	if(!visit.shape) return changes;
	for(std::size_t i = 0; i < state.entries.size(); ++i) {
		const EntryPlan& part = plan.entries[i];
		if(!part.shape || part.inserted + part.removed == 0U) continue;
		const ListEntry& entry = state.entries[i];
		const bool own = i == visit.entry;
		const std::string list = entry.place();
		const Reach reach = reachOf(reads, list, own ? entry.rows[number].recorded : ItemPlace::noRow);
		if(reach.list) changes.add(list);
		if(own && reach.number && number != visit.row) changes.add(list + '/' + std::string(indexKey));
	}
	return changes;
}

void ChildList::deliver(ListState& state, int depth, const Plan& plan, const std::vector<Visit>& visits) {
	for(const Visit& visit : visits) {
		const EntryPlan& part = plan.entries[visit.entry];
		std::size_t number = visit.row;
		if(part.shape && number >= part.at) {
			if(number < part.at + part.removed) continue; // destroyed
			number = number - part.removed + part.inserted;
		}
		if(std::ranges::binary_search(part.numbers, number) && !part.shape) continue; // built again already
		ListEntry& entry = state.entries[visit.entry];
		ListRow& row = entry.rows[number];
		Record* record = row.panel ? reconciler_.recordOf(row.panel) : nullptr;
		if(!record) continue;
		const StateChanges changes = changesFor(state, plan, visit, number, record->subtreeReads);
		if(changes.empty()) continue;
		const bool stale = entry.placed && row.recorded != number;
		if(stale && std::ranges::any_of(changes.paths, [&](const std::string& path) {
			   return overlaps(row.scope->origin(), path);
		   })) { // its panels read its item under its old number: built again under the new one
			Item item{record->node, record->locals, entry.at + "[" + std::to_string(number) + "]", row.key};
			rebuild(*row.panel, item, depth);
			row.recorded = number;
			continue;
		}
		reconciler_.updatePanel(*row.panel, *record, changes, depth);
	}
}

// What changed is sorted by list first, and the new rows built, so that anything this can't follow builds every child
// again before a panel changed
bool ChildList::partial(Panel& panel, Record& record, int depth, const StateChanges& changes) {
	ListState& state = record.list;
	if(changes.all || !inStep(panel, record)) return false;
	Plan plan;
	if(!classify(state, changes, plan)) return false;
	for(std::size_t i = 0; i < state.entries.size(); ++i)
		if(!prepare(state, i, changes, plan.entries[i], panel, record.firstManaged)) return false;
	const std::vector<Visit> visits = visitsOf(panel, record, plan);
	apply(panel, record, depth + 1, plan, changes);
	deliver(record.list, depth + 1, plan, visits);
	ReadSet reads;
	for(const ListEntry& entry : record.list.entries) {
		reads.add(entry.reads);
		if(entry.placed) reads.addPath(entry.place());
	}
	reconciler_.listReconciled(panel, record, std::move(reads));
	return true;
}

} // namespace affresco::format
