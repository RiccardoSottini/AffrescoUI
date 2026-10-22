#pragma once

/// @file
/// @brief The syntax engine behind the `syntax` provider, for natives that colour text themselves (the editor's
/// highlighter): a text parsed with tree-sitter, kept in step with its edits (only what changed is parsed again), and
/// read through queries for highlights, folds and bracket pairs. No tree-sitter type shows here.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief A place in a text: a row (line) and a byte within it, both from 0.
struct SyntaxPoint {
	std::uint32_t row = 0;    ///< The line.
	std::uint32_t column = 0; ///< Bytes before it on the line.

	/// @brief Ordered as in the text.
	constexpr auto operator<=>(const SyntaxPoint&) const noexcept = default;
};

/// @brief An edit: bytes from start to oldEnd replaced by bytes from start to newEnd.
struct SyntaxEdit {
	std::uint32_t startByte = 0;  ///< Where it starts.
	std::uint32_t oldEndByte = 0; ///< Where the replaced text ended, before it.
	std::uint32_t newEndByte = 0; ///< Where the new text ends, after it.
	SyntaxPoint start;            ///< startByte as a point.
	SyntaxPoint oldEnd;           ///< oldEndByte as a point, before the edit.
	SyntaxPoint newEnd;           ///< newEndByte as a point, after it.
};

/// @brief A run of one style on one line. Runs are given outer first: a later one paints over an earlier one.
struct SyntaxSpan {
	std::uint32_t row = 0;   ///< The line.
	std::uint32_t start = 0; ///< Its first byte in the line.
	std::uint32_t end = 0;   ///< One past its last (may pass the line's end: to its end).
	std::uint16_t style = 0; ///< Its style: an index into SyntaxDocument::styles().
};

/// @brief Lines that fold: first stays shown, first + 1 to last hide.
struct SyntaxFold {
	std::uint32_t first = 0; ///< The line that stays shown.
	std::uint32_t last = 0;  ///< The last line hidden.
};

/// @brief A pair of brackets: where each starts.
struct SyntaxBrackets {
	SyntaxPoint open;  ///< The opening one.
	SyntaxPoint close; ///< The closing one.
};

/// @brief Rows a parse changed: first to last, both included.
struct SyntaxRows {
	std::uint32_t first = 0; ///< The first.
	std::uint32_t last = 0;  ///< The last.
};

/// @brief The bounds that keep a document cheap whatever its text (the text may be untrusted).
struct SyntaxLimits {
	std::size_t maxBytes = std::size_t{16} << 20; ///< Larger texts aren't parsed: they stay plain.
	std::uint32_t parseMilliseconds = 2000;       ///< A first parse stops after this long (the text stays plain).
	std::uint32_t editMilliseconds = 100;         ///< A parse after an edit stops after this long.
	std::uint32_t maxMatches = 2048;              ///< Query matches in progress at once (tree-sitter's limit).
	std::uint32_t maxCaptures = 200'000;          ///< Captures one highlight() reads, at most.
	std::uint32_t maxDepth = 512;                 ///< How deep in the tree a query match may start.
	std::uint32_t maxPredicateBytes = 1024;       ///< Bytes of a node a predicate reads, at most.
};

/// @brief Reads the text being parsed: the bytes from an offset (any number, none at the end).
/// @param byte The offset.
/// @param at The same place as a point.
using SyntaxReader = std::function<std::string_view(std::size_t byte, SyntaxPoint at)>;

/// @brief What parse() did.
enum class SyntaxParse {
	Parsed,     ///< The tree matches the text.
	TooLarge,   ///< Over SyntaxLimits::maxBytes: the text is plain.
	OverBudget, ///< Over the time budget: a first parse leaves the text plain, a later one keeps the edited tree.
};

