#pragma once

/// @file
/// @brief The language's set functions written as plain words (Words.hpp): the core's that make and change a set.
/// The `clojure.set` namespace's are written with it (`set/union`), and so aren't plain words.

#include <array>
#include <string_view>

namespace affresco::notation {

/// The plain words of the language's set functions, as std::to_array<std::string_view>({...}), the form
/// tools/notation/lisp_program.py reads; Words.hpp's isLanguageWord() reads it too. `set?` has a '?', so it isn't a
/// plain word and is on none of the lists; `set` is the host's word (a state's value), not the language's.
inline constexpr auto setsWords = std::to_array<std::string_view>({"disj", "hash-set"});

} // namespace affresco::notation
