#pragma once

/// @file
/// @brief The text nREPL clients (CIDER, Calva, Conjure) evaluate for themselves, in Clojure, when they connect or
/// switch files: recognised here by a small scanner and answered by the protocol, never sent to the program. Nothing
/// else is recognised: any other text goes to the program, which answers in its own words.

#include <cstddef>
#include <string>
#include <string_view>

namespace affresco::net::nrepl {

/// @brief What a client's own text asks.
enum class ClientFormKind {
	None,              ///< Not a client's own text: the program evaluates it.
	CurrentNamespace,  ///< `*ns*`: the session's namespace name.
	RecentValue,       ///< `*1`, `*2` or `*3`: one of the session's last values.
	LastError,         ///< `*e`: the session's last error.
	ReaderConditional, ///< `#?(...)`: which dialect this is; `unknown`.
	SwitchNamespace,   ///< `(ns name ...)` or `(in-ns 'name)`: the session's namespace becomes the name.
	Require,           ///< `(require ...)`, `(use ...)`: nothing to load; `nil`.
	Documentation,     ///< `(doc name)`: the name's documentation, which needs the program.
	ClientSetup        ///< A client's own setup (its helpers, its REPL requires): `nil`.
};

/// @brief A client's own text, recognised.
struct ClientForm {
	ClientFormKind kind = ClientFormKind::None; ///< What it asks.
	std::string name;                           ///< The namespace or the documented name, when it names one.
	std::size_t back = 0;                       ///< For RecentValue: 1, 2 or 3.
};

/// @brief Recognises a client's own text.
/// @param text The whole text of an `eval` (or a loaded file).
/// @return What it asks; ClientFormKind::None for anything else.
[[nodiscard]] ClientForm recogniseClientForm(std::string_view text);

/// @brief Tells whether answering a client's own text needs the program (and so a paired connection).
/// @param kind What it asks.
/// @return true for Documentation (and None, which the program answers).
[[nodiscard]] bool needsProgram(ClientFormKind kind) noexcept;

} // namespace affresco::net::nrepl
