/// @file
/// @brief The files of a history kept on disk: the lock, atomic writes and bounded reads.

#include "HistoryStore.hpp"

#include <algorithm>
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
	return std::fopen(file.c_str(), "wb");
#endif
}

/// @brief Waits until what was written to a file is on the disk, not only in the system's cache.
/// @param file The file, already flushed.
/// @return true if it is.
bool syncToDisk(std::FILE* file) {
#ifdef _WIN32
	return _commit(_fileno(file)) == 0;
#else
	return ::fsync(::fileno(file)) == 0;
#endif
}

/// @brief The current time, for a file name.
/// @return UTC, e.g. "20261019T120000Z" (no ':', which Windows doesn't allow in names).
std::string fileNameTime() {
	const std::time_t now = std::time(nullptr);
	std::tm utc{};
#ifdef _WIN32
	gmtime_s(&utc, &now);
#else
	gmtime_r(&now, &utc);
#endif
	char stamp[32];
	std::strftime(stamp, sizeof(stamp), "%Y%m%dT%H%M%SZ", &utc);
	return stamp;
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
			char holder[24] = {};
			const ssize_t length = ::pread(descriptor, holder, sizeof(holder) - 1, 0);
			const std::string process(holder, length > 0 ? static_cast<std::size_t>(length) : 0);
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
	std::error_code error;
	std::filesystem::create_directories(directory_ / "objects", error);
	if(error) {
		problem_ = "can't be created (" + error.message() + ")";
		return;
	}
	// Only its owner may read it. A folder someone else made keeps its permissions, and then the lock tells.
	std::filesystem::permissions(directory_, std::filesystem::perms::owner_all, std::filesystem::perm_options::replace,
	                             error);
	lock_ = FileLock::tryLock(directory_ / "lock", problem_);
}

bool HistoryStore::exists() const {
	std::error_code error;
	return std::filesystem::is_regular_file(refsFile(), error);
}

void HistoryStore::writeRefs(const std::string& text) {
	if(text == refs_) return;
	write(refsFile(), text);
	refs_ = text;
}

std::filesystem::path HistoryStore::objectFile(std::string_view id) const {
	if(!isCommitId(id)) throw HistoryError(directory_, "'" + std::string(id.substr(0, 80)) + "' is not a commit id");
	return directory_ / "objects" / std::string(id.substr(0, 2)) / std::string(id.substr(2));
}

std::string HistoryStore::readObject(const std::string& id) {
	std::string text = read(objectFile(id));
	objects_.insert(id);
	return text;
}

void HistoryStore::writeObject(const std::string& id, std::string_view text) {
	const std::filesystem::path file = objectFile(id);
	std::error_code error;
	std::filesystem::create_directories(file.parent_path(), error);
	if(error) throw HistoryError(file.parent_path(), "can't be created (" + error.message() + ")");
	write(file, text);
	objects_.insert(id);
}

std::filesystem::path HistoryStore::setAside(const std::filesystem::path& directory) {
	const std::string stamp = fileNameTime();
	std::filesystem::path aside = directory;
	aside += ".replaced-" + stamp;
	std::error_code error;
	for(int n = 2; std::filesystem::exists(aside, error); ++n) {
		aside = directory;
		aside += ".replaced-" + stamp + "-" + std::to_string(n);
	}
	std::filesystem::rename(directory, aside, error);
	if(error) throw HistoryError(directory, "can't be moved aside to " + aside.string() + " (" + error.message() + ")");
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

void HistoryStore::write(const std::filesystem::path& file, std::string_view text) {
	std::filesystem::path temporary = file;
	temporary += ".tmp"; // only the lock holder writes, so one name is enough

	std::unique_ptr<std::FILE, int (*)(std::FILE*)> out(openForWriting(temporary), &std::fclose);
	if(!out) throw HistoryError(temporary, "can't be written (" + lastError() + ")");
	const bool written = std::fwrite(text.data(), 1, text.size(), out.get()) == text.size() &&
	                     std::fflush(out.get()) == 0 && syncToDisk(out.get());
	const std::string why = written ? std::string() : lastError();
	if(std::fclose(out.release()) != 0 || !written) {
		std::error_code ignored;
		std::filesystem::remove(temporary, ignored); // a partial file is never renamed into place
		throw HistoryError(temporary, "can't be written (" + (written ? lastError() : why) + ")");
	}

	std::error_code error;
	std::filesystem::rename(temporary, file, error); // atomic: readers see the old file or the new one
	if(error) throw HistoryError(file, "can't be replaced (" + error.message() + ")");
}

bool isCommitId(std::string_view text) noexcept {
	return text.size() == 64 &&
	       std::ranges::all_of(text, [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); });
}

} // namespace affresco::vcs::detail
