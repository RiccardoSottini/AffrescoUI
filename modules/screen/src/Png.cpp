/// @file
/// @brief A small PNG encoder (RFC 2083, with RFC 1950/1951 zlib and deflate) and a PPM reader.

#include "Png.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>

namespace affresco::detail {

namespace {

/// @brief The CRC-32 lookup table PNG chunks use (polynomial 0xEDB88320).
constexpr std::array<std::uint32_t, 256> crcTable = [] {
	std::array<std::uint32_t, 256> table{};
	for(std::uint32_t n = 0; n < 256; ++n) {
		std::uint32_t c = n;
		for(int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		table[n] = c;
	}
	return table;
}();

/// @brief The CRC-32 of some bytes.
/// @param bytes The bytes.
/// @return The checksum.
std::uint32_t crc32(std::string_view bytes) {
	std::uint32_t c = 0xFFFFFFFFu;
	for(const char byte : bytes) c = crcTable[(c ^ static_cast<std::uint8_t>(byte)) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFu;
}

/// @brief The Adler-32 checksum that ends a zlib stream.
/// @param bytes The uncompressed bytes.
/// @return The checksum.
std::uint32_t adler32(std::span<const std::uint8_t> bytes) {
	std::uint32_t a = 1, b = 0;
	for(const std::uint8_t byte : bytes) {
		a = (a + byte) % 65521;
		b = (b + a) % 65521;
	}
	return (b << 16) | a;
}

/// @brief Appends a 32-bit number, most significant byte first (PNG's and zlib's order).
/// @param out Where to append.
/// @param value The number.
void appendBigEndian(std::string& out, std::uint32_t value) {
	for(int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<char>((value >> shift) & 0xFF));
}

/// @brief Writes bits least significant first, as deflate packs them.
class BitWriter {
public:
	/// @brief Starts writing at the end of a string.
	/// @param out Where the bytes go; must outlive the writer.
	explicit BitWriter(std::string& out) : out_(out) {}

	/// @brief Writes a number's low bits, least significant first (deflate's extra bits and block header).
	/// @param value The number.
	/// @param count How many bits (at most 16).
	void write(std::uint32_t value, int count) {
		buffer_ |= value << used_;
		used_ += count;
		while(used_ >= 8) {
			out_.push_back(static_cast<char>(buffer_ & 0xFF));
			buffer_ >>= 8;
			used_ -= 8;
		}
	}

	/// @brief Writes a Huffman code, most significant bit first.
	/// @param code The code.
	/// @param length Its length in bits.
	void writeCode(std::uint32_t code, int length) {
		std::uint32_t reversed = 0;
		for(int i = 0; i < length; ++i) reversed |= ((code >> i) & 1) << (length - 1 - i);
		write(reversed, length);
	}

	/// @brief Writes the last, partly filled byte.
	void flush() {
		if(used_ > 0) out_.push_back(static_cast<char>(buffer_ & 0xFF));
		buffer_ = 0;
		used_ = 0;
	}

private:
	std::string& out_;         ///< Where the bytes go.
	std::uint32_t buffer_ = 0; ///< Bits not yet written.
	int used_ = 0;             ///< How many bits buffer_ holds.
};

/// Deflate's length codes 257..285: the shortest length of each, and its extra bits (RFC 1951, 3.2.5).
constexpr std::array<int, 29> lengthBase = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                            31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<int, 29> lengthExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                             2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0}; ///< See lengthBase.
/// Deflate's distance codes 0..29: the shortest distance of each, and its extra bits.
constexpr std::array<int, 30> distanceBase = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                              33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                              1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<int, 30> distanceExtra = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                               6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13}; ///< See above.

/// @brief Writes a literal/length symbol with deflate's fixed Huffman codes (RFC 1951, 3.2.6).
/// @param bits The stream.
/// @param symbol 0..255 a byte, 256 the end of the block, 257..285 a length.
void writeSymbol(BitWriter& bits, int symbol) {
	const auto s = static_cast<std::uint32_t>(symbol);
	if(symbol < 144) bits.writeCode(0x30 + s, 8);
	else if(symbol < 256) bits.writeCode(0x190 + s - 144, 9);
	else if(symbol < 280) bits.writeCode(s - 256, 7);
	else bits.writeCode(0xC0 + s - 280, 8);
}

/// @brief Writes a back-reference: copy @p length bytes from @p distance bytes back.
/// @param bits The stream.
/// @param length 3..258.
/// @param distance 1..32768.
void writeMatch(BitWriter& bits, int length, int distance) {
	const auto lengthCode =
	    static_cast<std::size_t>(std::ranges::upper_bound(lengthBase, length) - lengthBase.begin() - 1);
	writeSymbol(bits, 257 + static_cast<int>(lengthCode));
	bits.write(static_cast<std::uint32_t>(length - lengthBase[lengthCode]), lengthExtra[lengthCode]);

	const auto distanceCode =
	    static_cast<std::size_t>(std::ranges::upper_bound(distanceBase, distance) - distanceBase.begin() - 1);
	bits.writeCode(static_cast<std::uint32_t>(distanceCode), 5);
	bits.write(static_cast<std::uint32_t>(distance - distanceBase[distanceCode]), distanceExtra[distanceCode]);
}

/// @brief Finds earlier copies of the bytes ahead in a 32 KiB window (LZ77), through chains of 3-byte hashes.
class MatchFinder {
public:
	static constexpr int window = 32768; ///< The farthest deflate can refer back.
	static constexpr int minimum = 3;    ///< The shortest match deflate can encode.
	static constexpr int maximum = 258;  ///< The longest.

