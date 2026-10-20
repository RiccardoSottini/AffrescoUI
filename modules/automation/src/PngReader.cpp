/// @file
/// @brief A small PNG decoder (RFC 2083) with its own inflate (RFC 1950 and 1951), after zlib's "puff": canonical
/// Huffman tables decoded a bit at a time. Pictures are compared once per expectation, so clarity beats speed here.

#include "PngReader.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string_view>

namespace affresco::automation::detail {

namespace {

/// @brief Why a stream or file can't be read; caught at the boundary and turned into an error message.
class Malformed : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

constexpr int maxCodeLength = 15;     ///< Deflate's longest Huffman code.
constexpr int lengthSymbols = 286;    ///< Literal/length symbols deflate can use (0-285).
constexpr int distanceSymbols = 30;   ///< Distance symbols deflate can use (0-29).

/// @brief Base lengths of length symbols 257-285 (RFC 1951, 3.2.5).
constexpr std::array<int, 29> lengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                            31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
/// @brief Extra bits of length symbols 257-285.
constexpr std::array<int, 29> lengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                             2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
/// @brief Base distances of distance symbols 0-29.
constexpr std::array<int, 30> distanceBase = {1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
                                              193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
/// @brief Extra bits of distance symbols 0-29.
constexpr std::array<int, 30> distanceExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                               6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
/// @brief The order code length code lengths come in (RFC 1951, 3.2.7).
constexpr std::array<int, 19> codeLengthOrder = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

/// @brief Reads a stream a bit at a time, least significant bit first, as deflate packs them.
class Bits {
public:
	/// @brief Reads from bytes.
	/// @param bytes The stream; must outlive the reader.
	explicit Bits(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

	/// @brief Reads a number of bits.
	/// @param count How many, up to 16.
	/// @return Them, the first read in the lowest bit.
	/// @throws Malformed If the stream ends first.
	int read(int count) {
		int value = 0;
		for(int i = 0; i < count; ++i) value |= bit() << i;
		return value;
	}

	/// @brief Reads one bit.
	/// @return 0 or 1.
	/// @throws Malformed If the stream ends.
	int bit() {
		if(position_ >= bytes_.size() * 8) throw Malformed("the compressed data ends too early");
		const int value = (bytes_[position_ / 8] >> (position_ % 8)) & 1;
		++position_;
		return value;
	}

	/// @brief Skips to the next whole byte, as a stored block starts.
	void alignToByte() noexcept { position_ = (position_ + 7) / 8 * 8; }

	/// @brief Reads a whole byte; only after alignToByte().
	/// @return The byte.
	/// @throws Malformed If the stream ends.
	std::uint8_t byte() { return static_cast<std::uint8_t>(read(8)); }

private:
	std::span<const std::uint8_t> bytes_; ///< The stream.
	std::size_t position_ = 0;            ///< The next bit.
};

/// @brief A canonical Huffman code: how many codes each length has, and the symbols in code order.
class Huffman {
public:
	/// @brief Builds the code from each symbol's code length.
	/// @param lengths 0 (unused) to 15, per symbol.
	/// @throws Malformed If the lengths overfill the code space.
	explicit Huffman(std::span<const int> lengths) : symbols_(lengths.size()) {
		for(const int length : lengths) ++counts_[static_cast<std::size_t>(length)];
		int left = 1; // codes of the current length still free
		for(std::size_t length = 1; length <= maxCodeLength; ++length) {
			left = left * 2 - counts_[length];
			if(left < 0) throw Malformed("a Huffman code has more codes than fit");
		}
		std::array<int, maxCodeLength + 2> offsets{};
		for(std::size_t length = 1; length <= maxCodeLength; ++length)
			offsets[length + 1] = offsets[length] + counts_[length];
		for(std::size_t symbol = 0; symbol < lengths.size(); ++symbol)
			if(lengths[symbol] != 0)
				symbols_[static_cast<std::size_t>(offsets[static_cast<std::size_t>(lengths[symbol])]++)] =
				    static_cast<int>(symbol);
	}

	/// @brief Reads one symbol.
	/// @param bits The stream.
	/// @return The symbol.
	/// @throws Malformed If no code matches.
	int decode(Bits& bits) const {
		int code = 0, first = 0, index = 0;
		for(std::size_t length = 1; length <= maxCodeLength; ++length) {
			code |= bits.bit();
			const int count = counts_[length];
			if(code - count < first) return symbols_[static_cast<std::size_t>(index + (code - first))];
			index += count;
			first = (first + count) << 1;
			code <<= 1;
		}
		throw Malformed("a Huffman code that the table doesn't have");
	}

private:
	std::array<int, maxCodeLength + 1> counts_{}; ///< Codes of each length.
	std::vector<int> symbols_;                    ///< Symbols, in canonical code order.
};

/// @brief Inflates deflate blocks into a buffer of a known size.
class Inflater {
public:
	/// @brief Reads a stream.
	/// @param bits The deflate data.
	/// @param expected The size the output must have.
	Inflater(Bits& bits, std::size_t expected) : bits_(bits), expected_(expected) { out_.reserve(expected); }

	/// @brief Inflates every block.
	/// @return The output.
	/// @throws Malformed If the data is malformed or gives another size.
	std::vector<std::uint8_t> run() {
		bool last = false;
		while(!last) {
			last = bits_.bit() == 1;
			switch(bits_.read(2)) {
			case 0: stored(); break;
			case 1: fixed(); break;
			case 2: dynamic(); break;
			default: throw Malformed("a deflate block of an unknown type");
			}
		}
		if(out_.size() != expected_) throw Malformed("the image data is shorter than the image");
		return std::move(out_);
	}

private:
	/// @brief Appends a byte, never past the expected size.
	/// @param byte The byte.
	void put(std::uint8_t byte) {
		if(out_.size() >= expected_) throw Malformed("the image data is longer than the image");
		out_.push_back(byte);
	}

	/// @brief An uncompressed block.
	void stored() {
		bits_.alignToByte();
		const int length = bits_.byte() | (bits_.byte() << 8);
		const int complement = bits_.byte() | (bits_.byte() << 8);
		if(length != (~complement & 0xffff)) throw Malformed("a stored block's length doesn't match its check");
		for(int i = 0; i < length; ++i) put(bits_.byte());
	}

	/// @brief A block with deflate's fixed codes.
	void fixed() {
		static const std::pair<Huffman, Huffman> fixedCodes = [] {
			std::array<int, 288> lengths{};
			std::fill_n(lengths.begin(), 144, 8);
			std::fill_n(lengths.begin() + 144, 112, 9);
			std::fill_n(lengths.begin() + 256, 24, 7);
			std::fill_n(lengths.begin() + 280, 8, 8);
			std::array<int, distanceSymbols> distances{};
			distances.fill(5);
			return std::pair{Huffman(lengths), Huffman(distances)};
		}();
		codes(fixedCodes.first, fixedCodes.second);
	}

	/// @brief A block with its own codes, which come first, themselves Huffman-coded.
	void dynamic() {
		const int literals = bits_.read(5) + 257, distances = bits_.read(5) + 1, codeLengths = bits_.read(4) + 4;
		if(literals > lengthSymbols || distances > distanceSymbols) throw Malformed("a block with too many codes");
		std::array<int, 19> lengthOfLength{};
		for(int i = 0; i < codeLengths; ++i) lengthOfLength[static_cast<std::size_t>(codeLengthOrder[static_cast<std::size_t>(i)])] = bits_.read(3);
		const Huffman lengthCode(lengthOfLength);

		std::vector<int> lengths;
		const auto total = static_cast<std::size_t>(literals + distances);
		while(lengths.size() < total) {
			const int symbol = lengthCode.decode(bits_);
			if(symbol < 16) {
				lengths.push_back(symbol);
				continue;
			}
			if(symbol == 16 && lengths.empty()) throw Malformed("a repeated code length with nothing before it");
			const int value = symbol == 16 ? lengths.back() : 0;
			const int repeat = symbol == 16 ? 3 + bits_.read(2) : symbol == 17 ? 3 + bits_.read(3) : 11 + bits_.read(7);
			if(lengths.size() + static_cast<std::size_t>(repeat) > total) throw Malformed("code lengths that overflow");
			lengths.insert(lengths.end(), static_cast<std::size_t>(repeat), value);
		}
		if(lengths[256] == 0) throw Malformed("a block without an end code");
		const std::span<const int> all(lengths);
		codes(Huffman(all.first(static_cast<std::size_t>(literals))),
		      Huffman(all.subspan(static_cast<std::size_t>(literals))));
	}

	/// @brief Decodes literals and back-references until the end of the block.
	/// @param literal The literal/length code.
	/// @param distance The distance code.
	void codes(const Huffman& literal, const Huffman& distance) {
		for(int symbol = literal.decode(bits_); symbol != 256; symbol = literal.decode(bits_)) {
			if(symbol < 256) {
				put(static_cast<std::uint8_t>(symbol));
				continue;
			}
			const auto lengthIndex = static_cast<std::size_t>(symbol - 257);
			if(lengthIndex >= lengthBase.size()) throw Malformed("a length code out of range");
			const int length = lengthBase[lengthIndex] + bits_.read(lengthExtra[lengthIndex]);
			const auto distanceIndex = static_cast<std::size_t>(distance.decode(bits_));
			if(distanceIndex >= distanceBase.size()) throw Malformed("a distance code out of range");
			const auto back = static_cast<std::size_t>(distanceBase[distanceIndex] + bits_.read(distanceExtra[distanceIndex]));
			if(back > out_.size()) throw Malformed("a reference before the start of the data");
			for(int i = 0; i < length; ++i) put(out_[out_.size() - back]);
		}
	}

	Bits& bits_;                     ///< The stream.
	std::size_t expected_;           ///< The size the output must have.
	std::vector<std::uint8_t> out_;  ///< The output so far.
};

/// @brief Reads a big-endian 32-bit number (PNG's and zlib's order).
/// @param bytes At least 4 bytes.
/// @return The number.
std::uint32_t bigEndian(std::span<const std::uint8_t> bytes) {
	return (std::uint32_t{bytes[0]} << 24) | (std::uint32_t{bytes[1]} << 16) | (std::uint32_t{bytes[2]} << 8) |
	       std::uint32_t{bytes[3]};
}

/// @brief The Adler-32 checksum that ends a zlib stream.
/// @param data The data.
/// @return Its checksum.
std::uint32_t adler32(std::span<const std::uint8_t> data) {
	std::uint32_t a = 1, b = 0;
	for(const std::uint8_t byte : data) {
		a = (a + byte) % 65521;
		b = (b + a) % 65521;
	}
	return (b << 16) | a;
}

/// @brief Paeth's predictor (PNG filter 4).
/// @param left The byte to the left.
/// @param up The byte above.
/// @param corner The byte above to the left.
/// @return The one closest to left + up - corner.
int paeth(int left, int up, int corner) {
	const int estimate = left + up - corner;
	const int toLeft = std::abs(estimate - left), toUp = std::abs(estimate - up), toCorner = std::abs(estimate - corner);
	if(toLeft <= toUp && toLeft <= toCorner) return left;
	return toUp <= toCorner ? up : corner;
}

/// @brief Undoes the row filters, in place.
/// @param data The rows, each after its filter byte.
/// @param stride Bytes per row, without the filter byte.
/// @param channels Bytes per pixel.
/// @throws Malformed If a row names an unknown filter.
void unfilter(std::vector<std::uint8_t>& data, std::size_t stride, std::size_t channels) {
	const std::size_t rows = data.size() / (stride + 1);
	for(std::size_t y = 0; y < rows; ++y) {
		std::uint8_t* row = data.data() + y * (stride + 1) + 1;
		const std::uint8_t* above = y > 0 ? row - (stride + 1) : nullptr;
		const int filter = row[-1];
		if(filter > 4) throw Malformed("a row with an unknown filter");
		for(std::size_t i = 0; i < stride && filter != 0; ++i) {
			const int left = i >= channels ? row[i - channels] : 0;
			const int up = above ? above[i] : 0;
			const int corner = above && i >= channels ? above[i - channels] : 0;
			const int predicted = filter == 1 ? left : filter == 2 ? up : filter == 3 ? (left + up) / 2 : paeth(left, up, corner);
			row[i] = static_cast<std::uint8_t>(row[i] + predicted);
		}
	}
}

/// @brief What a PNG's chunks say: its header's numbers and its compressed image data.
struct Chunks {
	std::uint32_t width = 0, height = 0; ///< In pixels.
	std::size_t channels = 0;            ///< 3 (RGB) or 4 (RGBA).
	std::vector<std::uint8_t> data;      ///< The IDAT chunks, joined.
};

/// @brief Reads a PNG's chunks.
/// @param file The file.
/// @return Its header and image data.
/// @throws Malformed If it isn't a PNG of the kind decodePng() reads.
Chunks readChunks(std::span<const std::uint8_t> file) {
	static constexpr std::array<std::uint8_t, 8> signature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	if(file.size() < 8 || !std::equal(signature.begin(), signature.end(), file.begin())) throw Malformed("not a PNG file");
	Chunks chunks;
	for(std::size_t at = 8; at + 12 <= file.size();) {
		const std::uint32_t length = bigEndian(file.subspan(at));
		if(length > file.size() - at - 12) throw Malformed("a chunk longer than the file");
		const std::string_view kind(reinterpret_cast<const char*>(file.data() + at + 4), 4);
		const std::span<const std::uint8_t> body = file.subspan(at + 8, length);
		if(kind == "IHDR") {
			if(length != 13) throw Malformed("a header of the wrong size");
			chunks.width = bigEndian(body);
			chunks.height = bigEndian(body.subspan(4));
			if(body[8] != 8 || (body[9] != 2 && body[9] != 6) || body[10] != 0 || body[11] != 0 || body[12] != 0)
				throw Malformed("not an 8-bit RGB or RGBA image without interlacing");
			chunks.channels = body[9] == 2 ? 3 : 4;
		} else if(kind == "IDAT") {
			chunks.data.insert(chunks.data.end(), body.begin(), body.end());
		} else if(kind == "IEND") {
			break;
		}
		at += 12 + length;
	}
	if(chunks.channels == 0) throw Malformed("no header");
	if(chunks.width == 0 || chunks.height == 0 ||
	   std::size_t{chunks.width} * chunks.height > maxPicturePixels)
		throw Malformed("an image of " + std::to_string(chunks.width) + " x " + std::to_string(chunks.height) +
		                " pixels, more than " + std::to_string(maxPicturePixels));
	return chunks;
}

/// @brief Decompresses a zlib stream, throwing.
/// @param stream The stream.
/// @param expected The size it must give.
/// @return The bytes.
/// @throws Malformed If it can't.
std::vector<std::uint8_t> inflateOrThrow(std::span<const std::uint8_t> stream, std::size_t expected) {
	if(stream.size() < 6) throw Malformed("the compressed data is too short");
	if((stream[0] & 0x0f) != 8 || (stream[0] >> 4) > 7 || ((stream[0] << 8) | stream[1]) % 31 != 0 || (stream[1] & 0x20))
		throw Malformed("not a zlib stream of deflate data without a dictionary");
	Bits bits(stream.subspan(2, stream.size() - 6));
	std::vector<std::uint8_t> out = Inflater(bits, expected).run();
	if(adler32(out) != bigEndian(stream.subspan(stream.size() - 4))) throw Malformed("the data's checksum is wrong");
	return out;
}

} // namespace

std::optional<std::vector<std::uint8_t>> inflateZlib(std::span<const std::uint8_t> stream, std::size_t expected,
                                                     std::string& error) {
	try {
		return inflateOrThrow(stream, expected);
	} catch(const Malformed& problem) {
		error = problem.what();
		return std::nullopt;
	}
}

std::optional<Pixels> decodePng(std::span<const std::uint8_t> file, std::string& error) {
	try {
		const Chunks chunks = readChunks(file);
		const std::size_t stride = std::size_t{chunks.width} * chunks.channels;
		std::vector<std::uint8_t> data = inflateOrThrow(chunks.data, (stride + 1) * chunks.height);
		unfilter(data, stride, chunks.channels);

		Pixels pixels{static_cast<int>(chunks.width), static_cast<int>(chunks.height), {}};
		pixels.rgb.reserve(std::size_t{chunks.width} * chunks.height * 3);
		for(std::size_t y = 0; y < chunks.height; ++y) {
			const std::uint8_t* row = data.data() + y * (stride + 1) + 1;
			for(std::size_t x = 0; x < chunks.width; ++x)
				pixels.rgb.insert(pixels.rgb.end(), row + x * chunks.channels, row + x * chunks.channels + 3);
		}
		return pixels;
	} catch(const Malformed& problem) {
		error = problem.what();
		return std::nullopt;
	}
}

} // namespace affresco::automation::detail
