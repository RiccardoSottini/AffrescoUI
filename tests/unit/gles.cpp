/// @file
/// @brief Unit tests for the OpenGL backends' shader prelude (gl/Shader.hpp): every shader body is written once, in
/// the common subset of GLSL 4.10 core and GLSL ES 3.00, and the prelude of this build's backend
/// (AFFRESCOUI_GL_BACKEND) adds what differs. No context is needed.

#include "gl/Shader.hpp"

#include <affresco/render/GpuSurface.hpp>

#include <doctest/doctest.h>

#include <string_view>

using affresco::gl::ShaderStage;
using affresco::gl::shaderPrelude;

namespace {

/// @brief Whether a prelude is OpenGL ES's.
/// @param prelude A shader prelude.
/// @return True for `#version 300 es`.
bool isEs(std::string_view prelude) { return prelude.starts_with("#version 300 es\n"); }

} // namespace

TEST_CASE("gles: both stages' preludes start with the backend's #version line, the same for both") {
	const std::string_view vertex = shaderPrelude(ShaderStage::vertex);
	const std::string_view fragment = shaderPrelude(ShaderStage::fragment);
	CHECK((vertex == "#version 410 core\n" || vertex == "#version 300 es\n"));
	CHECK(isEs(vertex) == isEs(fragment));
	CHECK(fragment.starts_with(vertex.substr(0, vertex.find('\n') + 1)));
}

TEST_CASE("gles: on OpenGL ES the fragment prelude sets the default float and int precision, high") {
	const std::string_view fragment = shaderPrelude(ShaderStage::fragment);
	if(isEs(fragment)) {
		CHECK(fragment.find("precision highp float;\n") != std::string_view::npos);
		CHECK(fragment.find("precision highp int;\n") != std::string_view::npos);
	} else {
		CHECK(fragment == "#version 410 core\n"); // GLSL 4.10 core has no precision to set
	}
}

TEST_CASE("gles: each prelude is whole lines, so a body appended after it starts on a line of its own") {
	for(const ShaderStage stage : {ShaderStage::vertex, ShaderStage::fragment}) {
		const std::string_view prelude = shaderPrelude(stage);
		REQUIRE(!prelude.empty());
		CHECK(prelude.back() == '\n');
		CHECK(prelude.find("\n\n") == std::string_view::npos);
	}
}

TEST_CASE("gles: the vertex prelude is only the #version line: vertex shaders have default precisions on both") {
	const std::string_view vertex = shaderPrelude(ShaderStage::vertex);
	CHECK(vertex.find('\n') == vertex.size() - 1);
}

TEST_CASE("gles: surfaces' shaders start with the backend's prelude, so they compile on OpenGL ES too") {
	CHECK(affresco::surfaceShaderPrelude() == shaderPrelude(ShaderStage::fragment));
}
