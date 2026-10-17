#pragma once

/// @file
/// @brief Panel: a rectangle in the UI tree, with layout, input events and drawing.

#include "affresco/render/Color.hpp"
#include "affresco/render/Events.hpp"
#include "affresco/render/Geometry.hpp"
#include "affresco/render/Length.hpp"
#include "affresco/core/Signal.hpp"

#include <array>
#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

class Painter;
class Window;

namespace gl {
class QuadRenderer;
}

/// @brief An edge of a panel, for anchors.
enum class Edge {
	Top,    ///< Top edge.
	Bottom, ///< Bottom edge.
	Left,   ///< Left edge.
	Right,  ///< Right edge.
};

/// @brief A rectangle in the UI tree. Owns its children; positions and sizes are relative to the parent.
///
/// Layout rules, per axis (vertical shown):
/// - anchored to Top: `top = parent.top + offset`
/// - anchored to Bottom: `bottom = parent.top + parent.height - offset` (with Top too, the panel stretches)
/// - not anchored: `top = parent.top + y`
/// - `size = max(height, minHeight)`; percentages refer to the parent's size
/// - the result is clipped to the parent; a panel clipped to nothing is not shown
///
/// Setters only mark the window dirty: layout runs once, before the next frame. Subclasses draw in paint(), react in
/// the event hooks and size themselves in prepareLayout().
class Panel {
public:
	Panel() = default;
	/// @brief Destroys the subtree and clears the window's focus, hover and capture if they point here.
	virtual ~Panel();

	Panel(const Panel&) = delete;
	Panel& operator=(const Panel&) = delete;
	Panel(Panel&&) = delete;
	Panel& operator=(Panel&&) = delete;

	/* Tree */

	/// @brief Creates a child, drawn on top of the existing ones.
	/// @tparam T Panel or a subclass.
	/// @param args Forwarded to T's constructor.
	/// @return The child, owned by this panel.
	template <std::derived_from<Panel> T = Panel, typename... Args> T& add(Args&&... args) {
		auto child = std::make_unique<T>(std::forward<Args>(args)...);
		T& ref = *child;
		adopt(std::move(child));
		return ref;
	}

	/// @brief Adds an existing panel as the last child, e.g. one made by a factory.
	/// @param child The panel; this panel takes ownership.
	/// @return The child.
	Panel& addChild(std::unique_ptr<Panel> child);

	/// @brief Replaces a child, destroying the old one.
	/// @param index The child's position.
	/// @param child The new panel; this panel takes ownership.
	/// @return The new child.
	/// @throws std::out_of_range If there is no child at @p index.
	Panel& replaceChild(std::size_t index, std::unique_ptr<Panel> child);

	/// @brief Destroys the children from a position to the end.
	/// @param from The first child to destroy.
	void removeChildren(std::size_t from = 0);

	/// @brief The parent, or nullptr for a root.
	[[nodiscard]] Panel* parent() const noexcept { return parent_; }
	/// @brief The window, or nullptr while not attached to one.
	[[nodiscard]] Window* window() const noexcept { return window_; }
	/// @brief The children, back to front.
	[[nodiscard]] std::span<const std::unique_ptr<Panel>> children() const noexcept { return children_; }

	/* Identity: lets code find panels built elsewhere (e.g. from a screen file) */

	/// @brief Sets the id.
	/// @param id Any string.
	void setId(std::string id) { id_ = std::move(id); }
	/// @brief The id; empty if none was set.
	[[nodiscard]] const std::string& id() const noexcept { return id_; }

	/// @brief Finds a panel by id, depth first.
	/// @param id The id.
	/// @return This panel or the first matching descendant, or nullptr.
	[[nodiscard]] Panel* findById(std::string_view id) noexcept;

	/* Position: used on the axes without anchors */

