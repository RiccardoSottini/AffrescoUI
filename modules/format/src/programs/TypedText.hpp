#pragma once

/// @file
/// @brief format::TypedText: what the person typed themselves, proved from their keystrokes. It is what `evaluate`
/// gives their authority to (RFC 0005, decision 5; SECURITY.md, "The loop").

#include "affresco/core/Json.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace affresco::format {

/// @brief The runs of text the person typed, in the state values their text widgets are bound to.
///
/// It records what is known, never what is suspected: a text with no record is nobody's, so nothing has to mark what an
/// agent writes, and no way of moving text around can carry a mark along. Only a keystroke of the person's, delivered
/// to the widget directly, makes a run (Panel::takeTyped()). Whatever else changes a value is seen as a difference the
/// next time the value is looked at, and what it changed stops being theirs: both sides stay separate runs, so taking
/// characters out can't make a new form of what they typed. Nothing is kept across runs of the app.
///
/// Invariants: every byte of a run came from a keystroke of the person, in this order, with nothing taken out from
/// between them but by the person. A run's length is what a person typed, so looking inside runs costs little whatever
/// an agent wrote around them. Main thread only.
class TypedText {
public:
	/// @brief Records one keystroke's change of a bound value.
	/// @param place The value's JSON Pointer in the state.
	/// @param before What the value holds.
	/// @param after What the keystroke made of it.
	void typed(const std::string& place, std::string_view before, std::string_view after);
	/// @brief Tells whether a text is, whole, in one run the person typed, in what a bound value holds now.
	/// @param form The text.
	/// @param state The live state.
	/// @return true if it is.
	[[nodiscard]] bool holds(std::string_view form, const json& state);

private:
	/// @brief Some bytes of a value.
	struct Run {
		std::size_t start = 0; ///< The first.
		std::size_t end = 0;   ///< One past the last.
	};
	/// @brief What is known of one bound value.
	struct Held {
		std::string place;     ///< Its JSON Pointer in the state.
		std::string text;      ///< What it held when last looked at.
		std::vector<Run> runs; ///< What in that the person typed. They don't overlap, and adjoining ones stay apart.
	};

	/// @brief Takes a change anyone may have made: what it changed stops being the person's.
	/// @param held The value.
	/// @param now What it holds now.
	static void follow(Held& held, std::string_view now);

	std::vector<Held> held_; ///< The values typed into, the oldest first.
};

} // namespace affresco::format
