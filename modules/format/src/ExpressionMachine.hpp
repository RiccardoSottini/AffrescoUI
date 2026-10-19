#pragma once

/// @file
/// @brief The typed form of expressions and operators, and the machine that evaluates it on Values.
///
/// compile() parses an expression into an operator tree (JSON, which the Checker reads) and lowers that tree once into
/// Nodes: names resolved to functions, paths split into keys, literals made Values. The machine walks the Nodes. Places
/// for errors are built on the stack as it descends and written out only when an error is reported, so a binding that
/// succeeds allocates only what its result needs.

#include "Expression.hpp"
#include "ExpressionFunctions.hpp"
#include "Value.hpp"

#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format::detail {

/// @brief Where a value is in the document, for errors: "kanban.root.children[2].text.$add[1]".
struct Place {
	/// No index.
	static constexpr std::size_t noIndex = std::numeric_limits<std::size_t>::max();

	/// @brief The outermost place.
	/// @param where What evaluate() was given; must outlive the place.
	explicit Place(const std::string& where) noexcept : root(&where) {}
	/// @brief A place inside another: "<parent>.<key>".
	/// @param outer The enclosing place; must outlive this one.
	/// @param name The key; must outlive the place.
	Place(const Place& outer, std::string_view name) noexcept : parent(&outer), key(name) {}
	/// @brief An item inside another place: "<parent>[<position>]".
	/// @param outer The enclosing place; must outlive this one.
	/// @param position The item's position.
	Place(const Place& outer, std::size_t position) noexcept : parent(&outer), index(position) {}

	const Place* parent = nullptr;     ///< The enclosing place; nullptr for the outermost.
	const std::string* root = nullptr; ///< The outermost place: what evaluate() was given.
	std::string_view key;              ///< ".key" after the parent's text, if not empty.
	std::size_t index = noIndex;       ///< "[index]" after that, if set.

	/// @brief The place as text.
	[[nodiscard]] std::string text() const;
};

/// What a path starts from.
enum class Root : std::uint8_t {
	State,    ///< `state`.
	Window,   ///< `window`.
	Revision, ///< `revision`.
	Event,    ///< `event`.
	Computed, ///< `computed`: the next key names a computed value, evaluated when read.
	Nodes,    ///< `nodes`: the next key names a built panel, unless a variable is called that.
	Named,    ///< A local variable or a global.
};

/// @brief One key of a path, after the variable.
struct Key {
	/// What the key is.
	enum class Kind : std::uint8_t {
		Name,       ///< An object's key (or `length`).
		Index,      ///< A list's position.
		Expression, ///< Evaluated: a name if it gives a text, a position if it gives an integer.
	};
	Kind kind = Kind::Name; ///< What it is.
	std::string name;       ///< Kind::Name.
	std::int64_t index = 0; ///< Kind::Index.
	std::size_t child = 0;  ///< Kind::Expression: the Node in Node::children.
};

/// @brief A key of a path, ready to follow (ExpressionMachine.cpp).
struct PathKey;

/// @brief An expression or operator, typed.
struct Node {
	/// What the node does.
	enum class Kind : std::uint8_t {
		Literal,  ///< Gives `value`.
		List,     ///< A list of `children`.
		Object,   ///< An object: `names` with `children`.
		Template, ///< A text with `{{ }}` placeholders: `text`.
		Get,      ///< A path: a variable (`root`, `text`) or `children.back()`, then `keys`.
		Path,     ///< A path written as text, read when evaluated (`$path`): `text`.
		Call,     ///< A function (`function`) of `children`; with no function, an unknown operator (`label`).
		Fail,     ///< An invalid operator: fails with `text` when it is evaluated (an unused branch never fails).
	};
	Kind kind = Kind::Literal;          ///< What it does.
	Value value;                        ///< Kind::Literal.
	std::string text;                   ///< The template, the path, the variable's name, or the problem.
	std::string label;                  ///< The operator it stands for ("$add"): errors name "<place>.$add".
	Root root = Root::Named;            ///< Kind::Get with a variable.
	bool fromValue = false;             ///< Kind::Get: starts from the value of `children.back()` instead.
	std::vector<Key> keys;              ///< Kind::Get.
	std::vector<std::string> names;     ///< Kind::Object: the key of each child.
	std::vector<Node> children;         ///< Operands, items, fields.
	const Function* function = nullptr; ///< Kind::Call.
	bool indexed = false;               ///< Kind::Call: arguments are placed "$name[i]" (evaluated ones), else "$name".
};

/// @brief Lowers an operator tree, or any value holding operators and templates, into its typed form.
/// @param tree A tree compile() parsed, or a value written in a document.
/// @return The node. Never throws for a malformed operator: that becomes a Kind::Fail node.
[[nodiscard]] Node lower(const json& tree);

/// @brief A compiled expression: its operator tree and its typed form. Shared, so that an expression still running
/// stays alive when the compile cache is cleared under it.
struct Compiled {
	json tree;    ///< What compile() returns.
	Node program; ///< What the machine runs.
};

/// @brief Compiles the inside of a `{{ }}`, or finds it in the cache.
/// @param text The expression.
/// @param place Its place, written into a syntax error.
/// @return It.
/// @throws DocumentError If it isn't a valid expression.
[[nodiscard]] std::shared_ptr<const Compiled> compiled(std::string_view text, const Place& place);

