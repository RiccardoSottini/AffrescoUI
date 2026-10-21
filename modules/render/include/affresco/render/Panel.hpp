#pragma once

/// @file
/// @brief Panel: a rectangle in the UI tree, with layout, input events and drawing.

#include "affresco/render/Color.hpp"
#include "affresco/render/Events.hpp"
#include "affresco/render/Geometry.hpp"
#include "affresco/render/Layout.hpp"
#include "affresco/render/Length.hpp"
#include "affresco/render/Painter.hpp"
#include "affresco/core/Signal.hpp"

#include <array>
#include <cstdint>
#include <concepts>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace affresco {

class Window;

namespace gl {
class QuadRenderer;
}

namespace detail {
struct PanelGeometry;
class GeometryCache;
} // namespace detail

/// @brief An edge of a panel, for anchors.
enum class Edge {
	Top,    ///< Top edge.
	Bottom, ///< Bottom edge.
	Left,   ///< Left edge.
	Right,  ///< Right edge.
};

/// @brief Where an overlay panel is placed (see Panel::setOverlay).
enum class Placement {
	Below,    ///< Under its anchor, left edges aligned.
	BelowEnd, ///< Under its anchor, right edges aligned.
	Above,    ///< Over its anchor, left edges aligned.
	AboveEnd, ///< Over its anchor, right edges aligned.
	Right,    ///< To the right of its anchor, top edges aligned.
	Left,     ///< To the left of its anchor, top edges aligned.
	Center,   ///< Centred in the window.
	Window,   ///< In the window, placed by its own x, y and anchors, as if the window were its parent.
};

/// @brief A rectangle in the UI tree. Owns its children; positions and sizes are relative to the parent.
///
/// Layout rules, per axis (vertical shown):
/// - anchored to Top: `top = parent.top + offset`
/// - anchored to Bottom: `bottom = parent.top + parent.height - offset` (with Top too, the panel stretches)
/// - anchored to another panel's edge: `top = other.edge + offset` (the other panel must be laid out earlier)
/// - not anchored: `top = parent.top + y`
/// - `size = max(height, minHeight)`; percentages refer to the parent's size
/// - the result is clipped to the parent; a panel clipped to nothing is not shown
///
/// A scrollable panel shifts its children by its scroll offset. An overlay panel is laid out, drawn and hit-tested
/// after the whole tree, unclipped, next to its anchor.
///
/// Setters only mark the window dirty: layout runs once, before the next frame. Subclasses draw in paint(), react in
/// the event hooks, size themselves in prepareLayout() and place their children in arrange().
class Panel {
public:
	/// @brief An empty panel: no size, anchored nowhere, the default colour.
	Panel();
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

	/// @brief Moves a child to another position, keeping the panel (and its focus, hover and scroll).
	/// @param from Its position.
	/// @param to The new position.
	/// @throws std::out_of_range If either position is past the end.
	void moveChild(std::size_t from, std::size_t to);

	/// @brief The parent, or nullptr for a root.
	[[nodiscard]] Panel* parent() const noexcept { return parent_; }
	/// @brief The window, or nullptr while not attached to one.
	[[nodiscard]] Window* window() const noexcept { return window_; }
	/// @brief The children, back to front.
	[[nodiscard]] std::span<const std::unique_ptr<Panel>> children() const noexcept { return children_; }

	/* Identity: lets code find panels built elsewhere (e.g. from a screen file) */

