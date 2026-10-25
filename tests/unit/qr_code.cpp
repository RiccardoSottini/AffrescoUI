/// @file
/// @brief Tests of the QR code encoder and the qrCode expression function: the function patterns, the format and
/// version bits, the capacities, a whole code against a reference matrix, the path data and the errors.

#include "Expression.hpp"
#include "QrCode.hpp"

#include <affresco/affresco.hpp>

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

using affresco::json;
using affresco::format::detail::encodeQr;
using affresco::format::detail::QrCode;
using affresco::format::detail::qrPathData;
using affresco::format::detail::QrLevel;

namespace {

/// @brief The 15 format bits the standard gives a level and a mask: BCH(15, 5), then the fixed mask 0x5412.
/// @param level The level.
/// @param mask The mask pattern.
/// @return The bits, bit 0 first in the reading order.
int formatBits(QrLevel level, int mask) {
	static constexpr int levelBits[] = {1, 0, 3, 2}; // L, M, Q, H
	const int data = levelBits[static_cast<int>(level)] << 3 | mask;
	int remainder = data;
	for(int i = 0; i < 10; ++i) remainder = (remainder << 1) ^ ((remainder >> 9) * 0x537);
	return ((data << 10) | remainder) ^ 0x5412;
}

/// @brief Reads the format bits of a code from its copy around the top-left finder.
/// @param code The code.
/// @return The bits.
int readFormatBits(const QrCode& code) {
	int bits = 0;
	for(int i = 0; i < 6; ++i) bits |= int{code.at(8, i)} << i;
	bits |= int{code.at(8, 7)} << 6 | int{code.at(8, 8)} << 7 | int{code.at(7, 8)} << 8;
	for(int i = 9; i < 15; ++i) bits |= int{code.at(14 - i, 8)} << i;
	return bits;
}

/// @brief Reads the format bits from their second copy, under the top-right finder and right of the bottom-left one.
/// @param code The code.
/// @return The bits.
int readSecondFormatBits(const QrCode& code) {
	int bits = 0;
	for(int i = 0; i < 8; ++i) bits |= int{code.at(code.size - 1 - i, 8)} << i;
	for(int i = 8; i < 15; ++i) bits |= int{code.at(8, code.size - 15 + i)} << i;
	return bits;
}

/// @brief Reads the 18 version bits from the block left of the top-right finder.
/// @param code The code, version 7 or above.
/// @return The bits.
int readVersionBits(const QrCode& code) {
	int bits = 0;
	for(int i = 0; i < 18; ++i) bits |= int{code.at(code.size - 11 + i % 3, i / 3)} << i;
	return bits;
}

/// @brief Tells whether a finder pattern (7 by 7, its separator light) has its top-left corner at a place.
/// @param code The code.
/// @param left The finder's first column.
/// @param top The finder's first row.
/// @return true when every module is as the standard draws it.
bool hasFinder(const QrCode& code, int left, int top) {
	for(int y = -1; y <= 7; ++y)
		for(int x = -1; x <= 7; ++x) {
			const int column = left + x, row = top + y;
			if(column < 0 || row < 0 || column >= code.size || row >= code.size) continue;
			const int ring = std::max(std::abs(x - 3), std::abs(y - 3));
			if(code.at(column, row) != (ring != 2 && ring != 4)) return false;
		}
	return true;
}

/// @brief Encodes, failing the test when the bytes don't fit.
/// @param bytes The bytes.
/// @param level The level.
/// @param mask The mask, or -1.
/// @return The code.
QrCode encode(std::string_view bytes, QrLevel level, int mask = -1) {
	const std::optional<QrCode> code = encodeQr(bytes, level, mask);
	REQUIRE(code.has_value());
	return *code;
}

/// @brief Evaluates an expression with no state, as a screen would.
/// @param expression The expression, without braces.
/// @return Its value.
json evaluate(const std::string& expression) {
	static const json state = json::object(), window = {{"width", 800}, {"height", 600}}, revision = 1;
	static const json event = json::object(), globals = json::object();
	static const affresco::format::Scope locals{};
	const affresco::format::Context context{state, window, revision, event, locals, globals};
	return affresco::format::evaluate("{{" + expression + "}}", context, "test");
}

/// The rows of "Affresca" at level M with mask 0, checked module for module against an independent encoder.
constexpr std::string_view referenceRows[] = {
    "111111100110001111111", "100000101011001000001", "101110100110001011101", "101110100101001011101",
    "101110101001101011101", "100000100111001000001", "111111101010101111111", "000000000110000000000",
    "101010100000100010010", "101001011111010110011", "010100110001011110011", "010101010001110010000",
    "011110110011001010001", "000000001000001110010", "111111100100100110011", "100000100010001010010",
    "101110101000101011000", "101110100011010011010", "101110101011011110001", "100000100111110000010",
    "111111101101011110011",
};

} // namespace

