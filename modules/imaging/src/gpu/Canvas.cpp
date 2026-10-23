/// @file
/// @brief The canvas: the view's mapping, the model, and the GPU compositor of a document's layers through passes.

#include "affresco/imaging/Canvas.hpp"

#include "CanvasShaders.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>

namespace affresco::imaging {

namespace {

/// @brief Group depths the canvas has passes for: the top level and Limits::groupDepth below it.
constexpr std::size_t depths = 3;
/// @brief The passes' names, by depth.
constexpr std::array<const char*, depths> accumulationNames{"accumulation0", "accumulation1", "accumulation2"};
/// @brief The clipping bases' names, by depth.
constexpr std::array<const char*, depths> baseNames{"base0", "base1", "base2"};
/// @brief Textures kept for reuse per format, beyond which evicted ones are freed.
constexpr std::size_t pooled = 32;

/// @brief A number parameter, finite, or a default.
/// @param params The parameters.
/// @param key Its name.
/// @param otherwise The default.
[[nodiscard]] double numberOf(const json& params, const char* key, double otherwise) {
	const auto found = params.find(key);
	return found != params.end() && found->is_number() && std::isfinite(found->get<double>()) ? found->get<double>()
	                                                                                          : otherwise;
}

/// @brief A boolean parameter, or a default.
/// @param params The parameters.
/// @param key Its name.
/// @param otherwise The default.
[[nodiscard]] bool flagOf(const json& params, const char* key, bool otherwise) {
	const auto found = params.find(key);
	return found != params.end() && found->is_boolean() ? found->get<bool>() : otherwise;
}

/// @brief The GL formats of a pixel format: internal, format and type.
/// @param format The pixel format.
[[nodiscard]] std::array<GLenum, 3> glFormatOf(PixelFormat format) noexcept {
	switch(format) {
	case PixelFormat::rgba16f:
		return {GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT};
	case PixelFormat::mask8:
		return {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
	default:
		return {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
	}
}

/// @brief Clears a pass to transparent.
/// @param pass The pass.
void clearPass(const SurfacePass& pass) {
	glBindFramebuffer(GL_FRAMEBUFFER, pass.framebuffer);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

/// @brief A uniform's location in the program in use.
/// @param program The program.
/// @param name Its name.
[[nodiscard]] GLint at(unsigned program, const char* name) { return glGetUniformLocation(program, name); }

/// @brief Binds a texture to a unit.
/// @param unit The unit.
/// @param texture The texture.
/// @param target Its target.
void bindTexture(int unit, unsigned texture, GLenum target = GL_TEXTURE_2D) {
	glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + unit));
	glBindTexture(target, texture);
}

/// @brief The view a frame's parameters ask for: the zoom clamped, the centre the document's by default. A zoom set
/// for another scale (the "scale" parameter: pixels per window unit) is drawn at the frame's, so a capture drawn at
/// another scale shows the same picture.
/// @param params The frame's parameters.
/// @param document The document.
/// @param frame The frame, for its size and scale.
/// @return The view.
[[nodiscard]] CanvasView viewOf(const json& params, const Document& document, const SurfaceFrame& frame) {
	const double setFor = std::clamp(numberOf(params, "scale", frame.scale), 0.25, 8.0);
	return {std::clamp(numberOf(params, "zoom", 1.0) * frame.scale / setFor, 1.0 / 1024.0, 256.0),
	        numberOf(params, "x", document.width() * 0.5), numberOf(params, "y", document.height() * 0.5), frame.width,
	        frame.height};
}

/// @brief Puts a model's view back when it goes, for a frame drawn at another scale than its zoom was set for (a
/// capture): such a frame doesn't change where the canvas looks, which fitting an image reads.
class ViewKeeper {
public:
	/// @brief Keeps the model's view if the frame is drawn at another scale than the parameters' zoom was set for.
	/// @param model The model.
	/// @param params The frame's parameters.
	/// @param frame The frame.
	ViewKeeper(CanvasModel& model, const json& params, const SurfaceFrame& frame)
	    : model_(model), kept_(model.view),
	      keep_(params.contains("scale") && numberOf(params, "scale", frame.scale) != frame.scale) {}
	/// @brief Puts the view back, if it kept it.
	~ViewKeeper() {
		if(keep_) model_.view = kept_;
	}
	ViewKeeper(const ViewKeeper&) = delete;
	ViewKeeper& operator=(const ViewKeeper&) = delete;
	ViewKeeper(ViewKeeper&&) = delete;
	ViewKeeper& operator=(ViewKeeper&&) = delete;

private:
	CanvasModel& model_; ///< The model.
	CanvasView kept_;    ///< Its view before the frame.
	bool keep_;          ///< Whether to put it back.
};

/// @brief Hands a frame's pointer events to the model's tools, in document coordinates.
/// @param model The model.
/// @param view The frame's view (the tools may change the model's).
/// @param input The events.
void forwardPointers(const CanvasModel& model, const CanvasView& view, std::span<const SurfacePointer> input) {
	for(const SurfacePointer& event : input)
		if(model.onPointer) {
			const auto [x, y] = view.toDocument(event.position.x, event.position.y);
			model.onPointer(CanvasPointer{event, x, y});
		}
}

/// @brief An edge of the region in a level's pixels, from a coordinate of the view, within [0, size].
/// @param value The coordinate, already rounded outwards. It is clamped as a double, because a view far off the
/// document (or not a number) must not overflow an int.
/// @param size The level's width or height.
/// @return The edge.
[[nodiscard]] int edgeOf(double value, int size) noexcept {
	if(!(value > 0.0)) return 0;
	return value >= static_cast<double>(size) ? size : static_cast<int>(value);
}

/// @brief Whether a list has a visible clipped layer, so its depth needs a clipping base.
/// @param document The document.
/// @param list The layers.
/// @return true if one is clipped and visible.
[[nodiscard]] bool clips(const Document& document, const std::vector<std::uint64_t>& list) {
	return std::ranges::any_of(list, [&](std::uint64_t id) {
		const Layer* layer = document.layer(id);
		return layer != nullptr && layer->clipped && layer->visible;
	});
}

/// @brief Whether a layer is the base the clipped layers above it clip to.
/// @param layer The layer.
/// @param clipping Whether its list clips.
/// @return true if the list clips and the layer isn't clipped itself.
[[nodiscard]] bool isBase(const Layer& layer, bool clipping) noexcept { return clipping && !layer.clipped; }

/// @brief Binds an adjustment layer's table and sets its uniforms.
/// @param program The layer program, in use.
/// @param tableTexture The adjustment table's texture.
/// @param adjustment The adjustment.
void bindAdjustment(unsigned program, unsigned tableTexture, const Adjustment& adjustment) {
	const auto table = adjustmentTable(adjustment);
	bindTexture(canvas::tableUnit, tableTexture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, table.data());
	// A threshold compares the luminosity itself: through the table, a half step below the level would flip
	const bool threshold = adjustment.kind == AdjustmentKind::threshold;
	glUniform1i(at(program, "adjustKind"),
	            threshold ? canvas::thresholdAdjust : static_cast<int>(adjustmentShader(adjustment)));
	glUniform1f(at(program, "level"), adjustment.level);
	glUniform3f(at(program, "hsl"), adjustment.hue, adjustment.saturation, adjustment.lightness);
}

/// @brief Sets the layer program's per-layer uniforms: the stage, clipping, blend mode and opacity.
/// @param program The layer program, in use.
/// @param layer The layer.
/// @param stage canvas::baseStage or blendStage.
/// @param clipped Whether the layer clips to the base pass bound.
void setLayerUniforms(unsigned program, const Layer& layer, int stage, bool clipped) {
	glUniform1i(at(program, "stage"), stage);
	glUniform1i(at(program, "clipped"), clipped ? 1 : 0);
	glUniform1i(at(program, "mode"), static_cast<int>(layer.blend));
	glUniform1f(at(program, "opacity"), layer.opacity);
}

/// @brief A layer's mask, when it has one and it is enabled.
/// @param document The document.
/// @param layer The layer.
/// @return The mask, or nullptr.
[[nodiscard]] const Image* maskOf(const Document& document, const Layer& layer) {
	return layer.mask != 0 && layer.maskEnabled ? document.image(layer.mask) : nullptr;
}

/// @brief Binds a tile of a layer's pixels and of its mask, either of which may be missing.
/// @param layer The layer.
/// @param pixels Its pixels, or nullptr.
/// @param mask Its mask, or nullptr.
/// @param key The tile.
/// @param bind Binds one image's tile to a unit (CanvasSurface::bindTile), and says whether it has one.
/// @return Whether to draw the tile: false when an image present has no such tile.
template <typename Bind>
[[nodiscard]] bool bindLayerTile(const Layer& layer, const Image* pixels, const Image* mask, const TileKey& key,
                                 const Bind& bind) {
	if(pixels != nullptr && !bind(*pixels, layer.pixels, key, canvas::sourceUnit)) return false;
	return mask == nullptr || bind(*mask, layer.mask, key, canvas::maskUnit);
}

/// @brief Binds what a layer draws from, other than its tiles, and says which source it is.
/// @param program The layer program, in use.
/// @param tableTexture The adjustment table's texture.
/// @param layer The layer.
/// @param groupTexture A group's pass (only read for a group).
/// @return canvas::tileSource, passSource or adjustedSource.
[[nodiscard]] int bindSource(unsigned program, unsigned tableTexture, const Layer& layer, unsigned groupTexture) {
	switch(layer.kind) {
	case LayerKind::group:
		bindTexture(canvas::sourceUnit, groupTexture);
		return canvas::passSource;
	case LayerKind::adjustment:
		bindAdjustment(program, tableTexture, layer.adjustment);
		return canvas::adjustedSource;
	default:
		return canvas::tileSource;
	}
}

/// @brief A texture for a tile, from the pool or new, bound to a unit, with the tile's pixels.
/// @param free The pool of the tile's format.
/// @param tile The tile.
/// @param unit The texture unit.
/// @return The texture.
[[nodiscard]] unsigned uploadTile(std::vector<unsigned>& free, const Tile& tile, int unit) {
	const auto [internal, format, type] = glFormatOf(tile.format());
	unsigned texture = 0;
	if(!free.empty()) {
		texture = free.back();
		free.pop_back();
		bindTexture(unit, texture);
	} else {
		glGenTextures(1, &texture);
		bindTexture(unit, texture);
		glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(internal), tileSize, tileSize, 0, format, type, nullptr);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tileSize, tileSize, format, type, tile.bytes().data());
	return texture;
}

/// @brief Takes the residency's evictions: their textures go back to the pool, a few of each format, or are freed.
/// @param pool The pools, by PixelFormat.
/// @param evicted The evictions, emptied.
void recycle(std::array<std::vector<unsigned>, 3>& pool, std::vector<Resident>& evicted) {
	for(const Resident& resident : evicted) {
		if(resident.texture == 0) continue;
		// An evicted texture's format is that of its size; the pool keeps a few of each
		const std::size_t format = resident.bytes == tilePixels ? 2 : resident.bytes == tilePixels * 4 ? 0 : 1;
		std::vector<unsigned>& free = pool.at(format);
		if(free.size() < pooled) free.push_back(resident.texture);
		else glDeleteTextures(1, &resident.texture);
	}
	evicted.clear();
}

} // namespace

std::array<double, 2> CanvasView::toDocument(double surfaceX, double surfaceY) const noexcept {
	return {x + (surfaceX - width * 0.5) / zoom, y + (surfaceY - height * 0.5) / zoom};
}

std::array<double, 2> CanvasView::toSurface(double documentX, double documentY) const noexcept {
	return {(documentX - x) * zoom + width * 0.5, (documentY - y) * zoom + height * 0.5};
}

int canvasLevel(double zoom, int levels) noexcept {
	if(!(zoom < 1.0) || zoom <= 0.0) return 0;
	// Clamped before the cast: a zoom near zero puts the level past any int
	const double level = std::ceil(std::log2(1.0 / zoom) - 1e-9);
	return static_cast<int>(std::clamp(level, 0.0, static_cast<double>(std::max(levels - 1, 0))));
}

void CanvasModel::setDocument(std::unique_ptr<Document> document) {
	document_ = std::move(document);
	++generation_;
}

void CanvasModel::setDisplay(const ColorTransform* transform) {
	constexpr int side = 33;
	displayTable_ = transform != nullptr ? transform->table(side) : std::vector<float>{};
	displaySide_ = transform != nullptr ? side : 0;
	++displayVersion_;
}

/// @brief One frame's region (in its mip level's pixels) and the passes it draws through.
struct CanvasSurface::Region {
	SurfaceFrame& frame; ///< The frame, which owns the passes.
	int level = 0;       ///< The mip level.
	int x = 0;           ///< The region's left.
	int y = 0;           ///< The region's top.
	int width = 0;       ///< Its width.
	int height = 0;      ///< Its height.
	int passWidth = 0;   ///< Its passes' width (rounded up, so panning seldom remakes them).
	int passHeight = 0;  ///< Its passes' height.

