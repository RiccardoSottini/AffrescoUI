#pragma once

/// @file
/// @brief format::Builder: what a node's children are once "use", "repeat", "with", "let", "check", styles and "when"
/// are applied, from nodes compiled once per version of the document.

#include "Record.hpp"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief Turns the nodes of the document into the nodes to build: components instantiated, styles merged, repeats
/// expanded, variables scoped and "when" decided. It creates no panels.
///
/// Each node is resolved (its component instantiated, its static styles merged) and compiled (CompiledNode) once per
/// version of the document, then shared by every update and every panel built from it; a node with a dynamic style
/// is compiled once per style it chooses. Invariants: it never changes the document; its cache belongs to one version
/// of the document (forgetVersion() drops it, and it drops itself when the document is another object). Callers: the
/// reconciler (compiled(), expand(), scoped(), values()) and the properties subsystem (withStyles() for state
/// blocks). Main thread only.
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
	/// @brief A node of the document outside "children" (the root, a screen, a template), its static styles merged,
	/// compiled once per version.
	/// @param node The node, in the document.
	/// @param path Its place, for errors.
	/// @param fill Whether it fills its parent unless it places itself (a screen in an outlet).
	/// @return The compiled node.
	/// @throws DocumentError If a style is unknown.
	[[nodiscard]] CompiledRef compiled(const json& node, const std::string& path, bool fill = false) const;
	/// @brief A node of "overlays", compiled once per version: an overlay placed in the window unless it says
	/// otherwise ("overlay": {"placement": "window"}).
	/// @param node The node, in the document.
	/// @param path Its place, for errors ("overlays.chat").
	/// @return The compiled node.
	/// @throws DocumentError If a style is unknown.
	[[nodiscard]] CompiledRef compiledOverlay(const json& node, const std::string& path) const;
	/// @brief The values of a node an update evaluates, placed as CompiledProperty::evaluated says.
	/// @param node The compiled node.
	/// @param locals Its variables.
	/// @param path Its place, for errors.
	/// @return The values.
	/// @throws DocumentError If one is invalid.
	[[nodiscard]] std::vector<json> values(const CompiledNode& node, const Scope& locals,
	                                       const std::string& path) const;
	/// @brief Expands "use", "repeat", "with", "let", "check", dynamic styles and "when" into the children to build.
	/// @param children The "children" array.
	/// @param locals The parent's variables.
	/// @param path Its place, for errors.
	/// @return The children.
	[[nodiscard]] std::vector<Item> expand(const json& children, const SharedScope& locals,
	                                       const std::string& path) const;
	/// @brief The variables and the final node of one built instance: "with", "let", "check", dynamic style.
	/// @param node The compiled node.
	/// @param scope Its variables so far; gets the new ones.
	/// @param at Its place, for errors.
	/// @return The node, with a dynamic style merged.
	[[nodiscard]] CompiledRef scoped(const CompiledRef& node, SharedScope& scope, const std::string& at) const;
	/// @brief Drops what was compiled: the document changed.
	void forgetVersion() const noexcept;

private:
	/// @brief A node compiled for each style a dynamic "style" chose.
	using Styled = std::vector<std::pair<json, CompiledRef>>;
	/// @brief What is compiled for one version of the document.
	struct Cache {
		const json* document = nullptr;                         ///< The version: the document's object.
		std::unordered_map<const json*, CompiledRef> children;  ///< Children of "children", by their place.
		std::unordered_map<const json*, CompiledRef> nodes;     ///< The root, screens and templates.
		std::unordered_map<const json*, CompiledRef> screens;   ///< Screens as an outlet shows them.
		std::unordered_map<const json*, CompiledRef> overlays;  ///< The nodes of "overlays".
		std::unordered_map<const CompiledNode*, Styled> styled; ///< Nodes with a dynamic style, per style.
	};
	/// @brief The cache, dropped first if it belongs to another version of the document.
	/// @return It.
	Cache& cache() const;
	/// @brief Turns a "use" of a component into its node: the component's node with the use's properties on top, its
	/// params defaulted under "with", and the use's children in its slot.
	/// @param use The node with "use".
	/// @param path Its place, for errors.
	/// @return The node.
	/// @throws DocumentError If the component is unknown.
	[[nodiscard]] json instantiateComponent(const json& use, const std::string& path) const;
	/// @brief A child of "children" with its components instantiated and its static styles merged, compiled.
	/// @param child The child.
	/// @param at Its place, for errors.
	/// @return The node, compiled once per version.
	[[nodiscard]] CompiledRef resolve(const json& child, const std::string& at) const;
	/// @brief A node with a dynamic style, with the style an instance chose merged, compiled.
	/// @param node The node.
	/// @param style The style chosen: a name, a list of names, or null or "" for none.
	/// @param at Its place, for errors.
	/// @return The node.
	/// @throws DocumentError If a style is unknown.
	[[nodiscard]] CompiledRef styled(const CompiledRef& node, const json& style, const std::string& at) const;
	/// @brief The list a "repeat" goes over.
	/// @param repeat The node's "repeat", compiled.
	/// @param locals The variables.
	/// @param at The node's place, for errors.
	/// @return The list, or null to build nothing.
	/// @throws DocumentError If it isn't a list.
	[[nodiscard]] json repeated(const CompiledRepeat& repeat, const Scope& locals, const std::string& at) const;
	/// @brief The list a "repeat" goes over, a data model's rows or repeated(), and records what it read.
	/// @param repeat The node's "repeat", compiled.
	/// @param locals The variables.
	/// @param at The node's place, for errors.
	/// @param place Gets where the list lies, if its items lie in the state or in a model.
	/// @return The list, or null to build nothing.
	/// @throws DocumentError If it isn't a list, or names no registered model.
	[[nodiscard]] json listOf(const CompiledRepeat& repeat, const Scope& locals, const std::string& at,
	                          std::optional<std::string>& place) const;
	/// @brief Adds the instances of one child: one, or one per item of its "repeat".
	/// @param node The child, compiled.
	/// @param locals The parent's variables.
	/// @param at The child's place, for errors.
	/// @param items Gets the instances whose "when" holds.
	void instances(const CompiledRef& node, const SharedScope& locals, const std::string& at,
	               std::vector<Item>& items) const;
	/// @brief The "check" variables of an instance: value, valid and error.
	/// @param node The node.
	/// @param layer The instance's variables so far; gets the new ones.
	/// @param at Its place, for errors.
	void check(const json& node, Scope& layer, const std::string& at) const;

	Engine& engine_;      ///< The runtime.
	mutable Cache cache_; ///< What is compiled for the current version (filled as updates need it).
};

} // namespace affresco::format
