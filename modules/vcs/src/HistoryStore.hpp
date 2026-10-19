#pragma once

/// @file
/// @brief The files of a history kept on disk: its folder, its lock, its commit objects and its refs.

#include "affresco/core/Json.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

namespace affresco::vcs::detail {

/// @brief A problem with a history file; what() names the file: "AffrescoUI history: <file>: <problem>".
class HistoryError : public DocumentError {
public:
	/// @brief Describes a problem with one file.
	/// @param file The file.
	/// @param problem What is wrong with it.
	HistoryError(const std::filesystem::path& file, const std::string& problem)
	    : DocumentError("AffrescoUI history: " + file.string() + ": " + problem) {}
};

/// @brief An exclusive lock on a file, held until it is destroyed. The system releases it if the program ends, so a
/// crash never leaves a history locked.
class FileLock {
public:
	/// @brief Takes the lock, creating the file, and writes this program's process id into it.
	/// @param file The lock file.
	/// @param problem Receives why there is no lock: another program holds it, or the file can't be opened.
	/// @return The lock, or std::nullopt.
	[[nodiscard]] static std::optional<FileLock> tryLock(const std::filesystem::path& file, std::string& problem);

	FileLock(FileLock&& other) noexcept;
	FileLock& operator=(FileLock&& other) noexcept;
	FileLock(const FileLock&) = delete;
	FileLock& operator=(const FileLock&) = delete;
	/// @brief Releases the lock.
	~FileLock();

private:
	/// @brief Takes ownership of a locked file.
	/// @param handle The file descriptor (POSIX) or HANDLE (Windows).
	explicit FileLock(std::intptr_t handle) noexcept : handle_(handle) {}
	/// @brief Releases the lock and closes the file, if there is one.
	void release() noexcept;

	std::intptr_t handle_ = -1; ///< The locked file: a descriptor, or a HANDLE on Windows; -1 for none.
};

/// @brief The folder a history is kept in, laid out like a Git repository:
///
///     <name>.history/
///       lock            held by the program that writes the history
///       refs.json       main, the open proposals, the grants and the mode
///       objects/ab/cd…  commits and documents, each named by its SHA-256
///
/// Knows nothing about what an object holds: it reads and writes text. Every write is atomic (a temporary file, synced,
/// then renamed over the old one), so a crash leaves either the old file or the new one. Only the program that holds
/// the lock writes; any other one only reads. Used on the main thread, like the Repository that owns it.
class HistoryStore {
public:
	/// Largest file it reads: far beyond a real screen's commit, small enough to read whole.
	static constexpr std::uintmax_t maxFileSize = 64u * 1024u * 1024u;

	/// @brief Opens a history folder, creating it if needed, and takes its lock if it can.
	/// @param directory The folder.
	explicit HistoryStore(std::filesystem::path directory);

	/// @brief Tells whether this program may write the history (it holds the lock).
	[[nodiscard]] bool writable() const noexcept { return lock_.has_value(); }
	/// @brief Why this program may not write the history: in use by another program, or not creatable.
	[[nodiscard]] const std::string& problem() const noexcept { return problem_; }
	/// @brief The folder.
	[[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

	/// @brief The refs file.
	[[nodiscard]] std::filesystem::path refsFile() const { return directory_ / "refs.json"; }
	/// @brief Tells whether a history was written here (it has a refs file).
	[[nodiscard]] bool exists() const;
	/// @brief Reads the refs file.
	/// @return Its text.
	/// @throws HistoryError If it can't be read.
	[[nodiscard]] std::string readRefs() { return refs_ = read(refsFile()); }
	/// @brief Replaces the refs file, unless it already has this text.
	/// @param text The new text.
	/// @throws HistoryError If it can't be written.
	void writeRefs(const std::string& text);

	/// @brief The file of an object: a commit, or a document that commits name.
	/// @param id The object's id: 64 lowercase hex digits (see isObjectId()).
	/// @return objects/<first two digits>/<the other 62>.
	[[nodiscard]] std::filesystem::path objectFile(std::string_view id) const;
	/// @brief Tells whether an object is known to be on disk (written or read by this store).
	/// @param id The object's id.
	/// @return true if it is.
	[[nodiscard]] bool hasObject(const std::string& id) const { return objects_.contains(id); }
	/// @brief Reads an object, and remembers that it is on disk.
	/// @param id The object's id.
	/// @return Its text.
	/// @throws HistoryError If it can't be read.
	[[nodiscard]] std::string readObject(const std::string& id);
	/// @brief Writes an object.
	/// @param id The object's id.
	/// @param text Its text.
	/// @throws HistoryError If it can't be written.
	void writeObject(const std::string& id, std::string_view text);

	/// @brief Moves the folder aside, so that a new history can start in its place, then gives up its lock: this store
	/// is done. On POSIX the lock is held while it moves, so no other program can start writing the old folder in
	/// between. Windows can't move a folder with an open file in it, so there the lock goes first.
	/// @return Where it is now: "kanban.history.replaced-20261019T120000Z" (with "-2", "-3"… if that exists).
	/// @throws HistoryError If it can't be moved; then it stays where it was (and, on POSIX, locked).
	[[nodiscard]] std::filesystem::path moveAside();

private:
	/// @brief Reads a whole file, up to maxFileSize.
	/// @param file The file.
	/// @return Its bytes.
	/// @throws HistoryError If it can't be read or is too large.
	[[nodiscard]] static std::string read(const std::filesystem::path& file);
	/// @brief Replaces a file atomically: writes a temporary file next to it, syncs it to disk, renames it, then syncs
	/// the folder so that the rename is on the disk too.
	/// @param file The file.
	/// @param text Its new content.
	/// @throws HistoryError If it can't be written.
	static void write(const std::filesystem::path& file, std::string_view text);

	std::filesystem::path directory_;         ///< The folder.
	std::optional<FileLock> lock_;            ///< Held while this program writes the history.
	std::string problem_;                     ///< Why there is no lock.
	std::unordered_set<std::string> objects_; ///< Objects known to be on disk.
	std::string refs_;                        ///< What the refs file was last written or read with.
};

/// @brief Tells whether a text is a whole object id (a commit's, or a document's).
/// @param text The text.
/// @return true for 64 lowercase hex digits.
[[nodiscard]] bool isObjectId(std::string_view text) noexcept;

} // namespace affresco::vcs::detail
