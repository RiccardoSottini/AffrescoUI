#pragma once

/// @file
/// @brief The checker's private parts, shared by its source files: Checker.cpp (passes, findings, hints),
/// CheckerNodes.cpp (nodes, components, styles, handlers and steps) and CheckerValues.cpp (expressions, paths and
/// what the state holds).

#include "Checker.hpp"

#include "Expression.hpp"
#include "Record.hpp"
#include "StateTypes.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
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

inline constexpr std::size_t maxVisits = 1000000; ///< Nodes, steps and values a check visits before giving up.
/// Findings a pass gives a hint ("did you mean ..."): a hint compares the name with every name that exists, so a
/// revision with many findings against a large state would otherwise cost their product. The first are listed.
inline constexpr std::size_t maxHints = 50;
inline constexpr int maxPasses = 5;                ///< Passes that learn what steps write, at most.
inline constexpr int maxDepth = maxNesting;        ///< Nodes and actions inside each other before a branch stops.
inline constexpr std::string_view openScope = "*"; ///< A variable that lets any name through (callers unknown).

/// `$if`, `$and`, `$or` and `$coalesce`: operators that give one of their arguments.
inline constexpr auto choiceOperators = std::to_array<std::string_view>({"$if", "$and", "$or", "$coalesce"});

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
/// @return Its keys, sorted; none if it isn't an object.
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
		std::vector<const std::string*> seen; // keys stay where they are in their objects
		for(const Layer& layer : std::views::reverse(layers))
			for(const auto& [key, value] : layer.properties->items()) {
				if(!layer.holds(key) || std::ranges::any_of(seen, [&](const std::string* s) { return *s == key; }))
					continue;
				seen.push_back(&key);
				visit(key, value, layer);
			}
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

/// @brief Checks one document. Not thread-safe (expressions compile through the main thread's cache); a checker
/// lives for one check.
class Checker {
public:
	/// @brief Prepares a check.
	/// @param document The document; references to it are kept.
	/// @param state The state to check against.
	/// @param nativeActions Actions the application registered; a reference is kept.
	Checker(const json& document, const json& state, const std::set<std::string, std::less<>>& nativeActions);

	/// @brief Runs the passes.
	/// @return The findings of the last pass; std::nullopt if the document is too large (maxVisits).
	[[nodiscard]] std::optional<std::vector<Finding>> run();

private:
	/* Passes (Checker.cpp) */
	/// @brief Walks the whole document once, learning what steps write and collecting findings.
	void pass();
	/// @brief Walks the sections that hold nodes and values: computed, theme, root, screens and templates.
	void walkNodes();
	/// @brief Walks the screen-wide handlers: keys, onLoad and watch.
	void walkHandlers();
	/// @brief Walks styles, components and actions that nothing reached, with every variable allowed.
	void walkUnreached();
	/// @brief Records a finding.
	/// @param place Where.
	/// @param problem What.
	/// @param hint What may be meant, or "".
	void report(const std::string& place, std::string problem, std::string hint = {});
	/// @brief A finding's hint, made only while the pass has hints left (maxHints).
	/// @param make Makes the hint (hintFor()).
	/// @return The hint, or "" once maxHints findings have one.
	template <typename Make> [[nodiscard]] std::string hint(Make make) {
		if(hints_ >= maxHints) return {};
		++hints_;
		return make();
	}
	/// @brief Counts one visit against maxVisits.
	/// @return false once the limit is passed: the check stops.
	[[nodiscard]] bool visit();
	/// @brief A top-level section, if it is an object.
	/// @param name E.g. "actions".
	/// @return The section, or nullptr.
	[[nodiscard]] const json* section(const char* name) const;
	/// @brief An entry of a section.
	/// @param sectionName E.g. "styles".
	/// @param name The entry.
	/// @return The entry, or nullptr.
	[[nodiscard]] const json* definition(const char* sectionName, const std::string& name) const;
	/// @brief The names a section defines.
	/// @param sectionName E.g. "colors".
	/// @return Its keys.
	[[nodiscard]] std::vector<std::string> definedNames(const char* sectionName) const;
	/// @brief Reports a reference to an entry a section doesn't define.
	/// @param kind For the message: "style", "color", ...
	/// @param sectionName The section.
	/// @param name The entry.
	/// @param prefix Written before names in the message ("@" for colours).
	/// @param place Where the reference is.
	void refer(std::string_view kind, const char* sectionName, const std::string& name, std::string_view prefix,
	           const std::string& place);

