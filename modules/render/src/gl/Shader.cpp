/// @file
/// @brief Shader program creation, for either backend (Shader.hpp).

#include "gl/Shader.hpp"

#include <stdexcept>
#include <string>

namespace affresco::gl {

std::string_view shaderPrelude(ShaderStage stage) noexcept {
#if defined(AFFRESCOUI_GLES3)
	// highp: what GLSL 4.10 computes in, so both backends round alike (ES 3.0 guarantees it in fragment shaders)
	if(stage == ShaderStage::fragment) return "#version 300 es\nprecision highp float;\nprecision highp int;\n";
	return "#version 300 es\n";
#else
	static_cast<void>(stage); // one prelude for every stage
	return "#version 410 core\n";
#endif
}

namespace {

/// @brief Compiles one shader.
/// @param type GL_VERTEX_SHADER or GL_FRAGMENT_SHADER.
/// @param source GLSL source.
/// @return The shader; the caller deletes it.
/// @throws std::runtime_error With the driver's log, if compiling fails.
GLuint compile(GLenum type, std::string_view source) {
	const GLuint shader = glCreateShader(type);
	const GLchar* text = source.data();
	const auto length = static_cast<GLint>(source.size());
	glShaderSource(shader, 1, &text, &length);
	glCompileShader(shader);

	GLint compiled = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if(compiled == GL_FALSE) {
		std::string log(1024, '\0');
		glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
		glDeleteShader(shader);
		throw std::runtime_error("AffrescoUI: shader compilation failed: " + log);
	}

	return shader;
}

} // namespace

Program linkProgram(std::string_view vertexSource, std::string_view fragmentSource) {
	const GLuint vertexShader = compile(GL_VERTEX_SHADER, vertexSource);
	const GLuint fragmentShader = compile(GL_FRAGMENT_SHADER, fragmentSource);

	Program program = Program::create();
	glAttachShader(program.id(), vertexShader);
	glAttachShader(program.id(), fragmentShader);
	glLinkProgram(program.id());

	// The program keeps the compiled code; the shader objects are no longer needed
	glDetachShader(program.id(), vertexShader);
	glDetachShader(program.id(), fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	GLint linked = GL_FALSE;
	glGetProgramiv(program.id(), GL_LINK_STATUS, &linked);
	if(linked == GL_FALSE) {
		std::string log(1024, '\0');
		glGetProgramInfoLog(program.id(), static_cast<GLsizei>(log.size()), nullptr, log.data());
		throw std::runtime_error("AffrescoUI: shader link failed: " + log);
	}

	return program;
}

} // namespace affresco::gl