	/// @brief Sets the offset from the parent's top-left corner.
	/// @param x Horizontal offset.
	/// @param y Vertical offset.
	void setPosition(Length x, Length y);
	/// @brief Sets the horizontal offset.
	/// @param x The offset.
	void setX(Length x);
	/// @brief Sets the vertical offset.
	/// @param y The offset.
	void setY(Length y);
	/// @brief The horizontal offset.
	[[nodiscard]] Length x() const noexcept { return x_; }
	/// @brief The vertical offset.
	[[nodiscard]] Length y() const noexcept { return y_; }

	/* Size */

	/// @brief Sets the size.
	/// @param width The width.
	/// @param height The height.
	void setSize(Length width, Length height);
	/// @brief Sets the width.
	/// @param width The width.
	void setWidth(Length width);
	/// @brief Sets the height.
	/// @param height The height.
	void setHeight(Length height);
	/// @brief Sets the minimum size.
	/// @param minWidth The minimum width.
	/// @param minHeight The minimum height.
	void setMinSize(Length minWidth, Length minHeight);
	/// @brief Sets the minimum width.
	/// @param minWidth The minimum.
	void setMinWidth(Length minWidth);
	/// @brief Sets the minimum height.
	/// @param minHeight The minimum.
	void setMinHeight(Length minHeight);
	/// @brief The width.
	[[nodiscard]] Length width() const noexcept { return width_; }
	/// @brief The height.
	[[nodiscard]] Length height() const noexcept { return height_; }
	/// @brief The minimum width.
	[[nodiscard]] Length minWidth() const noexcept { return minWidth_; }
	/// @brief The minimum height.
	[[nodiscard]] Length minHeight() const noexcept { return minHeight_; }

	/* Anchors: keep an edge at a fixed distance from the same edge of the parent */

	/// @brief Anchors an edge.
	/// @param edge The edge.
	/// @param offset Distance from the parent's same edge.
	void setAnchor(Edge edge, Length offset);
	/// @brief Removes an anchor.
	/// @param edge The edge.
	void clearAnchor(Edge edge);
	/// @brief An edge's anchor.
	/// @param edge The edge.
	/// @return Its offset, or std::nullopt if not anchored.
	[[nodiscard]] std::optional<Length> anchor(Edge edge) const noexcept;

	/* Appearance */

	/// @brief Sets the background colour that paint() fills with.
	/// @param color The colour.
	void setColor(Color color);
	/// @brief The background colour.
	[[nodiscard]] Color color() const noexcept { return color_; }

	/// @brief Shows or hides the panel and its subtree.
	/// @param visible false to hide it from drawing, hit-testing and stacks.
	void setVisible(bool visible);

	/// @brief Sets the cursor shown over this panel, or while it holds the mouse during a drag.
	/// @param cursor The shape.
	void setCursor(Cursor cursor);
	/// @brief The cursor shape.
	[[nodiscard]] Cursor cursor() const noexcept { return cursor_; }
	/// @brief The flag set by setVisible().
	[[nodiscard]] bool isVisible() const noexcept { return visible_; }
	/// @brief Whether it is on screen: visible, with visible ancestors and a non-empty area.
	[[nodiscard]] bool isShown() const noexcept {
		return shown_;
	}

	/* Layout results, valid after the last layout pass */