	/* Nodes (CheckerNodes.cpp) */
	/// @brief Checks a node and its children.
	/// @param node The node.
	/// @param place Where it is.
	/// @param scope Its parent's variables.
	/// @param slot An enclosing use's children, for a `{"slot": true}` inside a component.
	/// @param child Whether it is a child (only children may "use" and "repeat", as in the runtime).
	void walkNode(const json& node, const std::string& place, const Scope& scope, const Slot* slot, bool child);
	/// @brief Checks a node once its component, if any, is in: styles, repeat, variables, properties, children.
	/// @param node The node.
	/// @param scope Its parent's variables.
	/// @param slot A use's children, for a `{"slot": true}` inside a component.
	/// @param child Whether it is a child (only children may "repeat").
	void walkPrepared(Prepared& node, const Scope& scope, const Slot* slot, bool child);
	/// @brief Replaces a use with its component, reporting unknown components and parameters.
	/// @param node The node; becomes the component's node with the use's properties on top.
	/// @param slot Receives the use's children when the component has a slot.
	/// @return false if the component is unknown.
	bool expandUse(Prepared& node, Slot& slot);
	/// @brief Finds a component, reporting it if there is none (or it has no node).
	/// @param name The component.
	/// @param place Where it is used.
	/// @return The component, or nullptr.
	[[nodiscard]] const json* componentNamed(const std::string& name, const std::string& place);
	/// @brief Gives a component's node its parameters: the defaults, then the use's "with".
	/// @param use The node with "use".
	/// @param component The component.
	/// @param name Its name.
	/// @param places Receives where each parameter's value was written.
	/// @param written Whether @p use is written in the document (its unknown parameters are reported).
	/// @return The parameters, the node's "with".
	[[nodiscard]] json takeParameters(const Prepared& use, const json& component, const std::string& name,
	                                  std::map<std::string, std::string>& places, bool written);
	/// @brief Merges static styles into a node, reporting unknown ones.
	/// @param node The node.
	/// @param depth Styles inside styles so far.
	void applyStyles(Prepared& node, int depth);
	/// @brief Adds the layers of styles, and of the styles they use, reporting unknown ones.
	/// @param names A style's name, or a list of them.
	/// @param place Where they are named.
	/// @param depth Styles inside styles so far.
	/// @param styles Receives the layers, the ones that win last.
	void mergeStyles(const json& names, const std::string& place, int depth, std::vector<Layer>& styles);
	/// @brief Adds a repeat's variables: the item and its index.
	/// @param node The node.
	/// @param scope The layer to add them to.
	void walkRepeat(const Prepared& node, Scope& scope);
	/// @brief Adds the variables of "with", "let" and "check", each seeing the ones before it.
	/// @param node The node.
	/// @param layer The layer to add them to.
	void walkLayer(const Prepared& node, Scope& layer);
	/// @brief Adds the variables of "check": value, valid and error.
	/// @param node The node.
	/// @param check Its "check".
	/// @param layer The layer to add them to.
	void walkCheck(const Prepared& node, const json& check, Scope& layer);
	/// @brief Checks a node's own properties: values, references and handlers.
	/// @param node The node.
	/// @param scope Its variables.
	void walkProperties(const Prepared& node, const Scope& scope);
	/// @brief Checks one property of a node.
	/// @param node The node.
	/// @param key The property.
	/// @param value Its value.
	/// @param layer The object it was written in.
	/// @param scope The node's variables.
	void walkProperty(const Prepared& node, const std::string& key, const json& value, const Layer& layer,
	                  const Scope& scope);
	/// @brief Checks a property that holds behaviour: an event handler, "keys" or "every".
	/// @param key The property.
	/// @param value Its value.
	/// @param scope The node's variables.
	/// @param place Where it is.
	/// @return false if the property holds no behaviour.
	bool walkBehaviour(const std::string& key, const json& value, const Scope& scope, const std::string& place);
	/// @brief Checks a style chosen by an expression: its names, and each style it may choose.
	/// @param node The node.
	/// @param style The expression.
	/// @param place Where it is.
	/// @param scope Its variables.
	void walkDynamicStyle(const Prepared& node, const json& style, const std::string& place, const Scope& scope);
	/// @brief Checks the names of styles, fonts and colours a property refers to.
	/// @param key The property.
	/// @param value Its value.
	/// @param place Where it is.
	void references(const std::string& key, const json& value, const std::string& place);
	/// @brief Checks "bind" or "caret": a state path the control reads and writes.
	/// @param key "bind" or "caret".
	/// @param value The path, or an expression giving one.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkBinding(const std::string& key, const json& value, const Scope& scope, const std::string& place);
	/// @brief Checks "overlay", whose "open" is a state path that "dismiss" writes.
	/// @param value The overlay.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkOverlay(const json& value, const Scope& scope, const std::string& place);
	/// @brief Checks a node's children, putting a use's children where the component has its slot.
	/// @param node The node.
	/// @param scope Its variables.
	/// @param slot The use's children, if any.
	void walkChildren(const Prepared& node, const Scope& scope, const Slot* slot);
	/// @brief Checks a use's children on their own (when the component's body was already checked).
	/// @param slot The use's children.
	/// @param scope The variables they see.
	void walkSlot(const Slot& slot, const Scope& scope);

