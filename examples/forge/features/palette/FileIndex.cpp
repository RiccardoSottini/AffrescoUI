/// @file
/// @brief Forge's file index (FileIndex.hpp): one walk of the open folder, bounded, without following links.

#include "features/palette/FileIndex.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace forge {

namespace fs = std::filesystem;

namespace {

/// @brief Folders the walk never enters: build output and dependencies, which only crowd quick open.
constexpr std::array<std::string_view, 6> skippedFolders = {"build", "node_modules", "out", "target", "dist",
                                                            "__pycache__"};

/// @brief A folder of the tree being built: its subfolders and files by name, so they come out sorted.
struct Folder {
	std::map<std::string, std::unique_ptr<Folder>> folders; ///< Subfolders, by name.
	std::map<std::string, std::string> files;               ///< Files: name → id.
};

/// @brief Whether the walk skips an entry: hidden ones (".git" too) and the skipped folders.
/// @param name The entry's file name.
/// @param folder Whether it is a folder.
/// @return Whether to skip it.
bool skipped(const std::string& name, bool folder) {
	if(name.empty() || name.front() == '.') return true;
	return folder && std::ranges::find(skippedFolders, name) != skippedFolders.end();
}

/// @brief Adds a file to the tree under its folders.
/// @param root The tree.
/// @param id The file's path relative to the root, "/" separated.
void addToTree(Folder& root, const std::string& id) {
	Folder* folder = &root;
	std::size_t start = 0;
	for(std::size_t slash = id.find('/'); slash != std::string::npos; slash = id.find('/', start)) {
		auto& next = folder->folders[id.substr(start, slash - start)];
		if(!next) next = std::make_unique<Folder>();
		folder = next.get();
		start = slash + 1;
	}
	folder->files.emplace(id.substr(start), id);
}

/// @brief Converts a folder of the tree to `tree` items: folders first, then files, each by name.
/// @param folder The folder.
/// @param prefix Its path relative to the root, with a trailing "/" (empty for the root).
/// @return The items.
affresco::json treeItems(const Folder& folder, const std::string& prefix) {
	affresco::json items = affresco::json::array();
	for(const auto& [name, child] : folder.folders) {
		const std::string id = prefix + name;
		items.push_back({{"id", id + "/"}, {"label", name}, {"children", treeItems(*child, id + "/")}});
	}
	for(const auto& [name, id] : folder.files) items.push_back({{"id", id}, {"label", name}});
	return items;
}

/// @brief Whether the walk indexes the entry it is at as a file; it doesn't descend into a folder it skips.
/// @param entry The walk, at the entry.
/// @param limits The limits, for the depth.
/// @param error Set when the entry can't be read.
/// @return True for a regular file that isn't a link, isn't skipped, and isn't too deep.
bool indexedFile(fs::recursive_directory_iterator& entry, const IndexLimits& limits, std::error_code& error) {
	const bool folder = entry->is_directory(error) && !entry->is_symlink(error);
	if(skipped(entry->path().filename().string(), folder) || entry.depth() >= limits.depth) {
		if(folder) entry.disable_recursion_pending();
		return false;
	}
	return !folder && entry->is_regular_file(error) && !entry->is_symlink(error);
}

} // namespace

affresco::json indexFolder(const fs::path& root, const IndexLimits& limits) {
	std::error_code error;
	if(!fs::is_directory(root, error)) throw std::runtime_error("not a folder that can be read");
	const auto walk = fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied, error);
	if(error) throw std::runtime_error("the folder can't be read: " + error.message());

	affresco::json files = affresco::json::array();
	Folder tree;
	bool truncated = false;
	for(auto entry = fs::begin(walk); entry != fs::end(walk); entry.increment(error)) {
		if(error) break;
		if(!indexedFile(entry, limits, error)) continue;
		if(files.size() >= limits.files) {
			truncated = true;
			break;
		}
		const std::string id = entry->path().lexically_relative(root).generic_string();
		const std::string parent = fs::path(id).parent_path().generic_string();
		files.push_back({{"id", id}, {"title", entry->path().filename().string()}, {"detail", parent}});
		addToTree(tree, id);
	}
	// NOLINTNEXTLINE(modernize-use-ranges): json's iterators don't model the ranges concepts
	std::sort(files.begin(), files.end(), [](const affresco::json& a, const affresco::json& b) { return a["id"] < b["id"]; });
	return {{"root", root.filename().string()},
	        {"files", std::move(files)},
	        {"tree", treeItems(tree, "")},
	        {"truncated", truncated}};
}

