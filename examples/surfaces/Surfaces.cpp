/// @file
/// @brief The example GPU surfaces: plasma (a shader over the whole surface), cube (depth-tested, turned by a drag) and
/// blur (a pattern blurred in two passes).

#include "Surfaces.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

namespace examples {

namespace {

using affresco::json;
using affresco::SurfaceFrame;

/// @brief A triangle over the whole viewport, from gl_VertexID alone (no buffers); uv is 0..1 over the viewport.
constexpr const char* fullscreenVertex = R"(
out vec2 uv;
void main() {
	vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
	uv = corner;
	gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

/// @brief Moving colour waves.
constexpr const char* plasmaFragment = R"(
precision highp float;
in vec2 uv;
uniform float time;
uniform vec2 size;
layout(location = 0) out vec4 color;
void main() {
	vec2 p = uv * vec2(size.x / size.y, 1.0) * 3.0;
	float v = sin(p.x + time) + sin(p.y * 1.3 + time * 0.7) + sin((p.x + p.y) * 0.8 + time * 1.1) +
	          sin(length(p - 1.5) * 2.0 - time);
	color = vec4(0.5 + 0.5 * sin(v * 3.14159 + vec3(0.0, 2.1, 4.2)), 1.0);
}
)";

/// @brief The cube's vertices, transformed, with their face's colour.
constexpr const char* cubeVertex = R"(
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 shade;
uniform mat4 transform;
out vec3 faceColor;
void main() {
	faceColor = shade;
	gl_Position = transform * vec4(position, 1.0);
}
)";

/// @brief The face's colour, opaque.
constexpr const char* cubeFragment = R"(
precision highp float;
in vec3 faceColor;
layout(location = 0) out vec4 color;
void main() { color = vec4(faceColor, 1.0); }
)";

/// @brief Stripes and a ring that move with time: something with edges to blur.
constexpr const char* patternFragment = R"(
precision highp float;
in vec2 uv;
uniform float time;
uniform vec2 size;
layout(location = 0) out vec4 color;
void main() {
	vec2 p = uv * size;
	float stripes = step(0.5, fract((p.x + p.y) / 24.0 + time * 0.2));
	float ring = step(abs(length(p - size * 0.5) - min(size.x, size.y) * 0.3), 6.0);
	color = vec4(mix(vec3(0.1, 0.2, 0.5), vec3(0.9, 0.8, 0.3), stripes) * (1.0 - ring) + vec3(1.0) * ring, 1.0);
}
)";

/// @brief One direction of a Gaussian blur of the source.
constexpr const char* blurFragment = R"(
precision highp float;
in vec2 uv;
uniform sampler2D source;
uniform vec2 direction;
uniform int radius;
layout(location = 0) out vec4 color;
void main() {
	vec4 sum = vec4(0.0);
	float total = 0.0;
	float sigma = max(float(radius) * 0.5, 0.5);
	for(int i = -radius; i <= radius; ++i) {
		float weight = exp(-float(i * i) / (2.0 * sigma * sigma));
		sum += texture(source, uv + direction * float(i)) * weight;
		total += weight;
	}
	color = sum / total;
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
	throw std::runtime_error(std::string("a shader didn't compile: ") + log.data());
}

/// @brief Compiles and links a program.
/// @param prelude The version line.
/// @param vertex The vertex shader, after the prelude.
/// @param fragment The fragment shader, after the prelude.
/// @return The program.
/// @throws std::runtime_error If it doesn't compile or link.
GLuint link(std::string_view prelude, const char* vertex, const char* fragment) {
	const GLuint vertexShader = compile(GL_VERTEX_SHADER, prelude, vertex);
	GLuint fragmentShader = 0;
	try {
		fragmentShader = compile(GL_FRAGMENT_SHADER, prelude, fragment);
	} catch(...) {
		glDeleteShader(vertexShader);
		throw;
	}
	const GLuint program = glCreateProgram();
	glAttachShader(program, vertexShader);
	glAttachShader(program, fragmentShader);
	glLinkProgram(program);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	GLint linked = GL_FALSE;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if(linked == GL_TRUE) return program;
	glDeleteProgram(program);
	throw std::runtime_error("a program didn't link");
}

