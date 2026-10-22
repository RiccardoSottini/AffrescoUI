#pragma once

/// @file
/// @brief TerminalModel: the screen of a VT terminal (libvterm) and its bounded scrollback, without drawing, so it is
/// tested without a window. Terminal.hpp draws it and feeds it a pty.

#include <affresco/render/Input.hpp>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct VTerm;
struct VTermScreen;

namespace forge {

/// @brief One cell of the terminal: a character and how it looks.
struct TerminalCell {
	char32_t character = 0;        ///< The character (its first code point; combining marks are dropped); 0 if blank.
	std::uint32_t foreground = 0;  ///< 0xRRGGBB, when not defaultForeground.
	std::uint32_t background = 0;  ///< 0xRRGGBB, when not defaultBackground.
	bool defaultForeground = true; ///< Whether it uses the widget's text colour.
	bool defaultBackground = true; ///< Whether it uses the widget's colour.
	bool bold = false;             ///< Bold.
	bool underline = false;        ///< Underlined.
	bool italic = false;           ///< Italic.
	bool reverse = false;          ///< Foreground and background swapped.
	std::uint8_t width = 1; ///< Columns it covers: 2 for a wide character, 0 for the column a wide character covers.

	/// @brief Compares every field.
	[[nodiscard]] bool operator==(const TerminalCell&) const noexcept = default;
};

/// @brief Where the cursor is, in screen rows and columns from 0.
struct TerminalCursor {
	int row = 0;         ///< Its row.
	int column = 0;      ///< Its column.
	bool visible = true; ///< Whether the program shows it.
};

/// @brief The most rows or columns a terminal has (the pty provider's limit).
constexpr int mostTerminalCells = 1000;
/// @brief The scrollback lines kept when none are asked for.
constexpr std::size_t defaultScrollbackLines = 1000;
/// @brief The most scrollback lines a terminal keeps.
constexpr std::size_t mostScrollbackLines = 10000;
/// @brief The most bytes of replies (to the program's queries, and typed keys) waiting for takeOutput().
constexpr std::size_t mostPendingOutput = std::size_t{64} * 1024;

/// @brief A VT terminal's screen: feeds it the bytes a program writes, turns keys into the bytes it reads.
///
/// Nothing the program writes leaves the model: the window title is kept only when allowTitle(), clipboard writes
/// (OSC 52) and the bell are dropped. Lines scrolled off the top go to a scrollback of at most scrollbackLimit() lines
/// (trailing blanks trimmed), and come back when the terminal grows taller.
class TerminalModel {
public:
	/// @brief A blank terminal.
	/// @param rows Its rows, clamped to 1..mostTerminalCells.
	/// @param columns Its columns, clamped likewise.
	/// @param scrollbackLines The scrollback it keeps, at most mostScrollbackLines.
	TerminalModel(int rows, int columns, std::size_t scrollbackLines = defaultScrollbackLines);
	/// @brief Frees the terminal.
	~TerminalModel();

	TerminalModel(const TerminalModel&) = delete;
	TerminalModel& operator=(const TerminalModel&) = delete;
	TerminalModel(TerminalModel&&) = delete;
	TerminalModel& operator=(TerminalModel&&) = delete;

	/// @brief Feeds it what the program wrote.
	/// @param bytes UTF-8 text and escape sequences, split anywhere.
	void write(std::string_view bytes);

	/// @brief Changes its size; the program learns it from the pty.
	/// @param rows Its rows, clamped to 1..mostTerminalCells.
	/// @param columns Its columns, clamped likewise.
	void resize(int rows, int columns);

	/// @brief Takes the bytes waiting for the program: typed keys and replies to its queries.
	/// @return The bytes; none wait afterwards.
	[[nodiscard]] std::string takeOutput();

	/// @brief Types a character.
	/// @param character The code point.
	/// @param mods The modifiers held (Control turns a letter into its control code).
	void typeCharacter(char32_t character, affresco::Mods mods = {});

	/// @brief Presses a key that isn't a character: Enter, Tab, Backspace, Escape, the arrows, Insert, Delete, Home,
	/// End, Page Up and Down, F1 to F12.
	/// @param key The key.
	/// @param mods The modifiers held.
	/// @return Whether it is such a key (and its bytes wait in takeOutput()).
	bool pressKey(affresco::Key key, affresco::Mods mods = {});

	/// @brief Its rows.
	[[nodiscard]] int rows() const noexcept { return rows_; }
	/// @brief Its columns.
	[[nodiscard]] int columns() const noexcept { return columns_; }
	/// @brief The lines in the scrollback, at most scrollbackLimit().
	[[nodiscard]] std::size_t scrollbackSize() const noexcept { return scrollback_.size(); }
	/// @brief The most lines the scrollback keeps.
	[[nodiscard]] std::size_t scrollbackLimit() const noexcept { return scrollbackLimit_; }
	/// @brief Changes how many lines the scrollback keeps, dropping the oldest past it.
	/// @param lines The lines, at most mostScrollbackLines.
	void setScrollbackLimit(std::size_t lines);

	/// @brief A cell of the scrollback or the screen.
	/// @param line The line: the scrollback's lines first (oldest at 0), then the screen's rows.
	/// @param column The column, from 0.
	/// @return The cell; a blank one past the line's end.
	[[nodiscard]] TerminalCell cell(std::size_t line, int column) const;

	/// @brief The text of a line, trailing blanks trimmed (for tests and for copying).
	/// @param line As for cell().
	/// @return The text, in UTF-32.
	[[nodiscard]] std::u32string lineText(std::size_t line) const;

	/// @brief The cursor.
	[[nodiscard]] TerminalCursor cursor() const;

	/// @brief Whether the program shows its alternate screen (a full-screen program such as an editor).
	[[nodiscard]] bool alternateScreen() const noexcept { return alternateScreen_; }

	/// @brief Whether the program may set the title.
	[[nodiscard]] bool allowTitle() const noexcept { return allowTitle_; }
	/// @brief Lets the program set the title, or not (then title() stays empty).
	/// @param allow Whether it may.
	void setAllowTitle(bool allow);
	/// @brief The title the program set, if allowTitle(); at most 256 bytes.
	[[nodiscard]] const std::string& title() const noexcept { return title_; }

private:
	/// @brief Frees a VTerm.
	struct VTermDeleter {
		/// @brief Frees it.
		/// @param terminal The terminal.
		void operator()(VTerm* terminal) const noexcept;
	};

	/// @brief The libvterm callbacks, which reach the private members.
	friend struct TerminalCallbacks;

	std::unique_ptr<VTerm, VTermDeleter> terminal_;        ///< The terminal.
	VTermScreen* screen_ = nullptr;                        ///< Its screen, owned by terminal_.
	std::deque<std::vector<TerminalCell>> scrollback_;     ///< Lines scrolled off the top, oldest first.
	std::size_t scrollbackLimit_ = defaultScrollbackLines; ///< See scrollbackLimit().
	std::string output_;                                   ///< See takeOutput().
	std::string title_;                                    ///< See title().
	int rows_ = 0;                                         ///< See rows().
	int columns_ = 0;                                      ///< See columns().
	bool cursorVisible_ = true;                            ///< Whether the program shows the cursor.
	bool alternateScreen_ = false;                         ///< See alternateScreen().
	bool allowTitle_ = false;                              ///< See allowTitle().
};

} // namespace forge
