#pragma once

/// @file
/// @brief format::Builder: what a node's children are once "use", "repeat", "with", "let", "check", styles and "when"
/// are applied.

#include "Record.hpp"

#include <string>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief Turns the nodes of the document into the nodes to build: components instantiated, styles merged, repeats
/// expanded, variables scoped and "when" decided. It creates no panels.
///
/// Invariants: it never changes the document; an Item's node is either borrowed from the document (which outlives
/// the update) or a copy of its own. Callers: the reconciler (expand(), scoped(), evaluateNode()) and the properties
/// subsystem (withStyles() for state blocks). Main thread only.
class Builder {
public:
	/// @brief A builder for an engine's document.
	/// @param engine The runtime; outlives this object.
	explicit Builder(Engine& engine) : engine_(engine) {}

	/// @brief Merges a node's properties on top of its styles' (later styles override earlier ones; a style may itself
	/// have a "style").
	/// @param node The node.
	/// @param path Its place, for errors.
	/// @param depth Styles inside styles so far (recursion guard).
	/// @return The merged node.
	/// @throws DocumentError If a style is unknown.
	[[nodiscard]] json withStyles(const json& node, const std::string& path, int depth = 0) const;
	/// @brief Evaluates a node's properties (structural keys are left as they are).
	/// @param merged The node, styles merged.
	/// @param locals Its variables.
	/// @param path Its place, for errors.
	/// @return The evaluated properties.
	[[nodiscard]] json evaluateNode(const json& merged, const Scope& locals, const std::string& path) const;
	/// @brief Expands "use", "repeat", "with", "let", "check", dynamic styles and "when" into the children to build.
	/// @param children The "children" array.
	/// @param locals The parent's variables.
	/// @param path Its place, for errors.
	/// @return The children.
	[[nodiscard]] std::vector<Item> expand(const json& children, const SharedScope& locals,
	                                       const std::string& path) const;
	/// @brief The variables and the final node of one built instance: "with", "let", "check", dynamic style.
	/// @param shared The node.
	/// @param scope Its variables so far; gets the new ones.
	/// @param at Its place, for errors.
	/// @return The node, with a dynamic style merged.
	[[nodiscard]] NodeRef scoped(const NodeRef& shared, SharedScope& scope, const std::string& at) const;

private:
	/// @brief Turns a "use" of a component into its node: the component's node with the use's properties on top, its
	/// params defaulted under "with", and the use's children in its slot.
	/// @param use The node with "use".
	/// @param path Its place, for errors.
	/// @return The node.
	/// @throws DocumentError If the component is unknown.
	[[nodiscard]] json instantiateComponent(const json& use, const std::string& path) const;
	/// @brief A child of "children" with its components instantiated and its static styles merged.
	/// @param child The child.
	/// @param at Its place, for errors.
	/// @return The node, borrowed from the document when nothing changed.
	[[nodiscard]] NodeRef resolve(const json& child, const std::string& at) const;
	/// @brief The list a "repeat" goes over: "in" as a path ("state.messages") or as any value.
	/// @param repeat The "repeat" object.
	/// @param locals The variables.
	/// @param at The node's place, for errors.
	/// @return The list, or null to build nothing.
	/// @throws DocumentError If it isn't a list.
	[[nodiscard]] json repeated(const json& repeat, const Scope& locals, const std::string& at) const;
	/// @brief The "check" variables of an instance: value, valid and error.
	/// @param node The node.
	/// @param layer The instance's variables so far; gets the new ones.
	/// @param at Its place, for errors.
	void check(const json& node, Scope& layer, const std::string& at) const;

	Engine& engine_; ///< The runtime.
};

} // namespace affresco::format
