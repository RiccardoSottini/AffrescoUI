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

} // namespace

std::array<double, 2> CanvasView::toDocument(double surfaceX, double surfaceY) const noexcept {
	return {x + (surfaceX - width * 0.5) / zoom, y + (surfaceY - height * 0.5) / zoom};
}

std::array<double, 2> CanvasView::toSurface(double documentX, double documentY) const noexcept {
	return {(documentX - x) * zoom + width * 0.5, (documentY - y) * zoom + height * 0.5};
}

int canvasLevel(double zoom, int levels) noexcept {
	if(!(zoom < 1.0) || zoom <= 0.0) return 0;
	const int level = static_cast<int>(std::ceil(std::log2(1.0 / zoom) - 1e-9));
	return std::clamp(level, 0, std::max(levels - 1, 0));
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
	CanvasView view{std::clamp(numberOf(params, "zoom", 1.0), 1.0 / 1024.0, 256.0),
	                numberOf(params, "x", model->document().width() * 0.5),
	                numberOf(params, "y", model->document().height() * 0.5), frame.width, frame.height};
	model->view = view;
	for(const SurfacePointer& event : frame.input)
		if(model->onPointer) {
			const auto [x, y] = view.toDocument(event.position.x, event.position.y);
			model->onPointer(CanvasPointer{event, x, y});
		}
	const Document& document = model->document(); // after the tools, which may have changed it
	if(model->generation() != generation_ || residency_.budget() != document.limits().gpuBytes) {
		dropResident();
		residency_ = TileResidency(document.limits().gpuBytes);
		generation_ = model->generation();
	}
	updateDisplay(*model);
	const int level = canvasLevel(view.zoom, document.selection().levels());
	const double scale = std::ldexp(1.0, level);
	const int levelWidth = (document.width() + (1 << level) - 1) >> level;
	const int levelHeight = (document.height() + (1 << level) - 1) >> level;
	const auto [left, top] = view.toDocument(0.0, 0.0);
	const auto [right, bottom] = view.toDocument(frame.width, frame.height);
	Region region{frame, level};
	region.x = std::clamp(static_cast<int>(std::floor(left / scale)) - 1, 0, levelWidth);
	region.y = std::clamp(static_cast<int>(std::floor(top / scale)) - 1, 0, levelHeight);
	region.width = std::min(std::clamp(static_cast<int>(std::ceil(right / scale)) + 1, 0, levelWidth) - region.x,
	                        SurfaceFrame::maxPassSide);
	region.height = std::min(std::clamp(static_cast<int>(std::ceil(bottom / scale)) + 1, 0, levelHeight) - region.y,
	                         SurfaceFrame::maxPassSide);
	CanvasStatistics& statistics = model->statistics;
	statistics.level = level;
	statistics.regionWidth = std::max(region.width, 0);
	statistics.regionHeight = std::max(region.height, 0);
	++statistics.frames;
	if(region.width <= 0 || region.height <= 0) return;
	region.passWidth = std::min((region.width + 255) / 256 * 256, SurfaceFrame::maxPassSide);
	region.passHeight = std::min((region.height + 255) / 256 * 256, SurfaceFrame::maxPassSide);
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
	const bool clipping = std::any_of(list.begin(), list.end(), [&](std::uint64_t id) {
		const Layer* layer = document.layer(id);
		return layer != nullptr && layer->clipped && layer->visible;
	});
	if(clipping) clearPass(region.base(depth));
	for(const std::uint64_t id : list) {
		const Layer* layer = document.layer(id);
		if(layer == nullptr) continue;
		if(!layer->clipped && !layer->visible && clipping) clearPass(region.base(depth));
		if(!layer->visible) continue;
		if(layer->kind == LayerKind::group) {
			if(depth + 1 >= depths) continue;
			clearPass(region.accumulation(depth + 1));
			compositeList(document, layer->children, region, depth + 1);
		}
		if(clipping && !layer->clipped) drawLayer(document, *layer, region, depth, canvas::baseStage);
		drawLayer(document, *layer, region, depth, canvas::blendStage);
	}
}

