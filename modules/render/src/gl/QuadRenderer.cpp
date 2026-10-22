#include "gl/QuadRenderer.hpp"
#include "PaintRecorder.hpp"
#include "gl/Shader.hpp"
#include "gl/SurfaceState.hpp"

#include "affresco/render/Bitmap.hpp"
#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>

namespace affresco::gl {

namespace {

/// @brief Vertex shader: window units to clip space.
constexpr std::string_view vertexSource = R"glsl(
	uniform vec2 uScale; // (2 / width, -2 / height): window units -> clip space, y pointing down

	layout(location = 0) in vec2 aPosition;
	layout(location = 1) in vec2 aTexCoord;
	layout(location = 2) in vec4 aColor;
	layout(location = 3) in vec2 aInside;
	layout(location = 4) in vec4 aShape; // radius, outline, blur (in sixteenths), and the kind of quad

	out vec2 vTexCoord;
	out vec4 vColor;
	out vec2 vInside;
	flat out vec4 vShape;

	void main() {
		vTexCoord = aTexCoord;
		vColor = aColor;
		vInside = aInside;
		vShape = vec4(aShape.xyz / 16.0, aShape.w);
		gl_Position = vec4(aPosition * uScale + vec2(-1.0, 1.0), 0.0, 1.0);
	}
)glsl";

/// @brief Fragment shader for frames without shapes. The texture holds coverage: 1 for the white texel (solid
/// rectangles), the glyph's shape for text. It reads none of the shape's inputs: some drivers (Mesa's llvmpipe) round
/// text differently as soon as a fragment shader reads more of them, so frames without shapes keep their exact pixels.
constexpr std::string_view plainFragmentSource = R"glsl(
	uniform sampler2D uAtlas;

	in vec2 vTexCoord;
	in vec4 vColor;
	out vec4 fColor;

	void main() {
		fColor = vec4(vColor.rgb, vColor.a * texture(uAtlas, vTexCoord).r);
	}
)glsl";

/// @brief The inputs of the fragment shaders with shapes or colour glyphs. Those with shapes are as
/// plainFragmentSource, and shapes multiply the coverage by the share of the pixel inside their edge.
constexpr std::string_view shapeInputs = R"glsl(
	uniform sampler2D uAtlas;

	in vec2 vTexCoord;
	in vec4 vColor;
	in vec2 vInside;      // distance inside the nearest vertical and horizontal edges
	flat in vec4 vShape;  // the corner's radius, the outline's width (0: filled), the blur (in window units), the kind
	out vec4 fColor;
)glsl";

/// @brief The shape functions of the fragment shaders with shapes.
constexpr std::string_view shapeFunctions = R"glsl(

	// erf, approximated (Abramowitz and Stegun 7.1.27, error under 5e-4): the share of a Gaussian blur inside an edge
	float approximateErf(float x) {
		float a = abs(x);
		float t = 1.0 + (0.278393 + (0.230389 + 0.078108 * (a * a)) * a) * a;
		t *= t;
		return sign(x) * (1.0 - 1.0 / (t * t));
	}

	// The share of this pixel inside the shape
	float coverage() {
		float radius = vShape.x, outline = vShape.y, blur = vShape.z;

		// Signed distance to the rounded edge, negative inside: the nearest two edges and their corner decide it
		vec2 corner = radius - vInside;
		float distance = length(max(corner, 0.0)) + min(max(corner.x, corner.y), 0.0) - radius;
		distance = outline > 0.0 ? max(distance, -distance - outline) : distance; // an outline: a band inside the edge

		float pixel = max(length(vec2(dFdx(distance), dFdy(distance))), 1e-4); // window units per pixel, across the edge
		if(blur > 0.0) return 0.5 - 0.5 * approximateErf(distance * 1.41421356 / blur); // sigma = blur / 2, like CSS
		return clamp(0.5 - distance / pixel, 0.0, 1.0);
	}
)glsl";

/// @brief The main of the fragment shader with shapes.
constexpr std::string_view shapeMain = R"glsl(
	void main() {
		float alpha = vColor.a * texture(uAtlas, vTexCoord).r;
		// The same for a whole quad, so rectangles and text skip the shape's work
		if(vShape.w > 0.0) alpha *= coverage();
		fColor = vec4(vColor.rgb, alpha);
	}
)glsl";

