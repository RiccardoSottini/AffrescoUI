/// @file
/// @brief PNG and JPEG through the module's stb build, with ICC profiles found in and added to the files' own
/// containers: PNG's iCCP chunk (zlib) and JPEG's APP2 "ICC_PROFILE" segments (a profile split in numbered parts).

#include "affresco/imaging/Codec.hpp"

#include "Stb.hpp"
#include "affresco/core/Json.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <map>
#include <string_view>
#include <utility>

namespace affresco::imaging {

namespace {

constexpr std::array<std::uint8_t, 8> pngSignature{0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
constexpr std::string_view iccMarker{"ICC_PROFILE\0", 12};
constexpr std::size_t jpegSegmentData = 65535 - 2 - iccMarker.size() - 2; ///< Profile bytes in one APP2 segment.
constexpr std::size_t pngChunks = 10000;   ///< The most chunks read before the pixels, looking for iCCP.
constexpr std::size_t jpegSegments = 1000; ///< The most segments read before the scan, looking for APP2.

/// @brief A big-endian number of four bytes.
/// @param at Its first byte.
[[nodiscard]] std::uint32_t read32(const std::uint8_t* at) noexcept {
	return static_cast<std::uint32_t>(at[0]) << 24 | static_cast<std::uint32_t>(at[1]) << 16 |
	       static_cast<std::uint32_t>(at[2]) << 8 | at[3];
}

/// @brief Appends a big-endian number of four bytes.
/// @param out The bytes.
/// @param value The number.
void write32(std::vector<std::uint8_t>& out, std::uint32_t value) {
	out.insert(out.end(), {static_cast<std::uint8_t>(value >> 24), static_cast<std::uint8_t>(value >> 16),
	                       static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value)});
}

/// @brief PNG's CRC-32 (ISO 3309) of bytes.
/// @param bytes The chunk's type and data.
[[nodiscard]] std::uint32_t crc32(std::span<const std::uint8_t> bytes) noexcept {
	std::uint32_t crc = 0xffffffffU;
	for(const std::uint8_t byte : bytes) {
		crc ^= byte;
		for(int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
	}
	return crc ^ 0xffffffffU;
}

/// @brief The profile in a PNG's iCCP chunk.
/// @param file The PNG.
/// @param limits profileBytes.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> pngProfile(std::span<const std::uint8_t> file,
                                                                  const Limits& limits) {
	std::size_t at = pngSignature.size();
	for(std::size_t chunk = 0; chunk < pngChunks && at + 12 <= file.size(); ++chunk) {
		const std::size_t length = read32(file.data() + at);
		const std::string_view type(reinterpret_cast<const char*>(file.data() + at + 4), 4);
		if(length > file.size() - at - 12) throw DocumentError("imaging: a PNG chunk runs past the file's end");
		if(type == "IDAT" || type == "IEND") return std::nullopt;
		if(type == "iCCP") {
			const std::span<const std::uint8_t> data = file.subspan(at + 8, length);
			const std::span<const std::uint8_t> head = data.first(std::min<std::size_t>(data.size(), 80));
			const auto name = static_cast<std::size_t>(std::ranges::find(head, 0) - head.begin());
			if(name + 2 > data.size() || data[name] != 0 || data[name + 1] != 0)
				throw DocumentError("imaging: a malformed iCCP chunk");
			auto bytes = stb::inflate(data.subspan(name + 2), limits.profileBytes);
			if(!bytes) throw DocumentError("imaging: the embedded colour profile is broken or too big");
			return bytes;
		}
		at += length + 12;
	}
	return std::nullopt;
}

/// @brief Whether a JPEG segment is a part of an ICC profile: an APP2 with the marker and the part's two numbers.
/// @param marker The segment's marker.
/// @param data Its data.
[[nodiscard]] bool iccSegment(std::uint8_t marker, std::span<const std::uint8_t> data) noexcept {
	return marker == 0xe2 && data.size() >= iccMarker.size() + 2 &&
	       std::memcmp(data.data(), iccMarker.data(), iccMarker.size()) == 0;
}

/// @brief The parts of a JPEG's profile, gathered in any order and put back together in theirs.
struct ProfileParts {
	std::map<int, std::span<const std::uint8_t>> parts; ///< The parts' data, by sequence number.
	int count = 0;                                      ///< The parts declared (0 before the first).
	std::size_t total = 0;                              ///< Their bytes so far.