	/// @brief A pass of the region's size.
	/// @param name Its name.
	/// @param format Its format.
	[[nodiscard]] SurfacePass pass(const char* name, PassFormat format) const {
		return frame.pass(name, {passWidth, passHeight, format});
	}
	/// @brief A depth's accumulation pass.
	/// @param depth The depth.
	[[nodiscard]] SurfacePass accumulation(std::size_t depth) const {
		return pass(accumulationNames.at(depth), PassFormat::rgba16f);
	}
	/// @brief A depth's clipping base.
	/// @param depth The depth.
	[[nodiscard]] SurfacePass base(std::size_t depth) const { return pass(baseNames.at(depth), PassFormat::rgba8); }
	/// @brief The scratch pass: the backdrop's copy, then the selection.
	[[nodiscard]] SurfacePass scratch() const { return pass("scratch", PassFormat::rgba16f); }

	/// @brief Fits the region to what the view shows of the level, a pixel more each side for the filtering, within
	/// the level and the largest pass.
	/// @param view The view.
	/// @param documentWidth The document's width.
	/// @param documentHeight Its height.
	void cover(const CanvasView& view, int documentWidth, int documentHeight) {
		const double scale = std::ldexp(1.0, level);
		const int levelWidth = (documentWidth + (1 << level) - 1) >> level;
		const int levelHeight = (documentHeight + (1 << level) - 1) >> level;
		const auto [left, top] = view.toDocument(0.0, 0.0);
		const auto [right, bottom] = view.toDocument(frame.width, frame.height);
		x = edgeOf(std::floor(left / scale) - 1.0, levelWidth);
		y = edgeOf(std::floor(top / scale) - 1.0, levelHeight);
		width = std::min(edgeOf(std::ceil(right / scale) + 1.0, levelWidth) - x, SurfaceFrame::maxPassSide);
		height = std::min(edgeOf(std::ceil(bottom / scale) + 1.0, levelHeight) - y, SurfaceFrame::maxPassSide);
		passWidth = std::min((width + 255) / 256 * 256, SurfaceFrame::maxPassSide);
		passHeight = std::min((height + 255) / 256 * 256, SurfaceFrame::maxPassSide);
	}

