#pragma once

/// @file
/// @brief What reading a program and the machine agree on about the definition forms.

#include "Objects.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace affresco::lisp {

/// @brief Tells whether a word is the head of a definition form: `def`, `defn`, `defview`, `defmacro`, `defscreen`,
/// `defcommand`, `defkey`, `defcustom`, `defhook`, `add-hook`, `remove-hook`, `advice`, or one that defines a member of
/// a section (sectionOfForm()).
/// @param word The word.
/// @return true if it is.
[[nodiscard]] bool isDefinitionWord(std::string_view word) noexcept;

/// @brief The section of the document a form defines a member of: `defstate` gives "state", `defcomputed`
/// "computed", `defstyle` "styles", `defcomponent` "components", `defmenu` "menus", `deftokens` "tokens".
/// @param head The form's head.
/// @return The section, or "" if it is no such form.
[[nodiscard]] std::string_view sectionOfForm(std::string_view head) noexcept;

/// @brief Tells whether a word is the head of a form that joins something: `add-hook`, `remove-hook` or `advice`.
/// @param word The word.
/// @return true if it is.
[[nodiscard]] bool isExtensionWord(std::string_view word) noexcept;

/// @brief A top-level form's kind (its head) and its address: the name it defines; `defkey ctrl+k` and
/// `add-hook after-save f` for the forms that define no name.
/// @param form The form.
/// @return The kind and the address.
/// @throws LispError If it has neither.
[[nodiscard]] std::pair<std::string, std::string> definitionAddress(const Value& form);

/// @brief A place, as an error names it.
/// @param place The place.
/// @return "killring.lisp: line 3, column 8".
[[nodiscard]] std::string describePlace(Place place);

} // namespace affresco::lisp
