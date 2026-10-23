#pragma once

/// @file
/// @brief The image editor's canvas: a GpuSurface that composites a document's layers on the GPU, at the mip level
/// the zoom needs, from tiles kept resident within a budget, and shows them through the display profile with the
/// selection's marching ants or the quick mask.
///
/// The document lives in a CanvasModel the host owns; the surface holds it weakly, so a closed document leaves an
/// empty canvas. The view (zoom, centre) and the overlays are the surface's parameters, bound to the screen's state:
///   zoom (1 = one document pixel per surface pixel), x and y (the document point at the centre), ants, quickMask and
///   checker (booleans: marching ants around the selection, the red quick mask outside it, a checkerboard under
///   transparency; checker is on by default), revision (any number that changes when the pixels did) and scale (the
///   pixels per window unit the zoom was set for, by default the frame's: a frame drawn at another scale, such as a
///   capture, zooms by the ratio, so it shows the same picture).
/// The pixels never go into the screen's state: only these parameters do.

#include "affresco/imaging/Color.hpp"
#include "affresco/imaging/Document.hpp"
#include "affresco/imaging/Residency.hpp"
#include "affresco/render/GpuSurface.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace affresco::imaging {

/// @brief Where a canvas looks: its zoom and the document point at its centre.
struct CanvasView {
	double zoom = 1.0; ///< Surface pixels per document pixel.
	double x = 0.0;    ///< The document point at the centre, across.
	double y = 0.0;    ///< The document point at the centre, down.
	int width = 0;     ///< The surface's width, in pixels.
	int height = 0;    ///< The surface's height, in pixels.

	/// @brief The document point under a surface point.
	/// @param surfaceX Pixels from the surface's left.
	/// @param surfaceY Pixels from the surface's top.
	/// @return The document point, x then y.
	[[nodiscard]] std::array<double, 2> toDocument(double surfaceX, double surfaceY) const noexcept;
	/// @brief The surface point over a document point.
	/// @param documentX Across the document.
	/// @param documentY Down the document.
	/// @return The surface point, x then y.
	[[nodiscard]] std::array<double, 2> toSurface(double documentX, double documentY) const noexcept;
};

/// @brief The mip level a zoom draws from: the coarsest whose pixels are still no larger than the surface's.
/// @param zoom The zoom.
/// @param levels The image's levels.
/// @return 0 at a zoom of 1 or more; ceil(log2(1 / zoom)) below, at most levels - 1.
[[nodiscard]] int canvasLevel(double zoom, int levels) noexcept;

/// @brief What a canvas did in its last frame, for the status bar, the Navigator and the budgets.
struct CanvasStatistics {
	std::size_t residentBytes = 0; ///< GPU memory of resident tiles.
	std::size_t residentTiles = 0; ///< Tiles resident (transparent ones included, at no memory).
	std::uint64_t uploads = 0;     ///< Tiles uploaded, in all.
	std::uint64_t evictions = 0;   ///< Tiles evicted, in all.
	std::uint64_t frames = 0;      ///< Frames drawn, in all.
	int level = 0;                 ///< The mip level drawn.
	int regionWidth = 0;           ///< The region composited, in that level's pixels.
	int regionHeight = 0;          ///< The region composited, in that level's pixels.
};

/// @brief A pointer event over a canvas, with the document point under it.
struct CanvasPointer {
	SurfacePointer pointer; ///< The event, in surface pixels.
	double x = 0.0;         ///< The document point, across.
	double y = 0.0;         ///< The document point, down.
};

/// @brief A document shown on canvases: its pixels, its display conversion, and what the canvases report back.
class CanvasModel {
public:
	/// @brief A model of a document.
	/// @param document The document.
	explicit CanvasModel(std::unique_ptr<Document> document) : document_(std::move(document)) {}

