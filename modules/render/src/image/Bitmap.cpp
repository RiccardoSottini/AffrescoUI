/// @file
/// @brief Bitmap: decoding SVG icons (vector/Svg.hpp), and PNG and JPEG with stb_image (compiled here, PNG and JPEG
/// only, no file access of its own), within limits checked before any pixel is decoded.

#include "affresco/render/Bitmap.hpp"

#include "vector/Svg.hpp"

#include <cstdlib>

namespace {

/// @brief The most stb_image may grow a buffer to on this thread, set from the image's header before it decodes.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): stb_image's allocator macros take no context
thread_local std::size_t growthLimit = 0;

/// @brief stb_image's realloc, refused beyond growthLimit: what a PNG inflates to is otherwise bounded only by 4 GiB,
/// whatever its header says, so a small file could make it allocate gigabytes.
/// @param pointer The buffer.
/// @param bytes Its new size.
/// @return The buffer, or nullptr (stb_image then fails the image).
void* growWithin(void* pointer, std::size_t bytes) noexcept {
	// NOLINTNEXTLINE(cppcoreguidelines-no-malloc): stb_image frees its buffers with free()
	return bytes > growthLimit ? nullptr : std::realloc(pointer, bytes);
}

} // namespace

// stb_image casts what these macros return in C style, and GCC reports it where they are defined (here, outside the
// system header), so the warning is off from the definitions to the end of the include (MSVC has no such warning,
// and warns about pragmas it doesn't know).
#if defined(__GNUC__)
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
#define STBI_MAX_DIMENSIONS (1 << 16) // the limits below are lower; this only bounds stb_image's own arithmetic
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace affresco {

namespace {

/// @brief Frees what stb_image decoded, for std::unique_ptr.
struct StbDeleter {
	/// @brief Frees it.
	/// @param pixels What stbi_load_from_memory returned.
	void operator()(stbi_uc* pixels) const noexcept { stbi_image_free(pixels); }
};

/// @brief Checks a size against the limits.
/// @param width The width.
/// @param height The height.
/// @param limits The limits.
/// @return What is wrong, or nullptr if nothing is.
const char* sizeProblem(int width, int height, const Bitmap::Limits& limits) noexcept {
	if(width < 1 || height < 1) return "it has no pixels";
	if(width > limits.side || height > limits.side) return "it is wider or taller than allowed";
	if(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) > limits.pixels)
		return "it has more pixels than allowed";
	return nullptr;
}

/// @brief Premultiplies pixels by their alpha, rounding to the nearest value, as the renderer filters them.
/// @param rgba Red, green, blue and alpha per pixel.
/// @return The premultiplied pixels.
std::vector<std::uint8_t> premultiplied(std::span<const std::uint8_t> rgba) {
	std::vector<std::uint8_t> pixels(rgba.begin(), rgba.end());
	for(std::size_t i = 0; i + 3 < pixels.size(); i += 4) {
		const unsigned alpha = pixels[i + 3];
		if(alpha == 255) continue;
		for(std::size_t channel = 0; channel < 3; ++channel)
			pixels[i + channel] = static_cast<std::uint8_t>((pixels[i + channel] * alpha + 127) / 255);
	}
	return pixels;
}

/// @brief Tells whether a PNG's first IDAT chunk is empty. stb_image copies such a chunk from a null buffer (undefined
/// behaviour found by fuzzing), so it is refused before decoding. Other data, and a broken chunk list, are left to
/// stb_image.
/// @param bytes The image file's bytes.
/// @return Whether the bytes are a PNG whose first IDAT chunk has no data.
bool emptyFirstImageData(std::span<const std::uint8_t> bytes) noexcept {
	static constexpr std::uint8_t signature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
	if(bytes.size() < sizeof signature || !std::equal(std::begin(signature), std::end(signature), bytes.begin()))
		return false;
	std::size_t at = sizeof signature;
	while(bytes.size() - at >= 8) { // a length and a type
		std::uint32_t length = 0;
		for(std::size_t i = 0; i < 4; ++i) length = (length << 8) | bytes[at + i];
		if(std::equal(bytes.begin() + static_cast<std::ptrdiff_t>(at + 4),
		              bytes.begin() + static_cast<std::ptrdiff_t>(at + 8), "IDAT"))
			return length == 0;
		if(length > 0x7FFFFFFFu || bytes.size() - at - 8 < std::size_t{length} + 4) return false; // its data and CRC
		at += 12 + std::size_t{length};
	}
	return false;
}

} // namespace

