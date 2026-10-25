#pragma once

/// @file
/// @brief A QR code encoder (model 2, byte mode, versions 1 to 40, the four error correction levels), for the
/// `qrCode` expression function, and the path data that draws a code's dark modules.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format::detail {

/// How much of a code can be lost and still be read.
enum class QrLevel : std::uint8_t {
	Low,      ///< L: about 7%.
	Medium,   ///< M: about 15%.
	Quartile, ///< Q: about 25%.
	High,     ///< H: about 30%.
};

/// @brief A QR code: a square of modules, dark or light, without the quiet zone around it.
struct QrCode {
	int version = 0;                 ///< 1 to 40.
	int size = 0;                    ///< Modules on a side: 17 + 4 * version.
	int mask = 0;                    ///< The mask pattern used, 0 to 7.
	std::vector<std::uint8_t> dark;  ///< size * size, row after row: 1 for a dark module.

	/// @brief Tells whether a module is dark.
	/// @param x Its column, below size.
	/// @param y Its row, below size.
	/// @return true for dark.
	[[nodiscard]] bool at(int x, int y) const noexcept {
		return dark[static_cast<std::size_t>(y) * static_cast<std::size_t>(size) + static_cast<std::size_t>(x)] != 0;
	}
};

/// @brief Encodes bytes in the smallest version that holds them, in byte mode.
/// @param bytes The bytes (a text's UTF-8).
/// @param level The error correction level.
/// @param mask The mask pattern, 0 to 7, or -1 for the one the standard's penalty rules prefer.
/// @return The code, or nullopt when the bytes don't fit version 40 (2953 bytes at Low, 1273 at High).
[[nodiscard]] std::optional<QrCode> encodeQr(std::string_view bytes, QrLevel level, int mask = -1);

/// @brief The path data of a code's dark modules, each module a 1 by 1 square from (0, 0) to (size, size): runs of
/// a row as rectangles, those that repeat on the rows below merged into one.
/// @param code The code.
/// @param commands Receives how many path commands the data has.
/// @return Data such as "M0 0h7v1h-7z...".
[[nodiscard]] std::string qrPathData(const QrCode& code, std::size_t& commands);

} // namespace affresco::format::detail
