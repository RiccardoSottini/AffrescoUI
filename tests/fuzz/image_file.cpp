/// @file
/// @brief Fuzzes the PNG and JPEG files the image editor and the "image" step open (affresco/imaging/Codec.hpp): a
/// revision can bring any file as an asset, and a person can open any file they were sent.
///
/// The input is a file's bytes, decoded within small limits so that inputs reach them: 512 KiB, 512 on a side,
/// 65,536 pixels. Decoding gives an image within them, or a DocumentError.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Codec.hpp"
#include "Fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	using namespace affresco::imaging;
	Limits limits;
	limits.imageFileBytes = std::size_t{512} << 10;
	limits.imageFileSide = 512;
	limits.imageFilePixels = 65'536;
	try {
		const DecodedImage decoded = decodeImageFile(std::span(data, size), limits);
		const auto width = static_cast<std::size_t>(decoded.image->width()),
		           height = static_cast<std::size_t>(decoded.image->height());
		if(width < 1 || height < 1 || width > 512 || height > 512 || width * height > 65'536)
			affresco::fuzz::broken("an image is within the limits");
	} catch(const affresco::DocumentError&) { // NOLINT(bugprone-empty-catch): refusing is the expected outcome
	}
	return 0;
}
