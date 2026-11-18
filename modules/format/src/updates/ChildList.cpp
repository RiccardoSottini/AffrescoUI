/// @file
/// @brief format::ChildList: building a panel's children and keeping them in line with the document.

#include "ChildList.hpp"
#include "Engine.hpp"

#include <format>

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

} // namespace

const std::string& ChildList::typeOf(const CompiledNode& node, const std::string& path) {
	if(!node.typeIsText) static_cast<void>(string(node.own["type"], path + ".type"));
	return node.type;
}

std::optional<std::vector<Item>> ChildList::childrenOf(const CompiledNode& node, const Record& record,
                                                       const std::string& path) const {
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
	if(node.children) items = engine_.builder.expand(*node.children, record.locals, path);
	else if(record.managed > 0) items.emplace(); // else nothing built here before: leave the panel's own children alone
	if(record.type == "root" && path == "root") addOverlays(record, items);
	return items;
}

// After the root's children, so above the overlays inside them; keyed, so they stay when the root loses a child
void ChildList::addOverlays(const Record& record, std::optional<std::vector<Item>>& items) const {
	const json& document = engine_.versions.document();
	const auto overlays = document.find("overlays");
	if(overlays == document.end() || !overlays->is_object() || overlays->empty()) return;
	if(!items) items.emplace();
	for(const auto& [name, node] : overlays->items()) {
		std::string at = "overlays." + name;
		SharedScope scope = record.locals;
		CompiledRef scoped = engine_.builder.scoped(engine_.builder.compiler().compiledOverlay(node, at), scope, at);
		if(scoped->when && // as a child's: not built while it doesn't hold
		   !format::truthy(scoped->when->evaluate(engine_.document.context(*scope, emptyObject()), at, "when")))
			continue;
		items->push_back({std::move(scoped), std::move(scope), at, json(at)});
	}
}

Panel* ChildList::reusable(Panel& panel, std::size_t index, const Item& item, const std::string& type) {
	// A keyed item finds its panel wherever it is now, so it keeps its focus, scroll and drag when lists reorder
	if(!item.key.is_null()) {
		for(std::size_t j = index; j < panel.children().size(); ++j) {
			const Record* r = reconciler_.recordOf(panel.children()[j].get());
			if(r && r->type == type && r->key == item.key) {
				if(j != index) panel.moveChild(j, index);
				break;
			}
		}
	}

	// Reuse the panel already in this place if it has the same type (and key): it keeps focus, hover and drag
	if(index < panel.children().size()) {
		const Record* r = reconciler_.recordOf(panel.children()[index].get());
		if(r && r->type == type && r->key == item.key) return panel.children()[index].get();
	}
	return nullptr;
}

Panel& ChildList::make(Panel& panel, std::size_t index, const Item& item, const std::string& type) {
	if(reconciler_.records().size() >= maxPanels)
		fail(item.path,
		     std::format("more than {} panels (components or repeats inside each other multiply)", maxPanels));
	UpdateStatistics& statistics = reconciler_.counters();
	if(index < panel.children().size()) {
		reconciler_.forgetSubtree(*panel.children()[index]);
		++statistics.panelsCreated;
		return panel.replaceChild(index, reconciler_.create(type, item.path));
	}
	++statistics.panelsCreated;
	return panel.addChild(reconciler_.create(type, item.path));
}

bool ChildList::updateKept(Panel& kept, const Item& item, const StateChanges& changes, int depth) {
	Record& record = *reconciler_.recordOf(&kept);
	if(record.node != item.node || record.path != item.path || !sameScope(record.locals.get(), item.locals.get()))
		return false; // other values may be read: built again
	reconciler_.updatePanel(kept, record, changes, depth);
	return true;
}

void ChildList::reconcileChild(Panel& panel, std::size_t index, Item& item, int depth, const StateChanges* changes) {
	++reconciler_.counters().panelsVisited;
	const std::string& type = typeOf(*item.node, item.path);
	Panel* kept = reusable(panel, index, item, type);
	if(kept && changes && updateKept(*kept, item, *changes, depth)) return;

	// Its values before a panel is made: an error leaves none half built
	ReadSet reads;
	std::vector<json> values = reconciler_.valuesOf(item, reads);
	const bool created = !kept;
	Panel& child = created ? make(panel, index, item, type) : *kept;
	Record& childRecord = created ? reconciler_.adopt(child, type) : *reconciler_.recordOf(&child);
	childRecord.locals = std::move(item.locals);
	childRecord.key = std::move(item.key);
	reconciler_.applyValues(child, childRecord, item, std::move(values), std::move(reads), created);
	reconcileChildren(*item.node, child, childRecord, item.path, depth); // other variables: everything below
	if(created && !engine_.handlersSuspended && childRecord.handlers.contains("onShow"))
		engine_.scheduler.defer(engine_.runner.later(childRecord.handlers["onShow"], child, {{"type", "show"}},
		                                             childRecord.locals, childRecord.path + ".onShow"));
}

void ChildList::reconcileChildren(const CompiledNode& node, Panel& panel, Record& record, const std::string& path,
                                  int depth, const StateChanges* changes) {
	if(depth >= maxNodeDepth)
		fail(path, "nodes nested too deeply (more than " + std::to_string(maxNodeDepth) +
		               " levels: a component or a screen inside itself?)");
	ReadSet reads;
	std::optional<std::vector<Item>> items;
	{
		const DocumentContext::ReadScope scope(engine_.document, &reads);
		items = childrenOf(node, record, path);
	}
	if(items) {
		std::size_t index = record.firstManaged;
		for(Item& item : *items) reconcileChild(panel, index++, item, depth + 1, changes);
		for(std::size_t i = index; i < panel.children().size(); ++i) reconciler_.forgetSubtree(*panel.children()[i]);
		panel.removeChildren(index);
		record.managed = index - record.firstManaged;
	}
	reconciler_.listReconciled(panel, record, std::move(reads));
}

} // namespace affresco::format