	/* Behaviour (CheckerNodes.cpp) */
	/// @brief Checks a handler: an action name or steps.
	/// @param handler The handler.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkHandler(const json& handler, const Scope& scope, const std::string& place);
	/// @brief Checks a list of steps, or one step.
	/// @param steps The steps.
	/// @param scope The variables.
	/// @param place Where they are.
	void walkSteps(const json& steps, const Scope& scope, const std::string& place);
	/// @brief Checks one step.
	/// @param step The step.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkStep(const json& step, const Scope& scope, const std::string& place);
	/// @brief Checks the values a step reads, and the screen a "navigate" names.
	/// @param kind The step.
	/// @param step The step's object.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkStepValues(std::string_view kind, const json& step, const Scope& scope, const std::string& place);
	/// @brief Checks the steps inside a step: then, else, do and onError (which sees `error`).
	/// @param step The step.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkNestedSteps(const json& step, const Scope& scope, const std::string& place);
	/// @brief Records what a state step (set, push, toggle, increment, animate, remove, call into) writes.
	/// @param kind The step.
	/// @param step The step's object.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkWrite(std::string_view kind, const json& step, const Scope& scope, const std::string& place);
	/// @brief Checks a "run" step: its "with" values, then the action with them as variables.
	/// @param step The step.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkRun(const json& step, const Scope& scope, const std::string& place);
	/// @brief Checks a "forEach" step: its list, then its steps with the item as a variable.
	/// @param step The step.
	/// @param scope The variables.
	/// @param place Where it is.
	void walkForEach(const json& step, const Scope& scope, const std::string& place);
	/// @brief Checks an action run by name, with the caller's variables (as the runtime runs it).
	/// @param name The action.
	/// @param scope The caller's variables.
	/// @param place Where it is named.
	void callAction(const std::string& name, const Scope& scope, const std::string& place);

