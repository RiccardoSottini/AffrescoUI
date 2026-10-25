/// @file
/// @brief Expression functions: keys, `keyLabel` (a chord as people read it, in a notation).

#include "ExpressionFunctions.hpp"
#include "KeyNotation.hpp"
#include "Shortcuts.hpp"

#include <array>

namespace affresco::format::detail {
namespace {

/// @brief `keyLabel(chord, notation?)`: "ctrl+x ctrl+f" as "⌃X ⌃F", or "C-x C-f" in the "letters" notation.
/// @param call [chord, notation?].
/// @return The label, "" for no chord (null).
Value keyLabelOf(const Call& call) {
	if(call[0].isNull()) return Value::fromText("");
	const std::string notation = call.size() > 1 ? call[1].toText() : "platform";
	if(!knownKeyNotation(notation)) call.fail(R"(expected the notation "platform", "mac", "pc" or "letters")");
	return Value::fromText(keyLabel(normalizeChord(call[0].toText()), notation));
}

/// The table.
constexpr auto table = std::to_array<Function>({
    {.name = "keyLabel",
	 .arguments = "chord, notation?",
	 .about = "A chord as people read it: \"ctrl+x ctrl+f\" is \"⌃X ⌃F\" on macOS; the notation \"mac\", \"pc\" or "
	          "\"emacs\" (\"C-x C-f\")",
	 .fewest = 1,
	 .most = 2,
	 .apply = keyLabelOf},
});

} // namespace

std::span<const Function> keyFunctions() { return table; }

} // namespace affresco::format::detail