	/// @brief Searches some bytes.
	/// @param data The bytes; must outlive the finder.
	explicit MatchFinder(std::span<const std::uint8_t> data)
	    : data_(data), head_(std::size_t{1} << hashBits, -1), previous_(window, -1) {}

	/// @brief Remembers that the bytes at a position start here, for later matches.
	/// @param position The position.
	void insert(int position) {
		if(position + minimum > size()) return;
		int& head = head_[hash(position)];
		previous_[static_cast<std::size_t>(position & (window - 1))] = head;
		head = position;
	}

	/// @brief The longest earlier copy of the bytes at a position, among the most recent candidates.
	/// @param position The position (not inserted yet).
	/// @return {length, distance}; length is 0 if there is none of at least `minimum` bytes.
	[[nodiscard]] std::pair<int, int> find(int position) const {
		if(position + minimum > size()) return {0, 0};
		const int limit = std::min(maximum, size() - position);
		int best = 0, distance = 0;
		int candidate = head_[hash(position)];
		for(int probes = 0; probes < maxProbes && candidate >= 0 && position - candidate <= window; ++probes) {
			int length = 0;
			while(length < limit && at(candidate + length) == at(position + length)) ++length;
			if(length > best) {
				best = length;
				distance = position - candidate;
				if(length == limit) break;
			}
			const int next = previous_[static_cast<std::size_t>(candidate & (window - 1))];
			if(next >= candidate) break; // the slot was reused by a newer position: the chain ends here
			candidate = next;
		}
		return best >= minimum ? std::pair{best, distance} : std::pair{0, 0};
	}

private:
	static constexpr int hashBits = 15; ///< Size of the hash table, as a power of two.
	static constexpr int maxProbes = 8; ///< Candidates tried per position: speed over the last few percent of size.

	/// @brief The number of bytes.
	[[nodiscard]] int size() const { return static_cast<int>(data_.size()); }
	/// @brief The byte at a position.
	/// @param position The position.
	[[nodiscard]] std::uint8_t at(int position) const { return data_[static_cast<std::size_t>(position)]; }
	/// @brief The hash of the three bytes at a position.
	/// @param position The position.
	[[nodiscard]] std::size_t hash(int position) const {
		const std::uint32_t key =
		    at(position) | (std::uint32_t{at(position + 1)} << 8) | (std::uint32_t{at(position + 2)} << 16);
		return (key * 2654435761u) >> (32 - hashBits);
	}