TEST_CASE("qr code: a whole version 1 code matches the reference matrix") {
	const QrCode code = encode("Affresca", QrLevel::Medium, 0);
	REQUIRE(code.version == 1);
	REQUIRE(code.size == 21);
	CHECK(code.mask == 0);
	for(int y = 0; y < code.size; ++y) {
		std::string row;
		for(int x = 0; x < code.size; ++x) row.push_back(code.at(x, y) ? '1' : '0');
		CHECK_MESSAGE(row == referenceRows[y], "row ", y);
	}
}

TEST_CASE("qr code: the finders, the timing patterns and the dark module are where the standard puts them") {
	for(const int length : {0, 20, 200, 1000}) {
		const QrCode code = encode(std::string(static_cast<std::size_t>(length), 'q'), QrLevel::Low);
		CAPTURE(code.version);
		CHECK(code.size == 17 + 4 * code.version);
		CHECK(code.dark.size() == static_cast<std::size_t>(code.size * code.size));
		CHECK(hasFinder(code, 0, 0));
		CHECK(hasFinder(code, code.size - 7, 0));
		CHECK(hasFinder(code, 0, code.size - 7));
		for(int i = 8; i < code.size - 8; ++i) {
			CHECK(code.at(i, 6) == (i % 2 == 0));
			CHECK(code.at(6, i) == (i % 2 == 0));
		}
		CHECK(code.at(8, code.size - 8));
	}
}

TEST_CASE("qr code: both copies of the format bits name the level and the mask") {
	for(const QrLevel level : {QrLevel::Low, QrLevel::Medium, QrLevel::Quartile, QrLevel::High})
		for(const int mask : {0, 1, 7}) {
			const QrCode code = encode("format", level, mask);
			CAPTURE(static_cast<int>(level));
			CAPTURE(mask);
			CHECK(code.mask == mask);
			CHECK(readFormatBits(code) == formatBits(level, mask));
			CHECK(readSecondFormatBits(code) == formatBits(level, mask));
		}
	CHECK(formatBits(QrLevel::Medium, 0) == 0x5412);
}

TEST_CASE("qr code: the version bits from version 7, in both corners") {
	struct Case {
		std::size_t bytes;
		int version;
		int bits;
	};
	// At level L, version 7 holds 154 bytes, 8 holds 192 and 40 holds 2953 (ISO 18004, table 7).
	for(const Case c : {Case{154, 7, 0x07C94}, Case{155, 8, 0x085BC}, Case{2953, 40, 0x28C69}}) {
		const QrCode code = encode(std::string(c.bytes, 'v'), QrLevel::Low);
		REQUIRE(code.version == c.version);
		CHECK(readVersionBits(code) == c.bits);
		int mirrored = 0;
		for(int i = 0; i < 18; ++i) mirrored |= int{code.at(i / 3, code.size - 11 + i % 3)} << i;
		CHECK(mirrored == c.bits);
	}
}

