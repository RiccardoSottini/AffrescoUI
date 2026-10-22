/// @file
/// @brief Forge's explorer (Explorer.hpp).

#include "features/explorer/Explorer.hpp"

#include "features/palette/FileIndex.hpp"
#include "features/themes/IconTheme.hpp"

#include <affresco/services/DirectoryModel.hpp>

#include <algorithm>
#include <array>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace forge {

namespace fs = std::filesystem;
using affresco::json;
using affresco::services::DirectoryModel;

namespace {

/// @brief Folders the explorer leaves out, as quick open does (FileIndex.cpp): build output and dependencies.
constexpr std::array<std::string_view, 6> skippedFolders = {"build",  "node_modules", "out",
                                                            "target", "dist",         "__pycache__"};
/// @brief The most entries a copy makes, and the most ids one drop moves.
constexpr std::size_t mostCopied = 10000, mostDropped = 1000;
/// @brief The deepest a listing goes below the folder.
constexpr int deepest = 24;

/// @brief What the explorer's natives share: the open folder and, with an executor, its watched model.
struct ExplorerState {
	std::shared_ptr<Workspace> workspace;   ///< The open folder.
	affresco::Screen* screen = nullptr;     ///< The screen, which owns this state through its natives.
	affresco::Executor* executor = nullptr; ///< The main thread's, or nullptr for no watching.
	std::shared_ptr<DirectoryModel> model;  ///< The watched folder, once a tree was asked for.
	fs::path watched;                       ///< The folder the model is of.
	bool refreshPending = false;            ///< Whether a refresh is posted and hasn't run yet.
};

/// @brief Whether the explorer leaves an entry out: hidden ones (".git" too) and the skipped folders' names.
/// @param name The entry's name.
/// @return Whether it does.
bool skippedName(const std::string& name) {
	return name.empty() || name.front() == '.' || std::ranges::find(skippedFolders, name) != skippedFolders.end();
}

/// @brief The same as skippedName(), as the watcher's ignore globs.
/// @return The globs.
std::vector<std::string> ignoredGlobs() {
	std::vector<std::string> globs = {".*"};
	for(const std::string_view name : skippedFolders) globs.emplace_back(name);
	return globs;
}

/// @brief Lists a folder at once, on this thread, for a host with no executor: what a DirectoryModel would hold.
/// @param root The folder.
/// @return Its entries in tree order, at most mostExplorerRows + 1 (so the caller sees it was truncated).
std::vector<ExplorerEntry> listNow(const fs::path& root) {
	std::error_code error;
	std::vector<DirectoryModel::Entry> found;
	auto walk = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, error);
	for(const auto end = fs::recursive_directory_iterator(); !error && walk != end && found.size() <= mostExplorerRows;
	    walk.increment(error)) {
		std::error_code ignored; // an entry that can't be read is listed as a file
		const bool folder = walk->is_directory(ignored) && !walk->is_symlink(ignored);
		if(folder && (skippedName(walk->path().filename().string()) || walk.depth() + 1 >= deepest))
			walk.disable_recursion_pending();
		if(skippedName(walk->path().filename().string())) continue;
		found.push_back({walk->path().lexically_relative(root).generic_string(),
		                 folder ? DirectoryModel::Kind::Folder : DirectoryModel::Kind::File, 0});
	}
	std::ranges::sort(found, DirectoryModel::before);
	std::vector<ExplorerEntry> entries;
	entries.reserve(found.size());
	for(DirectoryModel::Entry& entry : found)
		entries.push_back({std::move(entry.path), entry.kind == DirectoryModel::Kind::Folder});
	return entries;
}

/// @brief A watched model's entries.
/// @param model The model.
/// @return Its entries, in its order, at most mostExplorerRows + 1.
std::vector<ExplorerEntry> modelEntries(const DirectoryModel& model) {
	std::vector<ExplorerEntry> entries;
	const std::size_t count = std::min(model.size(), mostExplorerRows + 1);
	entries.reserve(count);
	for(std::size_t index = 0; index < count; ++index) {
		const DirectoryModel::Entry& entry = model.entry(index);
		entries.push_back({entry.path, entry.kind == DirectoryModel::Kind::Folder});
	}
	return entries;
}

/// @brief Posts one run of `explorerRefresh`, unless one is posted already: so a burst of changes refreshes once.
/// @param weak The state (gone with the screen).
void scheduleRefresh(const std::weak_ptr<ExplorerState>& weak) {
	const std::shared_ptr<ExplorerState> state = weak.lock();
	if(!state || state->refreshPending) return;
	state->refreshPending = true;
	state->executor->post([weak] {
		const std::shared_ptr<ExplorerState> alive = weak.lock();
		if(!alive) return;
		alive->refreshPending = false;
		if(affresco::Panel* root = alive->screen->find("root")) alive->screen->runAction("explorerRefresh", *root);
	});
}

