#pragma once

/// @file
/// @brief Carousel: pages shown one at a time, with previous and next arrows and a row of dots.

#include "affresco/render/Panel.hpp"

#include <cstddef>
#include <vector>

namespace affresco {

/// @brief Its children are pages, shown one at a time: the others are placed beside it, outside the carousel, where
/// clipping hides them (they draw nothing, take no clicks and no focus).
///
/// The carousel keeps a gutter on each side for its arrows and a strip below for one dot per page; clicking an arrow
/// or a dot changes the page. With the keyboard: Left and Right (mirrored right to left), Home and End. A horizontal
/// swipe on a trackpad turns the page too. With setLoop(), the last page is followed by the first. The page is the
/// owner's: the carousel reports what the user asks for (onIndexChanged) after showing it.
class Carousel : public Panel {
public:
	/// @brief An empty carousel with arrows and dots.
	Carousel();
	/// @brief Shows a page, without firing onIndexChanged.
	/// @param index From 0; one past the last shows the last.
	void setIndex(std::size_t index);
	/// @brief The page shown.
	/// @return Its index, 0 with no pages.
	[[nodiscard]] std::size_t index() const noexcept { return index_; }
	/// @brief The number of pages: visible children that aren't overlays.
	/// @return The count.
	[[nodiscard]] std::size_t pageCount() const noexcept;
	/// @brief Whether turning past the last page goes to the first, and back.
	/// @param loop True to loop.
	void setLoop(bool loop) noexcept { loop_ = loop; }
	/// @brief Shows or hides the arrows and their gutters.
	/// @param arrows True to show them.
	void setArrows(bool arrows);
	/// @brief Shows or hides the dots and their strip.
	/// @param dots True to show them.
	void setDots(bool dots);
	/// @brief Sets the colours of the arrows and the dots.
	/// @param arrow The arrows.
	/// @param dot The other pages' dots.
	/// @param activeDot The page shown's dot.
	void setColors(Color arrow, Color dot, Color activeDot);
	/// @brief Turns the page by a step, as the user does, firing onIndexChanged if it changes.
	/// @param step -1 for the previous page, +1 for the next.
	void turn(int step);
	/// @brief Where a point falls: an arrow (-1 or +1, in reading order), a dot (its page index + 2), or 0.
	/// @param position In window units.
	/// @return The code.
	[[nodiscard]] int controlAt(Vec2 position) const noexcept;

	Signal<std::size_t> onIndexChanged; ///< The user turned to a page (its index).

	/// The width of an arrow's gutter.
	static constexpr float gutter = 28.0f;
	/// The height of the dots' strip.
	static constexpr float strip = 20.0f;

protected:
	void arrange() override;
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	void keyEvent(const KeyEvent& event) override;
	bool scrollContent(Vec2 delta) override;
	void paint(Painter& painter) const override;

private:
	/// @brief The pages, in order.
	/// @return Visible children that aren't overlays.
	[[nodiscard]] std::vector<Panel*> pages() const;
	/// @brief Shows a page and reports it if it changed.
	/// @param index The page.
	void choose(std::size_t index);
	/// @brief The area the page shown takes, inside the gutters and above the strip.
	/// @return In the carousel's own units, from its top-left corner.
	[[nodiscard]] Rect pageArea() const noexcept;

	std::size_t index_ = 0;                 ///< See setIndex().
	bool loop_ = false;                     ///< See setLoop().
	bool arrows_ = true;                    ///< See setArrows().
	bool dots_ = true;                      ///< See setDots().
	Color arrowColor_ = colors::muted;      ///< See setColors().
	Color dotColor_ = colors::idle;         ///< See setColors().
	Color activeDotColor_ = colors::accent; ///< See setColors().
	float swipe_ = 0.0f;                    ///< Horizontal scrolling gathered towards a page turn.
};

} // namespace affresco