	/// @brief Readies a layer's target: to blend, copies it to the scratch pass the program reads the backdrop from;
	/// for a clipping base, clears it (where the layer has no tile, its coverage is 0).
	/// @param target The target.
	/// @param blending Whether the layer blends (else it writes its coverage).
	void ready(const SurfacePass& target, bool blending) const {
		if(!blending) {
			clearPass(target);
			return;
		}
		const SurfacePass copy = scratch();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, target.framebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, copy.framebuffer);
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		bindTexture(canvas::backdropUnit, copy.texture);
	}

	/// @brief Draws one triangle over the whole region, into the framebuffer bound.
	/// @param program The layer program, in use.
	void drawWhole(unsigned program) const {
		glUniform2i(at(program, "origin"), 0, 0);
		glViewport(0, 0, width, height);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	/// @brief Draws an image's tiles in the region, the ones bind() could bind, into the framebuffer bound.
	/// @param cells The image whose tiles are drawn.
	/// @param program The layer program, in use.
	/// @param bind Binds a tile's textures, and says whether to draw it.
	template <typename Bind> void drawTiles(const Image& cells, unsigned program, const Bind& bind) const {
		const GLint origin = at(program, "origin");
		const int lastColumn = std::min((x + width - 1) / tileSize, cells.columns(level) - 1);
		const int lastRow = std::min((y + height - 1) / tileSize, cells.rows(level) - 1);
		for(int row = y / tileSize; row <= lastRow; ++row)
			for(int column = x / tileSize; column <= lastColumn; ++column) {
				if(!bind(TileKey{level, column, row})) continue;
				const int originX = column * tileSize - x, originY = row * tileSize - y;
				glUniform2i(origin, originX, originY);
				glViewport(originX, originY, tileSize, tileSize);
				glDrawArrays(GL_TRIANGLES, 0, 3);
			}
	}
};

void CanvasSurface::prepare(const SurfaceSetup& setup) {
	if(vertexArray_ != 0) return;
	glGenVertexArrays(1, &vertexArray_);
	layerProgram_ = canvas::layerProgram(setup.prelude);
	displayProgram_ = canvas::displayProgram(setup.prelude);
	glGenTextures(1, &tableTexture_);
	glBindTexture(GL_TEXTURE_2D, tableTexture_);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glGenTextures(1, &displayTexture_);
	glBindTexture(GL_TEXTURE_3D, displayTexture_); // complete until the table comes, so every sampler has a texture
	constexpr std::array<float, 3> black{};
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F, 1, 1, 1, 0, GL_RGB, GL_FLOAT, black.data());
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	displayVersion_ = 0;
}

