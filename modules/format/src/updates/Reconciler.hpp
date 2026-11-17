#pragma once

/// @file
/// @brief format::Reconciler: the panels built from the document, kept in line with it and the state.

#include "ChildList.hpp"
#include "Record.hpp"

#include "affresco/format/Runtime.hpp"
#include "affresco/render/Panel.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
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
	explicit Reconciler(Engine& engine) : engine_(engine), children_(engine, *this) {}

	/// @brief Brings every panel in line with the document and the state, then runs "watch" handlers.
	/// @throws DocumentError If the document can't be built (the version is then refused).
	/// @param reason Why, for UpdateStatistics::fullReason: a string that lives as long as the program.
	void reconcile(std::string_view reason = "requested") { update(true, reason); }
	/// @brief Schedules one full reconcile after the current event (batches state changes).
	/// @param reason Why, for UpdateStatistics::fullReason: a string that lives as long as the program.
	void requestReconcile(std::string_view reason = "requested");
	/// @brief Schedules one update after the current event for the state changes recorded with their places
	/// (DocumentContext::markStateChanged()): only what reads them is evaluated again.
	void requestStateUpdate();
	/// @brief Brings the panels in line now with the state changes recorded, as the update requestStateUpdate()
	/// schedules does (a full one if the changes call for it): a revision of the state only is shown before it returns.
	/// @throws DocumentError If the document can't be built (the version is then refused).
	void updateNow() { update(false); }
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

	/* For the children lists (ChildList) */

	/// @brief What the running update has cost so far, to count work where it happens.
	/// @return The counters.
	[[nodiscard]] UpdateStatistics& counters() noexcept { return statistics_; }
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
	/// @brief Drops the records of a subtree about to be destroyed, stopping its timers and running "onHide".
	/// @param panel The subtree's root.
	void forgetSubtree(const Panel& panel);
	/// @brief Evaluates a node's values, recording what they read.
	/// @param item The node, its variables and its place.
	/// @param reads Gets what they read.
	/// @return The values.
	[[nodiscard]] std::vector<json> valuesOf(const Item& item, ReadSet& reads) const;
	/// @brief Applies a node's values to its panel, recording what they read into the record.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param item What to apply: the node, and the place for errors.
	/// @param values Its values, evaluated.
	/// @param reads What evaluating them read.
	/// @param created Whether the panel was just created.
	void applyValues(Panel& panel, Record& record, const Item& item, std::vector<json> values, ReadSet reads,
	                 bool created);
	/// @brief Updates a kept panel and what is below it, if they read a change: its values are applied again if they
	/// read one, then its children are updated (updateChildren()).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param changes What changed.
	/// @param depth The panel's depth (ChildList::reconcileChildren()).
	void updatePanel(Panel& panel, Record& record, const StateChanges& changes, int depth);
	/// @brief Records what building a panel's list of children read, and gathers what its subtree reads.
	/// @param panel The panel, its children built.
	/// @param record Its record.
	/// @param reads What building the list read (repeat, when, with, ...).
	void listReconciled(Panel& panel, Record& record, ReadSet reads);

private:
	/// @brief Brings the panels in line with the document and the state, then runs "watch" handlers.
	/// @param full Evaluate everything, whatever changed.
	/// @param reason Why, when full.
	/// @throws DocumentError If the document can't be built.
	void update(bool full, std::string_view reason = "requested");
	/// @brief Why an update must evaluate everything, if it must: it may evaluate only what reads the changes when the
	/// last one ended without an error, under the same appearance and screen, and nothing changed that no place
	/// describes.
	/// @param changes What changed.
	/// @param resized Whether the window changed size, scale or colour scheme.
	/// @param requested The reason given to DocumentContext::requestUpdate(), if any.
	/// @return The reason (UpdateStatistics::fullReason), or empty if it may update a part.
	[[nodiscard]] std::string_view fullReason(const StateChanges& changes, bool resized,
	                                          std::string_view requested) const;
	/// @brief Updates the window's root: its values and children, all or only what reads the changes.
	/// @param root The root.
	/// @param record Its record.
	/// @param changes What changed (nullptr: everything).
	void updateRoot(Panel& root, Record& record, const StateChanges* changes);
	/// @brief Schedules one update after the current event.
	void queue();
	/// @brief Updates only what reads the changes below a panel: a child whose values read one is applied again, and
	/// a list of children that reads one is built again.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param changes What changed.
	/// @param depth The panel's depth (ChildList::reconcileChildren()).
	void updateChildren(Panel& panel, Record& record, const StateChanges& changes, int depth);
	/// @brief Copies again, in place, the items a panel's variables hold that a change lies in or covers.
	/// @param locals The panel's variables.
	/// @param changes What changed.
	void refreshItems(const Scope& locals, const StateChanges& changes) const;
	/// @brief Gathers what a panel's subtree reads, from its own reads and its children's.
	/// @param panel The panel.
	/// @param record Its record.
	void gatherReads(const Panel& panel, Record& record) const;

	Engine& engine_;                                   ///< The runtime.
	std::unordered_map<const Panel*, Record> records_; ///< See records().
	ChildList children_;                               ///< Builds the panels' children.
	bool reconcileQueued_ = false;                     ///< A reconcile is deferred to the end of the event.
	bool synced_ = false;                              ///< The last update ended without an error.
	std::uint64_t syncedAppearance_ = 0;               ///< DocumentContext::appearance() then.
	std::string syncedScreen_;                         ///< DocumentContext::screen() then.
	ReadSet rootScopeReads_;                           ///< What the root's "with", "let" and "style" read.
	mutable UpdateStatistics statistics_;              ///< See statistics().
};

} // namespace affresco::format