	/// @brief Sets the id.
	/// @param id Any string.
	void setId(std::string id);
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
	/// @brief Sets the maximum size; the minimum wins when they disagree, as in CSS.
	/// @param maxWidth The maximum width, or std::nullopt (the default) for none.
	/// @param maxHeight The maximum height, or std::nullopt for none.
	void setMaxSize(std::optional<Length> maxWidth, std::optional<Length> maxHeight);
	/// @brief Sets the maximum width.
	/// @param maxWidth The maximum, or std::nullopt for none.
	void setMaxWidth(std::optional<Length> maxWidth);
	/// @brief Sets the maximum height.
	/// @param maxHeight The maximum, or std::nullopt for none.
	void setMaxHeight(std::optional<Length> maxHeight);
	/// @brief Keeps the width and the height in a ratio: the height follows the width, unless only the height is set
	/// (a non-zero height, or stretched by anchors or a container), then the width follows the height. When both are
	/// set, the ratio is ignored, as in CSS.
	/// @param ratio Width divided by height; 0 (the default) for none.
	void setAspectRatio(float ratio);
	/// @brief The width.
	[[nodiscard]] Length width() const noexcept { return width_; }
	/// @brief The height.
	[[nodiscard]] Length height() const noexcept { return height_; }
	/// @brief The minimum width.
	[[nodiscard]] Length minWidth() const noexcept { return minWidth_; }
	/// @brief The minimum height.
	[[nodiscard]] Length minHeight() const noexcept { return minHeight_; }
	/// @brief The maximum width, if any.
	[[nodiscard]] std::optional<Length> maxWidth() const noexcept { return maxWidth_; }
	/// @brief The maximum height, if any.
	[[nodiscard]] std::optional<Length> maxHeight() const noexcept { return maxHeight_; }
	/// @brief Width divided by height, or 0 for none.
	[[nodiscard]] float aspectRatio() const noexcept { return aspectRatio_; }

	/* Anchors: keep an edge at a fixed distance from the same edge of the parent */

	/// @brief Anchors an edge.
	/// @param edge The edge.
	/// @param offset Distance from the parent's same edge.
	void setAnchor(Edge edge, Length offset);
	/// @brief Anchors an edge to an edge of another panel, found by id: `edge = other.targetEdge + offset`.
	/// @param edge The edge.
	/// @param id The other panel's id; it should be laid out before this one (e.g. an earlier sibling).
	/// @param targetEdge Its edge.
	/// @param offset Pixels added to that edge's position (negative moves up or left).
	void setAnchorTo(Edge edge, std::string id, Edge targetEdge, float offset);
	/// @brief Removes an anchor.
	/// @param edge The edge.
	void clearAnchor(Edge edge);
	/// @brief An edge's anchor.
	/// @param edge The edge.
	/// @return Its offset, or std::nullopt if not anchored (an anchor to another panel reports 0).
	[[nodiscard]] std::optional<Length> anchor(Edge edge) const noexcept;

	/// @brief Sets how much of a container's spare room this panel takes (Stack and Grid), like CSS flex-grow.
	/// @param grow 0 (the default) keeps its own size; otherwise the share of the room left over.
	void setGrow(float grow);
	/// @brief The share of spare room it takes in a Stack.
	[[nodiscard]] float grow() const noexcept { return grow_; }
	/// @brief Sets how much this panel gives up when the children of a row or a column don't fit, like CSS
	/// flex-shrink: the overflow is taken in proportion to shrink times size, down to the minimum size.
	/// @param shrink 0 (the default) keeps its size, so children overflow as they always have.
	void setShrink(float shrink);
	/// @brief How much it gives up when its row or column overflows.
	[[nodiscard]] float shrink() const noexcept { return shrink_; }
	/// @brief Sets its size along a row or a column before growing or shrinking, like CSS flex-basis.
	/// @param basis The size (percentages of the container), or std::nullopt (the default) for its own width or height.
	void setBasis(std::optional<Length> basis);
	/// @brief Its size along a row or a column before flexing, if set.
	[[nodiscard]] std::optional<Length> basis() const noexcept { return basis_; }
	/// @brief Sets where this panel sits across its row or column, over the container's choice (CSS align-self).
	/// @param align The place, or std::nullopt (the default) for the container's.
	void setAlignSelf(std::optional<CrossAlign> align);
	/// @brief Where it sits across its row or column, if it chose.
	[[nodiscard]] std::optional<CrossAlign> alignSelf() const noexcept { return alignSelf_; }
	/// @brief Sets the cell it takes in a Grid.
	/// @param cell Its column, row and spans; the default lets the grid place it in the next free cell.
	void setGridCell(GridCell cell);
	/// @brief The cell it asks for in a Grid.
	[[nodiscard]] const GridCell& gridCell() const noexcept { return gridCell_; }

	/* Scrolling */

