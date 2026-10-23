#pragma once

/// @file
/// @brief The search engine behind the `search` provider, for natives that search a folder themselves (Forge's
/// Search view): a folder's text files matched against a TextPattern on several threads, filtered by globs and
/// `.gitignore`, streamed one file at a time, cancellable and capped; and the replacement of every match in a text.

#include <affresco/core/TextPattern.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief What to look for, and where.
struct SearchQuery {
	static constexpr std::size_t mostResults = 20000; ///< The largest maxResults.
	static constexpr unsigned mostThreads = 16;       ///< The most threads.

	std::string pattern;                                   ///< The text or expression; empty matches nothing.
	PatternOptions options;                                ///< How it reads.
	std::string include;                                   ///< Comma-separated globs a file must match (none: all).
	std::string exclude;                                   ///< Comma-separated globs no file or folder may match.
	std::string folder = ".";                              ///< The folder searched, relative to the root.
	std::size_t maxResults = 2000;                         ///< Matches reported, at most (up to mostResults).
	std::size_t maxFiles = 100000;                         ///< Files looked at, at most.
	std::uintmax_t maxFileBytes = std::uintmax_t{8} << 20; ///< Larger files are skipped.
	unsigned threads = 0;                                  ///< Threads matching; 0 for the cores', at most 8.
	bool gitignore = true;                                 ///< Whether `.gitignore` files are followed.
};

/// @brief One match.
struct SearchMatch {
	std::size_t line = 0;   ///< Its line, from 1.
	std::size_t column = 0; ///< Its first byte in the line, from 0.
	std::size_t length = 0; ///< Its bytes (to the end of the line, at most, in the preview).
	std::string text{};     ///< Its line, or the part around it (at most 240 bytes, cut at code points).
	std::size_t at = 0;     ///< Where the match starts in text.
};

/// @brief The matches of one file.
struct SearchFile {
	std::string path;                   ///< Relative to the root, with '/'.
	std::vector<SearchMatch> matches{}; ///< In order.
};

/// @brief What a search did.
struct SearchSummary {
	std::size_t searched = 0; ///< Files read and matched.
	std::size_t files = 0;    ///< Files with matches.
	std::size_t matches = 0;  ///< Matches reported.
	std::size_t skipped = 0;  ///< Files skipped: too large, binary, unreadable, or a pattern over its limits.
	bool truncated = false;   ///< maxResults or maxFiles stopped it.
	bool cancelled = false;   ///< The stop token stopped it.
};

/// @brief Searches a folder. Symbolic links and `.git` aren't followed, nor folders over 64 deep, and a file with a
/// NUL byte in its first 8 KiB is binary (skipped).
/// @param root The root the folder is in; nothing outside it is read.
/// @param query What to look for.
/// @param stop Stops it early (the files found so far stay reported).
/// @param onFile Called for each file with matches, from the matching threads but never two at once.
/// @param summary Gets what it did.
/// @return An error (the pattern, a glob, the folder), or std::nullopt.
[[nodiscard]] std::optional<std::string> searchFiles(const std::filesystem::path& root, const SearchQuery& query,
                                                     std::stop_token stop,
                                                     const std::function<void(SearchFile&&)>& onFile,
                                                     SearchSummary& summary);

/// @brief Lists the files of a folder as searchFiles() would search them (the query's pattern is ignored): its globs,
/// `.gitignore`, folder and maxFiles apply, links and `.git` are left out.
/// @param root The root the folder is in; nothing outside it is listed.
/// @param query Where to look and what to leave out.
/// @param stop Stops it early (the files found so far stay listed).
/// @param paths Gets the files, relative to the root with '/', sorted.
/// @param truncated Gets whether maxFiles stopped it.
/// @return An error (a glob, the folder), or std::nullopt.
[[nodiscard]] std::optional<std::string> listFiles(const std::filesystem::path& root, const SearchQuery& query,
                                                   const std::stop_token& stop, std::vector<std::string>& paths,
                                                   bool& truncated);

/// @brief A text with its matches replaced.
struct Replaced {
	std::string text;      ///< The new text.
	std::size_t count = 0; ///< The replacements.
};

/// @brief Replaces every match of a pattern in a text (TextPattern::expand(): `$1` in an expression).
/// @param text The text.
/// @param pattern The compiled pattern.
/// @param replacement What replaces each match.
/// @param replaced Gets the text and the count.
/// @param maxCount Replacements made, at most.
/// @return An error (the replacement, the pattern's limits, too many, over 64 MiB), or std::nullopt.
[[nodiscard]] std::optional<std::string> replaceAll(std::string_view text, const TextPattern& pattern,
                                                    std::string_view replacement, Replaced& replaced,
                                                    std::size_t maxCount = 100000);

} // namespace affresco::services