/// @brief The colour page of the fragment shaders with colour glyphs, and what they do first: a colour glyph's pixel is
/// the page's colour, with the text's opacity.
constexpr std::string_view colorFunctions = R"glsl(
	uniform sampler2D uColorAtlas; // the batch's colour page: premultiplied RGBA

	bool colorGlyph() {
		if(vShape.w < 1.5) return false;
		vec4 texel = texture(uColorAtlas, vTexCoord);
		fColor = vec4(texel.rgb / max(texel.a, 1.0 / 255.0), texel.a * vColor.a);
		return true;
	}
)glsl";

/// @brief The main of the fragment shader with colour glyphs and no shapes.
constexpr std::string_view colorMain = R"glsl(
	void main() {
		if(colorGlyph()) return;
		fColor = vec4(vColor.rgb, vColor.a * texture(uAtlas, vTexCoord).r);
	}
)glsl";

/// @brief The main of the fragment shader with colour glyphs and shapes.
constexpr std::string_view shapeColorMain = R"glsl(
	void main() {
		if(colorGlyph()) return;
		float alpha = vColor.a * texture(uAtlas, vTexCoord).r;
		if(vShape.w > 0.0) alpha *= coverage();
		fColor = vec4(vColor.rgb, alpha);
	}
)glsl";

/// @brief Joins the parts of a shader.
/// @param parts Its parts, in order.
/// @return Its source.
std::string join(std::initializer_list<std::string_view> parts) {
	std::string source;
	for(const std::string_view part : parts) source += part;
	return source;
}

/// @brief Links vertexSource with a fragment shader, each after its shaderPrelude().
/// @param fragmentParts The fragment shader's parts, in order.
/// @return The program.
Program linkQuadProgram(std::initializer_list<std::string_view> fragmentParts) {
	std::string fragmentSource(shaderPrelude(ShaderStage::fragment));
	for(const std::string_view part : fragmentParts) fragmentSource += part;
	return linkProgram(join({shaderPrelude(ShaderStage::vertex), vertexSource}), fragmentSource);
}

constexpr std::size_t indicesPerQuad = 6; ///< Two triangles.

/// @brief Converts to a normalized 16-bit integer.
/// @param value In [0, 1]; clamped.
/// @return In [0, 65535].
std::uint16_t toUnorm16(float value) noexcept {
	return static_cast<std::uint16_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f));
}

} // namespace

QuadRenderer::QuadRenderer()
    : shaders_{Shader{linkQuadProgram({plainFragmentSource})},
	           Shader{linkQuadProgram({shapeInputs, shapeFunctions, shapeMain})},
	           Shader{linkQuadProgram({shapeInputs, colorFunctions, colorMain})},
	           Shader{linkQuadProgram({shapeInputs, shapeFunctions, colorFunctions, shapeColorMain})}},
      vertexArray_(VertexArray::create()), vertexBuffer_(Buffer::create()), indexBuffer_(Buffer::create()) {
	for(Shader& shader : shaders_) {
		shader.scaleLocation = glGetUniformLocation(shader.program.id(), "uScale");
		glUseProgram(shader.program.id());
		glUniform1i(glGetUniformLocation(shader.program.id(), "uAtlas"), 0);
		if(const GLint colorAtlas = glGetUniformLocation(shader.program.id(), "uColorAtlas"); colorAtlas >= 0)
			glUniform1i(colorAtlas, 1);
	}

	static constexpr std::array<std::uint8_t, 1> white{255};
	whiteTexture_ = createTexture(1, white);

	// The vertex layout and the index buffer binding are stored in the VAO once, then reused every frame
	glBindVertexArray(vertexArray_.id());

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.id());
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(detail::Vertex),
	                      reinterpret_cast<const void*>(offsetof(detail::Vertex, x)));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(detail::Vertex),
	                      reinterpret_cast<const void*>(offsetof(detail::Vertex, u)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(detail::Vertex),
	                      reinterpret_cast<const void*>(offsetof(detail::Vertex, r)));
	glEnableVertexAttribArray(2);
	glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(detail::Vertex),
	                      reinterpret_cast<const void*>(offsetof(detail::Vertex, insideX)));
	glEnableVertexAttribArray(3);
	glVertexAttribPointer(4, 4, GL_UNSIGNED_SHORT, GL_FALSE, sizeof(detail::Vertex),
	                      reinterpret_cast<const void*>(offsetof(detail::Vertex, radius)));
	glEnableVertexAttribArray(4);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_.id());

	glBindVertexArray(0);
}

