#pragma once

/// @file
/// @brief format::editing::ImageDocuments: the images a screen edits ("images" section, "image" step), each a layered
/// document on a canvas model with its tools, view, colour profiles and the snapshot its state shows.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Brush.hpp"
#include "affresco/imaging/Canvas.hpp"
#include "affresco/imaging/Color.hpp"
#include "affresco/imaging/Filter.hpp"
#include "affresco/imaging/Gradient.hpp"
#include "affresco/imaging/Limits.hpp"
#include "affresco/imaging/Selection.hpp"
#include "affresco/render/ImageCache.hpp"

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <cstddef>
#include <utility>
#include <vector>

namespace affresco {
class Window;
}

namespace affresco::format::editing {

namespace imaging = affresco::imaging;

/// @brief What a tool does on the canvas: the library's gestures, which a document's tools are named after.
enum class Gesture : std::uint8_t {
	move,             ///< Moves the active layer's pixels.
	selectRectangle,  ///< Selects a rectangle.
	selectEllipse,    ///< Selects an ellipse.
	lasso,            ///< Selects a freehand shape.
	polygonLasso,     ///< Selects a polygon, click by click, closed at its first point.
	magicWand,        ///< Selects similar colours.
	crop,             ///< Crops the image to a rectangle.
	paint,            ///< Paints with the brush.
	erase,            ///< Erases with the brush.
	clone,            ///< Paints the image from an offset; the right button sets the source.
	gradient,         ///< Draws a gradient from the press to the release.
	fill,             ///< Fills similar colours.
	sample,           ///< Picks the foreground colour (the background's with the right button).
	pan,              ///< Pans the view.
	zoom,             ///< Zooms in, out with the right button.
};

/// @brief The gestures' names, in Gesture's order ("paint", "select-rectangle", ...).
[[nodiscard]] std::span<const std::string_view> gestureNames() noexcept;

/// @brief The limits an image may lower ("limits" in the "images" section), with the library's values.
/// @return {key, value}, in the reference's order.
[[nodiscard]] std::vector<std::pair<std::string_view, std::size_t>> libraryLimits();

/// @brief The gesture of a name.
/// @param name Its name.
/// @return The gesture, or std::nullopt for an unknown name.
[[nodiscard]] std::optional<Gesture> gestureOf(std::string_view name) noexcept;

/// @brief The keys of the tool options (the "setOptions" operation, and a tool's settings in the section).
[[nodiscard]] std::span<const std::string_view> optionKeys() noexcept;

/// @brief A tool a document declares: its own name for a gesture, and the options it sets when chosen.
struct ImageTool {
	std::string name;                     ///< The document's name for it, e.g. "brush".
	Gesture gesture = Gesture::paint;     ///< What it does.
	json settings = json::object();       ///< Options set when it is chosen (optionKeys()).
};

/// @brief One entry of the "images" section, read and checked.
struct ImageDefinition {
	int width = 1;                                           ///< The new image's width.
	int height = 1;                                          ///< The new image's height.
	imaging::PixelFormat format = imaging::PixelFormat::rgba8; ///< The new image's pixels.
	imaging::Pixel background{1.0f, 1.0f, 1.0f, 1.0f};       ///< What the new image's one layer is filled with.
	std::string into;                                        ///< The state path the snapshot goes to ("" for none).
	std::vector<ImageTool> tools;                            ///< The tools, in order (every gesture if none given).
	std::string tool;                                        ///< The tool chosen at first.
	imaging::Limits limits;                                  ///< The library's limits, lowered as it says.
};

/// @brief The most images one screen declares.
inline constexpr std::size_t maxImages = 16;

/// @brief Reads and checks one entry of the "images" section.
/// @param value The entry.
/// @param at Its place, e.g. "images.photo".
/// @return The definition.
/// @throws DocumentError If it is malformed or a limit would rise above the library's; what() names the place.
[[nodiscard]] ImageDefinition readImageDefinition(const json& value, const std::string& at);

/// @brief What the tools do, as the options set it.
struct ToolSettings {
	std::string tool;                                              ///< The document's name of the tool chosen.
	Gesture gesture = Gesture::paint;                              ///< Its gesture.
	imaging::BrushTip tip;                                         ///< paint, erase, clone: the tip.
	imaging::Pixel foreground{0.0f, 0.0f, 0.0f, 1.0f};             ///< The foreground colour.
	imaging::Pixel background{1.0f, 1.0f, 1.0f, 1.0f};             ///< The background colour.
	imaging::SelectionMode mode = imaging::SelectionMode::replace; ///< The selection gestures' mode.
	imaging::SampleOptions sample;                                 ///< magic-wand, fill: tolerance and contiguity.
	imaging::GradientShape shape = imaging::GradientShape::linear; ///< gradient: its shape.
	std::optional<imaging::Gradient> gradient; ///< gradient: its stops; none: foreground to background.
	float opacity = 1.0f;                      ///< fill, gradient: 0 to 1.
	bool quickMask = false;                    ///< The paint gestures paint the selection.
	bool autoSelect = false;                   ///< move: a press picks the topmost layer painted under it.
	std::optional<imaging::Point> cloneSource; ///< clone: the point copied from.
};

/// @brief A press in progress on the canvas.
struct Press {
	bool down = false;        ///< Whether a button is down.
	bool right = false;       ///< Whether it is the right button.
	imaging::Point start;     ///< Where it began, in document pixels.
	imaging::Point last;      ///< Where the pointer was last, in document pixels.
	double surfaceX = 0.0;    ///< Where it began on the surface.
	double surfaceY = 0.0;    ///< Where it began on the surface.
	imaging::CanvasView view; ///< The view when it began.
	bool pan = false;         ///< Whether it pans the view whatever the tool (the space bar was held).
	/// The selection mode of what it selects: Shift held adds, Alt subtracts, both intersect, else the tool's.
	imaging::SelectionMode mode = imaging::SelectionMode::replace;
};

/// @brief One image being edited: its document on a canvas model, and everything the operations share. Main thread
/// only.
struct ImageDocument {
	/// @brief An image as its definition says: one layer of its size, filled with its background.
	/// @param imageName Its name in the "images" section.
	/// @param read Its definition.
	/// @param source The definition's JSON, kept to notice a different one.
	ImageDocument(std::string imageName, ImageDefinition read, json source);
	/// @brief Ends the stroke and the preview, and takes back what the canvas holds (its pointer callback and the
	/// display transform): the canvas model is shared with the views that show it, which may outlive the image.
	~ImageDocument();
	ImageDocument(const ImageDocument&) = delete;
	ImageDocument& operator=(const ImageDocument&) = delete;
	ImageDocument(ImageDocument&&) = delete;
	ImageDocument& operator=(ImageDocument&&) = delete;

