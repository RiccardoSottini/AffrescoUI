#pragma once

/// @file
/// @brief DirectoryModel: every file and folder under a folder, as a DataModel a screen reads by row
/// (`"repeat": {"in": "models.files"}`), listed on a thread of its own and kept up to date by a Watcher.

#include "affresco/core/DataModel.hpp"
#include "affresco/core/Executor.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief What a DirectoryModel lists and how.
struct DirectoryOptions {
	std::vector<std::string> ignore;         ///< Globs of paths neither listed nor watched (e.g. ".git", "*.o").
	std::chrono::milliseconds debounce{100}; ///< How long changes are gathered before they are applied.
	std::size_t mostEntries = 200000;        ///< Rows at most; past them the model is truncated().
	bool watch = true;                       ///< Whether changes are watched (else it is listed once).
};

/// @brief Every file and folder under a folder, one row each, in the order a tree shows them: each folder before
/// what it holds, folders before files, names in byte order.
///
/// A row is {path, name, kind ("file", "folder" or "other"), depth (0 at the top), size (files only, else null)}, path
/// relative to the root with `/`. Listing and watching happen on its own threads; rows change on the main thread
/// (posted through the Executor), and onChange tells which ones: Inserted and Removed ranges for files and folders
/// that came and went, Changed for a new size, Reset after the first listing and a rescan. Symbolic links are listed
/// but never followed. Invariants: main thread only; at most DirectoryOptions::mostEntries rows.
// NOLINTNEXTLINE(misc-multiple-inheritance): enable_shared_from_this is a mixin, for its threads' weak references
class DirectoryModel final : public DataModel, public std::enable_shared_from_this<DirectoryModel> {
	struct Key {}; ///< Keeps the constructor for open().

public:
	/// @brief What an entry is.
	enum class Kind : std::uint8_t { Folder, File, Other };
	/// @brief One file or folder.
	struct Entry {
		std::string path;        ///< Relative to the root, with `/`.
		Kind kind = Kind::File;  ///< What it is.
		std::uintmax_t size = 0; ///< A file's size in bytes, else 0.
		/// @brief Whether two entries are the same.
		/// @return Whether they are.
		bool operator==(const Entry&) const = default;
	};

	/// @brief Opens a folder: starts watching it, then lists it. Main thread.
	/// @param executor The main thread, which outlives the model.
	/// @param root The folder.
	/// @param options What to ignore, how many rows at most.
	/// @return The model, empty until ready().
	[[nodiscard]] static std::shared_ptr<DirectoryModel> open(Executor& executor, std::filesystem::path root,
	                                                          DirectoryOptions options = {});
	/// @brief For open() only.
	/// @param executor See open().
	/// @param root See open().
	/// @param options See open().
	DirectoryModel(Key /*key*/, Executor& executor, std::filesystem::path root, DirectoryOptions options);
	/// @brief Stops watching and listing, and waits for its threads.
	~DirectoryModel() override;
	DirectoryModel(const DirectoryModel&) = delete;
	DirectoryModel& operator=(const DirectoryModel&) = delete;
	DirectoryModel(DirectoryModel&&) = delete;
	DirectoryModel& operator=(DirectoryModel&&) = delete;

	/// @brief The number of rows.
	/// @return It.
	[[nodiscard]] std::size_t size() const override { return entries_.size(); }
	/// @brief One row: {path, name, kind, depth, size}.
	/// @param index Its index; past the end gives null.
	/// @return It.
	[[nodiscard]] json row(std::size_t index) const override;

	/// @brief One entry.
	/// @param index Its index, below size().
	/// @return It.
	[[nodiscard]] const Entry& entry(std::size_t index) const noexcept { return entries_[index]; }
	/// @brief Where an entry is.
	/// @param path Its path, relative to the root.
	/// @return Its index, or std::nullopt if it isn't listed.
	[[nodiscard]] std::optional<std::size_t> find(std::string_view path) const;
	/// @brief One past the last row under an entry (its own index + 1 for a file).
	/// @param index The entry's index, below size().
	/// @return The end of its subtree.
	[[nodiscard]] std::size_t subtreeEnd(std::size_t index) const noexcept;
	/// @brief The folder, canonical.
	/// @return It.
	[[nodiscard]] const std::filesystem::path& root() const noexcept { return root_; }
	/// @brief Whether the first listing is in.
	/// @return Whether it is.
	[[nodiscard]] bool ready() const noexcept { return ready_; }
	/// @brief Whether rows were left out (more than mostEntries).
	/// @return Whether they were.
	[[nodiscard]] bool truncated() const noexcept { return truncated_; }
	/// @brief Whether changes are watched, for every folder.
	/// @return Whether they are.
	[[nodiscard]] bool watching() const noexcept;
	/// @brief Lists the folder again, in the background (a Reset follows).
	void rescan();

	/// @brief Whether an entry goes before another in the model's order.
	/// @param first One.
	/// @param second The other.
	/// @return Whether @p first goes first.
	[[nodiscard]] static bool before(const Entry& first, const Entry& second) noexcept;

private:
	struct Worker;
	/// @brief A path that changed, and what is there now.
	struct Update {
		std::string path;           ///< The path.
		std::optional<Entry> entry; ///< What is there, or std::nullopt if nothing is.
	};
	/// @brief Starts the worker and the watcher, and the first listing.
	void start();
	/// @brief Replaces every row (the first listing, a rescan).
	/// @param entries The rows, in order.
	/// @param truncated Whether some were left out.
	void reset(std::vector<Entry> entries, bool truncated);
	/// @brief Applies changes the watcher saw.
	/// @param updates The changes.
	void apply(std::vector<Update> updates);
	/// @brief Removes entries and what they hold, one range at a time from the last.
	/// @param paths Their paths.
	void removeAll(const std::vector<std::string>& paths);
	/// @brief Inserts entries not listed yet whose folders are, one run at a time from the last.
	/// @param entries The entries.
	void insertAll(std::vector<Entry> entries);
	/// @brief Lists a new folder's contents in the background, then inserts them.
	/// @param path The folder.
	void listLater(const std::string& path);

	Executor& executor_;             ///< See open().
	std::filesystem::path root_;     ///< See open().
	DirectoryOptions options_;       ///< See open().
	std::vector<Entry> entries_;     ///< The rows, in order.
	bool ready_ = false;             ///< See ready().
	bool truncated_ = false;         ///< See truncated().
	std::unique_ptr<Worker> worker_; ///< The thread and the watcher (last, so they stop first).
};

} // namespace affresco::services
