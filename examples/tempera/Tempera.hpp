#pragma once

/// @file
/// @brief Tempera, an image editor whose workspace is a screen document (index.screen.json): the session the
/// natives share (the document on its canvas, the tools' settings, the colour spaces) and the functions that register
/// them. The pixels stay here, in the canvas model; the screen's state holds only a snapshot that describes them.

#include <affresco/Screen.hpp>
#include <affresco/core/Executor.hpp>
#include <affresco/core/Json.hpp>
#include <affresco/imaging/Brush.hpp>
#include <affresco/imaging/Canvas.hpp>
#include <affresco/imaging/Color.hpp>
#include <affresco/imaging/Filter.hpp>
#include <affresco/imaging/Gradient.hpp>
#include <affresco/imaging/Selection.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace tempera {

namespace imaging = affresco::imaging;
using affresco::json;

/// @brief A tool of the tool column.
enum class Tool : std::uint8_t {
	move,       ///< Moves the active layer's pixels (V).
	marquee,    ///< Selects a rectangle (M).
	ellipse,    ///< Selects an ellipse (shift+M).
	lasso,      ///< Selects a freehand shape (L).
	polygon,    ///< Selects a polygon, click by click, closed at its first point (shift+L).
	wand,       ///< Selects similar colours (W).
	crop,       ///< Crops the document to a rectangle (C).
	brush,      ///< Paints (B).
	eraser,     ///< Erases (E).
	clone,      ///< Paints the image from an offset (S); the right button sets the source.
	gradient,   ///< Draws a gradient from the press to the release (G).
	fill,       ///< Fills similar colours (shift+G).
	eyedropper, ///< Picks the foreground colour (I).
	hand,       ///< Pans (H).
	zoom,       ///< Zooms in, out with the right button (Z).
};

/// @brief The tools' names, in Tool's order.
[[nodiscard]] std::span<const std::string_view> toolNames() noexcept;

/// @brief The tool of a name.
/// @param name Its name.
/// @return The tool, or std::nullopt for an unknown name.
[[nodiscard]] std::optional<Tool> toolOf(std::string_view name) noexcept;

/// @brief What the tools do, as the options bar sets it.
struct ToolSettings {
	Tool tool = Tool::brush;                                       ///< The tool chosen.
	imaging::BrushTip tip;                                         ///< brush, eraser, clone: the tip.
	imaging::Pixel foreground{0.0f, 0.0f, 0.0f, 1.0f};             ///< The foreground colour.
	imaging::Pixel background{1.0f, 1.0f, 1.0f, 1.0f};             ///< The background colour.
	imaging::SelectionMode mode = imaging::SelectionMode::replace; ///< The selection tools' mode.
	imaging::SampleOptions sample;                                 ///< wand, fill: tolerance and contiguity.
	imaging::GradientShape shape = imaging::GradientShape::linear; ///< gradient: its shape.
	std::optional<imaging::Gradient> gradient; ///< gradient: its stops; none: foreground to background.
	float opacity = 1.0f;                      ///< fill, gradient: 0 to 1.
	bool quickMask = false;                    ///< Paint tools paint the selection.
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
};

/// @brief Everything Tempera's natives share. Main thread only.
struct Session {
	/// @brief A session with a blank document.
	/// @param bounds The bounds of everything opened or made.
	explicit Session(const imaging::Limits& bounds);
	/// @brief Ends the stroke and the preview, and takes back what the canvas holds of the session (its pointer
	/// callback and the display transform): the canvas is shared with the surface, which may outlive it.
	~Session();
	Session(const Session&) = delete;
	Session& operator=(const Session&) = delete;

	imaging::Limits limits;                       ///< Bounds of everything opened or made.
	std::shared_ptr<imaging::CanvasModel> canvas; ///< The document, drawn by the "temperaCanvas" surface.
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
	std::vector<imaging::Point> path;                      ///< lasso, polygon: the points so far.
	std::vector<std::uint8_t> saved;                       ///< The bytes last saved (Revert reads them).
	std::string message;                                   ///< The status bar's message.
	std::uint64_t thumbnailRevision = ~std::uint64_t{0};   ///< The document revision the thumbnails show.
	std::uint64_t thumbnailGeneration = ~std::uint64_t{0}; ///< The document the thumbnails show.
	affresco::Window* window = nullptr;                    ///< Shows file dialogs; null in scenarios.
	std::function<void()> changed;                         ///< Tells the screen after a change made on the canvas.
};

