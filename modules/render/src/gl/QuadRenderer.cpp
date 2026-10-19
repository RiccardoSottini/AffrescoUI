#include "gl/QuadRenderer.hpp"
#include "gl/Shader.hpp"

#include "affresco/render/Font.hpp"
#include "affresco/render/Panel.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace affresco::gl {

namespace {

/// @brief Vertex shader: window units to clip space.
constexpr std::string_view vertexSource = R"glsl(
	#version 410 core

	uniform vec2 uScale; // (2 / width, -2 / height): window units -> clip space, y pointing down

	layout(location = 0) in vec2 aPosition;
	layout(location = 1) in vec2 aTexCoord;
	layout(location = 2) in vec4 aColor;

	out vec2 vTexCoord;
	out vec4 vColor;

	void main() {
		vTexCoord = aTexCoord;
		vColor = aColor;
		gl_Position = vec4(aPosition * uScale + vec2(-1.0, 1.0), 0.0, 1.0);
	}
)glsl";

/// @brief Fragment shader. The texture holds coverage: 1 for the white texel (solid rectangles), the glyph's shape
/// for text.
constexpr std::string_view fragmentSource = R"glsl(
	#version 410 core

	uniform sampler2D uAtlas;

	in vec2 vTexCoord;
	in vec4 vColor;
	out vec4 fColor;

	void main() {
		fColor = vec4(vColor.rgb, vColor.a * texture(uAtlas, vTexCoord).r);
	}
)glsl";

constexpr std::size_t verticesPerQuad = 4; ///< Corners.
constexpr std::size_t indicesPerQuad = 6;  ///< Two triangles.

/// @brief Converts to a normalized 16-bit integer.
/// @param value In [0, 1]; clamped.
/// @return In [0, 65535].
std::uint16_t toUnorm16(float value) noexcept {
	return static_cast<std::uint16_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * 65535.0f));
}

} // namespace

QuadRenderer::QuadRenderer()
    : program_(linkProgram(vertexSource, fragmentSource)), vertexArray_(VertexArray::create()),
      vertexBuffer_(Buffer::create()), indexBuffer_(Buffer::create()) {
	scaleLocation_ = glGetUniformLocation(program_.id(), "uScale");
	glUseProgram(program_.id());
	glUniform1i(glGetUniformLocation(program_.id(), "uAtlas"), 0);

	static constexpr std::array<std::uint8_t, 1> white{255};
	whiteTexture_ = createTexture(1, white);

	// The vertex layout and the index buffer binding are stored in the VAO once, then reused every frame
	glBindVertexArray(vertexArray_.id());

	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.id());
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(offsetof(Vertex, x)));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_UNSIGNED_SHORT, GL_TRUE, sizeof(Vertex),
	                      reinterpret_cast<const void*>(offsetof(Vertex, u)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex),
	                      reinterpret_cast<const void*>(offsetof(Vertex, r)));
	glEnableVertexAttribArray(2);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer_.id());

	glBindVertexArray(0);
}

void QuadRenderer::draw(const Panel& root, std::span<Panel* const> overlays, const Panel* tooltip, int width,
                        int height) {
	vertices_.clear();
	batches_.clear();
	collect(root);
	for(const Panel* overlay : overlays) collect(*overlay);
	if(tooltip) collect(*tooltip);

	const std::size_t quads = vertices_.size() / verticesPerQuad;
	if(quads == 0) return;

	glUseProgram(program_.id());
	if(width != width_ || height != height_) {
		glUniform2f(scaleLocation_, 2.0f / static_cast<float>(width), -2.0f / static_cast<float>(height));
		width_ = width;
		height_ = height;
	}

	for(const Batch& batch : batches_)
		if(batch.font) updateFontTexture(*batch.font, batch.page);

	glBindVertexArray(vertexArray_.id());
	reserveQuads(quads);

	// Re-specifying the whole store lets the driver orphan the old one instead of waiting for the GPU to finish with it
	glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer_.id());
	glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices_.size() * sizeof(Vertex)), vertices_.data(),
	             GL_STREAM_DRAW);

	glActiveTexture(GL_TEXTURE0);
	for(const Batch& batch : batches_) {
		if(batch.quadCount == 0) continue;
		glBindTexture(GL_TEXTURE_2D, batch.texture);
		glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(batch.quadCount * indicesPerQuad), GL_UNSIGNED_INT,
		               reinterpret_cast<const void*>(batch.firstQuad * indicesPerQuad * sizeof(GLuint)));
	}

	glBindVertexArray(0);
}

// Pre-order traversal: parents first, so children (and later siblings) are drawn on top
void QuadRenderer::collect(const Panel& panel) {
	if(!panel.isShown()) return; // hidden panels hide their whole subtree

	clip_ = panel.bounds();
	panel.paint(*this);

	for(const auto& child : panel.children())
		if(!child->isOverlay()) collect(*child); // overlays are drawn after the whole tree

	clip_ = panel.bounds();
	panel.paintOver(*this);
}

void QuadRenderer::fillRect(const Rect& rect, Color color) {
	if(batches_.empty()) useTexture(whiteTexture_.id(), {0.5f, 0.5f});

	// Solid rectangles never break a batch: they sample the current texture's white texel
	const Vec2 white = batches_.back().white;
	pushQuad(rect, Rect{white.x, white.y, white.x, white.y}, color);
}

