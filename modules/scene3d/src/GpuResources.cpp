/// @file
/// @brief Uploading meshes and textures, and freeing them within a budget (GpuResources.hpp).

#include "GpuResources.hpp"

#include <glad/glad.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace affresco::scene3d::detail {

namespace {

/// @brief Uploads an array into a new buffer. @param target GL_ARRAY_BUFFER or GL_ELEMENT_ARRAY_BUFFER.
/// @param bytes Its size. @param data Its first byte. @return The buffer.
GLuint upload(GLenum target, std::size_t bytes, const void* data) {
	GLuint buffer = 0;
	glGenBuffers(1, &buffer);
	glBindBuffer(target, buffer);
	glBufferData(target, static_cast<GLsizeiptr>(bytes), data, GL_STATIC_DRAW);
	return buffer;
}

/// @brief Points an attribute at the bound array buffer. @param location It. @param components Floats per vertex.
void attribute(GLuint location, GLint components) {
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location, components, GL_FLOAT, GL_FALSE, 0, nullptr);
}

/// @brief Frees a mesh's GL objects. @param mesh It.
void destroy(GpuMesh& mesh) noexcept {
	glDeleteVertexArrays(1, &mesh.vertexArray);
	if(mesh.edgeArray != 0) glDeleteVertexArrays(1, &mesh.edgeArray);
	for(auto& buffer : mesh.buffers)
		if(buffer != 0) glDeleteBuffers(1, &buffer);
	mesh = {};
}

/// @brief Uploads a mesh's triangles; normals are computed if it has none.
/// @param data The mesh. @return It on the GPU.
GpuMesh uploadMesh(const std::shared_ptr<const MeshData>& data) {
	GpuMesh mesh;
	mesh.source = data;
	const MeshData* source = data.get();
	MeshData withNormals;
	if(source->normals.size() != source->positions.size()) {
		withNormals = *source;
		withNormals.computeNormals();
		source = &withNormals;
	}
	glGenVertexArrays(1, &mesh.vertexArray);
	glBindVertexArray(mesh.vertexArray);
	const std::size_t vertexBytes = source->positions.size() * sizeof(Vec3);
	mesh.buffers[0] = upload(GL_ARRAY_BUFFER, vertexBytes, source->positions.data());
	attribute(0, 3);
	mesh.buffers[1] = upload(GL_ARRAY_BUFFER, vertexBytes, source->normals.data());
	attribute(1, 3);
	mesh.bytes = vertexBytes * 2;
	if(source->texCoords.size() == source->positions.size()) {
		const std::size_t bytes = source->texCoords.size() * sizeof(TexCoord);
		mesh.buffers[2] = upload(GL_ARRAY_BUFFER, bytes, source->texCoords.data());
		attribute(2, 2);
		mesh.bytes += bytes;
	}
	const std::size_t indexBytes = source->indices.size() * sizeof(std::uint32_t);
	mesh.buffers[3] = upload(GL_ELEMENT_ARRAY_BUFFER, indexBytes, source->indices.data());
	mesh.bytes += indexBytes;
	mesh.indexCount = static_cast<int>(source->indices.size());
	glBindVertexArray(0);
	return mesh;
}

/// @brief Uploads a mesh's edges, as a second vertex array over its positions. @param mesh It.
void uploadEdges(GpuMesh& mesh) {
	const std::vector<std::uint32_t> edges = mesh.source->edges();
	glGenVertexArrays(1, &mesh.edgeArray);
	glBindVertexArray(mesh.edgeArray);
	glBindBuffer(GL_ARRAY_BUFFER, mesh.buffers[0]);
	attribute(0, 3);
	mesh.buffers[4] = upload(GL_ELEMENT_ARRAY_BUFFER, edges.size() * sizeof(std::uint32_t), edges.data());
	mesh.edgeCount = static_cast<int>(edges.size());
	mesh.bytes += edges.size() * sizeof(std::uint32_t);
	glBindVertexArray(0);
}