	/// @brief The document.
	[[nodiscard]] Document& document() noexcept { return *document_; }
	/// @brief The document.
	[[nodiscard]] const Document& document() const noexcept { return *document_; }
	/// @brief Shows another document (canvases drop their resident tiles).
	/// @param document The document.
	void setDocument(std::unique_ptr<Document> document);
	/// @brief Counts the documents shown, so canvases know when to start again.
	[[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }

	/// @brief Converts what is shown from the working space to the display's, or stops converting.
	/// @param transform The conversion (working to display, possibly proofing), or null to show the values as they are.
	void setDisplay(const ColorTransform* transform);
	/// @brief The display conversion sampled on a cube (ColorTransform::table), or empty for none.
	[[nodiscard]] const std::vector<float>& displayTable() const noexcept { return displayTable_; }
	/// @brief Points per edge of the display table.
	[[nodiscard]] int displaySide() const noexcept { return displaySide_; }
	/// @brief Counts the display table's changes.
	[[nodiscard]] std::uint64_t displayVersion() const noexcept { return displayVersion_; }

	/// @brief Tools: each pointer event over a canvas, in order, before it draws. Owned here, so it goes with the
	/// model.
	std::function<void(const CanvasPointer&)> onPointer;
	CanvasStatistics statistics; ///< What the canvas drew last (one canvas per model reports here).
	CanvasView view;             ///< The view the canvas drew last.

private:
	std::unique_ptr<Document> document_; ///< See document().
	std::uint64_t generation_ = 0;       ///< See generation().
	std::vector<float> displayTable_;    ///< See displayTable().
	int displaySide_ = 0;                ///< See displaySide().
	std::uint64_t displayVersion_ = 0;   ///< See displayVersion().
};

/// @brief The canvas: composites the model's document on the GPU (CanvasModel describes the parameters).
///
/// Each frame it picks the mip level and the region of it under the surface, then composites the layers bottom
/// first into a half-float pass per group depth, a tile cell at a time: a tile's texture is uploaded when first
/// needed or changed and evicted least recently used beyond Limits::gpuBytes. Its blending, masks, clipping, groups
/// and adjustments match the CPU Compositor (the reference it is tested against).
class CanvasSurface final : public GpuSurface {
public:
	/// @brief A canvas of a model.
	/// @param model The model, held weakly: when it is gone the canvas draws nothing.
	explicit CanvasSurface(std::weak_ptr<CanvasModel> model) noexcept : model_(std::move(model)) {}
	/// @brief Releases what is left (release() normally ran first).
	~CanvasSurface() override = default;

	/// @brief Makes the programs, the vertex array and the adjustment and display tables' textures.
	/// @param setup The shader prelude.
	void prepare(const SurfaceSetup& setup) override;
	/// @brief Hands the input to the model's tools, then composites and shows the document.
	/// @param frame The frame.
	void render(SurfaceFrame& frame) override;
	/// @brief Frees the programs, the textures and every resident tile.
	void release() override;

private:
	/// @brief The passes and the region of one frame.
	struct Region;

	/// @brief Composites a list of layers into a depth's pass.
	/// @param document The document.
	/// @param list The layers, the bottom first.
	/// @param region The frame's region.
	/// @param depth The depth.
	void compositeList(const Document& document, const std::vector<std::uint64_t>& list, Region& region,
	                   std::size_t depth);
	/// @brief Composites one layer into its depth's pass.
	/// @param document The document.
	/// @param layer The layer.
	/// @param region The frame's region.
	/// @param depth The depth.
	/// @param stage 0 to blend it, 1 to write its coverage (its clipping base).
	void drawLayer(const Document& document, const Layer& layer, Region& region, std::size_t depth, int stage);
	/// @brief Draws the selection's coverage into the scratch pass, for the ants and the quick mask.
	/// @param document The document.
	/// @param region The frame's region.
	void drawSelection(const Document& document, Region& region);
	/// @brief Binds a tile's texture to a unit, uploading it if needed.
	/// @param image The image.
	/// @param id Its id.
	/// @param key The tile.
	/// @param unit The texture unit.
	/// @return Whether there is a tile (false: it is transparent, nothing bound).
	bool bindTile(const Image& image, std::uint64_t id, const TileKey& key, int unit);
	/// @brief Shows the composite on the surface's framebuffer.
	/// @param frame The frame.
	/// @param region The frame's region.
	/// @param model The model.
	void present(const SurfaceFrame& frame, const Region& region, const CanvasModel& model);
	/// @brief Uploads the display table if it changed.
	/// @param model The model.
	void updateDisplay(const CanvasModel& model);
	/// @brief Frees every resident tile's texture.
	void dropResident();

	std::weak_ptr<CanvasModel> model_;          ///< The model.
	unsigned vertexArray_ = 0;                  ///< The empty vertex array the triangles need.
	unsigned layerProgram_ = 0;                 ///< Composites a layer (or writes its coverage).
	unsigned displayProgram_ = 0;               ///< Shows the composite.
	unsigned tableTexture_ = 0;                 ///< An adjustment's 256-entry table.
	unsigned displayTexture_ = 0;               ///< The display table, a 3D texture.
	std::uint64_t displayVersion_ = 0;          ///< The display table's version uploaded.
	std::uint64_t generation_ = 0;              ///< The model's generation the residency is of.
	TileResidency residency_{0};                ///< The resident tiles.
	std::array<std::vector<unsigned>, 3> pool_; ///< Textures evicted, kept for reuse, by PixelFormat.
	std::vector<Resident> evicted_;             ///< Scratch for the residency's evictions.
	std::uint64_t uploads_ = 0;                 ///< Tiles uploaded.
};

/// @brief Makes canvases of a model, for Screen::registerSurface.
/// @param model The model, held weakly.
[[nodiscard]] SurfaceFactory canvasSurfaceFactory(std::weak_ptr<CanvasModel> model);

} // namespace affresco::imaging
