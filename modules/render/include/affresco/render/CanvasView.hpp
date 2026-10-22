#pragma once

/// @file
/// @brief CanvasView: an infinite canvas. Children and model items are placed in world units and seen through a
/// Camera2D; the view pans, zooms, selects, moves, resizes and rotates, snaps, and shows rulers and guides. It never
/// changes the items itself: it reports transforms, and whoever owns the items applies them.

#include "affresco/render/Camera2D.hpp"
#include "affresco/render/Color.hpp"
#include "affresco/render/Events.hpp"
#include "affresco/render/Panel.hpp"

#include "affresco/core/Signal.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace affresco {

class Font;

/// @brief One item a canvas draws itself, from a model: a box with an optional line of text.
struct CanvasItem {
	std::string id{};                   ///< Unique within the canvas; what selection and transforms name.
	Rect bounds{};                      ///< Where it is, in world units.
	float rotate = 0.0f;                ///< Degrees clockwise about its centre (kept and reported; not yet drawn).
	Color color = Color::hex(0xffffff); ///< The fill.
	Color borderColor = Color::hex(0xd1d5db); ///< The outline.
	float borderWidth = 1.0f;                 ///< The outline's width, in world units (0 for none).
	float radius = 0.0f;                      ///< The corners' radius, in world units.
	std::u32string text{};                    ///< One line of text at its top-left; empty for none.
	float textSize = 14.0f;                   ///< The text's height, in world units.
	Color textColor = Color::hex(0x111827);   ///< The text's colour.
};

/// @brief Where a canvas reads the items it draws itself: implement it over the host's own data, or use
/// CanvasItemList. After the items change, call CanvasView::modelChanged().
class CanvasItemModel {
public:
	virtual ~CanvasItemModel() = default;
	/// @brief The number of items.
	[[nodiscard]] virtual std::size_t size() const noexcept = 0;
	/// @brief One item, in drawing order (later ones on top).
	/// @param index Below size().
	/// @return The item.
	[[nodiscard]] virtual const CanvasItem& item(std::size_t index) const noexcept = 0;
};

/// @brief A CanvasItemModel over a vector it owns.
class CanvasItemList final : public CanvasItemModel {
public:
	/// @brief Takes the items.
	/// @param items The items, in drawing order.
	explicit CanvasItemList(std::vector<CanvasItem> items) noexcept : items_(std::move(items)) {}
	/// @copydoc CanvasItemModel::size
	[[nodiscard]] std::size_t size() const noexcept override { return items_.size(); }
	/// @copydoc CanvasItemModel::item
	[[nodiscard]] const CanvasItem& item(std::size_t index) const noexcept override { return items_[index]; }

private:
	std::vector<CanvasItem> items_; ///< The items.
};

/// @brief The guides of a canvas: lines a person dragged out of the rulers, that items snap to.
struct CanvasGuides {
	std::vector<float> vertical{};   ///< Vertical guides: their world x.
	std::vector<float> horizontal{}; ///< Horizontal guides: their world y.

	/// @brief Equal when every member is equal.
	bool operator==(const CanvasGuides&) const = default;
};

/// @brief A transform a canvas asks for: where an item should go.
struct CanvasTransform {
	std::string id{};    ///< The item.
	Rect bounds{};       ///< Its new rectangle, in world units.
	float rotate = 0.0f; ///< Its new rotation, in degrees.
};

/// @brief A level of detail of a child: below a zoom, another child (a lighter one) stands in for it.
struct CanvasLevel {
	float below = 0.0f; ///< The zoom under which it applies.
	std::string use{};  ///< The id of the sibling drawn in its place.
};

/// @brief An infinite canvas.
///
/// Children are placed at their own x, y, width and height, read as world units (pixels only) and mapped through
/// the camera; until panels can be transformed, their text keeps its size while boxes, images and paths scale (all
/// in placeChildren(), the one function to switch to a transform). Model items are drawn by the canvas itself, only
/// those in view (a spatial index), with their text scaled through setTextFonts() and drawn as a bar when too small
/// to read. Pointer: drag the background or a ruler, or with Space or the middle button to pan; scroll to pan;
/// Ctrl or Cmd with the wheel zooms at the pointer; click, Shift-click and a marquee select; the handles move,
/// resize and rotate. Keys while it (or one of its children) has the focus: + and - zoom at the pointer, 0 resets
/// the zoom, Shift+1 fits the content, Shift+2 the selection, arrows nudge (Shift: 10), Escape clears the selection.
///
/// Accessibility: children keep their roles and names; the selection is announced in the canvas's accessible
/// description ("3 selected").
///
/// Lifetime: the canvas listens to its window's input; it stops in its destructor, so it must not outlive the
/// window it was shown in (panels are owned by their window's tree, so this holds unless one is taken out).
class CanvasView : public Panel {
public:
	static constexpr std::size_t maximumItems = 1'000'000;  ///< Model items indexed and drawn at most.
	static constexpr std::size_t maximumGuides = 256;       ///< Guides along each axis, at most.
	static constexpr std::size_t maximumSelection = 10'000; ///< Items selected at once, at most.
	static constexpr float handleSize = 8.0f;               ///< The selection handles' size, in screen units.