void CanvasSurface::release() {
	dropResident();
	for(std::vector<unsigned>& textures : pool_) {
		if(!textures.empty()) glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
		textures.clear();
	}
	glDeleteVertexArrays(1, &vertexArray_);
	glDeleteProgram(layerProgram_);
	glDeleteProgram(displayProgram_);
	glDeleteTextures(1, &tableTexture_);
	glDeleteTextures(1, &displayTexture_);
	vertexArray_ = layerProgram_ = displayProgram_ = tableTexture_ = displayTexture_ = 0;
}

void CanvasSurface::dropResident() {
	residency_.clear(evicted_);
	for(const Resident& resident : evicted_)
		if(resident.texture != 0) glDeleteTextures(1, &resident.texture);
	evicted_.clear();
}

void CanvasSurface::updateDisplay(const CanvasModel& model) {
	if(model.displayVersion() == displayVersion_) return;
	displayVersion_ = model.displayVersion();
	if(model.displayTable().empty()) return;
	const int side = model.displaySide();
	glBindTexture(GL_TEXTURE_3D, displayTexture_);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGB16F, side, side, side, 0, GL_RGB, GL_FLOAT, model.displayTable().data());
	for(const GLenum parameter : {GLenum{GL_TEXTURE_MIN_FILTER}, GLenum{GL_TEXTURE_MAG_FILTER}})
		glTexParameteri(GL_TEXTURE_3D, parameter, GL_LINEAR);
	for(const GLenum parameter : {GLenum{GL_TEXTURE_WRAP_S}, GLenum{GL_TEXTURE_WRAP_T}, GLenum{GL_TEXTURE_WRAP_R}})
		glTexParameteri(GL_TEXTURE_3D, parameter, GL_CLAMP_TO_EDGE);
}

