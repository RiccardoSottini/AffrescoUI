#pragma once

/// @file
/// @brief format::CompiledNode: a node of one version of the document, compiled once: its styles and component
/// resolved, each property a typed slot with its value compiled, its scoped variables and repeat ready to evaluate.

#include "Binding.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::format {

/// @brief A node to build: shared, so a repeat's items don't each copy their template (and its children).
using NodeRef = std::shared_ptr<const json>;

/// @brief What a property of a node sets, decided when the node is compiled: the update then dispatches on it
/// instead of comparing the key's text.
enum class Slot : std::uint8_t {
	/* Every panel's (Properties::commonProperty) */
	Id,         ///< "id".
	Color,      ///< "color".
	Visible,    ///< "visible".
	Cursor,     ///< "cursor".
	Tooltip,    ///< "tooltip".
	Disabled,   ///< "disabled".
	Transition, ///< "transition".
	Scroll,     ///< "scroll".
	Overlay,    ///< "overlay".
	Focusable,  ///< "focusable".
	TabIndex,   ///< "tabIndex".
	Arrows,     ///< "arrows".
	Mnemonic,   ///< "mnemonic".
	/* Every panel's accessibility */
	Role,        ///< "role".
	Label,       ///< "label".
	Description, ///< "description".
	Open,        ///< "isOpen": expanded, to assistive technologies.
	Selected,    ///< "selected".
	Live,        ///< "live".
	/* Every panel's shape */
	Radius,      ///< "radius".
	BorderWidth, ///< "borderWidth".
	BorderColor, ///< "borderColor".
	Shadow,      ///< "shadow".
	Gradient,    ///< "gradient".
	Cache,       ///< "cache".
	FocusRing,   ///< "focusRing".
	/* Every panel's layout (layoutProperty()) */
	Layout, ///< x, y, width, height, minWidth, minHeight, grow, fill, anchors.
	/* Labels, text areas, rows, columns and grids */
	TextColor,        ///< "textColor".
	Padding,          ///< "padding".
	Wrap,             ///< "wrap": a label's or a stack's.
	Font,             ///< "font".
	Align,            ///< "align": a label's, or a stack's (its first name).
	FitText,          ///< "fitText".
	Placeholder,      ///< "placeholder".
	PlaceholderColor, ///< "placeholderColor".
	Gap,              ///< "gap": a stack's or a grid's.
	Direction,        ///< "direction".
	Columns,          ///< "columns".
	/* Read with the widget's value or its behaviour, not applied with the look */
	Value,      ///< "type", "autofocus", "locked", "checked", "value", "text".
	StateBlock, ///< "hover", "focused", "dragging", "dragOver", "pressed".
	/* Structural: read by the behaviour, never evaluated */
	Handler, ///< "onClick" and the other handlers.
	Bind,    ///< "bind".
	Caret,   ///< "caret".
	Keys,    ///< "keys".
	Drag,    ///< "drag".
	Context, ///< "context".
	Every,   ///< "every".
	/* Anything else */
	Other, ///< A property a registered type has, or none has (an error when it is applied).
};

/// @brief The slot of a property.
/// @param key Its key in a node.
/// @return The slot; Slot::Other for one the runtime doesn't apply itself.
[[nodiscard]] Slot slotOf(std::string_view key);

/// @brief One property of a compiled node.
struct CompiledProperty {
	std::string key;                 ///< Its key in the node.
	Slot slot = Slot::Other;         ///< What it sets.
	Binding value;                   ///< Its value, compiled (a constant for structural keys).
	std::size_t evaluated = noValue; ///< Its place among the values an update evaluates; noValue if it is static.

	/// No place: the value as written is used.
	static constexpr std::size_t noValue = static_cast<std::size_t>(-1);
};

/// @brief A "with" or "let" variable of a compiled node.
struct CompiledVariable {
	std::string name; ///< Its name.
	Binding value;    ///< Its value.
};

