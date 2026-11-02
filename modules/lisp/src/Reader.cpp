/// @file
/// @brief The language's reader (Reader.hpp). It descends one call per form and refuses a form before it would nest
/// deeper than maxNesting, so the stack it uses is bounded whatever the text.

#include "Reader.hpp"

#include <affresco/notation/Notation.hpp>
#include <affresco/notation/Words.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace affresco::lisp {

namespace {

/// @brief Makes a LispError of a NotationError, with the file and the text's own first line and column.
/// @param error The error.
/// @param file The file.
/// @param line The text's first line.
/// @param column The text's first column.
/// @return The LispError.
LispError placed(const notation::NotationError& error, const std::string& file, std::size_t line, std::size_t column) {
	const std::size_t at = error.line() + line - 1;
	return {file, at, error.line() == 1 ? error.column() + column - 1 : error.column(), "", error.message()};
}

} // namespace

bool isSymbolWord(std::string_view text) noexcept {
	static constexpr std::string_view extra = "_.@$-+*/<>=!?&%";
	const auto allowed = [](char c) {
		return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || extra.contains(c);
	};
	if(text.empty() || !std::ranges::all_of(text, allowed)) return false;
	const auto digit = [](char c) { return c >= '0' && c <= '9'; };
	if(digit(text.front())) return false;
	const bool signed_ = text.front() == '-' || text.front() == '+' || text.front() == '.';
	return !signed_ || text.size() == 1 || !digit(text[1]);
}

Reader::Reader(Heap& heap, std::string_view text, const std::string& file, std::size_t line, std::size_t column)
    : heap_(heap), lexer_([&] -> notation::Lexer {
	      try {
		      return notation::Lexer(text, true);
	      } catch(const notation::NotationError& error) {
		      throw placed(error, file, line, column);
	      }
      }()),
      file_(heap.file(file)), firstLine_(line), firstColumn_(column) {}

std::optional<ReadForm> Reader::next() {
	try {
		const Token token = lexer_.next();
		if(token.kind == TokenKind::End) return std::nullopt;
		ReadForm read;
		read.start = token.offset;
		read.form = form(token, 0);
		read.end = lexer_.lastEnd();
		return read;
	} catch(const notation::NotationError& error) {
		throw placed(error, *file_, firstLine_, firstColumn_);
	}
}

void Reader::shallowEnough(const Token& token, int levels) const {
	if(levels > maxNesting)
		lexer_.fail(token.offset, "nested more than " + std::to_string(maxNesting) + " levels deep");
}

Place Reader::placeAt(std::size_t offset) {
	const std::string_view text = lexer_.text();
	for(; scanned_ < offset && scanned_ < text.size(); ++scanned_) {
		if(text[scanned_] != '\n') continue;
		++line_;
		lineStart_ = scanned_ + 1;
	}
	std::size_t column = 1;
	for(std::size_t at = lineStart_; at < offset && at < text.size(); ++at)
		if((static_cast<unsigned char>(text[at]) & 0xC0U) != 0x80U) ++column; // a UTF-8 continuation byte isn't one
	if(line_ == 0) column += firstColumn_ - 1;
	return {file_, static_cast<std::uint32_t>(firstLine_ + line_), static_cast<std::uint32_t>(column)};
}

Value Reader::form(const Token& token, int depth) {
	switch(token.kind) {
	case TokenKind::OpenNode:
		return list(token, depth);
	case TokenKind::OpenList:
		return vector(token, depth);
	case TokenKind::OpenMap:
		return map(token, depth);
	case TokenKind::String:
		return heap_.string(lexer_.unquote(token.text, token.offset), placeAt(token.offset));
	case TokenKind::Word:
		return word(token);
	case TokenKind::Key:
		return heap_.keyword(lexer_.name(token), placeAt(token.offset));
	case TokenKind::Quote:
		return prefixed(token, "quote", depth);
	case TokenKind::Backquote:
		return prefixed(token, "backquote", depth);
	case TokenKind::Unquote:
		return prefixed(token, "unquote", depth);
	case TokenKind::Splice:
		return prefixed(token, "unquote-splicing", depth);
	case TokenKind::Id:
		lexer_.fail(token.offset, "an id (" + notation::Lexer::shown(token) + ") outside a node");
	case TokenKind::End:
		lexer_.fail(token.offset, "expected a value, found the end of the text");
	default:
		lexer_.fail(token.offset, "expected a value, found " + notation::Lexer::shown(token));
	}
}

Value Reader::prefixed(const Token& prefix, std::string_view name, int depth) {
	shallowEnough(prefix, depth + 1);
	const Place place = placeAt(prefix.offset);
	const Token next = lexer_.next();
	const std::vector<Value> items = {heap_.symbol(name, place), form(next, depth + 1)};
	return heap_.sequence(Kind::List, std::span<const Value>(items), place);
}

