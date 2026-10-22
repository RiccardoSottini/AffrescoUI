/// @file
/// @brief Tempo's visualiser: one shader over the surface draws bars whose heights follow the time and the level.

#include "Visualiser.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

namespace examples::tempo {

namespace {

using affresco::json;

/// @brief A triangle over the whole viewport, from gl_VertexID alone; uv is 0..1 over the viewport.
constexpr const char* vertexShader = R"(
out vec2 uv;
void main() {
	vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
	uv = corner;
	gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

/// @brief Bars with rounded tops, brighter towards the top, and their faint reflection below the middle line.
constexpr const char* fragmentShader = R"(
precision highp float;
in vec2 uv;
uniform float time;
uniform float level;
uniform float bars;
uniform vec3 tint;
uniform vec3 background;
layout(location = 0) out vec4 color;
float height(float bar) {
	float wave = 0.5 + 0.25 * sin(bar * 0.9 + time * 3.1) + 0.15 * sin(bar * 2.3 - time * 5.3) +
	             0.1 * sin(bar * 0.37 + time * 1.7);
	float shape = 1.0 - 0.5 * abs(bar / bars - 0.35);
	return clamp(0.06 + level * wave * shape, 0.04, 1.0);
}
void main() {
	float bar = floor(uv.x * bars);
	float inside = fract(uv.x * bars);
	float gap = step(0.18, inside) * step(inside, 0.82);
	float base = 0.28;
	float top = base + height(bar) * (1.0 - base) * 0.95;
	float up = step(base, uv.y) * step(uv.y, top);
	float down = step(uv.y, base) * step(base - (top - base) * 0.35, uv.y);
	float glow = mix(0.55, 1.0, (uv.y - base) / max(top - base, 0.001));
	vec3 lit = mix(background, tint * glow, up * gap);
	lit = mix(lit, mix(background, tint, 0.25), down * gap);
	color = vec4(lit, 1.0);
}
)";

/// @brief Compiles a shader.
/// @param kind GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
/// @param prelude The version line (SurfaceSetup::prelude).
/// @param body The rest.
/// @return The shader.
/// @throws std::runtime_error With the log, if it doesn't compile.
GLuint compile(GLenum kind, std::string_view prelude, const char* body) {
	const GLuint shader = glCreateShader(kind);
	const std::array<const GLchar*, 2> parts{prelude.data(), body};
	const std::array<GLint, 2> lengths{static_cast<GLint>(prelude.size()), -1};
	glShaderSource(shader, 2, parts.data(), lengths.data());
	glCompileShader(shader);
	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(compiled == GL_TRUE) return shader;
	std::array<GLchar, 512> log{};
	glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
	glDeleteShader(shader);
	throw std::runtime_error(std::string("the visualiser's shader didn't compile: ") + log.data());
}

/// @brief A number parameter.
/// @param params The parameters.
/// @param key Its name.
/// @param fallback When it is missing or not a number.
/// @return Its value.
float number(const json& params, const char* key, float fallback) {
	const auto found = params.find(key);
	return found != params.end() && found->is_number() ? found->get<float>() : fallback;
}

/// @brief A colour parameter, [red, green, blue] from 0 to 1.
/// @param params The parameters.
/// @param key Its name.
/// @param fallback When it is missing or not three numbers.
/// @return Its value.
std::array<float, 3> colour(const json& params, const char* key, std::array<float, 3> fallback) {
	const auto found = params.find(key);
	if(found == params.end() || !found->is_array() || found->size() != 3) return fallback;
	for(std::size_t i = 0; i < 3; ++i)
		if((*found)[i].is_number()) fallback.at(i) = std::clamp((*found)[i].get<float>(), 0.0f, 1.0f);
	return fallback;
}

/// @brief The bars, drawn with one full-viewport triangle.
class VisualiserSurface : public affresco::GpuSurface {
public:
	/// @brief Compiles and links its program, and makes the empty vertex array the triangle needs.
	void prepare(const affresco::SurfaceSetup& setup) override {
		if(program_ != 0) return;
		const GLuint vertex = compile(GL_VERTEX_SHADER, setup.prelude, vertexShader);
		GLuint fragment = 0;
		try {
			fragment = compile(GL_FRAGMENT_SHADER, setup.prelude, fragmentShader);
		} catch(...) {
			glDeleteShader(vertex);
			throw;
		}
		program_ = glCreateProgram();
		glAttachShader(program_, vertex);
		glAttachShader(program_, fragment);
		glLinkProgram(program_);
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		GLint linked = GL_FALSE;
		glGetProgramiv(program_, GL_LINK_STATUS, &linked);
		if(linked != GL_TRUE) throw std::runtime_error("the visualiser's program didn't link");
		glGenVertexArrays(1, &vertexArray_);
	}

	/// @brief Draws the bars for this frame's parameters.
	void render(affresco::SurfaceFrame& frame) override {
		const json& params = frame.params();
		const std::array<float, 3> tint = colour(params, "color", {0.13f, 0.83f, 0.93f});
		const std::array<float, 3> background = colour(params, "background", {0.06f, 0.07f, 0.12f});
		glUseProgram(program_);
		glUniform1f(glGetUniformLocation(program_, "time"), number(params, "time", 0.0f));
		glUniform1f(glGetUniformLocation(program_, "level"), std::clamp(number(params, "level", 0.0f), 0.0f, 1.0f));
		glUniform1f(glGetUniformLocation(program_, "bars"), std::clamp(number(params, "bars", 24.0f), 4.0f, 96.0f));
		glUniform3f(glGetUniformLocation(program_, "tint"), tint[0], tint[1], tint[2]);
		glUniform3f(glGetUniformLocation(program_, "background"), background[0], background[1], background[2]);
		glBindVertexArray(vertexArray_);
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	/// @brief Frees its program and vertex array.
	void release() override {
		glDeleteProgram(program_);
		glDeleteVertexArrays(1, &vertexArray_);
		program_ = vertexArray_ = 0;
	}

private:
	GLuint program_ = 0;     ///< Its program.
	GLuint vertexArray_ = 0; ///< An empty vertex array.
};

} // namespace

void registerVisualiser(affresco::Screen& screen) {
	screen.registerSurface("tempo.visualiser", [] { return std::make_unique<VisualiserSurface>(); });
}

} // namespace examples::tempo
