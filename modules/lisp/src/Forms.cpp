/// @file
/// @brief Forms in a text a person is editing (Program.hpp): the one before a position, the one around it, all of them.

#include "Compiler.hpp"
#include "Definitions.hpp"
#include "Machine.hpp"
#include "Reader.hpp"

#include <affresco/lisp/Program.hpp>
#include <affresco/notation/Lexer.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

using notation::Token;
using notation::TokenKind;

/// @brief Where a place is in a text, in bytes.
/// @param text The text.
/// @param line Its line, from 1.
/// @param column Its column, from 1, in characters.
/// @return The offset; the end of the line or of the text if it is past them.
std::size_t offsetOf(std::string_view text, std::size_t line, std::size_t column) {
	std::size_t at = 0;
	for(std::size_t left = line; left > 1; --left) {
		const std::size_t end = text.find('\n', at);
		if(end == std::string_view::npos) return text.size();
		at = end + 1;
	}
	for(std::size_t left = column; left > 1 && at < text.size() && text[at] != '\n'; --left) {
		++at;
		while(at < text.size() && (static_cast<std::uint8_t>(text[at]) & 0xC0U) == 0x80U) ++at;
	}
	return at;
}

/// @brief A place in a text, as a text editor counts.
/// @param text The text.
/// @param offset The place, in bytes.
/// @return Its line and its column, from 1, in characters.
std::pair<std::size_t, std::size_t> placeOf(std::string_view text, std::size_t offset) {
	std::size_t line = 1;
	std::size_t column = 1;
	for(std::size_t at = 0; at < offset && at < text.size(); ++at) {
		if(text[at] == '\n') {
			++line;
			column = 1;
		} else if((static_cast<std::uint8_t>(text[at]) & 0xC0U) != 0x80U) {
			++column;
		}
	}
	return {line, column};
}

/// @brief Gives what a form of a text has in hand: its head and address, if it is a definition form.
/// @param form The form.
/// @param found Gets them.
void readHead(const Value& form, FormAt& found) {
	if(!isDefinitionWord(headWord(form))) return;
	auto [kind, name] = definitionAddress(form);
	found.kind = std::move(kind);
	found.name = std::move(name);
}

/// @brief A form of a text, from where it starts to where it ends.
/// @param text The text.
/// @param start Where it starts.
/// @param end Where it ends.
/// @return The form; its head is read if it can be.
FormAt formIn(std::string_view text, std::size_t start, std::size_t end) {
	FormAt found;
	found.text = std::string(text.substr(start, end - start));
	std::tie(found.line, found.column) = placeOf(text, start);
	std::tie(found.endLine, found.endColumn) = placeOf(text, end);
	try {
		Heap heap;
		Reader reader(heap, found.text, "");
		if(const auto read = reader.next()) readHead(read->form, found);
		// NOLINTNEXTLINE(bugprone-empty-catch): a text being written needn't be a good one: it says so when it runs
	} catch(const DocumentError&) {
	}
	return found;
}

/// @brief Walks the forms of a text, however deep, each when it ends, up to a position, and stops at what can't be
/// read.
class FormWalk {
public:
	/// @brief Starts on a text.
	/// @param text The text.
	explicit FormWalk(std::string_view text) : lexer_(text, true) {}

	/// @brief Walks.
	/// @tparam Each What to call: (start, end, depth).
	/// @param until Stops at a token that starts at or past this.
	/// @param each What to call.
	template <class Each> void walk(std::size_t until, const Each& each) {
		std::size_t prefix = std::string_view::npos;
		try {
			for(Token token = lexer_.next(); token.kind != TokenKind::End && token.offset < until;
			    token = lexer_.next()) {
				const std::size_t start = prefix != std::string_view::npos ? prefix : token.offset;
				if(token.kind >= TokenKind::Quote && token.kind <= TokenKind::Splice) {
					prefix = start;
					continue;
				}
				prefix = std::string_view::npos;
				if(token.kind == TokenKind::OpenNode || token.kind == TokenKind::OpenList ||
				   token.kind == TokenKind::OpenMap) {
					opens_.push_back(start);
				} else if(token.kind == TokenKind::CloseNode || token.kind == TokenKind::CloseList ||
				          token.kind == TokenKind::CloseMap) {
					if(opens_.empty()) return;
					const std::size_t from = opens_.back();
					opens_.pop_back();
					each(from, token.offset + token.text.size(), opens_.size());
				} else {
					each(start, token.offset + token.text.size(), opens_.size());
				}
			}
			// NOLINTNEXTLINE(bugprone-empty-catch): what follows can't be read: what came before still stands
		} catch(const DocumentError&) {
		}
	}

	/// @brief The opening of the outermost form not closed where the walk stopped.
	/// @return It, or std::string_view::npos.
	[[nodiscard]] std::size_t open() const noexcept { return opens_.empty() ? std::string_view::npos : opens_.front(); }

private:
	notation::Lexer lexer_;          ///< The tokens.
	std::vector<std::size_t> opens_; ///< Where each form not closed yet starts.
};

} // namespace

std::optional<FormAt> formBefore(std::string_view text, std::size_t line, std::size_t column) {
	const std::size_t position = offsetOf(text, line, column);
	std::optional<std::pair<std::size_t, std::size_t>> last;
	FormWalk(text).walk(position, [&](std::size_t start, std::size_t end, std::size_t) {
		if(end <= position) last = {start, end};
	});
	if(!last) return std::nullopt;
	return formIn(text, last->first, last->second);
}

std::optional<FormAt> formAround(std::string_view text, std::size_t line, std::size_t column) {
	const std::size_t position = offsetOf(text, line, column);
	std::optional<std::pair<std::size_t, std::size_t>> around;
	std::optional<std::pair<std::size_t, std::size_t>> before;
	std::optional<std::pair<std::size_t, std::size_t>> after;
	FormWalk walk(text);
	walk.walk(text.size() + 1, [&](std::size_t start, std::size_t end, std::size_t depth) {
		if(depth != 0) return;
		if(start <= position && position <= end) {
			if(!around) around = {start, end}; // between two forms: the one before
		} else if(end < position) {
			before = {start, end};
		} else if(!after) {
			after = {start, end};
		}
	});
	if(walk.open() != std::string_view::npos && walk.open() <= position) return std::nullopt; // not finished yet
	const auto& form = around ? around : before ? before : after;
	if(!form) return std::nullopt;
	return formIn(text, form->first, form->second);
}

std::vector<FormAt> formsOf(std::string_view text, const std::string& file) {
	Heap heap;
	Reader reader(heap, text, file);
	std::vector<FormAt> forms;
	while(const auto read = reader.next()) {
		FormAt found;
		found.text = std::string(text.substr(read->start, read->end - read->start));
		std::tie(found.line, found.column) = placeOf(text, read->start);
		std::tie(found.endLine, found.endColumn) = placeOf(text, read->end);
		readHead(read->form, found);
		forms.push_back(std::move(found));
	}
	return forms;
}

} // namespace affresco::lisp
