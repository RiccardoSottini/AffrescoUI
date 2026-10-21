/// @file
/// @brief PieceTree: the treap's split and merge, buffers shared by pieces, and the line index.

#include "PieceTree.hpp"

#include <algorithm>

namespace affresco::detail {

namespace {

constexpr std::size_t largeInsert = std::size_t{64} << 10;  ///< Inserts this large get buffers of their own.
constexpr std::size_t largeBuffer = std::size_t{64} << 20;  ///< The most bytes in one buffer of a large insert.
constexpr std::size_t sharedBuffer = std::size_t{16} << 20; ///< The shared buffer is replaced past this size.

} // namespace

std::size_t PieceTree::lineStart(std::size_t line) const noexcept {
	if(line == 0) return 0;
	std::size_t remaining = line, base = 0;
	for(std::uint32_t node = root_; node != none;) {
		const Node& n = nodes_[node];
		if(remaining <= feedsOf(n.left)) {
			node = n.left;
			continue;
		}
		remaining -= feedsOf(n.left);
		base += bytesOf(n.left);
		if(remaining <= n.piece.lineFeeds) {
			const std::uint32_t at = buffers_[n.piece.buffer].breaks[n.piece.firstBreak + remaining - 1];
			return base + (at - n.piece.start) + 1;
		}
		remaining -= n.piece.lineFeeds;
		base += n.piece.length;
		node = n.right;
	}
	return size();
}

std::size_t PieceTree::lineFeedsBefore(std::size_t offset) const noexcept {
	std::size_t count = 0;
	for(std::uint32_t node = root_; node != none;) {
		const Node& n = nodes_[node];
		if(offset < bytesOf(n.left)) {
			node = n.left;
			continue;
		}
		count += feedsOf(n.left);
		offset -= bytesOf(n.left);
		if(offset <= n.piece.length) {
			const std::vector<std::uint32_t>& breaks = buffers_[n.piece.buffer].breaks;
			const auto first = breaks.begin() + n.piece.firstBreak;
			const auto last = first + n.piece.lineFeeds;
			return count + static_cast<std::size_t>(std::lower_bound(first, last, n.piece.start + offset) - first);
		}
		count += n.piece.lineFeeds;
		offset -= n.piece.length;
		node = n.right;
	}
	return count;
}

std::size_t PieceTree::longestLine() const noexcept {
	std::size_t longest = 0, run = 0;
	std::vector<std::uint32_t> path;
	for(std::uint32_t node = root_; node != none || !path.empty();) {
		for(; node != none; node = nodes_[node].left) path.push_back(node);
		const Piece& piece = nodes_[path.back()].piece;
		node = nodes_[path.back()].right;
		path.pop_back();
		if(piece.lineFeeds == 0) {
			run += piece.length;
			continue;
		}
		const std::vector<std::uint32_t>& breaks = buffers_[piece.buffer].breaks;
		longest = std::max(longest, run + (breaks[piece.firstBreak] - piece.start));
		for(std::uint32_t index = 1; index < piece.lineFeeds; ++index) {
			const std::uint32_t at = piece.firstBreak + index;
			longest = std::max<std::size_t>(longest, breaks[at] - breaks[at - 1] - 1);
		}
		run = piece.start + piece.length - breaks[piece.firstBreak + piece.lineFeeds - 1] - 1;
	}
	return std::max(longest, run);
}

std::size_t PieceTree::memory() const noexcept {
	std::size_t total = nodes_.capacity() * sizeof(Node) + freeNodes_.capacity() * sizeof(std::uint32_t) +
	                    buffers_.capacity() * sizeof(Buffer) + freeBuffers_.capacity() * sizeof(std::uint32_t);
	for(const Buffer& buffer : buffers_)
		total += buffer.bytes.capacity() + buffer.breaks.capacity() * sizeof(std::uint32_t);
	return total;
}

void PieceTree::insert(std::size_t offset, std::string_view bytes) {
	if(bytes.empty()) return;
	auto [left, right] = split(root_, std::min(offset, size()));
	std::uint32_t middle = none;
	if(bytes.size() < largeInsert)
		middle = appendSmall(left, bytes);
	else
		for(std::size_t at = 0; at < bytes.size(); at += largeBuffer) {
			const std::string_view chunk = bytes.substr(at, largeBuffer);
			const std::uint32_t buffer = makeBuffer();
			buffers_[buffer].bytes.reserve(chunk.size());
			append(buffer, chunk);
			middle = merge(middle, makeNode(makePiece(buffer, 0, chunk.size())));
		}
	root_ = merge(merge(left, middle), right);
}

void PieceTree::erase(std::size_t offset, std::size_t length) {
	offset = std::min(offset, size());
	length = std::min(length, size() - offset);
	if(length == 0) return;
	auto [before, rest] = split(root_, offset);
	auto [middle, after] = split(rest, length);
	release(middle);
	root_ = merge(before, after);
}

void PieceTree::update(std::uint32_t node) noexcept {
	Node& n = nodes_[node];
	n.bytes = bytesOf(n.left) + n.piece.length + bytesOf(n.right);
	n.lineFeeds = feedsOf(n.left) + n.piece.lineFeeds + feedsOf(n.right);
}

PieceTree::Piece PieceTree::makePiece(std::uint32_t buffer, std::size_t start, std::size_t length) {
	const std::vector<std::uint32_t>& breaks = buffers_[buffer].breaks;
	const auto first = std::lower_bound(breaks.begin(), breaks.end(), start);
	const auto last = std::lower_bound(first, breaks.end(), start + length);
	return {buffer, static_cast<std::uint32_t>(start), static_cast<std::uint32_t>(length),
	        static_cast<std::uint32_t>(first - breaks.begin()), static_cast<std::uint32_t>(last - first)};
}

std::uint32_t PieceTree::makeNode(const Piece& piece) {
	seed_ ^= seed_ << 13;
	seed_ ^= seed_ >> 17;
	seed_ ^= seed_ << 5;
	Node node;
	node.piece = piece;
	node.priority = seed_;
	++buffers_[piece.buffer].pieces;
	std::uint32_t index = 0;
	if(freeNodes_.empty()) {
		index = static_cast<std::uint32_t>(nodes_.size());
		nodes_.push_back(node);
	} else {
		index = freeNodes_.back();
		freeNodes_.pop_back();
		nodes_[index] = node;
	}
	update(index);
	return index;
}

std::uint32_t PieceTree::makeBuffer() {
	if(freeBuffers_.empty()) {
		buffers_.emplace_back();
		return static_cast<std::uint32_t>(buffers_.size() - 1);
	}
	const std::uint32_t index = freeBuffers_.back();
	freeBuffers_.pop_back();
	return index;
}

void PieceTree::append(std::uint32_t buffer, std::string_view bytes) {
	Buffer& target = buffers_[buffer];
	const std::size_t base = target.bytes.size();
	if(bytes.size() >= largeInsert)
		target.breaks.reserve(target.breaks.size() +
		                      static_cast<std::size_t>(std::count(bytes.begin(), bytes.end(), '\n')));
	for(std::size_t at = bytes.find('\n'); at != std::string_view::npos; at = bytes.find('\n', at + 1))
		target.breaks.push_back(static_cast<std::uint32_t>(base + at));
	target.bytes.append(bytes);
}

void PieceTree::dropUse(std::uint32_t buffer) noexcept {
	Buffer& used = buffers_[buffer];
	if(--used.pieces > 0 || buffer == shared_) return;
	used.bytes = std::string();
	used.breaks = std::vector<std::uint32_t>();
	freeBuffers_.push_back(buffer);
}

void PieceTree::release(std::uint32_t node) noexcept {
	if(node == none) return;
	release(nodes_[node].left);
	release(nodes_[node].right);
	dropUse(nodes_[node].piece.buffer);
	freeNodes_.push_back(node);
}

std::pair<std::uint32_t, std::uint32_t> PieceTree::split(std::uint32_t node, std::size_t offset) {
	if(node == none) return {none, none};
	const std::size_t leftBytes = bytesOf(nodes_[node].left);
	if(offset <= leftBytes) {
		const auto [first, rest] = split(nodes_[node].left, offset);
		nodes_[node].left = rest;
		update(node);
		return {first, node};
	}
	const std::size_t pieceEnd = leftBytes + nodes_[node].piece.length;
	if(offset >= pieceEnd) {
		const auto [first, rest] = split(nodes_[node].right, offset - pieceEnd);
		nodes_[node].right = first;
		update(node);
		return {node, rest};
	}
	const std::size_t cut = offset - leftBytes;
	const Piece piece = nodes_[node].piece;
	const std::uint32_t second = makeNode(makePiece(piece.buffer, piece.start + cut, piece.length - cut));
	nodes_[node].piece = makePiece(piece.buffer, piece.start, cut);
	const std::uint32_t right = nodes_[node].right;
	nodes_[node].right = none;
	update(node);
	return {node, merge(second, right)};
}

std::uint32_t PieceTree::merge(std::uint32_t left, std::uint32_t right) noexcept {
	if(left == none) return right;
	if(right == none) return left;
	if(nodes_[left].priority > nodes_[right].priority) {
		const std::uint32_t joined = merge(nodes_[left].right, right);
		nodes_[left].right = joined;
		update(left);
		return left;
	}
	const std::uint32_t joined = merge(left, nodes_[right].left);
	nodes_[right].left = joined;
	update(right);
	return right;
}

std::uint32_t PieceTree::appendSmall(std::uint32_t tree, std::string_view bytes) {
	if(shared_ == none || buffers_[shared_].bytes.size() + bytes.size() > sharedBuffer) {
		const std::uint32_t old = shared_;
		shared_ = makeBuffer();
		if(old != none && buffers_[old].pieces == 0) {
			++buffers_[old].pieces;
			dropUse(old);
		}
	}
	const std::size_t start = buffers_[shared_].bytes.size(), feeds = buffers_[shared_].breaks.size();
	append(shared_, bytes);
	std::uint32_t last = tree;
	while(last != none && nodes_[last].right != none) last = nodes_[last].right;
	if(last == none || nodes_[last].piece.buffer != shared_ ||
	   nodes_[last].piece.start + nodes_[last].piece.length != start)
		return makeNode(makePiece(shared_, start, bytes.size()));
	const std::size_t added = buffers_[shared_].breaks.size() - feeds;
	nodes_[last].piece.length += static_cast<std::uint32_t>(bytes.size());
	nodes_[last].piece.lineFeeds += static_cast<std::uint32_t>(added);
	for(std::uint32_t node = tree; node != none; node = nodes_[node].right) {
		nodes_[node].bytes += bytes.size();
		nodes_[node].lineFeeds += added;
	}
	return none;
}

} // namespace affresco::detail
