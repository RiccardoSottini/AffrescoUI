#pragma once

/// @file
/// @brief The files of a history kept on disk: its folder, its lock, its commit objects and its refs.

#include "affresco/core/Json.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

	/// @brief Takes over a lock.
	/// @param other The lock; it holds none after.
	FileLock(FileLock&& other) noexcept;
	/// @brief Releases this lock, and takes over another.
	/// @param other The lock; it holds none after.
	/// @return This.
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
///       refs.json       main, the open proposals, the grants, the mode, and the packs
///       objects/ab/cd…  commits and documents, each named by its SHA-256 (loose objects)
///       packs/ef01…     many objects in one file, named by the SHA-256 of its text
///
/// Knows nothing about what an object holds: it reads and writes text. Every write is atomic (a temporary file, synced,
/// then renamed over the old one), so a crash leaves either the old file or the new one. Only the program that holds
/// the lock writes; any other one only reads. Used on the main thread, like the Repository that owns it.
///
/// Objects are written loose, one file each, and once packObjects of them are loose they go into a pack, as Git packs
/// them: reading a history then opens a few files instead of one per commit (opening a file costs far more than
/// reading a small one). A pack holds one line per object, `<id> <text>`.
class HistoryStore {
public:
	/// Largest file it reads: far beyond a real screen's commit, small enough to read whole.
	static constexpr std::uintmax_t maxFileSize = std::uintmax_t{64} * 1024 * 1024;
	/// How many loose objects are packed together.
	static constexpr std::size_t packObjects = 256;
	/// Largest pack it writes: more objects go into another one.
	static constexpr std::size_t maxPackSize = std::size_t{16} * 1024 * 1024;

	/// @brief How a write waits for the disk.
	enum class Sync {
		ThisFile,          ///< Until the file is on the disk (an object).
		AfterEarlierWrites ///< And never before any earlier write (the refs, after the objects they name).
	};

	/// @brief Opens a history folder, creating it if needed, and takes its lock if it can. A folder that is a
	/// symbolic link, or (on POSIX) belongs to another user, is neither read nor written.
	/// @param directory The folder.
	explicit HistoryStore(std::filesystem::path directory);

	/// @brief Tells whether this program may write the history (it holds the lock).
	[[nodiscard]] bool writable() const noexcept { return lock_.has_value(); }
	/// @brief Why this program may not write the history: in use by another program, not creatable, a symbolic link
	/// or another user's.
	[[nodiscard]] const std::string& problem() const noexcept { return problem_; }
	/// @brief The folder.
	[[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }

	/// @brief The refs file.
	[[nodiscard]] std::filesystem::path refsFile() const { return directory_ / "refs.json"; }
	/// @brief Tells whether a history was written here (it has a refs file), in a folder that may be read.
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
	/// @return In objects/, a folder named by its first two digits, holding a file named by the other 62.
	[[nodiscard]] std::filesystem::path objectFile(std::string_view id) const;
	/// @brief Tells whether an object is known to be on disk (written or read by this store).
	/// @param id The object's id.
	/// @return true if it is.
	[[nodiscard]] bool hasObject(const std::string& id) const { return objects_.contains(id); }
	/// @brief Reads an object, from a pack read before (readPacks()) or its own file, and remembers that it is on disk.
	/// Each object is read once: a packed one is handed over.
	/// @param id The object's id.
	/// @return Its text.
	/// @throws HistoryError If it can't be read.
	[[nodiscard]] std::string readObject(const std::string& id);
	/// @brief Writes an object as a loose file.
	/// @param id The object's id.
	/// @param text Its text.
	/// @throws HistoryError If it can't be written.
	void writeObject(const std::string& id, std::string_view text);

	/// @brief The file of a pack.
	/// @param name The pack's name: the SHA-256 of its text.
	/// @return The file named @p name in packs/.
	[[nodiscard]] std::filesystem::path packFile(std::string_view name) const;
	/// @brief The packs this history has, oldest first: what the refs name.
	[[nodiscard]] const std::vector<std::string>& packs() const noexcept { return packs_; }
	/// @brief Reads packs, checking each against its name, so that readObject() finds their objects without opening a
	/// file each.
	/// @param names The packs the refs name.
	/// @throws HistoryError If one is missing, damaged or malformed.
	void readPacks(const std::vector<std::string>& names);
	/// @brief Forgets the packed objects no read asked for (commits no ref reaches).
	void forgetUnread() noexcept { packed_.clear(); }
	/// @brief Tells whether enough objects are loose to pack them.
	[[nodiscard]] bool packDue() const noexcept { return loose_.size() >= packObjects; }
	/// @brief Writes the loose objects into new packs (each at most maxPackSize). The refs must name them before
	/// removeLoose() deletes the loose files.
	/// @throws HistoryError If a pack can't be written; then the objects stay loose.
	void pack();
	/// @brief Deletes the loose files of the objects pack() packed, once the refs that name the packs are on disk. A
	/// file that can't be deleted stays, and is never read again.
	void removeLoose();

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
	/// @param sync How it must reach the disk.
	/// @throws HistoryError If it can't be written, or is larger than maxFileSize (read() would refuse it).
	static void write(const std::filesystem::path& file, std::string_view text, Sync sync);
	/// @brief Creates a folder if needed, and syncs the folder it is in when it was created.
	/// @param folder The folder.
	/// @throws HistoryError If it can't be created or synced.
	static void createFolder(const std::filesystem::path& folder);

	std::filesystem::path directory_;                     ///< The folder.
	std::optional<FileLock> lock_;                        ///< Held while this program writes the history.
	std::string problem_;                                 ///< Why there is no lock.
	bool usable_ = true;                                  ///< false for a symbolic link or another user's folder.
	std::unordered_set<std::string> objects_;             ///< Objects known to be on disk.
	std::string refs_;                                    ///< What the refs file was last written or read with.
	std::vector<std::string> packs_;                      ///< The packs, oldest first.
	std::unordered_map<std::string, std::string> packed_; ///< Objects of the packs read, by id, until read.
	/// Loose objects not yet packed: id, and the text of one written here (one read is read again to pack it).
	std::vector<std::pair<std::string, std::optional<std::string>>> loose_;
	std::vector<std::string> packedLoose_; ///< Loose objects packed, whose files removeLoose() deletes.
};

/// @brief Tells whether a text is a whole object id (a commit's, or a document's).
/// @param text The text.
/// @return true for 64 lowercase hex digits.
[[nodiscard]] bool isObjectId(std::string_view text) noexcept;

} // namespace affresco::vcs::detail
