#pragma once

/// @file
/// @brief Split: panes side by side or one above the other, resized by dragging the sashes between them.

#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <vector>

namespace affresco {

/// @brief Lays its children out as panes along one axis, with a sash between each two that the pointer or the
/// keyboard moves.
///
/// Every pane but the last has a size in pixels; the last takes what is left. A pane never gets smaller than its
/// minimum: when the split is too small for its sizes, the panes before the last give way, the latest first.
/// Dragging a sash moves only the two panes beside it. A double click on a sash, or Home, puts the sizes back to
/// the defaults. With the keyboard (focus comes with a click on a sash), the arrows move the active sash by 8
/// pixels (32 with Shift), Page Up and Page Down pick the previous or the next sash.
class Split : public Panel {
public:
	/// @brief The axis the panes are laid along.
	enum class Direction {
		Row,    ///< Side by side, sashes are vertical.
		Column, ///< One above the other, sashes are horizontal.
	};

	/// @brief A transparent split with the resize cursor.
	/// @param direction See Direction.
	explicit Split(Direction direction = Direction::Row);

	/// @brief Sets the axis.
	/// @param direction See Direction.
	void setDirection(Direction direction);
	/// @brief The axis.
	[[nodiscard]] Direction direction() const noexcept { return direction_; }

	/// @brief Sets the sizes of the panes, without firing onSizesChanged.
	/// @param sizes Pixels per pane, in order; the last pane's (and any after the panes) are ignored, missing ones
	/// share what is left equally.
	void setSizes(std::vector<float> sizes);
	/// @brief The sizes the panes got in the last layout pass, the last pane's included.
	[[nodiscard]] const std::vector<float>& sizes() const noexcept { return laidOut_; }
	/// @brief Sets the sizes a double click on a sash (or Home) goes back to.
	/// @param sizes As in setSizes(); empty for equal panes.
	void setDefaultSizes(std::vector<float> sizes);
	/// @brief Sets the smallest size of each pane.
	/// @param minimums Pixels per pane, in order; the last one is used for the panes after it (empty for 0).
	void setMinimumSizes(std::vector<float> minimums);

	/// @brief Sets the look of the sashes.
	/// @param thickness Pixels between two panes.
	/// @param idle The sash's colour.
	/// @param active Its colour while dragged, or while the split has focus (the active sash).
	void setSash(float thickness, Color idle, Color active);

	/// @brief Puts the sizes back to the defaults and fires onSizesChanged.
	void reset();

	Signal<const std::vector<float>&> onSizesChanged; ///< The user moved a sash or reset: the new sizes().

protected:
	void arrange() override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	void keyEvent(const KeyEvent& event) override;
	/// @brief Repaints: the active sash shows only while the split has the focus.
	/// @param focused Whether it gained the focus.
	void focusEvent(bool focused) override;
	void paint(Painter& painter) const override;

private:
	/// @brief The minimum of a pane.
	/// @param pane Its index.
	/// @return Pixels.
	[[nodiscard]] float minimumOf(std::size_t pane) const noexcept;
	/// @brief Where a sash starts along the axis, relative to the split.
	/// @param sash Its index (the sash after pane `sash`).
	/// @return Pixels.
	[[nodiscard]] float sashStart(std::size_t sash) const noexcept;
	/// @brief The sash under a point.
	/// @param position In window units.
	/// @return Its index, or the number of sashes for none.
	[[nodiscard]] std::size_t sashAt(Vec2 position) const noexcept;
	/// @brief Moves a sash, keeping both panes beside it at least at their minimums, and fires onSizesChanged.
	/// @param sash Its index.
	/// @param delta Pixels along the axis.
	void moveSash(std::size_t sash, float delta);
	/// @brief Sizes for a number of panes and an amount of room: the wanted ones, within the minimums.
	/// @param wanted The sizes asked for (setSizes() or the defaults).
	/// @param panes How many panes.
	/// @param room Pixels for all panes.
	/// @return One size per pane.
	[[nodiscard]] std::vector<float> fit(const std::vector<float>& wanted, std::size_t panes, float room) const;

	Direction direction_;           ///< See direction().
	std::vector<float> wanted_;     ///< See setSizes().
	std::vector<float> defaults_;   ///< See setDefaultSizes().
	std::vector<float> minimums_;   ///< See setMinimumSizes().
	std::vector<float> laidOut_;    ///< See sizes().
	float thickness_ = 4.0f;        ///< See setSash().
	Color idle_ = colors::track;    ///< See setSash().
	Color active_ = colors::accent; ///< See setSash().
	std::size_t dragged_ = 0;       ///< The sash being dragged, if dragging_.
	bool dragging_ = false;         ///< The left button is held on a sash.
	float grab_ = 0.0f;             ///< Where the pointer held the dragged sash, from its start.
	std::size_t activeSash_ = 0;    ///< The sash the keyboard moves.
	double lastPress_ = -1.0;       ///< When a sash was last pressed, for double clicks (clock seconds).
	std::size_t lastPressed_ = 0;   ///< Which sash that was.
};

} // namespace affresco
