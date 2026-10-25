/// @file
/// @brief Fuzzes inline images (render's Bitmap::fromDataUri): an image node's "source" can be any `data:` URI a
/// document, a channel's message or a peer's evidence sends.
///
/// The input is the URI's text, decoded within small limits so that inputs reach them: 64 KiB of data, 256 on a side,
/// 16,384 pixels. Each input is tried as given and behind both prefixes, so the fuzzer reaches the base64 decoder.

#include "Fuzz.hpp"

#include <affresco/render/Bitmap.hpp>

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

/// @brief Decodes one URI, and checks that what decodes is within the limits.
/// @param uri The URI.
void decode(std::string_view uri) {
	constexpr affresco::Bitmap::Limits limits{.fileBytes = std::size_t{64} << 10, .side = 256, .pixels = 16'384};
	try {
		const auto bitmap = affresco::Bitmap::fromDataUri(uri, limits);
		const auto width = static_cast<std::size_t>(bitmap->width()),
		           height = static_cast<std::size_t>(bitmap->height());
		if(width < 1 || height < 1 || width > 256 || height > 256 || width * height > 16'384)
			affresco::fuzz::broken("an inline image is within the limits");
		if(bitmap->pixels().size() != width * height * 4) affresco::fuzz::broken("four bytes a pixel");
	} catch(const std::runtime_error&) {
		// not such a URI, not base64, not the type it declares, or beyond the limits: refused, as it should be
	}
}

} // namespace

extern "C" int LLVMFuzzerInitialize(int* /*argc*/, char*** /*argv*/) {
	affresco::fuzz::silenceLog();
	return 0;
}

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	const std::string text(reinterpret_cast<const char*>(data), size);
	decode(text);
	decode("data:image/png;base64," + text);
	decode("data:image/jpeg;base64," + text);
	return 0;
}
