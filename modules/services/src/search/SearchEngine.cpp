/// @file
/// @brief The search engine (affresco/services/Search.hpp): one thread walks the folder, then several match its files.

#include "affresco/services/Search.hpp"

#include "Providers.hpp"
#include "search/SearchGlob.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <mutex>
#include <system_error>
#include <thread>
#include <utility>

namespace affresco::services {

namespace fs = std::filesystem;

namespace {

constexpr std::size_t mostDepth = 64;                     ///< Folders deeper than this aren't walked.
constexpr std::size_t previewBytes = 240;                 ///< A match's preview, at most.
constexpr std::size_t previewBefore = 40;                 ///< Bytes kept before a match in a long line's preview.
constexpr std::size_t binaryProbe = 8192;                 ///< The bytes looked at for a NUL.
constexpr std::size_t mostOutput = std::size_t{64} << 20; ///< A replaced text, at most 64 MiB.

/// @brief A file to match.
struct Candidate {
	fs::path path;        ///< Where it is.
	std::string relative; ///< Relative to the root, with '/'.
};

/// @brief What decides whether a walked path is searched.
struct Filters {
	GlobList include;      ///< See SearchQuery::include.
	GlobList exclude;      ///< See SearchQuery::exclude.
	IgnoreRules ignore;    ///< The .gitignore rules met so far.
	bool gitignore = true; ///< Whether they count.
};

/// @brief What the matching threads share.
struct Shared {
	const TextPattern& pattern;                      ///< What they look for.
	const SearchQuery& query;                        ///< The limits.
	const std::vector<Candidate>& files;             ///< What they match.
	const std::function<void(SearchFile&&)>& onFile; ///< Where results go, under mutex.
	SearchSummary& summary;                          ///< Its files and matches, under mutex.
	std::stop_source stop{};                         ///< Stops them all.
	std::mutex mutex{};                              ///< Guards onFile and summary.
	std::atomic<std::size_t> next{0};                ///< The next file to take.
	std::atomic<std::size_t> taken{0};               ///< Matches taken towards maxResults.
	std::atomic<std::size_t> searched{0};            ///< See SearchSummary.
	std::atomic<std::size_t> skipped{0};             ///< See SearchSummary.
	std::atomic<bool> truncated{false};              ///< maxResults was reached.
};

/// @brief Whether a byte continues a code point.
/// @param byte The byte.
/// @return Whether it is 10xxxxxx.
bool continuation(char byte) noexcept { return (static_cast<unsigned char>(byte) & 0xC0) == 0x80; }

/// @brief The byte after the code point at a byte.
/// @param text The text.
/// @param at The byte.
/// @return The next code point's first byte, or past the end.
std::size_t nextCodePoint(std::string_view text, std::size_t at) noexcept {
	++at;
	while(at < text.size() && continuation(text[at])) ++at;
	return at;
}

/// @brief A relative path one name deeper.
/// @param base The folder ("" for the root).
/// @param name The name.
/// @return The path.
std::string joined(const std::string& base, std::string_view name) {
	return base.empty() ? std::string(name) : base + '/' + std::string(name);
}

/// @brief Reads a folder's .gitignore, if it has one and it is a plain file: a FIFO would block the walk, and a link
/// may point out of the root, which the walk never leaves.
/// @param folder The folder.
/// @param relative It, relative to the root.
/// @param rules Gets its rules.
void readIgnore(const fs::path& folder, const std::string& relative, IgnoreRules& rules) {
	std::error_code error;
	if(!fs::is_regular_file(fs::symlink_status(folder / ".gitignore", error))) return;
	std::ifstream in(folder / ".gitignore", std::ios::binary);
	if(!in) return;
	std::string text(IgnoreRules::maxFileBytes, '\0');
	in.read(text.data(), static_cast<std::streamsize>(text.size()));
	text.resize(static_cast<std::size_t>(in.gcount()));
	rules.add(relative, text);
}

/// @brief Whether a walked path is left out by "exclude" or a .gitignore.
/// @param filters The filters.
/// @param relative The path, relative to the root.
/// @param folder Whether it is a folder.
/// @return Whether it is.
bool excluded(const Filters& filters, std::string_view relative, bool folder) {
	const std::vector<std::string_view> names = splitPath(relative);
	return filters.exclude.matches(names, folder) || (filters.gitignore && filters.ignore.ignored(names, folder));
}

/// @brief A folder waiting to be walked.
struct PendingFolder {
	fs::path path;         ///< The folder.
	std::string relative;  ///< It, relative to the root.
	std::size_t depth = 0; ///< How deep it is below the start.
};

/// @brief One walk of the folders: what decides which files it takes, and where they go.
struct Walk {
	Filters& filters;                     ///< The filters; the walked folders' .gitignore rules are added.
	const SearchQuery& query;             ///< The limits.
	std::vector<Candidate>& files;        ///< Gets the files to match.
	std::vector<PendingFolder> pending{}; ///< The folders still to walk.
};

/// @brief Takes a walked file if "include" lets it in.
/// @param path The file.
/// @param relative It, relative to the root.
/// @param walk The walk.
/// @return False if maxFiles is reached, so the walk stops.
bool takeFile(const fs::path& path, std::string relative, Walk& walk) {
	if(!walk.filters.include.empty() && !walk.filters.include.matches(splitPath(relative), false)) return true;
	if(walk.files.size() == walk.query.maxFiles) return false;
	walk.files.push_back({path, std::move(relative)});
	return true;
}

/// @brief Takes one entry of a walked folder: a folder to walk later, or a file to match. Links, special files and
/// names that aren't UTF-8 are left out.
/// @param entry The entry.
/// @param folder The folder it is in.
/// @param walk The walk.
/// @return False if maxFiles is reached, so the walk stops.
bool take(const fs::directory_entry& entry, const PendingFolder& folder, Walk& walk) {
	std::error_code status;
	const std::string name = entry.path().filename().string();
	const fs::file_status kind = entry.symlink_status(status);
	if(name == ".git" || !isUtf8(name) || status || fs::is_symlink(kind)) return true;
	const bool isFolder = fs::is_directory(kind);
	std::string path = joined(folder.relative, name);
	if((!isFolder && !fs::is_regular_file(kind)) || excluded(walk.filters, path, isFolder)) return true;
	if(!isFolder) return takeFile(entry.path(), std::move(path), walk);
	if(folder.depth + 1 < mostDepth) walk.pending.push_back({entry.path(), std::move(path), folder.depth + 1});
	return true;
}

/// @brief Walks a folder, depth first without recursion, gathering the files to match.
/// @param start The folder, relative to the root too.
/// @param walk The walk.
/// @param stop Stops the walk.
/// @return False if maxFiles was reached.
bool collect(PendingFolder start, Walk& walk, const std::stop_token& stop) {
	walk.pending.push_back(std::move(start));
	while(!walk.pending.empty() && !stop.stop_requested()) {
		const PendingFolder folder = std::move(walk.pending.back());
		walk.pending.pop_back();
		if(walk.filters.gitignore) readIgnore(folder.path, folder.relative, walk.filters.ignore);
		std::error_code error;
		for(fs::directory_iterator entry(folder.path, error), end; !error && entry != end; entry.increment(error))
			if(!take(*entry, folder, walk)) return false;
	}
	return true;
}

/// @brief Reads a file to match.
/// @param path The file.
/// @param maxBytes The largest it may be.
/// @param text Gets its bytes.
/// @return Whether it was read and isn't binary.
bool readText(const fs::path& path, std::uintmax_t maxBytes, std::string& text) {
	std::error_code error;
	const std::uintmax_t size = fs::file_size(path, error);
	if(error || size > maxBytes) return false;
	std::ifstream in(path, std::ios::binary);
	text.resize(static_cast<std::size_t>(size));
	if(!in.read(text.data(), static_cast<std::streamsize>(size)) && !in.eof()) return false;
	text.resize(static_cast<std::size_t>(in.gcount()));
	return text.find('\0') >= binaryProbe;
}

/// @brief Fills a match's preview: its line, or the part around it in a long line.
/// @param line The line, without its '\n'.
/// @param column The match's first byte in it.
/// @param match Gets text and at; its length is cut to the preview.
void preview(std::string_view line, std::size_t column, SearchMatch& match) {
	if(line.ends_with('\r')) line.remove_suffix(1);
	std::size_t start = line.size() > previewBytes && column > previewBefore ? column - previewBefore : 0;
	while(start > 0 && continuation(line[start])) --start;
	std::size_t end = std::min(line.size(), start + previewBytes);
	while(end < line.size() && end > start && continuation(line[end])) --end;
	const std::string_view shown = line.substr(start, end - start);
	if(!isUtf8(shown)) {
		match.text = "(not UTF-8 text)";
		match.at = match.length = 0;
		return;
	}
	match.text = shown;
	match.at = std::min(column - start, shown.size());
	match.length = std::min(match.length, shown.size() - match.at);
}

/// @brief Matches one file's text, empty matches left out.
/// @param text The text.
/// @param shared The threads' shared state; reaching maxResults stops them.
/// @param file Gets the matches.
void matchText(std::string_view text, Shared& shared, SearchFile& file) {
	std::size_t at = 0, line = 1, counted = 0, lineStart = 0;
	while(at <= text.size() && !shared.stop.stop_requested()) {
		const PatternFind found = shared.pattern.find(text, at);
		if(found.exceeded) ++shared.skipped; // its matches so far are kept
		if(!found.match) return;
		const auto [offset, length] = *found.match;
		if(length == 0) {
			at = nextCodePoint(text, offset);
			continue;
		}
		if(const auto newlines = std::count(text.begin() + counted, text.begin() + offset, '\n')) {
			line += static_cast<std::size_t>(newlines);
			lineStart = text.rfind('\n', offset - 1) + 1;
		}
		counted = offset;
		if(shared.taken.fetch_add(1) >= shared.query.maxResults) {
			shared.truncated = true;
			shared.stop.request_stop();
			return;
		}
		const std::size_t lineEnd = std::min(text.find('\n', offset), text.size());
		SearchMatch match{.line = line, .column = offset - lineStart, .length = std::min(length, lineEnd - offset)};
		preview(text.substr(lineStart, lineEnd - lineStart), offset - lineStart, match);
		file.matches.push_back(std::move(match));
		at = offset + length;
	}
}

/// @brief One matching thread: takes files until none is left or it is stopped.
/// @param shared The threads' shared state.
void work(Shared& shared) {
	std::string text;
	while(!shared.stop.stop_requested()) {
		const std::size_t index = shared.next.fetch_add(1);
		if(index >= shared.files.size()) return;
		if(!readText(shared.files[index].path, shared.query.maxFileBytes, text)) {
			++shared.skipped;
			continue;
		}
		++shared.searched;
		SearchFile file{.path = shared.files[index].relative};
		matchText(text, shared, file);
		if(file.matches.empty()) continue;
		const std::scoped_lock lock(shared.mutex);
		++shared.summary.files;
		shared.summary.matches += file.matches.size();
		try {
			shared.onFile(std::move(file));
		} catch(...) { // nothing may leave the thread: stop the search instead
			shared.stop.request_stop();
		}
	}
}

/// @brief How many threads match.
/// @param query The query.
/// @param files The files to match.
/// @return At least 1, at most SearchQuery::mostThreads and the files.
std::size_t threadCount(const SearchQuery& query, std::size_t files) noexcept {
	const unsigned cores = std::max(1u, std::min(std::thread::hardware_concurrency(), 8u));
	const unsigned wanted = query.threads ? std::min(query.threads, SearchQuery::mostThreads) : cores;
	return std::max<std::size_t>(1, std::min<std::size_t>(wanted, files));
}

/// @brief A query made ready to run: its compiled pattern and globs, and its folder.
struct Prepared {
	TextPattern pattern;  ///< The pattern.
	Filters filters;      ///< The globs, and the .gitignore rules of the folders above the folder.
	fs::path start;       ///< The folder.
	std::string relative; ///< It, relative to the root ("" for the root).
};

/// @brief Reads the .gitignore files of the folders between the root and a folder below it, the folder left out.
/// @param root The root, canonical.
/// @param relative The folder, relative to the root.
/// @param rules Gets their rules.
void readIgnoresAbove(const fs::path& root, const std::string& relative, IgnoreRules& rules) {
	const std::vector<std::string_view> names = splitPath(relative);
	std::string above;
	for(std::size_t depth = 0; depth < names.size(); above = joined(above, names[depth++]))
		readIgnore(root / above, above, rules);
}

/// @brief Compiles a query's pattern and globs, and finds its folder inside the root.
/// @param root The root.
/// @param query The query.
/// @param prepared Gets the compiled query.
/// @return An error, or std::nullopt.
std::optional<std::string> prepare(const fs::path& root, const SearchQuery& query, Prepared& prepared) {
	if(auto error = prepared.pattern.compile(query.pattern, query.options)) return "pattern: " + *error;
	if(auto error = prepared.filters.include.compile(query.include)) return "include: " + *error;
	if(auto error = prepared.filters.exclude.compile(query.exclude)) return "exclude: " + *error;
	prepared.filters.gitignore = query.gitignore;
	std::error_code error;
	const fs::path canonical = fs::canonical(root, error);
	if(error) return "the root can't be read";
	const std::optional<fs::path> folder = underRoot(canonical, query.folder);
	if(!folder || !fs::is_directory(*folder, error)) return "folder: not a folder inside the root";
	prepared.start = *folder;
	prepared.relative = folder->lexically_relative(canonical).generic_string();
	if(prepared.relative == ".") prepared.relative.clear();
	if(prepared.filters.gitignore) readIgnoresAbove(canonical, prepared.relative, prepared.filters.ignore);
	return std::nullopt;
}

} // namespace

// NOLINTNEXTLINE(performance-unnecessary-value-param): the public signature (a stop_token is one shared pointer)
std::optional<std::string> searchFiles(const fs::path& root, const SearchQuery& query, std::stop_token stop,
                                       const std::function<void(SearchFile&&)>& onFile, SearchSummary& summary) {
	summary = {};
	SearchQuery limited = query;
	limited.maxResults = std::clamp<std::size_t>(query.maxResults, 1, SearchQuery::mostResults);
	Prepared prepared;
	if(auto error = prepare(root, limited, prepared)) return error;
	if(query.pattern.empty()) return std::nullopt;
	std::vector<Candidate> files;
	Shared shared{.pattern = prepared.pattern, .query = limited, .files = files, .onFile = onFile, .summary = summary};
	const std::stop_callback forward(stop, [&shared] { shared.stop.request_stop(); });
	Walk walk{.filters = prepared.filters, .query = limited, .files = files};
	if(!collect({prepared.start, prepared.relative, 0}, walk, shared.stop.get_token())) summary.truncated = true;
	{
		std::vector<std::jthread> threads;
		for(const std::size_t count = threadCount(limited, files.size()); threads.size() < count;)
			threads.emplace_back([&shared] { work(shared); });
	}
	summary.searched = shared.searched;
	summary.skipped = shared.skipped;
	summary.truncated = summary.truncated || shared.truncated;
	summary.cancelled = stop.stop_requested();
	return std::nullopt;
}

std::optional<std::string> listFiles(const fs::path& root, const SearchQuery& query, const std::stop_token& stop,
                                     std::vector<std::string>& paths, bool& truncated) {
	paths.clear();
	SearchQuery listing = query;
	listing.pattern.clear();
	Prepared prepared;
	if(auto error = prepare(root, listing, prepared)) return error;
	std::vector<Candidate> files;
	Walk walk{.filters = prepared.filters, .query = listing, .files = files};
	truncated = !collect({prepared.start, prepared.relative, 0}, walk, stop);
	paths.reserve(files.size());
	for(Candidate& file : files) paths.push_back(std::move(file.relative));
	std::ranges::sort(paths);
	return std::nullopt;
}

std::optional<std::string> replaceAll(std::string_view text, const TextPattern& pattern, std::string_view replacement,
                                      Replaced& replaced, std::size_t maxCount) {
	replaced = {};
	std::size_t at = 0, copied = 0;
	while(at <= text.size()) {
		const PatternFind found = pattern.find(text, at);
		if(found.exceeded) return "the pattern is too costly to match in this text";
		if(!found.match) break;
		const PatternMatch match = *found.match;
		if(replaced.count == maxCount) return "over " + std::to_string(maxCount) + " replacements";
		const std::optional<std::string> with = pattern.expand(text, match, replacement);
		if(!with) return "the replacement is invalid";
		replaced.text.append(text.substr(copied, match.offset - copied)).append(*with);
		if(replaced.text.size() > mostOutput) return "the text would be over 64 MiB";
		copied = match.offset + match.length;
		++replaced.count;
		at = match.length ? copied : nextCodePoint(text, match.offset);
	}
	replaced.text.append(text.substr(copied));
	return std::nullopt;
}

} // namespace affresco::services
