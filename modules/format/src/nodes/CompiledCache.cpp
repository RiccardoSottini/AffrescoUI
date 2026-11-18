/// @file
/// @brief format::CompiledCache: the document's nodes with their components instantiated and their styles merged,
/// compiled once per version of the document.

#include "CompiledCache.hpp"
#include "Library.hpp"
#include "Engine.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace affresco::format {

namespace {

/// @brief Puts a use's children wherever a component has {"slot": true}, searching its node depth first (not inside
/// other uses). Every slot gets them, so a component can show them in several places (a dock's areas, each showing
/// what is active in it), as the checker walks every slot.
/// @param node The component's node, or a node inside it.
/// @param children The use's children.
/// @return true if at least one slot took them.
bool fillSlot(json& node, const json& children) {
	if(!node.is_object() || !node.contains("children") || !node["children"].is_array()) return false;
	json& list = node["children"];
	bool placed = false;
	json replaced = json::array();
	for(json& child : list) {
		if(child.is_object() && child.value("slot", json(false)) == true) {
			replaced.insert(replaced.end(), children.begin(), children.end());
			placed = true;
			continue;
		}
		if(!child.contains("use") && fillSlot(child, children)) placed = true;
		replaced.push_back(std::move(child));
	}
	list = std::move(replaced);
	return placed;
}

/// @brief Tells whether a node sets its own position or size (otherwise a screen fills its outlet).
/// @param node The node.
/// @return true if it has layout properties.
bool hasLayout(const json& node) {
	static constexpr std::array<const char*, 6> keys = {"x", "y", "width", "height", "anchors", "fill"};
	return std::ranges::any_of(keys, [&node](const char* key) { return node.contains(key); });
}

/// @brief Whether two versions of the document name the same members of a section, in the same order.
/// @param before The document before.
/// @param after The document after.
/// @param section The section: "screens" or "overlays".
/// @return true if they do, or neither has it.
bool sameNames(const json& before, const json& after, const char* section) {
	const auto a = before.find(section);
	const auto b = after.find(section);
	if(a == before.end() || b == after.end()) return (a == before.end()) == (b == after.end());
	if(!a->is_object() || !b->is_object()) return *a == *b;
	if(a->size() != b->size()) return false;
	return std::ranges::equal(a->items(), b->items(), [](const auto& x, const auto& y) { return x.key() == y.key(); });
}

/// @brief A node's "children", where it is.
/// @param node The node.
/// @return The array as written, or nullptr.
const json* childrenOf(const json& node) {
	const auto it = node.find("children");
	return it == node.end() ? nullptr : &*it;
}

} // namespace

CompiledCache::Cache& CompiledCache::cache() const {
	const json* document = &engine_.versions.document();
	if(cache_.document != document) {
		forgetVersion();
		cache_.document = document;
	}
	return cache_;
}

void CompiledCache::forgetVersion() const noexcept { cache_ = Cache{}; }

CompiledRef CompiledCache::numbered(CompiledRef node) const noexcept {
	node->serial = ++serials_;
	return node;
}

/// @brief carryOver()'s walk over the document before and after: what was compiled, what is kept, by its new place.
struct CompiledCache::Carrier {
	/// @brief The maps of compiled nodes kept by the place of their node.
	static constexpr std::array<std::unordered_map<const json*, CompiledRef> Cache::*, 4> maps = {
	    &Cache::children, &Cache::nodes, &Cache::screens, &Cache::overlays};

	const Cache* old = nullptr;         // What was compiled for the version before.
	Cache next;                         // What is kept, by the places of the version after.
	std::vector<const json*> keeps;     // The instances of the kept nodes that hold their children.
	std::size_t carried = 0;            // The nodes kept.
	std::size_t topCarried = 0;         // Among them, the root, screens, templates and overlays.
	std::vector<std::uint64_t> changed; // The kept nodes whose list of children changed, by their number.

	/// @brief Walks a value of before and the value after at its place.
	/// @param before The value before.
	/// @param after The value after.
	/// @return Whether they are the same.
	bool walk(const json& before, const json& after) {
		if(before.type() != after.type()) return false;
		if(before.is_object()) return walkObject(before, after);
		if(before.is_array()) return walkArray(before, after);
		return before == after;
	}

