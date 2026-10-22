#pragma once

/// @file
/// @brief Meshes and textures on the GPU: uploaded from an Assets store on first use, re-uploaded when the store's
/// data under an id changes, and the least recently used freed when over a budget.

#include "affresco/scene3d/Mesh.hpp"
#include "affresco/scene3d/Scene.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>

namespace affresco::scene3d::detail {

/// @brief A mesh on the GPU.
struct GpuMesh {
	unsigned vertexArray = 0;               ///< Positions (0), normals (1), texture coordinates (2) and triangles.
	unsigned edgeArray = 0;                 ///< The same positions with the edges as indices, or 0 until asked.
	unsigned buffers[5] = {};               ///< Positions, normals, texture coordinates, triangles, edges.
	int indexCount = 0;                     ///< Indices of the triangles.
	int edgeCount = 0;                      ///< Indices of the edges.
	std::size_t bytes = 0;                  ///< On the GPU.
	std::shared_ptr<const MeshData> source; ///< What was uploaded: a new pointer under the id means new data.
	std::uint64_t lastUse = 0;              ///< The frame it was last drawn in.
};

/// @brief A texture on the GPU.
struct GpuTexture {
	unsigned texture = 0;                      ///< The GL name.
	std::size_t bytes = 0;                     ///< On the GPU, with its mipmaps.
	std::shared_ptr<const TextureData> source; ///< What was uploaded.
	std::uint64_t lastUse = 0;                 ///< The frame it was last drawn in.
};

/// @brief The uploaded meshes and textures of one renderer.
class GpuResources {
public:
	/// @brief Resources within a budget. @param budget Bytes.
	explicit GpuResources(std::size_t budget) noexcept : budget_(budget) {}
	~GpuResources() = default;
	GpuResources(const GpuResources&) = delete;
	GpuResources& operator=(const GpuResources&) = delete;

	/// @brief A mesh, uploaded if needed.
	/// @param id Its id. @param assets The store. @param frame This frame's number. @param edges Whether its edges are
	/// needed too.
	/// @return It, or null if the store has none (or it is invalid).
	[[nodiscard]] const GpuMesh* mesh(std::string_view id, const Assets& assets, std::uint64_t frame, bool edges);
	/// @brief A texture, uploaded if needed.
	/// @param id Its id. @param assets The store. @param frame This frame's number.
	/// @return The GL name, or 0 if the store has none.
	[[nodiscard]] unsigned texture(std::string_view id, const Assets& assets, std::uint64_t frame);
	/// @brief Frees the least recently used, not used in this frame, while over the budget.
	/// @param frame This frame's number.
	void trim(std::uint64_t frame);
	/// @brief Frees everything (with the context current).
	void release() noexcept;
	/// @brief Bytes on the GPU. @return Them.
	[[nodiscard]] std::size_t bytes() const noexcept { return bytes_; }
	/// @brief Meshes and textures freed by trim() so far. @return The count.
	[[nodiscard]] std::size_t evicted() const noexcept { return evicted_; }

private:
	std::size_t budget_;                                      ///< See the constructor.
	std::size_t bytes_ = 0;                                   ///< See bytes().
	std::size_t evicted_ = 0;                                 ///< See evicted().
	std::map<std::string, GpuMesh, std::less<>> meshes_;      ///< By id.
	std::map<std::string, GpuTexture, std::less<>> textures_; ///< By id.
};

} // namespace affresco::scene3d::detail
