/// @file
/// @brief Compiling and linking programs (GlProgram.hpp).

#include "GlProgram.hpp"

#include <glad/glad.h>

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace affresco::scene3d::detail {

namespace {

/// @brief Compiles a shader from parts.
/// @param kind GL_VERTEX_SHADER or GL_FRAGMENT_SHADER. @param prelude The version line.
/// @param body The parts after it.
/// @return The shader.
/// @throws std::runtime_error With the log, if it doesn't compile.
GLuint compile(GLenum kind, std::string_view prelude, std::initializer_list<std::string_view> body) {
	std::vector<const GLchar*> parts{prelude.data()};
	std::vector<GLint> lengths{static_cast<GLint>(prelude.size())};
	for(const auto part : body) {
		parts.push_back(part.data());
		lengths.push_back(static_cast<GLint>(part.size()));
	}
	const GLuint shader = glCreateShader(kind);
	glShaderSource(shader, static_cast<GLsizei>(parts.size()), parts.data(), lengths.data());
	glCompileShader(shader);
	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(compiled == GL_TRUE) return shader;
	std::array<GLchar, 1024> log{};
	glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
	glDeleteShader(shader);
	throw std::runtime_error(std::string("scene3d: a shader didn't compile: ") + log.data());
}

} // namespace

GlProgram::GlProgram(std::string_view prelude, std::initializer_list<std::string_view> vertex,
                     std::initializer_list<std::string_view> fragment) {
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
	if(linked != GL_TRUE) {
		glDeleteProgram(program);
		throw std::runtime_error("scene3d: a program didn't link");
	}
	program_ = program;
}

GlProgram::GlProgram(GlProgram&& other) noexcept
    : program_(std::exchange(other.program_, 0)), locations_(std::move(other.locations_)) {}

GlProgram& GlProgram::operator=(GlProgram&& other) noexcept {
	if(this != &other) {
		release();
		program_ = std::exchange(other.program_, 0);
		locations_ = std::move(other.locations_);
	}
	return *this;
}

void GlProgram::release() noexcept {
	if(program_ != 0) glDeleteProgram(program_);
	program_ = 0;
	locations_.clear();
}

void GlProgram::use() const noexcept { glUseProgram(program_); }

int GlProgram::uniform(std::string_view name) const {
	if(const auto found = locations_.find(name); found != locations_.end()) return found->second;
	const std::string key(name);
	const int location = glGetUniformLocation(program_, key.c_str());
	locations_.emplace(key, location);
	return location;
}

} // namespace affresco::scene3d::detail
