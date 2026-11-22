#pragma once

/// @file
/// @brief format::PlaceIndex: the readers of the state by the place they read, so an update finds the readers of a
/// change without walking the panels.

#include "Reads.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace affresco::format {

/// @brief A reader's handle (0: none): its slot in the low readerSlotBits bits, a generation above them, so a handle
/// kept after its reader went away never names the reader that takes its slot next.
using ReaderId = std::uint32_t;
/// The bits of a ReaderId that number its slot: about 4 million readers at once.
inline constexpr unsigned readerSlotBits = 22;
/// The bits of a ReaderId that hold its generation: the top bit stays free for the index's own use.
inline constexpr unsigned readerGenerationBits = 9;

/// @brief The slot of a reader's handle.
/// @param id The handle.
/// @return Its slot.
[[nodiscard]] constexpr std::uint32_t slotOf(ReaderId id) noexcept { return id & ((1u << readerSlotBits) - 1u); }

/// @brief The places of the state the readers read, as a trie of their keys, each node with the readers that read
/// exactly there and those that read only the shape of the list there (shapeKey).
///
/// A change of a place collects the readers on the way down to it (they read a place that contains it), those at it,
/// and every reader below it (it contains what they read); the shape readers of a list only when the change is the
/// list or around it, never inside one of its items. anyKey in a read matches every key at its level, and a row's key
/// (rowKey and a number) the number's: what rows read of their own items, which an insert or a removal numbers again
/// in place (shift()) and a change of the list's shape announced as one (StateChanges::lists) leaves to the list's
/// panel, since the items stay what they were (collectRows() when it can't follow it). Each key is
/// kept once, as a number; a reader keeps where it is in the trie, so removing it costs what it read, and a trie node
/// nobody reads at or below is freed with its key.
///
/// Invariants: a reader is in the index under one handle at a time; its places are at most what one add() gave
/// since it was last removed. No dependency on the engine. Main thread only (collect() reuses its own vectors).
class PlaceIndex {
public:
	/// @brief An empty index.
	PlaceIndex();

	/// @brief Adds places a reader reads, to those it reads already.
	/// @param reader Its handle (not 0).
	/// @param places The places, as JSON Pointers; one ending in shapeKey reads the shape of the list before it.
	void add(ReaderId reader, std::span<const std::string> places);
	/// @brief Replaces what a reader reads: the trie nodes both read stay, so a reader that reads what it read
	/// before frees and makes none.
	/// @param reader Its handle (not 0).
	/// @param places The places now.
	void set(ReaderId reader, std::span<const std::string> places);
	/// @brief Removes a reader and what it read, freeing the trie nodes nobody reads at or below any more.
	/// @param reader Its handle; one not in the index, or of an older generation, does nothing.
	void remove(ReaderId reader) noexcept;
	/// @brief Whether a reader is in the index, under this handle (not one of an older generation).
	/// @param reader The handle.
	/// @return true if it is.
	[[nodiscard]] bool contains(ReaderId reader) const noexcept;
	/// @brief Collects the readers some changes touch, each once.
	/// @param changes The changes; `all` collects nothing (the caller updates everything).
	/// @param out Gets the readers, appended in no particular order.
	void collect(const StateChanges& changes, std::vector<ReaderId>& out);
	/// @brief Numbers again what rows of a list read of their own items after an insert or a removal before them:
	/// the rows' keys from a number on move by as many; what reads where a key moves to (rows being removed, read by
	/// another list's panels, which follow the same change) is dropped and collected.
	/// @param list The list, as a JSON Pointer; every node it reaches (its rows' keys among its keys) moves.
	/// @param from The first row that moves, numbered as before.
	/// @param by How far: the rows inserted less those removed.
	/// @param out Gets the readers dropped, and those of the rows moved that read their number (indexKey).
	void shift(std::string_view list, std::size_t from, std::ptrdiff_t by, std::vector<ReaderId>& out);
	/// @brief Collects what the rows of a list read of their own items, which a change of its shape leaves out.
	/// @param list The list, as a JSON Pointer.
	/// @param out Gets the readers.
	void collectRows(std::string_view list, std::vector<ReaderId>& out);

