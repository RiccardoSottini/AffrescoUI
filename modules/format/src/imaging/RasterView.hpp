#pragma once

/// @file
/// @brief format::RasterView: the panel of a "raster" node. It shows an image the screen edits (editing::ImageDocument)
/// on its canvas surface, draws what the tools are doing over it, and plays the pointer through the current tool.

#include "ImageDocuments.hpp"

#include "affresco/render/Color.hpp"
#include "affresco/render/GpuSurface.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace affresco::format {

/// @brief How a raster node draws, besides the image.
struct RasterLook {
	bool checker = true;                         ///< A checkerboard where the image is transparent.
	Color overlay = colors::white;               ///< The outlines of what the tools are doing, the crop's thirds.
	Color shade = colors::black.withAlpha(0.5f); ///< Over what a crop leaves out.

	/// @brief Equal when every member is equal.
	bool operator==(const RasterLook&) const = default;
};

/// @brief Shows an image of the store: its composite over a checkerboard where it is transparent, on the panel's own
/// background, at the image's view. Over it: the selection's marching ants and the quick mask (the canvas surface
/// draws them), the crop being dragged (its thirds, the rest shaded), the marquee or the lasso being drawn, and the
/// brush's outline under the pointer.
///
/// Its pointer events go through editing::applyPointer(), as the "pointer" operation's do; the space bar held while
/// it has the focus pans whatever the tool, and the wheel zooms about the pointer. It watches its image in the store,
/// and draws again when the store says it changed. Main thread only.
class RasterView final : public SurfaceView, private editing::ImageWatcher {
public:
	/// @brief An empty view, until show().
	/// @param images The runtime's images; held weakly, as the view may outlive the runtime.
	explicit RasterView(std::weak_ptr<editing::ImageDocuments> images);
	/// @brief Stops watching its image.
	~RasterView() override;
	RasterView(const RasterView&) = delete;
	RasterView& operator=(const RasterView&) = delete;
	RasterView(RasterView&&) = delete;
	RasterView& operator=(RasterView&&) = delete;

	/// @brief Shows an image the document declares, made the first time it is named.
	/// @param name Its name in the "images" section.
	/// @param look How to draw it.
	/// @return Whether the document declares it.
	/// @throws DocumentError If its entry is malformed.
	bool show(const std::string& name, const RasterLook& look);
	/// @brief The name of the image it shows; empty before show().
	[[nodiscard]] const std::string& imageName() const noexcept { return name_; }

protected:
	/// @brief Draws the image, then what the tools are doing over it.
	/// @param painter The painter.
	void paint(Painter& painter) const override;
	/// @brief Keeps the image's view the size of the panel, in pixels; the first view to size an image fits it.
	void arrange() override;
	/// @brief Plays a move through the tool, and moves the brush's outline.
	/// @param event The event.
	void mouseMoveEvent(const MouseMoveEvent& event) override;
	/// @brief Plays a press or a release through the tool.
	/// @param event The event.
	void mouseButtonEvent(const MouseButtonEvent& event) override;
	/// @brief Zooms about the pointer.
	/// @param event The event.
	void scrollEvent(const ScrollEvent& event) override;
	/// @brief The space bar pans while held; other keys go on.
	/// @param event The event.
	void keyEvent(const KeyEvent& event) override;
	/// @brief Losing the focus lets go of the space bar.
	/// @param focused Whether it has the focus.
	void focusEvent(bool focused) override;
	/// @brief Shows the brush's outline only while the pointer is over the view.
	/// @param hovered Whether it is.
	void hoverEvent(bool hovered) override;

private:
	/// @brief Draws again: a new canvas, a new view or new pixels.
	/// @param image The image.
	void imageChanged(editing::ImageDocument& image) override;
	/// @brief The image it shows, if the store still has it.
	/// @return It, or nullptr.
	[[nodiscard]] editing::ImageDocument* image() const;
	/// @brief Sizes the image's view (and its canvas's) to the panel, in pixels, fitting the image the first time a
	/// view sizes its canvas.
	/// @param image The image.
	/// @return Whether it fitted the image.
	bool size(editing::ImageDocument& image) const;
	/// @brief Plays a pointer event through the current tool, then tells the store what changed.
	/// @param pointer The event, in window units.
	void play(SurfacePointer pointer);
	/// @brief Pixels per window unit.
	/// @return The window's content scale, or 1 outside one.
	[[nodiscard]] float scale() const noexcept;
	/// @brief Where a document point lands in the window.
	/// @param image The image.
	/// @param x Across, in document pixels.
	/// @param y Down.
	/// @return In window units.
	[[nodiscard]] Vec2 toWindow(const editing::ImageDocument& image, double x, double y) const noexcept;
	/// @brief Draws the crop being dragged: the rest shaded, the outline and its thirds.
	/// @param painter The painter.
	/// @param image The image.
	void paintCrop(Painter& painter, const editing::ImageDocument& image) const;
	/// @brief Draws the marquee being dragged, or the lasso being drawn.
	/// @param painter The painter.
	/// @param image The image.
	void paintOutline(Painter& painter, const editing::ImageDocument& image) const;
	/// @brief Draws the brush's outline under the pointer.
	/// @param painter The painter.
	/// @param image The image.
	void paintBrush(Painter& painter, const editing::ImageDocument& image) const;

	std::weak_ptr<editing::ImageDocuments> images_; ///< The runtime's images.
	std::string name_;                              ///< See imageName().
	RasterLook look_;                               ///< See show().
	std::weak_ptr<imaging::CanvasModel> shown_;     ///< The canvas the surface draws.
	std::uint64_t surfaces_ = 0;                    ///< Canvases shown so far, to name each surface anew.
	std::uint64_t changes_ = 0;                     ///< Changes told so far: each draws again.
	Vec2 pointer_;                                  ///< Where the pointer is, in window units.
	bool hovered_ = false;                          ///< Whether the pointer is over the view.
	Mods mods_;                                     ///< The modifier keys of the last button event.
};

} // namespace affresco::format
