#pragma once

/// @file
/// @brief The search's path filters: globs (`*`, `?`, `[...]`, `**`) for "include" and "exclude", and the rules of
/// `.gitignore` files. Matching is iterative (no recursion) and linear in the path times the glob, and every input is
/// capped, since the globs come from a peer.

#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::services {

/// @brief One compiled glob, matched against a path split at '/'.
class Glob {
public:
	static constexpr std::size_t maxBytes = 256; ///< The longest glob.

	/// @brief One element of a segment.
	struct Element {
		/// @brief What it matches.
		enum class Kind { Literal, Any, Star, Class };
		Kind kind = Kind::Literal;                         ///< See Kind.
		std::string literal{};                             ///< A Literal's bytes.
		std::vector<std::pair<char32_t, char32_t>> ranges{}; ///< A Class's code point ranges, inclusive.
		bool negated = false;                              ///< A Class that matches what isn't in its ranges.
	};
	/// @brief One segment: `**`, or elements matched against one name.
	struct Segment {
		bool globstar = false;         ///< `**`: any number of names, none included.
		std::vector<Element> elements{}; ///< The others'.
	};

	/// @brief Compiles a glob. A glob without '/' (other than a trailing one) matches a name at any depth; one with a
	/// '/' is anchored to the start (a leading '/' only anchors); a trailing '/' matches only folders.
	/// @param text The glob (at most maxBytes).
	/// @return The glob, or std::nullopt if it is empty or too long.
	[[nodiscard]] static std::optional<Glob> compile(std::string_view text);

	/// @brief Whether a whole path matches.
	/// @param segments The path's names.
	/// @param folder Whether the path is a folder.
	/// @return Whether it does.
	[[nodiscard]] bool matches(std::span<const std::string_view> segments, bool folder) const;

	/// @brief Whether the path, or one of the folders it is in, matches.
	/// @param segments The path's names.
	/// @param folder Whether the path is a folder.
	/// @return Whether one does.
	[[nodiscard]] bool matchesOrParent(std::span<const std::string_view> segments, bool folder) const;

private:
	/// @brief Matches some of a path's names.
	/// @param names The names.
	/// @return Whether they match.
	[[nodiscard]] bool matchesNames(std::span<const std::string_view> names) const;

	std::vector<Segment> segments_; ///< The glob's segments.
	bool folderOnly_ = false;       ///< Whether it ended with '/'.
};

/// @brief A comma-separated list of globs, as a screen gives "include" and "exclude".
class GlobList {
public:
	static constexpr std::size_t maxGlobs = 64; ///< The most globs in a list.

	/// @brief Compiles a list; spaces around each glob are dropped, and empty ones skipped.
	/// @param text The list.
	/// @return An error, or std::nullopt.
	[[nodiscard]] std::optional<std::string> compile(std::string_view text);

	/// @brief Whether the list has no glob.
	/// @return Whether it is empty.
	[[nodiscard]] bool empty() const noexcept { return globs_.empty(); }

	/// @brief Whether a path, or a folder it is in, matches one of the globs.
	/// @param segments The path's names.
	/// @param folder Whether the path is a folder.
	/// @return Whether one does.
	[[nodiscard]] bool matches(std::span<const std::string_view> segments, bool folder) const;

private:
	std::vector<Glob> globs_; ///< The globs.
};

/// @brief The rules of the `.gitignore` files met so far, each applying under the folder it was in.
class IgnoreRules {
public:
	static constexpr std::size_t maxFileBytes = 64 * 1024; ///< A .gitignore read, at most (the rest is ignored).
	static constexpr std::size_t maxRules = 10000;         ///< Rules kept, at most (later ones are dropped).

	/// @brief Adds the rules of one file: comments (`#`), `!` to include again, a trailing '/' for folders only.
	/// @param base The folder it is in, relative to the walk's root ("" for the root), without a trailing '/'.
	/// @param text The file's text.
	void add(std::string_view base, std::string_view text);

	/// @brief Whether a path is ignored: the last rule that matches it says. Its folders are checked when they are
	/// walked, so a file in an ignored folder is never asked about.
	/// @param segments The path's names, relative to the walk's root.
	/// @param folder Whether the path is a folder.
	/// @return Whether it is ignored.
	[[nodiscard]] bool ignored(std::span<const std::string_view> segments, bool folder) const;

	/// @brief How many rules there are.
	/// @return The count.
	[[nodiscard]] std::size_t size() const noexcept { return rules_.size(); }

private:
	/// @brief One rule.
	struct Rule {
		Glob glob;                     ///< What it matches, under its base.
		std::vector<std::string> base; ///< The folder it applies under.
		bool negated = false;          ///< `!`: the path isn't ignored.
	};
	std::vector<Rule> rules_; ///< In the order they were read.
};

/// @brief Splits a relative path at '/', skipping empty names.
/// @param path The path; the views point into it.
/// @return Its names.
[[nodiscard]] std::vector<std::string_view> splitPath(std::string_view path);

} // namespace affresco::services
