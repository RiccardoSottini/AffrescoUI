/// @file
/// @brief Loading a glTF file: limits, buffers (GLB chunk, `data:` URIs, safe relative files), images, and the
/// calls into GltfConvert.cpp that turn it into a Scene.

#include "GltfInternal.hpp"

#include "affresco/render/Bitmap.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <set>

namespace affresco::scene3d {

namespace {

/// @brief Frees a cgltf_data and the buffers it loaded.
struct CgltfFree {
	/// @brief Frees it. @param data It, or null.
	void operator()(cgltf_data* data) const noexcept { cgltf_free(data); }
};

/// @brief The extensions this loader reads; others used by a file are named in a warning.
constexpr std::array<std::string_view, 2> supportedExtensions{"KHR_lights_punctual", "KHR_materials_emissive_strength"};

/// @brief A cgltf result in words. @param result It. @return E.g. "invalid JSON".
[[nodiscard]] const char* resultText(cgltf_result result) noexcept {
	switch(result) {
	case cgltf_result_data_too_short:
		return "data too short";
	case cgltf_result_unknown_format:
		return "unknown format";
	case cgltf_result_invalid_json:
		return "invalid JSON";
	case cgltf_result_invalid_gltf:
		return "invalid glTF";
	case cgltf_result_out_of_memory:
		return "out of memory";
	case cgltf_result_legacy_gltf:
		return "glTF 1.0 is not supported";
	default:
		return "unreadable";
	}
}

/// @brief Fails if a count is over its limit. @param count It. @param limit The limit. @param what Names it.
void checkCount(std::size_t count, std::size_t limit, const char* what) {
	if(count > limit)
		throw GltfError("glTF: " + std::to_string(count) + " " + what + ", more than " + std::to_string(limit));
}

/// @brief Checks the counts of everything against the limits, before anything is read.
/// @param data The parsed file. @param limits The limits.
void checkCounts(const cgltf_data& data, const GltfLimits& limits) {
	checkCount(data.nodes_count, limits.nodes, "nodes");
	checkCount(data.meshes_count, limits.meshes, "meshes");
	checkCount(data.materials_count, limits.materials, "materials");
	checkCount(data.images_count, limits.images, "images");
	checkCount(data.animations_count, limits.animations, "animations");
	std::size_t primitives = 0;
	for(std::size_t i = 0; i < data.meshes_count; ++i) primitives += data.meshes[i].primitives_count;
	checkCount(primitives, limits.meshes, "primitives");
	std::size_t bytes = 0;
	for(std::size_t i = 0; i < data.buffers_count; ++i) {
		checkCount(data.buffers[i].size, limits.fileBytes, "bytes in a buffer");
		bytes += data.buffers[i].size;
	}
	checkCount(bytes, limits.bufferBytes, "bytes in buffers");
}

/// @brief Whether `count` items of `size` bytes, after `offset`, end within `end`, without wrapping around.
/// @param offset The first byte. @param count The items. @param size The bytes of each. @param end The bytes there are.
/// @return true if so.
[[nodiscard]] bool fits(std::size_t offset, std::size_t count, std::size_t size, std::size_t end) noexcept {
	return offset <= end && (size == 0 || count <= (end - offset) / size);
}

/// @brief Whether an accessor's elements lie within its buffer view. @param accessor It. @return true if so.
[[nodiscard]] bool viewFits(const cgltf_accessor& accessor) noexcept {
	const cgltf_buffer_view* view = accessor.buffer_view;
	if(view == nullptr || accessor.count == 0) return true;
	if(!fits(accessor.offset, accessor.count - 1, accessor.stride, view->size)) return false;
	return cgltf_calc_size(accessor.type, accessor.component_type) <=
	       view->size - accessor.offset - accessor.stride * (accessor.count - 1);
}

/// @brief Whether a sparse accessor's indices and values lie within their buffer views. @param accessor It.
/// @return true if so, or if it isn't sparse.
[[nodiscard]] bool sparseFits(const cgltf_accessor& accessor) noexcept {
	const cgltf_accessor_sparse& sparse = accessor.sparse;
	if(!accessor.is_sparse || sparse.indices_buffer_view == nullptr || sparse.values_buffer_view == nullptr)
		return true;
	return fits(sparse.indices_byte_offset, sparse.count, cgltf_component_size(sparse.indices_component_type),
	            sparse.indices_buffer_view->size) &&
	       fits(sparse.values_byte_offset, sparse.count, cgltf_calc_size(accessor.type, accessor.component_type),
	            sparse.values_buffer_view->size);
}

/// @brief Whether data at `offset` in a buffer view starts on a multiple of `size` in memory, as glTF requires (cgltf
/// reads it through typed pointers). @param view The view, or null. @param offset The offset in it. @param size The
/// component's bytes. @return true if so, or if there's no loaded view.
[[nodiscard]] bool aligned(const cgltf_buffer_view* view, std::size_t offset, std::size_t size) noexcept {
	if(view == nullptr || view->buffer == nullptr || view->buffer->data == nullptr || size <= 1) return true;
	const auto start = reinterpret_cast<std::uintptr_t>(view->buffer->data) + view->offset + offset;
	return start % size == 0;
}

/// @brief Whether an accessor's data, and its sparse indices and values, are aligned to their components.
/// @param accessor It. @return true if so.
[[nodiscard]] bool accessorAligned(const cgltf_accessor& accessor) noexcept {
	const std::size_t size = cgltf_component_size(accessor.component_type);
	if(!aligned(accessor.buffer_view, accessor.offset, size) || (size > 1 && accessor.stride % size != 0)) return false;
	if(!accessor.is_sparse) return true;
	const cgltf_accessor_sparse& sparse = accessor.sparse;
	return aligned(sparse.indices_buffer_view, sparse.indices_byte_offset,
	               cgltf_component_size(sparse.indices_component_type)) &&
	       aligned(sparse.values_buffer_view, sparse.values_byte_offset, size);
}

/// @brief Checks that every accessor's data lies within its buffer views, aligned to its components.
///
/// cgltf_validate does the same sums unchecked, so a huge stride or count wraps them around to a small size that
/// passes, and the reads then run far past the buffer. A buffer view's own sum can't wrap: cgltf parses its offset and
/// size as at most 2^63 - 1 each. It also reads indices through typed pointers, so misaligned ones are refused first.
/// @param data The parsed file. @throws GltfError If one doesn't fit or isn't aligned.
void checkRanges(const cgltf_data& data) {
	for(std::size_t i = 0; i < data.accessors_count; ++i) {
		if(!viewFits(data.accessors[i]) || !sparseFits(data.accessors[i])) throw GltfError("glTF: data too short");
		if(!accessorAligned(data.accessors[i])) throw GltfError("glTF: an accessor isn't aligned to its components");
	}
}

/// @brief A relative file in a directory, refusing one whose real path (after links) leaves it.
/// @param directory The directory, not empty. @param uri The percent-decoded URI.
/// @return The path. @throws GltfError If refused.
[[nodiscard]] std::filesystem::path safeFile(const std::filesystem::path& directory, std::string_view uri) {
	if(directory.empty() || !isSafeRelativeUri(uri))
		throw GltfError("glTF: refused the file \"" + std::string(uri) + "\": only relative files beside it are read");
	std::error_code error;
	const auto root = std::filesystem::weakly_canonical(directory, error);
	const auto file = std::filesystem::weakly_canonical(directory / std::filesystem::path(std::string(uri)), error);
	const auto [end, _] = std::ranges::mismatch(root, file);
	if(error || end != root.end())
		throw GltfError("glTF: refused the file \"" + std::string(uri) + "\": it leads outside its directory");
	return file;
}

/// @brief The percent-decoded copy of a URI. @param uri It. @return The decoded text.
[[nodiscard]] std::string decodedUri(const char* uri) {
	std::string text(uri);
	text.push_back('\0');
	cgltf_decode_uri(text.data()); // decoding shortens the text in place and ends it with a NUL
	text.erase(text.find('\0'));
	return text;
}

/// @brief The bytes of a `data:` URI with base64 content.
/// @param options The parse options (cgltf allocates with them). @param uri The URI. @param size The bytes wanted, or
/// 0 for all the text holds. @param maxBytes The largest accepted.
/// @return The bytes, malloc'd (as cgltf frees them). @throws GltfError If malformed or too large.
[[nodiscard]] std::pair<void*, std::size_t> dataUri(const cgltf_options& options, std::string_view uri,
                                                    std::size_t size, std::size_t maxBytes) {
	const auto comma = uri.find(";base64,");
	if(!uri.starts_with("data:") || comma == std::string_view::npos)
		throw GltfError("glTF: a data: URI that isn't base64");
	const std::string base64(uri.substr(comma + 8));
	if(size == 0) {
		const std::size_t padding = static_cast<std::size_t>(std::count(base64.begin(), base64.end(), '='));
		const std::size_t whole = base64.size() / 4 * 3;
		size = whole - std::min({padding, std::size_t{2}, whole});
	}
	if(size > maxBytes || size == 0)
		throw GltfError("glTF: a data: URI of " + std::to_string(size) + " bytes is refused");
	void* bytes = nullptr;
	if(cgltf_load_buffer_base64(&options, size, base64.c_str(), &bytes) != cgltf_result_success)
		throw GltfError("glTF: a data: URI is shorter than its buffer or not base64");
	return {bytes, size};
}

/// @brief Loads one buffer's bytes.
/// @param options The parse options. @param data The file. @param buffer The buffer. @param directory Where
/// relative files are. @param limits The limits.
void loadBuffer(const cgltf_options& options, const cgltf_data& data, cgltf_buffer& buffer,
                const std::filesystem::path& directory, const GltfLimits& limits) {
	if(buffer.uri == nullptr) {
		// The GLB's own chunk, if this is the first buffer; otherwise a meshopt fallback that stays unloaded
		if(&buffer == data.buffers && data.bin != nullptr) {
			if(data.bin_size < buffer.size) throw GltfError("glTF: the binary chunk is shorter than its buffer");
			// NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast): cgltf's field is mutable; this one is only read
			buffer.data = const_cast<void*>(data.bin);
			buffer.data_free_method = cgltf_data_free_method_none;
		}
		return;
	}
	if(std::string_view(buffer.uri).starts_with("data:")) {
		buffer.data = dataUri(options, buffer.uri, buffer.size, limits.fileBytes).first;
		buffer.data_free_method = cgltf_data_free_method_memory_free;
		return;
	}
	const auto bytes = detail::readLimited(safeFile(directory, decodedUri(buffer.uri)), limits.fileBytes);
	if(bytes.size() < buffer.size)
		throw GltfError("glTF: the file \"" + std::string(buffer.uri) + "\" is shorter than its buffer");
	// NOLINTNEXTLINE(cppcoreguidelines-no-malloc): cgltf frees it with free()
	void* copy = std::malloc(std::max<std::size_t>(buffer.size, 1));
	if(copy == nullptr) throw GltfError("glTF: out of memory");
	std::memcpy(copy, bytes.data(), buffer.size);
	buffer.data = copy;
	buffer.data_free_method = cgltf_data_free_method_memory_free;
}

/// @brief Names the extensions a file uses that this loader doesn't read. @param context The load.
void warnExtensions(detail::GltfContext& context) {
	for(std::size_t i = 0; i < context.data.extensions_used_count; ++i) {
		const std::string_view name(context.data.extensions_used[i]);
		if(std::ranges::find(supportedExtensions, name) == supportedExtensions.end())
			detail::warn(context, "the extension " + std::string(name) + " is not supported: what needs it is skipped");
	}
}

/// @brief The encoded bytes of an image, from a buffer view, a `data:` URI or a relative file.
/// @param context The load. @param image The image.
/// @return The bytes. @throws GltfError If they can't be had.
[[nodiscard]] std::vector<std::uint8_t> imageBytes(const detail::GltfContext& context, const cgltf_image& image) {
	if(image.buffer_view != nullptr) {
		const cgltf_buffer_view& view = *image.buffer_view;
		if(view.buffer->data == nullptr) throw GltfError("its buffer isn't loaded");
		const auto* begin = static_cast<const std::uint8_t*>(view.buffer->data) + view.offset;
		return {begin, begin + view.size};
	}
	if(image.uri == nullptr) throw GltfError("it has no data");
	if(std::string_view(image.uri).starts_with("data:")) {
		const cgltf_options options{};
		const auto [bytes, size] = dataUri(options, image.uri, 0, context.limits.fileBytes);
		// NOLINTNEXTLINE(cppcoreguidelines-no-malloc): cgltf allocated it with malloc(); freed even if the copy throws
		const std::unique_ptr<void, decltype(&std::free)> owned(bytes, &std::free);
		const auto* begin = static_cast<const std::uint8_t*>(bytes);
		return {begin, begin + size};
	}
	return detail::readLimited(safeFile(context.directory, decodedUri(image.uri)), context.limits.fileBytes);
}

/// @brief Decodes a PNG or JPEG image (nothing else: Bitmap also reads SVG) into straight-alpha RGBA.
/// @param bytes The file. @param limits The limits. @param srgb Whether it holds colour.
/// @return The texture. @throws std::runtime_error If it can't be decoded.
[[nodiscard]] TextureData decodeImage(std::span<const std::uint8_t> bytes, const GltfLimits& limits, bool srgb) {
	const bool png = bytes.size() > 8 && std::memcmp(bytes.data(), "\x89PNG\r\n\x1a\n", 8) == 0;
	const bool jpeg = bytes.size() > 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF;
	if(!png && !jpeg) throw GltfError("only PNG and JPEG images are read");
	const auto bitmap = Bitmap::decode(bytes, Bitmap::Limits{limits.fileBytes, limits.imageSide, limits.imagePixels});
	TextureData texture{bitmap->width(), bitmap->height(), {bitmap->pixels().begin(), bitmap->pixels().end()}, srgb};
	for(std::size_t i = 0; i + 3 < texture.pixels.size(); i += 4) {
		const unsigned alpha = texture.pixels[i + 3];
		if(alpha == 0 || alpha == 255) continue;
		for(std::size_t c = 0; c < 3; ++c)
			texture.pixels[i + c] =
			    static_cast<std::uint8_t>(std::min(255u, (texture.pixels[i + c] * 255u + alpha / 2) / alpha));
	}
	return texture;
}

} // namespace

namespace detail {

std::vector<std::uint8_t> readLimited(const std::filesystem::path& file, std::size_t maxBytes) {
	std::error_code error;
	const auto size = std::filesystem::file_size(file, error);
	if(error) throw GltfError("glTF: can't read \"" + file.filename().string() + "\"");
	if(size > maxBytes)
		throw GltfError("glTF: \"" + file.filename().string() + "\" is larger than " + std::to_string(maxBytes) +
		                " bytes");
	std::ifstream stream(file, std::ios::binary);
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	if(!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size())))
		throw GltfError("glTF: can't read \"" + file.filename().string() + "\"");
	return bytes;
}

