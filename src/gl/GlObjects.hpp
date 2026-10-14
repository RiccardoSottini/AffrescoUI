#pragma once

// Move-only RAII owners for OpenGL object names: no leaks, no double deletes.
// They must be created and destroyed while their context is current.

#include <glad/glad.h>

#include <utility>

namespace sgl::gl {

template <typename Traits> class Object {
public:
	Object() = default;
	explicit Object(GLuint id) noexcept : id_(id) {}
	~Object() { reset(); }

	Object(const Object&) = delete;
	Object& operator=(const Object&) = delete;
	Object(Object&& other) noexcept : id_(std::exchange(other.id_, 0)) {}
	Object& operator=(Object&& other) noexcept {
		if(this != &other) {
			reset();
			id_ = std::exchange(other.id_, 0);
		}
		return *this;
	}

	[[nodiscard]] static Object create() { return Object(Traits::create()); }
	[[nodiscard]] GLuint id() const noexcept { return id_; }

	void reset() noexcept {
		if(id_ != 0) Traits::destroy(id_);
		id_ = 0;
	}

private:
	GLuint id_ = 0;
};

struct BufferTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenBuffers(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteBuffers(1, &id); }
};

struct VertexArrayTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenVertexArrays(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteVertexArrays(1, &id); }
};

struct ProgramTraits {
	static GLuint create() { return glCreateProgram(); }
	static void destroy(GLuint id) noexcept { glDeleteProgram(id); }
};

struct TextureTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenTextures(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteTextures(1, &id); }
};

using Buffer = Object<BufferTraits>;
using VertexArray = Object<VertexArrayTraits>;
using Program = Object<ProgramTraits>;
using Texture = Object<TextureTraits>;

} // namespace sgl::gl