	std::span<const std::uint8_t> data_; ///< The bytes.
	std::vector<int> head_;              ///< hash -> the latest position with it, or -1.
	std::vector<int> previous_;          ///< position % window -> the previous position with the same hash, or -1.
};

/// @brief Compresses bytes into a zlib stream: one deflate block with the fixed Huffman codes.
/// @param data The bytes.
/// @return The stream.
std::string zlibCompress(std::span<const std::uint8_t> data) {
	std::string out = {static_cast<char>(0x78), static_cast<char>(0x01)}; // deflate, 32 KiB window, no dictionary
	BitWriter bits(out);
	bits.write(1, 1); // the last block
	bits.write(1, 2); // fixed Huffman codes

	MatchFinder matches(data);
	const int size = static_cast<int>(data.size());
	for(int position = 0; position < size;) {
		const auto [length, distance] = matches.find(position);
		if(length == 0) {
			writeSymbol(bits, data[static_cast<std::size_t>(position)]);
			matches.insert(position++);
			continue;
		}
		writeMatch(bits, length, distance);
		for(const int end = position + length; position < end; ++position) matches.insert(position);
	}
	writeSymbol(bits, 256);
	bits.flush();
	appendBigEndian(out, adler32(data));
	return out;
}

/// @brief The PNG row filters tried (Paeth is left out: flat screens gain little from it over Sub and Up).
enum class Filter : std::uint8_t { None = 0, Sub = 1, Up = 2 };

/// @brief Filters one row: each byte minus its left (Sub) or upper (Up) neighbour, or as is.
/// @param filter The filter.
/// @param row The row's bytes.
/// @param above The row above (zeros for the first row).
/// @param out Receives the filtered bytes (as many as the row has).
void filterRow(Filter filter, std::span<const std::uint8_t> row, std::span<const std::uint8_t> above,
               std::span<std::uint8_t> out) {
	for(std::size_t i = 0; i < row.size(); ++i) {
		const std::uint8_t left = i >= 3 ? row[i - 3] : 0;
		const std::uint8_t predicted = filter == Filter::Sub ? left : filter == Filter::Up ? above[i] : 0;
		out[i] = static_cast<std::uint8_t>(row[i] - predicted);
	}
}

/// @brief Filters every row with whichever filter leaves the smallest values (the usual PNG heuristic).
/// @param width In pixels.
/// @param height In pixels.
/// @param rgb The pixels.
/// @return Each row's filter byte followed by its filtered bytes.
std::vector<std::uint8_t> filterRows(int width, int height, std::span<const std::uint8_t> rgb) {
	const std::size_t stride = static_cast<std::size_t>(width) * 3;
	const std::vector<std::uint8_t> zeros(stride, 0);
	std::vector<std::uint8_t> filtered((stride + 1) * static_cast<std::size_t>(height));
	std::vector<std::uint8_t> trial(stride);

	for(std::size_t y = 0; y < static_cast<std::size_t>(height); ++y) {
		const auto row = rgb.subspan(y * stride, stride);
		const auto above = y > 0 ? rgb.subspan((y - 1) * stride, stride) : std::span<const std::uint8_t>(zeros);
		const std::span<std::uint8_t> out(filtered.data() + y * (stride + 1) + 1, stride);

		std::size_t bestCost = ~std::size_t{0};
		for(const Filter filter : {Filter::None, Filter::Sub, Filter::Up}) {
			filterRow(filter, row, above, trial);
			std::size_t cost = 0;
			for(const std::uint8_t byte : trial)
				cost += static_cast<std::size_t>(std::abs(static_cast<std::int8_t>(byte)));
			if(cost < bestCost) {
				bestCost = cost;
				filtered[y * (stride + 1)] = static_cast<std::uint8_t>(filter);
				std::ranges::copy(trial, out.begin());
			}
		}
	}
	return filtered;
}

/// @brief Appends a PNG chunk: length, type, data and CRC.
/// @param out The file.
/// @param type The four-letter type.
/// @param data Its data.
void appendChunk(std::string& out, std::string_view type, std::string_view data) {
	appendBigEndian(out, static_cast<std::uint32_t>(data.size()));
	const std::size_t start = out.size();
	out.append(type);
	out.append(data);
	appendBigEndian(out, crc32(std::string_view(out).substr(start)));
}

} // namespace

std::optional<RgbImage> readPpm(const std::filesystem::path& path) {
	std::ifstream file(path, std::ios::binary);
	std::string magic;
	int maximum = 0;
	RgbImage image;
	if(!(file >> magic >> image.width >> image.height >> maximum) || magic != "P6" || maximum != 255)
		return std::nullopt;
	if(image.width <= 0 || image.height <= 0 || image.width > 32768 || image.height > 32768) return std::nullopt;
	file.get(); // the one whitespace before the pixels

	image.rgb.resize(static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 3);
	file.read(reinterpret_cast<char*>(image.rgb.data()), static_cast<std::streamsize>(image.rgb.size()));
	if(file.gcount() != static_cast<std::streamsize>(image.rgb.size())) return std::nullopt;
	return image;
}

std::string encodePng(int width, int height, std::span<const std::uint8_t> rgb) {
	if(width <= 0 || height <= 0 ||
	   rgb.size() != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3)
		return {};

	std::string header;
	appendBigEndian(header, static_cast<std::uint32_t>(width));
	appendBigEndian(header, static_cast<std::uint32_t>(height));
	header += {8, 2, 0, 0, 0}; // 8 bits per sample, RGB, deflate, adaptive filters, not interlaced

	std::string png = "\x89PNG\r\n\x1a\n";
	appendChunk(png, "IHDR", header);
	appendChunk(png, "IDAT", zlibCompress(filterRows(width, height, rgb)));
	appendChunk(png, "IEND", {});
	return png;
}

} // namespace affresco::detail
