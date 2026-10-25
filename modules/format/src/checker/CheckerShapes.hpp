#pragma once

/// @file
/// @brief The checker's vocabulary, shared by its source files and the rules: shapes (what it knows about a value
/// without running anything), its limits, and a node prepared for checking (its layers of properties).

#include "affresco/core/Json.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format::checker {

/// @brief What the checker knows about a value without running anything, written as JSON:
/// - null: anything (nothing is known, so nothing about it is reported);
/// - true: text, a number or a boolean (a single value; a boolean, because JSON text would cost an allocation);
/// - an object: an object with those keys, each with its shape;
/// - []: a list whose items aren't known yet; [shape]: a list of such items.
using Shape = json;

inline const Shape anything;      ///< Nothing known.
inline const Shape single = true; ///< Text, a number or a boolean.
/// @brief What an editor's "caret" writes (a function: a static object built from a list could throw before main).
/// @return Its line, its column and the selected length.
[[nodiscard]] inline Shape caretShape() { return {{"line", true}, {"column", true}, {"selected", true}}; }

/// @brief What an editor's "viewport" writes.
/// @return Its keys, each always there.
[[nodiscard]] inline Shape viewShape() { return {{"first", true}, {"last", true}, {"rows", true}, {"lines", true}}; }

inline constexpr std::size_t maxVisits = 1000000; ///< Nodes, steps and values a check visits before giving up.
/// Shape nodes a check copies before giving up: a step that writes the state into the state doubles what it may
/// hold, so a few dozen such steps would copy more than memory holds (the largest screens tested copy 40,000).
inline constexpr std::size_t maxShapeWork = 4000000;
/// Findings a pass gives a hint ("did you mean ..."): a hint compares the name with every name that exists, so a
/// revision with many findings against a large state would otherwise cost their product. The first are listed.
inline constexpr std::size_t maxHints = 50;
inline constexpr int maxPasses = 5;                ///< Passes that learn what steps write, at most.
inline constexpr int maxDepth = maxNesting;        ///< Nodes and actions inside each other before a branch stops.
inline constexpr std::string_view openScope = "*"; ///< A variable that lets any name through (callers unknown).
/// Keys an object has before the checker finds them by name rather than one after another (as an ordered object does).
inline constexpr std::size_t indexedEntries = 16;

/// `$if`, `$and`, `$or` and `$coalesce`: operators that give one of their arguments.
inline constexpr auto choiceOperators = std::to_array<std::string_view>({"$if", "$and", "$or", "$coalesce"});

/// @brief The shape nodes a check may still copy, maxShapeWork in all.
class Work {
public:
	/// @brief Pays for copying a shape, counting its nodes without recursion.
	/// @param shape The shape.
	/// @return false once the check has copied maxShapeWork nodes (then always): it gives up.
	[[nodiscard]] bool pay(const Shape& shape);
	/// @brief Tells whether the check copied too much.
	/// @return true once pay() has refused.
	[[nodiscard]] bool over() const { return over_; }

private:
	std::size_t left_ = maxShapeWork; ///< Nodes it may still copy.
	bool over_ = false;               ///< It copied more.
	/// The shapes pay() has still to count, kept between calls: it is called for each write, so allocating its own
	/// would cost an allocation each.
	std::vector<const Shape*> pending_;
};

/// @brief An object's members found by key. An ordered object finds a key by looking at each one, which, for each of
/// thousands of entries, took seconds: a large object's keys are sorted once (one allocation, where a hash map costs
/// one per member), and a small one is still looked through.
class Members {
public:
	/// @brief An object with no members.
	Members() = default;
	/// @brief Sorts a large object's keys.
	/// @param object The object (anything else has no members); it must outlive this and stay as it is.
	explicit Members(const json& object);
	/// @brief Finds a member.
	/// @param key Its key.
	/// @return It, or nullptr.
	[[nodiscard]] const json* find(std::string_view key) const;

private:
	const json* object_ = nullptr;                                 ///< The object, or nullptr if there is none.
	std::vector<std::pair<std::string_view, const json*>> sorted_; ///< Its members by key, once it has indexedEntries.
};