/// @brief Uploads a texture with mipmaps, colour as sRGB. @param data It. @return It on the GPU.
GpuTexture uploadTexture(const std::shared_ptr<const TextureData>& data) {
	GpuTexture texture;
	texture.source = data;
	glGenTextures(1, &texture.texture);
	glBindTexture(GL_TEXTURE_2D, texture.texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, data->srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8, data->width, data->height, 0, GL_RGBA,
	             GL_UNSIGNED_BYTE, data->pixels.data());
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	texture.bytes = data->pixels.size() * 4 / 3; // with the mipmaps
	return texture;
}

} // namespace

const GpuMesh* GpuResources::mesh(std::string_view id, const Assets& assets, std::uint64_t frame, bool edges) {
	const auto data = assets.mesh(id);
	if(!data || data->indices.empty() || data->positions.empty()) return nullptr;
	auto found = meshes_.find(id);
	if(found != meshes_.end() && found->second.source != data) {
		bytes_ -= found->second.bytes;
		destroy(found->second);
		meshes_.erase(found);
		found = meshes_.end();
	}
	if(found == meshes_.end()) {
		found = meshes_.emplace(std::string(id), uploadMesh(data)).first;
		bytes_ += found->second.bytes;
	}
	GpuMesh& mesh = found->second;
	if(edges && mesh.edgeArray == 0) {
		const std::size_t before = mesh.bytes;
		uploadEdges(mesh);
		bytes_ += mesh.bytes - before;
	}
	mesh.lastUse = frame;
	return &mesh;
}

unsigned GpuResources::texture(std::string_view id, const Assets& assets, std::uint64_t frame) {
	if(id.empty()) return 0;
	const auto data = assets.texture(id);
	if(!data || data->width <= 0 || data->height <= 0 ||
	   data->pixels.size() != static_cast<std::size_t>(data->width) * static_cast<std::size_t>(data->height) * 4)
		return 0;
	auto found = textures_.find(id);
	if(found != textures_.end() && found->second.source != data) {
		bytes_ -= found->second.bytes;
		glDeleteTextures(1, &found->second.texture);
		textures_.erase(found);
		found = textures_.end();
	}
	if(found == textures_.end()) {
		found = textures_.emplace(std::string(id), uploadTexture(data)).first;
		bytes_ += found->second.bytes;
	}
	found->second.lastUse = frame;
	return found->second.texture;
}

void GpuResources::trim(std::uint64_t frame) {
	while(bytes_ > budget_) {
		auto oldestMesh = meshes_.end();
		auto oldestTexture = textures_.end();
		std::uint64_t oldest = std::numeric_limits<std::uint64_t>::max();
		for(auto it = meshes_.begin(); it != meshes_.end(); ++it)
			if(it->second.lastUse < frame && it->second.lastUse < oldest) oldest = (oldestMesh = it)->second.lastUse;
		for(auto it = textures_.begin(); it != textures_.end(); ++it)
			if(it->second.lastUse < frame && it->second.lastUse < oldest) {
				oldest = (oldestTexture = it)->second.lastUse;
				oldestMesh = meshes_.end();
			}
		if(oldestTexture != textures_.end()) {
			bytes_ -= oldestTexture->second.bytes;
			glDeleteTextures(1, &oldestTexture->second.texture);
			textures_.erase(oldestTexture);
		} else if(oldestMesh != meshes_.end()) {
			bytes_ -= oldestMesh->second.bytes;
			destroy(oldestMesh->second);
			meshes_.erase(oldestMesh);
		} else return; // everything left is in use this frame
		++evicted_;
	}
}

void GpuResources::release() noexcept {
	for(auto& [id, mesh] : meshes_) destroy(mesh);
	for(auto& [id, texture] : textures_) glDeleteTextures(1, &texture.texture);
	meshes_.clear();
	textures_.clear();
	bytes_ = 0;
}

} // namespace affresco::scene3d::detail