	/// @brief An empty canvas at zoom 1, with rulers and snapping on.
	CanvasView();
	/// @brief Stops listening to the window.
	~CanvasView() override;
	CanvasView(const CanvasView&) = delete;            ///< Not copyable.
	CanvasView& operator=(const CanvasView&) = delete; ///< Not copyable.

	/* Content */

	/// @brief Sets the items the canvas draws itself (beside its children), and indexes them.
	/// @param model The items; nullptr for none. Only the first maximumItems are used.
	void setModel(std::shared_ptr<const CanvasItemModel> model);
	/// @brief The model.
	[[nodiscard]] const std::shared_ptr<const CanvasItemModel>& model() const noexcept { return model_; }
	/// @brief Indexes the model's items again, after they changed.
	void modelChanged();
	/// @brief Sets a child's levels of detail, by the child's id.
	/// @param childId The child; levels of a child the canvas doesn't have are kept for when it does.
	/// @param levels Its levels; empty removes them.
	void setLevels(const std::string& childId, std::vector<CanvasLevel> levels);
	/// @brief The levels of detail, by child id.
	[[nodiscard]] const std::vector<std::pair<std::string, std::vector<CanvasLevel>>>& levels() const noexcept {
		return levels_;
	}

	/* View state: what a person changes (each change emits onViewChanged) */

	/// @brief Sets the camera; the zoom is clamped. Doesn't emit onViewChanged.
	/// @param camera The camera.
	void setCamera(Camera2D camera);
	/// @brief The camera.
	[[nodiscard]] const Camera2D& camera() const noexcept { return camera_; }
	/// @brief Sets the selected items, by id. Doesn't emit onViewChanged.
	/// @param ids The ids; unknown ones are kept, as the items may come later.
	void setSelection(std::vector<std::string> ids);
	/// @brief The selected items' ids, sorted.
	[[nodiscard]] const std::vector<std::string>& selection() const noexcept { return selection_; }
	/// @brief Sets the guides; each axis keeps maximumGuides at most. Doesn't emit onViewChanged.
	/// @param guides The guides.
	void setGuides(CanvasGuides guides);
	/// @brief The guides.
	[[nodiscard]] const CanvasGuides& guides() const noexcept { return guides_; }
	/// @brief Moves the camera to show the content (or the selection) whole, and emits onViewChanged.
	/// @param selectionOnly Fit the selection instead of everything; with nothing selected, everything.
	void fit(bool selectionOnly);
	/// @brief The world rectangle the items (or the selected ones) cover.
	/// @param selectionOnly Only the selected items.
	/// @return The rectangle, or std::nullopt if there are none.
	[[nodiscard]] std::optional<Rect> contentBounds(bool selectionOnly) const;

	/* Options */

	/// @brief Sets the grid: drawn when its lines are at least 8 screen units apart, and snapped to.
	/// @param spacing World units between lines; 0 for none.
	void setGrid(float spacing);
	/// @brief The grid's spacing.
	[[nodiscard]] float grid() const noexcept { return grid_; }
	/// @brief Turns snapping to the grid, items and guides on or off.
	/// @param snap Whether moves and resizes snap.
	void setSnap(bool snap);
	/// @brief Shows or hides the rulers (and with them, new guides).
	/// @param rulers Whether to show them.
	void setRulers(bool rulers);
	/// @brief Sets the fonts the model items' text is drawn with: the canvas picks the largest no taller than the
	/// text on screen, so text scales in steps.
	/// @param fonts The fonts, any order; empty uses the window's default font at its own size.
	void setTextFonts(std::vector<std::shared_ptr<const Font>> fonts);
	/// @brief Sets the colour of the selection, the marquee, the handles and the guides.
	/// @param color The colour.
	void setAccentColor(Color color);