	/// @brief The places recorded, over every reader.
	/// @return Their number.
	[[nodiscard]] std::size_t entries() const noexcept { return entries_; }
	/// @brief The trie nodes in use, the root's among them.
	/// @return Their number.
	[[nodiscard]] std::size_t nodes() const noexcept { return nodes_.size() - freeNodes_.size(); }
	/// @brief The keys kept.
	/// @return Their number, anyKey's among them.
	[[nodiscard]] std::size_t keys() const noexcept { return keys_.size() - freeKeys_.size(); }

private:
	/// @brief A reader at a trie node.
	struct Entry {
		std::uint32_t reader = 0; ///< Its handle, with shapeBit if it reads the shape there.
		std::uint32_t link = 0;   ///< Where the reader keeps this entry, among its links.
	};
	/// @brief Where a reader is in the trie.
	struct Link {
		std::uint32_t node = 0;     ///< The trie node.
		std::uint32_t position = 0; ///< The entry's place among the node's readers.
	};
	/// @brief A node of the trie: one key below its parent.
	struct Node {
		std::uint32_t parent = 0;              ///< The node above (the root: itself).
		std::uint32_t key = 0;                 ///< Its key's number.
		std::uint32_t position = 0;            ///< Its place among its parent's children.
		std::vector<std::uint32_t> children{}; ///< The nodes below it.
		std::vector<Entry> readers{};          ///< The readers that read there.
	};
	/// @brief A key, kept once.
	struct Key {
		std::string text{};     ///< The key, escaped as in a JSON Pointer.
		std::uint32_t uses = 0; ///< The trie nodes with it.
		std::size_t row = 0;    ///< 1 + the row's number for a row's key (rowKey), else 0.
	};
	/// @brief A reader's slot.
	struct Slot {
		ReaderId id = 0;           ///< Its handle (0: free).
		std::vector<Link> links{}; ///< Where it is in the trie.
	};
	/// @brief Hashes keys for lookup by view.
	struct KeyHash {
		using is_transparent = void; ///< Lookup by std::string_view.
		/// @brief The hash.
		/// @param key A key.
		/// @return Its hash.
		std::size_t operator()(std::string_view key) const noexcept { return std::hash<std::string_view>{}(key); }
	};

	/// The bit of Entry::reader that marks a read of the list's shape.
	static constexpr std::uint32_t shapeBit = 1u << 31;
	/// The root's number.
	static constexpr std::uint32_t root = 0;
	/// The parent of a freed node.
	static constexpr std::uint32_t freed = 0xffffffffu;

