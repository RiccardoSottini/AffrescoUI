#pragma once

/// @file
/// @brief Forge's language servers, native side (features/lsp/lsp.json): one server per language, spoken to through
/// the `jsonrpc` service the screen declares for it (`lsp_cpp`, `lsp_python`, `lsp_typescript`). The client keeps the
/// protocol's bookkeeping the screen can't: initialize, the open documents and their versions, incremental changes,
/// positions in the encoding the server chose; what a server answers is bounded by LspProtocol.hpp and reaches the
/// screen as actions.

#include "Forge.hpp"
#include "features/editor/Buffers.hpp"

#include <affresco/affresco.hpp>

#include <filesystem>
#include <memory>
#include <string_view>

namespace forge {

/// @brief Finds a program on PATH: the first one of that name in one of PATH's absolute folders (findGit()'s search).
/// @param name The program, without ".exe" (added on Windows).
/// @return Its absolute path, or an empty path if there is none.
[[nodiscard]] std::filesystem::path findProgram(std::string_view name);

/// @brief Grants the language servers found on PATH as the programs the lsp services start: `clangd` (C and C++),
/// `pyright-langserver --stdio` (Python) and `typescript-language-server --stdio` (JavaScript and TypeScript). A
/// server that isn't installed isn't granted, so its service is refused and the editor works without it.
/// @param screen The screen.
void grantLanguageServers(affresco::Screen& screen);

/// @brief Registers the language server natives, after registerEditor():
///   - `forge.lsp.request` `{ "kind", "id", "language", "line", "column", "request", "newName", "query" }`: asks the
///     language's server (kinds "hover", "completion", "signature", "definition", "references", "rename", "format",
///     "symbols", "workspaceSymbols", "tokens"), opening the document with it first; returns `{ "pending" }` at once
///     (false when no server serves the language), and the answer, normalized (LspProtocol.hpp), runs the action
///     `lspShow` with `{ "kind", "id", "request", "result" }` or `{ "kind", "id", "request", "error" }`;
///   - `forge.lsp.normalize` `{ "kind", "result", "id", "root" }`: normalize() for a result given by hand (the
///     scenarios), its URIs inside `root` ("/folder", say) or else the open folder;
///   - `forge.lsp.apply` `{ "edits" }`: applies a rename's or a formatting's edits, all or none: to the open buffer of
///     a file (one undo step, left unsaved), else to the file itself (written beside it, then renamed over it);
///     returns `{ "files", "edits" }` or fails with the reason;
///   - `forge.lsp.status`: `{ service: { "state", "initialized", "documents" } }`.
/// A server's diagnostics run the action `lspDiagnostics` with normalize()'s "diagnostics" shape. A server that
/// crashed is started again by its declaration's `restart` (at most 3 times), and initialized again.
/// @param screen The screen, before it is built; it must not move afterwards.
/// @param workspace The open folder.
/// @param buffers The open files' texts (registerEditor()).
/// @param executor The main thread, which the servers' answers are posted to; nullptr for none (no server starts).
void registerLsp(affresco::Screen& screen, std::shared_ptr<Workspace> workspace, std::shared_ptr<Buffers> buffers,
                 affresco::Executor* executor);

} // namespace forge
