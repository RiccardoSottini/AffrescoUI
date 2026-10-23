#pragma once

/// @file
/// @brief Forge's native functions: the few things its JSON can't do itself, which need the file system or the OS's
/// dialogs. Everything else (the shell, the palette, the themes and the panels) is in the screen's JSON.

#include <affresco/affresco.hpp>

#include <filesystem>
#include <memory>
#include <string_view>

namespace forge {

/// @brief The folder Forge has open, shared by its native functions.
struct Workspace {
	std::filesystem::path root;        ///< The open folder; empty for none.
	affresco::Window* window = nullptr; ///< The window, for file dialogs; set once it is created.
};

/// @brief Registers Forge's native functions, for `call` steps. Their arguments never name a path outside the open
/// folder: a folder is opened, and a theme imported, only through the OS's dialog.
///   - `forge.index` `{ "icons" }`: the open folder's file index (FileIndex.hpp), its tree dressed with the file icon
///     theme `icons` (IconTheme.hpp); `{ "root": "" }` with none open;
///   - `forge.openFolder` `{ "icons" }`: asks for a folder, opens it and returns its index (the same one again on a
///     cancel);
///   - `forge.read` `{ "id" }`: the text of one of the folder's files;
///   - `forge.importTheme`: asks for a colour theme and returns it as tokens (ThemeImport.hpp); fails on a
///     cancel.
/// And the source control's (features/scm/Scm.hpp), with git granted.
/// @param screen The screen.
/// @param workspace The open folder, which the functions read and change.
void registerNatives(affresco::Screen& screen, const std::shared_ptr<Workspace>& workspace);

/// @brief The public key of the example catalogue's publisher (catalog/, README.md). Its private half was discarded
/// once the catalogue was signed, so nothing more can be signed with it.
inline constexpr std::string_view examplePublisher = "ed25519:wvzomDMyk9aPCGQCvTzNPugq4saaDmJdO_8LaeLFYQc";

/// @brief Trusts the example catalogue's publisher, for this screen only, and reads the catalogue, so the features
/// service offers its features (word count and the TODO tree).
/// @param screen The screen.
/// @param forgeDirectory Forge's folder (examples/forge), which holds catalog/index.json.
/// @throws affresco::DocumentError If the catalogue was changed after signing.
void addExampleCatalog(affresco::Screen& screen, const std::filesystem::path& forgeDirectory);

} // namespace forge
