#pragma once

/// @file
/// @brief PieceTree: text as pieces of a few large buffers, in a balanced tree, edited in O(log n), with a line index.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco::detail {

/// @brief Bytes of text kept as pieces of buffers that are never edited, only appended to, in a treap ordered by
/// position: inserting or erasing splits and joins pieces in O(log n), whatever the size of the text.
///
/// Large inserts get buffers of their own (opening a file copies it once), small ones are appended to a shared
/// buffer, so typing grows one piece. Each buffer keeps where its line feeds are, and each tree node the bytes and the
/// line feeds under it, so a line's start and the line of an offset are found in O(log n) too. A buffer no piece uses
/// any more is freed.
class PieceTree {
public:
	/// @brief The bytes, in order.
	[[nodiscard]] std::size_t size() const noexcept { return root_ == none ? 0 : nodes_[root_].bytes; }
	/// @brief The line feeds ('\n') in the text.
	[[nodiscard]] std::size_t lineFeeds() const noexcept { return root_ == none ? 0 : nodes_[root_].lineFeeds; }
	/// @brief Where a line starts.
	/// @param line The line, from 0; at most lineFeeds().
	/// @return Its first byte's offset.
	[[nodiscard]] std::size_t lineStart(std::size_t line) const noexcept;
	/// @brief How many line feeds come before an offset: the line it is on.
	/// @param offset The offset, at most size().
	[[nodiscard]] std::size_t lineFeedsBefore(std::size_t offset) const noexcept;
	/// @brief The longest line, in bytes, without its line feed: a bound on its width in code points.
	[[nodiscard]] std::size_t longestLine() const noexcept;
	/// @brief The memory kept: the buffers, their line feed offsets and the tree, in bytes.
	[[nodiscard]] std::size_t memory() const noexcept;

	/// @brief Inserts bytes.
	/// @param offset Where, at most size().
	/// @param bytes The bytes.
	void insert(std::size_t offset, std::string_view bytes);
	/// @brief Erases bytes.
	/// @param offset The first, at most size().
	/// @param length How many; fewer if the text ends first.
	void erase(std::size_t offset, std::size_t length);

	/// @brief Calls a function with the bytes of a range, in order, a piece at a time.
	/// @param offset The first byte.
	/// @param length How many bytes; fewer if the text ends first.
	/// @param visit Called with each part, as a std::string_view.
	template <typename Visit> void read(std::size_t offset, std::size_t length, Visit&& visit) const {
		if(length > 0) readRange(root_, 0, offset, offset + length, visit);
	}

private:
	static constexpr std::uint32_t none = UINT32_MAX; ///< No node, no buffer.

	/// @brief A buffer: bytes only ever appended to, and where its line feeds are.
	struct Buffer {
		std::string bytes;                 ///< The bytes.
		std::vector<std::uint32_t> breaks; ///< The offsets of its line feeds, in order.
		std::size_t pieces = 0;            ///< The pieces using it: freed at 0.
	};

	/// @brief A run of one buffer's bytes.
	struct Piece {
		std::uint32_t buffer = none;  ///< The buffer.
		std::uint32_t start = 0;      ///< Its first byte in the buffer.
		std::uint32_t length = 0;     ///< Its bytes.
		std::uint32_t firstBreak = 0; ///< Its first line feed, as an index in the buffer's breaks.
		std::uint32_t lineFeeds = 0;  ///< Its line feeds.
	};

	/// @brief A tree node: a piece, its children and what is under it.
	struct Node {
		Piece piece;                ///< The piece.
		std::uint32_t left = none;  ///< The pieces before it.
		std::uint32_t right = none; ///< The pieces after it.
		std::uint32_t priority = 0; ///< Higher than its children's: keeps the tree balanced, on average.
		std::size_t bytes = 0;      ///< The bytes of its subtree.
		std::size_t lineFeeds = 0;  ///< The line feeds of its subtree.
	};