void CanvasSurface::drawLayer(const Document& document, const Layer& layer, Region& region, std::size_t depth,
                              int stage) {
	const SurfacePass target = stage == canvas::blendStage ? region.accumulation(depth) : region.base(depth);
	if(stage == canvas::blendStage) {
		const SurfacePass scratch = region.scratch();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, target.framebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, scratch.framebuffer);
		glBlitFramebuffer(0, 0, region.width, region.height, 0, 0, region.width, region.height, GL_COLOR_BUFFER_BIT,
		                  GL_NEAREST);
		bindTexture(canvas::backdropUnit, scratch.texture);
	} else {
		clearPass(target); // where it has no tile, its coverage is 0
	}
	glBindFramebuffer(GL_FRAMEBUFFER, target.framebuffer);
	glUseProgram(layerProgram_);
	const bool clipped = stage == canvas::blendStage && layer.clipped;
	if(clipped) bindTexture(canvas::baseUnit, region.base(depth).texture);
	glUniform1i(at(layerProgram_, "stage"), stage);
	glUniform1i(at(layerProgram_, "clipped"), clipped ? 1 : 0);
	glUniform1i(at(layerProgram_, "mode"), static_cast<int>(layer.blend));
	glUniform1f(at(layerProgram_, "opacity"), layer.opacity);
	int sourceKind = canvas::tileSource;
	if(layer.kind == LayerKind::group) {
		sourceKind = canvas::passSource;
		bindTexture(canvas::sourceUnit, region.accumulation(depth + 1).texture);
	} else if(layer.kind == LayerKind::adjustment) {
		sourceKind = canvas::adjustedSource;
		const auto table = adjustmentTable(layer.adjustment);
		bindTexture(canvas::tableUnit, tableTexture_);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, table.data());
		// A threshold compares the luminosity itself: through the table, a half step below the level would flip
		const bool threshold = layer.adjustment.kind == AdjustmentKind::threshold;
		glUniform1i(at(layerProgram_, "adjustKind"),
		            threshold ? canvas::thresholdAdjust : static_cast<int>(adjustmentShader(layer.adjustment)));
		glUniform1f(at(layerProgram_, "level"), layer.adjustment.level);
		glUniform3f(at(layerProgram_, "hsl"), layer.adjustment.hue, layer.adjustment.saturation,
		            layer.adjustment.lightness);
	}
	glUniform1i(at(layerProgram_, "sourceKind"), sourceKind);
	const Image* pixels = layer.kind == LayerKind::pixel ? document.image(layer.pixels) : nullptr;
	const Image* mask = layer.mask != 0 && layer.maskEnabled ? document.image(layer.mask) : nullptr;
	glUniform1i(at(layerProgram_, "maskKind"), mask != nullptr ? canvas::tileMask : canvas::noMask);
	if(pixels == nullptr && mask == nullptr) {
		glUniform2i(at(layerProgram_, "origin"), 0, 0);
		glViewport(0, 0, region.width, region.height);
		glDrawArrays(GL_TRIANGLES, 0, 3);
		return;
	}
	if(layer.kind == LayerKind::pixel && pixels == nullptr) return;
	const Image& cells = pixels != nullptr ? *pixels : *mask;
	const int lastColumn = std::min((region.x + region.width - 1) / tileSize, cells.columns(region.level) - 1);
	const int lastRow = std::min((region.y + region.height - 1) / tileSize, cells.rows(region.level) - 1);
	for(int row = region.y / tileSize; row <= lastRow; ++row)
		for(int column = region.x / tileSize; column <= lastColumn; ++column) {
			const TileKey key{region.level, column, row};
			if(pixels != nullptr && !bindTile(*pixels, layer.pixels, key, canvas::sourceUnit)) continue;
			if(mask != nullptr && !bindTile(*mask, layer.mask, key, canvas::maskUnit)) continue;
			const int originX = column * tileSize - region.x, originY = row * tileSize - region.y;
			glUseProgram(layerProgram_);
			glUniform2i(at(layerProgram_, "origin"), originX, originY);
			glViewport(originX, originY, tileSize, tileSize);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
}

void CanvasSurface::drawSelection(const Document& document, Region& region) {
	const Image& selection = document.selection();
	glBindFramebuffer(GL_FRAMEBUFFER, region.scratch().framebuffer);
	glUseProgram(layerProgram_);
	glUniform1i(at(layerProgram_, "stage"), canvas::coverageStage);
	glUniform1i(at(layerProgram_, "maskKind"), canvas::tileMask);
	const int lastColumn = std::min((region.x + region.width - 1) / tileSize, selection.columns(region.level) - 1);
	const int lastRow = std::min((region.y + region.height - 1) / tileSize, selection.rows(region.level) - 1);
	for(int row = region.y / tileSize; row <= lastRow; ++row)
		for(int column = region.x / tileSize; column <= lastColumn; ++column) {
			if(!bindTile(selection, 1, {region.level, column, row}, canvas::maskUnit)) continue;
			const int originX = column * tileSize - region.x, originY = row * tileSize - region.y;
			glUniform2i(at(layerProgram_, "origin"), originX, originY);
			glViewport(originX, originY, tileSize, tileSize);
			glDrawArrays(GL_TRIANGLES, 0, 3);
		}
}

bool CanvasSurface::bindTile(const Image& image, std::uint64_t id, const TileKey& key, int unit) {
	const ResidentKey residentKey{id, key};
	const std::uint64_t version = image.version(key);
	if(Resident* found = residency_.find(residentKey); found != nullptr) {
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
		const auto [internal, format, type] = glFormatOf(tile->format());
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
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tileSize, tileSize, format, type, tile->bytes().data());
		++uploads_;
		// Over the budget alone, it is drawn now and its texture goes back to the pool
		if(!residency_.admit(residentKey, Resident{texture, version, tile->bytes().size()}, evicted_))
			free.push_back(texture);
	}
	for(const Resident& resident : evicted_) {
		if(resident.texture == 0) continue;
		// An evicted texture's format is that of its size; the pool keeps a few of each
		const std::size_t format = resident.bytes == tilePixels ? 2 : resident.bytes == tilePixels * 4 ? 0 : 1;
		std::vector<unsigned>& free = pool_.at(format);
		if(free.size() < pooled) free.push_back(resident.texture);
		else glDeleteTextures(1, &resident.texture);
	}
	evicted_.clear();
	return tile != nullptr;
}

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
