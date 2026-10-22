#pragma once

/// @file
/// @brief format::Reconciler: the panels built from the document, kept in line with it and the state.

#include "Record.hpp"

#include "affresco/format/Runtime.hpp"
#include "affresco/render/Panel.hpp"

#include <cstdint>
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
/// (requestReconcile()). Cost follows the change: each record keeps what its values and its children's list read
/// (Reads.hpp), so an update after state changes of known places (requestStateUpdate()) visits only the panels whose
/// subtree reads one of them; anything else (a revision, a resize, a new theme or screen) updates everything. Callers:
/// the engine (documentChanged()), Runtime::build() and instantiate(), and every subsystem that reads records. Main
/// thread only.
class Reconciler {
public:
	/// @brief The reconciler of an engine.
	/// @param engine The runtime; outlives this object.
	explicit Reconciler(Engine& engine) : engine_(engine) {}

	/// @brief Brings every panel in line with the document and the state, then runs "watch" handlers.
	/// @throws DocumentError If the document can't be built (the version is then refused).
	void reconcile() { update(true); }
	/// @brief Schedules one full reconcile after the current event (batches state changes).
	void requestReconcile();
	/// @brief Schedules one update after the current event for the state changes recorded with their places
	/// (DocumentContext::markStateChanged()): only what reads them is evaluated again.
	void requestStateUpdate();
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
	/// @brief What the last update cost (Runtime::lastUpdate()).
	[[nodiscard]] const UpdateStatistics& statistics() const noexcept { return statistics_; }

private:
	/// @brief Brings the panels in line with the document and the state, then runs "watch" handlers.
	/// @param full Evaluate everything, whatever changed.
	/// @throws DocumentError If the document can't be built.
	void update(bool full);
	/// @brief Whether an update may visit only what reads the changes: the panels were last brought in line without
	/// an error, under the same appearance and screen, and nothing changed that no place describes.
	/// @param changes What changed.
	/// @return true if it may.
	[[nodiscard]] bool canUpdatePart(const StateChanges& changes) const;
	/// @brief Updates the window's root: its values and children, all or only what reads the changes.
	/// @param root The root.
	/// @param record Its record.
	/// @param changes What changed (nullptr: everything).
	void updateRoot(Panel& root, Record& record, const StateChanges* changes);
	/// @brief Schedules one update after the current event.
	void queue();
	/// @brief Applies a node's values to its panel, recording what they read into the record.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param item What to apply: the node, and the place for errors.
	/// @param values Its values, evaluated.
	/// @param reads What evaluating them read.
	/// @param created Whether the panel was just created.
	void applyValues(Panel& panel, Record& record, const Item& item, std::vector<json> values, ReadSet reads,
	                 bool created);
	/// @brief Evaluates a node's values, recording what they read.
	/// @param item The node, its variables and its place.
	/// @param reads Gets what they read.
	/// @return The values.
	[[nodiscard]] std::vector<json> valuesOf(const Item& item, ReadSet& reads) const;
	/// @brief Updates a kept panel and what is below it, if they read a change: its values are applied again if they
	/// read one, then its children are updated (updateChildren()).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param changes What changed.
	/// @param depth The panel's depth (reconcileChildren()).
	void updatePanel(Panel& panel, Record& record, const StateChanges& changes, int depth);
	/// @brief Updates only what reads the changes below a panel: a child whose values read one is applied again, and
	/// a list of children that reads one is built again.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param changes What changed.
	/// @param depth The panel's depth (reconcileChildren()).
	void updateChildren(Panel& panel, Record& record, const StateChanges& changes, int depth);
	/// @brief Copies again, in place, the items a panel's variables hold that a change lies in or covers.
	/// @param locals The panel's variables.
	/// @param changes What changed.
	void refreshItems(const Scope& locals, const StateChanges& changes) const;
	/// @brief Gathers what a panel's subtree reads, from its own reads and its children's.
	/// @param panel The panel.
	/// @param record Its record.
	void gatherReads(const Panel& panel, Record& record) const;
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
	/// @param node The compiled node (its "children", unevaluated).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param path Its place, for errors.
	/// @param depth The panel's depth below the window's root, or below the panel a template is built in.
	/// @throws DocumentError If the panels would nest deeper than maxNodeDepth.
	/// @param changes What changed, when only what reads it is updated (nullptr: everything).
	void reconcileChildren(const CompiledNode& node, Panel& panel, Record& record, const std::string& path,
	                       int depth = 0, const StateChanges* changes = nullptr);
	/// @brief Builds one child in its place among a panel's children.
	/// @param panel The parent.
	/// @param index Its place.
	/// @param item The child.
	/// @param depth The child's depth.
	/// @param changes What changed (nullptr: everything): a panel kept with the same node and variables then updates
	/// only what reads it.
	void reconcileChild(Panel& panel, std::size_t index, Item& item, int depth, const StateChanges* changes);
	/// @brief The children to build: the current screen for an outlet, else the node's "children".
	/// @param node The node.
	/// @param record Its record.
	/// @param path Its place, for errors.
	/// @return The children, or std::nullopt to leave the panel's children alone.
	[[nodiscard]] std::optional<std::vector<Item>> childrenOf(const CompiledNode& node, const Record& record,
	                                                          const std::string& path) const;
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
	Panel& make(Panel& panel, std::size_t index, const Item& item, const std::string& type);
	/// @brief Updates a kept panel built from the same node with the same variables: only what reads the changes.
	/// @param kept The panel.
	/// @param item The child it is built from now.
	/// @param changes What changed.
	/// @param depth Its depth.
	/// @return false if its node, place or variables differ: it must be built again.
	bool updateKept(Panel& kept, const Item& item, const StateChanges& changes, int depth);
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
	bool synced_ = false;                              ///< The last update ended without an error.
	std::uint64_t syncedAppearance_ = 0;               ///< DocumentContext::appearance() then.
	std::string syncedScreen_;                         ///< DocumentContext::screen() then.
	ReadSet rootScopeReads_;                           ///< What the root's "with", "let" and "style" read.
	UpdateStatistics statistics_;                      ///< See statistics().
};

} // namespace affresco::format
