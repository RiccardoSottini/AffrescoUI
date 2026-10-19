#pragma once

/// @file
/// @brief detail::ShapedTextCache: shaped text by its text, the least recently used dropped first.

#include "affresco/render/Font.hpp"

#include <cstddef>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>

namespace affresco::detail {

/// @brief Shaped text by its text, within a number of entries and of code points; the least recently used goes first.
///
/// A hit allocates nothing, so a frame that draws the same text as the last one doesn't either. Main thread only.
class ShapedTextCache {
public:
	/// @brief An empty cache.
	/// @param maxEntries The most texts it holds.
	/// @param maxCodepoints The most code points their texts hold together; a text longer than a quarter of it isn't
	/// kept at all, so one text can't empty the cache.
	ShapedTextCache(std::size_t maxEntries, std::size_t maxCodepoints) noexcept
	    : maxEntries_(maxEntries), maxCodepoints_(maxCodepoints) {}

	/// @brief Whether a text that long would be kept.
	/// @param length Its code points.
	[[nodiscard]] bool keeps(std::size_t length) const noexcept { return length <= maxCodepoints_ / 4; }

	/// @brief Looks a text up, and makes it the most recently used.
	/// @param text The text.
	/// @return Its shaped text, valid until the next insert(); nullptr if it isn't there.
	[[nodiscard]] const Font::ShapedText* find(std::u32string_view text) noexcept {
		const auto found = index_.find(text);
		if(found == index_.end()) return nullptr;
		entries_.splice(entries_.begin(), entries_, found->second);
		return &found->second->shaped;
	}

	/// @brief Adds a text, dropping the least recently used ones beyond the limits.
	/// @param text The text: not in the cache, and keeps() its length.
	/// @param shaped Its shaped text.
	/// @return The kept shaped text, valid until a later insert() drops it (never the next one).
	const Font::ShapedText& insert(std::u32string_view text, Font::ShapedText shaped) {
		while(!entries_.empty() && (entries_.size() >= maxEntries_ || codepoints_ + text.size() > maxCodepoints_)) {
			codepoints_ -= entries_.back().text.size();
			index_.erase(entries_.back().text);
			entries_.pop_back();
		}
		const Entry& entry = entries_.emplace_front(Entry{std::u32string(text), std::move(shaped)});
		// The key views the entry's own text: list nodes never move, nor do the characters inside them
		index_.emplace(entry.text, entries_.begin());
		codepoints_ += text.size();
		return entry.shaped;
	}

	/// @brief Roughly how many bytes it holds.
	[[nodiscard]] std::size_t bytes() const noexcept {
		std::size_t total = 0;
		for(const Entry& entry : entries_)
			total += sizeof(Entry) + 64 + entry.text.size() * sizeof(char32_t) +
			         entry.shaped.glyphs.size() * sizeof(Font::ShapedGlyph) +
			         (entry.shaped.advances.size() + entry.shaped.carets.size()) * sizeof(float);
		return total;
	}

private:
	/// @brief A text and its shaping.
	struct Entry {
		std::u32string text;     ///< The text.
		Font::ShapedText shaped; ///< Its shaping.
	};

	std::list<Entry> entries_;                                                  ///< The most recently used first.
	std::unordered_map<std::u32string_view, std::list<Entry>::iterator> index_; ///< By text.
	std::size_t codepoints_ = 0;                                                ///< In all texts.
	std::size_t maxEntries_ = 0;                                                ///< See the constructor.
	std::size_t maxCodepoints_ = 0;                                             ///< See the constructor.
};

} // namespace affresco::detail