	/// @brief The number of a key, kept if it isn't yet.
	/// @param key The key.
	/// @return Its number.
	std::uint32_t intern(std::string_view key);
	/// @brief The trie node of a key below another, made if there is none.
	/// @param parent The node above.
	/// @param key The key's text.
	/// @return The node.
	std::uint32_t childMade(std::uint32_t parent, std::string_view key);
	/// @brief The trie node of a key below another.
	/// @param parent The node above.
	/// @param key The key's number.
	/// @return The node, or nullptr if there is none.
	[[nodiscard]] const std::uint32_t* child(std::uint32_t parent, std::uint32_t key) const;
	/// @brief Records that a reader reads a place.
	/// @param slot The reader's slot.
	/// @param place The place.
	void link(Slot& slot, std::string_view place);
	/// @brief Takes one of a reader's entries off its node.
	/// @param link The entry.
	/// @param pruned Whether to free the nodes that leaves empty (prune()).
	void unlink(const Link& link, bool pruned) noexcept;
	/// @brief Forgets one use of a key, and the key with its last.
	/// @param key The key's number.
	void release(std::uint32_t key) noexcept;
	/// @brief Frees a node and those above it that nobody reads at or below any more.
	/// @param node The node; one freed already does nothing.
	void prune(std::uint32_t node) noexcept;
	/// @brief Collects the readers of one changed place.
	/// @param place The place.
	/// @param out Gets them.
	/// @param rows Whether the rows' keys right below it too (false: a list whose shape changed, as announced).
	void collectPlace(std::string_view place, std::vector<ReaderId>& out, bool rows);
	/// @brief Collects the shape readers of a list whose shape changed.
	/// @param place The list.
	/// @param out Gets them.
	void collectShape(std::string_view place, std::vector<ReaderId>& out);
	/// @brief Collects the readers of a node.
	/// @param node The node.
	/// @param shape Whether those that read the shape there too.
	/// @param out Gets them.
	void collectAt(std::uint32_t node, bool shape, std::vector<ReaderId>& out);
	/// @brief Collects every reader at a node and below it.
	/// @param node The node.
	/// @param out Gets them.
	/// @param rows Whether below its rows' keys too.
	void collectBelow(std::uint32_t node, std::vector<ReaderId>& out, bool rows = true);
	/// @brief Starts collecting: no reader counts as collected yet.
	void startCollect();
	/// @brief Takes every reader off a node and the nodes below it, and frees them.
	/// @param node The node.
	/// @param out Gets the readers.
	void dropBelow(std::uint32_t node, std::vector<ReaderId>& out);
	/// @brief The node of a row's key (rowKey and a number) below a node.
	/// @param node The node.
	/// @param number The row's number.
	/// @return It, or nullptr if there is none.
	[[nodiscard]] const std::uint32_t* rowChild(std::uint32_t node, std::size_t number) const;
	/// @brief The number of a key, if a place reads it.
	/// @param key The key.
	/// @return Its number, or freed if no place has it.
	[[nodiscard]] std::uint32_t keyNumber(std::string_view key) const;
	/// @brief The number of a row's key (rowKey), interned if it isn't yet.
	/// @param number The row's number.
	/// @return The key's number.
	std::uint32_t rowKeyMade(std::size_t number);
	/// @brief Finds the nodes of a place, anyKey in the trie matching any key: the trie nodes the change reaches.
	/// @param place The place.
	/// @param ancestors Whether to collect the readers of the nodes on the way (not their shape readers).
	/// @param out Gets those readers.
	/// @return Whether the place reaches any node; the nodes are in frontier_.
	bool reach(std::string_view place, bool ancestors, std::vector<ReaderId>& out);

	std::vector<Node> nodes_;                                                         ///< The trie; 0 is the root.
	std::vector<std::uint32_t> freeNodes_;                                            ///< Nodes free for reuse.
	std::unordered_map<std::uint64_t, std::uint32_t> childOf_;                        ///< (parent, key) -> node.
	std::vector<Key> keys_;                                                           ///< The keys, by number.
	std::unordered_map<std::string, std::uint32_t, KeyHash, std::equal_to<>> keyIds_; ///< Key -> number.
	std::vector<std::uint32_t> freeKeys_;                                             ///< Key numbers free for reuse.
	std::uint32_t anyKeyId_ = 0;                                                      ///< anyKey's number.
	std::vector<Slot> slots_;                                                         ///< The readers, by slot.
	std::size_t entries_ = 0;                                                         ///< See entries().
	std::vector<std::uint32_t> frontier_;                                             ///< reach()'s nodes (reused).
	std::vector<std::uint32_t> next_;    ///< reach()'s next nodes (reused).
	std::vector<std::uint32_t> below_;   ///< collectBelow()'s stack (reused).
	std::vector<std::uint32_t> seen_;    ///< Per slot: the collect() it was last collected in (as long as slots_).
	std::uint32_t collecting_ = 0;       ///< The collect() running.
	std::vector<Link> scratch_;          ///< set()'s old links (reused).
	std::string rowText_;                ///< A row's key being made (reused).
	std::vector<std::uint32_t> rowKeys_; ///< By a row's number: 1 + its key's number, or 0 if it has none.
};

} // namespace affresco::format