	/// @brief Walks two lists: item by item when they are as long, else their common start, then their common end.
	/// @param before The list before.
	/// @param after The list after.
	/// @param shown Set, for a node's "children", when the list its panels show changed: a child added, removed or
	/// moved, or one whose compiled node isn't kept for it (a child kept with other children only changes its own
	/// list); nullptr for any other list.
	/// @return Whether they are the same.
	bool walkArray(const json& before, const json& after, bool* shown = nullptr) {
		const std::size_t common = std::min(before.size(), after.size());
		if(before.size() == after.size()) {
			bool same = true;
			for(std::size_t i = 0; i < common; ++i) {
				const bool item = walk(before[i], after[i]);
				if(shown && !item && !keptFor(before[i], after[i])) *shown = true;
				same = item && same;
			}
			return same;
		}
		if(shown) *shown = true;
		std::size_t start = 0;
		while(start < common && walk(before[start], after[start])) ++start;
		const std::size_t limit = start < common ? common - start - 1U : 0U; // the end never meets the start
		std::size_t end = 0;
		while(end < limit && walk(before[before.size() - 1U - end], after[after.size() - 1U - end])) ++end;
		return false;
	}

	/// @brief Walks two objects key by key, in order, "children" apart, then keeps what was compiled for them.
	/// @param before The object before.
	/// @param after The object after.
	/// @return Whether they are the same.
	bool walkObject(const json& before, const json& after) {
		const auto skip = [](const json& object, json::const_iterator it) {
			while(it != object.end() && it.key() == "children") ++it;
			return it;
		};
		bool own = true; // every key but "children" the same, in the same order
		auto b = skip(before, before.begin());
		auto a = skip(after, after.begin());
		for(; b != before.end() && a != after.end(); b = skip(before, ++b), a = skip(after, ++a)) {
			if(b.key() != a.key()) {
				own = false;
				break;
			}
			own = walk(b.value(), a.value()) && own;
		}
		own = own && b == before.end() && a == after.end();
		bool shown = false; // whether the list of children its panels show changed
		const bool same = walkChildren(before, after, shown) && own;
		if(own) keep(before, after, same, shown);
		return same;
	}

	/// @brief Walks two objects' "children".
	/// @param before The object before.
	/// @param after The object after.
	/// @param shown Set when the list of children the panels of a node show changed (see walkArray()).
	/// @return Whether they are the same, in the same place among the keys.
	bool walkChildren(const json& before, const json& after, bool& shown) {
		const auto childrenBefore = before.find("children");
		const auto childrenAfter = after.find("children");
		if(childrenBefore == before.end() || childrenAfter == after.end()) {
			const bool none = childrenBefore == before.end() && childrenAfter == after.end();
			shown = !none;
			return none;
		}
		const bool placed =
		    std::distance(before.begin(), childrenBefore) == std::distance(after.begin(), childrenAfter);
		bool same = false;
		if(childrenBefore->is_array() && childrenAfter->is_array())
			same = walkArray(*childrenBefore, *childrenAfter, &shown);
		else {
			same = walk(*childrenBefore, *childrenAfter);
			shown = !same;
		}
		return same && placed;
	}

	/// @brief Whether what was compiled for a value is all kept for the value after at its place.
	/// @param before The value before.
	/// @param after The value after.
	/// @return false if something compiled for it, or nothing at all (a child no list built), isn't kept.
	[[nodiscard]] bool keptFor(const json& before, const json& after) const {
		bool compiled = false;
		for(const auto map : maps)
			if((old->*map).contains(&before)) {
				if(!(next.*map).contains(&after)) return false;
				compiled = true;
			}
		return compiled;
	}

	/// @brief Keeps what was compiled for a node whose keys but "children" are the same, pointing it at its children
	/// after; a node whose children differ and are its own instance's is dropped.
	/// @param before The node before.
	/// @param after The node after.
	/// @param same Whether its children are the same too.
	/// @param shown Whether the list of children its panels show changed: its number is then among changed.
	void keep(const json& before, const json& after, bool same, bool shown) {
		for(const auto map : maps) {
			const auto found = (old->*map).find(&before);
			if(found == (old->*map).end()) continue;
			const CompiledRef& node = found->second;
			if(node->keep) {
				if(!same) continue; // its instance holds the children as they were
				keeps.push_back(node->keep.get());
			} else {
				const auto children = after.find("children");
				repoint(*node, children == after.end() ? nullptr : &*children);
				if(shown) changed.push_back(node->serial);
			}
			keepStyled(node);
			(next.*map).emplace(&after, node);
			counted(map);
		}
	}

	/// @brief Counts a compiled node kept.
	/// @param map The map it was kept in.
	void counted(std::unordered_map<const json*, CompiledRef> Cache::* map) {
		++carried;
		if(map != &Cache::children) ++topCarried;
	}