void QuadRenderer::startFrame(const detail::Frame& frame) {
	std::array<GLint, 4> viewport{};
	glGetIntegerv(GL_VIEWPORT, viewport.data());
	framebufferWidth_ = viewport[2];
	framebufferHeight_ = viewport[3];
	frameWidth_ = frame.width;
	frameHeight_ = frame.height;
	pixelScale_ = frameWidth_ > 0 ? static_cast<float>(framebufferWidth_) / static_cast<float>(frameWidth_) : 1.0f;
	vertices_.clear();
	batches_.clear();
	drawCalls_ = 0;
	clipDepth_ = 0;
	std::erase_if(fontTextures_, [](const auto& entry) { return entry.second.font.expired(); });
	images_.collect();
	surfaces_.collect();
}

void QuadRenderer::draw(const detail::Frame& frame, const std::optional<Rect>& region, bool shapes, bool color) {
	startFrame(frame);
	region_ = region;
	replayTree(frame.root);
	for(const Panel* overlay : frame.overlays) replayTree(*overlay);
	if(frame.tooltip) replayTree(*frame.tooltip);
	if(!region_) subtrees_.collect(); // a region's frame may have skipped cached subtrees that are still there
	region_.reset();
	submit(frame.width, frame.height, shapes, color);
}

void QuadRenderer::drawFromScratch(const detail::Frame& frame) {
	startFrame(frame);
	scratchShapes_ = scratchColor_ = false;
	const Rect window{0.0f, 0.0f, static_cast<float>(frame.width), static_cast<float>(frame.height)};
	scratchTree(frame.root, window, 0, {});
	for(const Panel* overlay : frame.overlays) scratchTree(*overlay, window, 0, {});
	if(frame.tooltip) scratchTree(*frame.tooltip, window, 0, {});
	submit(frame.width, frame.height, scratchShapes_, scratchColor_);
}

void QuadRenderer::draw(Panel& root, std::span<Panel* const> overlays, Panel* tooltip, int width, int height) {
	// A generation of its own, so every panel is recorded again: this renderer may not be the window's
	const detail::Frame frame{root, overlays, tooltip, width, height, 0, ++ownGeneration_, std::nullopt, true};
	const detail::Refreshed refreshed = refresh(frame);
	draw(frame, std::nullopt, refreshed.shapes, refreshed.color);
}

// One program for the whole frame, so shapes and colour glyphs still never break a batch
void QuadRenderer::submit(int width, int height, bool shapes, bool color) {
	const std::size_t quads = vertices_.size() / detail::verticesPerQuad;
	if(quads == 0) return;

	Shader& shader = shaders_[(shapes ? 1u : 0u) + (color ? 2u : 0u)];
	glUseProgram(shader.program.id());
	if(width != shader.width || height != shader.height) {
		glUniform2f(shader.scaleLocation, 2.0f / static_cast<float>(width), -2.0f / static_cast<float>(height));
		shader.width = width;
		shader.height = height;
	}

	bool clips = false;
	for(const Batch& batch : batches_) {
		if(batch.font) updateFontTexture(*batch.font, batch.page, AtlasKind::coverage);
		if(batch.colorFont) updateFontTexture(*batch.colorFont, batch.colorPage, AtlasKind::color);
		clips = clips || batch.clip != ClipStep::none;
	}

	glBindVertexArray(vertexArray_.id());
	reserveQuads(quads);

	// Re-specifying the whole store lets the driver orphan the old one instead of waiting for the GPU to finish with it
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.id());
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(detail::Vertex)), vertices_.data(),
	             GL_STREAM_DRAW);

	if(clips) stencil_.start();
	drawBatches(color);
	if(clips) stencil_.finish();
	glBindVertexArray(0);
}