	/// @brief Lets the wheel or trackpad scroll the children (clipped to this panel).
	/// @param horizontal Along x.
	/// @param vertical Along y.
	void setScrollable(bool horizontal, bool vertical);
	/// @brief Whether it scrolls along either axis.
	[[nodiscard]] bool isScrollable() const noexcept { return scrollX_ || scrollY_; }
	/// @brief How far the children are scrolled, in pixels (positive: content moved up or left).
	[[nodiscard]] Vec2 scrollOffset() const noexcept { return scroll_; }
	/// @brief Scrolls by an amount, within the content.
	/// @param delta Pixels; positive scrolls towards the end.
	/// @return true if the offset changed.
	bool scrollBy(Vec2 delta);
	/// @brief Scrolls every scrollable ancestor so this panel's frame is inside their visible area.
	void scrollIntoView();

	/* Overlays: menus, popovers, dialogs */

	/// @brief Makes this panel an overlay: drawn above everything, unclipped, placed next to an anchor panel.
	/// @param anchorId The anchor's id; empty for the parent.
	/// @param placement Where it goes.
	void setOverlay(std::string anchorId, Placement placement);
	/// @brief Makes it an ordinary panel again.
	void clearOverlay();
	/// @brief Whether it is an overlay.
	[[nodiscard]] bool isOverlay() const noexcept { return overlay_; }

	/// @brief Sets the text of the tooltip the window shows after the pointer rests on this panel.
	/// @param text UTF-8; empty for none.
	void setTooltip(std::string text);
	/// @brief The tooltip text; empty if none.
	[[nodiscard]] const std::string& tooltip() const noexcept { return tooltip_; }

	/* Appearance */

	/// @brief Sets the background colour that paint() fills with.
	/// @param color The colour.
	void setColor(Color color);
	/// @brief The background colour.
	[[nodiscard]] Color color() const noexcept { return color_; }

	/// @brief Rounds the corners of the background, the border and the shadow. Children are still clipped to the
	/// square bounds, and hit-testing ignores the corners.
	/// @param radii Each corner's radius, in pixels; all zero (the default) for square corners.
	void setCornerRadii(CornerRadii radii);
	/// @brief The corner radii.
	[[nodiscard]] const CornerRadii& cornerRadii() const noexcept { return radii_; }

	/// @brief Sets the border, drawn inside the frame over the background and under the children. It doesn't change
	/// the layout.
	/// @param width How thick it is, in pixels; 0 (the default) for none.
	/// @param color Its colour.
	void setBorder(float width, Color color);
	/// @brief How thick the border is; 0 if there is none.
	[[nodiscard]] float borderWidth() const noexcept { return borderWidth_; }
	/// @brief The border's colour.
	[[nodiscard]] Color borderColor() const noexcept { return borderColor_; }

	/// @brief Sets the shadow the panel casts: drawn under it, and clipped to its parent rather than to the panel.
	/// @param shadow The shadow; a transparent one (the default) draws nothing.
	void setShadow(const Shadow& shadow);
	/// @brief The shadow.
	[[nodiscard]] const Shadow& shadow() const noexcept { return shadow_; }
	/// @brief Keeps the subtree's drawing in a texture, drawn as one quad until something in it changes.
	/// @param cached true to cache it (worth it for a large subtree that rarely changes).
	void setCached(bool cached) { setLook(cached_, cached); }
	/// @brief The flag set by setCached().
	[[nodiscard]] bool isCached() const noexcept { return cached_; }

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
	[[nodiscard]] bool isShown() const noexcept { return shown_; }

	/* Layout results, valid after the last layout pass */

	/// @brief The visible area: absolute, clipped to the ancestors.
	[[nodiscard]] const Rect& bounds() const noexcept { return bounds_; }
	/// @brief The whole area: absolute, before clipping.
	[[nodiscard]] const Rect& frame() const noexcept { return frame_; }
	/// @brief The size before clipping.
	[[nodiscard]] Vec2 resolvedSize() const noexcept { return size_; }
	/// @brief The top-left corner, relative to the parent's.
	[[nodiscard]] Vec2 relativePosition() const noexcept;