/// @brief A number parameter.
/// @param params The parameters.
/// @param key Its name.
/// @param fallback When it is missing or not a number.
/// @return Its value.
double number(const json& params, const char* key, double fallback) {
	const auto found = params.find(key);
	return found != params.end() && found->is_number() ? found->get<double>() : fallback;
}

/// @brief The time a surface draws at: its "time" parameter, else the fixed time, else the window's clock.
/// @param frame The frame.
/// @param fixedTime The fixed time, if any.
/// @return Seconds.
double timeOf(const SurfaceFrame& frame, std::optional<double> fixedTime) {
	return number(frame.params(), "time", fixedTime.value_or(frame.time));
}

/// @brief A surface that draws with full-viewport triangles, so needs an empty vertex array.
class FullscreenSurface : public affresco::GpuSurface {
public:
	/// @brief Frees the vertex array and the programs.
	void release() override {
		glDeleteVertexArrays(1, &vertexArray_);
		vertexArray_ = 0;
		for(GLuint& program : programs_) {
			glDeleteProgram(program);
			program = 0;
		}
	}

protected:
	/// @brief Makes the vertex array and the programs once.
	/// @param prelude The version line.
	/// @param fragments The programs' fragment shaders (at most two).
	void make(std::string_view prelude, std::initializer_list<const char*> fragments) {
		if(vertexArray_ != 0) return;
		glGenVertexArrays(1, &vertexArray_);
		std::size_t index = 0;
		for(const char* fragment : fragments) programs_.at(index++) = link(prelude, fullscreenVertex, fragment);
	}
	/// @brief Binds a program and the vertex array, for its uniforms and then draw().
	/// @param program Which of the programs.
	void use(std::size_t program) const {
		glUseProgram(programs_.at(program));
		glBindVertexArray(vertexArray_);
	}
	/// @brief Draws the triangle into the bound framebuffer, with the program use() bound.
	static void draw() { glDrawArrays(GL_TRIANGLES, 0, 3); }
	/// @brief A uniform's location.
	/// @param program Which of the programs.
	/// @param name The uniform.
	/// @return Its location.
	[[nodiscard]] GLint uniform(std::size_t program, const char* name) const {
		return glGetUniformLocation(programs_.at(program), name);
	}

private:
	GLuint vertexArray_ = 0;              ///< An empty vertex array.
	std::array<GLuint, 2> programs_ = {}; ///< The programs.
};

/// @brief Moving colour waves over the whole surface.
class PlasmaSurface : public FullscreenSurface {
public:
	/// @brief A plasma.
	/// @param fixedTime If given, the time it draws at.
	explicit PlasmaSurface(std::optional<double> fixedTime) noexcept : fixedTime_(fixedTime) {}

	/// @brief Makes its program.
	void prepare(const affresco::SurfaceSetup& setup) override { make(setup.prelude, {plasmaFragment}); }

	/// @brief Draws the waves at this frame's time times "speed".
	void render(SurfaceFrame& frame) override {
		const double time = timeOf(frame, fixedTime_) * number(frame.params(), "speed", 1.0);
		use(0);
		glUniform1f(uniform(0, "time"), static_cast<float>(time));
		glUniform2f(uniform(0, "size"), static_cast<float>(frame.width), static_cast<float>(frame.height));
		draw();
	}

private:
	std::optional<double> fixedTime_; ///< See the constructor.
};

/// @brief A two-pass Gaussian blur of a moving pattern: pattern into "source", across into "across", down into the
/// surface.
class BlurSurface : public FullscreenSurface {
public:
	/// @brief A blur.
	/// @param fixedTime If given, the time it draws at.
	explicit BlurSurface(std::optional<double> fixedTime) noexcept : fixedTime_(fixedTime) {}

	/// @brief Makes its programs.
	void prepare(const affresco::SurfaceSetup& setup) override { make(setup.prelude, {patternFragment, blurFragment}); }