	/* Values (CheckerValues.cpp) */
	/// @brief Checks a value as evaluate() reads it: templates, expressions, operators, objects and lists.
	/// @param value The value.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it may hold.
	[[nodiscard]] Shape shapeOf(const json& value, const Scope& scope, const std::string& place);
	/// @brief Checks a value as shapeOf() does, for what it refers to only: no shape is built.
	/// @param value The value.
	/// @param scope The variables.
	/// @param place Where it is.
	void read(const json& value, const Scope& scope, const std::string& place);
	/// @brief Checks a string's placeholders.
	/// @param text The string.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it may hold: a lone placeholder keeps its type.
	[[nodiscard]] Shape shapeOfText(std::string_view text, const Scope& scope, const std::string& place);
	/// @brief Checks the inside of one `{{ }}`.
	/// @param text The expression.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it may hold.
	[[nodiscard]] Shape shapeOfExpression(std::string_view text, const Scope& scope, const std::string& place);
	/// @brief Checks one `$` operator.
	/// @param name E.g. "$get".
	/// @param raw Its arguments, unevaluated.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it may give.
	[[nodiscard]] Shape shapeOfOperator(const std::string& name, const json& raw, const Scope& scope,
	                                    const std::string& place);
	/// @brief Checks the other operators: their arguments, and what kind of value they give.
	/// @param name The operator.
	/// @param raw Its arguments.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it may give.
	[[nodiscard]] Shape shapeOfResult(const std::string& name, const json& raw, const Scope& scope,
	                                  const std::string& place);
	/// @brief Checks `$if`, `$and`, `$or` and `$coalesce`, which give one of their arguments.
	/// @param name The operator.
	/// @param raw Its arguments.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return The merge of what they may give.
	[[nodiscard]] Shape shapeOfChoice(const std::string& name, const json& raw, const Scope& scope,
	                                  const std::string& place);
	/// @brief Checks the list functions, whose second argument sees each item as `it`.
	/// @param name E.g. "$filter".
	/// @param raw Its arguments.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it may give.
	[[nodiscard]] Shape shapeOfItems(const std::string& name, const json& raw, const Scope& scope,
	                                 const std::string& place);
	/// @brief Checks a compiled path, `{"$get": [root, key, ...]}`.
	/// @param raw The operator's arguments.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it reads.
	[[nodiscard]] Shape shapeOfGet(const json& raw, const Scope& scope, const std::string& place);
	/// @brief Checks a path written as text: "state.items[state.current].title".
	/// @param text The path.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it reads.
	[[nodiscard]] Shape shapeOfPath(std::string_view text, const Scope& scope, const std::string& place);
	/// @brief Follows a path from its variable, reporting what doesn't exist.
	/// @param root The variable.
	/// @param keys The keys after it.
	/// @param scope The variables.
	/// @param place Where it is.
	/// @return What it reads.
	[[nodiscard]] Shape follow(const std::string& root, const std::vector<PathKey>& keys, const Scope& scope,
	                           const std::string& place);
	/// @brief Follows keys from a shape.
	/// @param start The shape.
	/// @param described The path so far, as written, for messages.
	/// @param keys The keys.
	/// @param first The first key to follow.
	/// @param scope The variables (for index expressions).
	/// @param place Where it is.
	/// @return What it reads.
	[[nodiscard]] Shape walkKeys(const Shape& start, std::string described, const std::vector<PathKey>& keys,
	                             std::size_t first, const Scope& scope, const std::string& place);
	/// @brief Reads one key of a shape.
	/// @param shape The shape.
	/// @param described Its path, for messages.
	/// @param name The key.
	/// @param place Where it is.
	/// @return The key's shape; anything (after a finding) if it can't be there.
	[[nodiscard]] const Shape& member(const Shape& shape, const std::string& described, const std::string& name,
	                                  const std::string& place);
	/// @brief A variable's shape, found as the runtime finds variables.
	/// @param name The variable.
	/// @param scope The variables in scope.
	/// @return Its shape, or nullptr if there is no such variable.
	[[nodiscard]] const Shape* variable(const std::string& name, const Scope& scope) const;
	/// @brief The names visible in a scope, for suggestions.
	/// @param scope The variables.
	/// @return Local and screen-wide names.
	[[nodiscard]] static std::vector<std::string> visibleNames(const Scope& scope);
	/// @brief The local names in a scope, sorted and joined: actions and components are checked once per set.
	/// @param scope The variables.
	/// @return E.g. "c,card,i".
	[[nodiscard]] static std::string variableNames(const Scope& scope);
	/// @brief A computed value's shape, checking its expression the first time in a pass.
	/// @param name The computed value.
	/// @return Its shape (anything inside a circle).
	[[nodiscard]] const Shape& computedShape(const std::string& name);
	/// @brief Splits a text path into keys, as the runtime does.
	/// @param text The path.
	/// @param place Where it is.
	/// @return The keys, the variable first; std::nullopt (after a finding) if it is malformed.
	[[nodiscard]] std::optional<std::vector<PathKey>> parsePath(std::string_view text, const std::string& place);
	/// @brief Reads what is inside a path's brackets: an index, or an expression giving a key.
	/// @param inner The text inside the brackets.
	/// @param place Where the path is.
	/// @return The key (neither an index nor an expression, after a finding, if it doesn't parse).
	[[nodiscard]] PathKey parseIndex(std::string_view inner, const std::string& place);
	/// @brief Checks the state path a step or control writes, and records what it writes, so reading it is fine.
	/// @param target The state path.
	/// @param value What is written.
	/// @param change How it is written.
	/// @param scope The variables (for index expressions).
	/// @param place Where it is.
	void write(const json& target, const Shape& value, Change change, const Scope& scope, const std::string& place);