	/// @brief Measures the size this panel takes in a layout pass, for containers such as Stack: its own size within
	/// its minimum and maximum, with its aspect ratio, after prepareLayout().
	/// @param reference The parent's size, for percentages.
	/// @return The size.
	[[nodiscard]] Vec2 measuredSize(Vec2 reference);
	/// @brief Measures the height this panel takes in a layout pass (measuredSize(), the parent's width as the other
	/// reference).
	/// @param reference The parent's height, for percentages.
	/// @return The height.
	[[nodiscard]] float measuredHeight(float reference);
	/// @brief Measures the width this panel takes in a layout pass (measuredSize(), the parent's height as the other
	/// reference).
	/// @param reference The parent's width, for percentages.
	/// @return The width.
	[[nodiscard]] float measuredWidth(float reference);

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
	Signal<bool> onHover; ///< Pointer entered (true) or left (false) this panel or one of its descendants.

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
	/// @brief Scrolls by a wheel or trackpad amount, for the window: the nearest panel under the pointer that scrolls
	/// takes it, its ancestors don't. The default scrolls the children of a scrollable panel; widgets that scroll
	/// what they draw themselves (lists, tab strips) override it.
	/// @param delta Pixels; positive scrolls towards the end.
	/// @return true if it scrolled.
	virtual bool scrollContent(Vec2 delta) { return isScrollable() && scrollBy(delta); }
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

	/// @brief Called once this panel's frame is known, before its children are laid out, so containers can place
	/// them (with setAnchor() and arrangeSize()). Property changes made here don't trigger another layout.
	virtual void arrange() {}

	/// @brief Draws this panel; the children are drawn after it, on top. The default draws the shadow, fills bounds()
	/// with color() (rounding the corners, if any), then draws the border.
	/// @param painter Clips to bounds().
	virtual void paint(Painter& painter) const;

	/// @brief Draws over the children (the default draws the scroll bar of a scrollable panel).
	/// @param painter Clips to bounds().
	virtual void paintOver(Painter& painter) const;

	/// @brief Tells the window that this panel's layout or drawing changed: the next layout pass lays it out again
	/// (with its ancestors and whatever they place differently), and the next frame draws it again.
	///
	/// Call it whenever something that prepareLayout(), arrange() or paint() reads changes, even something the panel
	/// doesn't own (its measured size is cached until then).
	void invalidate() noexcept;

	/// @brief Tells the window that only this panel's drawing changed (a colour, a caret): the next frame draws it
	/// again, and no layout pass runs for it. When its size or arrangement may change too, call invalidate() instead.
	void invalidatePaint() noexcept;

	/// @brief Whether invalidate() was called since this panel was last laid out: what prepareLayout() keeps from
	/// earlier passes may be out of date only then (or after Window::invalidate()).
	[[nodiscard]] bool isLayoutDirty() const noexcept { return layoutDirty_; }

	/// @brief Stores a property's new value and calls invalidate(), unless the value is the one already there.
	/// @tparam T The property's type; it must have ==.
	/// @param member The property.
	/// @param value Its new value.
	template <typename T> void setProperty(T& member, std::type_identity_t<T> value) {
		if(member == value) return;
		member = std::move(value);
		invalidate();
	}

	/// @brief Stores the new value of a property that only changes the drawing and calls invalidatePaint(), unless the
	/// value is the one already there.
	/// @tparam T The property's type; it must have ==.
	/// @param member The property.
	/// @param value Its new value.
	template <typename T> void setLook(T& member, std::type_identity_t<T> value) {
		if(member == value) return;
		member = std::move(value);
		invalidatePaint();
	}

	/// @brief Gives a child the size a container decided, for this layout pass, instead of its own.
	/// @param child The child.
	/// @param width Its width, or std::nullopt to keep its own.
	/// @param height Its height, or std::nullopt to keep its own.
	static void arrangeSize(Panel& child, std::optional<float> width, std::optional<float> height) noexcept;

private:
	friend class Window;
	friend class gl::QuadRenderer;
	friend class detail::GeometryCache;

