/// @file
/// @brief Fuzzes the PNG reader golden images go through (modules/automation/src/PngReader.hpp): any file in a goldens
/// folder is read, so a damaged or hostile one must be refused, never crash or read past its bytes.
///
/// The input is a file's bytes. Decoding gives a picture of width * height * 3 bytes within the pixel limit, or an
/// error message.

#include "../../modules/automation/src/PngReader.hpp" // by its path: clang-tidy reads this file without its include folders
#include "Fuzz.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	namespace detail = affresco::automation::detail;
	std::string error;
	const auto pixels = detail::decodePng(std::span(data, size), error);
	if(!pixels) {
		if(error.empty()) affresco::fuzz::broken("a refused file says why");
		return 0;
	}
	const auto count = static_cast<std::size_t>(pixels->width) * static_cast<std::size_t>(pixels->height);
	if(pixels->width <= 0 || pixels->height <= 0 || count > detail::maxPicturePixels || pixels->rgb.size() != count * 3)
		affresco::fuzz::broken("a decoded picture has width * height * 3 bytes, within the limit");
	return 0;
}
