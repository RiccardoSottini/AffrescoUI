/// @file
/// @brief The QR code encoder: the data codewords, their Reed-Solomon error correction, the function patterns, the
/// zigzag placement, the mask and the format and version information (ISO/IEC 18004, model 2).

#include "QrCode.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <format>
#include <iterator>
#include <limits>
#include <span>
#include <utility>

namespace affresco::format::detail {
namespace {

/// Error correction codewords per block, by level (L, M, Q, H) and version (1 to 40).
constexpr std::array<std::array<std::uint8_t, 40>, 4> ecPerBlock{{
    {7,  10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28,
     28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26,
     26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30,
     28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28,
     30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
}};

/// Error correction blocks, by level (L, M, Q, H) and version (1 to 40).
constexpr std::array<std::array<std::uint8_t, 40>, 4> blockCounts{{
    {1,  1,  1,  1,  1,  2,  2,  2,  2,  4,  4,  4,  4,  4,  6,  6,  6,  6,  7,  8,
     8,  9,  9,  10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {1,  1,  1,  2,  2,  4,  4,  4,  5,  5,  5,  8,  9,  9,  10, 10, 11, 13, 14, 16,
     17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {1,  1,  2,  2,  4,  4,  6,  6,  8,  8,  8,  10, 12, 16, 12, 17, 16, 18, 21, 20,
     23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {1,  1,  2,  4,  4,  4,  5,  6,  8,  8,  11, 11, 16, 16, 18, 16, 19, 21, 25, 25,
     25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
}};

/// The penalty of a run of five modules of one colour, and of each module more (N1).
constexpr int penaltyRun = 3;
/// The penalty of a 2 by 2 block of one colour (N2).
constexpr int penaltyBlock = 3;
/// The penalty of a finder-like pattern (N3).
constexpr int penaltyFinder = 40;
/// The penalty of each 5% the dark modules are away from half (N4).
constexpr int penaltyBalance = 10;

/// @brief The row of a level in the tables.
/// @param level The level.
/// @return 0 to 3.
[[nodiscard]] std::size_t row(QrLevel level) noexcept { return static_cast<std::size_t>(level); }

/// @brief The two bits the format information gives a level: L 01, M 00, Q 11, H 10.
/// @param level The level.
/// @return The bits.
[[nodiscard]] int formatLevelBits(QrLevel level) {
	static constexpr std::array<int, 4> bits{1, 0, 3, 2};
	return bits.at(row(level));
}

/// @brief The modules of a version left for data and error correction, once the function patterns are drawn.
/// @param version 1 to 40.
/// @return Their number (a multiple of 8, or 8 more than one for some versions: the remainder bits).
[[nodiscard]] int rawModules(int version) noexcept {
	int modules = (16 * version + 128) * version + 64;
	if(version >= 2) {
		const int alignments = version / 7 + 2;
		modules -= (25 * alignments - 10) * alignments - 55;
		if(version >= 7) modules -= 36;
	}
	return modules;
}

/// @brief The data codewords a version holds at a level.
/// @param version 1 to 40.
/// @param level The level.
/// @return Their number.
[[nodiscard]] int dataCodewords(int version, QrLevel level) {
	const auto v = static_cast<std::size_t>(version - 1);
	return rawModules(version) / 8 - ecPerBlock.at(row(level)).at(v) * blockCounts.at(row(level)).at(v);
}

/// @brief Multiplies two elements of GF(256), modulo x^8 + x^4 + x^3 + x^2 + 1.
/// @param x One.
/// @param y The other.
/// @return The product.
[[nodiscard]] std::uint8_t multiply(std::uint8_t x, std::uint8_t y) noexcept {
	unsigned z = 0;
	for(int i = 7; i >= 0; --i) {
		z = (z << 1U) ^ ((z >> 7U) * 0x11DU);
		z ^= ((static_cast<unsigned>(y) >> static_cast<unsigned>(i)) & 1U) * x;
	}
	return static_cast<std::uint8_t>(z);
}

/// @brief The Reed-Solomon generator polynomial of a degree, without its leading 1, highest power first.
/// @param degree 1 to 30.
/// @return Its coefficients.
[[nodiscard]] std::vector<std::uint8_t> generator(int degree) {
	std::vector<std::uint8_t> result(static_cast<std::size_t>(degree), 0);
	result.back() = 1;
	std::uint8_t root = 1;
	for(int i = 0; i < degree; ++i) {
		for(std::size_t j = 0; j < result.size(); ++j) {
			result[j] = multiply(result[j], root);
			if(j + 1 < result.size()) result[j] ^= result[j + 1];
		}
		root = multiply(root, 2);
	}
	return result;
}

/// @brief The error correction codewords of a block: the remainder of its data divided by the generator.
/// @param data The block's data codewords.
/// @param divisor The generator, from generator().
/// @return As many codewords as the divisor has.
[[nodiscard]] std::vector<std::uint8_t> remainder(std::span<const std::uint8_t> data,
                                                  const std::vector<std::uint8_t>& divisor) {
	std::vector<std::uint8_t> result(divisor.size(), 0);
	for(const std::uint8_t byte : data) {
		const auto factor = static_cast<std::uint8_t>(byte ^ result.front());
		std::ranges::rotate(result, result.begin() + 1);
		result.back() = 0;
		for(std::size_t i = 0; i < result.size(); ++i) result[i] ^= multiply(divisor[i], factor);
	}
	return result;
}

/// @brief Bits appended most significant first, then read as codewords.
class Bits {
public:
	/// @brief Appends the low bits of a value.
	/// @param value The value.
	/// @param count How many of its bits, most significant first.
	void append(unsigned value, int count) {
		for(int i = count - 1; i >= 0; --i)
			bits_.push_back(static_cast<std::uint8_t>((value >> static_cast<unsigned>(i)) & 1U));
	}
	/// @brief How many bits there are.
	[[nodiscard]] std::size_t size() const noexcept { return bits_.size(); }
	/// @brief The bits as codewords (their number a multiple of 8).
	[[nodiscard]] std::vector<std::uint8_t> codewords() const {
		std::vector<std::uint8_t> result(bits_.size() / 8, 0);
		for(std::size_t i = 0; i < bits_.size(); ++i)
			result[i / 8] = static_cast<std::uint8_t>(result[i / 8] | (bits_[i] << (7 - i % 8)));
		return result;
	}

private:
	std::vector<std::uint8_t> bits_; ///< One per bit.
};

/// @brief A code being drawn: its modules and which of them the function patterns took.
class Matrix {
public:
	/// @brief Starts a version's code, every module light.
	/// @param version 1 to 40.
	explicit Matrix(int version)
	    : size_(17 + 4 * version), dark_(cells(), 0), function_(cells(), 0) {}

	/// @brief The modules on a side.
	[[nodiscard]] int size() const noexcept { return size_; }
	/// @brief Tells whether a module is dark.
	/// @param x Its column.
	/// @param y Its row.
	[[nodiscard]] bool dark(int x, int y) const noexcept { return dark_[index(x, y)] != 0; }
	/// @brief Tells whether a function pattern took a module.
	/// @param x Its column.
	/// @param y Its row.
	[[nodiscard]] bool function(int x, int y) const noexcept { return function_[index(x, y)] != 0; }
	/// @brief Sets a module of a function pattern.
	/// @param x Its column.
	/// @param y Its row.
	/// @param dark Whether it is dark.
	void setFunction(int x, int y, bool dark) noexcept {
		dark_[index(x, y)] = dark ? 1 : 0;
		function_[index(x, y)] = 1;
	}
	/// @brief Sets a data module.
	/// @param x Its column.
	/// @param y Its row.
	/// @param dark Whether it is dark.
	void set(int x, int y, bool dark) noexcept { dark_[index(x, y)] = dark ? 1 : 0; }
	/// @brief Gives the modules away.
	[[nodiscard]] std::vector<std::uint8_t> release() && { return std::move(dark_); }

private:
	/// @brief The modules of the square.
	[[nodiscard]] std::size_t cells() const noexcept {
		return static_cast<std::size_t>(size_) * static_cast<std::size_t>(size_);
	}
	/// @brief A module's position in the vectors.
	/// @param x Its column.
	/// @param y Its row.
	[[nodiscard]] std::size_t index(int x, int y) const noexcept {
		return static_cast<std::size_t>(y) * static_cast<std::size_t>(size_) + static_cast<std::size_t>(x);
	}

	int size_;                          ///< Modules on a side.
	std::vector<std::uint8_t> dark_;     ///< 1 for dark.
	std::vector<std::uint8_t> function_; ///< 1 for a module of a function pattern.
};

/// @brief The centres of a version's alignment patterns, on either axis.
/// @param version 1 to 40.
/// @return Ascending; empty for version 1.
[[nodiscard]] std::vector<int> alignmentCentres(int version) {
	if(version == 1) return {};
	const int count = version / 7 + 2;
	const int step = (version * 8 + count * 3 + 5) / (count * 4 - 4) * 2;
	std::vector<int> result{6};
	for(int at = 17 + 4 * version - 7; std::cmp_less(result.size(), count); at -= step)
		result.insert(std::next(result.begin()), at);
	return result;
}

/// @brief Draws the format information, both copies, and the dark module beside the bottom left finder.
/// @param matrix The code.
/// @param level Its level.
/// @param mask Its mask.
void drawFormat(Matrix& matrix, QrLevel level, int mask) noexcept {
	const auto data = static_cast<unsigned>((formatLevelBits(level) << 3) | mask);
	unsigned rest = data;
	for(int i = 0; i < 10; ++i) rest = (rest << 1U) ^ ((rest >> 9U) * 0x537U);
	const unsigned bits = (data << 10U | rest) ^ 0x5412U;
	const auto bit = [bits](int i) { return ((bits >> static_cast<unsigned>(i)) & 1U) != 0; };
	const int size = matrix.size();
	for(int i = 0; i <= 5; ++i) matrix.setFunction(8, i, bit(i));
	matrix.setFunction(8, 7, bit(6));
	matrix.setFunction(8, 8, bit(7));
	matrix.setFunction(7, 8, bit(8));
	for(int i = 9; i < 15; ++i) matrix.setFunction(14 - i, 8, bit(i));
	for(int i = 0; i < 8; ++i) matrix.setFunction(size - 1 - i, 8, bit(i));
	for(int i = 8; i < 15; ++i) matrix.setFunction(8, size - 15 + i, bit(i));
	matrix.setFunction(8, size - 8, true);
}

/// @brief Draws the version information, both copies (versions 7 and up).
/// @param matrix The code.
/// @param version Its version.
void drawVersion(Matrix& matrix, int version) noexcept {
	if(version < 7) return;
	unsigned rest = static_cast<unsigned>(version);
	for(int i = 0; i < 12; ++i) rest = (rest << 1U) ^ ((rest >> 11U) * 0x1F25U);
	const unsigned bits = static_cast<unsigned>(version) << 12U | rest;
	for(int i = 0; i < 18; ++i) {
		const bool dark = ((bits >> static_cast<unsigned>(i)) & 1U) != 0;
		const int a = matrix.size() - 11 + i % 3;
		const int b = i / 3;
		matrix.setFunction(a, b, dark);
		matrix.setFunction(b, a, dark);
	}
}

/// @brief Draws a finder pattern, the 7x7 square in a corner and its light border, clipped to the code.
/// @param matrix The code.
/// @param cx The column of its centre.
/// @param cy The row of its centre.
void drawFinder(Matrix& matrix, int cx, int cy) noexcept {
	const int size = matrix.size();
	for(int dy = -4; dy <= 4; ++dy)
		for(int dx = -4; dx <= 4; ++dx) {
			const int x = cx + dx;
			const int y = cy + dy;
			if(x < 0 || y < 0 || x >= size || y >= size) continue;
			const int distance = std::max(std::abs(dx), std::abs(dy));
			matrix.setFunction(x, y, distance != 2 && distance != 4);
		}
}

/// @brief Draws the alignment patterns (versions 2 and up), all but the three that would overlap the finders.
/// @param matrix The code.
/// @param version Its version.
void drawAlignments(Matrix& matrix, int version) {
	const std::vector<int> centres = alignmentCentres(version);
	const std::size_t last = centres.empty() ? 0 : centres.size() - 1;
	for(std::size_t i = 0; i < centres.size(); ++i)
		for(std::size_t j = 0; j < centres.size(); ++j) {
			if((i == 0 && j == 0) || (i == 0 && j == last) || (i == last && j == 0)) continue;
			for(int dy = -2; dy <= 2; ++dy)
				for(int dx = -2; dx <= 2; ++dx)
					matrix.setFunction(centres[i] + dx, centres[j] + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
		}
}

/// @brief Draws the function patterns: the timing lines, the finders, the alignment patterns and the format and
/// version areas.
/// @param matrix The code.
/// @param version Its version.
/// @param level Its level.
void drawFunctionPatterns(Matrix& matrix, int version, QrLevel level) {
	const int size = matrix.size();
	for(int i = 0; i < size; ++i) {
		matrix.setFunction(6, i, i % 2 == 0);
		matrix.setFunction(i, 6, i % 2 == 0);
	}
	drawFinder(matrix, 3, 3);
	drawFinder(matrix, size - 4, 3);
	drawFinder(matrix, 3, size - 4);
	drawAlignments(matrix, version);
	drawFormat(matrix, level, 0);
	drawVersion(matrix, version);
}

/// @brief Places the codewords in the data modules, two columns at a time from the right, up then down.
/// @param matrix The code, its function patterns drawn.
/// @param codewords The interleaved data and error correction codewords.
void drawCodewords(Matrix& matrix, std::span<const std::uint8_t> codewords) noexcept {
	const int size = matrix.size();
	const std::size_t bits = codewords.size() * 8;
	std::size_t i = 0;
	for(int right = size - 1; right >= 1; right -= 2) {
		if(right == 6) right = 5;
		const bool upward = ((right + 1) & 2) == 0;
		for(int vertical = 0; vertical < size; ++vertical)
			for(int j = 0; j < 2; ++j) {
				const int x = right - j;
				const int y = upward ? size - 1 - vertical : vertical;
				if(matrix.function(x, y) || i >= bits) continue;
				matrix.set(x, y, ((codewords[i / 8] >> (7 - i % 8)) & 1U) != 0);
				++i;
			}
	}
}

/// @brief Flips the data modules a mask pattern selects; applying it twice undoes it.
/// @param matrix The code.
/// @param mask The pattern, 0 to 7.
void applyMask(Matrix& matrix, int mask) noexcept {
	const int size = matrix.size();
	for(int y = 0; y < size; ++y)
		for(int x = 0; x < size; ++x) {
			bool flip = false;
			switch(mask) {
			case 0: flip = (x + y) % 2 == 0; break;
			case 1: flip = y % 2 == 0; break;
			case 2: flip = x % 3 == 0; break;
			case 3: flip = (x + y) % 3 == 0; break;
			case 4: flip = (x / 3 + y / 2) % 2 == 0; break;
			case 5: flip = x * y % 2 + x * y % 3 == 0; break;
			case 6: flip = (x * y % 2 + x * y % 3) % 2 == 0; break;
			default: flip = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
			}
			if(flip && !matrix.function(x, y)) matrix.set(x, y, !matrix.dark(x, y));
		}
}

/// @brief The penalty of one row or column's long runs (N1): five modules of a colour or more.
/// @param line Its modules, 1 for dark.
/// @return The penalty.
[[nodiscard]] int runPenalty(std::span<const std::uint8_t> line) noexcept {
	int penalty = 0;
	for(std::size_t start = 0; start < line.size();) {
		std::size_t end = start;
		while(end < line.size() && line[end] == line[start]) ++end;
		if(end - start >= 5) penalty += penaltyRun + static_cast<int>(end - start - 5);
		start = end;
	}
	return penalty;
}

/// @brief Whether a module of a row or column is dark.
/// @param line Its modules, 1 for dark.
/// @param i The module's place; outside the line is light.
/// @return Whether it is dark.
[[nodiscard]] bool darkAt(std::span<const std::uint8_t> line, int i) noexcept {
	return i >= 0 && std::cmp_less(i, line.size()) && line[static_cast<std::size_t>(i)] != 0;
}

/// @brief Whether four modules of a row or column are light (the outside is).
/// @param line Its modules, 1 for dark.
/// @param from The first.
/// @return Whether all four are light.
[[nodiscard]] bool lightAt(std::span<const std::uint8_t> line, int from) noexcept {
	for(int i = from; i < from + 4; ++i)
		if(darkAt(line, i)) return false;
	return true;
}

/// @brief Whether a finder-like pattern starts at a module: dark 1:1:3:1:1.
/// @param line Its modules, 1 for dark.
/// @param p The module.
/// @return Whether the seven modules from it are the pattern.
[[nodiscard]] bool finderAt(std::span<const std::uint8_t> line, int p) noexcept {
	static constexpr std::array<bool, 7> pattern{true, false, true, true, true, false, true};
	for(int i = 0; i < 7; ++i)
		if(darkAt(line, p + i) != pattern.at(static_cast<std::size_t>(i))) return false;
	return true;
}

/// @brief The penalty of one row or column's finder-like patterns (N3): dark 1:1:3:1:1, with four light modules (or
/// the outside) before or after it; each side counts.
/// @param line Its modules, 1 for dark.
/// @return The penalty.
[[nodiscard]] int finderPenalty(std::span<const std::uint8_t> line) noexcept {
	int penalty = 0;
	for(int p = 0; std::cmp_less_equal(p + 7, line.size()); ++p) {
		if(!finderAt(line, p)) continue;
		if(lightAt(line, p - 4)) penalty += penaltyFinder;
		if(lightAt(line, p + 7)) penalty += penaltyFinder;
	}
	return penalty;
}

/// @brief The penalty of one row or column: its long runs (N1) and its finder-like patterns (N3).
/// @param line Its modules, 1 for dark.
/// @return The penalty.
[[nodiscard]] int linePenalty(std::span<const std::uint8_t> line) noexcept {
	return runPenalty(line) + finderPenalty(line);
}

/// @brief The penalty of a code, by the standard's four rules: the lower, the easier to read.
/// @param matrix The code, masked.
/// @return The penalty.
[[nodiscard]] int penalty(const Matrix& matrix) {
	const int size = matrix.size();
	int result = 0;
	std::vector<std::uint8_t> line(static_cast<std::size_t>(size), 0);
	for(int y = 0; y < size; ++y) {
		for(int x = 0; x < size; ++x) line[static_cast<std::size_t>(x)] = matrix.dark(x, y) ? 1 : 0;
		result += linePenalty(line);
	}
	for(int x = 0; x < size; ++x) {
		for(int y = 0; y < size; ++y) line[static_cast<std::size_t>(y)] = matrix.dark(x, y) ? 1 : 0;
		result += linePenalty(line);
	}
	long dark = 0;
	for(int y = 0; y < size; ++y)
		for(int x = 0; x < size; ++x) {
			if(matrix.dark(x, y)) ++dark;
			if(x + 1 < size && y + 1 < size) {
				const bool colour = matrix.dark(x, y);
				if(matrix.dark(x + 1, y) == colour && matrix.dark(x, y + 1) == colour && matrix.dark(x + 1, y + 1) == colour)
					result += penaltyBlock;
			}
		}
	const long total = static_cast<long>(size) * size;
	result += static_cast<int>(std::labs(dark * 20 - total * 10) / total) * penaltyBalance;
	return result;
}

/// @brief Splits the data codewords into blocks, adds each block's error correction and interleaves them.
/// @param data The data codewords.
/// @param version The version.
/// @param level The level.
/// @return Every codeword of the code, in placement order.
[[nodiscard]] std::vector<std::uint8_t> interleave(const std::vector<std::uint8_t>& data, int version, QrLevel level) {
	const auto v = static_cast<std::size_t>(version - 1);
	const int blocks = blockCounts.at(row(level)).at(v);
	const int ecLength = ecPerBlock.at(row(level)).at(v);
	const int total = rawModules(version) / 8;
	const int shortBlocks = blocks - total % blocks;
	const int shortLength = total / blocks;
	const std::vector<std::uint8_t> divisor = generator(ecLength);
	std::vector<std::vector<std::uint8_t>> parts;
	std::size_t taken = 0;
	for(int i = 0; i < blocks; ++i) {
		const int blockLength = shortLength - ecLength + (i < shortBlocks ? 0 : 1);
		const auto length = static_cast<std::size_t>(blockLength);
		std::vector<std::uint8_t> part(data.begin() + static_cast<std::ptrdiff_t>(taken),
		                               data.begin() + static_cast<std::ptrdiff_t>(taken + length));
		taken += length;
		const std::vector<std::uint8_t> ec = remainder(part, divisor);
		if(i < shortBlocks) part.push_back(0); // A place holder, so every block has the same length.
		part.insert(part.end(), ec.begin(), ec.end());
		parts.push_back(std::move(part));
	}
	std::vector<std::uint8_t> result;
	result.reserve(static_cast<std::size_t>(total));
	const auto holder = static_cast<std::size_t>(shortLength - ecLength);
	for(std::size_t i = 0; i < parts.front().size(); ++i)
		for(std::size_t j = 0; j < parts.size(); ++j)
			if(i != holder || std::cmp_greater_equal(j, shortBlocks)) result.push_back(parts[j][i]);
	return result;
}

} // namespace

std::optional<QrCode> encodeQr(std::string_view bytes, QrLevel level, int mask) {
	int version = 1;
	for(; version <= 40; ++version) {
		const std::size_t countBits = version <= 9 ? 8 : 16;
		const auto capacity = static_cast<std::size_t>(dataCodewords(version, level)) * 8;
		if(bytes.size() <= (std::numeric_limits<std::size_t>::max() - 4 - countBits) / 8 &&
		   4 + countBits + bytes.size() * 8 <= capacity)
			break;
	}
	if(version > 40) return std::nullopt;

	Bits bits;
	bits.append(0b0100, 4);
	bits.append(static_cast<unsigned>(bytes.size()), version <= 9 ? 8 : 16);
	for(const char c : bytes) bits.append(static_cast<unsigned char>(c), 8);
	const auto capacity = static_cast<std::size_t>(dataCodewords(version, level)) * 8;
	bits.append(0, static_cast<int>(std::min<std::size_t>(4, capacity - bits.size())));
	bits.append(0, static_cast<int>((8 - bits.size() % 8) % 8));
	for(unsigned pad = 0xEC; bits.size() < capacity; pad ^= 0xEC ^ 0x11) bits.append(pad, 8);

	Matrix matrix(version);
	drawFunctionPatterns(matrix, version, level);
	drawCodewords(matrix, interleave(bits.codewords(), version, level));
	if(mask < 0 || mask > 7) {
		int lowest = std::numeric_limits<int>::max();
		for(int candidate = 0; candidate < 8; ++candidate) {
			applyMask(matrix, candidate);
			drawFormat(matrix, level, candidate);
			const int score = penalty(matrix);
			if(score < lowest) {
				lowest = score;
				mask = candidate;
			}
			applyMask(matrix, candidate);
		}
	}
	applyMask(matrix, mask);
	drawFormat(matrix, level, mask);

	QrCode code;
	code.version = version;
	code.size = matrix.size();
	code.mask = mask;
	code.dark = std::move(matrix).release();
	return code;
}

std::string qrPathData(const QrCode& code, std::size_t& commands) {
	/// A rectangle still growing down: a run of a row, repeated on the rows since.
	struct Open {
		int from = 0; ///< Its first column.
		int to = 0;   ///< The column after its last.
		int top = 0;  ///< Its first row.
	};
	std::string data;
	commands = 0;
	const auto close = [&](const Open& open, int bottom) {
		std::format_to(std::back_inserter(data), "M{} {}h{}v{}h-{}z", open.from, open.top, open.to - open.from,
		               bottom - open.top, open.to - open.from);
		commands += 5;
	};
	std::vector<Open> open;
	std::vector<Open> next;
	for(int y = 0; y <= code.size; ++y) {
		next.clear();
		std::size_t o = 0;
		for(int x = 0; y < code.size && x < code.size;) {
			if(!code.at(x, y)) {
				++x;
				continue;
			}
			int end = x;
			while(end < code.size && code.at(end, y)) ++end;
			while(o < open.size() && open[o].from < x) close(open[o++], y);
			if(o < open.size() && open[o].from == x && open[o].to == end) next.push_back(open[o++]);
			else next.push_back({.from = x, .to = end, .top = y});
			x = end;
		}
		while(o < open.size()) close(open[o++], y);
		std::swap(open, next);
	}
	return data;
}

} // namespace affresco::format::detail