	/// @brief Computes frame, bounds and visibility for this panel and its subtree.
	/// @param origin The parent's frame, moved by its scroll offset: what positions and anchors are measured from.
	/// @param clip The parent's visible area, which clips this panel.
	/// @param parentSize The parent's unclipped size, for percentages and bottom/right anchors.
	/// @param parentShown Whether the parent is shown.
	void layout(const Rect& origin, const Rect& clip, Vec2 parentSize, bool parentShown);
	/// @brief Computes frame, bounds and visibility from the inputs of the pass (layout()'s arguments).
	/// @param origin See layout().
	/// @param clip See layout().
	/// @param parentSize See layout().
	/// @param parentShown See layout().
	void placeFrame(const Rect& origin, const Rect& clip, Vec2 parentSize, bool parentShown);
	/// @brief The positions of a panel's two edges on one axis, if anchored.
	struct AnchoredEdges {
		std::optional<float> start{}; ///< Top or left.
		std::optional<float> end{};   ///< Bottom or right.
	};
	/// @brief Where a panel's two edges go on one axis.
	/// @param edges Its anchored edges there.
	/// @param position Where its start goes when neither is anchored.
	/// @param size Its size there.
	/// @param arranged Whether a container gave it that size (which then wins over the far anchor).
	/// @return Its start and end.
	[[nodiscard]] static std::pair<float, float> alongAxis(AnchoredEdges edges, float position, float size,
	                                                       bool arranged) noexcept;
	/// @brief Makes the size follow the aspect ratio, from the axis that is set (stretched axes count as set).
	/// @param horizontal The anchored left and right edges.
	/// @param vertical The anchored top and bottom edges.
	/// @param parentSize See layout().
	void followAspectRatio(AnchoredEdges horizontal, AnchoredEdges vertical, Vec2 parentSize) noexcept;
	/// @brief Tells the window that panels came, went, moved or changed id: anchors to other panels find them again.
	void treeChanged() noexcept;
	/// @brief Marks this panel as needing layout, and its ancestors as having a descendant that does.
	void markLayoutDirty() noexcept;
	/// @brief Marks this panel's drawing as out of date, and its ancestors as having a descendant whose is.
	void markPaintDirty() noexcept;
	/// @brief Applies the aspect ratio to a size: the axis that is set leads, within the other's bounds.
	/// @param size The size, within its bounds.
	/// @param widthSet Whether the width is set (non-zero, stretched or given by a container).
	/// @param heightSet Whether the height is set.
	/// @param reference The parent's size, for percentages.
	/// @return The size; unchanged without a ratio, or with both or neither axis set.
	[[nodiscard]] Vec2 withAspectRatio(Vec2 size, bool widthSet, bool heightSet, Vec2 reference) const noexcept;
	/// @brief Lays out the children (not overlays) inside this panel's frame, moved by the scroll offset.
	void layoutChildren();
	/// @brief The absolute position of an anchored edge.
	/// @param edge The edge.
	/// @param origin See layout().
	/// @param parentSize See layout().
	/// @return The position, or std::nullopt if the edge isn't anchored (or its other panel isn't there).
	[[nodiscard]] std::optional<float> anchoredEdge(Edge edge, const Rect& origin, Vec2 parentSize) const;
	/// @brief Whether this panel's frame is final for a layout pass.
	/// @param pass The pass.
	/// @return true if the pass laid it out already, or skipped it or an ancestor.
	[[nodiscard]] bool isSettled(std::uint64_t pass) const noexcept;
	/// @brief The position of an edge anchored to another panel's edge.
	/// @param edge The edge; its bit in targetEdges_ is set.
	/// @return The position, or std::nullopt if the other panel isn't there.
	[[nodiscard]] std::optional<float> targetEdge(Edge edge) const;

	/// @brief An anchor to another panel's edge.
	struct AnchorTarget {
		std::string id; ///< The other panel.
		Edge edge;      ///< Its edge.
		float offset;   ///< Added to that edge's position.
		/// The panel @ref id found, kept while the window's tree generation is @ref found's (no panel came or went) and
		/// it still has that id: the tree isn't searched on every pass. Never dereferenced once the generation moved on.
		mutable const Panel* other = nullptr;
		mutable std::uint64_t found = 0; ///< The tree generation @ref other was found in; 0: not yet.

		/// @brief Equal when they name the same edge of the same panel, at the same offset.
		/// @param that The other target.
		/// @return true if so.
		bool operator==(const AnchorTarget& that) const noexcept {
			return id == that.id && edge == that.edge && offset == that.offset;
		}
	};

	/// @brief What a layout pass gave this panel. The next pass skips the panel and its subtree when they are the
	/// same, nothing in the subtree changed, and nothing in it is anchored to another panel.
	struct LayoutInputs {
		Rect origin;                         ///< See layout().
		Rect clip;                           ///< See layout().
		Vec2 parentSize;                     ///< See layout().
		std::optional<float> arrangedWidth;  ///< The width its container gave it.
		std::optional<float> arrangedHeight; ///< The height its container gave it.
		bool parentShown = false;            ///< See layout().