/// @brief Watches the open folder, if there is an executor and it isn't watched already.
/// @param state The state.
void watchFolder(const std::shared_ptr<ExplorerState>& state) {
	const fs::path& root = state->workspace->root;
	if(!state->executor || root.empty()) {
		state->model.reset();
		return;
	}
	if(state->model && state->watched == root) return;
	state->watched = root;
	state->model = DirectoryModel::open(*state->executor, root, {.ignore = ignoredGlobs()});
	const std::weak_ptr<ExplorerState> weak = state;
	(void)state->model->onChange.connect([weak](const affresco::ModelChange&) { scheduleRefresh(weak); });
}

/// @brief A call's arguments as an object.
/// @param text The arguments, as JSON text.
/// @return The object (empty if they aren't one).
json argumentsOf(const std::string& text) {
	json with = affresco::parseUntrusted(text);
	return with.is_object() ? with : json::object();
}

/// @brief A string argument.
/// @param with The arguments.
/// @param key Its name.
/// @return Its value, or "" if it isn't a string.
std::string textOf(const json& with, const char* key) {
	return with.contains(key) && with[key].is_string() ? with[key].get<std::string>() : std::string();
}

/// @brief A true-or-false argument.
/// @param with The arguments.
/// @param key Its name.
/// @return Its value, or false if it isn't one.
bool flagOf(const json& with, const char* key) {
	return with.contains(key) && with[key].is_boolean() && with[key].get<bool>();
}

/// @brief An id without the "/" that ends a folder's.
/// @param id The id.
/// @return The path relative to the folder.
std::string withoutSlash(std::string id) {
	while(!id.empty() && id.back() == '/') id.pop_back();
	return id;
}

/// @brief The folder an id names, the open folder itself for "".
/// @param root The open folder.
/// @param id The folder's id.
/// @return Its path (links resolved), or an empty one if it isn't a folder under the open one.
fs::path folderPath(const fs::path& root, const std::string& id) {
	const std::string relative = withoutSlash(id);
	std::error_code error;
	const fs::path path = relative.empty() ? fs::weakly_canonical(root, error) : insideFolder(root, relative);
	return !error && fs::is_directory(path, error) ? path : fs::path{};
}

/// @brief The folder an entry goes in when it is made or dropped next to a row of the tree.
/// @param root The open folder.
/// @param id The row's id ("" for the open folder).
/// @param inside Whether it goes inside the row (else beside it): only a folder's row has an inside.
/// @return The folder's path, or an empty one if it isn't one under the open folder.
fs::path folderNear(const fs::path& root, const std::string& id, bool inside) {
	if(id.empty() || (inside && id.ends_with('/'))) return folderPath(root, id);
	return folderPath(root, fs::path(withoutSlash(id)).parent_path().generic_string());
}

/// @brief The entry an id names: its folder resolved, its own name not (so a link is the link, not its target).
/// @param root The open folder.
/// @param id The entry's id.
/// @return Its path, or an empty one if it isn't an entry under the open folder.
fs::path entryPath(const fs::path& root, const std::string& id) {
	const fs::path relative(withoutSlash(id));
	const std::string name = relative.filename().string();
	if(!isPlainName(name)) return {};
	const fs::path folder = folderPath(root, relative.parent_path().generic_string());
	std::error_code error;
	if(folder.empty() || !fs::exists(fs::symlink_status(folder / name, error))) return {};
	return folder / name;
}

/// @brief The id of an entry under the open folder.
/// @param root The open folder.
/// @param path The entry.
/// @return Its id ("/" at the end of a folder's).
std::string idOf(const fs::path& root, const fs::path& path) {
	std::error_code error;
	const std::string id = path.lexically_relative(fs::weakly_canonical(root, error)).generic_string();
	return fs::is_directory(fs::symlink_status(path, error)) ? id + "/" : id;
}

/// @brief Whether a path is a folder or under it.
/// @param path The path.
/// @param folder The folder.
/// @return Whether it is.
bool isWithin(const fs::path& path, const fs::path& folder) {
	const auto [end, _] = std::ranges::mismatch(folder, path);
	return end == folder.end();
}

/// @brief A name not taken in a folder, for a copy: "main copy.cpp", then "main copy 2.cpp" and so on.
/// @param folder The folder.
/// @param name The name copied.
/// @return The name.
/// @throws std::runtime_error If the first hundred are taken.
std::string freeName(const fs::path& folder, const fs::path& name) {
	std::error_code error;
	for(int number = 1; number <= 100; ++number) {
		const std::string suffix = number == 1 ? " copy" : " copy " + std::to_string(number);
		const std::string candidate = name.stem().string() + suffix + name.extension().string();
		if(!fs::exists(fs::symlink_status(folder / candidate, error))) return candidate;
	}
	throw std::runtime_error("no free name for a copy of " + name.string());
}