void CanvasSurface::render(SurfaceFrame& frame) {
	const std::shared_ptr<CanvasModel> model = model_.lock();
	if(!model || layerProgram_ == 0) return; // the framebuffer is already transparent
	const json& params = frame.params();
	const CanvasView view = viewOf(params, model->document(), frame);
	const ViewKeeper keeper(*model, params, frame);
	model->view = view;
	forwardPointers(*model, view, frame.input);
	const Document& document = model->document(); // after the tools, which may have changed it
	if(model->generation() != generation_ || residency_.budget() != document.limits().gpuBytes) {
		dropResident();
		residency_ = TileResidency(document.limits().gpuBytes);
		generation_ = model->generation();
	}
	updateDisplay(*model);
	Region region{frame, canvasLevel(view.zoom, document.selection().levels())};
	region.cover(view, document.width(), document.height());
	CanvasStatistics& statistics = model->statistics;
	statistics.level = region.level;
	statistics.regionWidth = std::max(region.width, 0);
	statistics.regionHeight = std::max(region.height, 0);
	++statistics.frames;
	if(region.width <= 0 || region.height <= 0) return;
	glBindVertexArray(vertexArray_);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	for(int unit = canvas::sourceUnit; unit <= canvas::tableUnit; ++unit) bindTexture(unit, tableTexture_); // complete
	clearPass(region.accumulation(0));
	compositeList(document, document.structure().root, region, 0);
	const bool overlays = flagOf(params, "ants", false) || flagOf(params, "quickMask", false);
	clearPass(region.scratch());
	if(overlays && document.hasSelection()) drawSelection(document, region);
	present(frame, region, *model);
	statistics.residentBytes = residency_.bytes();
	statistics.residentTiles = residency_.size();
	statistics.uploads = uploads_;
	statistics.evictions = residency_.evictions();
}