		/// @brief Equal when every member is equal.
		bool operator==(const LayoutInputs&) const = default;
	};

	/// @brief A size measuredSize() computed, kept until the panel changes.
	struct MeasuredSize {
		Vec2 reference;               ///< What its percentages referred to.
		Vec2 size;                    ///< The size.
		std::uint64_t generation = 0; ///< The window's measure generation (Window::invalidate() starts a new one).
	};

	/// @brief Whether a layout pass can leave this panel and its subtree as they are.
	/// @param origin See layout().
	/// @param clip See layout().
	/// @param parentSize See layout().
	/// @param parentShown See layout().
	/// @return true if nothing they depend on changed.
	[[nodiscard]] bool canSkipLayout(const Rect& origin, const Rect& clip, Vec2 parentSize,
	                                 bool parentShown) const noexcept;

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

	Length x_, y_;                                       ///< Position on unanchored axes.
	Length width_, height_;                              ///< Size.
	Length minWidth_, minHeight_;                        ///< Minimum size.
	std::optional<Length> maxWidth_, maxHeight_;         ///< Maximum size, if any.
	float aspectRatio_ = 0.0f;                           ///< See aspectRatio().
	float shrink_ = 0.0f;                                ///< See shrink().
	std::optional<Length> basis_;                        ///< See basis().
	std::optional<CrossAlign> alignSelf_;                ///< See alignSelf().
	GridCell gridCell_;                                  ///< See gridCell().
	std::array<std::optional<Length>, 4> anchors_;       ///< Indexed by Edge.
	std::array<std::optional<AnchorTarget>, 4> targets_; ///< Anchors to other panels, indexed by Edge.
	std::uint8_t targetEdges_ = 0; ///< A bit per edge (1 << Edge) whose targets_ is set, read on every pass.
	float grow_ = 0.0f;            ///< See grow().
	std::optional<float> arrangedWidth_, arrangedHeight_; ///< Set by a container for this pass (arrangeSize()).
	Color color_ = colors::panel;                         ///< Background.
	CornerRadii radii_;                                   ///< See cornerRadii().
	float borderWidth_ = 0.0f;                            ///< See borderWidth().
	Color borderColor_ = colors::black;                   ///< See borderColor().
	Shadow shadow_;                                       ///< See shadow().
	bool visible_ = true;                                 ///< See setVisible().
	bool cached_ = false;                                 ///< See setCached().
	Cursor cursor_ = Cursor::Arrow;                       ///< See setCursor().
	std::string tooltip_;                                 ///< See tooltip().

	bool scrollX_ = false, scrollY_ = false; ///< See setScrollable().
	Vec2 scroll_;                            ///< See scrollOffset().
	Vec2 contentMin_, contentMax_;           ///< The children's extent, unscrolled, relative to the frame.

	bool overlay_ = false;                   ///< See isOverlay().
	std::string overlayAnchor_;              ///< See setOverlay().
	Placement placement_ = Placement::Below; ///< See setOverlay().

	std::uint64_t layoutPass_ = 0;         ///< The window's layout pass this panel was last laid out in.
	std::uint64_t skippedPass_ = 0;        ///< The window's layout pass that last skipped this panel and its subtree.
	LayoutInputs laidOut_;                 ///< What the last pass gave it, once laid out (see canSkipLayout()).
	std::optional<MeasuredSize> measured_; ///< See measuredSize(); nullopt once something changed.
	bool layoutDirty_ = true;              ///< Its own properties changed since it was last laid out.
	bool layoutBelow_ = false;             ///< A descendant's layoutDirty_ is set.
	bool anchorsBelow_ = false;            ///< It or a descendant is anchored to another panel: never skipped.
	bool overlaysBelow_ = false;           ///< A descendant is an overlay, for Window::layoutOverlays().
	Rect frame_;                           ///< See frame().
	Rect bounds_;                          ///< See bounds().
	Vec2 size_;                            ///< See resolvedSize().
	bool shown_ = false;                   ///< See isShown().

	bool paintStale_ = true;                          ///< Its drawing changed since the renderer last recorded it.
	bool paintBelow_ = false;                         ///< A descendant's paintStale_ is set.
	std::unique_ptr<detail::PanelGeometry> geometry_; ///< What it drew last, kept by the renderer; null until drawn.
};

} // namespace affresco