void QuadRenderer::drawBatches(bool color) {
	glActiveTexture(GL_TEXTURE0);
	for(const Batch& batch : batches_) {
		if(batch.quadCount == 0) continue;
		if(color) {
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, batch.colorTexture != 0 ? batch.colorTexture : whiteTexture_.id());
			glActiveTexture(GL_TEXTURE0);
		}
		glBindTexture(GL_TEXTURE_2D, batch.texture);
		if(batch.clip != ClipStep::none) stencil_.beginStep(batch.clip);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.quadCount * indicesPerQuad), GL_UNSIGNED_INT,
		               reinterpret_cast<const void*>(batch.firstQuad * indicesPerQuad * sizeof(GLuint)));
		if(batch.clip != ClipStep::none) stencil_.endStep(batch.clip);
		++drawCalls_;
	}
}

// Pre-order traversal: parents first, so children (and later siblings) are drawn on top. A subtree outside the region
// draws nothing there: its children are clipped to its bounds, and its shadow is in its record's extent
void QuadRenderer::replayTree(const Panel& panel) {
	if(!panel.isShown()) return; // hidden panels hide their whole subtree
	const detail::PanelGeometry* geometry = panel.geometry_.get();
	if(!geometry || !geometry->recorded) return;
	if(region_ && !detail::overlaps(*region_, detail::screenReachOf(*geometry))) return;
	// A texture holds a subtree as laid out: one drawn transformed, or holding transforms, is drawn directly
	const bool plain = geometry->world.isIdentity() && !geometry->subtreeTransformed;
	if(geometry->cached && plain && replayCached(panel, *geometry)) return;
	replayContent(panel, geometry->record);
}

void QuadRenderer::replayContent(const Panel& panel, const detail::PaintRecord& record) {
	const Affine2 world = panel.geometry_->world;
	world_ = world;
	replay(record, 0, record.overStart);
	for(const auto& child : panel.children()) {
		if(child->isOverlay()) continue; // overlays are drawn after the whole tree
		const bool clipped = clipChild(panel, *child, world, ClipStep::push);
		replayTree(*child);
		if(clipped) clipChild(panel, *child, world, ClipStep::pop);
	}
	world_ = world;
	replay(record, record.overStart, record.ops.size());
}

void QuadRenderer::scratchTree(const Panel& panel, const Rect& outerClip, std::size_t depth,
                               const Affine2& parentWorld) {
	if(!panel.isShown()) return;
	if(scratch_.size() <= depth) scratch_.emplace_back();
	detail::PaintRecord& record = scratch_[depth];
	record.clear();
	const Affine2 world = panel.transform().isIdentity() ? parentWorld : parentWorld * panel.localTransform();
	detail::PaintRecorder recorder(record, panel.bounds(), outerClip);
	if(!world.isAxisAligned()) recorder.smoothEdges(panel.frame());
	panel.paint(recorder);
	record.overStart = record.ops.size();
	panel.paintOver(recorder);
	scratchShapes_ = scratchShapes_ || record.shapes;
	scratchColor_ = scratchColor_ || record.color;

	world_ = world;
	replay(record, 0, record.overStart);
	for(const auto& child : panel.children()) {
		if(child->isOverlay()) continue;
		const bool clipped = clipChild(panel, *child, world, ClipStep::push);
		const std::optional<Affine2> inverse =
		    child->transform().isIdentity() ? std::nullopt : child->localTransform().inverse();
		scratchTree(*child, inverse ? inverse->boundingBox(panel.bounds()) : panel.bounds(), depth + 1, world);
		if(clipped) clipChild(panel, *child, world, ClipStep::pop);
	}
	world_ = world;
	replay(record, record.overStart, record.ops.size());
}

