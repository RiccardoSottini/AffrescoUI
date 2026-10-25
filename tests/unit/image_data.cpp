/// @file
/// @brief Unit tests for inline images and outlined regions (Bitmap::fromDataUri, ImageView::highlighted): base64 and
/// the URI's form checked before decoding, and a region kept near the image whatever a peer sends.

#include <affresco/render/Bitmap.hpp>
#include <affresco/render/ImageView.hpp>

#include <doctest/doctest.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>

using affresco::Bitmap;
using affresco::ImageFit;
using affresco::ImageView;
using affresco::Rect;

namespace {

/// @brief A file's bytes in base64.
/// @param file The file.
/// @return Its base64, padded.
std::string base64(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	const std::string bytes{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
	static constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string out;
	for(std::size_t i = 0; i < bytes.size(); i += 3) {
		unsigned bits = static_cast<unsigned>(static_cast<unsigned char>(bytes[i])) << 16U;
		if(i + 1 < bytes.size()) bits |= static_cast<unsigned>(static_cast<unsigned char>(bytes[i + 1])) << 8U;
		if(i + 2 < bytes.size()) bits |= static_cast<unsigned char>(bytes[i + 2]);
		out += alphabet[(bits >> 18) & 63];
		out += alphabet[(bits >> 12) & 63];
		out += i + 1 < bytes.size() ? alphabet[(bits >> 6) & 63] : '=';
		out += i + 2 < bytes.size() ? alphabet[bits & 63] : '=';
	}
	return out;
}

/// @brief The rendering check's badge, a small PNG.
std::filesystem::path badge() {
	return std::filesystem::path(AFFRESCOUI_TEST_FONTS_DIR).parent_path() / "screens/images/badge.png";
}

} // namespace

TEST_SUITE("image data") {
	TEST_CASE("a PNG given inline decodes") {
		const auto bitmap = Bitmap::fromDataUri("data:image/png;base64," + base64(badge()), Bitmap::Limits{});
		REQUIRE(bitmap);
		CHECK(bitmap->width() > 0);
		CHECK(bitmap->height() > 0);
	}

	TEST_CASE("anything but base64 of a PNG or JPEG is refused") {
		const Bitmap::Limits limits;
		const std::string png = base64(badge());
		CHECK_THROWS_AS((void)Bitmap::fromDataUri("data:image/png;base64,***", limits), std::runtime_error);
		CHECK_THROWS_AS((void)Bitmap::fromDataUri("data:image/png;base64,QUJD===", limits), std::runtime_error);
		CHECK_THROWS_AS((void)Bitmap::fromDataUri("data:image/svg+xml;base64," + png, limits), std::runtime_error);
		CHECK_THROWS_AS((void)Bitmap::fromDataUri("data:text/plain," + png, limits), std::runtime_error);
		CHECK_THROWS_AS((void)Bitmap::fromDataUri(png, limits), std::runtime_error);
		// a JPEG's prefix over a PNG's bytes, and bytes of no image
		CHECK_THROWS_AS((void)Bitmap::fromDataUri("data:image/jpeg;base64," + png, limits), std::runtime_error);
		CHECK_THROWS_AS((void)Bitmap::fromDataUri("data:image/png;base64,QUJDRA==", limits), std::runtime_error);
		Bitmap::Limits small;
		small.fileBytes = 16;
		CHECK_THROWS_AS((void)Bitmap::fromDataUri("data:image/png;base64," + png, small), std::runtime_error);
	}

	TEST_CASE("a region is mapped through the fit") {
		// a 100 by 50 image contained in 200 by 200: drawn 200 by 100, 50 from the top
		const Rect region = ImageView::highlighted(ImageFit::contain, {0, 0, 200, 200}, 100, 50, {10, 10, 20, 5});
		CHECK(region == Rect{20, 70, 60, 80});
		const Rect filled = ImageView::highlighted(ImageFit::fill, {0, 0, 200, 200}, 100, 50, {0, 0, 100, 50});
		CHECK(filled == Rect{0, 0, 200, 200});
	}

	TEST_CASE("an empty region, or none, is nothing") {
		const Rect frame{0, 0, 100, 100};
		CHECK(ImageView::highlighted(ImageFit::fill, frame, 10, 10, {1, 1, 0, 5}).width() <= 0);
		CHECK(ImageView::highlighted(ImageFit::fill, frame, 10, 10, {1, 1, -4, 5}).width() <= 0);
		CHECK(ImageView::highlighted(ImageFit::fill, frame, 0, 10, {1, 1, 4, 5}).width() <= 0);
		const double nan = std::numeric_limits<double>::quiet_NaN();
		CHECK(ImageView::highlighted(ImageFit::fill, frame, 10, 10, {nan, 1, 4, 5}).width() <= 0);
	}

	TEST_CASE("a region at the ends of the numbers is kept near the image") {
		const Rect frame{0, 0, 100, 100};
		const double huge = std::numeric_limits<double>::max(), low = std::numeric_limits<double>::lowest();
		for(const std::array<double, 4>& region :
		    {std::array<double, 4>{low, low, huge, huge}, std::array<double, 4>{0, 0, huge, huge},
		     std::array<double, 4>{1e30, 1e30, 1e30, 1e30}, std::array<double, 4>{-1e30, -1e30, 1e30, 1e30}}) {
			const Rect mapped = ImageView::highlighted(ImageFit::fill, frame, 10, 10, region);
			CHECK(std::isfinite(mapped.left));
			CHECK(std::isfinite(mapped.right));
			CHECK(mapped.left >= -100.0f);
			CHECK(mapped.right <= 200.0f);
		}
	}
}
