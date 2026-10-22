#pragma once

/// @file
/// @brief An editor highlighter on the syntax engine (affresco/services/Syntax.hpp): a TextEditor's text parsed with
/// tree-sitter, kept in step with each edit (only what changed is parsed and coloured again), its captures painted in
/// a palette's colours, and its folds and bracket pairs from the grammar's queries.

#include "affresco/render/Color.hpp"
#include "affresco/render/TextEditor.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief A grammar's highlighter for a TextEditor (TextEditor::setHighlighter()).
///
/// The text is parsed when first drawn after an edit, within the engine's time budget: a parse over it leaves the
/// text coloured from the edited tree until the next edit. Larger texts than the engine parses stay plain.
class SyntaxHighlighter final : public EditorHighlighter {
public:
	/// @brief Gives a style's colour, e.g. "keyword" or "function.method"; nothing to leave it the text colour.
	using Palette = std::function<std::optional<Color>(std::string_view style)>;

	/// @brief A run painted over the grammar's colours, e.g. a language server's semantic token. Runs move with the
	/// edits after them, and those on edited lines are dropped.
	struct Overlay {
		std::size_t line = 0;   ///< The line, from 0.
		std::size_t column = 0; ///< Where it starts, in code points.
		std::size_t length = 0; ///< Its code points.
		std::string style;      ///< Its style, coloured by the palette like the grammar's ("function", "type").
	};
	static constexpr std::size_t maxOverlays = 100'000; ///< Overlay runs kept, at most; the rest are dropped.

	/// @brief A highlighter for a language.
	/// @param language A language or alias (e.g. "cpp", "c++", "md"), any case.
	/// @return It, or nullptr if no grammar has that name.
	[[nodiscard]] static std::unique_ptr<SyntaxHighlighter> create(std::string_view language);
	/// @brief The language a name or alias is.
	/// @param name The name.
	/// @return Its name, or "" if no grammar has it.
	[[nodiscard]] static std::string canonical(std::string_view name);
	/// @brief The language a file's extension suggests.
	/// @param fileName The name, or a path.
	/// @return Its name, or "".
	[[nodiscard]] static std::string languageFor(std::string_view fileName);

	/// @brief Frees the tree.
	~SyntaxHighlighter() override;
	SyntaxHighlighter(const SyntaxHighlighter&) = delete;
	SyntaxHighlighter& operator=(const SyntaxHighlighter&) = delete;

	/// @brief Its language's name.
	[[nodiscard]] const std::string& language() const noexcept;
	/// @brief The styles its grammar names (e.g. "keyword", "function.method"), for a palette to resolve ahead.
	[[nodiscard]] const std::vector<std::string>& styles() const noexcept;
	/// @brief Sets the colours of the styles. A style without one takes its parent's ("function.method" then
	/// "function"); none leaves the text colour. The editor should repaint after.
	/// @param palette It.
	void setPalette(Palette palette);
	/// @brief Replaces the runs painted over the grammar's colours (at most maxOverlays). The editor should repaint
	/// after.
	/// @param runs The runs, in any order; a style the palette doesn't colour paints nothing.
	void setOverlay(std::vector<Overlay> runs);
	/// @brief Tags who installed it, so that only they replace it.
	/// @param owner Any address; nullptr for none.
	void setOwner(const void* owner) noexcept;
	/// @brief Its tag (setOwner()).
	[[nodiscard]] const void* owner() const noexcept;

	void reset(const TextModel& model) override;
	void changed(const TextModel& model, const TextChange& change) override;
	void colorLine(const TextModel& model, std::size_t line, std::u32string_view text,
	               std::vector<Color>& colors) override;
	bool foldable(const TextModel& model, std::size_t line, std::optional<FoldRange>& range) override;
	bool matchingBracket(const TextModel& model, TextPosition at, std::optional<TextPosition>& other) override;

private:
	struct State;
	/// @brief Takes the state create() built.
	explicit SyntaxHighlighter(std::unique_ptr<State> state);
	std::unique_ptr<State> state_; ///< The document, the palette and what was coloured.
};

} // namespace affresco::format