	/// @brief Points a node, and the nodes compiled for the styles it chose, at its children's new place.
	/// @param node The node.
	/// @param children Its children after.
	void repoint(const CompiledNode& node, const json* children) const {
		const json* was = node.children;
		node.children = children;
		if(const auto styled = old->styled.find(&node); styled != old->styled.end())
			for(const auto& [style, chosen] : styled->second)
				if(chosen->children == was && !chosen->keep) chosen->children = children;
	}

	/// @brief Keeps the nodes compiled for the styles a kept node chose.
	/// @param node The node.
	void keepStyled(const CompiledRef& node) {
		if(const auto styled = old->styled.find(node.get()); styled != old->styled.end())
			next.styled.emplace(node.get(), styled->second);
	}

	/// @brief Keeps, as they are, the nodes compiled inside the kept nodes' instances, and inside theirs.
	void keepInstances() {
		while(!keeps.empty()) {
			const json* instance = keeps.back();
			keeps.pop_back();
			keepInside(*instance);
		}
	}

	/// @brief Keeps what was compiled for a value inside an instance, and for the values inside it.
	/// @param value The value.
	void keepInside(const json& value) {
		for(const auto map : maps)
			if(const auto found = (old->*map).find(&value); found != (old->*map).end()) {
				if(found->second->keep) keeps.push_back(found->second->keep.get());
				keepStyled(found->second);
				(next.*map).emplace(&value, found->second);
				counted(map);
			}
		if(value.is_structured())
			for(const json& inner : value) keepInside(inner);
	}
};

void CompiledCache::carryOver(const json& before, const json& after, const DocumentChange& change) const {
	carried_ = 0;
	changedLists_.clear();
	listsOnly_ = false;
	const bool everyNode = change.everything || std::ranges::any_of(change.sections, [](const std::string& section) {
		                       return section == "styles" || section == "components" || section == "templates";
	                       });
	if(everyNode || !cache_.document) {
		forgetVersion();
		return;
	}
	Carrier carrier{.old = &cache_, .next = {}, .keeps = {}, .carried = 0, .topCarried = 0, .changed = {}};
	static_cast<void>(carrier.walk(before, after));
	carrier.keepInstances();
	carrier.next.document = &engine_.versions.document();
	carried_ = carrier.carried;
	revisionCounters().nodesCarried += carrier.carried;
	const std::size_t top = cache_.nodes.size() + cache_.screens.size() + cache_.overlays.size();
	listsOnly_ =
	    carrier.topCarried == top && sameNames(before, after, "screens") && sameNames(before, after, "overlays");
	std::ranges::sort(carrier.changed);
	const auto [last, end] = std::ranges::unique(carrier.changed);
	carrier.changed.erase(last, end);
	changedLists_ = std::move(carrier.changed);
	cache_ = std::move(carrier.next);
}

// The node's properties on top of its styles' (in order: later styles override earlier ones)
json CompiledCache::withStyles(const json& node, const std::string& path, int depth) const {
	object(node, path);
	if(!node.contains("style")) return node;
	if(depth > 16) fail(path + ".style", "styles nested too deeply (a style uses itself?)");

	const json& style = node["style"];
	if(isDynamic(style)) return node; // chosen per instance, once its variables are known (scoped())
	const json names = style.is_array() ? style : json::array({style});

	json merged = json::object();
	for(const json& name : names) {
		const std::string key = string(name, path + ".style");
		const json& document = engine_.versions.document();
		if(!document.contains("styles") || !document["styles"].contains(key))
			fail(path + ".style", "unknown style '" + key + "'");
		merged.update(withStyles(object(document["styles"][key], "styles." + key), "styles." + key, depth + 1));
	}

	for(const auto& [key, value] : node.items()) // the node's own keys win
		if(key != "style") merged[key] = value;
	return merged;
}

CompiledRef CompiledCache::compiled(const json& node, const std::string& path, bool fill) const {
	Cache& cached = cache();
	auto& map = fill ? cached.screens : cached.nodes;
	if(const auto it = map.find(&node); it != map.end()) return it->second;
	json merged = withStyles(withoutChildren(object(node, path)), path);
	if(fill && !hasLayout(merged)) merged["fill"] = 0;
	CompiledRef result = numbered(compileNode(std::move(merged), childrenOf(node)));
	map.emplace(&node, result);
	return result;
}

