#pragma once

/// @file
/// @brief format::CompiledCache: the document's nodes with their components instantiated and their static styles
/// merged, compiled once per version of the document and shared by every update.

#include "Record.hpp"

#include "affresco/core/VersionControl.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief The nodes of the document compiled (CompiledNode) for one version of it: each node is resolved (its
/// component instantiated, its static styles merged) and compiled once, then shared by every update and every panel
/// built from it; a node with a dynamic style is compiled once per style it chooses.
///
/// Invariants: it never changes the document; its cache belongs to one version of the document (forgetVersion()
/// drops it, carryOver() moves to the next version what that version didn't change, and it drops itself when the
/// document is another object). Callers: the builder (resolve(), styled()),
/// the children lists (compiled(), compiledOverlay()), the reconciler (compiled()) and the properties subsystem
/// (withStyles() for state blocks). Main thread only.
class CompiledCache {
public:
	/// @brief The compiled nodes of an engine's document.
	/// @param engine The runtime; outlives this object.
	explicit CompiledCache(Engine& engine) noexcept : engine_(engine) {}

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
	/// @brief Turns a "use" of a component into its node: the component's node with the use's properties on top, its
	/// "with" made of the use's arguments, the params it didn't pass and the node's own "with" (kept, so a component
	/// that uses another passes its arguments on), and the use's children in its slot.
	/// @param use The node with "use".
	/// @param path Its place, for errors.
	/// @return The node.
	/// @throws DocumentError If the component is unknown.
	[[nodiscard]] json instantiateComponent(const json& use, const std::string& path) const;
	/// @brief The component a use names: the document's own, else a built-in one.
	/// @param name Its name.
	/// @param path The use's place, for errors.
	/// @return The component.
	/// @throws DocumentError If there is none, or it has no "node".
	[[nodiscard]] const json& componentNamed(const std::string& name, const std::string& path) const;
	/// @brief A use's parameters, in one layer: what the use passes, the component's defaults it didn't pass, then
	/// the component node's own "with".
	/// @param use The node with "use".
	/// @param component The component.
	/// @param node The component's node, with the use's properties on top.
	/// @param name The component's name, for errors.
	/// @param path The use's place, for errors.
	/// @return The "with" object.
	[[nodiscard]] static json parameters(const json& use, const json& component, const json& node,
	                                     const std::string& name, const std::string& path);
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
	/// @brief Drops what was compiled: the document changed.
	void forgetVersion() const noexcept;
	/// @brief Moves to a new version of the document what was compiled for nodes it didn't change, by their new
	/// places, and drops the rest. A node is kept when it is the same, or differs only in its "children" and holds
	/// none of its own (they are then found where the new version has them); the nodes compiled inside a kept
	/// component's instance are kept with it. Nodes are matched by value, from the top: a list keeps its common start
	/// and end, so a child added or removed doesn't drop its siblings. Everything is dropped when the change touches
	/// what every node is compiled with ("styles", "components", "templates") or may have changed anything.
	/// @param before The document the cache was filled from; valid during the call.
	/// @param after The new version (what the engine's document() now returns).
	/// @param change What the version control says changed.
	void carryOver(const json& before, const json& after, const DocumentChange& change) const;
	/// @brief How many compiled nodes the last carryOver() kept, for the tests.
	/// @return The count.
	[[nodiscard]] std::size_t carried() const noexcept { return carried_; }
	/// @brief The nodes kept by the last carryOver() whose list of children the panels show changed (a child added,
	/// removed, moved, or changed in its own keys), by their numbers (CompiledNode::serial, childrenPlace()).
	/// @return Them, sorted.
	[[nodiscard]] const std::vector<std::uint64_t>& changedLists() const noexcept { return changedLists_; }
	/// @brief Whether running the lists of changedLists() again builds what the last carryOver()'s version shows: every
	/// compiled root, screen, template and overlay was kept, and the screens and the overlays are the same ones.
	/// Otherwise (or after everything was dropped) the panels need a full pass.
	/// @return true if they are enough.
	[[nodiscard]] bool listsOnly() const noexcept { return listsOnly_; }

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
	/// @brief Numbers a node this cache compiled (CompiledNode::serial).
	/// @param node The node.
	/// @return It.
	CompiledRef numbered(CompiledRef node) const noexcept;
	struct Carrier; // carryOver()'s walk (CompiledCache.cpp)

	Engine& engine_;                  ///< The runtime.
	mutable Cache cache_;             ///< What is compiled for the current version (filled as updates need it).
	mutable std::size_t carried_ = 0; ///< See carried().
	mutable std::vector<std::uint64_t> changedLists_; ///< See changedLists().
	mutable bool listsOnly_ = false;                  ///< See listsOnly().
	mutable std::uint64_t serials_ = 0;               ///< The last number numbered() gave, never reused.
};

} // namespace affresco::format
