#pragma once

/// @file
/// @brief format::ChildList: a panel's children built from its node's "children" (repeats, "when", components, the
/// screen an outlet shows, the overlays), kept in line with the document: reused, moved, created and destroyed.

#include "CompiledNode.hpp"
#include "Expression.hpp"
#include "Reads.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
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

/// @brief One instance of a child of a node: a repeat's row, or the one instance of a child without a repeat.
struct ListRow {
	Panel* panel = nullptr; ///< Its panel; null while its "when" doesn't hold.
	json key{};             ///< Its key (null: none).
	SharedScope scope{};    ///< Its variables before "with" and "let": a repeat's item and number (Scope::renumber()).
	ReadSet reads{};        ///< What its key, "when", "with", "let", "check" and dynamic style read.
	std::size_t recorded = 0; ///< The number its panels' reads were recorded under (stale while it isn't its number).
	bool shown = false;       ///< Whether its "when" held when it was built: it has a panel.
};

/// @brief One element of a node's "children": its instances, and where its repeat's list lies.
struct ListEntry {
	CompiledRef node{};          ///< The child, resolved ("use" instantiated, static styles merged).
	std::string at{};            ///< Its place in the document.
	SharedScope locals{};        ///< The parent's variables it was built in (they keep base alive).
	bool repeat = false;         ///< Whether it has a "repeat".
	bool placed = false;         ///< Whether its list lies in the state, at place().
	const Scope* base = nullptr; ///< The scope whose item the list lies in ("column.cards"), or null.
	std::string list{};          ///< The list's place, after base's origin.
	bool otherRows = false;      ///< A row's own reads reach the list, another row or its number (indexKey).
	ReadSet reads{};             ///< What building it read: its list (or its shape) and every row's own reads.
	std::vector<ListRow> rows{}; ///< Its instances, in order.

	/// @brief Where its list lies now.
	/// @return The place, as a JSON Pointer (only when placed).
	[[nodiscard]] std::string place() const { return base ? base->origin() + list : list; }

	/// @brief Its repeat, compiled.
	/// @return It.
	/// @throws std::logic_error If it has none (repeat is false).
	[[nodiscard]] const CompiledRepeat& compiledRepeat() const {
		const std::optional<CompiledRepeat>& compiled = node->repeat;
		if(!compiled) throw std::logic_error("a list's element without a repeat");
		return *compiled;
	}
};