CompiledRef CompiledCache::compiledOverlay(const json& node, const std::string& path) const {
	Cache& cached = cache();
	if(const auto it = cached.overlays.find(&node); it != cached.overlays.end()) return it->second;
	const auto placed = [](json& merged) {
		json& overlay = merged["overlay"];
		if(overlay.is_null()) overlay = json::object();
		if(overlay.is_object() && !overlay.contains("placement")) overlay["placement"] = "window";
	};
	CompiledRef result;
	if(object(node, path).contains("use")) { // an overlay that is a component: its node, as resolve() makes it
		json raw = node;
		for(int depth = 0; raw.contains("use"); ++depth) {
			if(depth >= maxComponentDepth) fail(path, "components nested too deeply (a component uses itself?)");
			raw = instantiateComponent(raw, path);
		}
		json merged = withStyles(raw, path);
		placed(merged);
		result = numbered(compileNode(std::make_shared<const json>(std::move(merged)))); // it holds the use's children
	} else {
		json merged = withStyles(withoutChildren(node), path);
		placed(merged);
		result = numbered(compileNode(std::move(merged), childrenOf(node)));
	}
	cached.overlays.emplace(&node, result);
	return result;
}

const json& CompiledCache::componentNamed(const std::string& name, const std::string& path) const {
	const json& document = engine_.versions.document();
	const bool own = document.contains("components") && document["components"].contains(name);
	const json* builtIn = own ? nullptr : libraryComponent(name); // the document's own win
	if(!own && !builtIn) fail(path + ".use", "unknown component '" + name + "'");
	const json& component = builtIn ? *builtIn : object(document["components"][name], "components." + name);
	if(!component.contains("node")) fail("components." + name, "a component needs a \"node\"");
	return component;
}

json CompiledCache::instantiateComponent(const json& use, const std::string& path) const {
	const std::string name = string(use["use"], path + ".use");
	const json& component = componentNamed(name, path);

	json result = object(component["node"], "components." + name + ".node");
	for(const auto& [key, value] : use.items())
		if(key != "use" && key != "with" && key != "children") result[key] = value;
	result["with"] = parameters(use, component, result, name, path);

	// The use's children go where the component has {"slot": true} (nothing does if it has none)
	const json children = use.contains("children") ? use["children"] : json::array();
	if(!fillSlot(result, children) && use.contains("children")) result["children"] = children;
	return result;
}

json CompiledCache::parameters(const json& use, const json& component, const json& node, const std::string& name,
                               const std::string& path) {
	// Parameters, evaluated in this order in one layer, each seeing the ones before it: what the use passes (in the
	// caller's scope), then the component's defaults it didn't pass (they may read the passed ones), then the
	// component node's own "with" (what it passes when it is itself a use of another component, or its own variables):
	// it reads this component's parameters, and a name it shares with one of them replaces it.
	json with = use.contains("with") ? object(use["with"], path + ".with") : json::object();
	if(component.contains("params"))
		for(const auto& [key, value] : object(component["params"], "components." + name + ".params").items())
			if(!with.contains(key)) with[key] = value;
	if(node.contains("with"))
		for(const auto& [key, value] : object(node["with"], "components." + name + ".node.with").items()) {
			with.erase(key);
			with[key] = value;
		}
	return with;
}

CompiledRef CompiledCache::styled(const CompiledRef& node, const json& style, const std::string& at) const {
	Styled& choices = cache().styled[node.get()];
	const auto it = std::ranges::find(choices, style, &Styled::value_type::first);
	if(it != choices.end()) return it->second;

	json chosen = node->own; // the node with the style this instance chose
	if(style.is_null() || (style.is_string() && style.get_ref<const std::string&>().empty())) chosen.erase("style");
	else chosen["style"] = style;
	CompiledRef result = compileNode(withStyles(chosen, at), node->children, node->keep);
	result->serial = node->serial;       // its panels' lists are the node's
	choices.emplace_back(style, result); // kept: the children cache knows its children by their place
	return result;
}

CompiledRef CompiledCache::resolve(const json& child, const std::string& at) const {
	Cache& cached = cache();
	if(const auto it = cached.children.find(&child); it != cached.children.end()) return it->second;
	CompiledRef result;
	if(child.contains("use")) {
		json raw = child;
		for(int depth = 0; raw.contains("use"); ++depth) {
			if(depth >= maxComponentDepth) fail(at, "components nested too deeply (a component uses itself?)");
			raw = instantiateComponent(raw, at);
		}
		result =
		    numbered(compileNode(std::make_shared<const json>(withStyles(raw, at)))); // it holds the use's children
	} else if(child.contains("style") && !isDynamic(child["style"])) {
		result = numbered(compileNode(withStyles(withoutChildren(child), at), childrenOf(child)));
	} else {
		result = numbered(compileNode(withoutChildren(child), childrenOf(child)));
	}
	cached.children.emplace(&child, result);
	return result;
}

} // namespace affresco::format