	/// @brief The visible area: absolute, clipped to the ancestors.
	[[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }
	/// @brief The whole area: absolute, before clipping.
	[[nodiscard]] const Rect& frame() const noexcept { return frame_; }
	/// @brief The size before clipping.
	[[nodiscard]] Vec2 resolvedSize() const noexcept { return size_; }
	/// @brief The top-left corner, relative to the parent's.
	[[nodiscard]] Vec2 relativePosition() const noexcept;

	/// @brief Measures the height this panel takes in a layout pass, for containers such as Stack.
	/// @param reference The parent's height, for percentages.
	/// @return max(height, minHeight), after prepareLayout().
	[[nodiscard]] float measuredHeight(float reference);

	/// @brief Finds the deepest shown panel at a point; later siblings are on top.
	/// @param point In window units.
	/// @return This panel or a descendant, or nullptr.
	[[nodiscard]] Panel* hitTest(Vec2 point) noexcept;

	/// @brief Whether this is its window's focused panel.
	[[nodiscard]] bool hasFocus() const noexcept;

	/* Signals, fired by the default event hooks */

	Signal<const KeyEvent&> onKey;                 ///< Key event while focused.
	Signal<const CharEvent&> onChar;               ///< Typed character while focused.
	Signal<const MouseMoveEvent&> onMouseMove;     ///< Pointer moved over it, or while it holds the mouse.
	Signal<const MouseButtonEvent&> onMouseButton; ///< Button pressed on it, or released while it holds the mouse.
	Signal<const ScrollEvent&> onScroll;           ///< Scroll with the pointer over it.
	Signal<bool> onFocusChanged;                   ///< Focus gained (true) or lost (false).
	Signal<bool> onHover;                          ///< Pointer entered (true) or left (false).

protected:
	/* Event hooks called by the Window. Widgets override them; call the base to keep the signals firing. */

	/// @brief Handles a key event while focused.
	/// @param event The event.
	virtual void keyEvent(const KeyEvent& event) { onKey(event); }
	/// @brief Handles a typed character while focused.
	/// @param event The event.
	virtual void charEvent(const CharEvent& event) { onChar(event); }
	/// @brief Handles a pointer move over it, or while it holds the mouse.
	/// @param event The event.
	virtual void mouseMoveEvent(const MouseMoveEvent& event) { onMouseMove(event); }
	/// @brief Handles a button press on it, or a release while it holds the mouse.
	/// @param event The event.
	virtual void mouseButtonEvent(const MouseButtonEvent& event) { onMouseButton(event); }
	/// @brief Handles a scroll with the pointer over it.
	/// @param event The event.
	virtual void scrollEvent(const ScrollEvent& event) { onScroll(event); }
	/// @brief Handles a focus change.
	/// @param focused true if it gained the focus.
	virtual void focusEvent(bool focused) { onFocusChanged(focused); }
	/// @brief Handles the pointer entering or leaving.
	/// @param hovered true if it entered.
	virtual void hoverEvent(bool hovered) { onHover(hovered); }

	/// @brief Called at the start of each layout pass, before the size is resolved, so the panel can size itself.
	///
	/// Property changes made here don't trigger another layout.
	virtual void prepareLayout() {}

	/// @brief Draws this panel; the children are drawn after it, on top. The default fills bounds() with color().
	/// @param painter Clips to bounds().
	virtual void paint(Painter& painter) const;

	/// @brief Tells the window that the layout or the drawing changed.
	void invalidate() noexcept;

private:
	friend class Window;
	friend class gl::QuadRenderer;

	/// @brief Computes frame, bounds and visibility for this panel and its subtree.
	/// @param parentBounds The parent's visible area, which clips this panel.
	/// @param parentSize The parent's unclipped size, for percentages and bottom/right anchors.
	/// @param parentShown Whether the parent is shown.
	void layout(const Rect& parentBounds, Vec2 parentSize, bool parentShown);

	/// @brief Takes ownership of a new last child.
	/// @param child The child.
	void adopt(std::unique_ptr<Panel> child);

	/// @brief Sets the window of this panel and its subtree.
	/// @param window The window, or nullptr.
	void attachTo(Window* window) noexcept;

	std::string id_;                               ///< See id().
	Panel* parent_ = nullptr;                      ///< Not owning.
	Window* window_ = nullptr;                     ///< Not owning.
	std::vector<std::unique_ptr<Panel>> children_; ///< Back to front.

	Length x_, y_;                                 ///< Position on unanchored axes.
	Length width_, height_;                        ///< Size.
	Length minWidth_, minHeight_;                  ///< Minimum size.
	std::array<std::optional<Length>, 4> anchors_; ///< Indexed by Edge.
	Color color_ = colors::panel;                  ///< Background.
	bool visible_ = true;                          ///< See setVisible().
	Cursor cursor_ = Cursor::Arrow;                ///< See setCursor().

	Rect frame_;         ///< See frame().
	Rect bounds_;        ///< See bounds().
	Vec2 size_;          ///< See resolvedSize().
	bool shown_ = false; ///< See isShown().
};

} // namespace affresco