	/* Typed state (CheckerTypes.cpp) */
	/// @brief Reads "stateTypes": what the state may hold gains each declared key and property.
	void learnTypes();
	/// @brief The declared type at a state path.
	/// @param keys The path's keys, "state" first.
	/// @return The type, or nullptr if none is declared there.
	[[nodiscard]] const json* typeOf(const std::vector<PathKey>& keys) const;
	/// @brief The kinds of value a value as written may give: its own, or an expression's, when known.
	/// @param value The value.
	/// @return The kinds; anyKind when unknown.
	[[nodiscard]] Kinds kindsOf(const json& value) const;
	/// @brief The kinds of value a compiled expression may give.
	/// @param tree The operator tree.
	/// @return The kinds; anyKind when unknown.
	[[nodiscard]] Kinds kindsOfTree(const json& tree) const;
	/// @brief The kinds of value `$if`, `$and`, `$or` and `$coalesce` may give: one of their arguments'.
	/// @param name The operator.
	/// @param raw Its arguments.
	/// @return The kinds; anyKind for another operator.
	[[nodiscard]] Kinds kindsOfChoice(std::string_view name, const json& raw) const;
	/// @brief Checks a value written to a typed place: in full if it is used as it is, else the kinds it may give.
	/// @param type The type there.
	/// @param value The value as written.
	/// @param keys The path written to, "state" first.
	/// @param place Where the value is.
	/// @param inside What is written inside the path, for the message: "" or "[...]" for an item pushed.
	void checkValue(const json& type, const json& value, const std::vector<PathKey>& keys, const std::string& place,
	                std::string_view inside);
	/// @brief Checks what a state step writes against the type declared where it writes.
	/// @param kind The step: set, push, toggle, increment or animate.
	/// @param step The step's object.
	/// @param place Where it is.
	void checkWrite(std::string_view kind, const json& step, const std::string& place);
	/// @brief Reports a value whose kinds a type doesn't take.
	/// @param type The type.
	/// @param kinds What the value may be.
	/// @param keys The path written to, "state" first.
	/// @param place Where the value is.
	/// @param what Who writes, for the message: "this writes", "a slider writes".
	void expectKinds(const json& type, Kinds kinds, const std::vector<PathKey>& keys, const std::string& place,
	                 std::string_view what);
	/// @brief Checks a widget's "bind" against the type declared at the path: a toggle writes true or false, a
	/// slider a number, a text area or an editor text.
	/// @param node The node.
	/// @param target The state path.
	/// @param place Where it is.
	void checkBind(const Prepared& node, const json& target, const std::string& place);
	/// @brief Checks `a == b` and `a != b` where one side is a state path with an "enum" or a "const" and the other
	/// a value it never takes.
	/// @param raw The operator's arguments.
	/// @param place Where it is.
	void checkComparison(const json& raw, const std::string& place);

	const json& document_;                                    ///< The document.
	const json* types_ = nullptr;                             ///< Its "stateTypes", if it has them.
	const std::set<std::string, std::less<>>& nativeActions_; ///< Registered actions.
	Shape state_;                                             ///< What the state may hold, learnt pass by pass.
	Learning learnt_ = Learning::nothing;                     ///< What this pass learnt about the state.
	std::set<Finding> findings_;                              ///< This pass's findings.
	std::set<std::string> walked_; ///< Actions and components checked, with their variables' names.
	std::set<std::string, std::less<>> actionsReached_;    ///< Actions a node or handler runs.
	std::set<std::string, std::less<>> componentsReached_; ///< Components a node uses.
	/// Computed values checked in this pass. A map, not a JSON object (a vector): a shape read through a reference
	/// stays where it is while other computed values are added.
	std::map<std::string, Shape, std::less<>> computedShapes_;
	std::vector<std::string> computing_; ///< Computed values being checked (finds circles).
	std::size_t visits_ = 0;             ///< Visits so far (maxVisits).
	bool exhausted_ = false;             ///< maxVisits was passed.
	std::size_t hints_ = 0;              ///< Hints made in this pass (maxHints).
	int nodeDepth_ = 0;                  ///< Nodes inside each other now (maxDepth).
	int actionDepth_ = 0;                ///< Actions inside each other now (maxActionDepth).
	int styleDepth_ = 0;                 ///< Styles chosen inside chosen styles now (16 at most).
};

} // namespace affresco::format::checker