	/// @brief Draws the pattern and blurs it, across then down.
	void render(SurfaceFrame& frame) override {
		const int width = frame.width, height = frame.height;
		const affresco::SurfacePass source = frame.pass("source", {width, height});
		const affresco::SurfacePass across = frame.pass("across", {width, height});
		glBindFramebuffer(GL_FRAMEBUFFER, source.framebuffer);
		use(0);
		glUniform1f(uniform(0, "time"), static_cast<float>(timeOf(frame, fixedTime_)));
		glUniform2f(uniform(0, "size"), static_cast<float>(width), static_cast<float>(height));
		draw();

		const int radius = static_cast<int>(number(frame.params(), "radius", 4.0) * frame.scale);
		blur(across.framebuffer, source.texture, 1.0f / static_cast<float>(width), 0.0f, radius);
		blur(frame.framebuffer, across.texture, 0.0f, 1.0f / static_cast<float>(height), radius);
	}

private:
	/// @brief One direction of the blur.
	/// @param target The framebuffer it draws into.
	/// @param texture The texture it blurs.
	/// @param x One texel across, or 0.
	/// @param y One texel down, or 0.
	/// @param radius Texels each side, 0..32.
	void blur(unsigned target, unsigned texture, float x, float y, int radius) const {
		glBindFramebuffer(GL_FRAMEBUFFER, target);
		glBindTexture(GL_TEXTURE_2D, texture);
		use(1);
		glUniform1i(uniform(1, "source"), 0);
		glUniform2f(uniform(1, "direction"), x, y);
		glUniform1i(uniform(1, "radius"), std::clamp(radius, 0, 32));
		draw();
	}

	std::optional<double> fixedTime_; ///< See the constructor.
};

/// @brief A 4x4 matrix, column after column.
using Matrix = std::array<float, 16>;

/// @brief a times b.
/// @param a The left matrix.
/// @param b The right matrix.
/// @return The product.
Matrix multiply(const Matrix& a, const Matrix& b) noexcept {
	Matrix product{};
	for(std::size_t column = 0; column < 4; ++column)
		for(std::size_t row = 0; row < 4; ++row)
			for(std::size_t k = 0; k < 4; ++k) product[column * 4 + row] += a[k * 4 + row] * b[column * 4 + k];
	return product;
}

/// @brief The cube's transform: turned by yaw and pitch, three units away, in perspective.
/// @param yaw The turn about y, in radians.
/// @param pitch The turn about x, in radians.
/// @param aspect Width over height.
/// @return The matrix.
Matrix cubeTransform(float yaw, float pitch, float aspect) noexcept {
	const float cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
	const Matrix turnY{cy, 0, -sy, 0, 0, 1, 0, 0, sy, 0, cy, 0, 0, 0, 0, 1};
	const Matrix turnX{1, 0, 0, 0, 0, cp, sp, 0, 0, -sp, cp, 0, 0, 0, 0, 1};
	const Matrix away{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, -3.5f, 1};
	const float focal = 1.0f / std::tan(std::numbers::pi_v<float> / 8.0f), near = 0.5f, far = 10.0f;
	const Matrix perspective{focal / aspect,
	                         0,
	                         0,
	                         0,
	                         0,
	                         focal,
	                         0,
	                         0,
	                         0,
	                         0,
	                         (far + near) / (near - far),
	                         -1,
	                         0,
	                         0,
	                         2 * far * near / (near - far),
	                         0};
	return multiply(perspective, multiply(away, multiply(turnX, turnY)));
}

/// @brief The cube's triangles: per vertex, its position and its face's colour (36 vertices of 6 floats).
/// @return The vertices.
std::array<float, 216> cubeVertices() noexcept {
	constexpr std::array<std::array<float, 3>, 8> corners{
	    {{-1, -1, -1}, {1, -1, -1}, {1, 1, -1}, {-1, 1, -1}, {-1, -1, 1}, {1, -1, 1}, {1, 1, 1}, {-1, 1, 1}}};
	constexpr std::array<std::array<std::size_t, 4>, 6> faces{
	    {{4, 5, 6, 7}, {1, 0, 3, 2}, {5, 1, 2, 6}, {0, 4, 7, 3}, {7, 6, 2, 3}, {0, 1, 5, 4}}};
	constexpr std::array<std::array<float, 3>, 6> colors{{{0.93f, 0.33f, 0.31f},
	                                                      {0.25f, 0.6f, 0.95f},
	                                                      {0.35f, 0.8f, 0.45f},
	                                                      {0.98f, 0.75f, 0.2f},
	                                                      {0.62f, 0.4f, 0.9f},
	                                                      {0.2f, 0.8f, 0.8f}}};
	constexpr std::array<std::size_t, 6> order{0, 1, 2, 0, 2, 3}; // two triangles per face
	std::array<float, 216> vertices{};
	std::size_t at = 0;
	for(std::size_t face = 0; face < faces.size(); ++face)
		for(const std::size_t corner : order) {
			for(const float value : corners.at(faces.at(face).at(corner))) vertices.at(at++) = value * 0.6f;
			for(const float value : colors.at(face)) vertices.at(at++) = value;
		}
	return vertices;
}