	/// @brief Visits a range under a node.
	/// @param node The node.
	/// @param base The offset of the node's subtree.
	/// @param from The range's start.
	/// @param to The range's end.
	/// @param visit Called with each part.
	template <typename Visit>
	void readRange(std::uint32_t node, std::size_t base, std::size_t from, std::size_t to, Visit& visit) const {
		if(node == none || from >= base + nodes_[node].bytes || to <= base) return;
		const Node& n = nodes_[node];
		const std::size_t leftBytes = n.left == none ? 0 : nodes_[n.left].bytes;
		readRange(n.left, base, from, to, visit);
		const std::size_t start = base + leftBytes, end = start + n.piece.length;
		if(from < end && to > start) {
			const std::size_t first = std::max(from, start) - start, last = std::min(to, end) - start;
			visit(std::string_view(buffers_[n.piece.buffer].bytes).substr(n.piece.start + first, last - first));
		}
		readRange(n.right, end, from, to, visit);
	}

	/// @brief Bytes under a node (0 for none).
	/// @param node The node.
	[[nodiscard]] std::size_t bytesOf(std::uint32_t node) const noexcept {
		return node == none ? 0 : nodes_[node].bytes;
	}
	/// @brief Line feeds under a node (0 for none).
	/// @param node The node.
	[[nodiscard]] std::size_t feedsOf(std::uint32_t node) const noexcept {
		return node == none ? 0 : nodes_[node].lineFeeds;
	}
	/// @brief Works out a node's totals from its piece and children.
	/// @param node The node.
	void update(std::uint32_t node) noexcept;
	/// @brief Makes a piece of a buffer, finding its line feeds (makeNode counts it as a user of the buffer).
	/// @param buffer The buffer.
	/// @param start Its first byte.
	/// @param length Its bytes.
	[[nodiscard]] Piece makePiece(std::uint32_t buffer, std::size_t start, std::size_t length);
	/// @brief A new node holding a piece, reusing a freed one.
	/// @param piece The piece.
	[[nodiscard]] std::uint32_t makeNode(const Piece& piece);
	/// @brief A new empty buffer, reusing a freed slot.
	[[nodiscard]] std::uint32_t makeBuffer();
	/// @brief Appends bytes to a buffer and records their line feeds.
	/// @param buffer The buffer.
	/// @param bytes The bytes.
	void append(std::uint32_t buffer, std::string_view bytes);
	/// @brief Forgets a piece's use of its buffer, freeing the buffer when nothing uses it.
	/// @param buffer The buffer.
	void dropUse(std::uint32_t buffer) noexcept;
	/// @brief Frees a subtree's nodes.
	/// @param node The subtree.
	void release(std::uint32_t node) noexcept;
	/// @brief Splits a subtree at an offset, splitting the piece there if it is inside one.
	/// @param node The subtree.
	/// @param offset Bytes in the first part.
	/// @return The first part and the rest.
	[[nodiscard]] std::pair<std::uint32_t, std::uint32_t> split(std::uint32_t node, std::size_t offset);
	/// @brief Joins two subtrees, all of @p left before @p right.
	/// @param left The first.
	/// @param right The second.
	/// @return The joined tree.
	[[nodiscard]] std::uint32_t merge(std::uint32_t left, std::uint32_t right) noexcept;
	/// @brief Typing: appends small bytes to the shared buffer, growing the last piece of a subtree if it ends there.
	/// @param tree The subtree before the insert.
	/// @param bytes The bytes.
	/// @return A node for them, or none if the last piece grew.
	[[nodiscard]] std::uint32_t appendSmall(std::uint32_t tree, std::string_view bytes);

	std::vector<Node> nodes_;                ///< Every node; freed ones are listed in freeNodes_.
	std::vector<std::uint32_t> freeNodes_;   ///< Nodes to reuse.
	std::vector<Buffer> buffers_;            ///< Every buffer; freed ones are listed in freeBuffers_.
	std::vector<std::uint32_t> freeBuffers_; ///< Buffers to reuse.
	std::uint32_t root_ = none;              ///< The tree.
	std::uint32_t shared_ = none;            ///< The buffer small inserts are appended to.
	std::uint32_t seed_ = 0x9E3779B9u;       ///< The priorities' generator (xorshift), so runs repeat.
};

} // namespace affresco::detail
