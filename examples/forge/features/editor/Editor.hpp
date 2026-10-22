#pragma once

/// @file
/// @brief Forge's editor feature, native side: the open files' buffers (Buffers.hpp) behind the "code" editors, and
/// the functions features/editor/editor.json calls to open, save, revert and close them.

#include "Forge.hpp"
#include "features/editor/Buffers.hpp"

#include <affresco/affresco.hpp>

#include <memory>

namespace forge {

/// @brief Registers the editor feature on a screen, after registerNatives():
///   - the "code" type again, with `"buffer": id` showing that open file's buffer (CodeEditor.hpp);
///   - the data model `models.buffers` (Buffers::model());
///   - the natives `forge.open`, `forge.save`, `forge.revert` and `forge.close` `{ "id" }` (each returns
///     `{ "id", "dirty" }`, or fails with the reason), `forge.saveAll` (returns `{ "saved": [ids] }`) and
///     `forge.todos` (Buffers::todos()) and `forge.outline` `{ "id" }` (Buffers::outline());
///   - an edit that makes a buffer dirty or clean runs the action `bufferChanged` with `{ "id", "dirty" }`.
/// @param screen The screen, before it is built; it must not move afterwards.
/// @param workspace The open folder the files are opened from.
/// @return The buffers, for the features that read them (registerLsp()).
std::shared_ptr<Buffers> registerEditor(affresco::Screen& screen, std::shared_ptr<Workspace> workspace);

} // namespace forge