/// @brief A text with `{{ }}` placeholders, split once: the texts between them and their compiled expressions.
struct Template {
	/// @brief A text, and the placeholder after it.
	struct Piece {
		std::string text;                           ///< Inserted as it is.
		std::shared_ptr<const Compiled> expression; ///< The placeholder's expression; null after the last text.
	};
	bool whole = false;         ///< One placeholder and nothing else: the value keeps its own type.
	std::vector<Piece> pieces;  ///< In order.
	std::exception_ptr failure; ///< What stops it after its pieces: an unclosed `{{` or an invalid expression. A
	                            ///< template that fails is never cached, since the error names the place it was used.
};

/// @brief Splits a text with placeholders, compiling their expressions.
/// @param text The text; it has a `{{`.
/// @param place Its place, written into errors.
/// @return It (with a failure, if it is invalid).
[[nodiscard]] std::shared_ptr<Template> splitTemplate(std::string_view text, const Place& place);

/// @brief Splits a text with placeholders, or finds it in the cache.
/// @param text The text; it has a `{{`.
/// @param place Its place, written into errors.
/// @return It (with a failure, if it is invalid).
[[nodiscard]] std::shared_ptr<const Template> templateOf(std::string_view text, const Place& place);

/// @brief Evaluates typed expressions for one call of evaluate(), lookup() or statePointer().
///
/// Holds the context, and the `it` and `index` of the list functions running. Lives on the stack for the call; runs on
/// the thread that called it (the main thread, which owns the compile cache).
class Machine {
public:
	/// @brief Prepares to evaluate.
	/// @param context What paths can see; must outlive the machine.
	explicit Machine(const Context& context) noexcept : context_(context) {}

	/// @brief Evaluates a value written in a document: templates, operators, and objects and lists holding them.
	/// @param value The value.
	/// @param place Its place.
	/// @return The result.
	/// @throws DocumentError If something in it is invalid.
	[[nodiscard]] json evaluateDocument(const json& value, const Place& place);
	/// @brief Evaluates a node.
	/// @param node The node.
	/// @param place Its place.
	/// @return Its value.
	/// @throws DocumentError If it is invalid.
	[[nodiscard]] Value run(const Node& node, const Place& place);
	/// @brief Evaluates a node with an item of a list in scope as `it`, and its position as `index`.
	/// @param node The node.
	/// @param place Its place.
	/// @param item The item.
	/// @param position Its position.
	/// @return Its value.
	/// @throws DocumentError If it is invalid.
	[[nodiscard]] Value runFor(const Node& node, const Place& place, const Value& item, std::size_t position);
	/// @brief Evaluates a text with `{{ }}` placeholders: a lone placeholder keeps its value's type.
	/// @param text The text.
	/// @param place Its place.
	/// @return The value, or the text with the values inserted.
	/// @throws DocumentError If a placeholder is unclosed or invalid.
	[[nodiscard]] Value runTemplate(std::string_view text, const Place& place);
	/// @brief Splits a path written as text into its variable and keys; `[expr]` keys are evaluated.
	/// @param path E.g. "state.items[state.current]".
	/// @param place Its place.
	/// @return The variable's name (a text), then the keys (texts and integers).
	/// @throws DocumentError If the path is malformed.
	[[nodiscard]] std::vector<Value> segments(std::string_view path, const Place& place);
	/// @brief Reads the value at a path written as text.
	/// @param path The path.
	/// @param place Its place.
	/// @return The value, or null.
	/// @throws DocumentError If the path is malformed or its variable unknown.
	[[nodiscard]] Value lookup(std::string_view path, const Place& place);

	/// @brief Reports a problem.
	/// @param place Where.
	/// @param message What is wrong.
	/// @throws DocumentError Always.
	[[noreturn]] static void fail(const Place& place, const std::string& message);

private:
	/// @brief An item of a list function, in scope as `it` and `index`.
	struct Item {
		const Item* parent; ///< The enclosing list function's, or nullptr.
		const Value& value; ///< `it`.
		std::size_t index;  ///< `index`.
	};

	/// @brief Evaluates a function call.
	/// @param node The call.
	/// @param place Its place.
	/// @return Its value.
	/// @throws DocumentError If it is invalid.
	[[nodiscard]] Value call(const Node& node, const Place& place);
	/// @brief Evaluates a path node.
	/// @param node The node.
	/// @param place Its place.
	/// @return The value, or null.
	/// @throws DocumentError If its variable is unknown, or a key is invalid.
	[[nodiscard]] Value get(const Node& node, const Place& place);
	/// @brief Finds a variable by name, as a path's start.
	/// @param root What kind of variable.
	/// @param name Its name.
	/// @param place The path's place, for errors.
	/// @return Its value, or std::nullopt for `computed` and `nodes`, which read the next key.
	/// @throws DocumentError If there is no such variable.
	[[nodiscard]] std::optional<Value> variable(Root root, std::string_view name, const Place& place) const;
	/// @brief Follows keys from a variable.
	/// @param root What kind of variable.
	/// @param name Its name.
	/// @param keys The keys.
	/// @param place The path's place, for errors.
	/// @return The value, or null if it doesn't exist.
	/// @throws DocumentError If the variable is unknown.
	[[nodiscard]] Value walk(Root root, std::string_view name, std::span<const PathKey> keys, const Place& place) const;

	const Context& context_;     ///< What paths can see.
	const Item* item_ = nullptr; ///< The innermost list function's item, if one is running.
};

/// @brief What a variable's name stands for.
/// @param name The name.
/// @return Its kind.
[[nodiscard]] Root rootOf(std::string_view name) noexcept;

/// @brief Finds the `}}` that closes a placeholder, skipping quoted text and object literals inside it.
/// @param text The string.
/// @param from Just after the `{{`.
/// @return Its position, or npos.
[[nodiscard]] std::size_t closing(std::string_view text, std::size_t from) noexcept;

} // namespace affresco::format::detail