void CanvasSurface::compositeList(const Document& document, const std::vector<std::uint64_t>& list, Region& region,
                                  std::size_t depth) {
	if(depth >= depths) return;
	const bool clipping = clips(document, list);
	if(clipping) clearPass(region.base(depth));
	for(const std::uint64_t id : list) {
		const Layer* layer = document.layer(id);
		if(layer == nullptr) continue;
		if(isBase(*layer, clipping) && !layer->visible) clearPass(region.base(depth)); // clips to nothing
		if(!layer->visible) continue;
		if(layer->kind == LayerKind::group) {
			if(depth + 1 >= depths) continue;
			clearPass(region.accumulation(depth + 1));
			compositeList(document, layer->children, region, depth + 1);
		}
		if(isBase(*layer, clipping)) drawLayer(document, *layer, region, depth, canvas::baseStage);
		drawLayer(document, *layer, region, depth, canvas::blendStage);
	}
}

void CanvasSurface::drawLayer(const Document& document, const Layer& layer, Region& region, std::size_t depth,
                              int stage) {
	const bool blending = stage == canvas::blendStage;
	const SurfacePass target = blending ? region.accumulation(depth) : region.base(depth);
	region.ready(target, blending);
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	glUseProgram(layerProgram_);
	const bool clipped = blending && layer.clipped;
	if(clipped) bindTexture(canvas::baseUnit, region.base(depth).texture);
	setLayerUniforms(layerProgram_, layer, stage, clipped);
	const unsigned group = layer.kind == LayerKind::group ? region.accumulation(depth + 1).texture : 0;
	glUniform1i(at(layerProgram_, "sourceKind"), bindSource(layerProgram_, tableTexture_, layer, group));
	const Image* pixels = layer.kind == LayerKind::pixel ? document.image(layer.pixels) : nullptr;
	const Image* mask = maskOf(document, layer);
	glUniform1i(at(layerProgram_, "maskKind"), mask != nullptr ? canvas::tileMask : canvas::noMask);
	if(pixels == nullptr && mask == nullptr) { // a group or an adjustment, unmasked: one triangle over the region
		region.drawWhole(layerProgram_);
		return;
	}
	if(layer.kind == LayerKind::pixel && pixels == nullptr) return;
	const auto bindOne = [this](const Image& image, std::uint64_t id, const TileKey& key, int unit) {
		return bindTile(image, id, key, unit);
	};
	region.drawTiles(pixels != nullptr ? *pixels : *mask, layerProgram_,
	                 [&](const TileKey& key) { return bindLayerTile(layer, pixels, mask, key, bindOne); });
}

