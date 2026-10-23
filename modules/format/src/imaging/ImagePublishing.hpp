#pragma once

/// @file
/// @brief Attaches a runtime's images (editing::ImageDocuments) to it: where they are declared, and how the screen
/// hears of them.

namespace affresco::format {

struct Engine;

/// @brief Attaches the runtime's images to it: they are declared by its document's "images" section, and an image
/// that changed is published to the screen (its snapshot to its "into" state path, then an update of what reads it;
/// its thumbnails to the image cache).
/// @param engine The runtime.
void attachImageDocuments(Engine& engine);

/// @brief Detaches the runtime's images from it, as it goes away: the views that outlive it publish nothing.
/// @param engine The runtime.
void detachImageDocuments(Engine& engine) noexcept;

} // namespace affresco::format
