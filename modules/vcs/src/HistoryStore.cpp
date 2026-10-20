/// @file
/// @brief The files of a history kept on disk: the lock, atomic writes and bounded reads.

#include "HistoryStore.hpp"

#include "Sha256.hpp"

#include "affresco/core/Portable.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <memory>
#include <system_error>
#include <utility>

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace affresco::vcs::detail {

namespace {

/// @brief Describes the last system error (errno).
/// @return e.g. "Permission denied".
std::string lastError() { return std::error_code(errno, std::generic_category()).message(); }

/// @brief Opens a file for writing, replacing what it has.
/// @param file The file.
/// @return It, or nullptr (errno says why).
std::FILE* openForWriting(const std::filesystem::path& file) {
#ifdef _WIN32
	std::FILE* opened = nullptr;
	return _wfopen_s(&opened, file.c_str(), L"wb") == 0 ? opened : nullptr;
#else
	// Its owner's only, whatever the process's umask; and never through a link left in its place
	const int descriptor = ::open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC | O_NOFOLLOW, 0600);
	if(descriptor < 0) return nullptr;
	std::FILE* opened = ::fdopen(descriptor, "wb");
	if(!opened) {
		const int error = errno;
		::close(descriptor);
		errno = error; // for lastError()
	}
	return opened;
#endif
}

/// @brief Why a history folder that is already there can't be used: one this user doesn't own, or a symbolic link,
/// would let whoever made it choose where the history goes, or read and change it.
/// @param directory The folder.
/// @return The problem, or "" if it is a folder of this user's (or isn't there yet).
std::string foreignFolder(const std::filesystem::path& directory) {
	std::error_code error;
	const std::filesystem::file_status status = std::filesystem::symlink_status(directory, error);
	if(error || status.type() == std::filesystem::file_type::not_found) return "";
	if(std::filesystem::is_symlink(status)) return "is a symbolic link, not a folder";
	if(!std::filesystem::is_directory(status)) return "is not a folder";
#ifndef _WIN32
	struct stat info{};
	if(::lstat(directory.c_str(), &info) != 0 || info.st_uid != ::geteuid()) return "belongs to another user";
#endif
	return "";
}

/// @brief Waits until what was written to a file is on the disk, not only in the system's cache.
///
/// On macOS fsync() only reaches the drive, whose cache may write in any order. A write that must come after all
/// the earlier ones (the refs, after the objects they name) gets a barrier (F_BARRIERFSYNC, measured about 0.4 ms)
/// instead: a power cut may then lose the latest changes, but never keeps refs that name a lost object.
/// F_FULLFSYNC would keep the latest change too, but it measured about 4 ms (up to 13 ms) per file, on the main
/// thread.
/// @param file The file, already flushed.
/// @param sync How it must reach the disk.
/// @return true if it did.
bool syncToDisk(std::FILE* file, [[maybe_unused]] HistoryStore::Sync sync) {
#ifdef _WIN32
	return _commit(_fileno(file)) == 0;
#elif defined(__APPLE__) // NOLINT(readability-use-concise-preprocessor-directives): #elifdef is C++23
	if(sync == HistoryStore::Sync::AfterEarlierWrites && ::fcntl(::fileno(file), F_BARRIERFSYNC) == 0) return true;
	return ::fsync(::fileno(file)) == 0; // also where the file system has no barriers
#else
	return ::fsync(::fileno(file)) == 0; // it returns once the data is on the disk, so later writes come after it
#endif
}

/// @brief Waits until a folder's entries (a file renamed into it, a folder made in it) are on the disk. POSIX keeps
/// them apart from the files' content, so without this a power cut could keep the refs and lose a commit they name.
/// @param directory The folder.
/// @return true if they are (always on Windows, where NTFS journals renames and there is no such call).
bool syncDirectory([[maybe_unused]] const std::filesystem::path& directory) {
#ifdef _WIN32
	return true;
#else
	const int descriptor = ::open(directory.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if(descriptor < 0) return false;
	const bool synced = ::fsync(descriptor) == 0;
	const int error = errno;
	::close(descriptor);
	errno = error; // for lastError()
	return synced;
#endif
}

/// @brief The current time, for a file name.
/// @return UTC, e.g. "20261019T120000Z" (no ':', which Windows doesn't allow in names).
std::string fileNameTime() {
	const std::tm utc = utcTime(std::time(nullptr));
	std::array<char, 32> stamp{};
	std::strftime(stamp.data(), stamp.size(), "%Y%m%dT%H%M%SZ", &utc);
	return stamp.data();
}

} // namespace

/* FileLock */

std::optional<FileLock> FileLock::tryLock(const std::filesystem::path& file, std::string& problem) {
#ifdef _WIN32
	// A file opened with no sharing is the lock: nobody else can open it until it is closed
	const HANDLE handle = CreateFileW(file.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS,
	                                  FILE_ATTRIBUTE_NORMAL, nullptr);
	if(handle == INVALID_HANDLE_VALUE) {
		const DWORD error = GetLastError();
		problem = error == ERROR_SHARING_VIOLATION ? "in use by another program"
		                                           : "can't be locked (error " + std::to_string(error) + ")";
		return std::nullopt;
	}
	return FileLock(reinterpret_cast<std::intptr_t>(handle));
#else
	// flock() locks belong to the open file, so a second open in the same program is refused too
	const int descriptor = ::open(file.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
	if(descriptor < 0) {
		problem = "can't be locked (" + file.string() + ": " + lastError() + ")";
		return std::nullopt;
	}
	if(::flock(descriptor, LOCK_EX | LOCK_NB) != 0) {
		if(errno == EWOULDBLOCK) {
			std::array<char, 24> holder{};
			const ssize_t length = ::pread(descriptor, holder.data(), holder.size() - 1, 0);
			const std::string process(holder.data(), length > 0 ? static_cast<std::size_t>(length) : 0);
			const bool known = !process.empty() &&
			                   std::ranges::all_of(process, [](char c) { return (c >= '0' && c <= '9') || c == '\n'; });
			problem = "in use by another program" +
			          (known ? " (process " + process.substr(0, process.find('\n')) + ")" : std::string());
		} else {
			problem = "can't be locked (" + lastError() + ")";
		}
		::close(descriptor);
		return std::nullopt;
	}
	FileLock lock(descriptor);
	// Only for the message another program shows: a failure to write it changes nothing
	const std::string process = std::to_string(::getpid()) + "\n";
	[[maybe_unused]] const bool noted =
	    ::ftruncate(descriptor, 0) == 0 && ::pwrite(descriptor, process.data(), process.size(), 0) > 0;
	return lock;
#endif
}

FileLock::FileLock(FileLock&& other) noexcept : handle_(std::exchange(other.handle_, -1)) {}

FileLock& FileLock::operator=(FileLock&& other) noexcept {
	if(this != &other) {
		release();
		handle_ = std::exchange(other.handle_, -1);
	}
	return *this;
}

FileLock::~FileLock() { release(); }

void FileLock::release() noexcept {
	if(handle_ == -1) return;
#ifdef _WIN32
	CloseHandle(reinterpret_cast<HANDLE>(handle_));
#else
	::close(static_cast<int>(handle_)); // closing the last descriptor releases the flock()
#endif
	handle_ = -1;
}

/* HistoryStore */

HistoryStore::HistoryStore(std::filesystem::path directory) : directory_(std::move(directory)) {
	if(const std::string foreign = foreignFolder(directory_); !foreign.empty()) {
		problem_ = foreign; // neither read nor written: the history stays in memory
		usable_ = false;
		return;
	}
	std::error_code error;
	const bool created = std::filesystem::create_directories(directory_ / "objects", error);
	if(error) {
		problem_ = "can't be created (" + error.message() + ")";
		return;
	}
	if(created) syncDirectory(directory_.parent_path()); // at worst a power cut loses a new, empty history
	// Only its owner may read it. A folder someone else made keeps its permissions, and then the lock tells.
	std::filesystem::permissions(directory_, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace,
	                             error);
	lock_ = FileLock::tryLock(directory_ / "lock", problem_);
}

bool HistoryStore::exists() const {
	std::error_code error;
	return usable_ && std::filesystem::is_regular_file(refsFile(), error);
}

void HistoryStore::writeRefs(const std::string& text) {
	if(text == refs_) return;
	write(refsFile(), text, Sync::AfterEarlierWrites);
	refs_ = text;
}

std::filesystem::path HistoryStore::objectFile(std::string_view id) const {
	if(!isObjectId(id)) throw HistoryError(directory_, "'" + std::string(id.substr(0, 80)) + "' is not an object id");
	return directory_ / "objects" / std::string(id.substr(0, 2)) / std::string(id.substr(2));
}

std::string HistoryStore::readObject(const std::string& id) {
	if(const auto inPack = packed_.find(id); inPack != packed_.end()) {
		std::string text = std::move(inPack->second);
		packed_.erase(inPack);
		objects_.insert(id);
		return text;
	}
	std::string text = read(objectFile(id));
	if(objects_.insert(id).second) loose_.emplace_back(id, std::nullopt);
	return text;
}

void HistoryStore::writeObject(const std::string& id, std::string_view text) {
	const std::filesystem::path file = objectFile(id);
	createFolder(file.parent_path());
	write(file, text, Sync::ThisFile);
	if(objects_.insert(id).second) loose_.emplace_back(id, std::string(text));
}

/* Packs */

std::filesystem::path HistoryStore::packFile(std::string_view name) const {
	if(!isObjectId(name))
		throw HistoryError(directory_, "'" + std::string(name.substr(0, 80)) + "' is not a pack's name");
	return directory_ / "packs" / std::string(name);
}

void HistoryStore::readPacks(const std::vector<std::string>& names) {
	for(const std::string& name : names) {
		const std::filesystem::path file = packFile(name);
		const std::string text = read(file);
		if(sha256(text) != name) throw HistoryError(file, "its content doesn't match its SHA-256: the file is damaged");
		std::size_t line = 1;
		for(std::size_t start = 0; start < text.size(); ++line) {
			std::size_t end = text.find('\n', start);
			if(end == std::string::npos) end = text.size();
			const std::string_view entry = std::string_view(text).substr(start, end - start);
			if(entry.size() < 66 || entry[64] != ' ' || !isObjectId(entry.substr(0, 64)))
				throw HistoryError(file, "line " + std::to_string(line) + " is not \"<id> <object>\"");
			packed_.try_emplace(std::string(entry.substr(0, 64)), entry.substr(65));
			start = end + 1;
		}
	}
	packs_ = names;
}

void HistoryStore::pack() {
	std::vector<std::string> written;
	std::string text;
	const auto flush = [&] {
		if(text.empty()) return;
		const std::string name = sha256(text);
		createFolder(directory_ / "packs");
		write(packFile(name), text, Sync::ThisFile);
		written.push_back(name);
		text.clear();
	};
	for(const auto& [id, loose] : loose_) {
		const std::string object = loose ? *loose : read(objectFile(id)); // copied: a failure keeps them loose
		if(!text.empty() && text.size() + object.size() + 66 > maxPackSize) flush();
		text.append(id).append(" ").append(object).append("\n");
	}
	flush();
	packs_.insert(packs_.end(), written.begin(), written.end());
	for(auto& [id, loose] : loose_) packedLoose_.push_back(std::move(id));
	loose_.clear();
}

void HistoryStore::removeLoose() {
	for(const std::string& id : packedLoose_) {
		std::error_code ignored;
		std::filesystem::remove(directory_ / "objects" / id.substr(0, 2) / id.substr(2), ignored);
	}
	packedLoose_.clear();
}

void HistoryStore::createFolder(const std::filesystem::path& folder) {
	std::error_code error;
	const bool created = std::filesystem::create_directories(folder, error);
	if(error) throw HistoryError(folder, "can't be created (" + error.message() + ")");
	if(created && !syncDirectory(folder.parent_path()))
		throw HistoryError(folder.parent_path(), "can't be synced (" + lastError() + ")");
}

std::filesystem::path HistoryStore::moveAside() {
	const std::string stamp = fileNameTime();
	std::filesystem::path aside = directory_;
	aside += ".replaced-" + stamp;
	std::error_code error;
	for(int n = 2; std::filesystem::exists(aside, error); ++n) {
		aside = directory_;
		aside += ".replaced-" + stamp + "-" + std::to_string(n);
	}
#ifdef _WIN32
	lock_.reset();
#endif
	std::filesystem::rename(directory_, aside, error);
	if(error)
		throw HistoryError(directory_, "can't be moved aside to " + aside.string() + " (" + error.message() + ")");
	lock_.reset();
	return aside;
}

std::string HistoryStore::read(const std::filesystem::path& file) {
	std::error_code error;
	const std::uintmax_t size = std::filesystem::file_size(file, error);
	if(error) throw HistoryError(file, "can't be read (" + error.message() + ")");
	if(size > maxFileSize) throw HistoryError(file, "is larger than 64 MiB");

	std::ifstream in(file, std::ios::binary);
	std::string text(static_cast<std::size_t>(size), '\0');
	if(!in.read(text.data(), static_cast<std::streamsize>(text.size()))) throw HistoryError(file, "can't be read");
	return text;
}

void HistoryStore::write(const std::filesystem::path& file, std::string_view text, Sync sync) {
	// Never write what read() would refuse: the history on disk would be damaged for good
	if(text.size() > maxFileSize)
		throw HistoryError(file, "would be larger than 64 MiB, more than a history reads back, so it isn't written");

	std::filesystem::path temporary = file;
	temporary += ".tmp"; // only the lock holder writes, so one name is enough

	std::unique_ptr<std::FILE, int (*)(std::FILE*)> out(openForWriting(temporary), &std::fclose);
	if(!out) throw HistoryError(temporary, "can't be written (" + lastError() + ")");
	const bool written = std::fwrite(text.data(), 1, text.size(), out.get()) == text.size() &&
	                     std::fflush(out.get()) == 0 && syncToDisk(out.get(), sync);
	const std::string why = written ? std::string() : lastError();
	if(std::fclose(out.release()) != 0 || !written) {
		std::error_code ignored;
		std::filesystem::remove(temporary, ignored); // a partial file is never renamed into place
		throw HistoryError(temporary, "can't be written (" + (written ? lastError() : why) + ")");
	}

	std::error_code error;
	std::filesystem::rename(temporary, file, error); // atomic: readers see the old file or the new one
	if(error) throw HistoryError(file, "can't be replaced (" + error.message() + ")");
	// The rename itself must be on the disk before the next file (the refs after their commits) is written
	if(!syncDirectory(file.parent_path()))
		throw HistoryError(file.parent_path(), "can't be synced (" + lastError() + ")");
}

bool isObjectId(std::string_view text) noexcept {
	return text.size() == 64 &&
	       std::ranges::all_of(text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

} // namespace affresco::vcs::detail
