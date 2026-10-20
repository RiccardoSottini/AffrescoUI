/// @file
/// @brief Fuzzes image decoding (render's Bitmap, with stb_image): a revision can name any file in an allowed folder,
/// and a file there can hold anything.
///
/// The input is a file's bytes, decoded within small limits so that inputs reach them: 512 KiB, 512 on a side,
/// 65,536 pixels.

#include "Fuzz.hpp"

#include <affresco/render/Bitmap.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	constexpr affresco::Bitmap::Limits limits{.fileBytes = std::size_t{512} << 10, .side = 512, .pixels = 65'536};
	try {
		const auto bitmap = affresco::Bitmap::decode(std::span(data, size), limits);
		const auto width = static_cast<std::size_t>(bitmap->width()),
		           height = static_cast<std::size_t>(bitmap->height());
		if(width < 1 || height < 1 || width > 512 || height > 512 || width * height > 65'536)
			affresco::fuzz::broken("an image is within the limits");
		if(bitmap->pixels().size() != width * height * 4) affresco::fuzz::broken("four bytes a pixel");
		const auto pixels = bitmap->pixels();
		for(std::size_t i = 0; i < pixels.size(); i += 4)
			if(pixels[i] > pixels[i + 3] || pixels[i + 1] > pixels[i + 3] || pixels[i + 2] > pixels[i + 3])
				affresco::fuzz::broken("premultiplied: no channel above the alpha");
	} catch(const std::runtime_error&) {
		return 0; // not an image, or beyond the limits: refused, as it should be
	}
	return 0;
}