/// @brief Counts the entries under a path, without following links, up to a bound.
/// @param path The path.
/// @param most The bound.
/// @return The count, at most @p most + 1.
std::size_t countUnder(const fs::path& path, std::size_t most) {
	std::error_code error;
	std::size_t count = 1;
	if(!fs::is_directory(fs::symlink_status(path, error))) return count;
	auto walk = fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied, error);
	for(const auto end = fs::recursive_directory_iterator(); !error && walk != end && count <= most;
	    walk.increment(error))
		++count;
	return count;
}

/// @brief The tree, from the watched model when it is ready, else listed now.
/// @param state The state.
/// @param arguments `{ "icons" }`.
/// @return `{ "tree", "truncated", "live" }`.
json treeOf(const std::shared_ptr<ExplorerState>& state, const std::string& arguments) {
	const fs::path& root = state->workspace->root;
	if(root.empty()) return {{"tree", json::array()}, {"truncated", false}, {"live", false}};
	watchFolder(state);
	const bool live = state->model && state->model->ready();
	const std::vector<ExplorerEntry> entries = live ? modelEntries(*state->model) : listNow(root);
	json tree = explorerItems(entries);
	const json with = argumentsOf(arguments);
	if(with.contains("icons")) applyIconTheme(tree, with["icons"]);
	const bool truncated = entries.size() > mostExplorerRows || (live && state->model->truncated());
	return {{"tree", std::move(tree)}, {"truncated", truncated}, {"live", live}};
}

/// @brief Makes an empty file or a folder.
/// @param root The open folder.
/// @param with `{ "in", "name", "folder" }`: `in` is a folder's id, or a file's (then its folder).
/// @return Its id.
/// @throws std::runtime_error If the name or the folder isn't one, or the name is taken.
std::string createEntry(const fs::path& root, const json& with) {
	const fs::path folder = folderNear(root, textOf(with, "in"), true);
	const std::string name = textOf(with, "name");
	if(folder.empty() || !isPlainName(name)) throw std::runtime_error("not a name a file can have here: " + name);
	const fs::path path = folder / name;
	std::error_code error;
	if(fs::exists(fs::symlink_status(path, error))) throw std::runtime_error(name + " already exists");
	if(flagOf(with, "folder")) {
		if(!fs::create_directory(path, error)) throw std::runtime_error("can't make the folder " + name);
	} else if(!std::ofstream(path, std::ios::binary)) throw std::runtime_error("can't make the file " + name);
	return idOf(root, path);
}

/// @brief Renames an entry in its folder.
/// @param root The open folder.
/// @param with `{ "id", "name" }`.
/// @return The new id.
/// @throws std::runtime_error If the entry or the name isn't one, or the name is taken.
std::string renameEntry(const fs::path& root, const json& with) {
	const fs::path from = entryPath(root, textOf(with, "id"));
	const std::string name = textOf(with, "name");
	if(from.empty() || !isPlainName(name)) throw std::runtime_error("can't rename that to " + name);
	const fs::path to = from.parent_path() / name;
	std::error_code error;
	// A rename that only changes the case finds the same file on a folder that ignores case
	if(fs::exists(fs::symlink_status(to, error)) && !fs::equivalent(from, to, error))
		throw std::runtime_error(name + " already exists");
	fs::rename(from, to, error);
	if(error) throw std::runtime_error("can't rename: " + error.message());
	return idOf(root, to);
}

/// @brief Deletes an entry, and all it holds.
/// @param root The open folder.
/// @param with `{ "id" }`.
/// @throws std::runtime_error If it isn't an entry under the open folder, or can't be deleted.
void removeEntry(const fs::path& root, const json& with) {
	const fs::path path = entryPath(root, textOf(with, "id"));
	if(path.empty()) throw std::runtime_error("nothing to delete there");
	std::error_code error;
	fs::remove_all(path, error);
	if(error) throw std::runtime_error("can't delete: " + error.message());
}