TEST_CASE("qr code: the smallest version that holds the bytes, and nothing past version 40") {
	CHECK(encode(std::string(14, 'm'), QrLevel::Medium).version == 1);
	CHECK(encode(std::string(15, 'm'), QrLevel::Medium).version == 2);
	CHECK(encode(std::string(7, 'h'), QrLevel::High).version == 1);
	CHECK(encode(std::string(8, 'h'), QrLevel::High).version == 2);
	CHECK(encode(std::string(2953, 'l'), QrLevel::Low).version == 40);
	CHECK_FALSE(encodeQr(std::string(2954, 'l'), QrLevel::Low).has_value());
	CHECK(encode(std::string(1273, 'h'), QrLevel::High).version == 40);
	CHECK_FALSE(encodeQr(std::string(1274, 'h'), QrLevel::High).has_value());
}

TEST_CASE("qr code: an alignment pattern has a dark centre, a light ring and a dark ring") {
	const QrCode code = encode(std::string(20, 'a'), QrLevel::Medium); // version 2: one pattern, centred at 18, 18
	REQUIRE(code.version == 2);
	for(int y = -2; y <= 2; ++y)
		for(int x = -2; x <= 2; ++x) CHECK(code.at(18 + x, 18 + y) == (std::max(std::abs(x), std::abs(y)) != 1));
}

TEST_CASE("qr code: the automatic mask is one of the eight and the same every time") {
	const QrCode first = encode("https://id.example/enroll?secret=EXAMPLE", QrLevel::Medium);
	const QrCode second = encode("https://id.example/enroll?secret=EXAMPLE", QrLevel::Medium);
	CHECK(first.mask >= 0);
	CHECK(first.mask <= 7);
	CHECK(first.dark == second.dark);
	CHECK(readFormatBits(first) == formatBits(QrLevel::Medium, first.mask));
}

TEST_CASE("qr code: the path data draws every dark module once, merging the runs that repeat") {
	const QrCode code = encode("Affresca", QrLevel::Medium, 0);
	std::size_t commands = 0;
	const std::string d = qrPathData(code, commands);
	CHECK(d.starts_with("M0 0h7v1h-7z"));
	CHECK(d.find("M2 2h3v3h-3z") != std::string::npos); // the finder's centre: three rows of three, one rectangle
	const auto rectangles = static_cast<std::size_t>(std::ranges::count(d, 'M'));
	CHECK(commands == 5 * rectangles);
	CHECK(commands == 400);
	std::size_t dark = 0;
	for(const std::uint8_t module : code.dark) dark += module;
	CHECK(rectangles < dark);
}

TEST_CASE("qr code: the qrCode expression function") {
	const json code = evaluate("qrCode('Affresca')");
	CHECK(code["size"] == 21);
	CHECK(code["version"] == 1);
	CHECK(code["viewBox"] == "-4 -4 29 29");
	CHECK(code["d"].get<std::string>().starts_with("M0 0h7v1h-7z"));
	CHECK(evaluate("qrCode('Affresca', 'H').version") == 2);
	CHECK(evaluate("qrCode('').size") == 21);
	CHECK_THROWS_WITH_AS((void)evaluate("qrCode('x', 'Z')"), doctest::Contains("the level"), affresco::DocumentError);
	CHECK_THROWS_WITH_AS((void)evaluate("qrCode('x', 1)"), doctest::Contains("the level"), affresco::DocumentError);
	CHECK_THROWS_WITH_AS((void)evaluate("qrCode('" + std::string(1300, 'x') + "', 'H')"),
	                     doctest::Contains("too long"), affresco::DocumentError);
	CHECK_THROWS_WITH_AS((void)evaluate("qrCode()"), doctest::Contains("qrCode"), affresco::DocumentError);
}

TEST_CASE("qr code: a code too large to draw fails clearly instead of drawing nothing") {
	std::string text;
	for(int i = 0; i < 2900; ++i) text.push_back(static_cast<char>('a' + (i * 7919) % 26));
	CHECK_THROWS_WITH_AS((void)evaluate("qrCode('" + text + "', 'L')"), doctest::Contains("too large to draw"),
	                     affresco::DocumentError);
}