	std::string name;                             ///< Its name in the "images" section.
	ImageDefinition definition;                   ///< Its definition.
	json source;                                  ///< The definition as the document wrote it.
	imaging::Limits limits;                       ///< Bounds of everything opened or made (definition.limits).
	std::shared_ptr<imaging::CanvasModel> canvas; ///< The document; shared with the views that draw it.
	ToolSettings settings;                        ///< The tools' settings.
	imaging::CanvasView view;                     ///< The view the screen shows (zoom and centre).
	imaging::ColorProfile working = imaging::ColorProfile::srgb(); ///< The document's working space.
	imaging::ColorProfile display = imaging::ColorProfile::srgb(); ///< The display's (sRGB when unknown).
	std::optional<imaging::ColorProfile> proof;                    ///< The output proofed against, if any.
	std::optional<imaging::ColorTransform> displayTransform;       ///< Working to display, when they differ.
	std::unique_ptr<imaging::Stroke> stroke;                       ///< The stroke being painted.
	std::unique_ptr<imaging::FilterRun> preview;                   ///< The filter being previewed.
	imaging::Filter previewed;                                     ///< What preview runs.
	Press press;                                                   ///< The press in progress.
	std::optional<imaging::Point> cloneOffset;                     ///< clone: from the point painted to the one copied.
	float pressure = 1.0f;                                 ///< The pen's pressure of the event played (a mouse's is 1).
	bool spaceHeld = false;                                ///< The space bar is held: a press pans the view.
	std::vector<imaging::Point> path;                      ///< lasso, polygon-lasso: the points so far.
	imaging::SelectionMode pathMode{};                     ///< polygon-lasso: the mode of the press that began it.
	std::vector<std::uint8_t> saved;                       ///< The bytes last saved or opened (revert reads them).
	std::string message;                                   ///< What the last operation has to say (a status line).
	std::shared_ptr<const imaging::Image> clipboard;       ///< What copy and cut kept, the image's size then.
	std::uint64_t thumbnailRevision = ~std::uint64_t{0};   ///< The document revision the thumbnails show.
	std::uint64_t thumbnailGeneration = ~std::uint64_t{0}; ///< The document the thumbnails show.
};

/// @brief What shows an image (a "raster" node's view): told when it changes, to draw it again.
class ImageWatcher {
public:
	/// @brief The image changed: its pixels, its view, its selection or its tools, or it was made again (a new
	/// canvas).
	/// @param image The image.
	virtual void imageChanged(ImageDocument& image) = 0;

protected:
	/// @brief Nothing to set up.
	ImageWatcher() = default;
	/// @brief Not destroyed through this interface.
	~ImageWatcher() = default;
	ImageWatcher(const ImageWatcher&) = default;
	ImageWatcher& operator=(const ImageWatcher&) = default;
	ImageWatcher(ImageWatcher&&) noexcept = default;
	ImageWatcher& operator=(ImageWatcher&&) noexcept = default;
};

/// @brief The images of one runtime, by name, made when an "image" step or a "raster" node first names one.
///
/// Shared (std::shared_ptr in TypeRegistry::imageDocuments()) because the steps and the views that show the images
/// read the same ones, and a view may outlive a screen rebuilt around it. The runtime attaches the document's
/// declarations and the publisher that hands an image to the screen (its snapshot to "into", its thumbnails); the
/// views watch the images they show. Main thread only.
class ImageDocuments {
public:
	/// @brief The "images" section of the document shown, or nullptr when it has none.
	using Declarations = std::function<const json*()>;
	/// @brief Hands an image to the screen: its snapshot to its "into" state path, its thumbnails to the image cache.
	using Publisher = std::function<void(ImageDocument&)>;

