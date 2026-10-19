#pragma once

/// @file
/// @brief format::Reconciler: the panels built from the document, kept in line with it and the state.

#include "Record.hpp"

#include "affresco/render/Panel.hpp"

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace affresco::format {

struct Engine;

/// @brief Builds the document into panels and keeps them in line with it: on every update it walks the nodes, reuses
/// the panel in each place if it has the same type (and key), moves keyed items, creates and destroys the rest, and
/// applies each node's values (Properties).
///
/// Invariants: there is a record for every panel it built that still exists, and none for any other (forgetSubtree()
/// drops a subtree's records before it is destroyed); a factory's own children come before those built from the
/// document and are left alone. State changes during one event become one update, run after the event
/// (requestReconcile()). Callers: the engine (documentChanged()), Runtime::build() and instantiate(), and every
/// subsystem that reads records. Main thread only.
class Reconciler {
public:
	/// @brief The reconciler of an engine.
	/// @param engine The runtime; outlives this object.
	explicit Reconciler(Engine& engine) : engine_(engine) {}

	/// @brief Brings the panels in line with the document and the state, then runs "watch" handlers.
	/// @throws DocumentError If the document can't be built (the version is then refused).
	void reconcile();
	/// @brief Schedules one reconcile after the current event (batches state changes).
	void requestReconcile();
	/// @brief Builds a node outside reconciliation (Runtime::instantiate).
	/// @param node The node.
	/// @param parent Where to add it.
	/// @param path Its place, for errors.
	/// @return The panel.
	Panel& buildDetached(const json& node, Panel& parent, const std::string& path);

	/// @brief The record of a panel built from the document.
	/// @param panel The panel.
	/// @return The record, or nullptr.
	[[nodiscard]] Record* recordOf(const Panel* panel) {
		const auto it = records_.find(panel);
		return it == records_.end() ? nullptr : &it->second;
	}
	/// @brief Every record, by panel.
	[[nodiscard]] std::unordered_map<const Panel*, Record>& records() noexcept { return records_; }

private:
	/// @brief Creates the panel of a node type.
	/// @param type The "type".
	/// @param path Its place, for errors.
	/// @return The panel.
	[[nodiscard]] std::unique_ptr<Panel> create(const std::string& type, const std::string& path) const;
	/// @brief Starts tracking a new panel, remembering its defaults, and connects its signals.
	/// @param panel The panel.
	/// @param type Its "type".
	/// @return Its record.
	Record& adopt(Panel& panel, const std::string& type);
	/// @brief Updates a panel's children in place, creating, replacing, moving or removing only what changed.
	/// @param node The node (its "children", unevaluated).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param path Its place, for errors.
	void reconcileChildren(const json& node, Panel& panel, Record& record, const std::string& path);
	/// @brief The children to build: the current screen for an outlet, else the node's "children".
	/// @param node The node.
	/// @param record Its record.
	/// @param path Its place, for errors.
	/// @return The children, or std::nullopt to leave the panel's children alone.
	[[nodiscard]] std::optional<std::vector<Item>> childrenOf(const json& node, const Record& record,
	                                                          const std::string& path) const;
	/// @brief The panel for one child, in its place: the one there if it has the same type and key, else a new one.
	/// @param panel The parent.
	/// @param index Its place among the children.
	/// @param item The child.
	/// @param type Its type.
	/// @return The panel, and whether it was just created.
	std::pair<Panel*, bool> place(Panel& panel, std::size_t index, const Item& item, const std::string& type);
	/// @brief Drops the records of a subtree about to be destroyed, stopping its timers and running "onHide".
	/// @param panel The subtree's root.
	void forgetSubtree(const Panel& panel);
	/// @brief Runs the "watch" handlers whose values changed since the last update.
	/// @param root The window's root, which sends them.
	void watch(Panel& root);

	Engine& engine_;                                   ///< The runtime.
	std::unordered_map<const Panel*, Record> records_; ///< See records().
	bool reconcileQueued_ = false;                     ///< A reconcile is deferred to the end of the event.
	json watched_ = json::object();                    ///< "watch": the values seen last.
};

} // namespace affresco::format