/// @brief Parses "#rrggbb" or "#rrggbbaa".
/// @param text The text.
/// @return The colour, or std::nullopt if it isn't one.
[[nodiscard]] std::optional<imaging::Pixel> colorOf(std::string_view text) noexcept;

/// @brief Writes a colour as "#rrggbb", or "#rrggbbaa" when it isn't opaque.
/// @param pixel The colour.
[[nodiscard]] std::string hexOf(const imaging::Pixel& pixel);

/// @brief A new document with a generated sample: a sky, hills and a sun over a background layer.
/// @param width Its width.
/// @param height Its height.
/// @param limits Its bounds.
/// @return The document, with no history.
/// @throws affresco::DocumentError If the size is beyond the limits.
[[nodiscard]] std::unique_ptr<imaging::Document> sampleDocument(int width, int height, const imaging::Limits& limits);

/// @brief Replaces the document, keeping the session's settings, and fits it in the view.
/// @param session The session.
/// @param document The new document.
void replaceDocument(Session& session, std::unique_ptr<imaging::Document> document);

/// @brief Makes the display transform again after a profile changed (none when it would change nothing).
/// @param session The session.
void updateDisplay(Session& session);

/// @brief Zooms so the whole document fits, centred.
/// @param session The session.
void fitView(Session& session);

/// @brief What the screen shows of the session: the document, its layers, history and selection, the colours.
/// @param session The session.
/// @param screen Receives the layers' thumbnails and the navigator's picture as images.
/// @return The snapshot.
[[nodiscard]] json snapshot(Session& session, affresco::Screen& screen);

/// @brief A document coordinate within reach: at most a billion pixels from the origin, far past any document and near
/// enough that the pixel coordinates the tools and the canvas compute from it fit an int.
/// @param coordinate The coordinate, finite.
/// @return It, clamped.
[[nodiscard]] double withinReach(double coordinate) noexcept;

/// @brief Plays a pointer event of the canvas with the tool chosen.
/// @param session The session.
/// @param event The event, in surface and document pixels.
/// @return Whether the screen should hear of it (the document, the view or a colour changed).
bool applyPointer(Session& session, const imaging::CanvasPointer& event);

/// @brief Offsets the active layer's pixels as one step.
/// @param document The document.
/// @param dx Across.
/// @param dy Down.
/// @throws affresco::DocumentError If the active layer can't be moved.
void moveActiveLayer(imaging::Document& document, int dx, int dy);

/// @brief Crops the document to a rectangle; the history starts again, since a crop changes the size.
/// @param session The session.
/// @param x Its left.
/// @param y Its top.
/// @param width Its width.
/// @param height Its height.
/// @throws affresco::DocumentError If the rectangle is empty.
void crop(Session& session, int x, int y, int width, int height);

/// @brief Registers the document natives (tempera.new, open, save, revert, undo, layers) and the canvas surface.
/// @param screen The screen.
/// @param session The session, which the natives hold weakly.
/// @param executor Where canvas changes are posted to the screen, or null to tell it at once.
void registerTempera(affresco::Screen& screen, const std::shared_ptr<Session>& session, affresco::Executor* executor);

/// @brief Registers the editing natives (selections, tools, filters, colour), called by registerTempera.
/// @param screen The screen.
/// @param session The session, which the natives hold weakly.
void registerEditing(affresco::Screen& screen, const std::shared_ptr<Session>& session);

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
[[nodiscard]] std::string textOf(const json& arguments, std::string_view key, std::string_view fallback = "");

/// @brief A boolean argument.
/// @param arguments The arguments.
/// @param key Its key.
/// @param fallback When it is absent or not a boolean.
[[nodiscard]] bool flagOf(const json& arguments, std::string_view key, bool fallback) noexcept;

/// @brief The layer an argument "id" names (its id as text), or the active layer without one.
/// @param session The session.
/// @param arguments The arguments.
/// @throws affresco::DocumentError If there is no such layer.
[[nodiscard]] std::uint64_t layerOf(const Session& session, const json& arguments);

/// @brief Ends a stroke or a filter preview in progress, before another edit.
/// @param session The session.
void settle(Session& session);

/// @brief Runs a native with the session, or fails if it has gone, and answers with the snapshot.
/// @param screen The screen.
/// @param name The native's name.
/// @param session The session, held weakly.
/// @param body What the native does with the session and its arguments.
void addNative(affresco::Screen& screen, const std::string& name, const std::weak_ptr<Session>& session,
               std::function<void(Session&, const json&)> body);

} // namespace tempera