Value Reader::word(const Token& token) {
	if(std::optional<json> constant = lexer_.constant(token)) return Value::view(*constant); // a scalar: copied
	if(!isSymbolWord(token.text))
		lexer_.fail(token.offset, "unexpected " + notation::Lexer::shown(token) +
		                              ": not a word, a number, true, false or nil (a string "
		                              "with other characters goes in quotes)");
	return heap_.symbol(token.text, placeAt(token.offset));
}

void Reader::head(const Token& token, std::vector<Value>& items, int depth) {
	const std::size_t hash = token.kind == TokenKind::Word ? token.text.find('#') : std::string_view::npos;
	if(hash == std::string_view::npos) {
		if(token.kind == TokenKind::Id)
			lexer_.fail(token.offset,
			            "expected the node's type, or node, after '(', found " + notation::Lexer::shown(token));
		items.push_back(form(token, depth));
		return;
	}
	const std::string_view type = token.text.substr(0, hash);
	const std::string_view id = token.text.substr(hash + 1);
	if(!notation::isPlainWord(type) || !notation::isPlainWord(id))
		lexer_.fail(token.offset,
		            "expected the node's type, or node, after '(', found " + notation::Lexer::shown(token));
	const Place place = placeAt(token.offset);
	items.push_back(heap_.symbol(type, place));
	items.push_back(heap_.keyword("id", place));
	items.push_back(heap_.string(id, place));
}

Value Reader::list(const Token& open, int depth) {
	shallowEnough(open, depth + 1);
	const Place place = placeAt(open.offset);
	std::vector<Value> items;
	Token item = lexer_.next();
	if(item.kind == TokenKind::CloseNode) return heap_.sequence(Kind::List, std::move(items), place);
	if(item.kind == TokenKind::End)
		lexer_.fail(item.offset, "expected ')' to close the node opened at " + lexer_.placeOf(open.offset));
	head(item, items, depth + 1);
	for(item = lexer_.next(); item.kind != TokenKind::CloseNode; item = lexer_.next()) {
		if(item.kind == TokenKind::End)
			lexer_.fail(item.offset, "expected ')' to close the node opened at " + lexer_.placeOf(open.offset));
		if(item.kind == TokenKind::Id) { // `#x` is `:id "x"`
			const Place at = placeAt(item.offset);
			items.push_back(heap_.keyword("id", at));
			items.push_back(heap_.string(lexer_.name(item), at));
		} else {
			items.push_back(form(item, depth + 1));
		}
	}
	return heap_.sequence(Kind::List, std::move(items), place);
}

Value Reader::vector(const Token& open, int depth) {
	shallowEnough(open, depth + 1);
	const Place place = placeAt(open.offset);
	std::vector<Value> items;
	for(Token item = lexer_.next(); item.kind != TokenKind::CloseList; item = lexer_.next()) {
		if(item.kind == TokenKind::End)
			lexer_.fail(item.offset, "expected ']' to close the list opened at " + lexer_.placeOf(open.offset));
		items.push_back(form(item, depth + 1));
	}
	return heap_.sequence(Kind::Vector, std::move(items), place);
}

Value Reader::map(const Token& open, int depth) {
	shallowEnough(open, depth + 1);
	const Place place = placeAt(open.offset);
	std::vector<Value> pairs;
	std::unordered_set<std::string_view> seen;
	bool literal = true;
	for(Token item = lexer_.next(); item.kind != TokenKind::CloseMap; item = lexer_.next()) {
		if(item.kind == TokenKind::End)
			lexer_.fail(item.offset, "expected '}' to close the object opened at " + lexer_.placeOf(open.offset));
		const Value key = form(item, depth + 1);
		literal = literal && (key.kind() == Kind::Keyword || key.kind() == Kind::String);
		if((key.kind() == Kind::Keyword || key.kind() == Kind::String) && !seen.insert(key.text()).second)
			lexer_.fail(item.offset,
			            "the key \"" + notation::Lexer::excerpt(key.text()) + "\" twice: a key has one value");
		pairs.push_back(key);
		pairs.push_back(form(lexer_.next(), depth + 1));
	}
	if(!literal) { // worked out when it runs
		pairs.insert(pairs.begin(), heap_.symbol("%map", place));
		return heap_.sequence(Kind::List, std::move(pairs), place);
	}
	std::vector<MapEntry> entries;
	entries.reserve(pairs.size() / 2);
	for(std::size_t at = 0; at + 1 < pairs.size(); at += 2) entries.push_back({pairs[at].text(), pairs[at + 1]});
	return heap_.map(std::move(entries), place);
}

} // namespace affresco::lisp
