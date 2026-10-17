#pragma once

/// @file
/// @brief Stack: a container that places its children in a column or a row; Grid: in equal columns.

#include "affresco/render/Panel.hpp"

namespace affresco {

/// @brief The axis a Stack places its children along.
enum class Direction {
	Column, ///< One below the other.
	Row,    ///< Side by side, left to right.
};

/// @brief Places its visible children one after the other, `gap` apart, in order: in a column (the default) or a row.
///
/// Along its direction the stack manages the children's anchors; across it they keep their own (in a column, their
/// width and horizontal anchors). Children with a grow share take the room left over, like CSS flex-grow. With
/// Align::End the block sits at the end (in a column, at the bottom like a chat history: older children overflow at
/// the top and get clipped); with Align::Center, in the middle. A wrapping row starts a new line when one is full.
class Stack : public Panel {
public:
	/// @brief An empty stack with a transparent background.
	Stack();

	/// @brief Sets the space between children.
	/// @param gap In pixels.
	void setGap(float gap);
	/// @brief The space between children, in pixels.
	[[nodiscard]] float gap() const noexcept { return gap_; }

	/// @brief Sets where the block of children sits along the direction.
	/// @param alignment Align::Start (top or left), Align::Center or Align::End (bottom or right).
	void setAlignment(Align alignment);
	/// @brief Where the block of children sits.
	[[nodiscard]] Align alignment() const noexcept { return alignment_; }

	/// @brief Sets the direction.
	/// @param direction Column or row.
	void setDirection(Direction direction);
	/// @brief The direction.
	[[nodiscard]] Direction direction() const noexcept { return direction_; }

	/// @brief Lets a row continue on a new line when the next child doesn't fit (lines are `gap` apart).
	/// @param wrap true to wrap.
	void setWrap(bool wrap);

protected:
	/// @brief Anchors each visible child after the previous one, sharing spare room among growing children.
	void arrange() override;

private:
	float gap_ = 0.0f;                       ///< See gap().
	Align alignment_ = Align::Start;         ///< See alignment().
	Direction direction_ = Direction::Column; ///< See direction().
	bool wrap_ = false;                      ///< See setWrap().
};

/// @brief Places its visible children in equal columns, row after row, `gap` apart; each row is as tall as its
/// tallest child.
class Grid : public Panel {
public:
	/// @brief An empty grid with a transparent background and 2 columns.
	Grid();

	/// @brief Sets the number of columns.
	/// @param columns At least 1.
	void setColumns(int columns);
	/// @brief Sets the space between cells, across and down.
	/// @param gap In pixels.
	void setGap(float gap);

protected:
	/// @brief Sizes each visible child to its cell and anchors it there.
	void arrange() override;

private:
	int columns_ = 2;  ///< See setColumns().
	float gap_ = 0.0f; ///< See setGap().
};

} // namespace affresco