	/* Queries */

	/// @brief The model items in a world rectangle, edges included.
	/// @param area The rectangle.
	/// @param found Cleared, then filled with their indices in drawing order.
	void itemsIn(Rect area, std::vector<std::uint32_t>& found) const;
	/// @brief The model items drawn by the last frame.
	[[nodiscard]] std::size_t drawnItems() const noexcept { return drawn_; }
	/// @brief The world point under a window point.
	/// @param point In window units.
	/// @return In world units.
	[[nodiscard]] Vec2 worldAt(Vec2 point) const noexcept;

	Signal<> onViewChanged; ///< A person changed the camera, the selection or the guides.
	Signal<const std::vector<CanvasTransform>&> onTransform; ///< A person moved, resized, rotated or nudged items.

protected:
	/// @brief Places the children at their camera-mapped rectangles, and starts listening to the window.
	void arrange() override;
	/// @brief Draws the background, the grid and the model items in view.
	/// @param painter The painter.
	void paint(Painter& painter) const override;
	/// @brief Draws the selection, its handles, the marquee, the guides and the rulers over the children.
	/// @param painter The painter.
	void paintOver(Painter& painter) const override;
	/// @brief Pans by a wheel or trackpad amount, or zooms at the pointer while Ctrl or Cmd is held.
	/// @param delta Pixels; positive scrolls towards the end.
	/// @return true.
	bool scrollContent(Vec2 delta) override;

private:
	struct Internals;
	/// @brief What a press grabbed.
	enum class Gesture { none, pan, marquee, move, resize, rotate, guide };

	/* CanvasView.cpp: content, layout and painting */

	/// @brief Gives each child its screen rectangle, or hides it for a level of detail. The one place children are
	/// mapped through the camera: once panels take a transform, it sets one instead of scaling their rectangles.
	void placeChildren();
	/// @brief The child that stands in for another at the current zoom, if any.
	/// @param child The child.
	/// @return The stand-in, or nullptr.
	[[nodiscard]] Panel* standIn(const Panel& child) const noexcept;
	/// @brief A child's world rectangle (its own x, y, width and height).
	/// @param child The child.
	/// @return The rectangle.
	[[nodiscard]] static Rect worldBoundsOf(const Panel& child) noexcept;
	/// @brief An item's rectangle as drawn: moved or resized while a drag previews it.
	/// @param id The item.
	/// @param bounds Its world rectangle.
	/// @return The rectangle to draw.
	[[nodiscard]] Rect previewed(const std::string& id, const Rect& bounds) const noexcept;
	/// @brief Whether an item is selected.
	/// @param id The item.
	[[nodiscard]] bool isSelected(const std::string& id) const noexcept;
	/// @brief Draws the grid's lines.
	/// @param painter The painter.
	void paintGrid(Painter& painter) const;
	/// @brief Draws one model item.
	/// @param painter The painter.
	/// @param item The item.
	void paintItem(Painter& painter, const CanvasItem& item) const;
	/// @brief Draws an item's text, or a bar where it would be too small to read.
	/// @param painter The painter.
	/// @param item The item.
	/// @param box Its rectangle on screen.
	void paintText(Painter& painter, const CanvasItem& item, const Rect& box) const;
	/// @brief Draws the selection's outline and handles.
	/// @param painter The painter.
	void paintSelection(Painter& painter) const;
	/// @brief Draws the guides and the smart guides.
	/// @param painter The painter.
	void paintGuides(Painter& painter) const;
	/// @brief A world rectangle on screen, in window units.
	/// @param world The rectangle.
	[[nodiscard]] Rect onScreen(const Rect& world) const noexcept;
	/// @brief The selected items (children and model items), as they are now.
	/// @param out Cleared, then filled.
	void selectedItems(std::vector<CanvasTransform>& out) const;
	/// @brief The selected items' world rectangles as drawn (with a drag's preview); allocates nothing once warm.
	/// @param out Cleared, then filled.
	void selectedRects(std::vector<Rect>& out) const;
	/// @brief A model item by id.
	/// @param id The id.
	/// @return The item, or nullptr.
	[[nodiscard]] const CanvasItem* modelItem(std::string_view id) const noexcept;
	/// @brief The world rectangle around some transforms.
	/// @param items The transforms.
	[[nodiscard]] static std::optional<Rect> boundsOf(const std::vector<CanvasTransform>& items) noexcept;

