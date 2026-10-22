/// @file
/// @brief The canvas's GLSL: a layer composited onto a pass a cell at a time with blendPixels(), its mask, its
/// clipping base and its adjustment; and the composite shown with the display table, a checkerboard, the quick mask
/// and marching ants.

#include "CanvasShaders.hpp"

#include "affresco/imaging/Adjustment.hpp"
#include "affresco/imaging/Blend.hpp"

#include <glad/glad.h>

#include <array>
#include <initializer_list>
#include <stdexcept>
#include <string>

namespace affresco::imaging::canvas {

namespace {

/// @brief A triangle over the whole viewport, from gl_VertexID alone.
constexpr std::string_view fullscreenVertex = R"(
void main() {
	vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
	gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

/// @brief Precision first, for GLSL ES (blendShaderSource() follows it).
constexpr std::string_view precision = "precision highp float;\nprecision highp int;\n";

/// @brief A layer onto the backdrop, its coverage, or a mask's coverage, at one pixel of the region's pass.
constexpr std::string_view layerFragment = R"(
uniform sampler2D source;
uniform sampler2D maskTile;
uniform sampler2D backdrop;
uniform sampler2D base;
uniform sampler2D table;
uniform ivec2 origin;
uniform int sourceKind;
uniform int maskKind;
uniform int adjustKind;
uniform int mode;
uniform int stage;
uniform int clipped;
uniform vec4 sourceColor;
uniform float maskValue;
uniform float opacity;
uniform vec3 hsl;
uniform float level;
layout(location = 0) out vec4 color;
vec4 lookup(float value) { return texelFetch(table, ivec2(int(clamp(value, 0.0, 1.0) * 255.0 + 0.5), 0), 0); }
vec3 adjusted(vec3 c) {
	if(adjustKind == 2) return adjustHueSaturation(c, hsl);
	if(adjustKind == 3) return vec3(blendLum(c) >= level ? 1.0 : 0.0); // a threshold, exactly as the CPU's
	if(adjustKind == 1) {
		// By luminosity: the table holds the result for grey; its alpha mixes it with the colour (a gradient map)
		float index = floor(clamp(blendLum(c), 0.0, 1.0) * 255.0 + 0.5);
		vec4 entry = lookup(index / 255.0);
		return entry.rgb + (c - index / 255.0) * (1.0 - entry.a);
	}
	vec3 kept = clamp(c, 0.0, 1.0);
	return vec3(lookup(kept.r).r, lookup(kept.g).g, lookup(kept.b).b);
}
void main() {
	ivec2 at = ivec2(gl_FragCoord.xy);
	ivec2 local = at - origin;
	float covered = maskKind == 0 ? 1.0 : (maskKind == 1 ? texelFetch(maskTile, local, 0).r : maskValue);
	if(stage == 2) {
		color = vec4(covered);
		return;
	}
	vec4 below = texelFetch(backdrop, at, 0);
	vec4 layer = sourceKind == 0 ? texelFetch(source, local, 0)
	           : sourceKind == 1 ? sourceColor
	           : sourceKind == 2 ? texelFetch(source, at, 0)
	                             : vec4(adjusted(below.rgb), below.a);
	float own = (sourceKind == 3 ? 1.0 : layer.a) * covered;
	if(stage == 1) {
		color = vec4(own);
		return;
	}
	float coverage = clipped == 1 ? texelFetch(base, at, 0).r : 1.0;
	color = blendPixels(below, layer, mode, opacity * covered * coverage);
}
)";

/// @brief The composite at a surface pixel: through the display table, over a checkerboard, with the overlays.
constexpr std::string_view displayFragment = R"(
uniform sampler2D composite;
uniform sampler2D selection;
uniform highp sampler3D display;
uniform vec2 size;
uniform vec2 centre;
uniform vec2 regionOrigin;
uniform vec2 passSize;
uniform vec2 documentSize;
uniform float zoom;
uniform float levelScale;
uniform float time;
uniform float displaySide;
uniform int nearest;
uniform int checker;
uniform int ants;
uniform int quickMask;
layout(location = 0) out vec4 color;
vec2 regionAt(vec2 screen) { return (centre + (screen - size * 0.5) / zoom) / levelScale - regionOrigin; }
float selectedAt(vec2 screen) {
	ivec2 at = clamp(ivec2(floor(regionAt(screen))), ivec2(0), ivec2(passSize) - 1);
	return texelFetch(selection, at, 0).r >= 0.5 ? 1.0 : 0.0;
}
void main() {
	vec2 screen = vec2(gl_FragCoord.x, size.y - gl_FragCoord.y);
	vec2 point = centre + (screen - size * 0.5) / zoom;
	if(any(lessThan(point, vec2(0.0))) || any(greaterThanEqual(point, documentSize))) {
		color = vec4(0.0);
		return;
	}
	vec2 at = regionAt(screen);
	vec4 pixel = nearest == 1 ? texelFetch(composite, clamp(ivec2(floor(at)), ivec2(0), ivec2(passSize) - 1), 0)
	                          : texture(composite, at / passSize);
	pixel = clamp(pixel, 0.0, 1.0);
	if(displaySide > 1.0) pixel.rgb = texture(display, pixel.rgb * (displaySide - 1.0) / displaySide + 0.5 / displaySide).rgb;
	color = vec4(pixel.rgb * pixel.a, pixel.a);
	if(checker == 1) {
		float square = mod(floor(screen.x / 8.0) + floor(screen.y / 8.0), 2.0) == 0.0 ? 1.0 : 0.8;
		color = color + vec4(vec3(square), 1.0) * (1.0 - color.a);
	}
	if(quickMask == 1) color = mix(color, vec4(0.9, 0.1, 0.1, 1.0), 0.5 * (1.0 - selectedAt(screen)));
	if(ants == 1) {
		float here = selectedAt(screen);
		float edge = abs(here - selectedAt(screen + vec2(1.0, 0.0))) + abs(here - selectedAt(screen + vec2(0.0, 1.0)));
		float dash = mod(floor((screen.x + screen.y) / 4.0 - time * 8.0), 2.0);
		if(edge > 0.0) color = vec4(vec3(dash), 1.0);
	}
}
)";

/// @brief Compiles a shader from parts.
/// @param kind GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
/// @param parts Its source, in order.
/// @return The shader.
/// @throws std::runtime_error with the log when it doesn't compile.
GLuint compile(GLenum kind, const std::array<std::string_view, 5>& parts) {
	const GLuint shader = glCreateShader(kind);
	std::array<const GLchar*, 5> pointers{};
	std::array<GLint, 5> lengths{};
	for(std::size_t index = 0; index < parts.size(); ++index) {
		pointers.at(index) = parts.at(index).data();
		lengths.at(index) = static_cast<GLint>(parts.at(index).size());
	}
	glShaderSource(shader, static_cast<GLsizei>(parts.size()), pointers.data(), lengths.data());
	glCompileShader(shader);
	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(compiled == GL_TRUE) return shader;
	std::array<GLchar, 1024> log{};
	glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
	glDeleteShader(shader);
	throw std::runtime_error(std::string("imaging: a canvas shader didn't compile: ") + log.data());
}

/// @brief Links a program of the fullscreen vertex shader and a fragment shader.
/// @param prelude The version line.
/// @param fragment The fragment shader's parts after the prelude.
/// @return The program.
/// @throws std::runtime_error when it doesn't compile or link.
GLuint link(std::string_view prelude, const std::array<std::string_view, 4>& fragment) {
	const GLuint vertex = compile(GL_VERTEX_SHADER, {prelude, fullscreenVertex, "", "", ""});
	GLuint pixels = 0;
	try {
		pixels = compile(GL_FRAGMENT_SHADER, {prelude, fragment[0], fragment[1], fragment[2], fragment[3]});
	} catch(...) {
		glDeleteShader(vertex);
		throw;
	}
	const GLuint program = glCreateProgram();
	glAttachShader(program, vertex);
	glAttachShader(program, pixels);
	glLinkProgram(program);
	glDeleteShader(vertex);
	glDeleteShader(pixels);
	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if(linked == GL_TRUE) return program;
	glDeleteProgram(program);
	throw std::runtime_error("imaging: a canvas program didn't link");
}

/// @brief Sets a program's samplers to their units.
/// @param program The program.
/// @param names Each sampler's name, by unit.
void bindSamplers(GLuint program, std::initializer_list<const char*> names) {
	glUseProgram(program);
	GLint unit = 0;
	for(const char* name : names) glUniform1i(glGetUniformLocation(program, name), unit++);
	glUseProgram(0);
}

} // namespace

unsigned layerProgram(std::string_view prelude) {
	const GLuint program = link(prelude, {precision, blendShaderSource(), adjustmentShaderSource(), layerFragment});
	bindSamplers(program, {"source", "maskTile", "backdrop", "base", "table"});
	return program;
}

unsigned displayProgram(std::string_view prelude) {
	const GLuint program = link(prelude, {precision, displayFragment, "", ""});
	bindSamplers(program, {"composite", "selection", "display"});
	return program;
}

} // namespace affresco::imaging::canvas