bool accessorInBounds(const cgltf_accessor& accessor) noexcept {
	const auto loaded = [](const cgltf_buffer_view* view) {
		return view == nullptr || view->data != nullptr || view->buffer->data != nullptr;
	};
	if(accessor.is_sparse &&
	   (!loaded(accessor.sparse.indices_buffer_view) || !loaded(accessor.sparse.values_buffer_view)))
		return false;
	return loaded(accessor.buffer_view);
}

bool accessorCompressed(const cgltf_accessor& accessor) noexcept {
	return accessor.buffer_view != nullptr && accessor.buffer_view->has_meshopt_compression &&
	       accessor.buffer_view->data == nullptr;
}

void warn(GltfContext& context, const std::string& warning) {
	auto& warnings = context.out.warnings;
	if(std::ranges::find(warnings, warning) == warnings.end()) warnings.push_back(warning);
}

std::string makeId(const GltfContext& context, std::string_view kind, std::size_t index) {
	return context.prefix + std::string(kind) + std::to_string(index);
}

std::string textureId(GltfContext& context, const cgltf_texture* texture, bool srgb) {
	if(texture == nullptr) return {};
	if(texture->image == nullptr) {
		warn(context, texture->has_basisu ? "KTX2 textures (KHR_texture_basisu) are skipped"
		                                  : "WebP textures (EXT_texture_webp) are skipped");
		return {};
	}
	const auto key = std::make_pair(static_cast<const cgltf_image*>(texture->image), srgb);
	if(const auto found = context.textures.find(key); found != context.textures.end()) return found->second;
	const auto index = static_cast<std::size_t>(texture->image - context.data.images);
	std::string id = makeId(context, "image", index) + (srgb ? "" : "l");
	try {
		context.out.assets.setTexture(id, decodeImage(imageBytes(context, *texture->image), context.limits, srgb));
	} catch(const std::exception& error) {
		warn(context, "image " + std::to_string(index) + " is skipped: " + error.what());
		id.clear();
	}
	context.textures.emplace(key, id);
	return id;
}

} // namespace detail