fs::path insideFolder(const fs::path& root, const std::string& relative) {
	const fs::path path(relative);
	if(relative.empty() || path.is_absolute() || path.has_root_name() || path.has_root_directory()) return {};
	for(const fs::path& part : path)
		if(part == "..") return {};
	// The lexical check above keeps ".." out; resolving links then keeps a link inside the folder from leading out
	std::error_code error;
	const fs::path base = fs::weakly_canonical(root, error);
	if(error) return {};
	const fs::path full = fs::weakly_canonical(base / path, error);
	if(error) return {};
	const auto [end, _] = std::ranges::mismatch(base, full);
	return end == base.end() ? full : fs::path{};
}

std::string readInFolder(const fs::path& root, const std::string& relative, std::size_t maxBytes) {
	const fs::path path = insideFolder(root, relative);
	if(path.empty()) throw std::runtime_error("the file isn't inside the open folder");
	std::error_code error;
	if(!fs::is_regular_file(path, error)) throw std::runtime_error("not a file: " + relative);
	if(fs::file_size(path, error) > maxBytes || error) throw std::runtime_error("the file is too large to open");
	std::ifstream in(path, std::ios::binary);
	if(!in) throw std::runtime_error("the file can't be read: " + relative);
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

namespace {

/// @brief Creates a new file and writes it; fails rather than open what is already there, a link included.
/// @param file The new file.
/// @param text Its bytes.
/// @param permissions Its permissions.
/// @return Whether it was created and written; if it was created but not written, it is removed.
bool createWith(const fs::path& file, const std::string& text, fs::perms permissions) {
#ifdef _WIN32
	if(fs::exists(fs::symlink_status(file))) return false;
	std::ofstream out(file, std::ios::binary | std::ios::trunc);
	std::error_code error;
	if(!out || !out.write(text.data(), static_cast<std::streamsize>(text.size())) || !out.flush()) {
		out.close();
		fs::remove(file, error);
		return false;
	}
	out.close();
	fs::permissions(file, permissions, error);
	return true;
#else
	const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC, 0600);
	if(descriptor < 0) return false;
	bool written = ::fchmod(descriptor, static_cast<mode_t>(permissions) & 07777) == 0;
	for(std::size_t at = 0; written && at < text.size();) {
		const ssize_t count = ::write(descriptor, text.data() + at, text.size() - at);
		if(count > 0) at += static_cast<std::size_t>(count);
		else written = count < 0 && errno == EINTR;
	}
	written = ::close(descriptor) == 0 && written;
	if(!written) ::unlink(file.c_str());
	return written;
#endif
}

} // namespace

std::optional<std::string> writeInFolder(const fs::path& path, const std::string& text) {
	std::error_code error;
	const fs::file_status status = fs::status(path, error);
	const fs::perms permissions = error ? fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read |
	                                          fs::perms::others_read
	                                    : status.permissions();
	std::random_device random;
	for(int attempt = 0; attempt < 8; ++attempt) {
		fs::path temporary = path;
		temporary += ".forge-" + std::to_string(random()) + "~";
		if(!createWith(temporary, text, permissions)) {
			if(fs::exists(fs::symlink_status(temporary, error))) continue; // a name taken: draw another
			return "the file can't be written";
		}
		fs::rename(temporary, path, error);
		if(!error) return std::nullopt;
		fs::remove(temporary, error);
		return "the file can't be replaced";
	}
	return "the file can't be written";
}

} // namespace forge