// The clip is one white quad in a batch of its own, then the batch in use carries on in a new one
bool QuadRenderer::clipChild(const Panel& parent, const Panel& child, const Affine2& parentWorld, ClipStep step) {
	if(!child.isShown() || child.transform().isAxisAligned()) return false;
	if(step == ClipStep::push && clipDepth_ >= maxClipDepth) return false;
	if(batches_.empty()) useTexture(whiteTexture_.id(), {0.5f, 0.5f});
	Batch resume = batches_.back();
	const std::size_t first = vertices_.size() / detail::verticesPerQuad;
	batches_.push_back({whiteTexture_.id(), {0.5f, 0.5f}, first, 1});
	batches_.back().clip = step;
	const Rect& bounds = parent.bounds();
	const std::uint16_t half = toUnorm16(0.5f);
	for(const Vec2 corner : {Vec2{bounds.left, bounds.top}, Vec2{bounds.right, bounds.top},
	                         Vec2{bounds.right, bounds.bottom}, Vec2{bounds.left, bounds.bottom}}) {
		const Vec2 at = parentWorld.apply(corner);
		vertices_.push_back({at.x, at.y, half, half, 255, 255, 255, 255, 0.0f, 0.0f, 0, 0, 0, 0});
	}
	resume.firstQuad = first + 1;
	resume.quadCount = 0;
	batches_.push_back(resume);
	clipDepth_ = step == ClipStep::push ? clipDepth_ + 1 : clipDepth_ - 1;
	return true;
}

// The steps change the batches as drawing directly did: fills take the white texel of the texture in use, and text
// carries on with its font's page in use
void QuadRenderer::replay(const detail::PaintRecord& record, std::size_t first, std::size_t last) {
	using detail::PaintStep;
	for(std::size_t i = first; i < last; ++i) {
		const detail::PaintOp& op = record.ops[i];
		switch(op.step) {
		case PaintStep::fill:
			if(batches_.empty()) useTexture(whiteTexture_.id(), {0.5f, 0.5f});
			addQuads(record, op, true);
			break;
		case PaintStep::text: {
			const bool sameFont = !batches_.empty() && batches_.back().font == op.font;
			textPage_ = sameFont ? batches_.back().page : 0;
			useFontPage(*op.font, textPage_);
			break;
		}
		case PaintStep::glyphs:
			if(op.page != textPage_) useFontPage(*op.font, textPage_ = op.page);
			addQuads(record, op, false);
			break;
		case PaintStep::colorGlyph:
			useColorTexture(fontTexture(*op.font, op.page, AtlasKind::color), op.font, op.page);
			addQuads(record, op, false);
			break;
		case PaintStep::image: {
			if(batches_.empty()) useTexture(whiteTexture_.id(), {0.5f, 0.5f});
			const ImageTextures::Placement placed = images_.place(*op.bitmap);
			useColorTexture(placed.texture, nullptr, 0);
			addQuads(record, op, false, &placed.uv);
			break;
		}
		case PaintStep::surface: { // drawn into its framebuffer first if it changed; nothing if it failed
			// At this frame's pixels per unit, which a picture at another scale doesn't share with the display
			op.surface->state()->scale = pixelScale_;
			const GLuint texture = surfaces_.place(op.surface->state());
			if(texture == 0) break;
			if(batches_.empty()) useTexture(whiteTexture_.id(), {0.5f, 0.5f});
			useColorTexture(texture, nullptr, 0);
			addQuads(record, op, false);
			break;
		}
		}
	}
}

// An image's quads hold coordinates within the image; its place in its texture is known only now
void QuadRenderer::addQuads(const detail::PaintRecord& record, const detail::PaintOp& op, bool white,
                            const Rect* within) {
	const Vec2 texel = batches_.back().white;
	const std::uint16_t u = toUnorm16(texel.x), v = toUnorm16(texel.y);
	const bool transformed = !world_.isIdentity();
	std::array<detail::Vertex, detail::verticesPerQuad> placed{};
	for(std::uint32_t quad = op.firstQuad; quad < op.firstQuad + op.quads; ++quad) {
		auto corners = std::span(record.vertices).subspan(quad * detail::verticesPerQuad, detail::verticesPerQuad);
		if(transformed) { // the shapes' distances stay in the panel's units: the shader measures them per pixel
			std::ranges::copy(corners, placed.begin());
			for(detail::Vertex& vertex : placed) {
				const Vec2 at = world_.apply({vertex.x, vertex.y});
				vertex.x = at.x;
				vertex.y = at.y;
			}
			corners = placed;
		}
		if(region_ && !detail::overlaps(*region_, detail::boundsOf(corners))) continue;
		for(detail::Vertex vertex : corners) {
			if(white) {
				vertex.u = u;
				vertex.v = v;
			} else if(within) {
				vertex.u = toUnorm16(within->left + within->width() * static_cast<float>(vertex.u) / 65535.0f);
				vertex.v = toUnorm16(within->top + within->height() * static_cast<float>(vertex.v) / 65535.0f);
			}
			vertices_.push_back(vertex);
		}
		++batches_.back().quadCount;
	}
}