	/// @brief Attaches the runtime: where the images are declared, and how the screen hears of them.
	/// @param declarations The document's "images" section.
	/// @param publisher The publisher.
	void attach(Declarations declarations, Publisher publisher);
	/// @brief Detaches the runtime, which is going away: no image is declared or published after.
	void detach() noexcept;
	/// @brief The image of a name the document declares, made from its entry the first time (image()).
	/// @param name Its name.
	/// @return The image, or nullptr if the document (or no runtime) declares none of that name.
	/// @throws DocumentError If its entry is malformed.
	[[nodiscard]] ImageDocument* declared(const std::string& name);
	/// @brief Tells a watcher whenever the image of a name changes, until unwatch().
	/// @param name The image's name.
	/// @param watcher The watcher; it unwatches before it goes away.
	void watch(const std::string& name, ImageWatcher& watcher);
	/// @brief Stops telling a watcher of an image.
	/// @param name The image's name.
	/// @param watcher The watcher.
	void unwatch(std::string_view name, const ImageWatcher& watcher) noexcept;
	/// @brief An image changed: tells the screen (when it should hear) and the watchers of that image only.
	/// @param image The image.
	/// @param publish Whether the screen hears of it (the publisher): a stroke on its way only draws again.
	/// @throws DocumentError If the publisher can't write the snapshot.
	void changed(ImageDocument& image, bool publish);

	/// @brief The image of a name, made from its definition the first time, and made again when the definition
	/// changes (a new screen with the same name).
	/// @param name Its name.
	/// @param definition Its entry of the "images" section.
	/// @param at Its place, for errors.
	/// @return The image.
	/// @throws DocumentError If the definition is malformed.
	ImageDocument& image(const std::string& name, const json& definition, const std::string& at);
	/// @brief An image already made.
	/// @param name Its name.
	/// @return It, or nullptr.
	[[nodiscard]] ImageDocument* find(std::string_view name) noexcept;
	/// @brief Forgets every image.
	void clear() noexcept { images_.clear(); }

private:
	/// @brief Tells the watchers of an image that it changed.
	/// @param image The image.
	void tell(ImageDocument& image);

