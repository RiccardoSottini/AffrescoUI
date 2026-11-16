#pragma once

/// @file
/// @brief format::ChildList: a panel's children built from its node's "children" (repeats, "when", components, the
/// screen an outlet shows, the overlays), kept in line with the document: reused, moved, created and destroyed.

#include "Reads.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace affresco {
class Panel;
} // namespace affresco

namespace affresco::format {

struct Engine;
struct CompiledNode;
struct Item;
struct Record;
class Reconciler;

/// @brief What the list of a panel's children keeps between updates, held by each Record as `list`: empty for now;
/// the tracking of lists adds its fields here.
struct ListState {};

/// @brief Builds a panel's children from its node and keeps them in line: each child reuses the panel in its place if
/// it has the same type (and key), keyed items move, the rest are created or destroyed.
///
/// Invariants: it reaches records only through the reconciler (Reconciler::recordOf(), adopt(), forgetSubtree());
/// what building the list read goes to Reconciler::listReconciled(), never into a record directly. Callers: the
/// reconciler. Main thread only.
class ChildList {
public:
	/// @brief The children lists of an engine's panels.
	/// @param engine The runtime; outlives this object.
	/// @param reconciler The reconciler that owns this object.
	ChildList(Engine& engine, Reconciler& reconciler) noexcept : engine_(engine), reconciler_(reconciler) {}

	/// @brief Updates a panel's children in place, creating, replacing, moving or removing only what changed.
	/// @param node The compiled node (its "children", unevaluated).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param path Its place, for errors.
	/// @param depth The panel's depth below the window's root, or below the panel a template is built in.
	/// @param changes What changed, when only what reads it is updated (nullptr: everything).
	/// @throws DocumentError If the panels would nest deeper than maxNodeDepth.
	void reconcileChildren(const CompiledNode& node, Panel& panel, Record& record, const std::string& path,
	                       int depth = 0, const StateChanges* changes = nullptr);
	/// @brief A node's type, which must be text.
	/// @param node The compiled node.
	/// @param path Its place, for errors.
	/// @return The type.
	/// @throws DocumentError If it isn't text.
	[[nodiscard]] static const std::string& typeOf(const CompiledNode& node, const std::string& path);

private:
	/// @brief Builds one child in its place among a panel's children.
	/// @param panel The parent.
	/// @param index Its place.
	/// @param item The child.
	/// @param depth The child's depth.
	/// @param changes What changed (nullptr: everything): a panel kept with the same node and variables then updates
	/// only what reads it.
	void reconcileChild(Panel& panel, std::size_t index, Item& item, int depth, const StateChanges* changes);
	/// @brief The children to build: the current screen for an outlet, else the node's "children" (and for the root,
	/// the document's "overlays" after them, those whose "when" holds).
	/// @param node The node.
	/// @param record Its record.
	/// @param path Its place, for errors.
	/// @return The children, or std::nullopt to leave the panel's children alone.
	[[nodiscard]] std::optional<std::vector<Item>> childrenOf(const CompiledNode& node, const Record& record,
	                                                          const std::string& path) const;
	/// @brief Adds the nodes of the document's "overlays" whose "when" holds to the root's children, each keyed by its
	/// place.
	/// @param record The root's record.
	/// @param items The root's children so far; made a list if there are overlays.
	void addOverlays(const Record& record, std::optional<std::vector<Item>>& items) const;
	/// @brief The panel already built for one child, moved to its place: the one there if it has the same type and key.
	/// @param panel The parent.
	/// @param index Its place among the children.
	/// @param item The child.
	/// @param type Its type.
	/// @return The panel, or nullptr if a new one is needed.
	Panel* reusable(Panel& panel, std::size_t index, const Item& item, const std::string& type);
	/// @brief Creates the panel for one child in its place, replacing the one there.
	/// @param panel The parent.
	/// @param index Its place among the children.
	/// @param item The child.
	/// @param type Its type.
	/// @return The new panel.
	/// @throws DocumentError If the screen already has maxPanels panels.
	Panel& make(Panel& panel, std::size_t index, const Item& item, const std::string& type);
	/// @brief Updates a kept panel built from the same node with the same variables: only what reads the changes.
	/// @param kept The panel.
	/// @param item The child it is built from now.
	/// @param changes What changed.
	/// @param depth Its depth.
	/// @return false if its node, place or variables differ: it must be built again.
	bool updateKept(Panel& kept, const Item& item, const StateChanges& changes, int depth);

	Engine& engine_;         ///< The runtime.
	Reconciler& reconciler_; ///< The records.
};

} // namespace affresco::format
