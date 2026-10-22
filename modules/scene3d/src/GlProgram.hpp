#pragma once

/// @file
/// @brief A linked GLSL program built from fixed source parts, with its uniforms' locations cached.

#include <initializer_list>
#include <map>
#include <string>
#include <string_view>

namespace affresco::scene3d::detail {

/// @brief A program. Owns its GL name: release() frees it while the context is current.
class GlProgram {
public:
	/// @brief No program.
	GlProgram() = default;
	/// @brief Compiles and links one.
	/// @param prelude The version line (SurfaceSetup::prelude), first in each shader.
	/// @param vertex The vertex shader's parts, after the prelude.
	/// @param fragment The fragment shader's parts, after the prelude.
	/// @throws std::runtime_error With the log, if it doesn't compile or link.
	GlProgram(std::string_view prelude, std::initializer_list<std::string_view> vertex,
	          std::initializer_list<std::string_view> fragment);
	~GlProgram() { release(); }
	GlProgram(const GlProgram&) = delete;
	GlProgram& operator=(const GlProgram&) = delete;
	/// @brief Takes another's program. @param other It, left empty.
	GlProgram(GlProgram&& other) noexcept;
	/// @brief Frees this one's program and takes another's. @param other It, left empty. @return This.
	GlProgram& operator=(GlProgram&& other) noexcept;

	/// @brief Frees the program.
	void release() noexcept;
	/// @brief Whether there is one. @return true if linked.
	[[nodiscard]] bool valid() const noexcept { return program_ != 0; }
	/// @brief Makes it current.
	void use() const noexcept;
	/// @brief A uniform's location, cached. @param name Its name. @return It, or -1 if the program has none.
	[[nodiscard]] int uniform(std::string_view name) const;

private:
	unsigned program_ = 0;                                      ///< The GL name, or 0.
	mutable std::map<std::string, int, std::less<>> locations_; ///< Looked up so far.
};

} // namespace affresco::scene3d::detail
