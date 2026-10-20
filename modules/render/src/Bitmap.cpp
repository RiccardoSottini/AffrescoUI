/// @file
/// @brief Bitmap: decoding PNG and JPEG with stb_image (compiled here, PNG and JPEG only, no file access of its own),
/// within limits checked before any pixel is decoded.

#include "affresco/render/Bitmap.hpp"

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#define STBI_MAX_DIMENSIONS (1 << 16) // the limits below are lower; this only bounds stb_image's own arithmetic
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <atomic>
#include <climits>
#include <fstream>
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
	const auto length = static_cast<int>(bytes.size());

	// The size first, from the header: nothing is decoded for an image beyond the limits
	int width = 0, height = 0, channels = 0;
	if(!stbi_info_from_memory(bytes.data(), length, &width, &height, &channels))
		throw std::runtime_error(std::string("AffrescoUI: not a PNG or JPEG image (") + stbi_failure_reason() + ")");
	if(const char* problem = sizeProblem(width, height, limits))
		throw std::runtime_error(std::string("AffrescoUI: image refused: ") + problem);

	int decodedWidth = 0, decodedHeight = 0;
	const std::unique_ptr<stbi_uc, StbDeleter> decoded(
	    stbi_load_from_memory(bytes.data(), length, &decodedWidth, &decodedHeight, &channels, 4));
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