/// @brief A node's "repeat", compiled.
struct CompiledRepeat {
	std::string item = "item";   ///< "for": the item's variable.
	std::string index = "index"; ///< "index": the position's variable.
	std::unique_ptr<json> path;  ///< "in" written as a path ("state.messages"): its value as "{{...}}".
	Binding list;                ///< "in", compiled.
	std::optional<Binding> key;  ///< "key", compiled.
};

/// @brief A node of one version of the document, compiled: what an update needs, decided once.
///
/// Built by the Builder from a resolved node (components instantiated, static styles merged) and shared by every
/// panel built from it, so a repeat's thousand items share one. It owns the node's keys but "children", which its
/// bindings refer to, so a record can keep it after the document changes. Its children are not copied: they stay in
/// the document, or in the component's instance it keeps, and an update reads them while that version is current.
/// Immutable once built.
struct CompiledNode {
	CompiledNode() = default;
	CompiledNode(const CompiledNode&) = delete;
	CompiledNode& operator=(const CompiledNode&) = delete;
	CompiledNode(CompiledNode&&) = delete;
	CompiledNode& operator=(CompiledNode&&) = delete;
	~CompiledNode() = default;

	json own;                       ///< The node's keys but "children": what the bindings refer to.
	const json* children = nullptr; ///< Its "children" as written, or nullptr: in the document, or in keep.
	NodeRef keep;                   ///< What holds children when the document doesn't (a component's instance).
	std::string type = "panel";     ///< "type".
	bool typeIsText = true;         ///< false: "type" isn't text, an error where the node is built.
	std::vector<CompiledProperty> properties; ///< Every key but "children", "style" and the scoping ones, in order.
	std::size_t evaluatedCount = 0;           ///< How many values an update evaluates.
	bool hasId = false;                       ///< It has an "id".
	bool hasStateBlocks = false;              ///< It has "hover", "pressed", ...
	/// Applying it reads more than its values, so it is applied on every update: an overlay's "open" written as a
	/// state path, a "bind" or "caret" computed from variables, or an "every" whose period is computed.
	bool alwaysApply = false;

	std::vector<CompiledVariable> with;   ///< "with", in order.
	std::vector<CompiledVariable> let;    ///< "let", in order.
	bool malformedWith = false;           ///< "with" isn't an object: reported where the node is built.
	bool malformedLet = false;            ///< "let" isn't an object: reported where the node is built.
	std::optional<Binding> style;         ///< A dynamic "style", chosen per instance.
	std::optional<Binding> when;          ///< "when".
	std::optional<CompiledRepeat> repeat; ///< "repeat", when it is well formed.

	/// @brief A property's value: evaluated by the update, or as written.
	/// @param property One of properties.
	/// @param values The values the update evaluated.
	/// @return The value.
	[[nodiscard]] static const json& valueOf(const CompiledProperty& property, const std::vector<json>& values) {
		return property.evaluated == CompiledProperty::noValue ? property.value.written() : values[property.evaluated];
	}
	/// @brief Finds a property.
	/// @param key Its key.
	/// @return It, or nullptr.
	[[nodiscard]] const CompiledProperty* find(std::string_view key) const noexcept;
};

/// @brief A compiled node, shared by the panels built from it.
using CompiledRef = std::shared_ptr<const CompiledNode>;

/// @brief A node's keys but "children", copied.
/// @param node The node, an object.
/// @return The copy.
[[nodiscard]] json withoutChildren(const json& node);

/// @brief Compiles a resolved node. Malformed scoping keys are kept for the builder to report at each instance's
/// place, as building reports them.
/// @param keys The node's keys (components instantiated, static styles merged). A "children" among them came from a
/// style: it is used if @p children is null, and kept by the compiled node.
/// @param children The node's own "children", or nullptr; outlives the compiled node's use in an update.
/// @param keep What holds @p children if the document doesn't, or null.
/// @return The compiled node.
[[nodiscard]] CompiledRef compileNode(json keys, const json* children = nullptr, NodeRef keep = nullptr);

/// @brief Compiles a node that holds its children (a component's instance), keeping it for them.
/// @param node The node.
/// @return The compiled node.
[[nodiscard]] CompiledRef compileNode(const NodeRef& node);

} // namespace affresco::format
