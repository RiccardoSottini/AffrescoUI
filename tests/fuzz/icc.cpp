/// @file
/// @brief Fuzzes the ICC profiles the image editor reads (affresco/imaging/Color.hpp): embedded in any PNG, JPEG or
/// project opened, so a damaged or hostile one must be refused, never crash, hang or read past its bytes.
///
/// The input is a profile's bytes. Parsing gives a usable profile, which then converts a few pixels and a small table
/// to sRGB (proofed against it when it's CMYK), or a DocumentError.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Color.hpp"
#include "Fuzz.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

/// @brief libFuzzer's entry point.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	using namespace affresco::imaging;
	try {
		const ColorProfile profile = ColorProfile::parse(std::span(data, size), Limits{});
		if(profile.name().empty() || profile.name().size() > 255) affresco::fuzz::broken("a profile has a short name");
		const ColorProfile srgb = ColorProfile::srgb();
		const ColorTransform transform = profile.model() == ColorModel::cmyk
		                                     ? ColorTransform(srgb, srgb, RenderingIntent::perceptual, profile)
		                                     : ColorTransform(profile, srgb, RenderingIntent::perceptual);
		const Pixel pixel = transform.apply(Pixel{0.25f, 0.5f, 0.75f, 0.5f});
		if(pixel.alpha != 0.5f) affresco::fuzz::broken("a conversion leaves alpha alone");
		if(transform.table(3).size() != 3 * 3 * 3 * 3) affresco::fuzz::broken("a table has side^3 colours");
	} catch(const affresco::DocumentError&) {
	}
	return 0;
}
