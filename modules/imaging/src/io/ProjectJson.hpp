#pragma once

/// @file
/// @brief The project manifest's JSON readers and writers for a layer's settings (private to the imaging module).

#include "affresco/core/Json.hpp"
#include "affresco/imaging/Document.hpp"

#include <cstdint>
#include <string>

namespace affresco::imaging::project {

/// @brief Refuses a project.
/// @param what What is wrong.
/// @throws DocumentError Always: "imaging: the project file <what>".
[[noreturn]] void malformed(const std::string& what);

/// @brief A whole number from 0 to a most.
/// @param value The JSON.
/// @param most The largest allowed.
/// @param what Its name, for the error.
/// @throws DocumentError otherwise.
[[nodiscard]] std::uint64_t whole(const json& value, std::uint64_t most, const std::string& what);

/// @brief A layer's settings (kind, name, visibility, opacity, blend, locks, clipping, mask on, adjustment) as JSON.
/// @param layer The layer.
[[nodiscard]] json layerSettingsToJson(const Layer& layer);

/// @brief Reads a layer's settings; those left out keep their defaults.
/// @param value The layer's JSON object.
/// @param layer Its settings are set.
/// @throws DocumentError when a setting is malformed or the name is over 255 bytes.
void readLayerSettings(const json& value, Layer& layer);

} // namespace affresco::imaging::project
