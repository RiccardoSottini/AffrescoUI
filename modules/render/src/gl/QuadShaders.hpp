#pragma once

/// @file
/// @brief The GLSL of gl::QuadRenderer's programs: one vertex shader, and the parts its four fragment shaders are
/// joined from (with or without shapes, with or without colour glyphs). Each starts after shaderPrelude().

#include <string_view>

namespace affresco::gl::quadShaders {

/// @brief Vertex shader: window units to clip space.
inline constexpr std::string_view vertexSource = R"glsl(
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
inline constexpr std::string_view plainFragmentSource = R"glsl(
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
inline constexpr std::string_view shapeInputs = R"glsl(
	uniform sampler2D uAtlas;

	in vec2 vTexCoord;
	in vec4 vColor;
	in vec2 vInside;      // distance inside the nearest vertical and horizontal edges
	flat in vec4 vShape;  // the corner's radius, the outline's width (0: filled), the blur (in window units), the kind
	out vec4 fColor;
)glsl";

/// @brief The shape functions of the fragment shaders with shapes.
inline constexpr std::string_view shapeFunctions = R"glsl(

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
inline constexpr std::string_view shapeMain = R"glsl(
	void main() {
		float alpha = vColor.a * texture(uAtlas, vTexCoord).r;
		// The same for a whole quad, so rectangles and text skip the shape's work
		if(vShape.w > 0.0) alpha *= coverage();
		fColor = vec4(vColor.rgb, alpha);
	}
)glsl";

/// @brief The colour page of the fragment shaders with colour glyphs, and what they do first: a colour glyph's pixel is
/// the page's colour, with the text's opacity.
inline constexpr std::string_view colorFunctions = R"glsl(
	uniform sampler2D uColorAtlas; // the batch's colour page: premultiplied RGBA

	bool colorGlyph() {
		if(vShape.w < 1.5) return false;
		vec4 texel = texture(uColorAtlas, vTexCoord);
		fColor = vec4(texel.rgb / max(texel.a, 1.0 / 255.0), texel.a * vColor.a);
		return true;
	}
)glsl";

/// @brief The main of the fragment shader with colour glyphs and no shapes.
inline constexpr std::string_view colorMain = R"glsl(
	void main() {
		if(colorGlyph()) return;
		fColor = vec4(vColor.rgb, vColor.a * texture(uAtlas, vTexCoord).r);
	}
)glsl";

/// @brief The main of the fragment shader with colour glyphs and shapes.
inline constexpr std::string_view shapeColorMain = R"glsl(
	void main() {
		if(colorGlyph()) return;
		float alpha = vColor.a * texture(uAtlas, vTexCoord).r;
		if(vShape.w > 0.0) alpha *= coverage();
		fColor = vec4(vColor.rgb, alpha);
	}
)glsl";

} // namespace affresco::gl::quadShaders
