#pragma once

/// @file
/// @brief A rename's or a formatting's edits applied to the open folder (`forge.lsp.apply`, Lsp.hpp): to a file's open
/// buffer, else to the file itself.

#include "features/editor/Buffers.hpp"

#include <affresco/core/Json.hpp>

#include <filesystem>

namespace forge::lsp {

/// @brief Applies edits, all or none: each file's are checked before any is changed. A file with an open buffer is
/// changed there (one undo step, left unsaved), any other is written beside itself and renamed over it.
/// @param with `{ "edits" }`: LspProtocol.hpp's edit rows, each naming its file by `id`.
/// @param root The open folder; every file must be inside it.
/// @param buffers The open files' texts.
/// @return `{ "files", "edits" }`.
/// @throws std::runtime_error With the reason nothing was applied.
[[nodiscard]] affresco::json applyToFolder(const affresco::json& with, const std::filesystem::path& root,
                                           const Buffers& buffers);

} // namespace forge::lsp
