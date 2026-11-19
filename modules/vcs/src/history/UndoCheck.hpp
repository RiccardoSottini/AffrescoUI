#pragma once

/// @file
/// @brief Test-only hooks of the undo check (vcs::Repository::checkUndo()): nothing in a document, a revision, a
/// channel or MCP reaches them.

namespace affresco::vcs::testing {

/// @brief Makes checkUndo() take the undo undoRevision() last worked out whatever patch the revision holds (a
/// comparison dropped, as a bug would): verify mode then reports it and judges the revision in full.
/// @param on true to drop the comparison; false (the default) to keep it.
void acceptAnyRememberedPatch(bool on) noexcept;

} // namespace affresco::vcs::testing