void CanvasSurface::drawSelection(const Document& document, Region& region) {
	const Image& selection = document.selection();
	glBindFramebuffer(GL_FRAMEBUFFER, region.scratch().framebuffer);
	glUseProgram(layerProgram_);
	glUniform1i(at(layerProgram_, "stage"), canvas::coverageStage);
	glUniform1i(at(layerProgram_, "maskKind"), canvas::tileMask);
	region.drawTiles(selection, layerProgram_,
	                 [&](const TileKey& key) { return bindTile(selection, 1, key, canvas::maskUnit); });
}

bool CanvasSurface::bindTile(const Image& image, std::uint64_t id, const TileKey& key, int unit) {
	const ResidentKey residentKey{id, key};
	const std::uint64_t version = image.version(key);
	if(const Resident* found = residency_.find(residentKey); found != nullptr) {
		if(found->version == version) {
			if(found->texture != 0) bindTexture(unit, found->texture);
			return found->texture != 0;
		}
		if(const auto stale = residency_.forget(residentKey); stale && stale->texture != 0)
			pool_.at(static_cast<std::size_t>(image.format())).push_back(stale->texture);
	}
	const std::shared_ptr<const Tile> tile = image.tile(key);
	if(!tile) {
		residency_.admit(residentKey, Resident{0, version, 0}, evicted_); // remembered as transparent
	} else {
		std::vector<unsigned>& free = pool_.at(static_cast<std::size_t>(tile->format()));
		const unsigned texture = uploadTile(free, *tile, unit);
		++uploads_;
		// Over the budget alone, it is drawn now and its texture goes back to the pool
		if(!residency_.admit(residentKey, Resident{texture, version, tile->bytes().size()}, evicted_))
			free.push_back(texture);
	}
	recycle(pool_, evicted_);
	return tile != nullptr;
}

// NOLINTNEXTLINE(readability-make-member-function-const): it draws, and its public declaration stays as it is
void CanvasSurface::present(const SurfaceFrame& frame, const Region& region, const CanvasModel& model) {
	const json& params = frame.params();
	const CanvasView& view = model.view;
	glBindFramebuffer(GL_FRAMEBUFFER, frame.framebuffer);
	glViewport(0, 0, frame.width, frame.height);
	glUseProgram(displayProgram_);
	bindTexture(0, region.accumulation(0).texture);
	bindTexture(1, region.scratch().texture);
	bindTexture(2, displayTexture_, GL_TEXTURE_3D);
	const auto program = displayProgram_;
	glUniform2f(at(program, "size"), static_cast<float>(frame.width), static_cast<float>(frame.height));
	glUniform2f(at(program, "centre"), static_cast<float>(view.x), static_cast<float>(view.y));
	glUniform2f(at(program, "regionOrigin"), static_cast<float>(region.x), static_cast<float>(region.y));
	glUniform2f(at(program, "passSize"), static_cast<float>(region.passWidth), static_cast<float>(region.passHeight));
	glUniform2f(at(program, "documentSize"), static_cast<float>(model.document().width()),
	            static_cast<float>(model.document().height()));
	glUniform1f(at(program, "zoom"), static_cast<float>(view.zoom));
	glUniform1f(at(program, "levelScale"), static_cast<float>(std::ldexp(1.0, region.level)));
	glUniform1f(at(program, "time"), static_cast<float>(std::fmod(frame.time, 3600.0)));
	glUniform1f(at(program, "displaySide"),
	            model.displayTable().empty() ? 0.0f : static_cast<float>(model.displaySide()));
	glUniform1i(at(program, "nearest"), view.zoom >= 2.0 ? 1 : 0);
	glUniform1i(at(program, "checker"), flagOf(params, "checker", true) ? 1 : 0);
	const bool selection = model.document().hasSelection();
	glUniform1i(at(program, "ants"), selection && flagOf(params, "ants", false) ? 1 : 0);
	glUniform1i(at(program, "quickMask"), selection && flagOf(params, "quickMask", false) ? 1 : 0);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glActiveTexture(GL_TEXTURE0);
}

SurfaceFactory canvasSurfaceFactory(std::weak_ptr<CanvasModel> model) {
	return [model = std::move(model)] { return std::make_unique<CanvasSurface>(model); };
}

} // namespace affresco::imaging
