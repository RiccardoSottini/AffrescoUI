/// @file
/// @brief The functions of `clojure.string` (Machine.hpp, defineStrings()), written `str/join` with its alias: native
/// functions, and library entries written in the language (Library.hpp). Those the language also has bare are the
/// same functions under the namespace's names.

#include "Machine.hpp"

#include <array>
#include <string>
#include <string_view>

namespace affresco::lisp {

namespace {

/// The namespace's functions the language has bare, by their bare names.
constexpr std::array<std::string_view, 3> sameAsBare = {"join", "lower-case", "upper-case"};

} // namespace

void defineStrings(Machine& machine) {
	for(const std::string_view name : sameAsBare)
		machine.defineLanguageValue("clojure.string/" + std::string(name), machine.languageGlobal(name).value);
}

} // namespace affresco::lisp
