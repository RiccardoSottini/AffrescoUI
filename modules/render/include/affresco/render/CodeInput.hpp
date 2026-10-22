#pragma once

/// @file
/// @brief CodeInput: a one-time-code input drawn as a row of cells, one character per cell.

#include "affresco/render/TextArea.hpp"

#include <cstddef>

namespace affresco {

/// @brief A one-time-code input: a TextArea that takes a fixed number of characters (digits unless told otherwise)
/// and draws each in a cell of its own, the next one to fill outlined while focused.
///
/// Typing, Backspace and pasting work as in a TextArea; a pasted code fills the cells it fits, dropping what the
/// input doesn't accept (spaces, dashes).
class CodeInput : public TextArea {
public:
	/// @brief Six digit cells.
	CodeInput();
	/// @brief Sets the number of cells, which is also the most characters.
	/// @param length 1 to 12.
	void setLength(std::size_t length);
	/// @brief The number of cells.
	/// @return See setLength().
	[[nodiscard]] std::size_t length() const noexcept { return length_; }
	/// @brief Sets the cells' background.
	/// @param color The colour.
	void setCellColor(Color color);
	/// @brief Sets the cells' outline.
	/// @param color The colour.
	void setCellBorderColor(Color color);
	/// @brief Sets the outline of the cell being filled, while focused.
	/// @param color The colour.
	void setActiveColor(Color color);
	/// @brief Sets the space between cells.
	/// @param gap In window units.
	void setGap(float gap);
	/// @brief Sets the cells' corner radius.
	/// @param radius In window units.
	void setCellRadius(float radius);

	/// The most cells.
	static constexpr std::size_t mostCells = 12;

protected:
	void paint(Painter& painter) const override;

private:
	std::size_t length_ = 6;               ///< See setLength().
	Color cellColor_ = colors::white;      ///< See setCellColor().
	Color cellBorderColor_ = colors::idle; ///< See setCellBorderColor().
	Color activeColor_ = colors::accent;   ///< See setActiveColor().
	float gap_ = 8.0f;                     ///< See setGap().
	float cellRadius_ = 6.0f;              ///< See setCellRadius().
};

} // namespace affresco
