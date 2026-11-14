/// @file
/// @brief What a name the language doesn't have means here (Machine.hpp, hintFor()): the error for an undefined name
/// that another dialect knows says what to write instead. A hint names names, never a value.

#include "Machine.hpp"

#include <affresco/notation/Lexer.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace affresco::lisp {

namespace {

constexpr std::string_view stateHint =
    "the language has no mutable state: a program keeps it in the app's state (defstate), and a step changes it";
constexpr std::string_view printHint = "the language prints nothing: the host's log step writes to the log";
constexpr std::string_view randomHint = "the language has no randomness: a program gives the same value every time";
constexpr std::string_view loadHint =
    "a program loads another file with include; str/ and set/ need none, they are always there";
constexpr std::string_view lazyHint = "the language has no lazy sequences: range, or a loop, makes one that ends";
constexpr std::string_view regexHint = "the language has no regular expressions: str/ has the text functions";

/// The names, sorted, and what to write instead.
constexpr auto hints = std::to_array<std::pair<std::string_view, std::string_view>>({
    {"atom", stateHint},
    {"cycle", lazyHint},
    {"declare", "a definition can use one written after it: declare isn't needed"},
    {"deref", stateHint},
    {"import", loadHint},
    {"in-ns", loadHint},
    {"iterate", lazyHint},
    {"lazy-seq", lazyHint},
    {"print", printHint},
    {"println", printHint},
    {"prn", printHint},
    {"rand", randomHint},
    {"rand-int", randomHint},
    {"rand-nth", randomHint},
    {"re-find", regexHint},
    {"re-matches", regexHint},
    {"re-pattern", regexHint},
    {"re-seq", regexHint},
    {"repeatedly", lazyHint},
    {"require", loadHint},
    {"reset!", stateHint},
    {"set", "\"set\" is the host's word for a state's value: make a set with (into #{} xs) or (hash-set x y)"},
    {"shuffle", randomHint},
    {"swap!", stateHint},
    {"use", loadHint},
    {"volatile!", stateHint},
});

static_assert(std::ranges::is_sorted(hints, {}, &std::pair<std::string_view, std::string_view>::first));

} // namespace

std::string_view hintFor(std::string_view name) {
	const auto* found =
	    std::ranges::lower_bound(hints, name, {}, &std::pair<std::string_view, std::string_view>::first);
	return found != hints.end() && found->first == name ? found->second : std::string_view();
}

std::string notDefinedMessage(std::string_view name) {
	std::string message = "\"" + notation::Lexer::excerpt(name) + "\" is not defined";
	if(const std::string_view hint = hintFor(name); !hint.empty()) message.append(": ").append(hint);
	return message;
}

} // namespace affresco::lisp