// NOLINTNEXTLINE(bugprone-exception-escape): substr never starts past the end, so it can't throw
bool isSafeRelativeUri(std::string_view uri) noexcept {
	if(uri.empty() || uri.front() == '/' || uri.find_first_of(std::string_view(":\\\0", 3)) != std::string_view::npos)
		return false;
	std::size_t start = 0;
	while(start <= uri.size()) {
		const auto end = std::min(uri.find('/', start), uri.size());
		if(uri.substr(start, end - start) == "..") return false;
		start = end + 1;
	}
	return true;
}

GltfImport loadGltf(std::span<const std::uint8_t> bytes, const std::filesystem::path& directory,
                    const GltfLimits& limits, std::string_view idPrefix) {
	if(bytes.size() > limits.fileBytes)
		throw GltfError("glTF: the file is larger than " + std::to_string(limits.fileBytes) + " bytes");
	const cgltf_options options{};
	cgltf_data* parsed = nullptr;
	const cgltf_result result = cgltf_parse(&options, bytes.data(), bytes.size(), &parsed);
	const std::unique_ptr<cgltf_data, CgltfFree> data(parsed);
	if(result != cgltf_result_success) throw GltfError(std::string("glTF: ") + resultText(result));
	checkCounts(*data, limits);
	for(std::size_t i = 0; i < data->buffers_count; ++i)
		loadBuffer(options, *data, data->buffers[i], directory, limits);
	checkRanges(*data);
	if(const cgltf_result valid = cgltf_validate(data.get()); valid != cgltf_result_success)
		throw GltfError(std::string("glTF: ") + resultText(valid));
	GltfImport out;
	detail::GltfContext context{*data, directory, limits, std::string(idPrefix), out, {}, {}};
	warnExtensions(context);
	detail::convertMaterials(context);
	detail::convertMeshes(context);
	detail::convertNodes(context);
	detail::convertAnimations(context);
	const SceneLimits sceneLimits{limits.nodes + limits.meshes, limits.materials, limits.depth};
	if(const std::string fault = out.scene.validate(sceneLimits); !fault.empty()) throw GltfError("glTF: " + fault);
	return out;
}

GltfImport loadGltfFile(const std::filesystem::path& file, const GltfLimits& limits, std::string_view idPrefix) {
	const auto bytes = detail::readLimited(file, limits.fileBytes);
	return loadGltf(bytes, file.parent_path().empty() ? std::filesystem::path(".") : file.parent_path(), limits,
	                idPrefix);
}

} // namespace affresco::scene3d
