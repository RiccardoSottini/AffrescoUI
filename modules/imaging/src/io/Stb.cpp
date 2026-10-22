/// @file
/// @brief stb_image (PNG and JPEG, no file access) and stb_image_write (no file access), compiled static for this
/// module only, with stb_image's buffers bounded by what the header promises, as render's Bitmap does.

#include "Stb.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace {

/// @brief The most stb_image may grow a buffer to on this thread, set before each call.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): stb_image's allocator macros take no context
thread_local std::size_t growthLimit = 0;

/// @brief stb_image's realloc, refused beyond growthLimit, so a small file can't inflate to gigabytes.
/// @param pointer The buffer.
/// @param bytes Its new size.
/// @return The buffer, or nullptr (stb_image then fails).
void* growWithin(void* pointer, std::size_t bytes) noexcept {
	// NOLINTNEXTLINE(cppcoreguidelines-no-malloc): stb_image frees its buffers with free()
	return bytes > growthLimit ? nullptr : std::realloc(pointer, bytes);
}

} // namespace

// stb casts in C style where its macros are defined (here), so the warning is off around the includes
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#define STBI_MALLOC(bytes) std::malloc(bytes)
#define STBI_FREE(pointer) std::free(pointer)
#define STBI_REALLOC_SIZED(pointer, oldBytes, newBytes) growWithin(pointer, newBytes)
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS (1 << 16) // the limits are lower; this only bounds stb_image's own arithmetic
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

namespace affresco::imaging::stb {

namespace {

/// @brief Appends what stb_image_write gives to a vector.
/// @param context The vector.
/// @param data The bytes.
/// @param size How many.
void append(void* context, void* data, int size) {
	auto& out = *static_cast<std::vector<std::uint8_t>*>(context);
	const auto* bytes = static_cast<const std::uint8_t*>(data);
	out.insert(out.end(), bytes, bytes + size);
}

/// @brief The length stb takes, if the span fits an int.
/// @param bytes The span.
[[nodiscard]] std::optional<int> lengthOf(std::span<const std::uint8_t> bytes) noexcept {
	if(bytes.size() > static_cast<std::size_t>(INT_MAX)) return std::nullopt;
	return static_cast<int>(bytes.size());
}

} // namespace

std::optional<Header> header(std::span<const std::uint8_t> file) noexcept {
	const auto length = lengthOf(file);
	Header found;
	int channels = 0;
	if(!length || stbi_info_from_memory(file.data(), *length, &found.width, &found.height, &channels) == 0)
		return std::nullopt;
	found.sixteenBit = stbi_is_16_bit_from_memory(file.data(), *length) != 0;
	return found;
}

std::optional<Decoded> decode(std::span<const std::uint8_t> file, const Header& header) {
	const auto length = lengthOf(file);
	if(!length) return std::nullopt;
	const auto pixels = static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height);
	// The image and PNG's filter bytes, at 16 bits, and the slack stb_image grows by
	growthLimit = (pixels + static_cast<std::size_t>(header.height)) * 8 + (std::size_t{1} << 20);
	Decoded decoded;
	int channels = 0;
	if(header.sixteenBit) {
		stbi_us* data =
		    stbi_load_16_from_memory(file.data(), *length, &decoded.width, &decoded.height, &channels, STBI_rgb_alpha);
		const bool same = data != nullptr && decoded.width == header.width && decoded.height == header.height;
		if(same) decoded.bytes16.assign(data, data + pixels * 4); // only as many pixels as were decoded
		stbi_image_free(data);
		if(!same) return std::nullopt;
	} else {
		stbi_uc* data =
		    stbi_load_from_memory(file.data(), *length, &decoded.width, &decoded.height, &channels, STBI_rgb_alpha);
		const bool same = data != nullptr && decoded.width == header.width && decoded.height == header.height;
		if(same) decoded.bytes8.assign(data, data + pixels * 4);
		stbi_image_free(data);
		if(!same) return std::nullopt;
	}
	return decoded;
}

std::optional<std::vector<std::uint8_t>> inflate(std::span<const std::uint8_t> stream, std::size_t most) {
	const auto length = lengthOf(stream);
	if(!length || most >= static_cast<std::size_t>(INT_MAX)) return std::nullopt;
	// One byte more than allowed, so a stream that fills it is known to be too long
	std::vector<std::uint8_t> out(most + 1);
	const int made = stbi_zlib_decode_buffer(reinterpret_cast<char*>(out.data()), static_cast<int>(out.size()),
	                                         reinterpret_cast<const char*>(stream.data()), *length);
	if(made < 0 || std::cmp_greater(made, most)) return std::nullopt;
	out.resize(static_cast<std::size_t>(made));
	return out;
}

std::vector<std::uint8_t> deflate(std::span<const std::uint8_t> bytes) {
	const auto length = lengthOf(bytes);
	if(!length) return {};
	int made = 0;
	// stb_image_write takes a mutable pointer but only reads it
	unsigned char* stream = stbi_zlib_compress(const_cast<unsigned char*>(bytes.data()), *length, &made, 8);
	if(stream == nullptr) return {};
	std::vector<std::uint8_t> out(stream, stream + made);
	STBIW_FREE(stream);
	return out;
}

bool pngFits(int width, int height) noexcept {
	if(width <= 0 || height <= 0) return false;
	const auto filtered = (std::size_t{4} * static_cast<std::size_t>(width) + 1) * static_cast<std::size_t>(height);
	return filtered <= static_cast<std::size_t>(INT_MAX);
}

bool jpegFits(int width, int height) noexcept {
	if(width <= 0 || height <= 0 || width > 0xffff || height > 0xffff) return false;
	return std::size_t{4} * static_cast<std::size_t>(width) * static_cast<std::size_t>(height) <=
	       static_cast<std::size_t>(INT_MAX);
}

std::vector<std::uint8_t> writePng(std::span<const std::uint8_t> rgba, int width, int height) {
	std::vector<std::uint8_t> out;
	if(!pngFits(width, height)) return out;
	if(stbi_write_png_to_func(append, &out, width, height, 4, rgba.data(), width * 4) == 0) out.clear();
	return out;
}

std::vector<std::uint8_t> writeJpeg(std::span<const std::uint8_t> rgba, int width, int height, int quality) {
	std::vector<std::uint8_t> out;
	if(!jpegFits(width, height)) return out;
	if(stbi_write_jpg_to_func(append, &out, width, height, 4, rgba.data(), std::clamp(quality, 1, 100)) == 0)
		out.clear();
	return out;
}

} // namespace affresco::imaging::stb