/// @brief A cube turning about y, depth-tested; dragging over it turns it further.
class CubeSurface : public affresco::GpuSurface {
public:
	/// @brief A cube.
	/// @param fixedTime If given, the time it draws at.
	explicit CubeSurface(std::optional<double> fixedTime) noexcept : fixedTime_(fixedTime) {}

	/// @brief Makes its program, vertex array and buffer once.
	void prepare(const affresco::SurfaceSetup& setup) override {
		if(program_ != 0) return;
		program_ = link(setup.prelude, cubeVertex, cubeFragment);
		glGenVertexArrays(1, &vertexArray_);
		glGenBuffers(1, &buffer_);
		glBindVertexArray(vertexArray_);
		glBindBuffer(GL_ARRAY_BUFFER, buffer_);
		const std::array<float, 216> vertices = cubeVertices();
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices.data(), GL_STATIC_DRAW);
		constexpr GLsizei stride = 6 * sizeof(float);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<const void*>(3 * sizeof(float)));
	}

	/// @brief Follows the drags since the last frame, then draws the cube with the depth test on.
	void render(SurfaceFrame& frame) override {
		follow(frame);
		const double turns = timeOf(frame, fixedTime_) * number(frame.params(), "speed", 1.0) / 10.0;
		const double angle = number(frame.params(), "angle", turns * 360.0);
		const float yaw = static_cast<float>(angle * std::numbers::pi / 180.0) + dragYaw_;
		const Matrix transform =
		    cubeTransform(yaw, 0.45f + dragPitch_, static_cast<float>(frame.width) / static_cast<float>(frame.height));
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glUseProgram(program_);
		glUniformMatrix4fv(glGetUniformLocation(program_, "transform"), 1, GL_FALSE, transform.data());
		glBindVertexArray(vertexArray_);
		glDrawArrays(GL_TRIANGLES, 0, 36);
	}

	/// @brief Frees its program, vertex array and buffer.
	void release() override {
		glDeleteProgram(program_);
		glDeleteVertexArrays(1, &vertexArray_);
		glDeleteBuffers(1, &buffer_);
		program_ = vertexArray_ = buffer_ = 0;
	}

private:
	/// @brief Turns the cube by the drags in this frame's input (in its pixels).
	/// @param frame The frame.
	void follow(const SurfaceFrame& frame) {
		constexpr float perPixel = 0.01f;
		for(const affresco::SurfacePointer& pointer : frame.input) {
			if(pointer.kind == affresco::PointerKind::press) dragging_ = true;
			else if(pointer.kind == affresco::PointerKind::release) dragging_ = false;
			else if(pointer.kind == affresco::PointerKind::move && dragging_) {
				dragYaw_ += (pointer.position.x - last_.x) * perPixel;
				dragPitch_ += (pointer.position.y - last_.y) * perPixel;
			}
			last_ = pointer.position;
		}
	}

	std::optional<double> fixedTime_; ///< See the constructor.
	GLuint program_ = 0;              ///< Its program.
	GLuint vertexArray_ = 0;          ///< Its vertex array.
	GLuint buffer_ = 0;               ///< Its vertices.
	bool dragging_ = false;           ///< A button is down over it.
	affresco::Vec2 last_;             ///< The pointer's last position.
	float dragYaw_ = 0.0f;            ///< The turn about y that drags added, in radians.
	float dragPitch_ = 0.0f;          ///< The turn about x that drags added, in radians.
};

} // namespace

void registerSurfaces(affresco::Screen& screen, std::optional<double> fixedTime) {
	screen.registerSurface("plasma", [fixedTime] { return std::make_unique<PlasmaSurface>(fixedTime); });
	screen.registerSurface("cube", [fixedTime] { return std::make_unique<CubeSurface>(fixedTime); }, {true});
	screen.registerSurface("blur", [fixedTime] { return std::make_unique<BlurSurface>(fixedTime); });
}

} // namespace examples