void QuadRenderer::useFontPage(const Font& font, std::uint32_t page) {
	useTexture(fontTexture(font, page, AtlasKind::coverage), font.whiteTexel(page));
	batches_.back().font = &font;
	batches_.back().page = page;
}

void QuadRenderer::useColorTexture(GLuint texture, const Font* font, std::uint32_t page) {
	if(batches_.back().colorTexture == texture) return;
	if(batches_.back().colorTexture != 0) { // another colour texture: the same coverage page, in a new batch
		const Batch current = batches_.back();
		batches_.push_back({current.texture, current.white, vertices_.size() / detail::verticesPerQuad, 0, current.font,
		                    current.page});
	}
	batches_.back().colorTexture = texture;
	batches_.back().colorFont = font;
	batches_.back().colorPage = page;
}

void QuadRenderer::useTexture(GLuint texture, Vec2 white) {
	if(!batches_.empty() && batches_.back().texture == texture) return;
	batches_.push_back({texture, white, vertices_.size() / detail::verticesPerQuad, 0});
}

GLuint QuadRenderer::fontTexture(const Font& font, std::uint32_t page, AtlasKind kind) {
	FontTextures& textures = fontTextures_[font.id()];
	if(textures.font.expired()) textures.font = font.weak_from_this();
	std::vector<PageTexture>& pages = textures.pages[static_cast<std::size_t>(kind)];
	while(pages.size() <= page) {
		const std::size_t next = pages.size();
		pages.push_back({createTexture(font.atlasSize(next, kind), font.atlasPixels(next, kind), kind),
		                 font.atlasVersion(next, kind)});
	}
	return pages[page].texture.id();
}

void QuadRenderer::updateFontTexture(const Font& font, std::uint32_t page, AtlasKind kind) {
	PageTexture& texture = fontTextures_.at(font.id()).pages[static_cast<std::size_t>(kind)][page];
	const std::uint64_t version = font.atlasVersion(page, kind);
	if(texture.version == version) return;

	const Font::AtlasRegion changed = font.atlasChangesSince(page, texture.version, kind);
	texture.version = version;
	if(changed.empty()) return;

	// Only the rectangle holding the new glyphs, read out of the whole page's rows
	const int size = font.atlasSize(page, kind);
	const std::size_t texelBytes = kind == AtlasKind::color ? 4 : 1;
	const std::size_t firstTexel =
	    static_cast<std::size_t>(changed.top) * static_cast<std::size_t>(size) + static_cast<std::size_t>(changed.left);
	glBindTexture(GL_TEXTURE_2D, texture.texture.id());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, size);
	glTexSubImage2D(GL_TEXTURE_2D, 0, changed.left, changed.top, changed.right - changed.left,
	                changed.bottom - changed.top, kind == AtlasKind::color ? GL_RGBA : GL_RED, GL_UNSIGNED_BYTE,
	                font.atlasPixels(page, kind).data() + firstTexel * texelBytes);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

Texture QuadRenderer::createTexture(int size, std::span<const std::uint8_t> pixels, AtlasKind kind) {
	Texture texture = Texture::create();
	glBindTexture(GL_TEXTURE_2D, texture.id());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	if(kind == AtlasKind::color)
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
	else glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	return texture;
}

// The index pattern never changes (0 1 2, 2 3 0 per quad), so it is generated only when the batch outgrows it
void QuadRenderer::reserveQuads(std::size_t quads) {
	if(quads <= quadCapacity_) return;

	quadCapacity_ = std::max<std::size_t>({quads, quadCapacity_ * 2, 64});

	std::vector<GLuint> indices;
	indices.reserve(quadCapacity_ * indicesPerQuad);
	for(std::size_t q = 0; q < quadCapacity_; ++q) {
		const auto v = static_cast<GLuint>(q * detail::verticesPerQuad);
		indices.insert(indices.end(), {v, v + 1, v + 2, v + 2, v + 3, v});
	}

	// GL_ELEMENT_ARRAY_BUFFER is VAO state: the caller has the VAO bound
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)), indices.data(),
	             GL_STATIC_DRAW);
}

} // namespace affresco::gl