	/* CanvasViewInput.cpp: the pointer and the keys */

	/// @brief Connects to the window's input, once it has one.
	void listen();
	/// @brief Disconnects from the window's input.
	void stopListening() noexcept;
	/// @brief Handles a press or release that the window routed.
	/// @param event The event.
	void pressed(const MouseButtonEvent& event);
	/// @brief Handles a pointer move.
	/// @param event The event.
	void moved(const MouseMoveEvent& event);
	/// @brief Handles a key.
	/// @param event The event.
	void keyed(const KeyEvent& event);
	/// @brief Starts a gesture for a left press.
	/// @param position In window units.
	/// @param shift Whether Shift was held.
	void begin(Vec2 position, bool shift);
	/// @brief Starts moving, resizing or rotating the selection (handle_ tells which).
	void grabSelection();
	/// @brief Ends the gesture: commits a marquee, a guide or a transform.
	void finish();
	/// @brief Ends dragging a guide: keeps it, or takes it away when dropped on its ruler.
	void finishGuide();
	/// @brief Previews a move or resize for the pointer's position, with snapping.
	/// @param position In window units.
	void dragItems(Vec2 position);
	/// @brief The item under a window point: a child or a model item, topmost first.
	/// @param position In window units.
	/// @return Its id, or empty.
	[[nodiscard]] std::string itemAt(Vec2 position) const;
	/// @brief The selection handle under a window point: 0 to 7 clockwise from the top-left, 8 for rotation.
	/// @param position In window units.
	/// @return The handle, or -1.
	[[nodiscard]] int handleAt(Vec2 position) const noexcept;
	/// @brief Selects the items a marquee meets.
	/// @param area The marquee, in world units.
	/// @param add Add to the selection instead of replacing it.
	void selectIn(const Rect& area, bool add);
	/// @brief Zooms by a factor at a window point and emits onViewChanged.
	/// @param position In window units.
	/// @param factor The factor.
	void zoomBy(Vec2 position, float factor);
	/// @brief Moves the selection by a world distance and emits onTransform.
	/// @param dx World units across.
	/// @param dy World units down.
	void nudge(float dx, float dy);
	/// @brief Whether the window's focus is on the canvas or one of its children.
	[[nodiscard]] bool focusWithin() const noexcept;

	std::shared_ptr<const CanvasItemModel> model_{};                         ///< See model().
	std::unique_ptr<Internals> internals_;                                   ///< The index and scratch storage.
	std::vector<std::pair<std::string, std::vector<CanvasLevel>>> levels_{}; ///< Levels of detail, by child id.
	std::vector<std::shared_ptr<const Font>> fonts_{};                       ///< See setTextFonts(); by height.
	std::vector<std::string> selection_{};                                   ///< See selection(); sorted.
	CanvasGuides guides_{};                                                  ///< See guides().
	Camera2D camera_{};                                                      ///< See camera().
	Color accent_ = Color::hex(0x4f46e5);                                    ///< See setAccentColor().
	float grid_ = 0.0f;                                                      ///< See grid().
	bool snap_ = true;                                                       ///< See setSnap().
	bool rulers_ = true;                                                     ///< See setRulers().
	mutable std::size_t drawn_ = 0;                                          ///< See drawnItems().

	Window* listening_ = nullptr;             ///< The window whose input it listens to.
	std::array<Connection, 3> connections_{}; ///< Button, move and key.
	Gesture gesture_ = Gesture::none;         ///< The current press.
	int handle_ = -1;                         ///< The handle a resize or rotation drags.
	bool space_ = false;                      ///< Space is held.
	bool zoomKey_ = false;                    ///< Ctrl or Cmd is held.
	Vec2 pressedAt_{};                        ///< Where the press was, in window units.
	Vec2 pointer_{};                          ///< Where the pointer is, in window units.
	Camera2D pressedCamera_{};                ///< The camera at the press (for panning).
	Rect from_{};                             ///< The selection's rectangle at the press, in world units.
	Rect to_{};                               ///< Where a drag puts it.
	float rotation_ = 0.0f;                   ///< A rotation's angle so far, in degrees.
	bool guideVertical_ = false;              ///< The guide dragged is vertical.
	int guideIndex_ = -1;                     ///< The existing guide dragged, or -1 for a new one.
	bool additive_ = false;                   ///< Shift was held at the press.
};

} // namespace affresco