void QuadRenderer::drawText(const Font& font, std::u32string_view text, Vec2 origin, Color color) {
	// Carry on with the font's page in use, so runs of text from a later page stay in one batch
	const bool sameFont = !batches_.empty() && batches_.back().font == &font;
	std::uint32_t page = sameFont ? batches_.back().page : 0;
	useFontPage(font, page);

	float penX = origin.x;
	for(char32_t cp : text) {
		if(penX > clip_.right) break; // the rest of the line is clipped anyway

		const Font::Glyph& glyph = font.glyph(cp);
		const Rect quad{penX + glyph.quad.left, origin.y + glyph.quad.top, penX + glyph.quad.right,
		                origin.y + glyph.quad.bottom};
		if(quad.right > quad.left) { // spaces have no quad
			if(glyph.page != page) useFontPage(font, page = glyph.page);
			pushQuad(quad, glyph.uv, color);
		}
		penX += glyph.advance;
	}
}

void QuadRenderer::useFontPage(const Font& font, std::uint32_t page) {
	useTexture(fontTexture(font, page), font.whiteTexel(page));
	batches_.back().font = &font;
	batches_.back().page = page;
}

void QuadRenderer::useTexture(GLuint texture, Vec2 white) {
	if(!batches_.empty() && batches_.back().texture == texture) return;
	batches_.push_back({texture, white, vertices_.size() / verticesPerQuad, 0});
}

// Clips the quad to the panel being painted (adjusting the texture coordinates to match) and appends it
void QuadRenderer::pushQuad(const Rect& rect, const Rect& uv, Color color) {
	const auto [cr, cg, cb, ca] = color.toBytes();
	if(ca == 0) return; // invisible: e.g. pure layout containers

	const Rect q{std::max(rect.left, clip_.left), std::max(rect.top, clip_.top), std::min(rect.right, clip_.right),
	             std::min(rect.bottom, clip_.bottom)};
	if(q.right <= q.left || q.bottom <= q.top) return;

	Rect t = uv;
	if(q != rect) {
		auto lerp = [](float a, float b, float f) { return a + (b - a) * f; };
		t.left = lerp(uv.left, uv.right, (q.left - rect.left) / rect.width());
		t.right = lerp(uv.left, uv.right, (q.right - rect.left) / rect.width());
		t.top = lerp(uv.top, uv.bottom, (q.top - rect.top) / rect.height());
		t.bottom = lerp(uv.top, uv.bottom, (q.bottom - rect.top) / rect.height());
	}

	const std::uint16_t u0 = toUnorm16(t.left), v0 = toUnorm16(t.top);
	const std::uint16_t u1 = toUnorm16(t.right), v1 = toUnorm16(t.bottom);
	vertices_.push_back({q.left, q.top, u0, v0, cr, cg, cb, ca});
	vertices_.push_back({q.right, q.top, u1, v0, cr, cg, cb, ca});
	vertices_.push_back({q.right, q.bottom, u1, v1, cr, cg, cb, ca});
	vertices_.push_back({q.left, q.bottom, u0, v1, cr, cg, cb, ca});
	++batches_.back().quadCount;
}

GLuint QuadRenderer::fontTexture(const Font& font, std::uint32_t page) {
	std::vector<PageTexture>& pages = fontTextures_[font.id()];
	while(pages.size() <= page) {
		const std::size_t next = pages.size();
		pages.push_back({createTexture(font.atlasSize(next), font.atlasPixels(next)), font.atlasVersion(next)});
	}
	return pages[page].texture.id();
}

void QuadRenderer::updateFontTexture(const Font& font, std::uint32_t page) {
	PageTexture& texture = fontTextures_.at(font.id())[page];
	const std::uint64_t version = font.atlasVersion(page);
	if(texture.version == version) return;

	const Font::AtlasRegion changed = font.atlasChangesSince(page, texture.version);
	texture.version = version;
	if(changed.empty()) return;

	// Only the rectangle holding the new glyphs, read out of the whole page's rows
	const int size = font.atlasSize(page);
	const std::uint8_t* first = font.atlasPixels(page).data() +
	                            static_cast<std::size_t>(changed.top) * static_cast<std::size_t>(size) +
	                            static_cast<std::size_t>(changed.left);
	glBindTexture(GL_TEXTURE_2D, texture.texture.id());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, size);
	glTexSubImage2D(GL_TEXTURE_2D, 0, changed.left, changed.top, changed.right - changed.left,
	                changed.bottom - changed.top, GL_RED, GL_UNSIGNED_BYTE, first);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

Texture QuadRenderer::createTexture(int size, std::span<const std::uint8_t> pixels) {
	Texture texture = Texture::create();
	glBindTexture(GL_TEXTURE_2D, texture.id());
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
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
		const auto v = static_cast<GLuint>(q * verticesPerQuad);
		indices.insert(indices.end(), {v, v + 1, v + 2, v + 2, v + 3, v});
	}

	// GL_ELEMENT_ARRAY_BUFFER is VAO state: the caller has the VAO bound
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(GLuint)), indices.data(),
	             GL_STATIC_DRAW);
}

} // namespace affresco::gl
