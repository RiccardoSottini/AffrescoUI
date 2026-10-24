/// @file
/// @brief The `fs` provider's operations on entries: mkdir, rename, delete, copy and stat.

#include "FileEntries.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <string>
#include <system_error>

namespace affresco::services {

namespace {

namespace fs = std::filesystem;

/// @brief A path as the document writes it: relative to the root, with "/" between names.
/// @param root The root (canonical).
/// @param path A path under it.
/// @return The relative path.
std::string relativeName(const fs::path& root, const fs::path& path) {
	const std::u8string name = path.lexically_relative(root).generic_u8string();
	return {name.begin(), name.end()};
}

/// @brief Whether a path is a folder's own path or under it.
/// @param folder The folder.
/// @param path The path.
/// @return Whether it is.
bool isWithin(const fs::path& folder, const fs::path& path) {
	const fs::path inside = path.lexically_relative(folder);
	return !inside.empty() && *inside.begin() != "..";
}

/// @brief Counts what an entry holds, itself included, without following links, and the bytes of its files.
/// @param from The entry.
/// @param limits Counting stops past these.
/// @param stop Says to give up.
/// @param bytes Receives the bytes counted.
/// @return The entries counted (past the limit, the limit plus one), or std::nullopt if they couldn't be listed.
std::optional<std::size_t> countEntries(const fs::path& from, const EntryLimits& limits, const std::stop_token& stop,
                                        std::uintmax_t& bytes) {
	std::error_code error;
	std::size_t count = 1;
	bytes = fs::is_regular_file(fs::symlink_status(from, error)) ? fs::file_size(from, error) : 0;
	if(!fs::is_directory(fs::symlink_status(from, error))) return count;
	fs::recursive_directory_iterator entry(from, fs::directory_options::none, error);
	for(; !error && entry != fs::recursive_directory_iterator(); entry.increment(error)) {
		if(stop.stop_requested() || ++count > limits.entries || bytes > limits.bytes) return count;
		if(entry->is_regular_file(error) && !entry->is_symlink(error)) bytes += entry->file_size(error);
	}
	return error ? std::nullopt : std::optional<std::size_t>(count);
}

/// @brief The first free name for a copy beside a taken path: "<name> copy<ext>", then "<name> copy 2<ext>" and on.
/// @param taken The taken path.
/// @param limits How many names to try.
/// @return The free path, or std::nullopt if every one tried is taken.
std::optional<fs::path> freeName(const fs::path& taken, const EntryLimits& limits) {
	std::error_code error;
	const bool folder = fs::is_directory(fs::symlink_status(taken, error));
	const fs::path stem = folder ? taken.filename() : taken.stem(), extension = folder ? fs::path() : taken.extension();
	for(int number = 1; number <= limits.names; ++number) {
		fs::path name = stem;
		name += number == 1 ? " copy" : " copy " + std::to_string(number);
		name += extension;
		const fs::path candidate = taken.parent_path() / name;
		if(!fs::exists(fs::symlink_status(candidate, error))) return candidate;
	}
	return std::nullopt;
}

} // namespace

std::optional<fs::path> entryUnderRoot(const fs::path& root, std::string_view relative) {
	fs::path given = fs::path(std::u8string(relative.begin(), relative.end())).lexically_normal();
	if(!given.empty() && !given.has_filename()) given = given.parent_path(); // "a/b/" is "a/b"
	const fs::path name = given.filename();
	if(name.empty() || name == "." || name == "..") return std::nullopt;
	const std::u8string folder = given.parent_path().generic_u8string();
	const std::optional<fs::path> parent =
	    underRoot(root, std::string_view(reinterpret_cast<const char*>(folder.data()), folder.size()));
	if(!parent) return std::nullopt;
	return *parent / name;
}

void makeFolder(const fs::path& root, const fs::path& path, Call& call) {
	std::error_code error;
	if(fs::exists(fs::symlink_status(path, error))) return call.fail("exists", "something is there already");
	fs::create_directories(path, error);
	if(error) return call.fail("failed", "the folder couldn't be made");
	call.finish({{"path", relativeName(root, path)}});
}

void moveEntry(const fs::path& root, const fs::path& from, const fs::path& to, Call& call) {
	std::error_code error;
	if(!fs::exists(fs::symlink_status(from, error))) return call.fail("notFound", "nothing is there");
	// A folder that ignores case finds the same entry when only the case changes: that rename is allowed
	if(fs::exists(fs::symlink_status(to, error)) && !fs::equivalent(from, to, error))
		return call.fail("exists", "the new path is taken");
	if(fs::is_directory(fs::symlink_status(from, error)) && isWithin(from, to) && from != to)
		return call.fail("failed", "a folder can't go into itself");
	if(!fs::is_directory(to.parent_path(), error)) return call.fail("notFound", "the new path's folder doesn't exist");
	fs::rename(from, to, error);
	if(error) return call.fail("failed", "it couldn't be moved");
	call.finish({{"path", relativeName(root, to)}});
}

void removeEntry(const fs::path& from, const EntryLimits& limits, const std::stop_token& stop, Call& call) {
	std::error_code error;
	if(!fs::exists(fs::symlink_status(from, error))) return call.fail("notFound", "nothing is there");
	std::uintmax_t bytes = 0;
	const std::optional<std::size_t> count =
	    countEntries(from, {limits.entries, std::numeric_limits<std::uintmax_t>::max(), 0}, stop, bytes);
	if(stop.stop_requested()) return;
	if(!count) return call.fail("failed", "the folder couldn't be listed");
	if(*count > limits.entries)
		return call.fail("tooLarge", "it holds over " + std::to_string(limits.entries) + " entries");
	const std::uintmax_t removed = fs::remove_all(from, error);
	if(error) return call.fail("failed", "it couldn't all be deleted");
	call.finish({{"removed", removed}});
}

bool matchesName(std::string_view glob, std::string_view name) noexcept {
	std::size_t at = 0, in = 0, star = std::string_view::npos, resume = 0;
	while(in < name.size()) {
		if(at < glob.size() && (glob[at] == '?' || glob[at] == name[in])) {
			++at;
			++in;
		} else if(at < glob.size() && glob[at] == '*') {
			star = at++;
			resume = in;
		} else if(star != std::string_view::npos) { // the last star takes one more character
			at = star + 1;
			in = ++resume;
		} else {
			return false;
		}
	}
	while(at < glob.size() && glob[at] == '*') ++at;
	return at == glob.size();
}

namespace {

/// @brief Whether a copy leaves an entry out.
/// @param options The copy's options.
/// @param path The entry.
/// @return Whether its name matches one of the excluded globs.
bool excluded(const CopyOptions& options, const fs::path& path) {
	const std::u8string name = path.filename().u8string();
	const std::string_view text(reinterpret_cast<const char*>(name.data()), name.size());
	return std::ranges::any_of(options.exclude, [text](const std::string& glob) { return matchesName(glob, text); });
}

/// @brief Copies one entry that isn't a folder: a link as a link, a file's bytes. What is at @p to already is an error,
/// unless merging: then a file there that is as new is kept, and anything else but a folder is replaced (a link
/// removed first, never written through). A plain copy (CopyOptions::plain) leaves a link out and gives a file only
/// read and write for its owner and read for the others.
/// @param from The entry.
/// @param to Its copy's path.
/// @param status The entry's own status (not following a link).
/// @param options The copy's options (merge, plain).
/// @param error Receives why it failed.
/// @return Whether it was copied (or kept, or left out).
bool copyOne(const fs::path& from, const fs::path& to, const fs::file_status& status, const CopyOptions& options,
             std::error_code& error) {
	if(options.plain && fs::is_symlink(status)) return true; // a link from another root could point anywhere
	const bool merge = options.merge;
	std::error_code ignored;
	const fs::file_status there = fs::symlink_status(to, ignored);
	if(fs::exists(there)) {
		if(!merge || fs::is_directory(there)) {
			error = std::make_error_code(std::errc::file_exists);
			return false;
		}
		if(fs::is_regular_file(there) && fs::is_regular_file(status) &&
		   fs::last_write_time(to, ignored) >= fs::last_write_time(from, ignored))
			return true; // as new as the original
		if(!fs::remove(to, error)) return false;
	}
	if(fs::is_symlink(status)) fs::copy_symlink(from, to, error);
	else fs::copy_file(from, to, error);
	if(!error && options.plain) // nothing copied from another root is runnable
		fs::permissions(to,
		                fs::perms::owner_read | fs::perms::owner_write | fs::perms::group_read | fs::perms::others_read,
		                fs::perm_options::replace, error);
	return !error;
}

/// @brief Copies a folder and what it holds, without following links, leaving out the excluded names.
/// @param from The folder.
/// @param to Its copy's path: nothing there, or a folder to merge into.
/// @param options The copy's options.
/// @param stop Says to give up.
/// @param error Receives why it failed.
/// @return Whether all of it was copied.
bool copyFolder(const fs::path& from, const fs::path& to, const CopyOptions& options, const std::stop_token& stop,
                std::error_code& error) {
	std::error_code ignored;
	if(!fs::is_directory(fs::symlink_status(to, ignored)) && !fs::create_directory(to, error)) return false;
	fs::recursive_directory_iterator entry(from, fs::directory_options::none, error);
	for(; !error && entry != fs::recursive_directory_iterator(); entry.increment(error)) {
		if(stop.stop_requested()) return false;
		const fs::file_status status = entry->symlink_status(error);
		if(error) return false;
		if(excluded(options, entry->path())) {
			if(fs::is_directory(status)) entry.disable_recursion_pending();
			continue;
		}
		const fs::path target = to / entry->path().lexically_relative(from);
		if(fs::is_directory(status)) {
			const fs::file_status there = fs::symlink_status(target, ignored);
			if(fs::is_directory(there)) continue; // merging into a folder there
			if(fs::exists(there) && (!options.merge || !fs::remove(target, error))) return false;
			if(!fs::create_directory(target, error)) return false;
		} else if(fs::is_regular_file(status) || fs::is_symlink(status)) {
			if(!copyOne(entry->path(), target, status, options, error)) return false;
		} // sockets, devices and pipes are left out
	}
	return !error;
}

} // namespace

void copyEntry(const fs::path& root, const fs::path& from, fs::path to, const CopyOptions& options,
               const EntryLimits& limits, const std::stop_token& stop, Call& call) {
	std::error_code error;
	const fs::file_status status = fs::symlink_status(from, error);
	if(!fs::exists(status)) return call.fail("notFound", "nothing is there");
	const bool folder = fs::is_directory(status);
	const fs::file_status there = fs::symlink_status(to, error);
	const bool merging = options.merge && fs::exists(there);
	if(merging && folder != fs::is_directory(there)) return call.fail("exists", "a folder and a file can't be merged");
	if(fs::exists(there) && !merging) {
		if(!options.free) return call.fail("exists", "the copy's path is taken");
		const std::optional<fs::path> name = freeName(to, limits);
		if(!name) return call.fail("exists", "no free name for the copy");
		to = *name;
	}
	if(folder && isWithin(from, to)) return call.fail("failed", "a folder can't be copied into itself");
	if(!fs::is_directory(to.parent_path(), error)) return call.fail("notFound", "the copy's folder doesn't exist");
	std::uintmax_t bytes = 0;
	const std::optional<std::size_t> count = countEntries(from, limits, stop, bytes);
	if(stop.stop_requested()) return;
	if(!count) return call.fail("failed", "the folder couldn't be listed");
	if(*count > limits.entries || bytes > limits.bytes)
		return call.fail("tooLarge", "it holds over " + std::to_string(limits.entries) + " entries or " +
		                                 std::to_string(limits.bytes >> 20) + " MiB");
	const bool copied = folder ? copyFolder(from, to, options, stop, error) : copyOne(from, to, status, options, error);
	if(!copied) {
		std::error_code ignored;
		if(!merging) fs::remove_all(to, ignored); // no half copy is left (a merge keeps what it brought up to date)
		if(stop.stop_requested()) return;
		return call.fail("failed", "it couldn't be copied");
	}
	call.finish({{"path", relativeName(root, to)}});
}

void describeEntry(const fs::path& path, Call& call) {
	std::error_code error;
	const fs::file_status status = fs::status(path, error);
	if(!fs::exists(status)) return call.finish({{"exists", false}});
	const bool file = fs::is_regular_file(status), folder = fs::is_directory(status);
	const fs::file_time_type written = fs::last_write_time(path, error);
	// The file clock's epoch differs from the system clock's on some systems: the two "now"s carry it across
	const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
	    (written - fs::file_time_type::clock::now()) + std::chrono::system_clock::now().time_since_epoch());
	call.finish({{"exists", true},
	             {"kind", file     ? "file"
	                      : folder ? "folder"
	                               : "other"},
	             {"size", file ? json(fs::file_size(path, error)) : json()},
	             {"modified", error ? json() : json(since.count())}});
}

} // namespace affresco::services
