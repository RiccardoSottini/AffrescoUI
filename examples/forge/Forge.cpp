/// @file
/// @brief Forge's native functions (Forge.hpp).

#include "Forge.hpp"

#include "features/palette/FileIndex.hpp"
#include "features/scm/Scm.hpp"
#include "features/themes/IconTheme.hpp"

#include <affresco/core/Json.hpp>

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace forge {

namespace {

/// @brief Indexes a folder and dresses its tree with the icon theme the call gives.
/// @param folder The folder, or empty for none.
/// @param arguments The call's arguments: `{ "icons": theme }` (IconTheme.hpp), or anything else for no icons.
/// @return indexFolder()'s result, or one with an empty root and no files for no folder.
affresco::json indexWithIcons(const std::filesystem::path& folder, const std::string& arguments) {
	if(folder.empty())
		return {{"root", ""}, {"files", affresco::json::array()}, {"tree", affresco::json::array()}, {"truncated", false}};
	affresco::json index = indexFolder(folder);
	const affresco::json with = affresco::parseUntrusted(arguments);
	if(with.is_object() && with.contains("icons")) applyIconTheme(index["tree"], with["icons"]);
	return index;
}

/// @brief Asks the OS for one path.
/// @param workspace The workspace, whose window shows the dialog.
/// @param dialog The dialog.
/// @return The path chosen, or an empty one for a cancel (or no window, or no dialogs on this system).
std::filesystem::path choose(const Workspace& workspace, const affresco::FileDialog& dialog) {
	if(!workspace.window) return {};
	const std::vector<std::string> chosen = workspace.window->chooseFiles(dialog);
	return chosen.empty() ? std::filesystem::path{} : std::filesystem::path(chosen.front());
}

} // namespace

void registerNatives(affresco::Screen& screen, const std::shared_ptr<Workspace>& workspace) {
	registerScm(screen, workspace); // features/scm
	screen.onCall("forge.index", [workspace](const std::string& arguments) {
		return indexWithIcons(workspace->root, arguments).dump();
	});
	screen.onCall("forge.openFolder", [workspace](const std::string& arguments) {
		const std::filesystem::path folder =
		    choose(*workspace, {.kind = affresco::FileDialogKind::Folder, .title = "Open Folder"});
		if(folder.empty()) return indexWithIcons(workspace->root, arguments).dump();
		const affresco::json index = indexWithIcons(folder, arguments); // throws for a folder that can't be read
		workspace->root = folder;
		return index.dump();
	});
	screen.onCall("forge.read", [workspace](const std::string& arguments) {
		const affresco::json with = affresco::parseUntrusted(arguments);
		const std::string id = with.is_object() ? with.value("id", std::string()) : std::string();
		if(workspace->root.empty()) throw std::runtime_error("no folder is open");
		return affresco::json(readInFolder(workspace->root, id)).dump();
	});
}

void addExampleCatalog(affresco::Screen& screen, const std::filesystem::path& forgeDirectory) {
	screen.features().trustPublisher(examplePublisher);
	screen.features().addCatalog(forgeDirectory / "catalog" / "index.json");
}

} // namespace forge
