#pragma once

/// @file
/// @brief format::editing::ImageOperation: what the "image" step can do to an image, one table that the step runs and
/// the checker and the reference can list.

#include "ImageDocuments.hpp"

#include <span>
#include <string_view>
#include <vector>

namespace affresco::format::editing {

/// @brief Runs an operation on an image.
/// @param image The image.
/// @param with The step's "with", an object.
/// @param window Shows file dialogs; null when there is no window (then opening fails and saving only keeps the
/// bytes for revert).
/// @throws std::exception (DocumentError, mostly) If the operation can't be done; what() says why.
using ImageOperationRunner = void (*)(ImageDocument& image, const json& with, Window* window);

/// @brief One operation of the "image" step.
struct ImageOperation {
	std::string_view name;              ///< Its name, the step's "image" value, e.g. "addLayer".
	std::vector<std::string_view> with; ///< The keys of "with" it reads.
	std::string_view summary;           ///< What it does, in one sentence.
	ImageOperationRunner run = nullptr; ///< Runs it.
};

/// @brief The operations, in the order the reference lists them; built once, never changed.
/// @return The table.
[[nodiscard]] std::span<const ImageOperation> imageOperations();

/// @brief An operation by its name.
/// @param name The name.
/// @return It, or nullptr.
[[nodiscard]] const ImageOperation* findImageOperation(std::string_view name);

/// @brief newImage, open, save, revert (ImageFiles.cpp).
/// @return The operations.
[[nodiscard]] std::vector<ImageOperation> fileOperations();
/// @brief undo, redo, history, clearHistory and the layer operations (ImageLayers.cpp).
/// @return The operations.
[[nodiscard]] std::vector<ImageOperation> layerOperations();
/// @brief select, feather, fillSelection, crop, the filters (ImageEdits.cpp).
/// @return The operations.
[[nodiscard]] std::vector<ImageOperation> editOperations();
/// @brief snapshot, fit, zoom, setTool, setOptions, setProfile, pointer (ImageView.cpp).
/// @return The operations.
[[nodiscard]] std::vector<ImageOperation> viewOperations();

/// @brief The longest layer name or file name an operation takes.
inline constexpr std::size_t maxNameLength = 256;

/// @brief A name argument, at most maxNameLength bytes.
/// @param with The arguments.
/// @param key Its key.
/// @param fallback When it is absent or not text.
/// @return It.
/// @throws DocumentError If it is longer.
[[nodiscard]] std::string nameOf(const json& with, std::string_view key, std::string_view fallback = "");

} // namespace affresco::format::editing
