#pragma once

/// @file
/// @brief Move-only RAII owners for OpenGL object names: no leaks, no double deletes.
///
/// They must be created and destroyed while their context is current.

#include <glad/glad.h>

#include <utility>

namespace affresco::gl {

/// @brief Owns one OpenGL object name and deletes it when destroyed.
/// @tparam Traits Provides `create()` and `destroy(GLuint)`.
template <typename Traits> class Object {
public:
	Object() = default;
	/// @brief Takes ownership of a name.
	/// @param id The name (0: nothing).
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

	/// @brief Creates an object in the current context.
	/// @return Its owner.
	[[nodiscard]] static Object create() { return Object(Traits::create()); }
	/// @brief The name, or 0.
	[[nodiscard]] GLuint id() const noexcept { return id_; }

	/// @brief Deletes the object, if any.
	void reset() noexcept {
		if(id_ != 0) Traits::destroy(id_);
		id_ = 0;
	}

private:
	GLuint id_ = 0; ///< 0: nothing.
};

/// @brief Buffer object lifetime.
struct BufferTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenBuffers(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteBuffers(1, &id); }
};

/// @brief Vertex array object lifetime.
struct VertexArrayTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenVertexArrays(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteVertexArrays(1, &id); }
};

/// @brief Shader program lifetime.
struct ProgramTraits {
	static GLuint create() { return glCreateProgram(); }
	static void destroy(GLuint id) noexcept { glDeleteProgram(id); }
};

/// @brief Texture lifetime.
struct TextureTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenTextures(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteTextures(1, &id); }
};

/// @brief Framebuffer object lifetime.
struct FramebufferTraits {
	static GLuint create() {
		GLuint id = 0;
		glGenFramebuffers(1, &id);
		return id;
	}
	static void destroy(GLuint id) noexcept { glDeleteFramebuffers(1, &id); }
};

using Buffer = Object<BufferTraits>;           ///< An owned buffer.
using VertexArray = Object<VertexArrayTraits>; ///< An owned vertex array.
using Program = Object<ProgramTraits>;         ///< An owned shader program.
using Texture = Object<TextureTraits>;         ///< An owned texture.
using Framebuffer = Object<FramebufferTraits>; ///< An owned framebuffer object.

} // namespace affresco::gl