/// @brief Tells whether a list of names has one.
/// @param names The list.
/// @param name The name.
/// @return true if it is there.
[[nodiscard]] inline bool among(std::span<const std::string_view> names, std::string_view name) {
	return std::ranges::find(names, name) != names.end();
}

/// @brief Merges what two values may hold, in place: the keys of both objects, the items of both lists.
/// @param into A shape; becomes the merge (anything if the types differ).
/// @param other Another.
void mergeInto(Shape& into, const Shape& other);

/// @brief Merges what two values may hold.
/// @param a A shape.
/// @param b Another.
/// @return Their merge; anything if their types differ.
[[nodiscard]] Shape merge(Shape a, const Shape& b);

/// @brief Adds what an item may hold to a list's shape.
/// @param list A list's shape.
/// @param item The item's shape.
void addItem(Shape& list, const Shape& item);

/// @brief Describes a value from the state.
/// @param data The value.
/// @return Its shape; an empty object is anything, since it is most likely a map filled later.
[[nodiscard]] Shape shapeOfData(const json& data);

/// @brief The shape of a list's items.
/// @param list A shape.
/// @return The items' shape, or anything.
[[nodiscard]] const Shape& itemOf(const Shape& list);

/// @brief Suggests what a mistyped name may have meant.
/// @param word The name as written.
/// @param names The names that exist.
/// @param prefix Written before a suggestion: "card." gives "did you mean 'card.title'?".
/// @param listIntro Without a near name, the names are listed after this ("defined:"); empty: no list.
/// @return The hint, or "".
[[nodiscard]] std::string hintFor(std::string_view word, const std::vector<std::string>& names, std::string_view prefix,
                                  std::string_view listIntro);

/// @brief The keys of an object.
/// @param object Any value.
/// @return Its keys, in the order they were added; none if it isn't an object.
[[nodiscard]] std::vector<std::string> keysOf(const json& object);

/// @brief Writes a compiled index expression the way it reads: `state.current`, `0`, or "...".
/// @param tree The compiled expression.
/// @return The text.
[[nodiscard]] std::string describe(const json& tree);

/// @brief How a step changes the value at a state path.
enum class Change {
	set,  ///< Replaces it (or creates it).
	push, ///< Adds an item to the list there.
	none  ///< Removes it, or something in it: nothing new can be read.
};

/// @brief What a write taught the checker about the state, in increasing order.
enum class Learning {
	nothing, ///< Nothing new.
	more,    ///< More keys, or less known: findings can only go away.
	items    ///< A list with no known items got some: findings can appear in reads of them.
};

/// @brief One step of a path: a name, an index, or an expression giving either.
struct PathKey {
	json name;       ///< A name (string) or an index (integer); null when an expression gives it.
	json expression; ///< The expression, compiled (only when name is null).
};

/// @brief One object a node's properties come from: the node as written, one of its styles, its component's node,
/// or the parameters the checker merged for a use.
struct Layer {
	const json* properties = nullptr; ///< The object: in the document, or in Prepared::made (both outlive the layer).
	std::string place;                ///< Its place: a property's place is "<place>.<key>".
	bool use = false;                 ///< The object of a use: its "use", "with" and "children" aren't the node's.

	/// @brief Tells whether a key of this object is one of the node's properties.
	/// @param key The key.
	/// @return false for a use's own "use", "with" and "children".
	[[nodiscard]] bool holds(std::string_view key) const {
		return !use || (key != "use" && key != "with" && key != "children");
	}
};

