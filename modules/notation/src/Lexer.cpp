/// @file
/// @brief The tokens of Lisp text (Lexer.hpp).

#include "affresco/notation/Lexer.hpp"

#include "affresco/notation/Notation.hpp"
#include "affresco/notation/Words.hpp"

#include <algorithm>
#include <string>

namespace affresco::notation {

namespace {

/// @brief Moves past digits.
/// @param text The text.
/// @param at Where they may start.
/// @return Past the last of them (@p at if there are none).
std::size_t pastDigits(std::string_view text, std::size_t at) noexcept {
	while(at < text.size() && text[at] >= '0' && text[at] <= '9') ++at;
	return at;
}

/// @brief Moves past an optional part of a number: one of its marks, an optional sign, then digits (".5", "e+10").
/// @param text The text.
/// @param at Where the part may start.
/// @param marks The characters that start it: "." or "eE".
/// @param sign Whether a sign may follow the mark.
/// @return Past the part, @p at if there is none, or std::string_view::npos if its mark has no digits after it.
std::size_t pastPart(std::string_view text, std::size_t at, std::string_view marks, bool sign) noexcept {
	if(at >= text.size() || !marks.contains(text[at])) return at;
	++at;
	if(sign && at < text.size() && (text[at] == '+' || text[at] == '-')) ++at;
	const std::size_t end = pastDigits(text, at);
	return end == at ? std::string_view::npos : end;
}

/// @brief Tells whether a string in quotes is plain ASCII without an escape, as most strings are.
/// @param written The string, with its quotes.
/// @return true if what is between its quotes is itself.
bool isPlainQuoted(std::string_view written) {
	return written.size() >= 2 && written.front() == '"' && written.back() == '"' &&
	       std::ranges::none_of(written.substr(1, written.size() - 2), [](char c) {
		       const auto byte = static_cast<unsigned char>(c);
		       return byte < 0x20 || byte >= 0x80 || c == '\\' || c == '"';
	       });
}

/// @brief Escapes the line breaks and tabs typed inside a string's quotes, for JSON to read them.
/// @param written The string, with its quotes.
/// @return It, with each typed as its escape; one after a backslash is no escape JSON knows, and stays as it is.
std::string escapedBreaks(std::string_view written) {
	std::string escaped;
	escaped.reserve(written.size() + 8);
	bool escaping = false; // after a backslash
	for(const char c : written) {
		if(!escaping && c == '\n') escaped += "\\n";
		else if(!escaping && c == '\r') escaped += "\\r";
		else if(!escaping && c == '\t') escaped += "\\t";
		else escaped += c;
		escaping = !escaping && c == '\\';
	}
	return escaped;
}

} // namespace

bool isJsonNumber(std::string_view text) noexcept {
	const std::size_t start = text.starts_with('-') ? 1 : 0;
	const std::size_t integer = pastDigits(text, start);
	if(integer == start || (text[start] == '0' && integer > start + 1)) return false; // no digit, or a leading zero
	const std::size_t fraction = pastPart(text, integer, ".", false);
	return fraction != std::string_view::npos && pastPart(text, fraction, "eE", true) == text.size();
}

Lexer::Lexer(std::string_view text, bool language) : text_(text), language_(language) {
	if(text_.size() > maxLispBytes) fail(0, "the text is longer than " + std::to_string(maxLispBytes >> 20) + " MiB");
	if(text_.starts_with("\xEF\xBB\xBF")) at_ = 3; // a byte order mark, as some editors write
}

Token Lexer::next() {
	for(;;) {
		bool discard = false;
		const Token token = raw(discard);
		if(!discard) return token;
		discardForm(token.offset);
		if(keepComments_) comments_.push_back({text_.substr(token.offset, lastEnd_ - token.offset), false});
	}
}

Token Lexer::raw(bool& discard) {
	skipSpace();
	if(at_ >= text_.size()) return {TokenKind::End, {}, at_};
	const std::size_t start = at_;
	const char c = text_[at_];
	static constexpr std::string_view brackets = "()[]{}";
	TokenKind kind = TokenKind::Word;
	if(const std::size_t bracket = brackets.find(c); bracket != std::string_view::npos) {
		++at_;
		kind = static_cast<TokenKind>(bracket);
	} else if(c == '"') {
		kind = TokenKind::String;
		quoted(start);
	} else if(c == ':' || c == '#') {
		kind = c == ':' ? TokenKind::Key : TokenKind::Id;
		discard = nameOrDiscard();
	} else if(const std::optional<TokenKind> prefix = language_ ? prefixAt() : std::nullopt) {
		kind = *prefix;
	} else {
		wordAt(start);
	}
	lastEnd_ = at_;
	return {kind, text_.substr(start, at_ - start), start};
}

bool Lexer::nameOrDiscard() {
	const bool discard = text_.substr(at_, 2) == "#_" && (at_ + 2 >= text_.size() || !isWordCharacter(text_[at_ + 2]));
	++at_;
	if(discard) ++at_;
	else if(at_ < text_.size() && text_[at_] == '"') quoted(at_);
	else wordAt(at_);
	return discard;
}

std::optional<TokenKind> Lexer::prefixAt() noexcept {
	const char c = text_[at_];
	if(c == '\'' || c == '`') {
		++at_;
		return c == '`' ? TokenKind::Backquote : TokenKind::Quote;
	}
	if(c != '~') return std::nullopt;
	const bool splice = at_ + 1 < text_.size() && text_[at_ + 1] == '@';
	at_ += splice ? 2 : 1;
	return splice ? TokenKind::Splice : TokenKind::Unquote;
}

void Lexer::keepComment(std::size_t end) {
	std::string_view comment = text_.substr(at_, end - at_);
	while(!comment.empty() && (comment.back() == '\r' || comment.back() == ' ' || comment.back() == '\t'))
		comment.remove_suffix(1);
	const std::size_t line = text_.rfind('\n', at_);
	const bool token = lastEnd_ > 0 && (line == std::string_view::npos || lastEnd_ > line);
	comments_.push_back({comment, token && comments_.empty()});
}

void Lexer::skipSpace() {
	while(at_ < text_.size()) {
		const char c = text_[at_];
		if(c == ';') {
			const std::size_t end = std::min(text_.find('\n', at_), text_.size());
			if(keepComments_) keepComment(end);
			at_ = end;
		} else if(c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ',') {
			++at_;
		} else {
			return;
		}
	}
}

void Lexer::quoted(std::size_t start) {
	std::size_t end = start + 1;
	while(end < text_.size() && text_[end] != '"') end += text_[end] == '\\' ? std::size_t{2} : std::size_t{1};
	if(end >= text_.size()) fail(start, "a string that never ends: expected '\"'");
	at_ = end + 1;
	if(at_ < text_.size() && !isDelimiter(text_[at_]))
		fail(at_, "expected a space or a bracket after the string's closing quote");
}

void Lexer::wordAt(std::size_t start) noexcept {
	at_ = start;
	while(at_ < text_.size() && !isDelimiter(text_[at_])) ++at_;
}

void Lexer::discardBrackets(const Token& token, std::vector<TokenKind>& closers, std::size_t& owed) const {
	const bool opens =
	    token.kind == TokenKind::OpenNode || token.kind == TokenKind::OpenList || token.kind == TokenKind::OpenMap;
	if(opens) {
		if(closers.size() >= static_cast<std::size_t>(maxNesting))
			fail(token.offset, "nested more than " + std::to_string(maxNesting) + " levels deep");
		if(closers.empty()) --owed;
		closers.push_back(static_cast<TokenKind>(static_cast<int>(token.kind) + 1));
		return;
	}
	if(closers.empty() || closers.back() != token.kind)
		fail(token.offset, "expected a form to discard, found " + shown(token));
	closers.pop_back();
}

void Lexer::discardForm(std::size_t start) {
	const bool keep = std::exchange(keepComments_, false); // what is inside goes with the form
	std::size_t owed = 1;
	std::vector<TokenKind> closers;
	while(owed > 0 || !closers.empty()) {
		bool discard = false;
		const Token token = raw(discard);
		if(discard) {
			if(closers.empty()) ++owed; // inside a form it goes with that form; here it takes the next one too
		} else if(token.kind == TokenKind::End) {
			fail(token.offset, "expected a form to discard after the #_ at " + placeOf(start));
		} else if(token.kind <= TokenKind::CloseMap) { // the brackets come first in TokenKind
			discardBrackets(token, closers, owed);
		} else if(closers.empty() && (token.kind < TokenKind::Quote || token.kind > TokenKind::Splice)) {
			--owed; // a prefix applies to the form still owed
		}
	}
	keepComments_ = keep;
}

std::string Lexer::name(const Token& token) const {
	const std::string_view written = token.text.substr(1);
	if(written.starts_with('"')) return unquote(written, token.offset + 1);
	if(!isPlainWord(written))
		fail(token.offset, "expected a word or a string in quotes after '" + std::string(token.text.substr(0, 1)) +
		                       "', found " + shown(token));
	return std::string(written);
}

std::string Lexer::unquote(std::string_view written, std::size_t offset) const {
	// Plain ASCII without an escape: itself, without a JSON parser
	if(isPlainQuoted(written)) return std::string(written.substr(1, written.size() - 2));
	try {
		// A line break or a tab typed inside the quotes is that character, as in Lisp (a docstring of several lines);
		// any other control character is still refused, as is a line break after a backslash
		if(written.find_first_of("\n\r\t") == std::string_view::npos) return json::parse(written).get<std::string>();
		return json::parse(escapedBreaks(written)).get<std::string>();
	} catch(const json::exception&) {
		fail(offset, "a string in quotes that JSON can't read: an unknown escape, a control character or bytes "
		             "that aren't UTF-8");
	}
}

std::optional<json> Lexer::constant(const Token& token) const {
	const std::string_view text = token.text;
	if(text == "true" || text == "false") return json(text == "true");
	if(text == "nil") return json(nullptr);
	if(!isJsonNumber(text)) return std::nullopt;
	if(text.size() > maxNumberLength)
		fail(token.offset, "a number longer than " + std::to_string(maxNumberLength) + " characters");
	try {
		return parseUntrusted(text);
	} catch(const DocumentError&) { // a number no double can hold, such as 1e400
		fail(token.offset, "a number too large: " + shown(token));
	}
}

std::string Lexer::excerpt(std::string_view text) {
	constexpr std::size_t most = 40;
	if(text.size() <= most) return std::string(text);
	std::size_t cut = most;
	while(cut > 0 && (static_cast<unsigned char>(text[cut]) & 0xC0U) == 0x80U) --cut; // a UTF-8 continuation byte
	return std::string(text.substr(0, cut)) + "...";
}

std::string Lexer::shown(const Token& token) {
	return token.kind == TokenKind::End ? "the end of the text" : "'" + excerpt(token.text) + "'";
}

std::pair<std::size_t, std::size_t> Lexer::lineAndColumn(std::size_t offset) const {
	offset = std::min(offset, text_.size());
	const std::string_view before = text_.substr(0, offset);
	const std::size_t line = static_cast<std::size_t>(std::ranges::count(before, '\n')) + 1;
	const std::size_t start = before.rfind('\n') == std::string_view::npos ? 0 : before.rfind('\n') + 1;
	std::size_t column = 1;
	for(const char c : before.substr(start))
		if((static_cast<unsigned char>(c) & 0xC0U) != 0x80U) ++column; // a UTF-8 continuation byte isn't one
	return {line, column};
}

std::string Lexer::placeOf(std::size_t offset) const {
	const auto [line, column] = lineAndColumn(offset);
	return "line " + std::to_string(line) + ", column " + std::to_string(column);
}

void Lexer::fail(std::size_t offset, const std::string& message) const {
	const auto [line, column] = lineAndColumn(offset);
	throw NotationError("", line, column, message);
}

} // namespace affresco::notation
