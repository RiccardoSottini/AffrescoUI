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
/// (requestReconcile()). Cost follows the change: each record's values, its children's list and the items its
/// variables copy are readers of the state (PanelReader), kept by the place they read (Tracker), so an update after
/// state changes of known places (requestStateUpdate()) runs only the readers of those places, in the order of the
/// panels, without walking the tree; anything else (a revision, a resize, a new theme or screen) updates everything.
/// Callers:
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
	/// @brief Updates a kept panel and what is below it, if they read a change: nothing to do here, since their
	/// readers run by themselves (runReader()), each in its turn.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param changes What changed.
	/// @param depth The panel's depth (ChildList::reconcileChildren()).
	void updatePanel(Panel& panel, Record& record, const StateChanges& changes, int depth);
	/// @brief Runs one of a record's readers (PanelReader::update()): copies its items again, applies its values
	/// again or builds its children's list again.
	/// @param reader The reader.
	/// @param changes The update's changes.
	void runReader(const PanelReader& reader, const StateChanges& changes);
	/// @brief Where a record's reader comes (PanelReader::position()).
	/// @param reader The reader.
	/// @param out Gets the numbers.
	void positionOf(const PanelReader& reader, std::vector<std::uint32_t>& out);
	/// @brief Records what building a panel's list of children read.
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
	/// @brief Updates the window's root and everything below it.
	/// @param root The root.
	/// @param record Its record.
	void updateRoot(Panel& root, Record& record);
	/// @brief Runs the readers the changes touch (Tracker::update()), the panels' among them unless a full pass
	/// follows anyway; falls back to one if they make it due.
	/// @param root The root.
	/// @param record Its record.
	/// @param changes What changed; gets what the readers marked.
	/// @param part Whether only what reads the changes is updated.
	/// @return Whether it stayed a partial update.
	bool runReaders(Panel& root, Record& record, StateChanges& changes, bool part);
	/// @brief Schedules one update after the current event.
	void queue();
	/// @brief Adds a record's values and list readers to the tracker.
	/// @param panel Its panel.
	/// @param record The record.
	void addReaders(Panel& panel, Record& record);
	/// @brief Makes a record's items reader read the items its own variables hold (not its parent's), when they are
	/// new: none, and the reader goes.
	/// @param panel Its panel.
	/// @param record The record.
	void readItems(const Panel& panel, Record& record);
	/// @brief The variables of a panel's parent's record, where its own variables stop.
	/// @param panel The panel.
	/// @return They, or nullptr.
	[[nodiscard]] const Scope* parentLocals(const Panel& panel);
	/// @brief Copies again, in place, the items a panel's own variables hold that a change lies in or covers.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param changes What changed.
	void refreshItems(const Panel& panel, const Record& record, const StateChanges& changes);
	/// @brief Copies again one item a change lies in or covers: only what changed when the change lies inside it.
	/// @param scope The scope that holds it.
	/// @param written The place changed.
	/// @return false when nothing is left to copy for it (copied whole, or gone).
	bool refreshItem(const Scope& scope, const std::string& written) const;
	/// @brief Where a panel is among its parent's children, from the hint in its record if it holds; if not, every
	/// sibling's hint is set again (after a reorder, each is found at once).
	/// @param panel The panel; has a parent.
	/// @return Its index.
	[[nodiscard]] std::uint32_t rankOf(const Panel& panel);

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
