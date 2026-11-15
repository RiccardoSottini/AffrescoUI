/// @file
/// @brief recogniseClientForm() (ClientForms.hpp): a scanner of its own, small on purpose: the net module links core
/// only, so it reads just enough of the text (white space, comments, strings, brackets, a head symbol) to tell a
/// client's own form from the program's code.

#include "ClientForms.hpp"

#include <array>

namespace affresco::net::nrepl {

namespace {

constexpr auto none = std::string_view::npos; ///< "Not found", for positions.

/// @brief Tells whether a character separates symbols (white space, a comma, a bracket, a quote, a comment).
/// @param character It.
/// @return true if it does.
constexpr bool separates(char character) noexcept {
	return std::string_view(" \t\r\n\f\v,()[]{}\"';").find(character) != none;
}

/// @brief Skips white space, commas and `;` comments.
/// @param text The text.
/// @param from Where to start.
/// @return The first position after them (text.size() at the end).
std::size_t skipBlank(std::string_view text, std::size_t from) noexcept {
	while(from < text.size()) {
		const char character = text[from];
		if(character == ';') {
			const std::size_t lineEnd = text.find('\n', from);
			from = lineEnd == none ? text.size() : lineEnd + 1U;
		} else if(std::string_view(" \t\r\n\f\v,").find(character) != none) {
			++from;
		} else {
			break;
		}
	}
	return from;
}

/// @brief Reads a symbol (or any token up to a separator).
/// @param text The text.
/// @param from Where it starts.
/// @return It; empty if a separator is at @p from.
std::string_view readSymbol(std::string_view text, std::size_t from) noexcept {
	std::size_t end = from;
	while(end < text.size() && !separates(text[end])) ++end;
	return text.substr(from, end - from);
}

/// @brief Finds the end of a bracketed form, past strings, character literals and comments.
/// @param text The text.
/// @param open Where its opening bracket is.
/// @return The position after its closing bracket, or npos if it never closes.
std::size_t formEnd(std::string_view text, std::size_t open) noexcept {
	std::size_t depth = 0;
	for(std::size_t at = open; at < text.size(); ++at) {
		const char character = text[at];
		if(character == '\\') {
			++at; // a character literal: `\(` opens nothing
		} else if(character == ';') {
			const std::size_t lineEnd = text.find('\n', at);
			if(lineEnd == none) return none;
			at = lineEnd;
		} else if(character == '"') {
			for(++at; at < text.size() && text[at] != '"'; ++at)
				if(text[at] == '\\') ++at;
			if(at >= text.size()) return none;
		} else if(character == '(' || character == '[' || character == '{') {
			++depth;
		} else if(character == ')' || character == ']' || character == '}') {
			if(depth == 0) return none;
			if(--depth == 0) return at + 1U;
		}
	}
	return none;
}

/// @brief Tells whether a text names a client's own setup anywhere: its REPL requires or its helper namespaces.
/// A text that only quotes one of these names in a string is matched too: an accepted limit, since the answer is
/// `nil` and nothing runs.
/// @param text The text.
/// @return true if it does.
bool namesClientSetup(std::string_view text) noexcept {
	constexpr std::array<std::string_view, 3> names{"clojure.main/repl-requires", "conjure.internal", "cider.nrepl"};
	for(const std::string_view name : names)
		if(text.find(name) != none) return true;
	return false;
}

/// @brief Recognises a text that is one symbol: `*ns*`, `*1`, `*2`, `*3`, `*e`.
/// @param symbol The symbol.
/// @return What it asks, or None.
ClientForm recogniseSymbol(std::string_view symbol) {
	if(symbol == "*ns*") return {.kind = ClientFormKind::CurrentNamespace, .name = {}, .back = 0};
	if(symbol == "*e") return {.kind = ClientFormKind::LastError, .name = {}, .back = 0};
	if(symbol.size() == 2U && symbol[0] == '*' && symbol[1] >= '1' && symbol[1] <= '3')
		return {.kind = ClientFormKind::RecentValue, .name = {}, .back = static_cast<std::size_t>(symbol[1] - '0')};
	return {};
}

/// @brief Reads the name a form's head is followed by, a quote before it dropped (`'user`).
/// @param text The text.
/// @param from Where the head ends.
/// @return The name; empty if none follows.
std::string_view followingName(std::string_view text, std::size_t from) noexcept {
	std::size_t at = skipBlank(text, from);
	if(at < text.size() && text[at] == '\'') ++at;
	return readSymbol(text, at);
}

/// @brief Recognises a text that is one bracketed form, by its head.
/// @param text The text.
/// @param open Where the form's `(` is.
/// @return What it asks, or None.
ClientForm recogniseForm(std::string_view text, std::size_t open) {
	const std::size_t end = formEnd(text, open);
	if(end == none || skipBlank(text, end) != text.size()) return {}; // not exactly one form
	const std::size_t headStart = skipBlank(text, open + 1U);
	const std::string_view head = readSymbol(text, headStart);
	const std::size_t headEnd = headStart + head.size();
	if(head == "ns" || head == "in-ns" || head == "clojure.repl/doc" || head == "doc") {
		const std::string_view name = followingName(text, headEnd);
		if(name.empty()) return {};
		const bool documentation = head.ends_with("doc");
		return {.kind = documentation ? ClientFormKind::Documentation : ClientFormKind::SwitchNamespace,
		        .name = std::string(name),
		        .back = 0};
	}
	if(head == "require" || head == "use" || head == "clojure.core/require")
		return {.kind = ClientFormKind::Require, .name = {}, .back = 0};
	return {};
}

} // namespace

ClientForm recogniseClientForm(std::string_view text) {
	const std::size_t start = skipBlank(text, 0);
	if(start == text.size()) return {};
	if(text.substr(start).starts_with("#?(")) return {.kind = ClientFormKind::ReaderConditional, .name = {}, .back = 0};
	if(namesClientSetup(text)) return {.kind = ClientFormKind::ClientSetup, .name = {}, .back = 0};
	if(text[start] == '(') return recogniseForm(text, start);
	const std::string_view symbol = readSymbol(text, start);
	if(symbol.empty() || skipBlank(text, start + symbol.size()) != text.size()) return {};
	return recogniseSymbol(symbol);
}

bool needsProgram(ClientFormKind kind) noexcept {
	return kind == ClientFormKind::None || kind == ClientFormKind::Documentation;
}

} // namespace affresco::net::nrepl
