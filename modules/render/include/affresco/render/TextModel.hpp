#pragma once

/// @file
/// @brief TextModel: editable UTF-8 text of any size, kept as a piece tree, with a line index, undo and change events.

#include "affresco/core/Signal.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace affresco {

namespace detail {
class PieceTree;
}

/// @brief A place in a text: a line and a code point within it, both from 0.
struct TextPosition {
	std::size_t line = 0;   ///< The line.
	std::size_t column = 0; ///< Code points before it on the line.

	/// @brief Ordered as in the text: by line, then by column.
	constexpr auto operator<=>(const TextPosition&) const noexcept = default;
};

/// @brief What an edit changed: a range replaced by new text. The text itself is read from the model.
struct TextChange {
	TextPosition from;         ///< Where the edit starts.
	TextPosition oldEnd;       ///< Where the replaced text ended, before the edit.
	TextPosition newEnd;       ///< Where the new text ends, after the edit.
	std::size_t offset = 0;    ///< Where the edit starts, in bytes.
	std::size_t removed = 0;   ///< Bytes replaced.
	std::size_t inserted = 0;  ///< Bytes inserted.
};

/// @brief UTF-8 text with lines separated by "\n", of any size: opening a file copies it once, an edit costs
/// O(log n) whatever the size, and a line is found in O(log n).
///
/// The text is kept as pieces of append-only buffers in a balanced tree (a piece tree, as in large code editors), so
/// it takes little more memory than its bytes. Invalid UTF-8 becomes U+FFFD as it comes in. Undo keeps operations, not
/// copies of the text: a step keeps what its edits removed, so opening a large file costs nothing to undo.
class TextModel {
public:
	/// @brief Undo steps kept: the oldest go first past 500 steps, or past 256 MiB of removed text in all; the newest
	/// step stays, however large.
	static constexpr std::size_t maxUndo = 500;
	static constexpr std::size_t maxUndoBytes = std::size_t{256} << 20; ///< See maxUndo.

	/// @brief An empty text.
	TextModel();
	/// @brief Frees the text.
	~TextModel();
	TextModel(const TextModel&) = delete;
	TextModel& operator=(const TextModel&) = delete;

	/* Reading */

	/// @brief The lines: the line feeds plus one.
	[[nodiscard]] std::size_t lineCount() const noexcept;
	/// @brief The text's size, in bytes.
	[[nodiscard]] std::size_t size() const noexcept;
	/// @brief Changes with every edit, so a copy of a line can tell whether it is out of date.
	[[nodiscard]] std::uint64_t version() const noexcept { return version_; }
	/// @brief Decodes one line, without its line feed.
	/// @param line The line, below lineCount().
	/// @param out Gets its code points (its capacity is reused).
	void line(std::size_t line, std::u32string& out) const;
	/// @brief A line's length, in code points.
	/// @param line The line, below lineCount().
	[[nodiscard]] std::size_t lineLength(std::size_t line) const;
	/// @brief The longest line, in bytes: a bound on its length in code points, found without decoding.
	[[nodiscard]] std::size_t longestLine() const noexcept;
	/// @brief The whole text.
	[[nodiscard]] std::string text() const;
	/// @brief The text between two positions.
	/// @param from The start.
	/// @param to The end (not before @p from).
	[[nodiscard]] std::string textIn(TextPosition from, TextPosition to) const;
	/// @brief Whether the text is this one, without copying it.
	/// @param utf8 The text to compare with.
	[[nodiscard]] bool equals(std::string_view utf8) const;
	/// @brief A position's offset in bytes; a column past its line's end is its end.
	/// @param at The position, its line below lineCount().
	[[nodiscard]] std::size_t offsetOf(TextPosition at) const;
	/// @brief The position of a byte offset.
	/// @param offset The offset, at most size(), at the start of a code point.
	[[nodiscard]] TextPosition positionOf(std::size_t offset) const;
	/// @brief The memory the text keeps (its buffers, line index and tree, and the undo steps), in bytes.
	[[nodiscard]] std::size_t memory() const noexcept;

	/* Editing: each edit fires onChanged */

