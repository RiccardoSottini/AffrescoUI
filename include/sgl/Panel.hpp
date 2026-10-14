#pragma once

#include "sgl/Color.hpp"
#include "sgl/Events.hpp"
#include "sgl/Geometry.hpp"
#include "sgl/Length.hpp"
#include "sgl/Signal.hpp"

#include <array>
#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sgl {

class Painter;
class Window;

namespace gl {
class QuadRenderer;
}

enum class Edge {
	Top,
	Bottom,
	Left,
	Right,
};

// A rectangle in the UI tree. Owns its children; positions/sizes are relative to the parent.
//
// Layout rules (per axis, vertical shown):
//  - anchored to Top:     top = parent.top + offset
//  - anchored to Bottom:  bottom = parent.top + parent.height - offset  (top follows unless also anchored to Top)
//  - not anchored:        top = parent.top + y
//  - size = max(height, minHeight); percentages refer to the parent's size
//  - the result is clipped to the parent; a panel clipped to nothing is not shown
class Panel {
public:
	Panel() = default;
	virtual ~Panel() = default;

	Panel(const Panel&) = delete;
	Panel& operator=(const Panel&) = delete;
	Panel(Panel&&) = delete;
	Panel& operator=(Panel&&) = delete;

	/* Tree */
	template <std::derived_from<Panel> T = Panel, typename... Args> T& add(Args&&... args) {
		auto child = std::make_unique<T>(std::forward<Args>(args)...);
		T& ref = *child;
		adopt(std::move(child));
		return ref;
	}

	Panel& addChild(std::unique_ptr<Panel> child); // takes ownership, e.g. of a panel made by a factory

	[[nodiscard]] Panel* parent() const noexcept { return parent_; }
	[[nodiscard]] Window* window() const noexcept { return window_; }
	[[nodiscard]] std::span<const std::unique_ptr<Panel>> children() const noexcept { return children_; }

	/* Identity: lets code find panels built elsewhere (e.g. from a screen file) */
	void setId(std::string id) { id_ = std::move(id); }
	[[nodiscard]] const std::string& id() const noexcept { return id_; }
	[[nodiscard]] Panel* findById(std::string_view id) noexcept; // this panel or the first matching descendant

	/* Position */
	void setPosition(Length x, Length y);
	void setX(Length x);
	void setY(Length y);
	[[nodiscard]] Length x() const noexcept { return x_; }
	[[nodiscard]] Length y() const noexcept { return y_; }

	/* Size */
	void setSize(Length width, Length height);
	void setWidth(Length width);
	void setHeight(Length height);
	void setMinSize(Length minWidth, Length minHeight);
	void setMinWidth(Length minWidth);
	void setMinHeight(Length minHeight);
	[[nodiscard]] Length width() const noexcept { return width_; }
	[[nodiscard]] Length height() const noexcept { return height_; }
	[[nodiscard]] Length minWidth() const noexcept { return minWidth_; }
	[[nodiscard]] Length minHeight() const noexcept { return minHeight_; }

	/* Anchors: keep an edge at a fixed distance from the same edge of the parent */
	void setAnchor(Edge edge, Length offset);
	void clearAnchor(Edge edge);
	[[nodiscard]] std::optional<Length> anchor(Edge edge) const noexcept;

	/* Appearance */
	void setColor(Color color);
	[[nodiscard]] Color color() const noexcept { return color_; }

	void setVisible(bool visible);

	// Cursor shape while the pointer is over this panel (or while it holds the mouse during a drag)
	void setCursor(Cursor cursor);
	[[nodiscard]] Cursor cursor() const noexcept { return cursor_; }
	[[nodiscard]] bool isVisible() const noexcept { return visible_; } // the flag set by setVisible
	[[nodiscard]] bool isShown() const noexcept {
		return shown_;
	} // visible, with visible ancestors and a non-empty area

	/* Layout results, valid after the last layout pass */
	[[nodiscard]] const Rect& bounds() const noexcept { return bounds_; } // absolute, clipped to the ancestors
	[[nodiscard]] const Rect& frame() const noexcept { return frame_; }   // absolute, before clipping
	[[nodiscard]] Vec2 resolvedSize() const noexcept { return size_; }    // size before clipping
	[[nodiscard]] Vec2 relativePosition() const noexcept;                 // top-left corner relative to the parent

	// Height this panel takes in a layout pass, against a parent of height `reference`. Used by containers (Stack).
	[[nodiscard]] float measuredHeight(float reference);

	// Deepest shown panel (this or a descendant) containing `point`; later siblings are on top
	[[nodiscard]] Panel* hitTest(Vec2 point) noexcept;

	[[nodiscard]] bool hasFocus() const noexcept;

	/* Signals */
	Signal<const KeyEvent&> onKey;
	Signal<const CharEvent&> onChar;
	Signal<const MouseMoveEvent&> onMouseMove;
	Signal<const MouseButtonEvent&> onMouseButton;
	Signal<const ScrollEvent&> onScroll;
	Signal<bool> onFocusChanged;
	Signal<bool> onHover; // the pointer entered (true) or left (false) this panel

protected:
	// Event hooks called by the Window. Widgets override them; call the base to keep the signals firing.
	virtual void keyEvent(const KeyEvent& event) { onKey(event); }
	virtual void charEvent(const CharEvent& event) { onChar(event); }
	virtual void mouseMoveEvent(const MouseMoveEvent& event) { onMouseMove(event); }
	virtual void mouseButtonEvent(const MouseButtonEvent& event) { onMouseButton(event); }
	virtual void scrollEvent(const ScrollEvent& event) { onScroll(event); }
	virtual void focusEvent(bool focused) { onFocusChanged(focused); }
	virtual void hoverEvent(bool hovered) { onHover(hovered); }

	// Called at the start of each layout pass, before this panel's size is resolved: lets it size itself, e.g. to
	// its content. Property changes made here don't trigger another layout.
	virtual void prepareLayout() {}

	// Draws this panel (not its children, which are drawn after it, on top). Default: fills the bounds with color().
	virtual void paint(Painter& painter) const;

	// Tells the window that the layout or the drawn output changed
	void invalidate() noexcept;

private:
	friend class Window;
	friend class gl::QuadRenderer;

	// Computes bounds for this panel and its subtree; driven by the Window before hit-testing and drawing
	void layout(const Rect& parentBounds, Vec2 parentSize, bool parentShown);

	void adopt(std::unique_ptr<Panel> child);
	void attachTo(Window* window) noexcept;

	std::string id_;
	Panel* parent_ = nullptr;
	Window* window_ = nullptr;
	std::vector<std::unique_ptr<Panel>> children_;

	Length x_, y_;
	Length width_, height_;
	Length minWidth_, minHeight_;
	std::array<std::optional<Length>, 4> anchors_; // indexed by Edge
	Color color_ = colors::panel;
	bool visible_ = true;
	Cursor cursor_ = Cursor::Arrow;

	Rect frame_;
	Rect bounds_;
	Vec2 size_;
	bool shown_ = false;
};

} // namespace sgl