	/// @brief Adds a part, checking it against the others and the limit.
	/// @param data An ICC segment's data.
	/// @param limits profileBytes.
	void add(std::span<const std::uint8_t> data, const Limits& limits) {
		const int sequence = data[iccMarker.size()], declared = data[iccMarker.size() + 1];
		if(sequence == 0 || sequence > declared || (count != 0 && declared != count) || parts.contains(sequence))
			throw DocumentError("imaging: the embedded colour profile's parts are inconsistent");
		count = declared;
		parts[sequence] = data.subspan(iccMarker.size() + 2);
		total += parts[sequence].size();
		if(total > limits.profileBytes) throw DocumentError("imaging: the embedded colour profile is too big");
	}

	/// @brief The profile, once every part is in.
	/// @return It, or nothing without parts.
	[[nodiscard]] std::optional<std::vector<std::uint8_t>> joined() const {
		if(parts.empty()) return std::nullopt;
		if(std::cmp_not_equal(parts.size(), count))
			throw DocumentError("imaging: the embedded colour profile is incomplete");
		std::vector<std::uint8_t> bytes;
		bytes.reserve(total);
		for(const auto& [sequence, part] : parts) bytes.insert(bytes.end(), part.begin(), part.end());
		return bytes;
	}
};

/// @brief The profile in a JPEG's APP2 segments, put back together.
/// @param file The JPEG.
/// @param limits profileBytes.
[[nodiscard]] std::optional<std::vector<std::uint8_t>> jpegProfile(std::span<const std::uint8_t> file,
                                                                   const Limits& limits) {
	ProfileParts parts;
	std::size_t at = 2;
	for(std::size_t segment = 0; segment < jpegSegments && at + 4 <= file.size() && file[at] == 0xff; ++segment) {
		const std::uint8_t marker = file[at + 1];
		if(marker == 0xda || marker == 0xd9) break; // the scan, or the end
		const std::size_t length = static_cast<std::size_t>(file[at + 2]) << 8 | file[at + 3];
		if(length < 2 || length > file.size() - at - 2)
			throw DocumentError("imaging: a JPEG segment runs past the end");
		const std::span<const std::uint8_t> data = file.subspan(at + 4, length - 2);
		at += length + 2;
		if(iccSegment(marker, data)) parts.add(data, limits);
	}
	return parts.joined();
}

/// @brief Checks a header against the limits.
/// @param header The header.
/// @param limits The limits.
void checkSize(const stb::Header& header, const Limits& limits) {
	const int side = std::min(limits.imageFileSide, limits.side);
	if(header.width < 1 || header.height < 1 || header.width > side || header.height > side)
		throw DocumentError("imaging: the image is " + std::to_string(header.width) + " by " +
		                    std::to_string(header.height) + " pixels, beyond " + std::to_string(side) + " a side");
	if(static_cast<std::size_t>(header.width) * static_cast<std::size_t>(header.height) > limits.pixels)
		throw DocumentError("imaging: the image has more pixels than the limit");
}

/// @brief Makes a tile of decoded pixels, or null if all transparent.
/// @param decoded The pixels.
/// @param format rgba8 or rgba16f.
/// @param column Its column.
/// @param row Its row.
[[nodiscard]] std::shared_ptr<const Tile> tileOf(const stb::Decoded& decoded, PixelFormat format, int column, int row) {
	const auto tile = std::make_shared<Tile>(format);
	const int left = column * tileSize, top = row * tileSize;
	const int right = std::min(left + tileSize, decoded.width), bottom = std::min(top + tileSize, decoded.height);
	for(int y = top; y < bottom; ++y) {
		const std::size_t start =
		    (static_cast<std::size_t>(y) * static_cast<std::size_t>(decoded.width) + static_cast<std::size_t>(left)) *
		    4;
		if(format == PixelFormat::rgba8) {
			std::memcpy(tile->bytes().data() + static_cast<std::size_t>(y - top) * tileSize * 4,
			            decoded.bytes8.data() + start, static_cast<std::size_t>(right - left) * 4);
			continue;
		}
		for(int x = left; x < right; ++x) {
			const std::uint16_t* at = decoded.bytes16.data() + start + static_cast<std::size_t>(x - left) * 4;
			tile->setPixel(x - left, y - top, {at[0] / 65535.0f, at[1] / 65535.0f, at[2] / 65535.0f, at[3] / 65535.0f});
		}
	}
	return tile->empty() ? nullptr : tile;
}

/// @brief The image's pixels, 8 bits RGBA, rows top first.
/// @param image The image.
[[nodiscard]] std::vector<std::uint8_t> rgbaOf(const Image& image) {
	const auto width = static_cast<std::size_t>(image.width());
	std::vector<std::uint8_t> rgba(width * static_cast<std::size_t>(image.height()) * 4, 0);
	for(int row = 0; row < image.rows(); ++row)
		for(int column = 0; column < image.columns(); ++column) {
			const std::shared_ptr<const Tile> tile = image.tile({0, column, row});
			if(!tile) continue;
			const int left = column * tileSize, top = row * tileSize;
			const int right = std::min(left + tileSize, image.width()),
			          bottom = std::min(top + tileSize, image.height());
			for(int y = top; y < bottom; ++y)
				for(int x = left; x < right; ++x) {
					const Pixel pixel = tile->pixel(x - left, y - top);
					std::uint8_t* at =
					    rgba.data() + (static_cast<std::size_t>(y) * width + static_cast<std::size_t>(x)) * 4;
					at[0] = byteFromUnit(pixel.red), at[1] = byteFromUnit(pixel.green);
					at[2] = byteFromUnit(pixel.blue), at[3] = byteFromUnit(pixel.alpha);
				}
		}
	return rgba;
}

/// @brief Adds an iCCP chunk after a PNG's IHDR.
/// @param png The PNG stb wrote.
/// @param profile The profile's bytes.
void embedInPng(std::vector<std::uint8_t>& png, std::span<const std::uint8_t> profile) {
	constexpr std::size_t afterHeader = 8 + 4 + 4 + 13 + 4;
	std::vector<std::uint8_t> chunk{'i', 'C', 'C', 'P', 'I', 'C', 'C', 0, 0};
	const std::vector<std::uint8_t> stream = stb::deflate(profile);
	chunk.insert(chunk.end(), stream.begin(), stream.end());
	std::vector<std::uint8_t> framed;
	write32(framed, static_cast<std::uint32_t>(chunk.size() - 4));
	framed.insert(framed.end(), chunk.begin(), chunk.end());
	write32(framed, crc32(chunk));
	png.insert(png.begin() + static_cast<std::ptrdiff_t>(afterHeader), framed.begin(), framed.end());
}

/// @brief Adds APP2 segments after a JPEG's SOI and JFIF APP0.
/// @param jpeg The JPEG stb wrote.
/// @param profile The profile's bytes.
void embedInJpeg(std::vector<std::uint8_t>& jpeg, std::span<const std::uint8_t> profile) {
	std::size_t at = 2;
	if(jpeg.size() > 6 && jpeg[2] == 0xff && jpeg[3] == 0xe0)
		at += 2 + (static_cast<std::size_t>(jpeg[4]) << 8 | jpeg[5]);
	const std::size_t count = (profile.size() + jpegSegmentData - 1) / jpegSegmentData;
	// Each segment numbers itself and the count in a byte
	if(count > 255) throw DocumentError("imaging: the colour profile is too big for a JPEG");
	std::vector<std::uint8_t> segments;
	for(std::size_t part = 0; part < count; ++part) {
		const std::span<const std::uint8_t> data =
		    profile.subspan(part * jpegSegmentData, std::min(jpegSegmentData, profile.size() - part * jpegSegmentData));
		const std::size_t length = 2 + iccMarker.size() + 2 + data.size();
		segments.insert(segments.end(),
		                {0xff, 0xe2, static_cast<std::uint8_t>(length >> 8), static_cast<std::uint8_t>(length)});
		segments.insert(segments.end(), iccMarker.begin(), iccMarker.end());
		segments.insert(segments.end(), {static_cast<std::uint8_t>(part + 1), static_cast<std::uint8_t>(count)});
		segments.insert(segments.end(), data.begin(), data.end());
	}
	jpeg.insert(jpeg.begin() + static_cast<std::ptrdiff_t>(at), segments.begin(), segments.end());
}

} // namespace

std::optional<ImageFileFormat> imageFileFormat(std::span<const std::uint8_t> file) noexcept {
	if(file.size() >= pngSignature.size() && std::equal(pngSignature.begin(), pngSignature.end(), file.begin()))
		return ImageFileFormat::png;
	if(file.size() >= 3 && file[0] == 0xff && file[1] == 0xd8 && file[2] == 0xff) return ImageFileFormat::jpeg;
	return std::nullopt;
}

std::optional<std::vector<std::uint8_t>> embeddedProfile(std::span<const std::uint8_t> file, const Limits& limits) {
	const auto format = imageFileFormat(file);
	if(!format) return std::nullopt;
	return *format == ImageFileFormat::png ? pngProfile(file, limits) : jpegProfile(file, limits);
}

DecodedImage decodeImageFile(std::span<const std::uint8_t> file, const Limits& limits) {
	if(file.size() > limits.imageFileBytes) throw DocumentError("imaging: the image file is too big");
	if(!imageFileFormat(file)) throw DocumentError("imaging: not a PNG or JPEG file");
	const std::optional<stb::Header> header = stb::header(file);
	if(!header) throw DocumentError("imaging: a malformed PNG or JPEG header");
	checkSize(*header, limits);
	const std::optional<stb::Decoded> decoded = stb::decode(file, *header);
	if(!decoded) throw DocumentError("imaging: the PNG or JPEG file is broken");
	const PixelFormat format = header->sixteenBit ? PixelFormat::rgba16f : PixelFormat::rgba8;
	DecodedImage result{std::make_shared<Image>(header->width, header->height, format, limits), std::nullopt, {}};
	for(int row = 0; row < result.image->rows(); ++row)
		for(int column = 0; column < result.image->columns(); ++column)
			if(auto tile = tileOf(*decoded, format, column, row)) result.image->setTile(column, row, std::move(tile));
	try {
		if(const auto bytes = embeddedProfile(file, limits)) {
			ColorProfile profile = ColorProfile::parse(*bytes, limits);
			if(profile.model() == ColorModel::cmyk) throw DocumentError("imaging: a CMYK profile on RGB pixels");
			result.profile = std::move(profile);
		}
	} catch(const DocumentError& error) {
		result.warning = std::string(error.what()) + "; the image is treated as sRGB";
	}
	return result;
}

std::vector<std::uint8_t> encodeImageFile(const Image& image, ImageFileFormat format,
                                          const std::optional<ColorProfile>& profile, int quality) {
	// Before the pixels are gathered, so a size stb can't write costs nothing
	const bool fits = format == ImageFileFormat::png ? stb::pngFits(image.width(), image.height())
	                                                 : stb::jpegFits(image.width(), image.height());
	if(!fits) throw DocumentError("imaging: the image could not be encoded");
	const std::vector<std::uint8_t> rgba = rgbaOf(image);
	std::vector<std::uint8_t> file = format == ImageFileFormat::png
	                                     ? stb::writePng(rgba, image.width(), image.height())
	                                     : stb::writeJpeg(rgba, image.width(), image.height(), quality);
	if(file.empty()) throw DocumentError("imaging: the image could not be encoded");
	if(profile && format == ImageFileFormat::png) embedInPng(file, profile->bytes());
	if(profile && format == ImageFileFormat::jpeg) embedInJpeg(file, profile->bytes());
	return file;
}

} // namespace affresco::imaging
