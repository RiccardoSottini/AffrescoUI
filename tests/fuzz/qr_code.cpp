/// @file
/// @brief Fuzzes encodeQr and qrPathData (format/src/functions/QrCode.hpp): the text of a qrCode is untrusted input.
///
/// The first byte picks the level and the mask, the rest is the text. A code is refused only past version 40's
/// capacity; otherwise it has the right size, its finders, format bits that name its level and mask, and path data
/// whose rectangles cover exactly its dark modules, each once, inside the square.

#include "QrCode.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using affresco::format::detail::encodeQr;
using affresco::format::detail::QrCode;
using affresco::format::detail::QrLevel;

namespace {

/// @brief Stops the run when an invariant doesn't hold.
/// @param condition The invariant.
void require(bool condition) {
	if(!condition) std::abort();
}

/// @brief Reads an integer of the path data and the text after it.
/// @param data The data, advanced past the integer.
/// @return The integer.
int readNumber(std::string_view& data) {
	int value = 0;
	const auto [end, error] = std::from_chars(data.data(), data.data() + data.size(), value);
	require(error == std::errc{});
	data.remove_prefix(static_cast<std::size_t>(end - data.data()));
	return value;
}

/// @brief Reads one expected character of the path data.
/// @param data The data, advanced past it.
/// @param expected The character.
void readCharacter(std::string_view& data, char expected) {
	require(!data.empty() && data.front() == expected);
	data.remove_prefix(1);
}

/// @brief Tells whether the format bits around the top-left finder name a level and a mask.
/// @param code The code.
/// @param level The level.
/// @param mask The mask.
/// @return true when they do.
bool formatMatches(const QrCode& code, QrLevel level, int mask) {
	static constexpr int levelBits[] = {1, 0, 3, 2};
	const int data = levelBits[static_cast<int>(level)] << 3 | mask;
	int remainder = data;
	for(int i = 0; i < 10; ++i) remainder = (remainder << 1) ^ ((remainder >> 9) * 0x537);
	const int expected = ((data << 10) | remainder) ^ 0x5412;
	int bits = 0;
	for(int i = 0; i < 6; ++i) bits |= int{code.at(8, i)} << i;
	bits |= int{code.at(8, 7)} << 6 | int{code.at(8, 8)} << 7 | int{code.at(7, 8)} << 8;
	for(int i = 9; i < 15; ++i) bits |= int{code.at(14 - i, 8)} << i;
	return bits == expected;
}

/// @brief Tells whether a finder sits with its top-left corner at a place.
/// @param code The code.
/// @param left Its first column.
/// @param top Its first row.
/// @return true when every module of it is right.
bool hasFinder(const QrCode& code, int left, int top) {
	for(int y = 0; y < 7; ++y)
		for(int x = 0; x < 7; ++x) {
			const int ring = std::max(std::abs(x - 3), std::abs(y - 3));
			if(code.at(left + x, top + y) != (ring != 2)) return false;
		}
	return true;
}

/// @brief Checks a code's path data: rectangles, each "Mx yhWvHh-Wz", that cover every dark module once and nothing
/// else, and as many commands as it says.
/// @param code The code.
void checkPath(const QrCode& code) {
	std::size_t commands = 0;
	const std::string data = affresco::format::detail::qrPathData(code, commands);
	std::string_view path = data;
	std::vector<std::uint8_t> covered(code.dark.size(), 0);
	std::size_t counted = 0;
	while(!path.empty()) {
		readCharacter(path, 'M');
		const int x = readNumber(path);
		readCharacter(path, ' ');
		const int y = readNumber(path);
		readCharacter(path, 'h');
		const int width = readNumber(path);
		readCharacter(path, 'v');
		const int height = readNumber(path);
		readCharacter(path, 'h');
		require(readNumber(path) == -width);
		readCharacter(path, 'z');
		counted += 5;
		require(x >= 0 && y >= 0 && width > 0 && height > 0 && x + width <= code.size && y + height <= code.size);
		for(int row = y; row < y + height; ++row)
			for(int column = x; column < x + width; ++column) {
				std::uint8_t& module = covered[static_cast<std::size_t>(row * code.size + column)];
				require(module == 0);
				module = 1;
			}
	}
	require(counted == commands);
	require(covered == code.dark);
}

} // namespace

/// @brief libFuzzer's entry point: encodes the input and checks the code and its path data.
/// @param data The input.
/// @param size Its size.
/// @return 0.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if(size == 0 || size > 4096) return 0;
	const auto level = static_cast<QrLevel>(data[0] & 3);
	const int mask = (data[0] >> 2 & 15) < 8 ? data[0] >> 2 & 7 : -1;
	const std::string_view text(reinterpret_cast<const char*>(data + 1), size - 1);
	static constexpr std::array<std::size_t, 4> capacity{2953, 2331, 1663, 1273};
	const std::optional<QrCode> code = encodeQr(text, level, mask);
	require(code.has_value() == (text.size() <= capacity.at(static_cast<std::size_t>(level))));
	if(!code) return 0;
	require(code->version >= 1 && code->version <= 40);
	require(code->size == 17 + 4 * code->version);
	require(code->dark.size() == static_cast<std::size_t>(code->size) * static_cast<std::size_t>(code->size));
	require(code->mask >= 0 && code->mask <= 7 && (mask < 0 || code->mask == mask));
	require(hasFinder(*code, 0, 0) && hasFinder(*code, code->size - 7, 0) && hasFinder(*code, 0, code->size - 7));
	require(formatMatches(*code, level, code->mask));
	checkPath(*code);
	return 0;
}
