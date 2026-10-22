#pragma once

/// @file
/// @brief format::ReadSet and format::StateChanges: what a value read from the state while it was evaluated, and what
/// changed in the state since the last update, so an update evaluates again only what reads a change.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// The places a read set keeps before it keeps their common place instead (which reads more, never less).
inline constexpr std::size_t maxReadPaths = 32;
/// The places the changes of one update keep before they count as a change of everything.
inline constexpr std::size_t maxChangedPaths = 64;

/// A key read as "any key": a read set with too many places keeps "/items/~*/id" for "/items/0/id", "/items/1/id"...
/// No JSON Pointer has it ("~" is always followed by 0 or 1), so it never names a real key.
inline constexpr std::string_view anyKey = "~*";
/// The key a repeat reads below a list of the state to read only its shape: "/items/~#" changes when "/items" or a
/// place around it changes, not when a place inside one item does (the items read that themselves).
inline constexpr std::string_view shapeKey = "~#";

/// @brief Whether a place is another or inside it, key by key.
/// @param outer The outer place, as a JSON Pointer.
/// @param inner The inner place.
/// @param both Whether anyKey in the inner place matches any key too (else only anyKey in the outer).
/// @return true for "/a" and "/a" or "/a/b", false for "/a" and "/ab".
[[nodiscard]] bool contains(std::string_view outer, std::string_view inner, bool both = false) noexcept;

/// @brief Whether a read of the state and a change of it meet: one place is the other or inside it (anyKey matching
/// any key).
/// @param read A place read, as a JSON Pointer ("" is the whole state).
/// @param written A place changed, as a JSON Pointer.
/// @return true if the value read may have changed.
[[nodiscard]] bool overlaps(std::string_view read, std::string_view written) noexcept;

/// @brief What changed in the state since the last update: places, or everything (a change no place describes, a
/// revision, a resize, a new screen or theme).
struct StateChanges {
	std::vector<std::string> paths; ///< The places changed, as JSON Pointers.
	bool all = false;               ///< Anything may have changed: every value is evaluated again.

	/// @brief Records a changed place.
	/// @param pointer The place, as a JSON Pointer.
	void add(std::string pointer);
	/// @brief Records that anything may have changed.
	void addAll() noexcept {
		all = true;
		paths.clear();
	}
	/// @brief Whether nothing changed.
	/// @return true if no place and not everything.
	[[nodiscard]] bool empty() const noexcept { return !all && paths.empty(); }
};

/// @brief What evaluating values read that a later update may change: places of the state, or something no state
/// change announces (a panel's geometry, the clock), which is read again on every update.
struct ReadSet {
	std::vector<std::string> paths; ///< The places of the state read, as JSON Pointers (at most maxReadPaths).
	bool always = false;            ///< It reads what no change announces: evaluated again on every update.

	/// @brief Records a place read.
	/// @param pointer The place, as a JSON Pointer.
	void addPath(std::string_view pointer);
	/// @brief Records what another set read.
	/// @param other The set.
	void add(const ReadSet& other);
	/// @brief Forgets everything read.
	void clear() noexcept {
		paths.clear();
		always = false;
	}
	/// @brief Whether a value that read this may have changed.
	/// @param changes What changed.
	/// @return true if it reads a place that changed, anything when everything changed, or reads always.
	[[nodiscard]] bool touchedBy(const StateChanges& changes) const noexcept;

private:
	/// @brief Reads fewer places, when there are too many: places that differ in one key become one with anyKey there
	/// ("/items/3/id" and "/items/4/id" become "/items/~*/id"), then, if still too many, their common place.
	void collapse();
	/// @brief Merges two places that differ in one key into one, if there are two such.
	/// @return true if it merged two.
	bool generaliseOnce();
};

/// @brief Appends a key to a JSON Pointer, escaped as RFC 6901 says.
/// @param pointer The pointer.
/// @param key The key.
void appendKey(std::string& pointer, std::string_view key);

} // namespace affresco::format