	std::map<std::string, std::unique_ptr<ImageDocument>, std::less<>> images_;    ///< The images, by name.
	std::map<std::string, std::vector<ImageWatcher*>, std::less<>> watchers_;      ///< The watchers, by image name.
	Declarations declarations_;                                                    ///< See attach().
	Publisher publisher_;                                                          ///< See attach().
};

/// @brief Parses "#rrggbb" or "#rrggbbaa".
/// @param text The text.
/// @return The colour, or std::nullopt if it isn't one.
[[nodiscard]] std::optional<imaging::Pixel> colorOf(std::string_view text) noexcept;

/// @brief Writes a colour as "#rrggbb", or "#rrggbbaa" when it isn't opaque.
/// @param pixel The colour.
[[nodiscard]] std::string hexOf(const imaging::Pixel& pixel);

/// @brief A blank document: one layer named "Background", filled with a colour, and no history.
/// @param width Its width.
/// @param height Its height.
/// @param format Its pixels.
/// @param background The fill (transparent leaves the layer empty).
/// @param limits Its bounds.
/// @throws DocumentError If the size is beyond the limits.
[[nodiscard]] std::unique_ptr<imaging::Document> blankDocument(int width, int height, imaging::PixelFormat format,
                                                               const imaging::Pixel& background,
                                                               const imaging::Limits& limits);

/// @brief Replaces the document, keeping the settings, and fits it in the view.
/// @param image The image.
/// @param document The new document.
void replaceDocument(ImageDocument& image, std::unique_ptr<imaging::Document> document);

/// @brief Makes the display transform again after a profile changed (none when it would change nothing).
/// @param image The image.
void updateDisplay(ImageDocument& image);

/// @brief Zooms so the whole document fits, centred.
/// @param image The image.
void fitView(ImageDocument& image);

/// @brief What a screen shows of an image: the document, its layers, history and selection, the colours, the tool.
/// @param image The image.
/// @return The snapshot.
[[nodiscard]] json snapshot(const ImageDocument& image);

/// @brief Sends the layers' thumbnails ("<image>/layer/<id>") and the navigator's picture ("<image>/navigator") to
/// the named images, when the document changed since they were last sent.
/// @param image The image.
/// @param images Where named images go.
void updateThumbnails(ImageDocument& image, ImageCache& images);

/// @brief A document coordinate within reach: at most a billion pixels from the origin, far past any document and near
/// enough that the pixel coordinates the gestures and the canvas compute from it fit an int.
/// @param coordinate The coordinate, finite.
/// @return It, clamped.
[[nodiscard]] double withinReach(double coordinate) noexcept;

/// @brief Plays a pointer event of the canvas with the tool chosen, or, for a press while the space bar is held
/// (ImageDocument::spaceHeld), pans the view until the release. The "pointer" operation and the "raster" node's view
/// both play their events here.
/// @param image The image.
/// @param event The event, in surface and document pixels.
/// @return Whether the screen should hear of it (the document, the view or a colour changed).
bool applyPointer(ImageDocument& image, const imaging::CanvasPointer& event);

/// @brief Offsets the active layer's pixels as one step.
/// @param document The document.
/// @param dx Across.
/// @param dy Down.
/// @throws DocumentError If the active layer can't be moved.
void moveActiveLayer(imaging::Document& document, int dx, int dy);

/// @brief Crops the document to a rectangle; the history starts again, since a crop changes the size.
/// @param image The image.
/// @param x Its left.
/// @param y Its top.
/// @param width Its width.
/// @param height Its height.
/// @throws DocumentError If the rectangle is empty.
void crop(ImageDocument& image, int x, int y, int width, int height);

/// @brief A number argument, within the range of a float so that it narrows to one safely.
/// @param arguments The arguments.
/// @param key Its key.
/// @param fallback When it is absent, not a number or not finite.
/// @return It, between -FLT_MAX and FLT_MAX.
[[nodiscard]] double numberOf(const json& arguments, std::string_view key, double fallback) noexcept;

/// @brief A whole number argument, rounded toward zero and within the range of an int.
/// @param arguments The arguments.
/// @param key Its key.
/// @param fallback When it is absent, not a number or not finite.
/// @return It.
[[nodiscard]] int wholeOf(const json& arguments, std::string_view key, int fallback) noexcept;

/// @brief A text argument.
/// @param arguments The arguments.
/// @param key Its key.
/// @param fallback When it is absent or not text.
/// @return It.
[[nodiscard]] std::string textOf(const json& arguments, std::string_view key, std::string_view fallback = "");

/// @brief A boolean argument.
/// @param arguments The arguments.
/// @param key Its key.
/// @param fallback When it is absent or not a boolean.
/// @return It.
[[nodiscard]] bool flagOf(const json& arguments, std::string_view key, bool fallback) noexcept;

/// @brief The layer an argument "id" names (its id as text), or the active layer without one.
/// @param image The image.
/// @param arguments The arguments.
/// @return The layer's id.
/// @throws DocumentError If there is no such layer.
[[nodiscard]] std::uint64_t layerOf(const ImageDocument& image, const json& arguments);

/// @brief Ends a stroke or a filter preview in progress, before another edit.
/// @param image The image.
void settle(ImageDocument& image);

/// @brief A selection mode by its name.
/// @param name "replace", "add", "subtract" or "intersect".
/// @param fallback When it is none of them.
/// @return The mode.
[[nodiscard]] imaging::SelectionMode selectionModeOf(std::string_view name, imaging::SelectionMode fallback) noexcept;

/// @brief A point argument: an object with x and y, within reach.
/// @param arguments The arguments.
/// @param key Its key.
/// @return The point.
/// @throws DocumentError If it is missing.
[[nodiscard]] imaging::Point pointOf(const json& arguments, std::string_view key);

/// @brief Chooses a tool by the document's name for it, and sets the options it declares.
/// @param image The image.
/// @param name The tool's name.
/// @throws DocumentError If the image has no such tool, or its options are malformed.
void chooseTool(ImageDocument& image, const std::string& name);

/// @brief Sets tool options: the brush, the sampling, the gradient and the colours (optionKeys()).
/// @param settings The settings.
/// @param cloneOffset The clone offset, forgotten when the clone source is set.
/// @param limits The bounds of the brush.
/// @param arguments The options set.
/// @throws DocumentError If an option is malformed.
void setOptions(ToolSettings& settings, std::optional<imaging::Point>& cloneOffset, const imaging::Limits& limits,
                const json& arguments);

} // namespace affresco::format::editing