/// @brief A node ready to check: the objects its properties come from, in order, read where they are (merging
/// styles and components by copying would cost more than the rest of the check).
struct Prepared {
	std::vector<Layer> layers; ///< Styles first, then the component's node, then the uses: later ones win.
	std::vector<std::unique_ptr<json>> made;            ///< Objects the checker made for layers (merged parameters).
	std::string place;                                  ///< Where the node is.
	std::map<std::string, std::string> parameterPlaces; ///< Where each "with" value was written.
	const json* children = nullptr;                     ///< Its children, in the document (which outlives the check).
	std::string childrenPlace;                          ///< The place of the object the children belong to.
	std::string component;                              ///< The component it was built from, if any.

	/// @brief Finds a property in the layer that wins.
	/// @param key The property.
	/// @return Its value and its layer, or two nullptrs.
	[[nodiscard]] std::pair<const json*, const Layer*> find(const std::string& key) const {
		for(const Layer& layer : std::views::reverse(layers))
			if(const auto it = layer.properties->find(key); it != layer.properties->end() && layer.holds(key))
				return {&*it, &layer};
		return {nullptr, nullptr};
	}
	/// @brief A property's value.
	/// @param key The property.
	/// @return It, or nullptr.
	[[nodiscard]] const json* value(const std::string& key) const { return find(key).first; }
	/// @brief The place of a property, for findings.
	/// @param key The property.
	/// @return "<its layer's place>.<key>" (the node's place if it has none).
	[[nodiscard]] std::string where(const std::string& key) const {
		const Layer* layer = find(key).second;
		return (layer ? layer->place : place) + "." + key;
	}
	/// @brief Visits each property once, from the layer that wins.
	/// @param visit Called with the key, the value and the layer.
	template <class Visit> void forEach(const Visit& visit) const {
		// Every property, those of the layer that wins first, then the first of each key: a few are compared one
		// after another, many sorted by key (a hash set would cost an allocation per key). Keys stay where they are
		struct Property {
			const std::string* key;
			const json* value;
			const Layer* layer;
		};
		std::size_t count = 0;
		for(const Layer& layer : layers) count += layer.properties->size();
		std::vector<Property> all;
		all.reserve(count);
		for(const Layer& layer : std::views::reverse(layers))
			for(const auto& [key, value] : layer.properties->items())
				if(layer.holds(key)) all.push_back({&key, &value, &layer});
		const auto keyOf = [&](std::size_t i) { return std::string_view(*all[i].key); };
		if(all.size() < indexedEntries) {
			for(std::size_t i = 0; i < all.size(); ++i)
				if(std::ranges::none_of(all | std::views::take(i),
				                        [&](const Property& p) { return *p.key == keyOf(i); }))
					visit(*all[i].key, *all[i].value, *all[i].layer);
			return;
		}
		std::vector<std::size_t> order(all.size());
		for(std::size_t i = 0; i < order.size(); ++i) order[i] = i;
		std::ranges::sort(
		    order, [&](std::size_t a, std::size_t b) { return std::pair(keyOf(a), a) < std::pair(keyOf(b), b); });
		std::vector<bool> first(all.size());
		for(std::size_t i = 0; i < order.size(); ++i)
			first[order[i]] = i == 0 || keyOf(order[i]) != keyOf(order[i - 1]);
		for(std::size_t i = 0; i < all.size(); ++i)
			if(first[i]) visit(*all[i].key, *all[i].value, *all[i].layer);
	}
};

/// @brief What a declared type lets the state hold, for reading paths (CheckerTypes.cpp): an object's properties
/// (unless it lets other keys in too: then any key may be read), a list's items, or a single value.
/// @param type The type.
/// @param depth Types inside types so far.
/// @return The shape.
[[nodiscard]] Shape shapeOfType(const json& type, int depth);

/// @brief Takes a node's properties, leaving its children in the document.
/// @param node The node.
/// @param place Where it is.
/// @return The node, ready for components and styles.
[[nodiscard]] Prepared prepare(const json& node, const std::string& place);

/// @brief A use's children, waiting for the component's `{"slot": true}`.
struct Slot {
	const json* children = nullptr; ///< The use's children (in the document).
	std::string place;              ///< The place of the use.
};

} // namespace affresco::format::checker