/// @brief One text in one language: its tree, its edits and its queries.
///
/// Invariants: edit() is called for every edit before the next parse(); highlight(), fold() and brackets() read the
/// last tree (positions shifted by the edits since). One thread at a time.
class SyntaxDocument {
public:
	/// @brief A document for a language.
	/// @param language A name from languages(), or an alias (e.g. "c++", "js", "md"); any case.
	/// @param limits Its bounds.
	/// @return The document, or nullptr if no grammar has that name.
	[[nodiscard]] static std::unique_ptr<SyntaxDocument> create(std::string_view language, SyntaxLimits limits = {});
	/// @brief The languages: c, cpp, json, python, javascript, typescript and markdown.
	[[nodiscard]] static std::vector<std::string> languages();
	/// @brief The language a file's name suggests, by its extension.
	/// @param fileName The name, or a path.
	/// @return The language, or "" if none.
	[[nodiscard]] static std::string languageFor(std::string_view fileName);
	/// @brief The language a name (or alias) is.
	/// @param name The name, any case.
	/// @return Its name in languages(), or "" if none.
	[[nodiscard]] static std::string canonical(std::string_view name);

	/// @brief Frees the tree.
	~SyntaxDocument();
	SyntaxDocument(const SyntaxDocument&) = delete;
	SyntaxDocument& operator=(const SyntaxDocument&) = delete;

	/// @brief Its language's name.
	[[nodiscard]] const std::string& language() const noexcept;
	/// @brief The style names, in the order SyntaxSpan::style counts: the highlight query's capture names (e.g.
	/// "keyword", "function.method").
	[[nodiscard]] const std::vector<std::string>& styles() const noexcept;

	/// @brief Records an edit of the text, so the next parse() reuses what it didn't touch.
	/// @param edit The edit.
	void edit(const SyntaxEdit& edit);
	/// @brief Parses the text (again, after edits: only what changed).
	/// @param read Reads it.
	/// @param size Its size, in bytes.
	/// @param changed If given, gets the rows whose syntax changed (with the rows edited since the last parse).
	/// @return What it did.
	SyntaxParse parse(const SyntaxReader& read, std::size_t size, std::vector<SyntaxRows>* changed = nullptr);
	/// @brief Whether there is a tree (a parse succeeded once).
	[[nodiscard]] bool parsed() const noexcept;
	/// @brief Whether edits came since the last complete parse.
	[[nodiscard]] bool stale() const noexcept;

	/// @brief The runs of style on some lines.
	/// @param read Reads the text (predicates such as #match? read nodes).
	/// @param first The first line.
	/// @param last The last line (included).
	/// @param out Gets the runs (cleared first), by line, outer first within a line.
	/// @return false if a limit cut them short (the runs found are kept).
	bool highlight(const SyntaxReader& read, std::uint32_t first, std::uint32_t last,
	               std::vector<SyntaxSpan>& out) const;
	/// @brief The largest fold starting on a line.
	/// @param row The line.
	/// @return The fold, or std::nullopt if none starts there.
	[[nodiscard]] std::optional<SyntaxFold> fold(std::uint32_t row) const;
	/// @brief The folds starting on some lines.
	/// @param first The first line.
	/// @param last The last line (included).
	/// @param out Gets them (cleared first), by first line, one per line (the largest).
	void folds(std::uint32_t first, std::uint32_t last, std::vector<SyntaxFold>& out) const;
	/// @brief The brackets a bracket starting at a place pairs with.
	/// @param at Where a bracket starts.
	/// @return Both brackets, or std::nullopt if no pair has one there.
	[[nodiscard]] std::optional<SyntaxBrackets> brackets(SyntaxPoint at) const;
	/// @brief The tree as an S-expression, for tests.
	[[nodiscard]] std::string tree() const;

private:
	struct State;
	/// @brief Takes the state create() built.
	explicit SyntaxDocument(std::unique_ptr<State> state);
	std::unique_ptr<State> state_; ///< The parser, tree and queries.
};

} // namespace affresco::services
