#pragma once

/// @file
/// @brief The image editor's project file: a short header, a JSON manifest of the layers and their settings, then
/// each tile deflated on its own, so a huge image opens at once and its tiles are inflated as they are first shown.
///
/// Layout: the 8 bytes of projectMagic, the format version and the manifest's length as big-endian 32-bit numbers,
/// the manifest (UTF-8 JSON), then the blobs: tiles and the working profile, each found by an offset (from the first
/// blob) and a length in the manifest. modules/imaging/README.md describes the manifest.

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Color.hpp"
#include "affresco/imaging/Document.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace affresco::imaging {

/// @brief The first bytes of every project file.
inline constexpr std::array<std::uint8_t, 8> projectMagic{'T', 'E', 'M', 'P', 'E', 'R', 'A', '\n'};
/// @brief The version of the format written.
inline constexpr std::uint32_t projectVersion = 1;

/// @brief A project read from a file.
struct LoadedProject {
	std::unique_ptr<Document> document;  ///< The layers, their tiles still in the file; no history.
	std::optional<ColorProfile> profile; ///< The working space, or none for sRGB.
	std::size_t tiles = 0;               ///< The tiles the manifest lists, all images together.
};

/// @brief Whether bytes start like a project file.
/// @param file The bytes.
[[nodiscard]] bool isProject(std::span<const std::uint8_t> file) noexcept;

/// @brief Writes a document (not its history or selection) and its working space.
/// @param document The document.
/// @param profile The working space, or none for sRGB.
/// @return The file. Tiles still in the file a document was loaded from are copied without inflating them.
[[nodiscard]] std::vector<std::uint8_t> saveProject(const Document& document,
                                                    const std::optional<ColorProfile>& profile);

/// @brief Reads a project: the manifest now, each tile when first used (a damaged tile reads as transparent).
/// @param file The file, kept by the document's images until they let go of it.
/// @param limits Its size (projectFileBytes, manifestBytes), the image's, the layers', the groups' depth and the
/// profile's.
/// @throws DocumentError when it is malformed or beyond the limits.
[[nodiscard]] LoadedProject loadProject(std::shared_ptr<const std::vector<std::uint8_t>> file, const Limits& limits);

/// @brief An adjustment's settings as JSON, as projects and the app's state keep them.
/// @param adjustment The adjustment.
[[nodiscard]] json adjustmentToJson(const Adjustment& adjustment);

/// @brief An adjustment from its JSON; settings left out keep their defaults, and all are brought into range.
/// @param value The JSON.
/// @throws DocumentError when it is not an adjustment.
[[nodiscard]] Adjustment adjustmentFromJson(const json& value);

} // namespace affresco::imaging
