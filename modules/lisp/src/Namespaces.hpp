#pragma once

/// @file
/// @brief The namespaces a qualified word names (README.md, "Qualified names"): `str/join` is the `join` of
/// `clojure.string`, through its alias `str`. A bare namespace's functions are globals of their own names
/// (`clojure.core/inc` is `inc`); a prefixed one's are globals named with the namespace (`clojure.string/join`).

#include <functional>
#include <map>
#include <string>
#include <string_view>

namespace affresco::lisp {

/// Aliases by name, to the namespace each stands for.
using AliasMap = std::map<std::string, std::string, std::less<>>;

/// @brief A namespace a prefix names.
struct NamespaceFound {
	std::string_view name; ///< Its full name.
	bool bare = false;     ///< Its functions are globals of their own names.
};

/// @brief The namespaces, and the aliases for them.
class Namespaces {
public:
	/// @brief The language's: `clojure.core` (bare), `clojure.string` and `clojure.set`; `str` and `string` stand for
	/// `clojure.string`, `set` for `clojure.set`.
	Namespaces();

	/// @brief Adds a namespace, or changes whether one is bare.
	/// @param name Its name.
	/// @param bare Whether its functions are globals of their own names.
	void add(std::string name, bool bare);
	/// @brief Adds an alias, or points one elsewhere.
	/// @param alias The alias.
	/// @param name The namespace.
	/// @return false if no namespace has that name.
	bool alias(std::string alias, std::string_view name);
	/// @brief The namespace a prefix names: through the aliases given, then these, then by its own name.
	/// @param prefix The prefix.
	/// @param aliases A file's own aliases, or nullptr.
	/// @return It; an empty name if none.
	[[nodiscard]] NamespaceFound find(std::string_view prefix, const AliasMap* aliases) const;

private:
	std::map<std::string, bool, std::less<>> spaces_; ///< Whether each is bare, by name.
	AliasMap aliases_;                                ///< See alias().
};

/// @brief Tells whether a word is qualified: a '/' that is neither its first character nor its last (`/` is
/// division, and `a/` a word of its own).
/// @param word The word.
/// @return true if it is; the namespace is what comes before its first '/'.
[[nodiscard]] constexpr bool isQualified(std::string_view word) noexcept {
	const std::size_t slash = word.find('/');
	return slash != std::string_view::npos && slash > 0 && slash + 1 < word.size();
}

} // namespace affresco::lisp