/// @brief Moves or copies one entry into a folder.
/// @param from The entry.
/// @param into The folder.
/// @param copy Whether to copy it.
/// @return Where it is now.
/// @throws std::runtime_error If a folder would go into itself, a move's name is taken, a copy is too large, or the
/// file system refuses.
fs::path dropOne(const fs::path& from, const fs::path& into, bool copy) {
	if(isWithin(into, from)) throw std::runtime_error("can't put a folder inside itself");
	std::error_code error;
	if(!copy && from.parent_path() == into) return from;
	const fs::path to = into / (copy && fs::exists(fs::symlink_status(into / from.filename(), error))
	                                ? fs::path(freeName(into, from.filename()))
	                                : from.filename());
	if(fs::exists(fs::symlink_status(to, error)))
		throw std::runtime_error(from.filename().string() + " is there already");
	if(copy && countUnder(from, mostCopied) > mostCopied) throw std::runtime_error("too many files to copy");
	if(copy) fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::copy_symlinks, error);
	else fs::rename(from, to, error);
	if(error) throw std::runtime_error((copy ? "can't copy: " : "can't move: ") + error.message());
	return to;
}

/// @brief Moves or copies entries into a folder (dragged there).
/// @param root The open folder.
/// @param with `{ "ids", "into", "place", "copy" }`: `place` is the tree's ("before", "inside" or "after" the row
/// `into`; inside by default).
/// @return Their new ids.
/// @throws std::runtime_error If the folder or an entry isn't one under the open folder, or dropOne() fails (the
/// entries before it are moved).
json dropEntries(const fs::path& root, const json& with) {
	const std::string place = textOf(with, "place");
	const fs::path into = folderNear(root, textOf(with, "into"), place.empty() || place == "inside");
	if(into.empty()) throw std::runtime_error("can't drop there");
	if(!with.contains("ids") || !with["ids"].is_array() || with["ids"].size() > mostDropped)
		throw std::runtime_error("nothing to drop");
	json ids = json::array();
	for(const json& id : with["ids"]) {
		const fs::path from = entryPath(root, id.is_string() ? id.get<std::string>() : std::string());
		if(from.empty()) throw std::runtime_error("nothing to move there");
		ids.push_back(idOf(root, dropOne(from, into, flagOf(with, "copy"))));
	}
	return ids;
}

} // namespace

json explorerItems(const std::vector<ExplorerEntry>& entries, std::size_t most) {
	json items = json::array();
	// The open folders, outermost first: each one's id prefix and its children (only the last one grows, so the others
	// stay where they are)
	std::vector<std::pair<std::string, json*>> open = {{"", &items}};
	std::size_t made = 0;
	for(const ExplorerEntry& entry : entries) {
		if(made == most) break;
		const auto depth = static_cast<std::size_t>(std::ranges::count(entry.path, '/'));
		if(depth >= open.size()) continue;
		open.resize(depth + 1);
		if(!entry.path.starts_with(open.back().first)) continue; // its folder wasn't listed
		const std::string name = entry.path.substr(open.back().first.size());
		const std::string id = entry.folder ? entry.path + "/" : entry.path;
		json& added = open.back().second->emplace_back(json{{"id", id}, {"label", name}});
		if(entry.folder) open.emplace_back(id, &(added["children"] = json::array()));
		++made;
	}
	return items;
}

std::vector<std::string> explorerAncestors(const std::string& id) {
	const std::string relative = withoutSlash(id);
	std::vector<std::string> folders;
	for(std::size_t slash = relative.find('/'); slash != std::string::npos; slash = relative.find('/', slash + 1))
		folders.push_back(relative.substr(0, slash + 1));
	return folders;
}

bool isPlainName(const std::string& name) noexcept {
	if(name.empty() || name.size() > 255 || name == "." || name == "..") return false;
	return std::ranges::none_of(name, [](char letter) {
		const auto code = static_cast<unsigned char>(letter);
		return letter == '/' || letter == '\\' || code < 0x20 || code == 0x7f;
	});
}

void registerExplorer(affresco::Screen& screen, std::shared_ptr<Workspace> workspace, affresco::Executor* executor) {
	const auto state = std::make_shared<ExplorerState>();
	state->workspace = std::move(workspace);
	state->screen = &screen;
	state->executor = executor;
	screen.onCall("forge.explorer.tree",
	              [state](const std::string& arguments) { return treeOf(state, arguments).dump(); });
	screen.onCall("forge.explorer.create", [state](const std::string& arguments) {
		return json(createEntry(state->workspace->root, argumentsOf(arguments))).dump();
	});
	screen.onCall("forge.explorer.rename", [state](const std::string& arguments) {
		return json(renameEntry(state->workspace->root, argumentsOf(arguments))).dump();
	});
	screen.onCall("forge.explorer.delete", [state](const std::string& arguments) {
		removeEntry(state->workspace->root, argumentsOf(arguments));
		return json::object().dump();
	});
	screen.onCall("forge.explorer.drop", [state](const std::string& arguments) {
		return dropEntries(state->workspace->root, argumentsOf(arguments)).dump();
	});
	screen.onCall("forge.explorer.reveal", [](const std::string& arguments) {
		return json(explorerAncestors(textOf(argumentsOf(arguments), "id"))).dump();
	});
}

} // namespace forge