	/// @brief Inserts text.
	/// @param at Where.
	/// @param utf8 The text, which may contain "\n".
	/// @return The position after it.
	TextPosition insert(TextPosition at, std::string_view utf8);
	/// @brief Erases the text between two positions.
	/// @param from The start.
	/// @param to The end (not before @p from).
	void erase(TextPosition from, TextPosition to);
	/// @brief Replaces the whole text.
	/// @param utf8 The new text.
	void assign(std::string_view utf8);

	/* Undo: edits made between beginStep() and endStep() are one step; other edits forget the history */

	/// @brief Starts recording the edits of an undo step, and forgets what was undone.
	/// @param merge true to add them to the newest step (consecutive typing), if there is one.
	/// @param caret The caret before the edits, restored by undo.
	/// @param anchor The selection's other end before the edits.
	void beginStep(bool merge, TextPosition caret, TextPosition anchor);
	/// @brief Stops recording and drops the oldest steps past the limits (see maxUndo).
	void endStep();
	/// @brief Undoes (or redoes) the newest step.
	/// @param redo true to redo.
	/// @param caret The caret now, kept for the opposite step; gets the caret the step restores.
	/// @param anchor The same for the selection's other end.
	/// @return false if there was nothing to undo (or redo).
	bool undo(bool redo, TextPosition& caret, TextPosition& anchor);
	/// @brief The undo steps kept.
	[[nodiscard]] std::size_t undoSteps() const noexcept { return undo_.size(); }

	Signal<const TextChange&> onChanged; ///< The text changed, by an edit or by undo.

private:
	/// @brief One edit: bytes at an offset replaced by others.
	struct Operation {
		std::size_t offset = 0;   ///< Where.
		std::string removed;      ///< The bytes it removed.
		std::size_t inserted = 0; ///< How many bytes it inserted (they are in the text).
	};
	/// @brief An undo step: its operations, in order, and the selection before them.
	struct Step {
		std::vector<Operation> operations; ///< The operations.
		TextPosition caret;                ///< The caret before them.
		TextPosition anchor;               ///< The selection's other end before them.
		std::size_t bytes = 0;             ///< What it keeps in memory, roughly.
	};

	/// @brief Replaces bytes, recording the operation if a step is being recorded, and fires onChanged.
	/// @param offset Where.
	/// @param length Bytes to remove.
	/// @param utf8 Valid UTF-8 to insert.
	void replace(std::size_t offset, std::size_t length, std::string_view utf8);
	/// @brief Adds an operation to the step being recorded, joined with the one before it when they touch.
	/// @param operation The operation.
	void record(Operation operation);

	std::unique_ptr<detail::PieceTree> tree_; ///< The text.
	std::uint64_t version_ = 0;               ///< See version().
	std::vector<Step> undo_;                  ///< Undo steps, newest last.
	std::vector<Step> redo_;                  ///< Undone steps, newest last.
	bool recording_ = false;                  ///< Between beginStep() and endStep().
	bool replaying_ = false;                  ///< Inside undo(): its edits are not recorded.
};

/// @brief A text model's lines as code points, decoded when read: a few are kept, so reading the same lines again
/// costs nothing. A reference to a line stays valid while fewer than 64 other lines are read.
class TextLines {
public:
	/// @brief The lines of a model.
	/// @param model The model; it must outlive the view.
	explicit TextLines(const TextModel& model) noexcept : model_(&model) {}

	/// @brief The lines (at least one).
	[[nodiscard]] std::size_t size() const noexcept { return model_->lineCount(); }
	/// @brief One line, without its line feed.
	/// @param line The line, below size().
	[[nodiscard]] const std::u32string& operator[](std::size_t line) const;
	/// @brief The last line.
	[[nodiscard]] const std::u32string& back() const { return (*this)[size() - 1]; }

private:
	/// @brief A decoded line.
	struct Entry {
		std::size_t line = SIZE_MAX;     ///< Its line.
		std::uint64_t version = 0;       ///< The model's version it was decoded at.
		std::u32string text;             ///< Its code points.
	};

	const TextModel* model_;               ///< The model.
	mutable std::array<Entry, 64> lines_;  ///< The lines decoded last.
	mutable std::size_t next_ = 0;         ///< The entry to replace next.
};

} // namespace affresco