Bitmap::Bitmap(Key /*key*/, int width, int height, std::vector<std::uint8_t> pixels)
    : width_(width), height_(height), pixels_(std::move(pixels)) {
	static std::atomic<std::uint64_t> nextId = 1; // images are decoded on worker threads
	id_ = nextId++;
}

std::shared_ptr<const Bitmap> Bitmap::load(const std::filesystem::path& file, const Limits& limits) {
	// Only regular files, and not too large: a revision can name any path, such as a device that never ends
	std::error_code error;
	const bool regular = std::filesystem::is_regular_file(file, error);
	const std::uintmax_t size = regular ? std::filesystem::file_size(file, error) : 0;
	if(!regular || error) throw std::runtime_error("AffrescoUI: could not open image file " + file.string());
	if(size > limits.fileBytes)
		throw std::runtime_error("AffrescoUI: image file larger than " + std::to_string(limits.fileBytes >> 20) +
		                         " MiB: " + file.string());

	std::ifstream stream(file, std::ios::binary);
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
	stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
	if(!stream || stream.gcount() != static_cast<std::streamsize>(size))
		throw std::runtime_error("AffrescoUI: could not read image file " + file.string());
	try {
		return decode(bytes, limits);
	} catch(const std::runtime_error& failure) {
		throw std::runtime_error(std::string(failure.what()) + ": " + file.string());
	}
}

std::shared_ptr<const Bitmap> Bitmap::decode(std::span<const std::uint8_t> bytes, const Limits& limits) {
	if(bytes.size() > limits.fileBytes || bytes.size() > static_cast<std::size_t>(INT_MAX))
		throw std::runtime_error("AffrescoUI: image larger than allowed");
	if(vector::looksLikeSvg(bytes)) return vector::rasterizeSvg(bytes, limits);
	const auto length = static_cast<int>(bytes.size());
	if(emptyFirstImageData(bytes)) throw std::runtime_error("AffrescoUI: broken image (an empty first IDAT chunk)");

	// The size first, from the header: nothing is decoded for an image beyond the limits
	int width = 0, height = 0, channels = 0;
	if(!stbi_info_from_memory(bytes.data(), length, &width, &height, &channels))
		throw std::runtime_error(std::string("AffrescoUI: not a PNG or JPEG image (") + stbi_failure_reason() + ")");
	if(const char* problem = sizeProblem(width, height, limits))
		throw std::runtime_error(std::string("AffrescoUI: image refused: ") + problem);

	// Buffers grow by doubling, to twice the compressed bytes or twice the rows at 16 bits and 4 channels at most
	const auto tall = static_cast<std::size_t>(height);
	const std::size_t rows = static_cast<std::size_t>(width) * tall * 8 + tall;
	growthLimit = 2 * (bytes.size() + rows) + (std::size_t{1} << 20);
	int decodedWidth = 0, decodedHeight = 0;
	const std::unique_ptr<stbi_uc, StbDeleter> decoded(
	    stbi_load_from_memory(bytes.data(), length, &decodedWidth, &decodedHeight, &channels, 4));
	growthLimit = 0;
	if(!decoded) throw std::runtime_error(std::string("AffrescoUI: broken image (") + stbi_failure_reason() + ")");
	if(decodedWidth != width || decodedHeight != height)
		throw std::runtime_error("AffrescoUI: broken image (its size changed while decoding)");

	const std::size_t count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
	return std::make_shared<const Bitmap>(Key{}, width, height, premultiplied({decoded.get(), count}));
}

std::shared_ptr<const Bitmap> Bitmap::fromPixels(int width, int height, std::span<const std::uint8_t> rgba,
                                                 const Limits& limits) {
	if(const char* problem = sizeProblem(width, height, limits))
		throw std::invalid_argument(std::string("AffrescoUI: image refused: ") + problem);
	if(rgba.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4)
		throw std::invalid_argument("AffrescoUI: image refused: its pixels aren't width * height * 4 bytes");
	return std::make_shared<const Bitmap>(Key{}, width, height, premultiplied(rgba));
}

} // namespace affresco