/// @brief What the list of a panel's children keeps between updates, held by each Record as `list`: which panel each
/// row has, so an insert, a removal or a change inside one item touches only the rows that changed.
struct ListState {
	bool tracked = false;               ///< Whether entries describe the panel's children built from the document.
	const CompiledNode* node = nullptr; ///< The node they were built from.
	std::vector<ListEntry> entries{};   ///< One per element of "children", in order.
	/// The shown rows' keys, after their panels' types (keyName()): how many rows have each.
	std::unordered_map<std::string, std::size_t> keys{};
};

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
	/// @brief Before a list is reconciled: the rows' readers of each list among the changes whose shape changed, left
	/// to the panels that repeat over it, run (all of them if this list isn't tracked).
	/// @param state The list as it was.
	/// @param changes The update's changes.
	void rerunRows(const ListState& state, const StateChanges& changes);
	/// @brief After a list is reconciled: its rows read where they lie now, so the lists among the changes it repeats
	/// over aren't shifted again by another panel of them.
	/// @param state The list as it is now.
	/// @param changes The update's changes.
	void listsRebuilt(const ListState& state, const StateChanges& changes);
	/// @brief The children to build: the current screen for an outlet, else the node's "children" (and for the root,
	/// the document's "overlays" after them, those whose "when" holds).
	/// @param node The node.
	/// @param record Its record.
	/// @param path Its place, for errors.
	/// @param state Gets the rows built, and whether they describe the children (ListState::tracked).
	/// @return The children, or std::nullopt to leave the panel's children alone.
	[[nodiscard]] std::optional<std::vector<Item>> childrenOf(const CompiledNode& node, const Record& record,
	                                                          const std::string& path, ListState& state) const;
	/// @brief Adds the nodes of the document's "overlays" whose "when" holds to the root's children, each keyed by its
	/// place.
	/// @param record The root's record.
	/// @param items The root's children so far; made a list if there are overlays.
	/// @return true if the document has overlays (the root's children then follow no list alone).
	bool addOverlays(const Record& record, std::optional<std::vector<Item>>& items) const;

	/// @brief What one update does to one element of the children (partial()); defined where Item is complete.
	struct EntryPlan;
	/// @brief What one update does to a panel's children (partial()).
	struct Plan;

	/// @brief Builds every child again, keeping each panel a child with the same key (or place) and type had.
	/// @param node The node.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param path Its place.
	/// @param depth Its depth.
	/// @param changes What changed (nullptr: everything).
	void reconcileAll(const CompiledNode& node, Panel& panel, Record& record, const std::string& path, int depth,
	                  const StateChanges* changes);
	/// @brief Matches the children built again with the panels the old ones had, builds the rest, and puts them in
	/// order with one Panel::reorderChildren(): keyed ones by key, the others by their place among them.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param items The children.
	/// @param depth Their depth.
	/// @param changes What changed (nullptr: everything).
	/// @param state The rows built; gets their panels and their keys.
	void arrange(Panel& panel, Record& record, std::vector<Item>& items, int depth, const StateChanges* changes,
	             ListState& state);
	/// @brief Gives the rows built their panels, and counts their keys.
	/// @param state The rows.
	/// @param items Their instances.
	/// @param panels Their panels.
	/// @param reused For each, its row's old variables when its panel was kept as it was (else null).
	static void fill(ListState& state, const std::vector<Item>& items, const std::vector<Panel*>& panels,
	                 std::vector<SharedScope>& reused);
	/// @brief The panel each child gets: one already built (by key, else by its place among those without one).
	/// @param panel The panel.
	/// @param record Its record.
	/// @param items The children.
	/// @return For each, the old child's index after firstManaged, or std::nullopt for a new panel.
	[[nodiscard]] std::vector<std::optional<std::size_t>> match(const Panel& panel, const Record& record,
	                                                            const std::vector<Item>& items) const;
	/// @brief Builds or updates one child with the panel it matched.
	/// @param panel The parent.
	/// @param kept The panel it keeps, or nullptr to build one at the end.
	/// @param item The child.
	/// @param depth Its depth.
	/// @param changes What changed (nullptr: everything).
	/// @param reused Gets the row's old variables, when the panel is kept as it was.
	/// @return Its panel.
	Panel& place(Panel& panel, Panel* kept, Item& item, int depth, const StateChanges* changes, SharedScope& reused);
	/// @brief Updates only the rows a change touches, when the list's shape changed as one insert or one removal, or
	/// only inside its items.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param depth Its depth.
	/// @param changes What changed.
	/// @return false to build every child again: what changed isn't one of those, or the rows read more.
	bool partial(Panel& panel, Record& record, int depth, const StateChanges& changes);
	/// @brief Sorts what changed by the lists it touches.
	/// @param state The rows.
	/// @param changes What changed.
	/// @param plan Gets it.
	/// @return false if a change touches the children in another way (around a list, or what rows read outside it).
	[[nodiscard]] static bool classify(const ListState& state, const StateChanges& changes, Plan& plan);
	/// @brief Plans an element's part of one changed place.
	/// @param entry The element.
	/// @param list Where its list lies (empty when it isn't placed).
	/// @param path The place that changed.
	/// @param plan The element's plan: a change of its list's shape, or inside a row's item, is added to it.
	/// @param shape Set if the place is the list itself.
	/// @return false if the change may reach every row: around the list, past its end, or what rows read outside
	/// their items.
	[[nodiscard]] static bool planPath(const ListEntry& entry, const std::string& list, const std::string& path,
	                                   EntryPlan& plan, bool& shape);
	/// @brief Works out an element's change, and builds its new rows, before any panel changes.
	/// @param state The rows.
	/// @param index The element.
	/// @param changes What changed.
	/// @param plan Its plan.
	/// @param panel The panel.
	/// @param first Its first managed child.
	/// @return false if it isn't one insert or one removal the panels can follow.
	bool prepare(ListState& state, std::size_t index, const StateChanges& changes, EntryPlan& plan, const Panel& panel,
	             std::size_t first);
	/// @brief Builds the rows an insert adds, before any panel changes.
	/// @param state The rows.
	/// @param entry The element.
	/// @param list Its list now.
	/// @param plan Its plan: the insert's place and size; the rows go into it.
	/// @param last Whether the rows go after every other child.
	/// @return false if a row's key is another shown row's, so the panels would have to be matched again.
	bool insertRows(const ListState& state, const ListEntry& entry, const json& list, EntryPlan& plan, bool last) const;
	/// @brief Builds again the rows whose own reads a change inside them touches, before any panel changes.
	/// @param state The rows.
	/// @param index The element.
	/// @param changes What changed.
	/// @param plan Its plan.
	/// @param panel The panel.
	/// @param first Its first managed child.
	/// @return false if a row's key changes, or it can't get or lose its panel in place: every child is built again.
	bool prepareRows(ListState& state, std::size_t index, const StateChanges& changes, EntryPlan& plan,
	                 const Panel& panel, std::size_t first);
	/// @brief Whether a row built again can get or lose its panel in place: at the end of the children, only the first
	/// of an update's rows can.
	/// @param state The rows.
	/// @param position The row's place among the panel's children.
	/// @param row The row as it was.
	/// @param item What it builds now, if anything.
	/// @param panel The panel.
	/// @param flips The rows that got or lost their panel so far in this update, counted here.
	/// @return Whether it can.
	bool canFlip(const ListState& state, std::size_t position, const ListRow& row, const std::optional<Item>& item,
	             const Panel& panel, std::size_t& flips) const;
	/// @brief Inserts, removes and builds again the rows of the plan, numbering the rows after them again.
	/// @param panel The panel.
	/// @param record Its record.
	/// @param depth Its depth.
	/// @param plan The plan.
	/// @param changes What changed.
	void apply(Panel& panel, Record& record, int depth, Plan& plan, const StateChanges& changes);
	/// @brief Builds or destroys a row's panel, or builds it again, after its "when", key or variables changed.
	/// @param panel The panel.
	/// @param state The rows.
	/// @param index The element.
	/// @param number The row's number.
	/// @param row The row built again.
	/// @param item Its instance.
	/// @param depth Its depth.
	/// @param changes What changed.
	void replaceRow(Panel& panel, ListState& state, std::size_t index, std::size_t number, ListRow row,
	                std::optional<Item>& item, int depth, const StateChanges& changes);
	/// @brief Builds a new child's panel in its place, and what is below it.
	/// @param panel The parent.
	/// @param index Its place among the children.
	/// @param item The child.
	/// @param depth Its depth.
	/// @return The panel.
	/// @throws DocumentError If the screen already has maxPanels panels, or it can't be built.
	Panel& build(Panel& panel, std::size_t index, Item& item, int depth);
	/// @brief Builds a kept panel again: its values, and everything below it.
	/// @param child The panel.
	/// @param item The child it is built from now.
	/// @param depth Its depth.
	void rebuild(Panel& child, Item& item, int depth);
	/// @brief The index among the panel's children of a row's panel, or of the place one would take.
	/// @param state The rows.
	/// @param first The panel's first managed child.
	/// @param index The element.
	/// @param number The row's number.
	/// @return The index.
	[[nodiscard]] static std::size_t positionOf(const ListState& state, std::size_t first, std::size_t index,
	                                            std::size_t number);
	/// @brief Whether the rows' panels are the panel's children, in order.
	/// @param panel The panel.
	/// @param record Its record.
	/// @return true if they are.
	[[nodiscard]] static bool inStep(const Panel& panel, const Record& record);

	Engine& engine_;         ///< The runtime.
	Reconciler& reconciler_; ///< The records.
};

} // namespace affresco::format
